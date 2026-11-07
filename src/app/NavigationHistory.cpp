#include "app/NavigationHistory.h"

namespace sheaf {

void NavigationHistory::RecordJump(const Position& from) {
  forward_.clear();
  // Following a link from the very same spot again adds nothing to return to.
  if (!back_.empty() && back_.back() == from) {
    return;
  }
  back_.push_back(from);
  if (back_.size() > kMaxDepth) {
    back_.pop_front();
  }
}

std::optional<Position> NavigationHistory::GoBack(const Position& current) {
  if (back_.empty()) {
    return std::nullopt;
  }
  forward_.push_back(current);
  const Position target = back_.back();
  back_.pop_back();
  return target;
}

std::optional<Position> NavigationHistory::GoForward(const Position& current) {
  if (forward_.empty()) {
    return std::nullopt;
  }
  back_.push_back(current);
  const Position target = forward_.back();
  forward_.pop_back();
  return target;
}

bool NavigationHistory::CanGoBack() const {
  return !back_.empty();
}

bool NavigationHistory::CanGoForward() const {
  return !forward_.empty();
}

}  // namespace sheaf
