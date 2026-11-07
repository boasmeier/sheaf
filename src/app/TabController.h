#pragma once

#include "app/SessionModel.h"
#include "storage/PositionStore.h"

#include <QList>
#include <QObject>
#include <QTimer>
#include <QUrl>
#include <QVariantMap>

namespace sheaf {

// Owns the session (groups and tabs) and the active tab, and persists the active document's position (debounced).
// Only the active tab's document is ever loaded by the UI; switching tabs reloads it with the stored position.
// Tabs and groups are addressed by their stable ids (see SessionModel), never by position.
// NOLINTBEGIN(readability-identifier-naming): QML-facing API follows Qt naming.
class TabController : public QObject {
  Q_OBJECT
  Q_PROPERTY(SessionModel* model READ model CONSTANT)
  Q_PROPERTY(int activeTabId READ activeTabId NOTIFY activeTabChanged)
  // New files open in the active group: the group of the active tab, or the one last selected or created.
  Q_PROPERTY(int activeGroupId READ activeGroupId NOTIFY activeGroupChanged)
  Q_PROPERTY(QUrl activeUrl READ activeUrl NOTIFY activeTabChanged)
  Q_PROPERTY(QString activeTitle READ activeTitle NOTIFY activeTabChanged)
  Q_PROPERTY(bool canGoBack READ canGoBack NOTIFY historyChanged)
  Q_PROPERTY(bool canGoForward READ canGoForward NOTIFY historyChanged)
  // {valid, page, offsetY, zoom} stored for the active tab; valid is false for a document never seen before.
  Q_PROPERTY(QVariantMap activePosition READ activePosition NOTIFY activeTabChanged)

public:
  static constexpr int kDefaultSaveDelayMs = 500;

  TabController(PositionStore& store, int save_delay_ms = kDefaultSaveDelayMs, QObject* parent = nullptr);

  SessionModel* model();
  const SessionModel* model() const;
  int activeTabId() const;
  int activeGroupId() const;
  QUrl activeUrl() const;
  QString activeTitle() const;
  QVariantMap activePosition() const;
  bool canGoBack() const;
  bool canGoForward() const;

  // Writes any pending position immediately (tab switch, tab close, quit).
  void Flush();

  // --- Tabs ---
  // Opens the file in a new tab of the active group (creating the group "Ungrouped" if there is none), or activates
  // the tab that already shows the same file. False if unreadable.
  Q_INVOKABLE bool openFile(const QUrl& url);
  Q_INVOKABLE void closeTab(int tab_id);
  Q_INVOKABLE void activate(int tab_id);
  // Moves to the next (+1) or previous (-1) tab in sidebar order, wrapping around, e.g. +1 for Ctrl+Tab.
  Q_INVOKABLE void cycle(int delta);
  // Ids of all tabs in sidebar order.
  Q_INVOKABLE QList<int> tabIds() const;
  Q_INVOKABLE int tabCount() const;
  // Moves the active tab one place down (+1) or up (-1) in sidebar order, across group borders.
  Q_INVOKABLE void moveActiveTab(int delta);
  // Moves a tab to the end of a group (the menu alternative to dragging).
  Q_INVOKABLE void moveTabToGroup(int tab_id, int group_id);

  // --- Groups ---
  // Creates a group (named "Group N" if `name` is empty), makes it the active group and returns its id.
  Q_INVOKABLE int newGroup(const QString& name = {});
  Q_INVOKABLE void renameGroup(int group_id, const QString& name);
  // Removes the group and closes its tabs.
  Q_INVOKABLE void deleteGroup(int group_id);
  Q_INVOKABLE void selectGroup(int group_id);
  // All groups in order as [{id, name}].
  Q_INVOKABLE QVariantList groups() const;
  Q_INVOKABLE void setCollapsed(int group_id, bool collapsed);
  Q_INVOKABLE void toggleCollapsed(int group_id);
  Q_INVOKABLE void moveGroup(int group_id, int index);
  // Drag and drop of a row; `is_group` tells whether `row_id` is a group id or a tab id. See SessionModel::Drop.
  Q_INVOKABLE void drop(bool is_group, int row_id, int target_row, bool after);

  // --- Active document ---
  // Reports the current view of the active document; written to the store after a quiet period.
  Q_INVOKABLE void reportPosition(int page, double offset_y, double zoom);
  // Link navigation, per tab. recordJump() takes the position being left; goBack()/goForward() take the current
  // position and return the one to go to ({valid:false} if there is none).
  Q_INVOKABLE void recordJump(int page, double offset_y, double zoom);
  Q_INVOKABLE QVariantMap goBack(int page, double offset_y, double zoom);
  Q_INVOKABLE QVariantMap goForward(int page, double offset_y, double zoom);

signals:
  void activeTabChanged();
  void activeGroupChanged();
  void historyChanged();
  // NOLINTEND(readability-identifier-naming)

private:
  struct Pending {
    QString file_key;
    QString path;
    Position position;
  };

  // Makes `tab_id` the active tab, or none for -1, and loads its stored position.
  void SetActive(int tab_id);
  void SetActiveGroup(int group_id);
  // The tab to show after the tab at sidebar position `position` went away (-1 if no tabs are left).
  int NeighbourAfterRemoval(int position) const;

  SessionModel model_;
  PositionStore& store_;
  QTimer save_timer_;
  int active_group_id_ = -1;
  QVariantMap active_position_;
  bool has_pending_ = false;
  Pending pending_;
};

}  // namespace sheaf
