#include "storage/SessionStore.h"

#include <QSqlQuery>
#include <QTemporaryDir>
#include <QtTest>

namespace {

sheaf::SessionRecord Sample() {
  return {.groups = {{.name = "Physics",
                      .collapsed = false,
                      .tabs = {{.path = "/p/one.pdf", .file_key = "k1"}, {.path = "/p/two.pdf", .file_key = "k2"}}},
                     {.name = "Maths", .collapsed = true, .tabs = {{.path = "/m/a.pdf", .file_key = "k3"}}},
                     {.name = "Empty", .collapsed = false, .tabs = {}}},
          .active_group = 1,
          .active_tab_group = 0,
          .active_tab = 1};
}

}  // namespace

class TstSessionStore : public QObject {
  Q_OBJECT

private slots:
  void NothingSavedLoadsAsEmptySession() {
    sheaf::Database db("ss1");
    QVERIFY(db.Open(":memory:"));
    const sheaf::SessionStore store(db);
    const sheaf::SessionRecord session = store.Load();
    QVERIFY(session.groups.isEmpty());
    QCOMPARE(session.active_group, -1);
    QCOMPARE(session.active_tab, -1);
  }

  void RoundTripKeepsOrderCollapsedStateAndActiveTab() {
    sheaf::Database db("ss2");
    QVERIFY(db.Open(":memory:"));
    sheaf::SessionStore store(db);
    QVERIFY(store.Save(Sample()));
    QCOMPARE(store.Load(), Sample());
  }

  void SaveReplacesThePreviousSession() {
    sheaf::Database db("ss3");
    QVERIFY(db.Open(":memory:"));
    sheaf::SessionStore store(db);
    QVERIFY(store.Save(Sample()));
    const sheaf::SessionRecord smaller{.groups = {{.name = "Only", .collapsed = false, .tabs = {{"/x.pdf", "kx"}}}},
                                       .active_group = 0,
                                       .active_tab_group = 0,
                                       .active_tab = 0};
    QVERIFY(store.Save(smaller));
    QCOMPARE(store.Load(), smaller);
    QSqlQuery count(db.Handle());
    QVERIFY(count.exec("SELECT (SELECT COUNT(*) FROM groups), (SELECT COUNT(*) FROM tabs)"));
    QVERIFY(count.next());
    QCOMPARE(count.value(0).toInt(), 1);
    QCOMPARE(count.value(1).toInt(), 1);
  }

  void SavingAnEmptySessionClearsEverything() {
    sheaf::Database db("ss4");
    QVERIFY(db.Open(":memory:"));
    sheaf::SessionStore store(db);
    QVERIFY(store.Save(Sample()));
    QVERIFY(store.Save({}));
    QVERIFY(store.Load().groups.isEmpty());
  }

  void TabsWithTheSamePathAndOrderStayDistinctPerGroup() {
    sheaf::Database db("ss5");
    QVERIFY(db.Open(":memory:"));
    sheaf::SessionStore store(db);
    const sheaf::SessionRecord session{.groups = {{.name = "A", .collapsed = false, .tabs = {{"/a.pdf", "k"}}},
                                                  {.name = "A", .collapsed = false, .tabs = {{"/a.pdf", "k"}}}},
                                       .active_group = -1,
                                       .active_tab_group = -1,
                                       .active_tab = -1};
    QVERIFY(store.Save(session));
    QCOMPARE(store.Load(), session);
  }

  void ActiveIndicesThatPointNowhereAreDropped() {
    sheaf::Database db("ss6");
    QVERIFY(db.Open(":memory:"));
    sheaf::SessionStore store(db);
    sheaf::SessionRecord session = Sample();
    session.active_group = 9;
    session.active_tab_group = 0;
    session.active_tab = 7;  // group 0 has two tabs
    QVERIFY(store.Save(session));
    const sheaf::SessionRecord loaded = store.Load();
    QCOMPARE(loaded.active_group, -1);
    QCOMPARE(loaded.active_tab_group, -1);
    QCOMPARE(loaded.active_tab, -1);
    QCOMPARE(loaded.groups.size(), 3);
  }

  void SettingsRoundTripAndReplace() {
    sheaf::Database db("ss7");
    QVERIFY(db.Open(":memory:"));
    sheaf::SessionStore store(db);
    QVERIFY(!store.Value("window_width").has_value());
    QVERIFY(store.SetValue("window_width", "1280"));
    QCOMPARE(store.Value("window_width").value(), QString("1280"));
    QVERIFY(store.SetValue("window_width", "900"));
    QCOMPARE(store.Value("window_width").value(), QString("900"));
    // Settings and session do not disturb each other.
    QVERIFY(store.Save(Sample()));
    QCOMPARE(store.Value("window_width").value(), QString("900"));
  }

  void SurvivesReopeningTheDatabaseFile() {
    const QTemporaryDir dir;
    const QString path = dir.filePath("sheaf.db");
    {
      sheaf::Database db("ss8a");
      QVERIFY(db.Open(path));
      sheaf::SessionStore store(db);
      QVERIFY(store.Save(Sample()));
      QVERIFY(store.SetValue("window_width", "777"));
    }
    sheaf::Database db("ss8b");
    QVERIFY(db.Open(path));
    const sheaf::SessionStore store(db);
    QCOMPARE(store.Load(), Sample());
    QCOMPARE(store.Value("window_width").value(), QString("777"));
  }

  void AFailedSaveLeavesTheOldSessionIntact() {
    sheaf::Database db("ss9");
    QVERIFY(db.Open(":memory:"));
    sheaf::SessionStore store(db);
    QVERIFY(store.Save(Sample()));
    // Make the next save fail halfway: a trigger rejects inserting a group named "boom".
    QSqlQuery trigger(db.Handle());
    QVERIFY(
        trigger.exec("CREATE TRIGGER reject BEFORE INSERT ON groups WHEN NEW.name = 'boom' "
                     "BEGIN SELECT RAISE(ABORT, 'rejected'); END"));
    sheaf::SessionRecord broken = Sample();
    broken.groups.append({.name = "boom", .collapsed = false, .tabs = {}});
    QVERIFY(!store.Save(broken));
    QCOMPARE(store.Load(), Sample());  // rolled back: still the complete old session
  }
};

QTEST_GUILESS_MAIN(TstSessionStore)
#include "tst_sessionstore.moc"
