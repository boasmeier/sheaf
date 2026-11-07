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

TabController::TabController(PositionStore& store, int save_delay_ms, QObject* parent)
    : QObject(parent), model_(this), store_(store) {
  save_timer_.setSingleShot(true);
  save_timer_.setInterval(save_delay_ms);
  connect(&save_timer_, &QTimer::timeout, this, [this] { Flush(); });
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
  if (tab_id < 0) {
    int group_id = model_.FindGroup(active_group_id_) != nullptr ? active_group_id_ : -1;
    if (group_id < 0) {
      group_id = model_.GroupCount() > 0 ? model_.GroupIdAt(0) : model_.AddGroup(tr("Ungrouped"));
      SetActiveGroup(group_id);
    }
    tab_id = model_.AddTab(
        group_id, {.id = 0, .path = path, .file_key = key, .title = QFileInfo(path).completeBaseName(), .history = {}});
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
  if (model_.FindTab(tab_id) == nullptr || tab_id == model_.ActiveTabId()) {
    return;
  }
  Flush();
  SetActive(tab_id);
}

void TabController::SetActive(int tab_id) {
  model_.SetActiveTab(tab_id);
  active_position_ = NoPosition();
  if (const Tab* tab = model_.FindTab(tab_id)) {
    if (const auto stored = store_.Load(tab->file_key)) {
      active_position_ = ToMap(*stored);
    }
    // A tab inside a collapsed group would be invisible, so show it.
    const int group_id = model_.GroupOfTab(tab_id);
    model_.SetCollapsed(group_id, false);
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
  save_timer_.stop();
  if (!has_pending_) {
    return;
  }
  has_pending_ = false;
  store_.Save(pending_.file_key, pending_.path, pending_.position);
}

}  // namespace sheaf
