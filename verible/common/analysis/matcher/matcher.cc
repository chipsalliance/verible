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

// Implementation of matcher.h
#include "verible/common/analysis/matcher/matcher.h"

#include <vector>

#include "verible/common/analysis/matcher/bound-symbol-manager.h"
#include "verible/common/text/symbol.h"

namespace verible {
namespace matcher {

bool Matcher::Matches(const Symbol &symbol, BoundSymbolManager *manager) const {
  if (predicate_(symbol)) {
    // If this matcher matches (as in, predicate succeeds), test inner matchers
    // to see if they also match.

    // Get set of symbols to try inner matchers on.
    auto next_targets = transformer_(symbol);

    // If we failed to fnd any next targets, we can't proceed.
    if (next_targets.empty()) return false;

    // If any target matches, this is set to true.
    bool any_target_matches = false;

    // TODO(jeremycs): add branching match groups here

    // Try to match inner matches to every target symbol.
    for (const auto &target_symbol : next_targets) {
      if (!target_symbol) continue;
      bool inner_match_result =
          inner_match_handler_(*target_symbol, inner_matchers_, manager);
      if (inner_match_result && manager && bind_id_) {
        manager->BindSymbol(bind_id_.value(), target_symbol);
      }
      any_target_matches |= inner_match_result;
    }

    return any_target_matches;
  }
  return false;
}

}  // namespace matcher
}  // namespace verible
