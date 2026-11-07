// Copyright (C) 2026 Boas Meier
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include "app/NavigationHistory.h"

#include <QAbstractListModel>
#include <QString>
#include <QVector>

namespace sheaf {

// The search of one tab: whether its search bar is open, what was searched for and which match was the current one
// (page and offset of the match in the page's text). In memory only, like the navigation history.
struct SearchState {
  bool open = false;
  QString query;
  int page = -1;
  int start = 0;
};

// A tab is just a path, a file key and a title until it is first activated; its document is loaded lazily.
struct Tab {
  int id = 0;
  QString path;
  QString file_key;
  QString title;
  NavigationHistory history;  // in memory only
  bool missing = false;       // the file is not at `path` (any more)
  SearchState search;
};

struct Group {
  int id = 0;
  QString name;
  bool collapsed = false;
  QVector<Tab> tabs;
};

// What a visible row of the sidebar is.
enum class RowKind { kGroup, kTab };

struct Row {
  RowKind kind = RowKind::kGroup;
  int id = 0;  // group id or tab id, depending on kind

  bool operator==(const Row&) const = default;
};

// The groups and their tabs, in order, with collapsed state and the active tab.
//
// The model exposes the *visible rows* as a flat list for a QML ListView: each group is a header row followed by its
// tabs unless the group is collapsed. Changes are reported as precise row insertions and removals, so a view keeps its
// scroll position. Every mutation keeps the tree consistent: ids are unique and stable, a tab belongs to exactly one
// group, and positions are plain list indices.
// NOLINTBEGIN(readability-identifier-naming): Qt model interface and QML role names.
class SessionModel : public QAbstractListModel {
  Q_OBJECT

public:
  enum Role {
    KindRole = Qt::UserRole + 1,  // "group" or "tab"
    TitleRole,
    IdRole,         // group id or tab id
    GroupIdRole,    // the group (for a group row: its own id)
    CollapsedRole,  // groups only
    TabCountRole,   // groups only
    ActiveRole,     // tabs only: is this the active tab
    PathRole,       // tabs only
    MissingRole,    // tabs only: the file was not found
  };

  explicit SessionModel(QObject* parent = nullptr);

  int rowCount(const QModelIndex& parent = {}) const override;
  QVariant data(const QModelIndex& index, int role) const override;
  QHash<int, QByteArray> roleNames() const override;
  // NOLINTEND(readability-identifier-naming)

  // --- Structure -----------------------------------------------------------------------------------------------
  // Appends a group; returns its id.
  int AddGroup(const QString& name);
  bool RenameGroup(int group_id, const QString& name);
  // Removes the group together with its tabs.
  bool RemoveGroup(int group_id);
  // Moves the group so it ends up at `index` among the groups (clamped).
  bool MoveGroup(int group_id, int index);
  bool SetCollapsed(int group_id, bool collapsed);

  // Adds the tab to a group at `index` (clamped; -1 appends) and gives it a fresh id, which is returned (-1 if the
  // group does not exist).
  int AddTab(int group_id, Tab tab, int index = -1);
  bool RemoveTab(int tab_id);
  // Marks the tab's file as found or not found.
  bool SetTabMissing(int tab_id, bool missing);
  // Points the tab at another file (relinking after the file moved): new path, key and title; no longer missing.
  bool RelinkTab(int tab_id, const QString& path, const QString& file_key, const QString& title);
  // Moves the tab so it ends up at `index` within the group (clamped), possibly another group.
  bool MoveTab(int tab_id, int group_id, int index);
  // Moves the tab one place down (+1) or up (-1) in the order of all tabs; past the end of a group it moves into the
  // neighbouring group. False at the very ends.
  bool MoveTabBy(int tab_id, int delta);
  // Drag and drop: the row `dragged` was dropped on visible row `target_row`, on its lower half if `after`.
  //  - tab on tab: next to that tab (same or other group); tab on group header: at the end of that group;
  //  - group on any row: before / after the group of that row.
  // False if nothing changed.
  bool Drop(Row dragged, int target_row, bool after);

  void SetActiveTab(int tab_id);
  int ActiveTabId() const;

  // --- Queries -------------------------------------------------------------------------------------------------
  int GroupCount() const;
  int GroupIdAt(int index) const;
  int IndexOfGroup(int group_id) const;  // -1 if unknown
  const Group* FindGroup(int group_id) const;
  const Tab* FindTab(int tab_id) const;
  Tab* FindTab(int tab_id);
  int GroupOfTab(int tab_id) const;  // -1 if unknown
  int IndexInGroup(int tab_id) const;
  int TabCount() const;
  // Every tab id in visual order (all groups, collapsed ones included).
  QVector<int> TabIds() const;
  int TabIdWithKey(const QString& file_key) const;  // -1 if none
  // Row numbers for QML (-1 if the row is not visible).
  // NOLINTBEGIN(readability-identifier-naming): QML-facing API follows Qt naming.
  Q_INVOKABLE int tabRow(int tab_id) const;
  Q_INVOKABLE int groupRow(int group_id) const;
  Q_INVOKABLE int groupIndex(int group_id) const;  // position among the groups
  Q_INVOKABLE int groupTabCount(int group_id) const;
  // NOLINTEND(readability-identifier-naming)
  // The visible rows, top to bottom.
  const QVector<Row>& Rows() const;
  int RowOf(Row row) const;  // -1 if not visible

private:
  Group* MutableGroup(int group_id);
  QVector<Row> BuildRows() const;
  // Runs `mutation` on the tree and reports the resulting change of the visible rows.
  template <typename Mutation>
  auto Mutate(Mutation&& mutation);
  void ReportRowChanges(const QVector<Row>& old_rows);

  QVector<Group> groups_;
  QVector<Row> rows_;
  int next_group_id_ = 1;
  int next_tab_id_ = 1;
  int active_tab_id_ = -1;
};

}  // namespace sheaf
