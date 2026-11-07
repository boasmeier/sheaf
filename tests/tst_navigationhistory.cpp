#include "app/NavigationHistory.h"

#include <QtTest>

namespace {

sheaf::Position At(int page) {
  return {.page = page, .offset_y = page / 100.0, .zoom = 1.5};
}

}  // namespace

class TstNavigationHistory : public QObject {
  Q_OBJECT

private slots:
  void EmptyHistoryGoesNowhere() {
    sheaf::NavigationHistory history;
    QVERIFY(!history.CanGoBack());
    QVERIFY(!history.CanGoForward());
    QVERIFY(!history.GoBack(At(1)).has_value());
    QVERIFY(!history.GoForward(At(1)).has_value());
  }

  void ThreeLinksThreeBacksReturnToTheStart() {
    sheaf::NavigationHistory history;
    // Start on page 10, follow links to 20, then 30, then 40.
    history.RecordJump(At(10));
    history.RecordJump(At(20));
    history.RecordJump(At(30));
    QVERIFY(history.CanGoBack());
    QCOMPARE(history.GoBack(At(40)).value(), At(30));
    QCOMPARE(history.GoBack(At(30)).value(), At(20));
    QCOMPARE(history.GoBack(At(20)).value(), At(10));
    QVERIFY(!history.CanGoBack());
  }

  void ForwardReplaysTheWayBack() {
    sheaf::NavigationHistory history;
    history.RecordJump(At(10));
    history.RecordJump(At(20));
    QVERIFY(history.GoBack(At(30)).has_value());
    QVERIFY(history.GoBack(At(20)).has_value());
    QVERIFY(history.CanGoForward());
    QCOMPARE(history.GoForward(At(10)).value(), At(20));
    QCOMPARE(history.GoForward(At(20)).value(), At(30));
    QVERIFY(!history.CanGoForward());
    QCOMPARE(history.GoBack(At(30)).value(), At(20));
  }

  void NewJumpDiscardsForwardHistory() {
    sheaf::NavigationHistory history;
    history.RecordJump(At(10));
    QVERIFY(history.GoBack(At(20)).has_value());
    QVERIFY(history.CanGoForward());
    history.RecordJump(At(10));
    QVERIFY(!history.CanGoForward());
  }

  void FollowingALinkFromTheSameSpotTwiceAddsOneEntry() {
    sheaf::NavigationHistory history;
    history.RecordJump(At(10));
    history.RecordJump(At(10));
    QVERIFY(history.GoBack(At(20)).has_value());
    QVERIFY(!history.CanGoBack());
  }

  void DepthIsCapped() {
    sheaf::NavigationHistory history;
    for (int i = 0; i < sheaf::NavigationHistory::kMaxDepth + 50; ++i) {
      history.RecordJump(At(i));
    }
    int steps = 0;
    while (history.GoBack(At(0)).has_value()) {
      ++steps;
    }
    QCOMPARE(steps, sheaf::NavigationHistory::kMaxDepth);
  }
};

QTEST_GUILESS_MAIN(TstNavigationHistory)
#include "tst_navigationhistory.moc"
