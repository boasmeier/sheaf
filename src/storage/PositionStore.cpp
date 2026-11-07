// Copyright (C) 2026 Boas Meier
// SPDX-License-Identifier: GPL-3.0-or-later

#include "storage/PositionStore.h"

#include <QDateTime>
#include <QSqlQuery>

namespace sheaf {

PositionStore::PositionStore(Database& database) : database_(database) {
}

bool PositionStore::Save(const QString& file_key, const QString& path, const Position& position) {
  QSqlQuery query(database_.Handle());
  query.prepare(
      "INSERT INTO documents (file_key, last_path, page, offset_y, zoom, updated_at) "
      "VALUES (:key, :path, :page, :offset, :zoom, :now) "
      "ON CONFLICT(file_key) DO UPDATE SET last_path = :path, page = :page, offset_y = :offset, zoom = :zoom, "
      "updated_at = :now");
  query.bindValue(":key", file_key);
  query.bindValue(":path", path);
  query.bindValue(":page", position.page);
  query.bindValue(":offset", position.offset_y);
  query.bindValue(":zoom", position.zoom);
  query.bindValue(":now", QDateTime::currentSecsSinceEpoch());
  return query.exec();
}

std::optional<Position> PositionStore::Load(const QString& file_key) const {
  QSqlQuery query(database_.Handle());
  query.prepare("SELECT page, offset_y, zoom FROM documents WHERE file_key = :key");
  query.bindValue(":key", file_key);
  if (!query.exec() || !query.next()) {
    return std::nullopt;
  }
  return Position{
      .page = query.value(0).toInt(), .offset_y = query.value(1).toDouble(), .zoom = query.value(2).toDouble()};
}

}  // namespace sheaf
