// Copyright (C) 2026 Boas Meier
// SPDX-License-Identifier: GPL-3.0-or-later

#include "storage/SessionStore.h"

#include <QSqlError>
#include <QSqlQuery>

namespace sheaf {

namespace {

// Settings keys holding the active group / tab of the saved session.
constexpr const char* kActiveGroupKey = "session_active_group";
constexpr const char* kActiveTabGroupKey = "session_active_tab_group";
constexpr const char* kActiveTabKey = "session_active_tab";

bool Exec(QSqlQuery& query) {
  return query.exec();
}

}  // namespace

SessionStore::SessionStore(Database& database) : database_(database) {
}

bool SessionStore::Save(const SessionRecord& session) {
  QSqlDatabase db = database_.Handle();
  if (!db.transaction()) {
    return false;
  }
  const auto fail = [&] {
    db.rollback();
    return false;
  };

  QSqlQuery clear(db);
  if (!clear.exec("DELETE FROM tabs") || !clear.exec("DELETE FROM groups")) {
    return fail();
  }

  QSqlQuery insert_group(db);
  insert_group.prepare("INSERT INTO groups (name, position, collapsed) VALUES (:name, :position, :collapsed)");
  QSqlQuery insert_tab(db);
  insert_tab.prepare("INSERT INTO tabs (group_id, path, file_key, position) VALUES (:group, :path, :key, :position)");

  for (int g = 0; g < session.groups.size(); ++g) {
    const GroupRecord& group = session.groups.at(g);
    insert_group.bindValue(":name", group.name);
    insert_group.bindValue(":position", g);
    insert_group.bindValue(":collapsed", group.collapsed ? 1 : 0);
    if (!Exec(insert_group)) {
      return fail();
    }
    const QVariant group_id = insert_group.lastInsertId();
    for (int t = 0; t < group.tabs.size(); ++t) {
      insert_tab.bindValue(":group", group_id);
      insert_tab.bindValue(":path", group.tabs.at(t).path);
      insert_tab.bindValue(":key", group.tabs.at(t).file_key);
      insert_tab.bindValue(":position", t);
      if (!Exec(insert_tab)) {
        return fail();
      }
    }
  }

  QSqlQuery setting(db);
  setting.prepare(
      "INSERT INTO settings (key, value) VALUES (:key, :value) "
      "ON CONFLICT(key) DO UPDATE SET value = :value");
  const auto set = [&](const char* key, int value) {
    setting.bindValue(":key", key);
    setting.bindValue(":value", QString::number(value));
    return Exec(setting);
  };
  if (!set(kActiveGroupKey, session.active_group) || !set(kActiveTabGroupKey, session.active_tab_group) ||
      !set(kActiveTabKey, session.active_tab)) {
    return fail();
  }
  return db.commit() || fail();
}

SessionRecord SessionStore::Load() const {
  SessionRecord session;
  QSqlQuery groups(database_.Handle());
  if (!groups.exec("SELECT id, name, collapsed FROM groups ORDER BY position")) {
    return session;
  }
  QVector<int> group_ids;
  while (groups.next()) {
    group_ids.append(groups.value(0).toInt());
    session.groups.append({.name = groups.value(1).toString(), .collapsed = groups.value(2).toInt() != 0, .tabs = {}});
  }
  for (int g = 0; g < group_ids.size(); ++g) {
    QSqlQuery tabs(database_.Handle());
    tabs.prepare("SELECT path, file_key FROM tabs WHERE group_id = :group ORDER BY position");
    tabs.bindValue(":group", group_ids.at(g));
    if (!tabs.exec()) {
      continue;
    }
    while (tabs.next()) {
      session.groups[g].tabs.append({.path = tabs.value(0).toString(), .file_key = tabs.value(1).toString()});
    }
  }

  const auto index = [&](const char* key) {
    const std::optional<QString> text = Value(key);
    return text ? text->toInt() : -1;
  };
  session.active_group = index(kActiveGroupKey);
  session.active_tab_group = index(kActiveTabGroupKey);
  session.active_tab = index(kActiveTabKey);
  if (session.active_group < 0 || session.active_group >= session.groups.size()) {
    session.active_group = -1;
  }
  const bool tab_valid = session.active_tab_group >= 0 && session.active_tab_group < session.groups.size() &&
                         session.active_tab >= 0 &&
                         session.active_tab < session.groups.at(session.active_tab_group).tabs.size();
  if (!tab_valid) {
    session.active_tab_group = -1;
    session.active_tab = -1;
  }
  return session;
}

std::optional<QString> SessionStore::Value(const QString& key) const {
  QSqlQuery query(database_.Handle());
  query.prepare("SELECT value FROM settings WHERE key = :key");
  query.bindValue(":key", key);
  if (!query.exec() || !query.next()) {
    return std::nullopt;
  }
  return query.value(0).toString();
}

bool SessionStore::SetValue(const QString& key, const QString& value) {
  QSqlQuery query(database_.Handle());
  query.prepare(
      "INSERT INTO settings (key, value) VALUES (:key, :value) ON CONFLICT(key) DO UPDATE SET value = :value");
  query.bindValue(":key", key);
  query.bindValue(":value", value);
  return query.exec();
}

}  // namespace sheaf
