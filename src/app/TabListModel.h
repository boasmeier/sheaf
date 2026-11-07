#pragma once

#include <QAbstractListModel>
#include <QString>
#include <QVector>

namespace sheaf {

// A tab is just a path, a file key and a title until it is first activated; its document is loaded lazily.
struct Tab {
  QString path;
  QString file_key;
  QString title;
};

// Flat list of tabs for QML (M4 replaces this with the group/tab tree).
// NOLINTBEGIN(readability-identifier-naming): Qt model interface and QML role names.
class TabListModel : public QAbstractListModel {
  Q_OBJECT

public:
  enum Role { TitleRole = Qt::UserRole + 1, PathRole };

  explicit TabListModel(QObject* parent = nullptr);

  int rowCount(const QModelIndex& parent = {}) const override;
  QVariant data(const QModelIndex& index, int role) const override;
  QHash<int, QByteArray> roleNames() const override;
  // NOLINTEND(readability-identifier-naming)

  int Count() const;
  const Tab& At(int index) const;
  int IndexOfKey(const QString& file_key) const;
  void Append(Tab tab);
  void RemoveAt(int index);

private:
  QVector<Tab> tabs_;
};

}  // namespace sheaf
