// Copyright (C) 2026 Boas Meier
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include "storage/PositionStore.h"

#include <deque>
#include <optional>
#include <vector>

namespace sheaf {

// Back/forward stacks of one tab, like a browser's. In memory only.
// Usage: before following a link, RecordJump(position the user is leaving). GoBack(current) returns the position to
// return to and remembers `current` for GoForward; recording a new jump discards the forward stack.
class NavigationHistory {
public:
  static constexpr int kMaxDepth = 200;

  void RecordJump(const Position& from);
  std::optional<Position> GoBack(const Position& current);
  std::optional<Position> GoForward(const Position& current);

  bool CanGoBack() const;
  bool CanGoForward() const;

private:
  std::deque<Position> back_;
  std::vector<Position> forward_;
};

}  // namespace sheaf
