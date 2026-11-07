#include "search/SearchResults.h"

#include <QtTest>

namespace {

using sheaf::MatchPosition;
using sheaf::TextMatch;

QVector<TextMatch> At(std::initializer_list<int> starts) {
  QVector<TextMatch> matches;
  for (const int start : starts) {
    matches.append({.start = start, .length = 3, .char_start = start, .char_length = 3});
  }
  return matches;
}

// 10 pages; matches on page 2 (offsets 5, 40), page 5 (10) and page 8 (0, 7, 90).
sheaf::SearchResults Sample(bool complete = true) {
  sheaf::SearchResults results;
  results.Reset(10);
  for (int page = 0; page < 10; ++page) {
    if (!complete && page > 5) {
      break;
    }
    QVector<TextMatch> matches;
    if (page == 2) {
      matches = At({5, 40});
    }
    else if (page == 5) {
      matches = At({10});
    }
    else if (page == 8) {
      matches = At({0, 7, 90});
    }
    results.AddPage(page, matches, true);
  }
  return results;
}

}  // namespace

class TstSearchResults : public QObject {
  Q_OBJECT

private slots:
  void CountsMatchesAndTracksCompleteness() {
    sheaf::SearchResults results = Sample(false);
    QCOMPARE(results.PageCount(), 10);
    QCOMPARE(results.PagesScanned(), 6);
    QVERIFY(!results.Complete());
    QCOMPARE(results.Count(), 3);  // pages 2 and 5 so far
    QCOMPARE(results.MatchesOnPage(2).size(), 2);
    QVERIFY(results.MatchesOnPage(3).isEmpty());
    for (int page = 6; page < 10; ++page) {
      results.AddPage(page, page == 8 ? At({0, 7, 90}) : QVector<TextMatch>{}, true);
    }
    QVERIFY(results.Complete());
    QCOMPARE(results.Count(), 6);
  }

  void APageIsCountedOnlyOnce() {
    sheaf::SearchResults results = Sample();
    results.AddPage(2, At({1, 2, 3, 4}), true);  // a repeated delivery changes nothing
    results.AddPage(99, At({1}), true);
    results.AddPage(-1, At({1}), true);
    QCOMPARE(results.Count(), 6);
    QCOMPARE(results.PagesScanned(), 10);
  }

  void SelectsTheFirstMatchAtOrAfterTheAnchor() {
    sheaf::SearchResults results = Sample();
    QVERIFY(results.SelectAtOrAfter({0, 0}));
    QCOMPARE(results.Current().value(), (MatchPosition{2, 5}));
    QVERIFY(results.SelectAtOrAfter({2, 6}));  // after the first match on page 2
    QCOMPARE(results.Current().value(), (MatchPosition{2, 40}));
    QVERIFY(results.SelectAtOrAfter({2, 40}));  // exactly at a match: that match
    QCOMPARE(results.Current().value(), (MatchPosition{2, 40}));
    QVERIFY(results.SelectAtOrAfter({3, 0}));
    QCOMPARE(results.Current().value(), (MatchPosition{5, 10}));
    QVERIFY(results.SelectAtOrAfter({8, 8}));
    QCOMPARE(results.Current().value(), (MatchPosition{8, 90}));
  }

  void SelectingPastTheLastMatchWrapsOnlyWhenTheScanIsComplete() {
    sheaf::SearchResults complete = Sample();
    QVERIFY(complete.SelectAtOrAfter({9, 0}));
    QCOMPARE(complete.Current().value(), (MatchPosition{2, 5}));

    sheaf::SearchResults partial = Sample(false);  // scanned up to page 5
    QVERIFY(!partial.SelectAtOrAfter({6, 0}));     // nothing after page 6 yet: wait for more pages
    QVERIFY(!partial.HasCurrent());
  }

  void NoMatchesSelectsNothing() {
    sheaf::SearchResults results;
    results.Reset(3);
    for (int page = 0; page < 3; ++page) {
      results.AddPage(page, {}, true);
    }
    QVERIFY(results.Complete());
    QVERIFY(!results.SelectAtOrAfter({0, 0}));
    QVERIFY(!results.Next());
    QVERIFY(!results.Previous());
    QCOMPARE(results.CurrentNumber(), 0);
  }

  void NextAndPreviousWalkInDocumentOrderAndWrap() {
    sheaf::SearchResults results = Sample();
    QVERIFY(results.SelectAtOrAfter({0, 0}));
    const QList<MatchPosition> order = {{2, 5}, {2, 40}, {5, 10}, {8, 0}, {8, 7}, {8, 90}};
    for (qsizetype i = 0; i < order.size(); ++i) {
      QCOMPARE(results.Current().value(), order.at(i));
      QCOMPARE(results.CurrentNumber(), static_cast<int>(i) + 1);
      QVERIFY(results.Next());
    }
    QCOMPARE(results.Current().value(), order.first());  // wrapped from the last to the first
    QVERIFY(results.Previous());
    QCOMPARE(results.Current().value(), order.last());  // and back
    QCOMPARE(results.CurrentNumber(), 6);
    for (qsizetype i = order.size() - 1; i > 0; --i) {
      QVERIFY(results.Previous());
      QCOMPARE(results.Current().value(), order.at(i - 1));
    }
  }

  void WhileScanningTheEndsDoNotWrap() {
    sheaf::SearchResults results = Sample(false);  // matches (2,5) (2,40) (5,10) known; the scan is not complete
    QVERIFY(results.SelectAtOrAfter({0, 0}));
    QVERIFY(results.Next());
    QVERIFY(results.Next());
    QCOMPARE(results.Current().value(), (MatchPosition{5, 10}));
    QVERIFY(!results.Next());  // might be more later, so no wrap and no change
    QCOMPARE(results.Current().value(), (MatchPosition{5, 10}));
    QVERIFY(results.SelectAtOrAfter({2, 5}));
    QVERIFY(!results.Previous());  // first of those known so far
    QCOMPARE(results.Current().value(), (MatchPosition{2, 5}));
  }

  void ASingleMatchStaysPut() {
    sheaf::SearchResults results;
    results.Reset(3);
    results.AddPage(0, {}, true);
    results.AddPage(1, At({4}), true);
    results.AddPage(2, {}, true);
    QVERIFY(results.SelectAtOrAfter({0, 0}));
    QVERIFY(!results.Next());
    QVERIFY(!results.Previous());
    QCOMPARE(results.CurrentNumber(), 1);
  }

  void PagesMayArriveOutOfOrder() {
    // The scan starts at the page being read and wraps round: pages 6..9 first, then 0..5.
    sheaf::SearchResults results;
    results.Reset(10);
    results.AddPage(8, At({0, 7, 90}), true);
    for (const int page : {6, 7, 9}) {
      results.AddPage(page, {}, true);
    }
    QVERIFY(results.SelectAtOrAfter({6, 0}));
    QCOMPARE(results.Current().value(), (MatchPosition{8, 0}));
    QCOMPARE(results.CurrentNumber(), 1);
    for (const int page : {0, 1, 3, 4}) {
      results.AddPage(page, {}, true);
    }
    results.AddPage(2, At({5, 40}), true);
    results.AddPage(5, At({10}), true);
    QVERIFY(results.Complete());
    // Numbering is in document order, now including the earlier pages found later.
    QCOMPARE(results.CurrentNumber(), 4);
    QCOMPARE(results.Count(), 6);
  }

  void TextPresenceIsKnownOnceTheScanIsComplete() {
    sheaf::SearchResults scan;
    scan.Reset(3);
    QVERIFY(scan.HasText());  // not known yet: do not claim the document has no text
    scan.AddPage(0, {}, false);
    scan.AddPage(1, {}, false);
    QVERIFY(scan.HasText());
    scan.AddPage(2, {}, false);
    QVERIFY(scan.Complete());
    QVERIFY(!scan.HasText());  // a scanned document: no page has a text layer

    sheaf::SearchResults mixed;
    mixed.Reset(2);
    mixed.AddPage(0, {}, false);
    mixed.AddPage(1, {}, true);  // text, but no match
    QVERIFY(mixed.HasText());
  }

  void ResetForgetsEverything() {
    sheaf::SearchResults results = Sample();
    QVERIFY(results.SelectAtOrAfter({0, 0}));
    results.Reset(4);
    QCOMPARE(results.Count(), 0);
    QCOMPARE(results.PagesScanned(), 0);
    QVERIFY(!results.HasCurrent());
    QVERIFY(!results.Complete());
  }

  void ClearCurrentKeepsTheMatches() {
    sheaf::SearchResults results = Sample();
    QVERIFY(results.SelectAtOrAfter({0, 0}));
    results.ClearCurrent();
    QVERIFY(!results.HasCurrent());
    QCOMPARE(results.Count(), 6);
    QCOMPARE(results.CurrentNumber(), 0);
  }
};

QTEST_GUILESS_MAIN(TstSearchResults)
#include "tst_searchresults.moc"
