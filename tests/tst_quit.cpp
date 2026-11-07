// Copyright (C) 2026 Boas Meier
// SPDX-License-Identifier: GPL-3.0-or-later

// Starts the real application (headless) and ends it the ways a desktop does: SIGTERM at logout or shutdown, SIGINT,
// SIGHUP. The session must be on disk afterwards and come back at the next start.
#include "Fixtures.h"
#include "storage/Database.h"
#include "storage/SessionStore.h"

#include <QProcess>
#include <QSqlDatabase>
#include <QSqlQuery>
#include <QTemporaryDir>
#include <QtTest>

#include <csignal>
#include <sys/types.h>

namespace {

#ifndef SHEAF_BINARY
#error "SHEAF_BINARY must point at the application"
#endif

// The number of saved tabs, read without disturbing the running application; -1 if the database is not there yet.
int SavedTabCount(const QString& path) {
  if (!QFile::exists(path)) {
    return -1;
  }
  const QString name = "poll";
  int count = -1;
  {
    QSqlDatabase db = QSqlDatabase::addDatabase("QSQLITE", name);
    db.setDatabaseName(path);
    db.setConnectOptions("QSQLITE_OPEN_READONLY");
    if (db.open()) {
      QSqlQuery query(db);
      if (query.exec("SELECT COUNT(*) FROM tabs") && query.next()) {
        count = query.value(0).toInt();
      }
    }
  }
  QSqlDatabase::removeDatabase(name);
  return count;
}

}  // namespace

class TstQuit : public QObject {
  Q_OBJECT

private:
  struct Result {
    bool exited_normally = false;
    int exit_code = -1;
  };

  // Runs the application with `arguments`, waits until it has shown its window, sends `signal_number` and waits for it
  // to finish. `expect_tabs` is how many saved tabs show that start-up is complete (0 = just wait a while).
  static Result RunAndSignal(const QString& data_home, const QStringList& arguments, int signal_number,
                             int expect_tabs) {
    QProcess process;
    QProcessEnvironment environment = QProcessEnvironment::systemEnvironment();
    environment.insert("XDG_DATA_HOME", data_home);
    environment.insert("QT_QPA_PLATFORM", "offscreen");
    environment.insert("QT_QUICK_BACKEND", "software");
    environment.insert("QT_LOGGING_RULES", "qt.pdf.links=false");
    process.setProcessEnvironment(environment);
    process.setProcessChannelMode(QProcess::MergedChannels);
    process.start(SHEAF_BINARY, arguments);
    if (!process.waitForStarted(10000)) {
      return {};
    }
    const QString database = data_home + "/sheaf/sheaf/sheaf.db";
    if (expect_tabs > 0) {
      for (int i = 0; i < 200 && SavedTabCount(database) < expect_tabs; ++i) {
        QTest::qWait(50);
      }
    }
    QTest::qWait(2500);  // let the window and the QML finish loading
    ::kill(static_cast<pid_t>(process.processId()), signal_number);
    const bool finished = process.waitForFinished(15000);
    if (!finished) {
      process.kill();
      process.waitForFinished(5000);
      return {};
    }
    return {.exited_normally = process.exitStatus() == QProcess::NormalExit, .exit_code = process.exitCode()};
  }

  static sheaf::SessionRecord LoadSession(const QString& data_home, QString* window_width = nullptr) {
    sheaf::Database database("check");
    if (!database.Open(data_home + "/sheaf/sheaf/sheaf.db")) {
      return {};
    }
    const sheaf::SessionStore store(database);
    if (window_width != nullptr) {
      *window_width = store.Value("window_width").value_or(QString());
    }
    return store.Load();
  }

private slots:  // NOLINT(readability-redundant-access-specifiers): moc needs `private slots:` after the helpers
  void SigtermSavesTheSessionAndTheWindowAndExitsCleanly() {
    const QTemporaryDir dir;
    const QString a = dir.filePath("a.pdf");
    const QString b = dir.filePath("b.pdf");
    QVERIFY(sheaf::testing::WritePdfFixture(a, "A", 3, false));
    QVERIFY(sheaf::testing::WritePdfFixture(b, "B", 3, false));
    const QString data = dir.filePath("data");

    const Result result = RunAndSignal(data, {a, b}, SIGTERM, 2);
    QVERIFY2(result.exited_normally, "the application was killed instead of quitting");
    QCOMPARE(result.exit_code, 0);

    QString window_width;
    const sheaf::SessionRecord session = LoadSession(data, &window_width);
    QCOMPARE(session.groups.size(), 1);
    QCOMPARE(session.groups.first().name, QString("Ungrouped"));
    QCOMPARE(session.groups.first().tabs.size(), 2);
    QCOMPARE(session.groups.first().tabs.at(0).path, a);
    QCOMPARE(session.groups.first().tabs.at(1).path, b);
    QCOMPARE(session.active_tab, 0);  // the first file given is the one shown
    QVERIFY2(window_width.toInt() >= 480, qPrintable("window width saved as '" + window_width + "'"));
  }

  void AnotherStartRestoresTheSessionAndKeepsAMissingFile() {
    const QTemporaryDir dir;
    const QString a = dir.filePath("a.pdf");
    const QString b = dir.filePath("b.pdf");
    QVERIFY(sheaf::testing::WritePdfFixture(a, "A", 3, false));
    QVERIFY(sheaf::testing::WritePdfFixture(b, "B", 3, false));
    const QString data = dir.filePath("data");

    QVERIFY(RunAndSignal(data, {a, b}, SIGTERM, 2).exited_normally);
    const sheaf::SessionRecord first = LoadSession(data);
    QCOMPARE(first.groups.first().tabs.size(), 2);

    QVERIFY(QFile::remove(b));  // the second document disappears between the runs
    const Result second = RunAndSignal(data, {}, SIGTERM, 0);
    QVERIFY2(second.exited_normally, "the second start did not quit cleanly");
    QCOMPARE(second.exit_code, 0);

    // Nothing was lost, not even the tab whose file is gone, and the active tab is the same.
    QCOMPARE(LoadSession(data), first);

    const Result third = RunAndSignal(data, {}, SIGTERM, 0);  // and once more
    QVERIFY(third.exited_normally);
    QCOMPARE(LoadSession(data), first);
  }

  void FilesGivenOnTheCommandLineJoinTheRestoredSession() {
    const QTemporaryDir dir;
    const QString a = dir.filePath("a.pdf");
    const QString c = dir.filePath("c.pdf");
    QVERIFY(sheaf::testing::WritePdfFixture(a, "A", 3, false));
    QVERIFY(sheaf::testing::WritePdfFixture(c, "C", 3, false));
    const QString data = dir.filePath("data");

    QVERIFY(RunAndSignal(data, {a}, SIGTERM, 1).exited_normally);
    QVERIFY(RunAndSignal(data, {c}, SIGTERM, 2).exited_normally);
    const sheaf::SessionRecord session = LoadSession(data);
    QCOMPARE(session.groups.size(), 1);
    QCOMPARE(session.groups.first().tabs.size(), 2);
    QCOMPARE(session.groups.first().tabs.at(1).path, c);
    QCOMPARE(session.active_tab, 1);  // the newly opened file is shown
  }

  // NOLINTNEXTLINE(readability-identifier-naming): Qt Test data function name
  void SigintAndSighupAlsoQuitCleanly_data() {
    QTest::addColumn<int>("signal_number");
    QTest::newRow("SIGINT") << SIGINT;
    QTest::newRow("SIGHUP") << SIGHUP;
  }

  void SigintAndSighupAlsoQuitCleanly() {
    // NOLINTNEXTLINE(misc-const-correctness): declared by the macro
    QFETCH(int, signal_number);
    const QTemporaryDir dir;
    const QString a = dir.filePath("a.pdf");
    QVERIFY(sheaf::testing::WritePdfFixture(a, "A", 3, false));
    const QString data = dir.filePath("data");
    const Result result = RunAndSignal(data, {a}, signal_number, 1);
    QVERIFY2(result.exited_normally, "the application was killed instead of quitting");
    QCOMPARE(result.exit_code, 0);
    QCOMPARE(LoadSession(data).groups.first().tabs.size(), 1);
  }
};

QTEST_MAIN(TstQuit)
#include "tst_quit.moc"
