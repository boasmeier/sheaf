// Copyright (C) 2026 Boas Meier
// SPDX-License-Identifier: GPL-3.0-or-later

#include "app/SessionModel.h"

#include <QtGlobal>

#include <algorithm>
#include <utility>

namespace sheaf {

SessionModel::SessionModel(QObject* parent) : QAbstractListModel(parent) {
}

// --- Qt model interface ----------------------------------------------------------------------------------------

int SessionModel::rowCount(const QModelIndex& parent) const {
  return parent.isValid() ? 0 : static_cast<int>(rows_.size());
}

QVariant SessionModel::data(const QModelIndex& index, int role) const {
  if (!index.isValid() || index.row() < 0 || index.row() >= rows_.size()) {
    return {};
  }
  const Row row = rows_.at(index.row());
  if (row.kind == RowKind::kGroup) {
    const Group* group = FindGroup(row.id);
    if (group == nullptr) {  // only while a change is being reported
      return {};
    }
    switch (role) {
      case Qt::DisplayRole:
      case TitleRole:
        return group->name;
      case KindRole:
        return QStringLiteral("group");
      case IdRole:
      case GroupIdRole:
        return group->id;
      case CollapsedRole:
        return group->collapsed;
      case TabCountRole:
        return static_cast<int>(group->tabs.size());
      case ActiveRole:
      case MissingRole:
        return false;
      case PathRole:
        return QString();
      default:
        return {};
    }
  }
  const Tab* tab = FindTab(row.id);
  if (tab == nullptr) {
    return {};
  }
  switch (role) {
    case Qt::DisplayRole:
    case TitleRole:
      return tab->title;
    case KindRole:
      return QStringLiteral("tab");
    case IdRole:
      return tab->id;
    case GroupIdRole:
      return GroupOfTab(tab->id);
    case ActiveRole:
      return tab->id == active_tab_id_;
    case PathRole:
      return tab->path;
    case MissingRole:
      return tab->missing;
    case CollapsedRole:
      return false;
    case TabCountRole:
      return 0;
    default:
      return {};
  }
}

QHash<int, QByteArray> SessionModel::roleNames() const {
  return {{KindRole, "kind"},       {TitleRole, "title"},         {IdRole, "rowId"},
          {GroupIdRole, "groupId"}, {CollapsedRole, "collapsed"}, {TabCountRole, "tabCount"},
          {ActiveRole, "active"},   {PathRole, "path"},           {MissingRole, "missing"}};
}

// --- Change reporting --------------------------------------------------------------------------------------------

QVector<Row> SessionModel::BuildRows() const {
  QVector<Row> rows;
  for (const Group& group : groups_) {
    rows.append({.kind = RowKind::kGroup, .id = group.id});
    if (!group.collapsed) {
      for (const Tab& tab : group.tabs) {
        rows.append({.kind = RowKind::kTab, .id = tab.id});
      }
    }
  }
  return rows;
}

void SessionModel::ReportRowChanges(const QVector<Row>& old_rows) {
  const QVector<Row> new_rows = BuildRows();

  // Everything outside the changed middle stays; only that middle is removed and inserted.
  qsizetype prefix = 0;
  const qsizetype limit = std::min(old_rows.size(), new_rows.size());
  while (prefix < limit && old_rows.at(prefix) == new_rows.at(prefix)) {
    ++prefix;
  }
  qsizetype suffix = 0;
  while (suffix < limit - prefix &&
         old_rows.at(old_rows.size() - 1 - suffix) == new_rows.at(new_rows.size() - 1 - suffix)) {
    ++suffix;
  }
  const qsizetype removed = old_rows.size() - prefix - suffix;
  const qsizetype inserted = new_rows.size() - prefix - suffix;

  if (removed > 0) {
    beginRemoveRows({}, static_cast<int>(prefix), static_cast<int>(prefix + removed - 1));
    rows_.remove(prefix, removed);
    endRemoveRows();
  }
  if (inserted > 0) {
    beginInsertRows({}, static_cast<int>(prefix), static_cast<int>(prefix + inserted - 1));
    for (qsizetype i = 0; i < inserted; ++i) {
      rows_.insert(prefix + i, new_rows.at(prefix + i));
    }
    endInsertRows();
  }
  // Titles, collapsed state, tab counts and the active flag may have changed on rows that stayed.
  if (!rows_.isEmpty()) {
    emit dataChanged(index(0), index(static_cast<int>(rows_.size()) - 1));
  }
}

template <typename Mutation>
auto SessionModel::Mutate(Mutation&& mutation) {
  const QVector<Row> old_rows = rows_;
  auto result = std::forward<Mutation>(mutation)();
  ReportRowChanges(old_rows);
  return result;
}

// --- Structure -------------------------------------------------------------------------------------------------

Group* SessionModel::MutableGroup(int group_id) {
  for (Group& group : groups_) {
    if (group.id == group_id) {
      return &group;
    }
  }
  return nullptr;
}

int SessionModel::AddGroup(const QString& name) {
  return Mutate([&] {
    groups_.append({.id = next_group_id_++, .name = name, .collapsed = false, .tabs = {}});
    return groups_.last().id;
  });
}

bool SessionModel::RenameGroup(int group_id, const QString& name) {
  return Mutate([&] {
    Group* group = MutableGroup(group_id);
    if (group == nullptr || group->name == name) {
      return false;
    }
    group->name = name;
    return true;
  });
}

bool SessionModel::RemoveGroup(int group_id) {
  return Mutate([&] {
    const int index = IndexOfGroup(group_id);
    if (index < 0) {
      return false;
    }
    if (FindTab(active_tab_id_) != nullptr && GroupOfTab(active_tab_id_) == group_id) {
      active_tab_id_ = -1;
    }
    groups_.removeAt(index);
    return true;
  });
}

bool SessionModel::MoveGroup(int group_id, int index) {
  return Mutate([&] {
    const int from = IndexOfGroup(group_id);
    if (from < 0) {
      return false;
    }
    const int to = std::clamp(index, 0, static_cast<int>(groups_.size()) - 1);
    if (from == to) {
      return false;
    }
    groups_.move(from, to);
    return true;
  });
}

bool SessionModel::SetCollapsed(int group_id, bool collapsed) {
  return Mutate([&] {
    Group* group = MutableGroup(group_id);
    if (group == nullptr || group->collapsed == collapsed) {
      return false;
    }
    group->collapsed = collapsed;
    return true;
  });
}

int SessionModel::AddTab(int group_id, Tab tab, int index) {
  return Mutate([&] {
    Group* group = MutableGroup(group_id);
    if (group == nullptr) {
      return -1;
    }
    tab.id = next_tab_id_++;
    const int at =
        index < 0 ? static_cast<int>(group->tabs.size()) : std::clamp(index, 0, static_cast<int>(group->tabs.size()));
    const int id = tab.id;
    group->tabs.insert(at, std::move(tab));
    return id;
  });
}

bool SessionModel::RemoveTab(int tab_id) {
  return Mutate([&] {
    const int group_id = GroupOfTab(tab_id);
    if (group_id < 0) {
      return false;
    }
    Group* group = MutableGroup(group_id);
    group->tabs.removeAt(IndexInGroup(tab_id));
    if (active_tab_id_ == tab_id) {
      active_tab_id_ = -1;
    }
    return true;
  });
}

bool SessionModel::SetTabMissing(int tab_id, bool missing) {
  return Mutate([&] {
    Tab* tab = FindTab(tab_id);
    if (tab == nullptr || tab->missing == missing) {
      return false;
    }
    tab->missing = missing;
    return true;
  });
}

bool SessionModel::RelinkTab(int tab_id, const QString& path, const QString& file_key, const QString& title) {
  return Mutate([&] {
    Tab* tab = FindTab(tab_id);
    if (tab == nullptr) {
      return false;
    }
    tab->path = path;
    tab->file_key = file_key;
    tab->title = title;
    tab->missing = false;
    return true;
  });
}

bool SessionModel::MoveTab(int tab_id, int group_id, int index) {
  return Mutate([&] {
    const int from_group_id = GroupOfTab(tab_id);
    Group* to_group = MutableGroup(group_id);
    if (from_group_id < 0 || to_group == nullptr) {
      return false;
    }
    Group* from_group = MutableGroup(from_group_id);
    const int from_index = IndexInGroup(tab_id);
    const int to_index =
        std::clamp(index, 0, static_cast<int>(to_group->tabs.size()) - (from_group == to_group ? 1 : 0));
    if (from_group == to_group && from_index == to_index) {
      return false;
    }
    Tab tab = std::move(from_group->tabs[from_index]);
    from_group->tabs.removeAt(from_index);
    to_group->tabs.insert(to_index, std::move(tab));
    return true;
  });
}

bool SessionModel::MoveTabBy(int tab_id, int delta) {
  const int group_id = GroupOfTab(tab_id);
  if (group_id < 0 || (delta != 1 && delta != -1)) {
    return false;
  }
  const int index = IndexInGroup(tab_id);
  const int group_index = IndexOfGroup(group_id);
  const Group& group = groups_.at(group_index);
  if (delta > 0) {
    if (index + 1 < group.tabs.size()) {
      return MoveTab(tab_id, group_id, index + 1);
    }
    if (group_index + 1 < groups_.size()) {
      return MoveTab(tab_id, groups_.at(group_index + 1).id, 0);
    }
    return false;
  }
  if (index > 0) {
    return MoveTab(tab_id, group_id, index - 1);
  }
  if (group_index > 0) {
    const Group& previous = groups_.at(group_index - 1);
    return MoveTab(tab_id, previous.id, static_cast<int>(previous.tabs.size()));
  }
  return false;
}

bool SessionModel::Drop(Row dragged, int target_row, bool after) {
  if (target_row < 0 || target_row >= rows_.size()) {
    return false;
  }
  const Row target = rows_.at(target_row);

  if (dragged.kind == RowKind::kTab) {
    if (FindTab(dragged.id) == nullptr) {
      return false;
    }
    if (target.kind == RowKind::kGroup) {
      const Group* group = FindGroup(target.id);
      // Same group: "to the end" means after the remaining tabs.
      const int count = static_cast<int>(group->tabs.size());
      return MoveTab(dragged.id, target.id, GroupOfTab(dragged.id) == target.id ? count - 1 : count);
    }
    if (target.id == dragged.id) {
      return false;
    }
    const int group_id = GroupOfTab(target.id);
    // Position of the target among its group's tabs once the dragged tab has been taken out.
    int position = 0;
    for (const Tab& tab : FindGroup(group_id)->tabs) {
      if (tab.id == dragged.id) {
        continue;
      }
      if (tab.id == target.id) {
        break;
      }
      ++position;
    }
    return MoveTab(dragged.id, group_id, position + (after ? 1 : 0));
  }

  // A group dropped on any row goes before or after the group that row belongs to.
  if (FindGroup(dragged.id) == nullptr) {
    return false;
  }
  const int target_group = target.kind == RowKind::kGroup ? target.id : GroupOfTab(target.id);
  if (target_group == dragged.id) {
    return false;
  }
  int position = 0;
  for (const Group& group : groups_) {
    if (group.id == dragged.id) {
      continue;
    }
    if (group.id == target_group) {
      break;
    }
    ++position;
  }
  return MoveGroup(dragged.id, position + (after ? 1 : 0));
}

void SessionModel::SetActiveTab(int tab_id) {
  if (active_tab_id_ == tab_id) {
    return;
  }
  Mutate([&] {
    active_tab_id_ = FindTab(tab_id) != nullptr ? tab_id : -1;
    return 0;
  });
}

int SessionModel::ActiveTabId() const {
  return active_tab_id_;
}

// --- Queries ---------------------------------------------------------------------------------------------------

int SessionModel::GroupCount() const {
  return static_cast<int>(groups_.size());
}

int SessionModel::GroupIdAt(int index) const {
  return index >= 0 && index < groups_.size() ? groups_.at(index).id : -1;
}

int SessionModel::IndexOfGroup(int group_id) const {
  for (int i = 0; i < groups_.size(); ++i) {
    if (groups_.at(i).id == group_id) {
      return i;
    }
  }
  return -1;
}

const Group* SessionModel::FindGroup(int group_id) const {
  const int index = IndexOfGroup(group_id);
  return index < 0 ? nullptr : &groups_.at(index);
}

const Tab* SessionModel::FindTab(int tab_id) const {
  for (const Group& group : groups_) {
    for (const Tab& tab : group.tabs) {
      if (tab.id == tab_id) {
        return &tab;
      }
    }
  }
  return nullptr;
}

Tab* SessionModel::FindTab(int tab_id) {
  for (Group& group : groups_) {
    for (Tab& tab : group.tabs) {
      if (tab.id == tab_id) {
        return &tab;
      }
    }
  }
  return nullptr;
}

int SessionModel::GroupOfTab(int tab_id) const {
  for (const Group& group : groups_) {
    for (const Tab& tab : group.tabs) {
      if (tab.id == tab_id) {
        return group.id;
      }
    }
  }
  return -1;
}

int SessionModel::IndexInGroup(int tab_id) const {
  for (const Group& group : groups_) {
    for (int i = 0; i < group.tabs.size(); ++i) {
      if (group.tabs.at(i).id == tab_id) {
        return i;
      }
    }
  }
  return -1;
}

int SessionModel::TabCount() const {
  int count = 0;
  for (const Group& group : groups_) {
    count += static_cast<int>(group.tabs.size());
  }
  return count;
}

QVector<int> SessionModel::TabIds() const {
  QVector<int> ids;
  for (const Group& group : groups_) {
    for (const Tab& tab : group.tabs) {
      ids.append(tab.id);
    }
  }
  return ids;
}

int SessionModel::TabIdWithKey(const QString& file_key) const {
  for (const Group& group : groups_) {
    for (const Tab& tab : group.tabs) {
      if (tab.file_key == file_key) {
        return tab.id;
      }
    }
  }
  return -1;
}

const QVector<Row>& SessionModel::Rows() const {
  return rows_;
}

int SessionModel::tabRow(int tab_id) const {
  return RowOf({.kind = RowKind::kTab, .id = tab_id});
}

int SessionModel::groupRow(int group_id) const {
  return RowOf({.kind = RowKind::kGroup, .id = group_id});
}

int SessionModel::groupTabCount(int group_id) const {
  const Group* group = FindGroup(group_id);
  return group == nullptr ? 0 : static_cast<int>(group->tabs.size());
}

int SessionModel::groupIndex(int group_id) const {
  return IndexOfGroup(group_id);
}

int SessionModel::RowOf(Row row) const {
  return static_cast<int>(rows_.indexOf(row));
}

}  // namespace sheaf
