#pragma once

#include <QString>
#include <QVector>

namespace sheaf {

// One occurrence of the query on a page.
struct TextMatch {
  int start = 0;       // offset into the page text, in UTF-16 units
  int length = 0;      // length in the page text
  int char_start = 0;  // the same place counted in characters, the way PDFium numbers them (see below)
  int char_length = 0;

  bool operator==(const TextMatch&) const = default;
};

// Finds the occurrences of `query` in the text of a page.
//  - case-insensitive;
//  - any run of white space in the query matches any run of white space in the text (PDF text breaks lines with
//    newlines, and a phrase may span lines);
//  - ligatures (ﬁ, ﬂ, ...) and soft hyphens in the text do not get in the way: "find" matches "ﬁnd".
// Matches do not overlap. `start`/`length` refer to the original text. `char_start`/`char_length` count characters
// instead of UTF-16 units (a character outside the BMP is one character but two units), which is what
// QPdfDocument::getSelectionAtIndex() expects.
QVector<TextMatch> FindMatches(const QString& text, const QString& query);

}  // namespace sheaf
