// Copyright (C) 2026 Boas Meier
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include "search/SearchResults.h"
#include "search/TextMatcher.h"

#include <QHash>
#include <QList>
#include <QObject>
#include <QPolygonF>
#include <QRectF>
#include <QString>
#include <QThread>
#include <QUrl>

#include <atomic>
#include <memory>

class QPdfDocument;

namespace sheaf {

// What the scan found on one page.
struct PageScan {
  int page = 0;
  QVector<TextMatch> matches;
  bool has_text = false;
};

class PageScanner;

// Searching one PDF for text, without ever blocking the interface.
//
// Qt's own PdfSearchModel needs many seconds for a few hundred pages and has no way to tell that it has finished, so
// this does the work itself: a worker thread reads the text of the pages with PDFium (starting at the page the user is
// on, so the first match appears at once, then wrapping round) and reports in small batches. The rectangles to
// highlight are worked out only for pages that are shown. Used from QML, one session per open document.
// NOLINTBEGIN(readability-identifier-naming): QML-facing API follows Qt naming.
class PdfSearchSession : public QObject {
  Q_OBJECT
  // The PDF to search: a file URL or a path.
  Q_PROPERTY(QUrl source READ source WRITE setSource NOTIFY sourceChanged)
  Q_PROPERTY(QString query READ query NOTIFY stateChanged)
  // Matches found so far; final once `complete`.
  Q_PROPERTY(int count READ count NOTIFY stateChanged)
  Q_PROPERTY(bool complete READ complete NOTIFY stateChanged)
  Q_PROPERTY(bool searching READ searching NOTIFY stateChanged)
  // False only when the whole document has been read and no page has any text (a scan without a text layer).
  Q_PROPERTY(bool hasText READ hasText NOTIFY stateChanged)
  // 1-based number of the current match in document order (0 if none); exact once `complete`.
  Q_PROPERTY(int currentNumber READ currentNumber NOTIFY currentChanged)
  Q_PROPERTY(int currentPage READ currentPage NOTIFY currentChanged)    // -1 if none
  Q_PROPERTY(int currentStart READ currentStart NOTIFY currentChanged)  // offset of the match in the page text
  Q_PROPERTY(QList<QPolygonF> currentBounds READ currentBounds NOTIFY currentChanged)
  Q_PROPERTY(QRectF currentRect READ currentRect NOTIFY currentChanged)

public:
  explicit PdfSearchSession(QObject* parent = nullptr);
  ~PdfSearchSession() override;

  QUrl source() const;
  void setSource(const QUrl& source);
  QString query() const;
  int count() const;
  bool complete() const;
  bool searching() const;
  bool hasText() const;
  int currentNumber() const;
  int currentPage() const;
  int currentStart() const;
  QList<QPolygonF> currentBounds();
  QRectF currentRect();

  // Starts (or restarts) a search for `query`. The current match becomes the first one at or after the position
  // (`anchor_page`, `anchor_start`), i.e. the user's place; when `reveal` is set, revealRequested() is emitted for it
  // as soon as it is known. An empty query is the same as clear().
  Q_INVOKABLE void start(const QString& query, int anchor_page, int anchor_start, bool reveal);
  // Stops any search and forgets the results, so nothing is highlighted any more.
  Q_INVOKABLE void clear();
  Q_INVOKABLE void next();
  Q_INVOKABLE void previous();
  // The rectangles (in page points, top-left origin) to highlight on a page, for all its matches.
  Q_INVOKABLE QList<QPolygonF> boundingPolygonsOnPage(int page);
  // The number of matches on a page; cheap, for tests and the UI.
  Q_INVOKABLE int matchesOnPage(int page) const;

signals:
  void sourceChanged();
  void stateChanged();
  void currentChanged();
  // The matches of `page` changed (new results arrived, or the search was reset); highlights must be redrawn.
  void pageHighlightsChanged(int page);
  void highlightsReset();
  // The current match changed because of the user's action and should be scrolled into view.
  void revealRequested();
  // NOLINTEND(readability-identifier-naming)

private:
  void OnScanned(quint64 generation, const QVector<PageScan>& pages);
  void OnFinished(quint64 generation);
  void SelectInitialMatch();
  void ReportCurrent(bool reveal);
  bool EnsureDocument();
  QList<QPolygonF> BoundsOfMatch(int page, const TextMatch& match);

  QUrl source_;
  QString path_;
  QString query_;
  SearchResults results_;
  MatchPosition anchor_;
  bool reveal_pending_ = false;
  bool started_ = false;

  std::atomic<quint64> generation_{0};
  QThread thread_;
  PageScanner* scanner_ = nullptr;  // lives in thread_

  std::unique_ptr<QPdfDocument> document_;  // GUI thread: bounds of the matches that are shown
  QHash<int, QList<QPolygonF>> page_bounds_;
  QList<QPolygonF> current_bounds_;
  QRectF current_rect_;
  bool current_bounds_valid_ = false;
};

}  // namespace sheaf

Q_DECLARE_METATYPE(sheaf::PageScan)
Q_DECLARE_METATYPE(QVector<sheaf::PageScan>)
