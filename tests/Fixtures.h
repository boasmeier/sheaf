#pragma once

#include <QFont>
#include <QPageSize>
#include <QPainter>
#include <QPdfWriter>
#include <QString>

namespace sheaf::testing {

// Writes a PDF whose every page shows `label` and its page number in large type, so pages are visually distinct.
// With `mixed_sizes`, pages alternate between A5 and A4 (real documents often mix page sizes).
inline bool WritePdfFixture(const QString& path, const QString& label, int pages, bool mixed_sizes) {
  QPdfWriter writer(path);
  writer.setResolution(72);
  writer.setPageSize(QPageSize(QPageSize::A5));
  QPainter painter(&writer);
  if (!painter.isActive()) {
    return false;
  }
  QFont font("sans-serif");
  font.setPixelSize(48);
  painter.setFont(font);
  for (int page = 0; page < pages; ++page) {
    if (page > 0) {
      if (mixed_sizes) {
        writer.setPageSize(QPageSize(page % 2 == 0 ? QPageSize::A5 : QPageSize::A4));
      }
      writer.newPage();
    }
    const QRect area(0, 0, writer.width(), writer.height());
    painter.drawText(area, Qt::AlignCenter, QString("%1\npage %2").arg(label).arg(page + 1));
    painter.drawRect(area.adjusted(10, 10, -10, -10));
    // A page-dependent bar, so neighbouring pages differ even at a glance.
    painter.fillRect(20, 20, 10 + ((page % 50) * 4), 14, Qt::darkBlue);
  }
  return painter.end();
}

}  // namespace sheaf::testing
