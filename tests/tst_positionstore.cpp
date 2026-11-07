#include "storage/PositionStore.h"

#include <QSqlQuery>
#include <QtTest>

class TstPositionStore : public QObject {
  Q_OBJECT

private slots:
  void UnknownKeyHasNoPosition() {
    sheaf::Database db("ps1");
    QVERIFY(db.Open(":memory:"));
    const sheaf::PositionStore store(db);
    QVERIFY(!store.Load("nope").has_value());
  }

  void RoundTrip() {
    sheaf::Database db("ps2");
    QVERIFY(db.Open(":memory:"));
    sheaf::PositionStore store(db);
    const sheaf::Position position{249, 0.375, 1.75};
    QVERIFY(store.Save("key", "/a.pdf", position));
    QCOMPARE(store.Load("key").value(), position);
  }

  void SaveReplacesAndKeepsOtherDocuments() {
    sheaf::Database db("ps3");
    QVERIFY(db.Open(":memory:"));
    sheaf::PositionStore store(db);
    QVERIFY(store.Save("one", "/a.pdf", {1, 0.1, 1}));
    QVERIFY(store.Save("two", "/b.pdf", {2, 0.2, 2}));
    QVERIFY(store.Save("one", "/moved/a.pdf", {5, 0.5, 3}));

    QCOMPARE(store.Load("one").value(), (sheaf::Position{5, 0.5, 3}));
    QCOMPARE(store.Load("two").value(), (sheaf::Position{2, 0.2, 2}));

    QSqlQuery query(db.Handle());
    QVERIFY(query.exec("SELECT last_path FROM documents WHERE file_key = 'one'"));
    QVERIFY(query.next());
    QCOMPARE(query.value(0).toString(), QString("/moved/a.pdf"));
  }
};

QTEST_GUILESS_MAIN(TstPositionStore)
#include "tst_positionstore.moc"
