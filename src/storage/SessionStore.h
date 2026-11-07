// Copyright (C) 2026 Boas Meier
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include "storage/Database.h"

#include <QString>
#include <QVector>

#include <optional>

namespace sheaf {

struct TabRecord {
  QString path;
  QString file_key;

  bool operator==(const TabRecord&) const = default;
};

struct GroupRecord {
  QString name;
  bool collapsed = false;
  QVector<TabRecord> tabs;

  bool operator==(const GroupRecord&) const = default;
};

// The persistent form of the sidebar: groups and tabs in order, and which of them were active. Indices (not ids) are
// stored, because ids are only meaningful within one run.
struct SessionRecord {
  QVector<GroupRecord> groups;
  int active_group = -1;      // index into groups
  int active_tab_group = -1;  // index into groups ...
  int active_tab = -1;        // ... and into that group's tabs

  bool operator==(const SessionRecord&) const = default;
};

// Saves and loads the session (tables `groups`, `tabs`) and small named settings (table `settings`, e.g. window
// geometry). A session is replaced as a whole inside one transaction, so a crash never leaves half of one.
class SessionStore {
public:
  explicit SessionStore(Database& database);

  bool Save(const SessionRecord& session);
  // An empty session if nothing was saved. Indices that do not point at anything are reset to -1.
  SessionRecord Load() const;

  std::optional<QString> Value(const QString& key) const;
  bool SetValue(const QString& key, const QString& value);

private:
  Database& database_;
};

}  // namespace sheaf
