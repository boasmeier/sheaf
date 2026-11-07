// Copyright (C) 2026 Boas Meier
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include "search/TextMatcher.h"

#include <QMap>
#include <QVector>

#include <optional>

namespace sheaf {

// A place in the document: the page and the offset of a match in that page's text. Ordered like reading order.
struct MatchPosition {
  int page = 0;
  int start = 0;

  auto operator<=>(const MatchPosition&) const = default;
};

// The matches found so far and which one is the current match. Pure data, no PDF and no threads: pages arrive in any
// order (the scan starts at the page being read and wraps around), the document order is kept here.
class SearchResults {
public:
  // Forgets everything and prepares for a document of `page_count` pages.
  void Reset(int page_count);

  // Records the matches of one page (possibly none) and whether the page has any text at all.
  void AddPage(int page, const QVector<TextMatch>& matches, bool page_has_text);

  int PageCount() const;
  int PagesScanned() const;
  bool Complete() const;  // every page has been scanned
  // Total matches found so far; final once Complete().
  int Count() const;
  // False only if the scan is complete and no page had any text (a scan without a text layer).
  bool HasText() const;
  const QVector<TextMatch>& MatchesOnPage(int page) const;

  // --- The current match ---

  std::optional<MatchPosition> Current() const;
  // 1-based number of the current match in document order, 0 if there is none. Exact once Complete(); before that it
  // counts only the matches found so far.
  int CurrentNumber() const;
  // Makes the first match at or after `anchor` the current one (wrapping to the very first match if there is none
  // after it and the scan is complete). Does nothing and returns false if there is no such match (yet).
  bool SelectAtOrAfter(MatchPosition anchor);
  // True if a current match has been chosen; used to pick one as soon as the first results arrive.
  bool HasCurrent() const;
  // Moves along the matches in document order. Past the last match the first one follows (and vice versa) once the
  // scan is complete; before that the ends stay where they are. False if the current match did not change.
  bool Next();
  bool Previous();
  void ClearCurrent();

private:
  // The match at the given position among the matches found so far (document order), or nullopt.
  std::optional<MatchPosition> First() const;
  std::optional<MatchPosition> Last() const;
  std::optional<MatchPosition> After(MatchPosition position) const;   // strictly after
  std::optional<MatchPosition> Before(MatchPosition position) const;  // strictly before

  int page_count_ = 0;
  QMap<int, QVector<TextMatch>> matches_;  // only pages with matches
  QVector<bool> scanned_;
  int pages_scanned_ = 0;
  int pages_with_text_ = 0;
  int count_ = 0;
  std::optional<MatchPosition> current_;
  QVector<TextMatch> none_;
};

}  // namespace sheaf
