#include "app/TabController.h"
#include "util/FileIdentity.h"

#include <QFile>
#include <QFileInfo>
#include <QSignalSpy>
#include <QTemporaryDir>
#include <QtTest>

#include <memory>

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

// Titles of the tabs in sidebar order, e.g. "a b c".
QString Order(const sheaf::TabController& tabs) {
  QStringList titles;
  for (const int id : tabs.tabIds()) {
    titles << tabs.model()->FindTab(id)->title;
  }
  return titles.join(' ');
}

}  // namespace

class TstTabController : public QObject {
  Q_OBJECT

private slots:
  // NOLINTNEXTLINE(readability-identifier-naming): Qt Test fixture name
  void init() {
    QVERIFY(db_.Open(":memory:"));
    store_ = std::make_unique<sheaf::PositionStore>(db_);
    session_ = std::make_unique<sheaf::SessionStore>(db_);
  }

  // NOLINTNEXTLINE(readability-identifier-naming): Qt Test fixture name
  void cleanup() { db_.Close(); }

  void OpenActivatesNewTabInADefaultGroup() {
    const QTemporaryDir dir;
    sheaf::TabController tabs(*store_, *session_, kDelayMs);
    const QSignalSpy spy(&tabs, &sheaf::TabController::activeTabChanged);
    QVERIFY(tabs.openFile(Url(MakeFile(dir, "a.pdf", 'a'))));
    QCOMPARE(tabs.tabCount(), 1);
    QCOMPARE(tabs.model()->GroupCount(), 1);
    QCOMPARE(tabs.model()->FindGroup(tabs.model()->GroupIdAt(0))->name, QString("Ungrouped"));
    QCOMPARE(tabs.activeTitle(), QString("a"));
    QCOMPARE(tabs.activeGroupId(), tabs.model()->GroupIdAt(0));
    QCOMPARE(spy.count(), 1);
    QVERIFY(!tabs.activePosition().value("valid").toBool());
  }

  void OpeningSameFileTwiceReusesTab() {
    const QTemporaryDir dir;
    sheaf::TabController tabs(*store_, *session_, kDelayMs);
    const QString a = MakeFile(dir, "a.pdf", 'a');
    QVERIFY(tabs.openFile(Url(a)));
    const int first = tabs.activeTabId();
    QVERIFY(tabs.openFile(Url(MakeFile(dir, "b.pdf", 'b'))));
    QVERIFY(tabs.openFile(Url(a)));
    QCOMPARE(tabs.tabCount(), 2);
    QCOMPARE(tabs.activeTabId(), first);
  }

  void UnreadableFileIsRejected() {
    sheaf::TabController tabs(*store_, *session_, kDelayMs);
    QVERIFY(!tabs.openFile(Url("/nonexistent/x.pdf")));
    QCOMPARE(tabs.tabCount(), 0);
    QCOMPARE(tabs.model()->GroupCount(), 0);  // no group created for nothing
  }

  void NewFilesOpenInTheActiveGroup() {
    const QTemporaryDir dir;
    sheaf::TabController tabs(*store_, *session_, kDelayMs);
    const int physics = tabs.newGroup("Physics");
    const int maths = tabs.newGroup("Maths");
    QCOMPARE(tabs.activeGroupId(), maths);  // a new group becomes the active one

    QVERIFY(tabs.openFile(Url(MakeFile(dir, "m1.pdf", 'a'))));
    QCOMPARE(tabs.model()->GroupOfTab(tabs.activeTabId()), maths);
    tabs.selectGroup(physics);
    QVERIFY(tabs.openFile(Url(MakeFile(dir, "p1.pdf", 'b'))));
    QCOMPARE(tabs.model()->GroupOfTab(tabs.activeTabId()), physics);
    QVERIFY(tabs.openFile(Url(MakeFile(dir, "p2.pdf", 'c'))));
    QCOMPARE(tabs.model()->GroupOfTab(tabs.activeTabId()), physics);

    // Activating a tab in another group makes that group the active one.
    tabs.activate(tabs.model()->FindGroup(maths)->tabs.first().id);
    QCOMPARE(tabs.activeGroupId(), maths);
    QVERIFY(tabs.openFile(Url(MakeFile(dir, "m2.pdf", 'd'))));
    QCOMPARE(tabs.model()->FindGroup(maths)->tabs.size(), 2);
  }

  void NewGroupsGetNamesAndCannotBeBlank() {
    sheaf::TabController tabs(*store_, *session_, kDelayMs);
    const int one = tabs.newGroup();
    const int two = tabs.newGroup("  Notes  ");
    QCOMPARE(tabs.model()->FindGroup(one)->name, QString("Group 1"));
    QCOMPARE(tabs.model()->FindGroup(two)->name, QString("Notes"));
    tabs.renameGroup(two, "Exam");
    QCOMPARE(tabs.model()->FindGroup(two)->name, QString("Exam"));
    tabs.renameGroup(two, "   ");  // blank names are refused
    QCOMPARE(tabs.model()->FindGroup(two)->name, QString("Exam"));
  }

  void PositionIsSavedOnlyAfterQuietPeriod() {
    const QTemporaryDir dir;
    sheaf::TabController tabs(*store_, *session_, 200);
    QVERIFY(tabs.openFile(Url(MakeFile(dir, "a.pdf", 'a'))));
    const QString key = tabs.model()->FindTab(tabs.activeTabId())->file_key;

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
    sheaf::TabController tabs(*store_, *session_, 60000);
    QVERIFY(tabs.openFile(Url(MakeFile(dir, "a.pdf", 'a'))));
    const int a = tabs.activeTabId();
    const QString key_a = tabs.model()->FindTab(a)->file_key;
    QVERIFY(tabs.openFile(Url(MakeFile(dir, "b.pdf", 'b'))));
    tabs.activate(a);
    tabs.reportPosition(7, 0.1, 2);
    QVERIFY(!store_->Load(key_a).has_value());
    tabs.activate(tabs.tabIds().last());
    QCOMPARE(store_->Load(key_a).value(), (sheaf::Position{7, 0.1, 2}));
  }

  void ClosingActiveTabFlushesAndActivatesNeighbour() {
    const QTemporaryDir dir;
    sheaf::TabController tabs(*store_, *session_, 60000);
    QVERIFY(tabs.openFile(Url(MakeFile(dir, "a.pdf", 'a'))));
    QVERIFY(tabs.openFile(Url(MakeFile(dir, "b.pdf", 'b'))));
    QVERIFY(tabs.openFile(Url(MakeFile(dir, "c.pdf", 'c'))));
    const QList<int> ids = tabs.tabIds();
    tabs.activate(ids[1]);
    const QString key_b = tabs.model()->FindTab(ids[1])->file_key;
    tabs.reportPosition(9, 0.9, 1);
    tabs.closeTab(ids[1]);
    QCOMPARE(store_->Load(key_b).value(), (sheaf::Position{9, 0.9, 1}));
    QCOMPARE(Order(tabs), QString("a c"));
    QCOMPARE(tabs.activeTitle(), QString("c"));  // the tab that moved into its place

    tabs.closeTab(tabs.activeTabId());  // last tab: falls back to the previous one
    QCOMPARE(tabs.activeTitle(), QString("a"));
    tabs.closeTab(tabs.activeTabId());
    QCOMPARE(tabs.activeTabId(), -1);
    QVERIFY(tabs.activeUrl().isEmpty());
    QCOMPARE(tabs.model()->GroupCount(), 1);  // the empty group stays
  }

  void ClosingOtherTabKeepsActiveTab() {
    const QTemporaryDir dir;
    sheaf::TabController tabs(*store_, *session_, kDelayMs);
    QVERIFY(tabs.openFile(Url(MakeFile(dir, "a.pdf", 'a'))));
    const int a = tabs.activeTabId();
    QVERIFY(tabs.openFile(Url(MakeFile(dir, "b.pdf", 'b'))));
    const int b = tabs.activeTabId();
    tabs.closeTab(a);
    QCOMPARE(tabs.activeTabId(), b);
    tabs.closeTab(12345);  // unknown ids are ignored
    QCOMPARE(tabs.tabCount(), 1);
  }

  // ---- Session restore ----

  void TheSessionIsSavedRightAfterEachChangeWithoutFlushing() {
    const QTemporaryDir dir;
    sheaf::TabController tabs(*store_, *session_, kDelayMs);
    tabs.newGroup("Physics");
    QVERIFY(tabs.openFile(Url(MakeFile(dir, "a.pdf", 'a'))));
    QTRY_COMPARE_WITH_TIMEOUT(session_->Load().groups.size(), 1, 1000);
    QCOMPARE(session_->Load().groups.first().name, QString("Physics"));
    QCOMPARE(session_->Load().groups.first().tabs.size(), 1);

    tabs.newGroup("Maths");
    QTRY_COMPARE_WITH_TIMEOUT(session_->Load().groups.size(), 2, 1000);
    tabs.renameGroup(tabs.model()->GroupIdAt(1), "Algebra");
    QTRY_COMPARE_WITH_TIMEOUT(session_->Load().groups.at(1).name, QString("Algebra"), 1000);
  }

  void RestoreRebuildsGroupsTabsOrderCollapsedStateAndActiveTab() {
    const QTemporaryDir dir;
    const QString a = MakeFile(dir, "a.pdf", 'a');
    const QString b = MakeFile(dir, "b.pdf", 'b');
    const QString c = MakeFile(dir, "c.pdf", 'c');
    {
      sheaf::TabController tabs(*store_, *session_, kDelayMs);
      const int physics = tabs.newGroup("Physics");
      QVERIFY(tabs.openFile(Url(a)));
      QVERIFY(tabs.openFile(Url(b)));
      const int maths = tabs.newGroup("Maths");
      QVERIFY(tabs.openFile(Url(c)));
      tabs.newGroup("Empty");
      tabs.setCollapsed(maths, true);
      tabs.selectGroup(physics);
      tabs.activate(tabs.model()->FindGroup(physics)->tabs.first().id);  // a
      tabs.reportPosition(33, 0.25, 1.5);
      tabs.Flush();
    }

    sheaf::TabController tabs(*store_, *session_, kDelayMs);
    tabs.Restore();
    QCOMPARE(Order(tabs), QString("a b c"));
    QCOMPARE(tabs.model()->GroupCount(), 3);
    const sheaf::Group* physics = tabs.model()->FindGroup(tabs.model()->GroupIdAt(0));
    const sheaf::Group* maths = tabs.model()->FindGroup(tabs.model()->GroupIdAt(1));
    const sheaf::Group* empty = tabs.model()->FindGroup(tabs.model()->GroupIdAt(2));
    QCOMPARE(physics->name, QString("Physics"));
    QCOMPARE(physics->tabs.size(), 2);
    QVERIFY(!physics->collapsed);
    QCOMPARE(maths->name, QString("Maths"));
    QVERIFY(maths->collapsed);
    QCOMPARE(empty->tabs.size(), 0);
    QCOMPARE(tabs.activeTitle(), QString("a"));
    QCOMPARE(tabs.activeGroupId(), physics->id);
    QVERIFY(!tabs.activeTabMissing());
    // The remembered position of the active tab comes back with it.
    QVERIFY(tabs.activePosition().value("valid").toBool());
    QCOMPARE(tabs.activePosition().value("page").toInt(), 33);
    QCOMPARE(tabs.activePosition().value("zoom").toDouble(), 1.5);
  }

  void RestoreKeepsACollapsedGroupCollapsedEvenWithTheActiveTabInIt() {
    const QTemporaryDir dir;
    {
      sheaf::TabController tabs(*store_, *session_, kDelayMs);
      const int group = tabs.newGroup("G");
      QVERIFY(tabs.openFile(Url(MakeFile(dir, "a.pdf", 'a'))));
      tabs.setCollapsed(group, true);  // the active tab is now hidden
      tabs.Flush();
    }
    sheaf::TabController tabs(*store_, *session_, kDelayMs);
    tabs.Restore();
    QVERIFY(tabs.model()->FindGroup(tabs.model()->GroupIdAt(0))->collapsed);
    QCOMPARE(tabs.activeTitle(), QString("a"));
  }

  void RestoringAnEmptyDatabaseGivesAnEmptySession() {
    sheaf::TabController tabs(*store_, *session_, kDelayMs);
    tabs.Restore();
    QCOMPARE(tabs.tabCount(), 0);
    QCOMPARE(tabs.model()->GroupCount(), 0);
    QCOMPARE(tabs.activeTabId(), -1);
  }

  void RestoreDoesNotRewriteTheSavedSessionWithPartialData() {
    const QTemporaryDir dir;
    {
      sheaf::TabController tabs(*store_, *session_, kDelayMs);
      tabs.newGroup("A");
      QVERIFY(tabs.openFile(Url(MakeFile(dir, "a.pdf", 'a'))));
      QVERIFY(tabs.openFile(Url(MakeFile(dir, "b.pdf", 'b'))));
      tabs.Flush();
    }
    const sheaf::SessionRecord before = session_->Load();
    sheaf::TabController tabs(*store_, *session_, kDelayMs);
    tabs.Restore();
    tabs.Flush();
    QCOMPARE(session_->Load(), before);
  }

  void NewFilesAfterARestoreJoinTheRestoredActiveGroup() {
    const QTemporaryDir dir;
    {
      sheaf::TabController tabs(*store_, *session_, kDelayMs);
      tabs.newGroup("One");
      QVERIFY(tabs.openFile(Url(MakeFile(dir, "a.pdf", 'a'))));
      tabs.newGroup("Two");
      QVERIFY(tabs.openFile(Url(MakeFile(dir, "b.pdf", 'b'))));
      tabs.Flush();
    }
    sheaf::TabController tabs(*store_, *session_, kDelayMs);
    tabs.Restore();
    QVERIFY(tabs.openFile(Url(MakeFile(dir, "c.pdf", 'c'))));
    QCOMPARE(tabs.model()->GroupOfTab(tabs.activeTabId()), tabs.model()->GroupIdAt(1));
    QCOMPARE(Order(tabs), QString("a b c"));
  }

  // ---- Missing files ----

  void ATabWhoseFileIsGoneIsRestoredAsMissing() {
    const QTemporaryDir dir;
    const QString keep = MakeFile(dir, "keep.pdf", 'k');
    const QString gone = MakeFile(dir, "gone.pdf", 'g');
    {
      sheaf::TabController tabs(*store_, *session_, kDelayMs);
      tabs.newGroup("G");
      QVERIFY(tabs.openFile(Url(keep)));
      QVERIFY(tabs.openFile(Url(gone)));  // active
      tabs.Flush();
    }
    QVERIFY(QFile::remove(gone));

    sheaf::TabController tabs(*store_, *session_, kDelayMs);
    tabs.Restore();  // must not fail because of the missing file
    QCOMPARE(Order(tabs), QString("keep gone"));
    QCOMPARE(tabs.activeTitle(), QString("gone"));
    QVERIFY(tabs.activeTabMissing());
    QCOMPARE(tabs.activePath(), gone);
    const int keep_id = tabs.tabIds().first();
    QVERIFY(!tabs.model()->FindTab(keep_id)->missing);
    const QModelIndex row = tabs.model()->index(tabs.model()->tabRow(tabs.activeTabId()));
    QVERIFY(tabs.model()->data(row, sheaf::SessionModel::MissingRole).toBool());

    tabs.activate(keep_id);
    QVERIFY(!tabs.activeTabMissing());
  }

  void AMissingTabCanBeRemoved() {
    const QTemporaryDir dir;
    const QString gone = MakeFile(dir, "gone.pdf", 'g');
    {
      sheaf::TabController tabs(*store_, *session_, kDelayMs);
      QVERIFY(tabs.openFile(Url(gone)));
      tabs.Flush();
    }
    QVERIFY(QFile::remove(gone));
    sheaf::TabController tabs(*store_, *session_, kDelayMs);
    tabs.Restore();
    QVERIFY(tabs.activeTabMissing());
    tabs.closeTab(tabs.activeTabId());
    QCOMPARE(tabs.tabCount(), 0);
    QVERIFY(!tabs.activeTabMissing());
  }

  void ActivatingATabNoticesThatItsFileCameBack() {
    const QTemporaryDir dir;
    const QString path = MakeFile(dir, "a.pdf", 'a');
    const QString other = MakeFile(dir, "o.pdf", 'o');
    const QString saved = dir.filePath("saved.pdf");
    {
      sheaf::TabController tabs(*store_, *session_, kDelayMs);
      QVERIFY(tabs.openFile(Url(path)));
      QVERIFY(tabs.openFile(Url(other)));
      tabs.Flush();
    }
    QVERIFY(QFile::rename(path, saved));
    sheaf::TabController tabs(*store_, *session_, kDelayMs);
    tabs.Restore();
    const int a = tabs.tabIds().first();
    QVERIFY(tabs.model()->FindTab(a)->missing);
    QVERIFY(QFile::rename(saved, path));  // it is back
    tabs.activate(a);
    QVERIFY(!tabs.activeTabMissing());
  }

  void LocateRelinksToTheSameFileUnderANewName() {
    const QTemporaryDir dir;
    const QString original = MakeFile(dir, "lecture.pdf", 'l');
    {
      sheaf::TabController tabs(*store_, *session_, kDelayMs);
      QVERIFY(tabs.openFile(Url(original)));
      tabs.reportPosition(12, 0.3, 1.2);
      tabs.Flush();
    }
    const QString moved = dir.filePath("moved-lecture.pdf");
    QVERIFY(QFile::rename(original, moved));

    sheaf::TabController tabs(*store_, *session_, kDelayMs);
    tabs.Restore();
    QVERIFY(tabs.activeTabMissing());
    const QSignalSpy changed(&tabs, &sheaf::TabController::activeTabChanged);
    QCOMPARE(tabs.locate(tabs.activeTabId(), Url(moved)), 0);
    QVERIFY(!tabs.activeTabMissing());
    QCOMPARE(tabs.activePath(), moved);
    QCOMPARE(tabs.activeTitle(), QString("moved-lecture"));
    QVERIFY(changed.count() >= 1);                              // the UI reloads
    QCOMPARE(tabs.activePosition().value("page").toInt(), 12);  // same file, same remembered place
  }

  void LocateRefusesADifferentFileUnlessForced() {
    const QTemporaryDir dir;
    const QString original = MakeFile(dir, "lecture.pdf", 'l');
    const QString impostor = MakeFile(dir, "other.pdf", 'x');
    {
      sheaf::TabController tabs(*store_, *session_, kDelayMs);
      QVERIFY(tabs.openFile(Url(original)));
      tabs.Flush();
    }
    QVERIFY(QFile::remove(original));
    sheaf::TabController tabs(*store_, *session_, kDelayMs);
    tabs.Restore();
    const int id = tabs.activeTabId();
    QCOMPARE(tabs.locate(id, Url(impostor)), 2);
    QVERIFY(tabs.activeTabMissing());  // untouched
    QCOMPARE(tabs.activePath(), original);

    QCOMPARE(tabs.locate(id, Url(impostor), true), 0);
    QVERIFY(!tabs.activeTabMissing());
    QCOMPARE(tabs.activePath(), impostor);
    QCOMPARE(tabs.model()->FindTab(id)->file_key, sheaf::FileIdentity::Compute(impostor));
  }

  void LocateReportsUnreadableFilesAndUnknownTabs() {
    const QTemporaryDir dir;
    sheaf::TabController tabs(*store_, *session_, kDelayMs);
    QVERIFY(tabs.openFile(Url(MakeFile(dir, "a.pdf", 'a'))));
    QCOMPARE(tabs.locate(tabs.activeTabId(), Url("/nonexistent/x.pdf")), 1);
    QCOMPARE(tabs.locate(12345, Url(MakeFile(dir, "b.pdf", 'b'))), 3);
  }

  void LocatingAFileThatIsAlreadyOpenMergesTheTabs() {
    const QTemporaryDir dir;
    const QString lost = MakeFile(dir, "lost.pdf", 'l');
    const QString copy = dir.filePath("copy.pdf");
    {
      sheaf::TabController tabs(*store_, *session_, kDelayMs);
      QVERIFY(tabs.openFile(Url(lost)));
      tabs.Flush();
    }
    QVERIFY(QFile::copy(lost, copy));
    QVERIFY(QFile::remove(lost));
    sheaf::TabController tabs(*store_, *session_, kDelayMs);
    tabs.Restore();
    QVERIFY(tabs.openFile(Url(copy)));  // same content: the lost tab follows the copy
    QCOMPARE(tabs.tabCount(), 1);
    QVERIFY(!tabs.activeTabMissing());
    QCOMPARE(tabs.activePath(), copy);
  }

  void ReopeningAMovedFileRelinksTheLostTabInsteadOfAddingOne() {
    const QTemporaryDir dir;
    const QString original = MakeFile(dir, "a.pdf", 'a');
    {
      sheaf::TabController tabs(*store_, *session_, kDelayMs);
      QVERIFY(tabs.openFile(Url(original)));
      tabs.Flush();
    }
    const QString moved = dir.filePath("b.pdf");
    QVERIFY(QFile::rename(original, moved));
    sheaf::TabController tabs(*store_, *session_, kDelayMs);
    tabs.Restore();
    QVERIFY(tabs.activeTabMissing());
    QVERIFY(tabs.openFile(Url(moved)));
    QCOMPARE(tabs.tabCount(), 1);
    QVERIFY(!tabs.activeTabMissing());
    QCOMPARE(tabs.activePath(), moved);
  }

  void SettingsAreStoredByKey() {
    sheaf::TabController tabs(*store_, *session_, kDelayMs);
    QCOMPARE(tabs.value("window_width", 1100).toInt(), 1100);  // default when nothing is stored
    tabs.setValue("window_width", 1280);
    QCOMPARE(tabs.value("window_width", 1100).toInt(), 1280);
    tabs.setValue("window_maximized", true);
    QCOMPARE(tabs.value("window_maximized", false).toString(), QString("true"));
  }

  void CycleFollowsSidebarOrderAcrossGroupsAndWraps() {
    const QTemporaryDir dir;
    sheaf::TabController tabs(*store_, *session_, kDelayMs);
    tabs.newGroup("One");
    QVERIFY(tabs.openFile(Url(MakeFile(dir, "a.pdf", 'a'))));
    QVERIFY(tabs.openFile(Url(MakeFile(dir, "b.pdf", 'b'))));
    tabs.newGroup("Two");
    QVERIFY(tabs.openFile(Url(MakeFile(dir, "c.pdf", 'c'))));
    QCOMPARE(Order(tabs), QString("a b c"));
    QCOMPARE(tabs.activeTitle(), QString("c"));
    tabs.cycle(1);
    QCOMPARE(tabs.activeTitle(), QString("a"));
    tabs.cycle(1);
    QCOMPARE(tabs.activeTitle(), QString("b"));
    tabs.cycle(-1);
    tabs.cycle(-1);
    QCOMPARE(tabs.activeTitle(), QString("c"));
  }

  void ActivatingATabRevealsItsCollapsedGroup() {
    const QTemporaryDir dir;
    sheaf::TabController tabs(*store_, *session_, kDelayMs);
    const int group = tabs.newGroup("G");
    QVERIFY(tabs.openFile(Url(MakeFile(dir, "a.pdf", 'a'))));
    QVERIFY(tabs.openFile(Url(MakeFile(dir, "b.pdf", 'b'))));
    tabs.newGroup("Other");
    QVERIFY(tabs.openFile(Url(MakeFile(dir, "c.pdf", 'c'))));
    tabs.setCollapsed(group, true);
    QVERIFY(tabs.model()->FindGroup(group)->collapsed);
    tabs.cycle(1);  // wraps from c to a, inside the collapsed group
    QCOMPARE(tabs.activeTitle(), QString("a"));
    QVERIFY(!tabs.model()->FindGroup(group)->collapsed);
  }

  void ToggleCollapsedAndMoveGroup() {
    sheaf::TabController tabs(*store_, *session_, kDelayMs);
    const int a = tabs.newGroup("A");
    const int b = tabs.newGroup("B");
    tabs.toggleCollapsed(a);
    QVERIFY(tabs.model()->FindGroup(a)->collapsed);
    tabs.toggleCollapsed(a);
    QVERIFY(!tabs.model()->FindGroup(a)->collapsed);
    tabs.moveGroup(b, 0);
    QCOMPARE(tabs.model()->GroupIdAt(0), b);
  }

  void DeletingAGroupClosesItsTabsAndPicksANewActiveTab() {
    const QTemporaryDir dir;
    sheaf::TabController tabs(*store_, *session_, 60000);
    const int first = tabs.newGroup("First");
    QVERIFY(tabs.openFile(Url(MakeFile(dir, "a.pdf", 'a'))));
    const int second = tabs.newGroup("Second");
    QVERIFY(tabs.openFile(Url(MakeFile(dir, "b.pdf", 'b'))));
    QVERIFY(tabs.openFile(Url(MakeFile(dir, "c.pdf", 'c'))));
    const int third = tabs.newGroup("Third");
    QVERIFY(tabs.openFile(Url(MakeFile(dir, "d.pdf", 'd'))));
    tabs.activate(tabs.model()->FindGroup(second)->tabs.first().id);
    tabs.reportPosition(5, 0.5, 1);
    const QString key_b = tabs.model()->FindTab(tabs.activeTabId())->file_key;

    tabs.deleteGroup(second);  // contains the active tab
    QCOMPARE(Order(tabs), QString("a d"));
    QCOMPARE(tabs.activeTitle(), QString("d"));     // the tab that followed the removed ones
    QCOMPARE(store_->Load(key_b).value().page, 5);  // its position was saved first
    QVERIFY(tabs.model()->FindGroup(second) == nullptr);
    QVERIFY(tabs.model()->FindGroup(first) != nullptr);
    QVERIFY(tabs.model()->FindGroup(third) != nullptr);

    tabs.deleteGroup(third);
    QCOMPARE(tabs.activeTitle(), QString("a"));
    tabs.deleteGroup(first);
    QCOMPARE(tabs.activeTabId(), -1);
    QCOMPARE(tabs.model()->GroupCount(), 0);
    tabs.deleteGroup(first);  // already gone: ignored
  }

  void DeletingTheActiveGroupLeavesNoDanglingActiveGroup() {
    const QTemporaryDir dir;
    sheaf::TabController tabs(*store_, *session_, kDelayMs);
    const int group = tabs.newGroup("Only");
    tabs.deleteGroup(group);
    QCOMPARE(tabs.activeGroupId(), -1);
    // New files still have somewhere to go.
    QVERIFY(tabs.openFile(Url(MakeFile(dir, "a.pdf", 'a'))));
    QCOMPARE(tabs.model()->GroupCount(), 1);
  }

  void MoveActiveTabKeepsItActiveAndRevealsIt() {
    const QTemporaryDir dir;
    sheaf::TabController tabs(*store_, *session_, kDelayMs);
    const int one = tabs.newGroup("One");
    QVERIFY(tabs.openFile(Url(MakeFile(dir, "a.pdf", 'a'))));
    const int two = tabs.newGroup("Two");
    QVERIFY(tabs.openFile(Url(MakeFile(dir, "b.pdf", 'b'))));
    tabs.activate(tabs.model()->FindGroup(one)->tabs.first().id);
    tabs.setCollapsed(two, true);
    tabs.moveActiveTab(1);  // out of One, into collapsed Two
    QCOMPARE(tabs.activeTitle(), QString("a"));
    QCOMPARE(tabs.model()->GroupOfTab(tabs.activeTabId()), two);
    QVERIFY(!tabs.model()->FindGroup(two)->collapsed);
    QCOMPARE(tabs.activeGroupId(), two);
  }

  void DropMovesRows() {
    const QTemporaryDir dir;
    sheaf::TabController tabs(*store_, *session_, kDelayMs);
    tabs.newGroup("One");
    QVERIFY(tabs.openFile(Url(MakeFile(dir, "a.pdf", 'a'))));
    const int a = tabs.activeTabId();
    const int two = tabs.newGroup("Two");
    QVERIFY(tabs.openFile(Url(MakeFile(dir, "b.pdf", 'b'))));
    // rows: [One] a [Two] b ; drop a on the header of Two
    tabs.drop(false, a, 2, false);
    QCOMPARE(tabs.model()->GroupOfTab(a), two);
    QCOMPARE(Order(tabs), QString("b a"));
  }

  void ReopeningAfterRenameRestoresPosition() {
    const QTemporaryDir dir;
    const QString original = MakeFile(dir, "lecture.pdf", 'l');
    {
      sheaf::TabController tabs(*store_, *session_, kDelayMs);
      QVERIFY(tabs.openFile(Url(original)));
      tabs.reportPosition(42, 0.6, 1.8);
      tabs.closeTab(tabs.activeTabId());
    }
    const QString renamed = dir.filePath("renamed/script-v2.pdf");
    QVERIFY(QDir(dir.path()).mkdir("renamed"));
    QVERIFY(QFile::rename(original, renamed));

    sheaf::TabController tabs(*store_, *session_, kDelayMs);
    QVERIFY(tabs.openFile(Url(renamed)));
    const QVariantMap position = tabs.activePosition();
    QVERIFY(position.value("valid").toBool());
    QCOMPARE(position.value("page").toInt(), 42);
    QCOMPARE(position.value("offsetY").toDouble(), 0.6);
    QCOMPARE(position.value("zoom").toDouble(), 1.8);
  }

  void EachTabKeepsItsOwnHistory() {
    const QTemporaryDir dir;
    sheaf::TabController tabs(*store_, *session_, kDelayMs);
    QVERIFY(tabs.openFile(Url(MakeFile(dir, "a.pdf", 'a'))));
    const int a = tabs.activeTabId();
    QVERIFY(tabs.openFile(Url(MakeFile(dir, "b.pdf", 'b'))));
    const int b = tabs.activeTabId();
    const QSignalSpy history_changed(&tabs, &sheaf::TabController::historyChanged);

    tabs.activate(a);
    QVERIFY(!tabs.canGoBack());
    tabs.recordJump(5, 0.5, 1);
    tabs.recordJump(9, 0.1, 1);
    QVERIFY(tabs.canGoBack());
    QVERIFY(!tabs.canGoForward());

    tabs.activate(b);  // the other tab has no history
    QVERIFY(!tabs.canGoBack());
    QVERIFY(!tabs.goBack(3, 0, 1).value("valid").toBool());
    tabs.recordJump(2, 0.2, 1);

    tabs.activate(a);  // and tab a still has its own
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

  void HistorySurvivesMovingATabBetweenGroups() {
    const QTemporaryDir dir;
    sheaf::TabController tabs(*store_, *session_, kDelayMs);
    tabs.newGroup("One");
    QVERIFY(tabs.openFile(Url(MakeFile(dir, "a.pdf", 'a'))));
    const int a = tabs.activeTabId();
    const int two = tabs.newGroup("Two");
    tabs.recordJump(5, 0, 1);
    QVERIFY(tabs.canGoBack());
    tabs.model()->MoveTab(a, two, 0);
    QVERIFY(tabs.canGoBack());
  }

  void ClosingATabDropsItsHistory() {
    const QTemporaryDir dir;
    sheaf::TabController tabs(*store_, *session_, kDelayMs);
    const QString path = MakeFile(dir, "a.pdf", 'a');
    QVERIFY(tabs.openFile(Url(path)));
    tabs.recordJump(5, 0, 1);
    tabs.closeTab(tabs.activeTabId());
    QVERIFY(!tabs.canGoBack());
    QVERIFY(tabs.openFile(Url(path)));  // reopening starts with a clean history
    QVERIFY(!tabs.canGoBack());
  }

  // NOLINTNEXTLINE(readability-redundant-access-specifiers): moc needs `private:` after the slots
private:
  sheaf::Database db_{"tabs"};
  std::unique_ptr<sheaf::PositionStore> store_;
  std::unique_ptr<sheaf::SessionStore> session_;
};

QTEST_GUILESS_MAIN(TstTabController)
#include "tst_tabcontroller.moc"
