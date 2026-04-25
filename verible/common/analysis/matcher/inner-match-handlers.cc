// Copyright 2017-2020 The Verible Authors.
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//      http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#include "verible/common/analysis/matcher/inner-match-handlers.h"

#include <vector>

#include "verible/common/analysis/matcher/bound-symbol-manager.h"
#include "verible/common/analysis/matcher/matcher.h"
#include "verible/common/text/symbol.h"
#include "verible/common/util/logging.h"

namespace verible {
namespace matcher {

bool InnerMatchAll(const Symbol &symbol,
                   const std::vector<Matcher> &inner_matchers,
                   BoundSymbolManager *manager) {
  BoundSymbolManager backtrack_checkpoint(*manager);

  for (const auto &matcher : inner_matchers) {
    if (!matcher.Matches(symbol, manager)) {
      *manager = backtrack_checkpoint;
      return false;
    }
  }
  return true;
}

bool InnerMatchAny(const Symbol &symbol,
                   const std::vector<Matcher> &inner_matchers,
                   BoundSymbolManager *manager) {
  for (const auto &matcher : inner_matchers) {
    BoundSymbolManager lookahead(*manager);
    if (matcher.Matches(symbol, &lookahead)) {
      *manager = lookahead;
      return true;
    }
  }
  return false;
}

bool InnerMatchEachOf(const Symbol &symbol,
                      const std::vector<Matcher> &inner_matchers,
                      BoundSymbolManager *manager) {
  bool some_inner_matched_passed = false;

  for (const auto &matcher : inner_matchers) {
    BoundSymbolManager backup(*manager);
    if (matcher.Matches(symbol, manager)) {
      // If matcher passes, remember that we found a passing inner matcher
      some_inner_matched_passed = true;
    } else {
      // If failed to match, revert manager to backup.
      *manager = backup;
    }
  }

  return some_inner_matched_passed;
}

bool InnerMatchUnless(const Symbol &symbol,
                      const std::vector<Matcher> &inner_matchers,
                      BoundSymbolManager *manager) {
  CHECK_EQ(inner_matchers.size(), 1);

  const auto &matcher = inner_matchers[0];

  // We don't need to keep track of what inner matcher matches because any
  // binds will be discarded if it matches.
  BoundSymbolManager dummy_manager;

  return !matcher.Matches(symbol, &dummy_manager);
}

}  // namespace matcher
}  // namespace verible
