#include "search/SearchResults.h"

namespace sheaf {

void SearchResults::Reset(int page_count) {
  page_count_ = page_count;
  matches_.clear();
  scanned_ = QVector<bool>(page_count, false);
  pages_scanned_ = 0;
  pages_with_text_ = 0;
  count_ = 0;
  current_.reset();
}

void SearchResults::AddPage(int page, const QVector<TextMatch>& matches, bool page_has_text) {
  if (page < 0 || page >= page_count_ || scanned_.at(page)) {
    return;
  }
  scanned_[page] = true;
  ++pages_scanned_;
  if (page_has_text) {
    ++pages_with_text_;
  }
  if (!matches.isEmpty()) {
    matches_.insert(page, matches);
    count_ += static_cast<int>(matches.size());
  }
}

int SearchResults::PageCount() const {
  return page_count_;
}

int SearchResults::PagesScanned() const {
  return pages_scanned_;
}

bool SearchResults::Complete() const {
  return pages_scanned_ == page_count_;
}

int SearchResults::Count() const {
  return count_;
}

bool SearchResults::HasText() const {
  return !Complete() || pages_with_text_ > 0;
}

const QVector<TextMatch>& SearchResults::MatchesOnPage(int page) const {
  const auto found = matches_.constFind(page);
  return found == matches_.constEnd() ? none_ : found.value();
}

std::optional<MatchPosition> SearchResults::Current() const {
  return current_;
}

bool SearchResults::HasCurrent() const {
  return current_.has_value();
}

int SearchResults::CurrentNumber() const {
  if (!current_) {
    return 0;
  }
  int number = 0;
  for (auto page = matches_.constBegin(); page != matches_.constEnd() && page.key() < current_->page; ++page) {
    number += static_cast<int>(page.value().size());
  }
  const QVector<TextMatch>& here = MatchesOnPage(current_->page);
  for (const TextMatch& match : here) {
    ++number;
    if (match.start == current_->start) {
      break;
    }
  }
  return number;
}

std::optional<MatchPosition> SearchResults::First() const {
  if (matches_.isEmpty()) {
    return std::nullopt;
  }
  return MatchPosition{.page = matches_.firstKey(), .start = matches_.first().first().start};
}

std::optional<MatchPosition> SearchResults::Last() const {
  if (matches_.isEmpty()) {
    return std::nullopt;
  }
  return MatchPosition{.page = matches_.lastKey(), .start = matches_.last().last().start};
}

std::optional<MatchPosition> SearchResults::After(MatchPosition position) const {
  for (auto page = matches_.lowerBound(position.page); page != matches_.constEnd(); ++page) {
    for (const TextMatch& match : page.value()) {
      const MatchPosition candidate{.page = page.key(), .start = match.start};
      if (candidate > position) {
        return candidate;
      }
    }
  }
  return std::nullopt;
}

std::optional<MatchPosition> SearchResults::Before(MatchPosition position) const {
  std::optional<MatchPosition> best;
  for (auto page = matches_.constBegin(); page != matches_.constEnd() && page.key() <= position.page; ++page) {
    for (const TextMatch& match : page.value()) {
      const MatchPosition candidate{.page = page.key(), .start = match.start};
      if (candidate < position) {
        best = candidate;
      }
    }
  }
  return best;
}

bool SearchResults::SelectAtOrAfter(MatchPosition anchor) {
  // "At or after" is "strictly after the position just before the anchor".
  std::optional<MatchPosition> found;
  for (auto page = matches_.lowerBound(anchor.page); page != matches_.constEnd() && !found; ++page) {
    for (const TextMatch& match : page.value()) {
      const MatchPosition candidate{.page = page.key(), .start = match.start};
      if (candidate >= anchor) {
        found = candidate;
        break;
      }
    }
  }
  if (!found && Complete()) {
    found = First();  // nothing after the anchor: wrap round to the beginning
  }
  if (!found) {
    return false;
  }
  current_ = found;
  return true;
}

bool SearchResults::Next() {
  if (!current_) {
    return false;
  }
  std::optional<MatchPosition> next = After(*current_);
  if (!next && Complete()) {
    next = First();
  }
  if (!next || next == current_) {
    return false;
  }
  current_ = next;
  return true;
}

bool SearchResults::Previous() {
  if (!current_) {
    return false;
  }
  std::optional<MatchPosition> previous = Before(*current_);
  if (!previous && Complete()) {
    previous = Last();
  }
  if (!previous || previous == current_) {
    return false;
  }
  current_ = previous;
  return true;
}

void SearchResults::ClearCurrent() {
  current_.reset();
}

}  // namespace sheaf
