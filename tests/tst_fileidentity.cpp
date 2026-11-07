// Copyright (C) 2026 Boas Meier
// SPDX-License-Identifier: GPL-3.0-or-later

#include "util/FileIdentity.h"

#include <QFile>
#include <QTemporaryDir>
#include <QtTest>

namespace {

void Write(const QString& path, const QByteArray& data) {
  QFile file(path);
  QVERIFY(file.open(QIODevice::WriteOnly));
  file.write(data);
}

}  // namespace

class TstFileIdentity : public QObject {
  Q_OBJECT

private slots:
  void SameKeyAfterRenameAndMove() {
    const QTemporaryDir dir;
    QVERIFY(QDir(dir.path()).mkdir("sub"));
    const QByteArray content(200000, 'x');
    Write(dir.filePath("a.pdf"), content);
    const QString before = sheaf::FileIdentity::Compute(dir.filePath("a.pdf"));
    QVERIFY(!before.isEmpty());

    QVERIFY(QFile::rename(dir.filePath("a.pdf"), dir.filePath("b.pdf")));
    QCOMPARE(sheaf::FileIdentity::Compute(dir.filePath("b.pdf")), before);
    QVERIFY(QFile::rename(dir.filePath("b.pdf"), dir.filePath("sub/c.pdf")));
    QCOMPARE(sheaf::FileIdentity::Compute(dir.filePath("sub/c.pdf")), before);
  }

  void DifferentFilesDifferentKeys() {
    const QTemporaryDir dir;
    Write(dir.filePath("a"), "hello");
    Write(dir.filePath("b"), "hellp");        // same size, different content
    Write(dir.filePath("c"), "hello world");  // different size
    const QString a = sheaf::FileIdentity::Compute(dir.filePath("a"));
    QVERIFY(a != sheaf::FileIdentity::Compute(dir.filePath("b")));
    QVERIFY(a != sheaf::FileIdentity::Compute(dir.filePath("c")));
  }

  void DetectsChangesInHeadAndTailOfLargeFiles() {
    const QTemporaryDir dir;
    const QByteArray content(300000, 'x');
    Write(dir.filePath("orig"), content);
    QByteArray head = content;
    head[10] = 'y';
    Write(dir.filePath("head"), head);
    QByteArray tail = content;
    tail[tail.size() - 10] = 'y';
    Write(dir.filePath("tail"), tail);

    const QString orig = sheaf::FileIdentity::Compute(dir.filePath("orig"));
    QVERIFY(orig != sheaf::FileIdentity::Compute(dir.filePath("head")));
    QVERIFY(orig != sheaf::FileIdentity::Compute(dir.filePath("tail")));
  }

  void SmallAndEmptyFilesWork() {
    const QTemporaryDir dir;
    Write(dir.filePath("empty"), "");
    QVERIFY(!sheaf::FileIdentity::Compute(dir.filePath("empty")).isEmpty());
  }

  void MissingFileGivesEmptyKey() { QVERIFY(sheaf::FileIdentity::Compute("/nonexistent/file.pdf").isEmpty()); }
};

QTEST_GUILESS_MAIN(TstFileIdentity)
#include "tst_fileidentity.moc"
