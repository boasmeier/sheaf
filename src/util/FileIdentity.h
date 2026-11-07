// Copyright (C) 2026 Boas Meier
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <QString>

namespace sheaf {

// Stable identity of a file that survives renames and moves: SHA-256 over the file size plus its first and last
// 64 KiB. Files that differ only in the middle (same size, same head and tail) share a key; that is accepted.
class FileIdentity {
public:
  static constexpr qint64 kSampleSize = qint64{64} * 1024;

  // Hex-encoded key, or an empty string if the file cannot be read.
  static QString Compute(const QString& path);
};

}  // namespace sheaf
