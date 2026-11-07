// Copyright (C) 2026 Boas Meier
// SPDX-License-Identifier: GPL-3.0-or-later

#include "app/TabController.h"

#include "util/FileIdentity.h"

#include <QFileInfo>

#include <algorithm>
#include <limits>
#include <utility>

namespace sheaf {

namespace {

QVariantMap ToMap(const Position& position) {
  return {{"valid", true}, {"page", position.page}, {"offsetY", position.offset_y}, {"zoom", position.zoom}};
}

QVariantMap NoPosition() {
  return {{"valid", false}};
}

}  // namespace

TabController::TabController(PositionStore& positions, SessionStore& session, int save_delay_ms, QObject* parent)
    : QObject(parent), model_(this), store_(positions), session_(session) {
  save_timer_.setSingleShot(true);
  save_timer_.setInterval(save_delay_ms);
  connect(&save_timer_, &QTimer::timeout, this, [this] { Flush(); });

  // Session changes are infrequent, so they are saved right away: a timer of 0 ms only merges the several signals one
  // user action causes into a single write.
  session_timer_.setSingleShot(true);
  session_timer_.setInterval(0);
  connect(&session_timer_, &QTimer::timeout, this, [this] { SaveSession(); });
  const auto queue = [this] { QueueSessionSave(); };
  connect(&model_, &QAbstractItemModel::rowsInserted, this, queue);
  connect(&model_, &QAbstractItemModel::rowsRemoved, this, queue);
  connect(&model_, &QAbstractItemModel::dataChanged, this, queue);
  connect(this, &TabController::activeTabChanged, this, queue);
  connect(this, &TabController::activeGroupChanged, this, queue);
}

void TabController::QueueSessionSave() {
  if (restoring_) {
    return;
  }
  session_dirty_ = true;
  session_timer_.start();
}

void TabController::SaveSession() {
  session_timer_.stop();
  session_dirty_ = false;
  SessionRecord record;
  for (int g = 0; g < model_.GroupCount(); ++g) {
    const Group* group = model_.FindGroup(model_.GroupIdAt(g));
    GroupRecord saved{.name = group->name, .collapsed = group->collapsed, .tabs = {}};
    for (const Tab& tab : group->tabs) {
      saved.tabs.append({.path = tab.path, .file_key = tab.file_key});
    }
    record.groups.append(saved);
  }
  record.active_group = model_.IndexOfGroup(active_group_id_);
  const int active_tab = model_.ActiveTabId();
  if (active_tab >= 0) {
    record.active_tab_group = model_.IndexOfGroup(model_.GroupOfTab(active_tab));
    record.active_tab = model_.IndexInGroup(active_tab);
  }
  session_.Save(record);
}

void TabController::Restore() {
  restoring_ = true;
  const SessionRecord record = session_.Load();
  int active_tab_id = -1;
  int active_group_id = -1;
  for (int g = 0; g < record.groups.size(); ++g) {
    const GroupRecord& group = record.groups.at(g);
    const int group_id = model_.AddGroup(group.name);
    model_.SetCollapsed(group_id, group.collapsed);
    if (g == record.active_group) {
      active_group_id = group_id;
    }
    for (int t = 0; t < group.tabs.size(); ++t) {
      const TabRecord& tab = group.tabs.at(t);
      const int tab_id = model_.AddTab(group_id, {.id = 0,
                                                  .path = tab.path,
                                                  .file_key = tab.file_key,
                                                  .title = QFileInfo(tab.path).completeBaseName(),
                                                  .history = {},
                                                  .missing = !QFileInfo::exists(tab.path),
                                                  .search = {}});
      if (g == record.active_tab_group && t == record.active_tab) {
        active_tab_id = tab_id;
      }
    }
  }
  restoring_ = false;
  if (active_group_id >= 0) {
    SetActiveGroup(active_group_id);
  }
  if (active_tab_id >= 0) {
    // Not revealed: the sidebar must look exactly as it was left, even with the active tab in a collapsed group.
    SetActive(active_tab_id, false);
  }
}

SessionModel* TabController::model() {
  return &model_;
}

const SessionModel* TabController::model() const {
  return &model_;
}

int TabController::activeTabId() const {
  return model_.ActiveTabId();
}

int TabController::activeGroupId() const {
  return active_group_id_;
}

QUrl TabController::activeUrl() const {
  const Tab* tab = model_.FindTab(model_.ActiveTabId());
  return tab == nullptr ? QUrl() : QUrl::fromLocalFile(tab->path);
}

bool TabController::activeTabMissing() const {
  const Tab* tab = model_.FindTab(model_.ActiveTabId());
  return tab != nullptr && tab->missing;
}

QVariantMap TabController::activeSearch() const {
  const Tab* tab = model_.FindTab(model_.ActiveTabId());
  const SearchState state = tab == nullptr ? SearchState{} : tab->search;
  return {{"open", state.open}, {"query", state.query}, {"page", state.page}, {"start", state.start}};
}

void TabController::setSearchState(bool open, const QString& query, int page, int start) {
  Tab* tab = model_.FindTab(model_.ActiveTabId());
  if (tab != nullptr) {
    tab->search = {.open = open, .query = query, .page = page, .start = start};
  }
}

QString TabController::activePath() const {
  const Tab* tab = model_.FindTab(model_.ActiveTabId());
  return tab == nullptr ? QString() : tab->path;
}

QString TabController::activeTitle() const {
  const Tab* tab = model_.FindTab(model_.ActiveTabId());
  return tab == nullptr ? QString() : tab->title;
}

QVariantMap TabController::activePosition() const {
  return active_position_;
}

// --- Tabs ------------------------------------------------------------------------------------------------------

bool TabController::openFile(const QUrl& url) {
  const QString path = QFileInfo(url.toLocalFile()).absoluteFilePath();
  const QString key = FileIdentity::Compute(path);
  if (key.isEmpty()) {
    return false;
  }
  int tab_id = model_.TabIdWithKey(key);
  if (tab_id >= 0 && model_.FindTab(tab_id)->missing) {
    // The same file turned up somewhere else (or is back): the tab that lost it follows it.
    model_.RelinkTab(tab_id, path, key, QFileInfo(path).completeBaseName());
    if (tab_id == model_.ActiveTabId()) {
      ReloadActive();
    }
  }
  if (tab_id < 0) {
    int group_id = model_.FindGroup(active_group_id_) != nullptr ? active_group_id_ : -1;
    if (group_id < 0) {
      group_id = model_.GroupCount() > 0 ? model_.GroupIdAt(0) : model_.AddGroup(tr("Ungrouped"));
      SetActiveGroup(group_id);
    }
    tab_id = model_.AddTab(group_id, {.id = 0,
                                      .path = path,
                                      .file_key = key,
                                      .title = QFileInfo(path).completeBaseName(),
                                      .history = {},
                                      .missing = false,
                                      .search = {}});
  }
  activate(tab_id);
  return true;
}

int TabController::NeighbourAfterRemoval(int position) const {
  const QVector<int> ids = model_.TabIds();
  if (ids.isEmpty()) {
    return -1;
  }
  return ids.at(std::min(position, static_cast<int>(ids.size()) - 1));
}

void TabController::closeTab(int tab_id) {
  if (model_.FindTab(tab_id) == nullptr) {
    return;
  }
  Flush();
  const bool was_active = tab_id == model_.ActiveTabId();
  const int position = static_cast<int>(model_.TabIds().indexOf(tab_id));
  model_.RemoveTab(tab_id);
  if (was_active) {
    // Prefer the tab that moved into its place, else the previous one.
    SetActive(NeighbourAfterRemoval(position));
  }
}

void TabController::activate(int tab_id) {
  const Tab* tab = model_.FindTab(tab_id);
  if (tab == nullptr || tab_id == model_.ActiveTabId()) {
    return;
  }
  Flush();
  // The file may have gone, or come back, since the tab was last shown.
  model_.SetTabMissing(tab_id, !QFileInfo::exists(tab->path));
  SetActive(tab_id);
}

void TabController::ReloadActive() {
  const Tab* tab = model_.FindTab(model_.ActiveTabId());
  active_position_ = NoPosition();
  if (tab != nullptr) {
    if (const auto stored = store_.Load(tab->file_key)) {
      active_position_ = ToMap(*stored);
    }
  }
  emit activeTabChanged();
  emit historyChanged();
}

int TabController::locate(int tab_id, const QUrl& url, bool force) {
  const Tab* tab = model_.FindTab(tab_id);
  if (tab == nullptr) {
    return 3;
  }
  const QString path = QFileInfo(url.toLocalFile()).absoluteFilePath();
  const QString key = FileIdentity::Compute(path);
  if (key.isEmpty()) {
    return 1;
  }
  if (!force && !tab->file_key.isEmpty() && key != tab->file_key) {
    return 2;
  }
  const int other = model_.TabIdWithKey(key);
  if (other >= 0 && other != tab_id) {
    // That file is already open in another tab: keep that one and drop the lost one.
    const bool was_active = tab_id == model_.ActiveTabId();
    closeTab(tab_id);
    if (was_active) {
      activate(other);
    }
    return 0;
  }
  model_.RelinkTab(tab_id, path, key, QFileInfo(path).completeBaseName());
  if (tab_id == model_.ActiveTabId()) {
    ReloadActive();
  }
  return 0;
}

QVariant TabController::value(const QString& key, const QVariant& default_value) const {
  const std::optional<QString> stored = session_.Value(key);
  return stored ? QVariant(*stored) : default_value;
}

void TabController::setValue(const QString& key, const QVariant& value) {
  session_.SetValue(key, value.toString());
}

void TabController::SetActive(int tab_id, bool reveal) {
  model_.SetActiveTab(tab_id);
  active_position_ = NoPosition();
  if (const Tab* tab = model_.FindTab(tab_id)) {
    if (const auto stored = store_.Load(tab->file_key)) {
      active_position_ = ToMap(*stored);
    }
    // A tab inside a collapsed group would be invisible, so show it.
    const int group_id = model_.GroupOfTab(tab_id);
    if (reveal) {
      model_.SetCollapsed(group_id, false);
    }
    SetActiveGroup(group_id);
  }
  emit activeTabChanged();
  emit historyChanged();
}

void TabController::SetActiveGroup(int group_id) {
  if (group_id != active_group_id_) {
    active_group_id_ = group_id;
    emit activeGroupChanged();
  }
}

void TabController::cycle(int delta) {
  const QVector<int> ids = model_.TabIds();
  const int count = static_cast<int>(ids.size());
  const qsizetype current = ids.indexOf(model_.ActiveTabId());
  if (count > 1 && current >= 0) {
    activate(ids.at(static_cast<int>(((current + delta) % count + count) % count)));
  }
}

QList<int> TabController::tabIds() const {
  return model_.TabIds();
}

int TabController::tabCount() const {
  return model_.TabCount();
}

void TabController::moveActiveTab(int delta) {
  model_.MoveTabBy(model_.ActiveTabId(), delta);
  // The tab may have landed in a collapsed group.
  const int group_id = model_.GroupOfTab(model_.ActiveTabId());
  if (group_id >= 0) {
    model_.SetCollapsed(group_id, false);
    SetActiveGroup(group_id);
  }
}

void TabController::moveTabToGroup(int tab_id, int group_id) {
  if (model_.MoveTab(tab_id, group_id, std::numeric_limits<int>::max())) {
    model_.SetCollapsed(group_id, false);
    if (tab_id == model_.ActiveTabId()) {
      SetActiveGroup(group_id);
    }
  }
}

QVariantList TabController::groups() const {
  QVariantList result;
  for (int i = 0; i < model_.GroupCount(); ++i) {
    const Group* group = model_.FindGroup(model_.GroupIdAt(i));
    result.append(QVariantMap{{"id", group->id}, {"name", group->name}});
  }
  return result;
}

// --- Groups ----------------------------------------------------------------------------------------------------

int TabController::newGroup(const QString& name) {
  const QString title = name.trimmed().isEmpty() ? tr("Group %1").arg(model_.GroupCount() + 1) : name.trimmed();
  const int id = model_.AddGroup(title);
  SetActiveGroup(id);
  return id;
}

void TabController::renameGroup(int group_id, const QString& name) {
  if (!name.trimmed().isEmpty()) {
    model_.RenameGroup(group_id, name.trimmed());
  }
}

void TabController::deleteGroup(int group_id) {
  const Group* group = model_.FindGroup(group_id);
  if (group == nullptr) {
    return;
  }
  Flush();
  const QVector<int> order = model_.TabIds();
  const bool active_goes = model_.GroupOfTab(model_.ActiveTabId()) == group_id;
  // Where the group's tabs started in sidebar order; the next tab after them takes over.
  const int first_removed = group->tabs.isEmpty() ? 0 : static_cast<int>(order.indexOf(group->tabs.first().id));
  model_.RemoveGroup(group_id);
  if (active_group_id_ == group_id) {
    active_group_id_ = -1;
    // Keep new files going somewhere sensible: the group that took its place, if any.
    emit activeGroupChanged();
  }
  if (active_goes) {
    SetActive(NeighbourAfterRemoval(first_removed));
  }
}

void TabController::selectGroup(int group_id) {
  if (model_.FindGroup(group_id) != nullptr) {
    SetActiveGroup(group_id);
  }
}

void TabController::setCollapsed(int group_id, bool collapsed) {
  model_.SetCollapsed(group_id, collapsed);
}

void TabController::toggleCollapsed(int group_id) {
  if (const Group* group = model_.FindGroup(group_id)) {
    model_.SetCollapsed(group_id, !group->collapsed);
  }
}

void TabController::moveGroup(int group_id, int index) {
  model_.MoveGroup(group_id, index);
}

void TabController::drop(bool is_group, int row_id, int target_row, bool after) {
  model_.Drop({.kind = is_group ? RowKind::kGroup : RowKind::kTab, .id = row_id}, target_row, after);
}

// --- Active document -------------------------------------------------------------------------------------------

bool TabController::canGoBack() const {
  const Tab* tab = model_.FindTab(model_.ActiveTabId());
  return tab != nullptr && tab->history.CanGoBack();
}

bool TabController::canGoForward() const {
  const Tab* tab = model_.FindTab(model_.ActiveTabId());
  return tab != nullptr && tab->history.CanGoForward();
}

void TabController::recordJump(int page, double offset_y, double zoom) {
  Tab* tab = model_.FindTab(model_.ActiveTabId());
  if (tab == nullptr) {
    return;
  }
  tab->history.RecordJump({.page = page, .offset_y = offset_y, .zoom = zoom});
  emit historyChanged();
}

QVariantMap TabController::goBack(int page, double offset_y, double zoom) {
  Tab* tab = model_.FindTab(model_.ActiveTabId());
  if (tab == nullptr) {
    return NoPosition();
  }
  const auto target = tab->history.GoBack({.page = page, .offset_y = offset_y, .zoom = zoom});
  emit historyChanged();
  return target ? ToMap(*target) : NoPosition();
}

QVariantMap TabController::goForward(int page, double offset_y, double zoom) {
  Tab* tab = model_.FindTab(model_.ActiveTabId());
  if (tab == nullptr) {
    return NoPosition();
  }
  const auto target = tab->history.GoForward({.page = page, .offset_y = offset_y, .zoom = zoom});
  emit historyChanged();
  return target ? ToMap(*target) : NoPosition();
}

void TabController::reportPosition(int page, double offset_y, double zoom) {
  const Tab* tab = model_.FindTab(model_.ActiveTabId());
  if (tab == nullptr) {
    return;
  }
  pending_ = {
      .file_key = tab->file_key, .path = tab->path, .position = {.page = page, .offset_y = offset_y, .zoom = zoom}};
  has_pending_ = true;
  save_timer_.start();
}

void TabController::Flush() {
  if (session_dirty_) {
    SaveSession();
  }
  save_timer_.stop();
  if (!has_pending_) {
    return;
  }
  has_pending_ = false;
  store_.Save(pending_.file_key, pending_.path, pending_.position);
}

}  // namespace sheaf
