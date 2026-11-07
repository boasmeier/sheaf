#pragma once

#include "app/TabListModel.h"
#include "storage/PositionStore.h"

#include <QObject>
#include <QTimer>
#include <QUrl>
#include <QVariantMap>

namespace sheaf {

// Owns the open tabs and the active one, and persists the active document's position (debounced).
// Only the active tab's document is ever loaded by the UI; switching tabs reloads it with the stored position.
// NOLINTBEGIN(readability-identifier-naming): QML-facing API follows Qt naming.
class TabController : public QObject {
  Q_OBJECT
  Q_PROPERTY(TabListModel* model READ model CONSTANT)
  Q_PROPERTY(int activeIndex READ activeIndex NOTIFY activeTabChanged)
  Q_PROPERTY(QUrl activeUrl READ activeUrl NOTIFY activeTabChanged)
  Q_PROPERTY(QString activeTitle READ activeTitle NOTIFY activeTabChanged)
  Q_PROPERTY(bool canGoBack READ canGoBack NOTIFY historyChanged)
  Q_PROPERTY(bool canGoForward READ canGoForward NOTIFY historyChanged)
  // {valid, page, offsetY, zoom} stored for the active tab; valid is false for a document never seen before.
  Q_PROPERTY(QVariantMap activePosition READ activePosition NOTIFY activeTabChanged)

public:
  static constexpr int kDefaultSaveDelayMs = 500;

  TabController(PositionStore& store, int save_delay_ms = kDefaultSaveDelayMs, QObject* parent = nullptr);

  TabListModel* model();
  int activeIndex() const;
  QUrl activeUrl() const;
  QString activeTitle() const;
  QVariantMap activePosition() const;
  bool canGoBack() const;
  bool canGoForward() const;

  // Writes any pending position immediately (tab switch, tab close, quit).
  void Flush();

  // Opens the file in a new tab, or activates the tab that already shows the same file. False if unreadable.
  Q_INVOKABLE bool openFile(const QUrl& url);
  Q_INVOKABLE void closeTab(int index);
  Q_INVOKABLE void activate(int index);
  // Moves the active tab by `delta` (wrapping around), e.g. +1 for Ctrl+Tab.
  Q_INVOKABLE void cycle(int delta);
  // Reports the current view of the active document; written to the store after a quiet period.
  Q_INVOKABLE void reportPosition(int page, double offset_y, double zoom);
  // Link navigation, per tab. recordJump() takes the position being left; goBack()/goForward() take the current
  // position and return the one to go to ({valid:false} if there is none).
  Q_INVOKABLE void recordJump(int page, double offset_y, double zoom);
  Q_INVOKABLE QVariantMap goBack(int page, double offset_y, double zoom);
  Q_INVOKABLE QVariantMap goForward(int page, double offset_y, double zoom);

signals:
  void activeTabChanged();
  void historyChanged();
  // NOLINTEND(readability-identifier-naming)

private:
  struct Pending {
    QString file_key;
    QString path;
    Position position;
  };

  TabListModel model_;
  PositionStore& store_;
  QTimer save_timer_;
  int active_index_ = -1;
  QVariantMap active_position_;
  bool has_pending_ = false;
  Pending pending_;
};

}  // namespace sheaf
