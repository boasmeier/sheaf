// Copyright (C) 2026 Boas Meier
// SPDX-License-Identifier: GPL-3.0-or-later

#include "storage/Database.h"

#include <QSqlQuery>
#include <QTemporaryDir>
#include <QtTest>

class TstDatabase : public QObject {
  Q_OBJECT

private slots:
  void OpensInMemoryAndMigrates() {
    sheaf::Database db("mem");
    QVERIFY2(db.Open(":memory:"), qPrintable(db.Error()));
    QCOMPARE(db.SchemaVersion(), sheaf::Database::LatestSchemaVersion());
    for (const char* table : {"documents", "groups", "tabs", "settings"}) {
      QSqlQuery query(db.Handle());
      QVERIFY2(query.exec(QString("SELECT COUNT(*) FROM %1").arg(table)), table);
    }
  }

  void ReopeningKeepsDataAndVersion() {
    const QTemporaryDir dir;
    QVERIFY(dir.isValid());
    const QString path = dir.filePath("sub/sheaf.db");
    {
      sheaf::Database db("first");
      QVERIFY2(db.Open(path), qPrintable(db.Error()));
      QSqlQuery query(db.Handle());
      QVERIFY(query.exec("INSERT INTO groups (name, position) VALUES ('Ungrouped', 0)"));
    }
    sheaf::Database db("second");
    QVERIFY2(db.Open(path), qPrintable(db.Error()));
    QCOMPARE(db.SchemaVersion(), sheaf::Database::LatestSchemaVersion());
    QSqlQuery query(db.Handle());
    QVERIFY(query.exec("SELECT name FROM groups"));
    QVERIFY(query.next());
    QCOMPARE(query.value(0).toString(), QString("Ungrouped"));
  }

  void ForeignKeysCascade() {
    sheaf::Database db("fk");
    QVERIFY(db.Open(":memory:"));
    QSqlQuery query(db.Handle());
    QVERIFY(query.exec("INSERT INTO groups (id, name, position) VALUES (1, 'g', 0)"));
    QVERIFY(query.exec("INSERT INTO tabs (group_id, path, position) VALUES (1, '/a.pdf', 0)"));
    QVERIFY(query.exec("DELETE FROM groups WHERE id = 1"));
    QVERIFY(query.exec("SELECT COUNT(*) FROM tabs"));
    QVERIFY(query.next());
    QCOMPARE(query.value(0).toInt(), 0);
  }

  void RejectsNewerSchema() {
    const QTemporaryDir dir;
    const QString path = dir.filePath("sheaf.db");
    {
      sheaf::Database db("seed");
      QVERIFY(db.Open(path));
      QSqlQuery query(db.Handle());
      QVERIFY(query.exec("UPDATE schema_version SET version = 999"));
    }
    sheaf::Database db("newer");
    QVERIFY(!db.Open(path));
    QVERIFY(!db.Error().isEmpty());
  }
};

QTEST_GUILESS_MAIN(TstDatabase)
#include "tst_database.moc"
