// Copyright (C) 2026 Boas Meier
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <QByteArray>
#include <QColor>
#include <QFile>
#include <QFont>
#include <QImage>
#include <QPageSize>
#include <QPainter>
#include <QPdfWriter>
#include <QString>

#include <array>

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
    // A page-dependent background shade: neighbouring pages differ by at least 60 grey levels, so a wrong page is
    // obvious to the tests even where the text is small.
    const int shade = 40 + ((page * 61) % 170);
    painter.fillRect(area, QColor(shade, shade, shade));
    painter.drawText(area, Qt::AlignCenter, QString("%1\npage %2").arg(label).arg(page + 1));
    painter.drawRect(area.adjusted(10, 10, -10, -10));
    // A page-dependent bar, so neighbouring pages differ even at a glance.
    painter.fillRect(20, 20, 10 + ((page % 50) * 4), 14, Qt::darkBlue);
  }
  return painter.end();
}

// Link geometry of WritePdfWithLinks(), in PDF points (origin bottom-left, A4 = 595 x 842).
inline constexpr int kLinkedPageHeight = 842;
inline constexpr std::array<int, 4> kInternalLinkRect = {50, 700, 300, 780};  // x0 y0 x1 y1
inline constexpr std::array<int, 4> kExternalLinkRect = {50, 580, 300, 660};
inline constexpr int kLinkJump = 11;  // page i links to page (i + kLinkJump) % pages
inline const char* const kExternalLinkUrl = "https://example.com/page";

// Writes an A4 PDF by hand (QPdfWriter cannot create links). Every page carries an internal link to page
// (i + kLinkJump) % pages, landing at the top of that page, and an external link to kExternalLinkUrl.
inline bool WritePdfWithLinks(const QString& path, int pages) {
  // Object numbers: 1 catalog, 2 page tree, 3 font, then per page i: page 4+3i, content 5+3i, annotations 6+3i / 7+3i.
  const int per_page = 4;
  const auto page_object = [&](int i) { return 4 + (i * per_page); };
  const int object_count = 3 + (pages * per_page);

  QByteArray pdf = "%PDF-1.4\n";
  QList<qsizetype> offsets(object_count + 1, 0);
  const auto add = [&](int number, const QByteArray& body) {
    offsets[number] = pdf.size();
    pdf += QByteArray::number(number) + " 0 obj\n" + body + "\nendobj\n";
  };

  add(1, "<< /Type /Catalog /Pages 2 0 R >>");
  QByteArray kids;
  for (int i = 0; i < pages; ++i) {
    kids += QByteArray::number(page_object(i)) + " 0 R ";
  }
  add(2, "<< /Type /Pages /Kids [" + kids + "] /Count " + QByteArray::number(pages) + " >>");
  add(3, "<< /Type /Font /Subtype /Type1 /BaseFont /Helvetica >>");

  for (int i = 0; i < pages; ++i) {
    const int page = page_object(i);
    const int target = page_object((i + kLinkJump) % pages);
    const QByteArray rect_in =
        QByteArray::number(kInternalLinkRect[0]) + " " + QByteArray::number(kInternalLinkRect[1]) + " " +
        QByteArray::number(kInternalLinkRect[2]) + " " + QByteArray::number(kInternalLinkRect[3]);
    const QByteArray rect_out =
        QByteArray::number(kExternalLinkRect[0]) + " " + QByteArray::number(kExternalLinkRect[1]) + " " +
        QByteArray::number(kExternalLinkRect[2]) + " " + QByteArray::number(kExternalLinkRect[3]);
    add(page, "<< /Type /Page /Parent 2 0 R /MediaBox [0 0 595 842] /Contents " + QByteArray::number(page + 1) +
                  " 0 R /Resources << /Font << /F1 3 0 R >> >> /Annots [" + QByteArray::number(page + 2) + " 0 R " +
                  QByteArray::number(page + 3) + " 0 R] >>");
    const QByteArray content = "0.8 0.9 1 rg " + rect_in + " re f 0.9 0.8 0.8 rg " + rect_out +
                               " re f 0 g BT /F1 48 Tf 60 400 Td (LINKED " + QByteArray::number(i + 1) + ") Tj ET";
    add(page + 1, "<< /Length " + QByteArray::number(content.size()) + " >>\nstream\n" + content + "\nendstream");
    add(page + 2, "<< /Type /Annot /Subtype /Link /Rect [" + rect_in + "] /Border [0 0 0] /Dest [" +
                      QByteArray::number(target) + " 0 R /XYZ 0 842 0] >>");
    add(page + 3, "<< /Type /Annot /Subtype /Link /Rect [" + rect_out + "] /Border [0 0 0] /A << /S /URI /URI (" +
                      kExternalLinkUrl + ") >> >>");
  }

  const qsizetype xref = pdf.size();
  pdf += "xref\n0 " + QByteArray::number(object_count + 1) + "\n0000000000 65535 f \n";
  for (int number = 1; number <= object_count; ++number) {
    pdf += QByteArray::number(offsets[number]).rightJustified(10, '0') + " 00000 n \n";
  }
  pdf += "trailer\n<< /Size " + QByteArray::number(object_count + 1) + " /Root 1 0 R >>\nstartxref\n" +
         QByteArray::number(xref) + "\n%%EOF\n";

  QFile file(path);
  return file.open(QIODevice::WriteOnly) && file.write(pdf) == pdf.size();
}

// ---- Text search fixtures ----

// What WritePdfWithText() puts on its pages, so tests know what a search must find.
inline constexpr int kSearchPages = 320;
inline constexpr int kCommonWordsPerPage = 3;  // "common" appears this often on every page
// "needle" is on every page whose 1-based number ends in 3 ...
inline bool PageHasNeedle(int page_index) {
  return (page_index + 1) % 10 == 3;
}
// ... and as "NEEDLE" (capitals) on page 5.
inline constexpr int kShoutedNeedlePage = 4;  // 0-based
// "red fox" is split across a line break on page 7 (0-based 6).
inline constexpr int kRedFoxPage = 6;

inline int ExpectedNeedleCount() {
  int count = 1;  // the shouted one
  for (int page = 0; page < kSearchPages; ++page) {
    if (PageHasNeedle(page)) {
      ++count;
    }
  }
  return count;
}

// A text-heavy A4 document of `pages` pages: filler text, the word "common" three times per page, "needle" as described
// above and a phrase spanning two lines. Real text, so it can be searched and selected.
inline bool WritePdfWithText(const QString& path, int pages) {
  QPdfWriter writer(path);
  writer.setResolution(72);
  writer.setPageSize(QPageSize(QPageSize::A4));
  QPainter painter(&writer);
  if (!painter.isActive()) {
    return false;
  }
  QFont font("sans-serif");
  font.setPixelSize(11);
  painter.setFont(font);
  for (int page = 0; page < pages; ++page) {
    if (page > 0) {
      writer.newPage();
    }
    QString text = QString("Chapter %1\n\n").arg(page + 1);
    for (int line = 0; line < 9; ++line) {
      text += "lorem ipsum dolor sit amet consectetur adipiscing elit sed do eiusmod tempor incididunt ut labore\n";
    }
    text += "The common value, a common error and the common case.\n";
    if (PageHasNeedle(page)) {
      text += "Here is the needle you were looking for.\n";
    }
    if (page == kShoutedNeedlePage) {
      text += "A NEEDLE in capitals.\n";
    }
    if (page == kRedFoxPage) {
      text += "The quick red\nfox jumps over the lazy dog.\n";
    }
    const QRect area(40, 40, writer.width() - 80, writer.height() - 80);
    painter.drawText(area, Qt::AlignTop | Qt::AlignLeft, text);
  }
  return painter.end();
}

// A document like a scan: every page is only a picture, so there is no text layer to search.
inline bool WriteScannedPdf(const QString& path, int pages) {
  QPdfWriter writer(path);
  writer.setResolution(72);
  writer.setPageSize(QPageSize(QPageSize::A5));
  QPainter painter(&writer);
  if (!painter.isActive()) {
    return false;
  }
  QImage picture(300, 400, QImage::Format_RGB32);
  for (int page = 0; page < pages; ++page) {
    if (page > 0) {
      writer.newPage();
    }
    picture.fill(QColor(200 - (page * 10), 220, 240));
    painter.drawImage(QRect(0, 0, writer.width(), writer.height()), picture);
  }
  return painter.end();
}

}  // namespace sheaf::testing
