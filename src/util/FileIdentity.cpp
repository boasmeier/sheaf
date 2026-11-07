// Copyright (C) 2026 Boas Meier
// SPDX-License-Identifier: GPL-3.0-or-later

#include "util/FileIdentity.h"

#include <QByteArray>
#include <QCryptographicHash>
#include <QFile>

namespace sheaf {

QString FileIdentity::Compute(const QString& path) {
  QFile file(path);
  if (!file.open(QIODevice::ReadOnly)) {
    return {};
  }

  const qint64 size = file.size();
  QCryptographicHash hash(QCryptographicHash::Sha256);
  hash.addData(QByteArray::number(size));

  const QByteArray head = file.read(kSampleSize);
  hash.addData(head);

  if (size > kSampleSize) {
    if (!file.seek(qMax(kSampleSize, size - kSampleSize))) {
      return {};
    }
    hash.addData(file.read(kSampleSize));
  }
  return QString::fromLatin1(hash.result().toHex());
}

}  // namespace sheaf
