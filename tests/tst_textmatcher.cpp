// Copyright (C) 2026 Boas Meier
// SPDX-License-Identifier: GPL-3.0-or-later

#include "search/TextMatcher.h"

#include <QtTest>

namespace {

// The matched text of each match, to compare against what the user would expect to be found.
QStringList Found(const QString& text, const QString& query) {
  QStringList found;
  for (const sheaf::TextMatch& match : sheaf::FindMatches(text, query)) {
    found << text.mid(match.start, match.length);
  }
  return found;
}

}  // namespace

class TstTextMatcher : public QObject {
  Q_OBJECT

private slots:
  void FindsEveryOccurrenceInOrder() {
    const QString text = "the cat and the hat and the bat";
    const QVector<sheaf::TextMatch> matches = sheaf::FindMatches(text, "the");
    QCOMPARE(matches.size(), 3);
    QCOMPARE(matches.at(0).start, 0);
    QCOMPARE(matches.at(1).start, 12);
    QCOMPARE(matches.at(2).start, 24);
    for (const sheaf::TextMatch& match : matches) {
      QCOMPARE(match.length, 3);
    }
  }

  void IgnoresCase() {
    QCOMPARE(Found("Gaussian GAUSSIAN gaussian", "GaUsSiAn"), (QStringList{"Gaussian", "GAUSSIAN", "gaussian"}));
    QCOMPARE(Found("Ünïcode über", "ÜBER"), (QStringList{"über"}));
  }

  void NoMatchAndEmptyQueries() {
    QVERIFY(sheaf::FindMatches("hello", "xyz").isEmpty());
    QVERIFY(sheaf::FindMatches("hello", "").isEmpty());
    QVERIFY(sheaf::FindMatches("hello", "   ").isEmpty());
    QVERIFY(sheaf::FindMatches("", "hello").isEmpty());
  }

  void MatchesAreNotOverlapping() { QCOMPARE(sheaf::FindMatches("aaaa", "aa").size(), 2); }

  void SpecialCharactersAreTakenLiterally() {
    QCOMPARE(Found("cost (a+b) * [c]? $5 .*", "(a+b)"), (QStringList{"(a+b)"}));
    QCOMPARE(Found("cost (a+b) * [c]? $5 .*", "[c]?"), (QStringList{"[c]?"}));
    QCOMPARE(Found("cost (a+b) * [c]? $5 .*", ".*"), (QStringList{".*"}));
    QCOMPARE(Found("a|b", "a|b"), (QStringList{"a|b"}));
    QVERIFY(sheaf::FindMatches("abc", "(").isEmpty());  // would be an invalid pattern if taken as one
  }

  void APhraseMatchesAcrossLineBreaksAndOddSpacing() {
    // PDF text puts a newline where a line ends.
    QCOMPARE(Found("the quick\r\nbrown  fox", "quick brown fox"), (QStringList{"quick\r\nbrown  fox"}));
    QCOMPARE(Found("a b", "a    b"), (QStringList{"a b"}));
    QCOMPARE(Found("non breaking space", "non breaking"), (QStringList{"non breaking"}));
    QCOMPARE(Found("  padded  query", "  padded query  "), (QStringList{"padded  query"}));
  }

  void LigaturesAndSoftHyphensDoNotGetInTheWay() {
    // "find" typed by the user, "ﬁnd" in the PDF
    const QString text = "to ﬁnd and eﬃcient and oﬀer";
    QCOMPARE(Found(text, "find"), (QStringList{"ﬁnd"}));
    QCOMPARE(Found(text, "efficient"), (QStringList{"eﬃcient"}));
    QCOMPARE(Found(text, "offer"), (QStringList{"oﬀer"}));
    // and the other way round: the user pastes the ligature
    QCOMPARE(Found("to find it", "ﬁnd"), (QStringList{"find"}));
    // a soft hyphen inside a word
    QCOMPARE(Found("exam­ple", "example"), (QStringList{"exam­ple"}));
    QCOMPARE(Found("eﬃcient", "ci").size(), 1);
  }

  void OffsetsRefToTheOriginalTextEvenAfterLigatures() {
    const QString text = "x ﬁnd y find";
    const QVector<sheaf::TextMatch> matches = sheaf::FindMatches(text, "find");
    QCOMPARE(matches.size(), 2);
    QCOMPARE(matches.at(0).start, 2);
    QCOMPARE(matches.at(0).length, 3);  // the ligature is one character of the original
    QCOMPARE(matches.at(1).start, 8);
    QCOMPARE(matches.at(1).length, 4);
  }

  void CharacterIndicesCountCodePointsNotUtf16Units() {
    // U+1D465 (mathematical italic x) is two UTF-16 units but one character.
    const QString x = QString::fromUcs4(U"\U0001D465");
    QCOMPARE(x.size(), 2);
    const QString text = "a" + x + x + "b needle";
    const QVector<sheaf::TextMatch> matches = sheaf::FindMatches(text, "needle");
    QCOMPARE(matches.size(), 1);
    QCOMPARE(matches.first().start, 7);  // UTF-16 offset: a(1) + 2 + 2 + b(1) + space(1) = 7 ... "needle" begins at 7
    QCOMPARE(matches.first().char_start, 5);  // characters: a, x, x, b, space = 5
    QCOMPARE(matches.first().char_length, 6);
    // A match that contains such a character.
    const QVector<sheaf::TextMatch> inside = sheaf::FindMatches("see " + x + "y here", QString(x) + "y");
    QCOMPARE(inside.size(), 1);
    QCOMPARE(inside.first().length, 3);
    QCOMPARE(inside.first().char_length, 2);
    QCOMPARE(inside.first().char_start, 4);
  }

  void ManyMatchesOnALongPage() {
    QString text;
    for (int i = 0; i < 5000; ++i) {
      text += "word ";
    }
    QCOMPARE(sheaf::FindMatches(text, "word").size(), 5000);
  }
};

QTEST_GUILESS_MAIN(TstTextMatcher)
#include "tst_textmatcher.moc"
