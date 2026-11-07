#include "app/TabController.h"

#include <QFile>
#include <QSignalSpy>
#include <QTemporaryDir>
#include <QtTest>

namespace {

constexpr int kDelayMs = 50;

QString MakeFile(const QTemporaryDir& dir, const QString& name, char fill) {
  QString path = dir.filePath(name);
  QFile file(path);
  if (!file.open(QIODevice::WriteOnly)) {
    return {};
  }
  file.write(QByteArray(1000, fill));
  return path;
}

QUrl Url(const QString& path) {
  return QUrl::fromLocalFile(path);
}

}  // namespace

class TstTabController : public QObject {
  Q_OBJECT

private slots:
  // NOLINTNEXTLINE(readability-identifier-naming): Qt Test fixture name
  void init() {
    QVERIFY(db_.Open(":memory:"));
    store_ = std::make_unique<sheaf::PositionStore>(db_);
  }

  // NOLINTNEXTLINE(readability-identifier-naming): Qt Test fixture name
  void cleanup() { db_.Close(); }

  void OpenActivatesNewTab() {
    const QTemporaryDir dir;
    sheaf::TabController tabs(*store_, kDelayMs);
    const QSignalSpy spy(&tabs, &sheaf::TabController::activeTabChanged);
    QVERIFY(tabs.openFile(Url(MakeFile(dir, "a.pdf", 'a'))));
    QCOMPARE(tabs.model()->Count(), 1);
    QCOMPARE(tabs.activeIndex(), 0);
    QCOMPARE(tabs.activeTitle(), QString("a"));
    QCOMPARE(spy.count(), 1);
    QVERIFY(!tabs.activePosition().value("valid").toBool());
  }

  void OpeningSameFileTwiceReusesTab() {
    const QTemporaryDir dir;
    sheaf::TabController tabs(*store_, kDelayMs);
    const QString a = MakeFile(dir, "a.pdf", 'a');
    QVERIFY(tabs.openFile(Url(a)));
    QVERIFY(tabs.openFile(Url(MakeFile(dir, "b.pdf", 'b'))));
    QVERIFY(tabs.openFile(Url(a)));
    QCOMPARE(tabs.model()->Count(), 2);
    QCOMPARE(tabs.activeIndex(), 0);
  }

  void UnreadableFileIsRejected() {
    sheaf::TabController tabs(*store_, kDelayMs);
    QVERIFY(!tabs.openFile(Url("/nonexistent/x.pdf")));
    QCOMPARE(tabs.model()->Count(), 0);
  }

  void PositionIsSavedOnlyAfterQuietPeriod() {
    const QTemporaryDir dir;
    sheaf::TabController tabs(*store_, 200);
    QVERIFY(tabs.openFile(Url(MakeFile(dir, "a.pdf", 'a'))));
    const QString key = tabs.model()->At(0).file_key;

    tabs.reportPosition(3, 0.5, 1.5);
    QTest::qWait(100);
    QVERIFY(!store_->Load(key).has_value());
    tabs.reportPosition(4, 0.25, 1.5);  // restarts the quiet period
    QTest::qWait(150);
    QVERIFY(!store_->Load(key).has_value());
    QTRY_VERIFY_WITH_TIMEOUT(store_->Load(key).has_value(), 1000);
    QCOMPARE(store_->Load(key).value(), (sheaf::Position{4, 0.25, 1.5}));
  }

  void SwitchingTabsFlushesPendingPosition() {
    const QTemporaryDir dir;
    sheaf::TabController tabs(*store_, 60000);
    QVERIFY(tabs.openFile(Url(MakeFile(dir, "a.pdf", 'a'))));
    const QString key_a = tabs.model()->At(0).file_key;
    QVERIFY(tabs.openFile(Url(MakeFile(dir, "b.pdf", 'b'))));  // tab 1 active, a's report not yet made
    tabs.activate(0);
    tabs.reportPosition(7, 0.1, 2);
    QVERIFY(!store_->Load(key_a).has_value());
    tabs.activate(1);
    QCOMPARE(store_->Load(key_a).value(), (sheaf::Position{7, 0.1, 2}));
  }

  void ClosingActiveTabFlushesAndActivatesNeighbour() {
    const QTemporaryDir dir;
    sheaf::TabController tabs(*store_, 60000);
    QVERIFY(tabs.openFile(Url(MakeFile(dir, "a.pdf", 'a'))));
    QVERIFY(tabs.openFile(Url(MakeFile(dir, "b.pdf", 'b'))));
    QVERIFY(tabs.openFile(Url(MakeFile(dir, "c.pdf", 'c'))));
    tabs.activate(1);
    const QString key_b = tabs.model()->At(1).file_key;
    tabs.reportPosition(9, 0.9, 1);
    tabs.closeTab(1);
    QCOMPARE(store_->Load(key_b).value(), (sheaf::Position{9, 0.9, 1}));
    QCOMPARE(tabs.model()->Count(), 2);
    QCOMPARE(tabs.activeIndex(), 1);
    QCOMPARE(tabs.activeTitle(), QString("c"));

    tabs.closeTab(1);  // last tab: falls back to the previous one
    QCOMPARE(tabs.activeTitle(), QString("a"));
    tabs.closeTab(0);
    QCOMPARE(tabs.activeIndex(), -1);
    QVERIFY(tabs.activeUrl().isEmpty());
  }

  void ClosingOtherTabKeepsActiveTab() {
    const QTemporaryDir dir;
    sheaf::TabController tabs(*store_, kDelayMs);
    QVERIFY(tabs.openFile(Url(MakeFile(dir, "a.pdf", 'a'))));
    QVERIFY(tabs.openFile(Url(MakeFile(dir, "b.pdf", 'b'))));
    tabs.closeTab(0);
    QCOMPARE(tabs.activeIndex(), 0);
    QCOMPARE(tabs.activeTitle(), QString("b"));
  }

  void EachTabKeepsItsOwnHistory() {
    const QTemporaryDir dir;
    sheaf::TabController tabs(*store_, kDelayMs);
    QVERIFY(tabs.openFile(Url(MakeFile(dir, "a.pdf", 'a'))));
    QVERIFY(tabs.openFile(Url(MakeFile(dir, "b.pdf", 'b'))));
    const QSignalSpy history_changed(&tabs, &sheaf::TabController::historyChanged);

    tabs.activate(0);
    QVERIFY(!tabs.canGoBack());
    tabs.recordJump(5, 0.5, 1);
    tabs.recordJump(9, 0.1, 1);
    QVERIFY(tabs.canGoBack());
    QVERIFY(!tabs.canGoForward());

    tabs.activate(1);  // the other tab has no history
    QVERIFY(!tabs.canGoBack());
    QVERIFY(!tabs.goBack(3, 0, 1).value("valid").toBool());
    tabs.recordJump(2, 0.2, 1);

    tabs.activate(0);  // and tab 0 still has its own
    QVERIFY(tabs.canGoBack());
    QVariantMap back = tabs.goBack(30, 0.3, 2);
    QVERIFY(back.value("valid").toBool());
    QCOMPARE(back.value("page").toInt(), 9);
    QCOMPARE(back.value("offsetY").toDouble(), 0.1);
    back = tabs.goBack(9, 0.1, 1);
    QCOMPARE(back.value("page").toInt(), 5);
    QCOMPARE(back.value("offsetY").toDouble(), 0.5);
    QVERIFY(!tabs.canGoBack());
    QVERIFY(tabs.canGoForward());
    const QVariantMap forward = tabs.goForward(5, 0.5, 1);
    QCOMPARE(forward.value("page").toInt(), 9);
    QVERIFY(history_changed.count() > 0);
  }

  void ClosingATabDropsItsHistory() {
    const QTemporaryDir dir;
    sheaf::TabController tabs(*store_, kDelayMs);
    const QString path = MakeFile(dir, "a.pdf", 'a');
    QVERIFY(tabs.openFile(Url(path)));
    tabs.recordJump(5, 0, 1);
    tabs.closeTab(0);
    QVERIFY(!tabs.canGoBack());
    QVERIFY(tabs.openFile(Url(path)));  // reopening starts with a clean history
    QVERIFY(!tabs.canGoBack());
  }

  void CycleWrapsAround() {
    const QTemporaryDir dir;
    sheaf::TabController tabs(*store_, kDelayMs);
    QVERIFY(tabs.openFile(Url(MakeFile(dir, "a.pdf", 'a'))));
    QVERIFY(tabs.openFile(Url(MakeFile(dir, "b.pdf", 'b'))));
    QVERIFY(tabs.openFile(Url(MakeFile(dir, "c.pdf", 'c'))));
    QCOMPARE(tabs.activeIndex(), 2);
    tabs.cycle(1);
    QCOMPARE(tabs.activeIndex(), 0);
    tabs.cycle(-1);
    QCOMPARE(tabs.activeIndex(), 2);
  }

  void ReopeningAfterRenameRestoresPosition() {
    const QTemporaryDir dir;
    const QString original = MakeFile(dir, "lecture.pdf", 'l');
    {
      sheaf::TabController tabs(*store_, kDelayMs);
      QVERIFY(tabs.openFile(Url(original)));
      tabs.reportPosition(42, 0.6, 1.8);
      tabs.closeTab(0);
    }
    const QString renamed = dir.filePath("renamed/script-v2.pdf");
    QVERIFY(QDir(dir.path()).mkdir("renamed"));
    QVERIFY(QFile::rename(original, renamed));

    sheaf::TabController tabs(*store_, kDelayMs);
    QVERIFY(tabs.openFile(Url(renamed)));
    const QVariantMap position = tabs.activePosition();
    QVERIFY(position.value("valid").toBool());
    QCOMPARE(position.value("page").toInt(), 42);
    QCOMPARE(position.value("offsetY").toDouble(), 0.6);
    QCOMPARE(position.value("zoom").toDouble(), 1.8);
  }

  // NOLINTNEXTLINE(readability-redundant-access-specifiers): moc needs `private:` after the slots
private:
  sheaf::Database db_{"tabs"};
  std::unique_ptr<sheaf::PositionStore> store_;
};

QTEST_GUILESS_MAIN(TstTabController)
#include "tst_tabcontroller.moc"
