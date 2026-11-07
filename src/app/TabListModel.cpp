#include "app/TabListModel.h"

#include <utility>

namespace sheaf {

TabListModel::TabListModel(QObject* parent) : QAbstractListModel(parent) {
}

int TabListModel::rowCount(const QModelIndex& parent) const {
  return parent.isValid() ? 0 : static_cast<int>(tabs_.size());
}

QVariant TabListModel::data(const QModelIndex& index, int role) const {
  if (!index.isValid() || index.row() >= tabs_.size()) {
    return {};
  }
  const Tab& tab = tabs_.at(index.row());
  switch (role) {
    case Qt::DisplayRole:
    case TitleRole:
      return tab.title;
    case PathRole:
      return tab.path;
    default:
      return {};
  }
}

QHash<int, QByteArray> TabListModel::roleNames() const {
  return {{TitleRole, "title"}, {PathRole, "path"}};
}

int TabListModel::Count() const {
  return static_cast<int>(tabs_.size());
}

const Tab& TabListModel::At(int index) const {
  return tabs_.at(index);
}

int TabListModel::IndexOfKey(const QString& file_key) const {
  for (int i = 0; i < tabs_.size(); ++i) {
    if (tabs_.at(i).file_key == file_key) {
      return i;
    }
  }
  return -1;
}

void TabListModel::Append(Tab tab) {
  const int row = static_cast<int>(tabs_.size());
  beginInsertRows({}, row, row);
  tabs_.append(std::move(tab));
  endInsertRows();
}

void TabListModel::RemoveAt(int index) {
  beginRemoveRows({}, index, index);
  tabs_.removeAt(index);
  endRemoveRows();
}

}  // namespace sheaf
