// Copyright (C) 2026 Boas Meier
// SPDX-License-Identifier: GPL-3.0-or-later

#include "search/PdfSearchSession.h"

#include <QElapsedTimer>
#include <QHash>
#include <QPdfDocument>
#include <QPdfSelection>

#include <utility>

namespace sheaf {

namespace {

// PDFium reports one rectangle per character. Joined per line they make one highlight per line of a match: no seams
// between letters when scaled, and a phrase over two lines gets two rectangles.
QList<QPolygonF> MergeOnLines(const QList<QPolygonF>& polygons) {
  QList<QPolygonF> merged;
  QRectF line;
  bool have_line = false;
  for (const QPolygonF& polygon : polygons) {
    const QRectF rect = polygon.boundingRect();
    if (!rect.isValid() || rect.isEmpty()) {
      continue;
    }
    if (have_line) {
      const qreal overlap = qMin(line.bottom(), rect.bottom()) - qMax(line.top(), rect.top());
      const qreal smaller = qMin(line.height(), rect.height());
      const qreal gap = rect.left() - line.right();
      if (overlap >= 0.6 * smaller && gap <= smaller && gap >= -smaller) {
        line = line.united(rect);
        continue;
      }
      merged.append(QPolygonF(line));
    }
    line = rect;
    have_line = true;
  }
  if (have_line) {
    merged.append(QPolygonF(line));
  }
  return merged;
}

}  // namespace

// Lives in the worker thread and owns its own PDFium document there.
class PageScanner : public QObject {
  Q_OBJECT

public:
  explicit PageScanner(const std::atomic<quint64>* current_generation) : current_generation_(current_generation) {}

  // Invoked by name across threads (see PdfSearchSession::start), hence Q_INVOKABLE.
  Q_INVOKABLE void Scan(quint64 generation, const QString& path, const QString& query, int start_page) {
    if (!document_ || loaded_path_ != path) {
      document_ = std::make_unique<QPdfDocument>();
      document_->load(path);
      loaded_path_ = path;
    }
    const int pages = document_->pageCount();
    if (path != cache_path_) {
      texts_.clear();
      cached_chars_ = 0;
      cache_path_ = path;
    }
    QVector<PageScan> batch;
    QElapsedTimer since_last_report;
    since_last_report.start();
    bool first = true;
    for (int i = 0; i < pages; ++i) {
      if (current_generation_->load() != generation) {
        return;  // a newer search replaced this one
      }
      const int page = (start_page + i) % pages;
      const QString text = PageText(page);
      batch.append({.page = page, .matches = FindMatches(text, query), .has_text = !text.trimmed().isEmpty()});
      // Report the first page at once (the first match is wanted fast), then in small batches.
      if (first || batch.size() >= kBatchPages || since_last_report.elapsed() >= kBatchMs) {
        emit Scanned(generation, batch);
        batch.clear();
        since_last_report.restart();
        first = false;
      }
    }
    if (!batch.isEmpty()) {
      emit Scanned(generation, batch);
    }
    emit Finished(generation);
  }

signals:
  // (Unnamed parameters: the generated definitions name them differently.) The generation of the search, and what the
  // scan found on some pages; the generation of a search that has read every page.
  // NOLINTNEXTLINE(readability-named-parameter): moc's generated definition names them differently
  void Scanned(quint64, const QVector<sheaf::PageScan>&);
  // NOLINTNEXTLINE(readability-named-parameter): see above
  void Finished(quint64);

private:
  static constexpr int kBatchPages = 16;
  static constexpr int kBatchMs = 15;

  // The text of a page, from PDFium the first time and from memory afterwards: refining a query while typing then needs
  // no PDFium work at all. The cache stops growing at kMaxCachedChars (some 64 MB), beyond that pages are read each
  // time.
  QString PageText(int page) {
    const auto cached = texts_.constFind(page);
    if (cached != texts_.constEnd()) {
      return cached.value();
    }
    const QString text = document_->getAllText(page).text();
    if (cached_chars_ + text.size() <= kMaxCachedChars) {
      texts_.insert(page, text);
      cached_chars_ += text.size();
    }
    return text;
  }

  static constexpr qsizetype kMaxCachedChars = qsizetype{32} * 1024 * 1024;

  const std::atomic<quint64>* current_generation_;
  std::unique_ptr<QPdfDocument> document_;
  QString loaded_path_;
  QString cache_path_;
  QHash<int, QString> texts_;
  qsizetype cached_chars_ = 0;
};

PdfSearchSession::PdfSearchSession(QObject* parent) : QObject(parent), scanner_(new PageScanner(&generation_)) {
  qRegisterMetaType<sheaf::PageScan>();
  qRegisterMetaType<QVector<sheaf::PageScan>>();
  scanner_->moveToThread(&thread_);
  connect(&thread_, &QThread::finished, scanner_, &QObject::deleteLater);
  connect(scanner_, &PageScanner::Scanned, this, &PdfSearchSession::OnScanned);
  connect(scanner_, &PageScanner::Finished, this, &PdfSearchSession::OnFinished);
  thread_.start();
}

PdfSearchSession::~PdfSearchSession() {
  ++generation_;  // makes a running scan stop at the next page
  thread_.quit();
  thread_.wait();
}

QUrl PdfSearchSession::source() const {
  return source_;
}

void PdfSearchSession::setSource(const QUrl& source) {
  if (source == source_) {
    return;
  }
  source_ = source;
  path_ = source.isLocalFile() ? source.toLocalFile() : source.toString();
  document_.reset();
  clear();
  emit sourceChanged();
}

QString PdfSearchSession::query() const {
  return query_;
}

int PdfSearchSession::count() const {
  return results_.Count();
}

bool PdfSearchSession::complete() const {
  return started_ && results_.Complete();
}

bool PdfSearchSession::searching() const {
  return started_ && !results_.Complete();
}

bool PdfSearchSession::hasText() const {
  return results_.HasText();
}

int PdfSearchSession::currentNumber() const {
  return results_.CurrentNumber();
}

int PdfSearchSession::currentPage() const {
  return results_.Current() ? results_.Current()->page : -1;
}

int PdfSearchSession::currentStart() const {
  return results_.Current() ? results_.Current()->start : -1;
}

bool PdfSearchSession::EnsureDocument() {
  if (!document_) {
    document_ = std::make_unique<QPdfDocument>();
    document_->load(path_);
  }
  return document_->status() == QPdfDocument::Status::Ready;
}

void PdfSearchSession::start(const QString& query, int anchor_page, int anchor_start, bool reveal) {
  if (query.trimmed().isEmpty()) {
    clear();
    return;
  }
  if (path_.isEmpty() || !EnsureDocument()) {
    clear();
    return;
  }
  ++generation_;
  query_ = query;
  started_ = true;
  results_.Reset(document_->pageCount());
  anchor_ = {.page = qBound(0, anchor_page, qMax(0, document_->pageCount() - 1)), .start = anchor_start};
  reveal_pending_ = reveal;
  page_bounds_.clear();
  current_bounds_valid_ = false;
  emit highlightsReset();
  emit stateChanged();
  emit currentChanged();
  QMetaObject::invokeMethod(scanner_, "Scan", Qt::QueuedConnection, Q_ARG(quint64, generation_.load()),
                            Q_ARG(QString, path_), Q_ARG(QString, query), Q_ARG(int, anchor_.page));
}

void PdfSearchSession::clear() {
  ++generation_;
  const bool had_something = started_ || results_.Count() > 0 || results_.HasCurrent();
  started_ = false;
  query_.clear();
  results_.Reset(0);
  reveal_pending_ = false;
  page_bounds_.clear();
  current_bounds_valid_ = false;
  current_bounds_.clear();
  current_rect_ = {};
  if (had_something) {
    emit highlightsReset();
    emit stateChanged();
    emit currentChanged();
  }
}

void PdfSearchSession::OnScanned(quint64 generation, const QVector<PageScan>& pages) {
  if (generation != generation_.load() || !started_) {
    return;  // results of a search that has been replaced
  }
  for (const PageScan& scan : pages) {
    results_.AddPage(scan.page, scan.matches, scan.has_text);
  }
  for (const PageScan& scan : pages) {
    if (!scan.matches.isEmpty()) {
      page_bounds_.remove(scan.page);
      emit pageHighlightsChanged(scan.page);
    }
  }
  const bool had_current = results_.HasCurrent();
  if (!had_current) {
    SelectInitialMatch();
  }
  emit stateChanged();
  if (!had_current && results_.HasCurrent()) {
    ReportCurrent(reveal_pending_);
    reveal_pending_ = false;
  }
}

void PdfSearchSession::OnFinished(quint64 generation) {
  if (generation != generation_.load() || !started_) {
    return;
  }
  // The scan is complete: a match before the anchor can now be chosen (wrapping round), and the numbering is exact.
  const bool had_current = results_.HasCurrent();
  if (!had_current) {
    SelectInitialMatch();
  }
  emit stateChanged();
  if (had_current) {
    emit currentChanged();  // the number of the current match is now exact
  }
  else if (results_.HasCurrent()) {
    ReportCurrent(reveal_pending_);
    reveal_pending_ = false;
  }
}

void PdfSearchSession::SelectInitialMatch() {
  results_.SelectAtOrAfter(anchor_);
}

void PdfSearchSession::ReportCurrent(bool reveal) {
  current_bounds_valid_ = false;
  emit currentChanged();
  if (reveal) {
    emit revealRequested();
  }
}

void PdfSearchSession::next() {
  if (results_.Next()) {
    ReportCurrent(true);
  }
}

void PdfSearchSession::previous() {
  if (results_.Previous()) {
    ReportCurrent(true);
  }
}

int PdfSearchSession::matchesOnPage(int page) const {
  return static_cast<int>(results_.MatchesOnPage(page).size());
}

QList<QPolygonF> PdfSearchSession::BoundsOfMatch(int page, const TextMatch& match) {
  if (!EnsureDocument()) {
    return {};
  }
  return MergeOnLines(document_->getSelectionAtIndex(page, match.char_start, match.char_length).bounds());
}

QList<QPolygonF> PdfSearchSession::boundingPolygonsOnPage(int page) {
  const auto cached = page_bounds_.constFind(page);
  if (cached != page_bounds_.constEnd()) {
    return cached.value();
  }
  QList<QPolygonF> polygons;
  for (const TextMatch& match : results_.MatchesOnPage(page)) {
    polygons.append(BoundsOfMatch(page, match));
  }
  page_bounds_.insert(page, polygons);
  return polygons;
}

QList<QPolygonF> PdfSearchSession::currentBounds() {
  if (!current_bounds_valid_) {
    current_bounds_.clear();
    current_rect_ = {};
    if (const auto current = results_.Current()) {
      for (const TextMatch& match : results_.MatchesOnPage(current->page)) {
        if (match.start == current->start && EnsureDocument()) {
          const QPdfSelection selection =
              document_->getSelectionAtIndex(current->page, match.char_start, match.char_length);
          current_bounds_ = MergeOnLines(selection.bounds());
          current_rect_ = selection.boundingRectangle();
        }
      }
    }
    current_bounds_valid_ = true;
  }
  return current_bounds_;
}

QRectF PdfSearchSession::currentRect() {
  currentBounds();
  return current_rect_;
}

}  // namespace sheaf

#include "PdfSearchSession.moc"
