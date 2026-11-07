// Copyright (C) 2026 Boas Meier
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include "storage/Database.h"

#include <QString>

#include <optional>

namespace sheaf {

// Where the user was in a document. `offset_y` is the scrolled fraction (0..1) of page `page`.
struct Position {
  int page = 0;
  double offset_y = 0;
  double zoom = 1;

  bool operator==(const Position&) const = default;
};

// Remembers the position per document, keyed by FileIdentity so moved or renamed files keep theirs.
class PositionStore {
public:
  explicit PositionStore(Database& database);

  // Inserts or replaces the position for `file_key` and records the file's current path.
  bool Save(const QString& file_key, const QString& path, const Position& position);
  std::optional<Position> Load(const QString& file_key) const;

private:
  Database& database_;
};

}  // namespace sheaf
