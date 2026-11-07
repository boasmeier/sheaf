#include "Fixtures.h"
#include "search/PdfSearchSession.h"

#include <QElapsedTimer>
#include <QPdfDocument>
#include <QPdfSelection>
#include <QSignalSpy>
#include <QTemporaryDir>
#include <QTimer>
#include <QtTest>

namespace {

constexpr int kFirstNeedlePage = 2;  // 0-based: page 3

}  // namespace

class TstPdfSearchSession : public QObject {
  Q_OBJECT

private:
  QTemporaryDir dir_;
  QString text_pdf_;
  QString scanned_pdf_;

  static bool WaitUntilComplete(const sheaf::PdfSearchSession& session, int timeout_ms = 15000) {
    QElapsedTimer timer;
    timer.start();
    while (!session.complete() && timer.elapsed() < timeout_ms) {
      QTest::qWait(5);
    }
    return session.complete();
  }

  static bool WaitForCurrent(const sheaf::PdfSearchSession& session, int timeout_ms = 15000) {
    QElapsedTimer timer;
    timer.start();
    while (session.currentPage() < 0 && timer.elapsed() < timeout_ms) {
      QTest::qWait(1);
    }
    return session.currentPage() >= 0;
  }

private slots:  // NOLINT(readability-redundant-access-specifiers): moc needs `private slots:` after the helpers
  // NOLINTNEXTLINE(readability-identifier-naming): Qt Test fixture name
  void initTestCase() {
    QVERIFY(dir_.isValid());
    text_pdf_ = dir_.filePath("text.pdf");
    scanned_pdf_ = dir_.filePath("scanned.pdf");
    QVERIFY(sheaf::testing::WritePdfWithText(text_pdf_, sheaf::testing::kSearchPages));
    QVERIFY(sheaf::testing::WriteScannedPdf(scanned_pdf_, 12));
  }

  void FindsEveryMatchAndKnowsWhenItIsDone() {
    sheaf::PdfSearchSession session;
    session.setSource(QUrl::fromLocalFile(text_pdf_));
    session.start("needle", 0, 0, false);
    QVERIFY(session.searching());
    QVERIFY(WaitUntilComplete(session));
    QVERIFY(!session.searching());
    QCOMPARE(session.count(), sheaf::testing::ExpectedNeedleCount());  // case-insensitive: the NEEDLE counts too
    QVERIFY(session.hasText());

    session.start("common", 0, 0, false);
    QVERIFY(WaitUntilComplete(session));
    QCOMPARE(session.count(), sheaf::testing::kSearchPages * sheaf::testing::kCommonWordsPerPage);
  }

  void ReportsNothingFoundForAnUnknownWord() {
    sheaf::PdfSearchSession session;
    session.setSource(QUrl::fromLocalFile(text_pdf_));
    session.start("zzyzxqq", 0, 0, true);
    QVERIFY(WaitUntilComplete(session));
    QCOMPARE(session.count(), 0);
    QVERIFY(session.hasText());  // there is text; just not this
    QCOMPARE(session.currentPage(), -1);
    QCOMPARE(session.currentNumber(), 0);
  }

  void APhraseSpanningALineBreakIsFound() {
    sheaf::PdfSearchSession session;
    session.setSource(QUrl::fromLocalFile(text_pdf_));
    session.start("red fox", 0, 0, false);
    QVERIFY(WaitUntilComplete(session));
    QCOMPARE(session.count(), 1);
    QCOMPARE(session.currentPage(), sheaf::testing::kRedFoxPage);
  }

  void ASearchFindsTheFirstMatchQuicklyAndTheFullCountWithinTwoSeconds() {
    sheaf::PdfSearchSession session;
    session.setSource(QUrl::fromLocalFile(text_pdf_));
    QElapsedTimer timer;
    timer.start();
    session.start("common", 0, 0, true);
    QVERIFY(WaitForCurrent(session));
    const qint64 first_match = timer.elapsed();
    QVERIFY(WaitUntilComplete(session));
    const qint64 complete = timer.elapsed();
    qInfo() << "first match after" << first_match << "ms, complete after" << complete << "ms," << session.count()
            << "matches on" << sheaf::testing::kSearchPages << "pages";
    QVERIFY2(first_match < 200, qPrintable(QString("first match took %1 ms").arg(first_match)));
    QVERIFY2(complete < 2000, qPrintable(QString("full count took %1 ms").arg(complete)));
  }

  void TheScanStartsAtThePageBeingReadAndWrapsRound() {
    sheaf::PdfSearchSession session;
    session.setSource(QUrl::fromLocalFile(text_pdf_));
    session.start("needle", 200, 0, false);  // reading page 201
    QVERIFY(WaitForCurrent(session));
    QCOMPARE(session.currentPage(), 202);  // the first needle at or after page 201 (page 203)
    QVERIFY(WaitUntilComplete(session));
    // Numbered in document order: the needles on earlier pages come first.
    int before = 0;
    for (int page = 0; page <= 202; ++page) {
      if (sheaf::testing::PageHasNeedle(page) || page == sheaf::testing::kShoutedNeedlePage) {
        ++before;
      }
    }
    QCOMPARE(session.currentNumber(), before);
  }

  void AnAnchorPastTheLastMatchWrapsToTheFirst() {
    sheaf::PdfSearchSession session;
    session.setSource(QUrl::fromLocalFile(text_pdf_));
    session.start("needle", 318, 0, false);  // the last needle is on page index 312
    QVERIFY(WaitUntilComplete(session));
    QVERIFY(WaitForCurrent(session, 100));
    QCOMPARE(session.currentPage(), kFirstNeedlePage);
    QCOMPARE(session.currentNumber(), 1);
  }

  void NextAndPreviousStepThroughAllMatchesAndWrap() {
    sheaf::PdfSearchSession session;
    session.setSource(QUrl::fromLocalFile(text_pdf_));
    session.start("needle", 0, 0, false);
    QVERIFY(WaitUntilComplete(session));
    const int total = session.count();
    QCOMPARE(session.currentNumber(), 1);
    QCOMPARE(session.currentPage(), kFirstNeedlePage);
    for (int i = 2; i <= total; ++i) {
      session.next();
      QCOMPARE(session.currentNumber(), i);
    }
    session.next();  // past the last: round to the first
    QCOMPARE(session.currentNumber(), 1);
    session.previous();  // before the first: round to the last
    QCOMPARE(session.currentNumber(), total);
    session.previous();
    QCOMPARE(session.currentNumber(), total - 1);
  }

  void RevealIsRequestedForTheUsersActionsButNotForARestore() {
    sheaf::PdfSearchSession session;
    session.setSource(QUrl::fromLocalFile(text_pdf_));
    const QSignalSpy reveal(&session, &sheaf::PdfSearchSession::revealRequested);
    session.start("needle", 0, 0, false);  // restoring a tab: highlight, but do not scroll
    QVERIFY(WaitUntilComplete(session));
    QCOMPARE(reveal.count(), 0);
    QVERIFY(session.currentPage() >= 0);

    session.next();  // the user steps on: scroll there
    QCOMPARE(reveal.count(), 1);

    session.start("needle", 0, 0, true);  // the user typed: scroll to the first match
    QVERIFY(WaitForCurrent(session));
    QTRY_COMPARE_WITH_TIMEOUT(reveal.count(), 2, 2000);
  }

  void AnAnchorOnAMatchSelectsExactlyThatMatchAgain() {
    // What a tab switch does: start again from the remembered current match.
    int page = 0;
    int start = 0;
    int number = 0;
    {
      sheaf::PdfSearchSession session;
      session.setSource(QUrl::fromLocalFile(text_pdf_));
      session.start("common", 0, 0, false);
      QVERIFY(WaitUntilComplete(session));
      for (int i = 0; i < 100; ++i) {
        session.next();
      }
      page = session.currentPage();
      start = session.currentStart();
      number = session.currentNumber();
      QCOMPARE(number, 101);
    }
    sheaf::PdfSearchSession again;
    again.setSource(QUrl::fromLocalFile(text_pdf_));
    again.start("common", page, start, false);
    QVERIFY(WaitUntilComplete(again));
    QCOMPARE(again.currentPage(), page);
    QCOMPARE(again.currentStart(), start);
    QCOMPARE(again.currentNumber(), number);
  }

  void ANewQueryReplacesTheOldOneAndOldResultsAreIgnored() {
    sheaf::PdfSearchSession session;
    session.setSource(QUrl::fromLocalFile(text_pdf_));
    session.start("common", 0, 0, false);  // thousands of matches...
    session.start("needle", 0, 0, false);  // ...replaced before they arrive
    QVERIFY(WaitUntilComplete(session));
    QCOMPARE(session.query(), QString("needle"));
    QCOMPARE(session.count(), sheaf::testing::ExpectedNeedleCount());
    for (int i = 0; i < 5; ++i) {  // many quick changes, as when typing
      session.start(QString("lorem").left(i + 1), 0, 0, false);
    }
    session.start("needle", 0, 0, false);
    QVERIFY(WaitUntilComplete(session));
    QCOMPARE(session.count(), sheaf::testing::ExpectedNeedleCount());
  }

  void ClearForgetsTheResults() {
    sheaf::PdfSearchSession session;
    session.setSource(QUrl::fromLocalFile(text_pdf_));
    session.start("needle", 0, 0, false);
    QVERIFY(WaitUntilComplete(session));
    const QSignalSpy reset(&session, &sheaf::PdfSearchSession::highlightsReset);
    session.clear();
    QCOMPARE(session.count(), 0);
    QCOMPARE(session.currentPage(), -1);
    QVERIFY(!session.searching());
    QVERIFY(!session.complete());
    QVERIFY(session.boundingPolygonsOnPage(kFirstNeedlePage).isEmpty());
    QCOMPARE(reset.count(), 1);
    session.start("   ", 0, 0, false);  // a blank query is the same as clearing
    QCOMPARE(session.count(), 0);
    QVERIFY(!session.searching());
  }

  void HighlightsSitWhereTheTextIs() {
    sheaf::PdfSearchSession session;
    session.setSource(QUrl::fromLocalFile(text_pdf_));
    session.start("needle", 0, 0, false);
    QVERIFY(WaitUntilComplete(session));

    QVERIFY(session.boundingPolygonsOnPage(0).isEmpty());  // no needle on the first page
    const QList<QPolygonF> polygons = session.boundingPolygonsOnPage(kFirstNeedlePage);
    QCOMPARE(polygons.size(), 1);  // one highlight for a match on one line, not one per letter
    // An independent check: ask the document what text is under the highlighted rectangle.
    QPdfDocument document;
    document.load(text_pdf_);
    const QRectF rect = polygons.first().boundingRect();
    QVERIFY(rect.isValid() && rect.width() > 10 && rect.height() > 3);
    QVERIFY(rect.right() < 595 && rect.bottom() < 842);  // inside the A4 page
    const QPdfSelection under =
        document.getSelection(kFirstNeedlePage, rect.center() - QPointF((rect.width() / 2) - 1, 0),
                              rect.center() + QPointF((rect.width() / 2) - 1, 0));
    QVERIFY2(under.text().contains("needle", Qt::CaseInsensitive), qPrintable(under.text()));

    // The shouted NEEDLE is highlighted too, and a page with several matches has several highlights.
    QCOMPARE(session.boundingPolygonsOnPage(sheaf::testing::kShoutedNeedlePage).size(), 1);
    session.start("common", 0, 0, false);
    QVERIFY(WaitUntilComplete(session));
    QCOMPARE(session.boundingPolygonsOnPage(10).size(), sheaf::testing::kCommonWordsPerPage);
  }

  void AMatchOverTwoLinesGetsOneHighlightPerLine() {
    sheaf::PdfSearchSession session;
    session.setSource(QUrl::fromLocalFile(text_pdf_));
    session.start("red fox", 0, 0, false);
    QVERIFY(WaitUntilComplete(session));
    const QList<QPolygonF> polygons = session.boundingPolygonsOnPage(sheaf::testing::kRedFoxPage);
    QCOMPARE(polygons.size(), 2);  // "red" at the end of one line, "fox" at the start of the next
    QVERIFY(polygons.at(1).boundingRect().top() > polygons.at(0).boundingRect().bottom() - 1);
    QCOMPARE(session.currentBounds().size(), 2);
  }

  void RefiningTheQueryReusesTheTextAndIsMuchFaster() {
    sheaf::PdfSearchSession session;
    session.setSource(QUrl::fromLocalFile(text_pdf_));
    QElapsedTimer timer;
    timer.start();
    session.start("n", 0, 0, false);  // the first search reads every page with PDFium
    QVERIFY(WaitUntilComplete(session));
    const qint64 first = timer.elapsed();
    timer.restart();
    session.start("ne", 0, 0, false);  // typing on: the text is already in memory
    QVERIFY(WaitUntilComplete(session));
    session.start("nee", 0, 0, false);
    QVERIFY(WaitUntilComplete(session));
    session.start("needle", 0, 0, false);
    QVERIFY(WaitUntilComplete(session));
    const qint64 refinements = timer.elapsed();
    qInfo() << "first search" << first << "ms; three refinements together" << refinements << "ms";
    QCOMPARE(session.count(), sheaf::testing::ExpectedNeedleCount());
    QVERIFY2(refinements < (first / 2) + 50, "refining a query should not read the document again");
  }

  void TheCurrentMatchHasItsOwnBounds() {
    sheaf::PdfSearchSession session;
    session.setSource(QUrl::fromLocalFile(text_pdf_));
    session.start("needle", 0, 0, false);
    QVERIFY(WaitUntilComplete(session));
    QCOMPARE(session.currentBounds().size(), 1);
    const QRectF first = session.currentRect();
    QVERIFY(first.isValid() && first.width() > 10);
    session.next();
    const QRectF second = session.currentRect();
    QVERIFY(second.isValid());
    QVERIFY(session.currentPage() > kFirstNeedlePage);
    QVERIFY(first != second || session.currentPage() != kFirstNeedlePage);
  }

  void AScannedDocumentWithoutTextIsRecognised() {
    sheaf::PdfSearchSession session;
    session.setSource(QUrl::fromLocalFile(scanned_pdf_));
    session.start("anything", 0, 0, false);
    QVERIFY(WaitUntilComplete(session));
    QCOMPARE(session.count(), 0);
    QVERIFY2(!session.hasText(), "a document with only pictures has no text to search");
  }

  void ADocumentWithTextButNoMatchIsNotMistakenForAScan() {
    sheaf::PdfSearchSession session;
    session.setSource(QUrl::fromLocalFile(text_pdf_));
    session.start("zzyzx", 0, 0, false);
    QVERIFY(WaitUntilComplete(session));
    QVERIFY(session.hasText());
  }

  void TextPresenceIsNotClaimedWhileTheScanIsRunning() {
    sheaf::PdfSearchSession session;
    session.setSource(QUrl::fromLocalFile(scanned_pdf_));
    session.start("anything", 0, 0, false);
    // Right after the start nothing is known yet.
    if (!session.complete()) {
      QVERIFY(session.hasText());
    }
    QVERIFY(WaitUntilComplete(session));
  }

  void TheInterfaceStaysResponsiveWhileSearching() {
    sheaf::PdfSearchSession session;
    session.setSource(QUrl::fromLocalFile(text_pdf_));
    qint64 longest_gap = 0;
    QElapsedTimer since_last;
    QTimer ticker;
    ticker.setInterval(2);
    QObject::connect(&ticker, &QTimer::timeout, [&] {
      if (since_last.isValid()) {
        longest_gap = qMax(longest_gap, since_last.elapsed());
      }
      since_last.restart();
    });
    ticker.start();
    session.start("common", 0, 0, true);  // ~1000 matches on 320 pages
    QVERIFY(WaitUntilComplete(session));
    // Drawing the highlights of a page while results arrive must not stall either.
    for (int page = 0; page < 40; ++page) {
      session.boundingPolygonsOnPage(page);
    }
    ticker.stop();
    qInfo() << "longest pause of the event loop while searching:" << longest_gap << "ms";
    QVERIFY2(longest_gap < 60, qPrintable(QString("the interface froze for %1 ms").arg(longest_gap)));
  }

  void DestroyingASessionDuringASearchIsSafe() {
    for (int i = 0; i < 5; ++i) {
      sheaf::PdfSearchSession session;
      session.setSource(QUrl::fromLocalFile(text_pdf_));
      session.start("common", 0, 0, false);
      QTest::qWait(20);
    }  // destroyed while the worker is still reading pages
    QVERIFY(true);
  }

  void ChangingTheSourceStopsTheSearch() {
    sheaf::PdfSearchSession session;
    session.setSource(QUrl::fromLocalFile(text_pdf_));
    session.start("common", 0, 0, false);
    session.setSource(QUrl::fromLocalFile(scanned_pdf_));
    QCOMPARE(session.count(), 0);
    QVERIFY(!session.searching());
    QTest::qWait(100);
    QCOMPARE(session.count(), 0);  // nothing from the old document trickles in
  }

  void AMissingOrBrokenFileGivesNoResultsInsteadOfCrashing() {
    sheaf::PdfSearchSession session;
    session.setSource(QUrl::fromLocalFile(dir_.filePath("missing.pdf")));
    session.start("x", 0, 0, true);
    QCOMPARE(session.count(), 0);
    QVERIFY(!session.searching());
    QFile broken(dir_.filePath("broken.pdf"));
    QVERIFY(broken.open(QIODevice::WriteOnly));
    broken.write("not a pdf at all");
    broken.close();
    session.setSource(QUrl::fromLocalFile(dir_.filePath("broken.pdf")));
    session.start("x", 0, 0, true);
    QVERIFY(!session.searching());
  }
};

QTEST_MAIN(TstPdfSearchSession)
#include "tst_pdfsearchsession.moc"
