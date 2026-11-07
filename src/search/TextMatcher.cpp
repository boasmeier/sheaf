#include "search/TextMatcher.h"

#include <QRegularExpression>
#include <QStringList>

namespace sheaf {

namespace {

// The text prepared for matching, with the offset in the original text of every character of it.
struct Prepared {
  QString text;
  QVector<int> origin;  // origin[i]: offset in the original of text[i]; origin[text.size()] is the original's size
};

// What a ligature stands for.
QString Expand(QChar c) {
  switch (c.unicode()) {
    case 0xFB00:
      return QStringLiteral("ff");
    case 0xFB01:
      return QStringLiteral("fi");
    case 0xFB02:
      return QStringLiteral("fl");
    case 0xFB03:
      return QStringLiteral("ffi");
    case 0xFB04:
      return QStringLiteral("ffl");
    case 0xFB05:
    case 0xFB06:
      return QStringLiteral("st");
    default:
      return {};
  }
}

Prepared Prepare(const QString& original) {
  Prepared prepared;
  prepared.text.reserve(original.size());
  prepared.origin.reserve(original.size() + 1);
  for (int i = 0; i < original.size(); ++i) {
    const QChar c = original.at(i);
    if (c.unicode() == 0x00AD) {  // soft hyphen: invisible
      continue;
    }
    const QString expanded = Expand(c);
    if (expanded.isEmpty()) {
      prepared.text.append(c);
      prepared.origin.append(i);
    }
    else {
      for (const QChar e : expanded) {
        prepared.text.append(e);
        prepared.origin.append(i);
      }
    }
  }
  prepared.origin.append(static_cast<int>(original.size()));
  return prepared;
}

int LowSurrogates(const QString& text, int from, int to) {
  int count = 0;
  for (int i = from; i < to; ++i) {
    if (text.at(i).isLowSurrogate()) {
      ++count;
    }
  }
  return count;
}

}  // namespace

QVector<TextMatch> FindMatches(const QString& text, const QString& query) {
  QVector<TextMatch> matches;
  const QStringList words = query.split(QRegularExpression(QStringLiteral("\\s+")), Qt::SkipEmptyParts);
  if (words.isEmpty() || text.isEmpty()) {
    return matches;
  }
  // The query's own ligatures are expanded the same way, so a pasted "ﬁnd" finds "find" as well.
  QStringList escaped;
  for (const QString& word : words) {
    escaped << QRegularExpression::escape(Prepare(word).text);
  }
  const QRegularExpression expression(
      escaped.join(QStringLiteral("\\s+")),
      QRegularExpression::CaseInsensitiveOption | QRegularExpression::UseUnicodePropertiesOption);
  if (!expression.isValid()) {
    return matches;
  }

  const Prepared prepared = Prepare(text);
  QRegularExpressionMatchIterator iterator = expression.globalMatch(prepared.text);
  while (iterator.hasNext()) {
    const QRegularExpressionMatch found = iterator.next();
    if (found.capturedLength() == 0) {
      continue;
    }
    const int start = prepared.origin.at(static_cast<int>(found.capturedStart()));
    const int end = prepared.origin.at(static_cast<int>(found.capturedEnd()));
    TextMatch match;
    match.start = start;
    match.length = end - start;
    match.char_start = start - LowSurrogates(text, 0, start);
    match.char_length = match.length - LowSurrogates(text, start, end);
    matches.append(match);
  }
  return matches;
}

}  // namespace sheaf
