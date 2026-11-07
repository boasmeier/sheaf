#include "app/TabController.h"

#include "util/FileIdentity.h"

#include <QFileInfo>

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

TabListModel* TabController::model() {
  return &model_;
}

int TabController::activeIndex() const {
  return active_index_;
}

QUrl TabController::activeUrl() const {
  if (active_index_ < 0) {
    return {};
  }
  return QUrl::fromLocalFile(model_.At(active_index_).path);
}

QString TabController::activeTitle() const {
  return active_index_ < 0 ? QString() : model_.At(active_index_).title;
}

QVariantMap TabController::activePosition() const {
  return active_position_;
}

bool TabController::openFile(const QUrl& url) {
  const QString path = QFileInfo(url.toLocalFile()).absoluteFilePath();
  const QString key = FileIdentity::Compute(path);
  if (key.isEmpty()) {
    return false;
  }
  int index = model_.IndexOfKey(key);
  if (index < 0) {
    model_.Append({.path = path, .file_key = key, .title = QFileInfo(path).completeBaseName(), .history = {}});
    index = model_.Count() - 1;
  }
  activate(index);
  return true;
}

void TabController::closeTab(int index) {
  if (index < 0 || index >= model_.Count()) {
    return;
  }
  Flush();
  model_.RemoveAt(index);

  if (index < active_index_) {
    --active_index_;  // same tab, new index; nothing else changes
    return;
  }
  if (index > active_index_) {
    return;
  }
  // The active tab was closed: prefer the tab that moved into its place, else the previous one.
  active_index_ = -1;
  active_position_.clear();
  if (model_.Count() > 0) {
    activate(qMin(index, model_.Count() - 1));
  }
  else {
    emit activeTabChanged();
    emit historyChanged();
  }
}

void TabController::activate(int index) {
  if (index < 0 || index >= model_.Count() || index == active_index_) {
    return;
  }
  Flush();
  active_index_ = index;

  const auto stored = store_.Load(model_.At(index).file_key);
  active_position_ = stored ? ToMap(*stored) : NoPosition();
  emit activeTabChanged();
  emit historyChanged();
}

bool TabController::canGoBack() const {
  return active_index_ >= 0 && model_.At(active_index_).history.CanGoBack();
}

bool TabController::canGoForward() const {
  return active_index_ >= 0 && model_.At(active_index_).history.CanGoForward();
}

void TabController::recordJump(int page, double offset_y, double zoom) {
  if (active_index_ < 0) {
    return;
  }
  model_.HistoryAt(active_index_).RecordJump({.page = page, .offset_y = offset_y, .zoom = zoom});
  emit historyChanged();
}

QVariantMap TabController::goBack(int page, double offset_y, double zoom) {
  if (active_index_ < 0) {
    return NoPosition();
  }
  const auto target = model_.HistoryAt(active_index_).GoBack({.page = page, .offset_y = offset_y, .zoom = zoom});
  emit historyChanged();
  return target ? ToMap(*target) : NoPosition();
}

QVariantMap TabController::goForward(int page, double offset_y, double zoom) {
  if (active_index_ < 0) {
    return NoPosition();
  }
  const auto target = model_.HistoryAt(active_index_).GoForward({.page = page, .offset_y = offset_y, .zoom = zoom});
  emit historyChanged();
  return target ? ToMap(*target) : NoPosition();
}

void TabController::cycle(int delta) {
  const int count = model_.Count();
  if (count > 1 && active_index_ >= 0) {
    activate(((active_index_ + delta) % count + count) % count);
  }
}

void TabController::reportPosition(int page, double offset_y, double zoom) {
  if (active_index_ < 0) {
    return;
  }
  const Tab& tab = model_.At(active_index_);
  pending_ = {
      .file_key = tab.file_key, .path = tab.path, .position = {.page = page, .offset_y = offset_y, .zoom = zoom}};
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
