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

#ifndef VERIBLE_COMMON_ANALYSIS_MATCHER_INNER_MATCH_HANDLERS_H_
#define VERIBLE_COMMON_ANALYSIS_MATCHER_INNER_MATCH_HANDLERS_H_

#include <vector>

#include "verible/common/analysis/matcher/bound-symbol-manager.h"
#include "verible/common/analysis/matcher/matcher.h"
#include "verible/common/text/symbol.h"

namespace verible {
namespace matcher {

// These are a collection of inner matcher handlers, which are used by
// the matcher class to describe how to process inner matchers.
//
// Broadly speaking, this behavior includes how many inner matchers need in
// order for the handler to return true and how inner matchers' bound symbols
// are bound into the manager.
//
// Usage:
//   InnerMatchHandler handler = ... select one of the functions ...
//   Matcher matcher(some_predicate, handler);

// Returns true if all inner_matchers match
//
// If all inner matchers match, each inner matcher binds its symbols to
// manager. The order of these binds is the order in which matchers appear in
// inner_matchers.
// If not all inner matchers match, then nothing is bound to manager.
//
bool InnerMatchAll(const Symbol &symbol,
                   const std::vector<Matcher> &inner_matchers,
                   BoundSymbolManager *manager);

// Returns true if one of inner_matchers matches
//
// Only the first matching inner matcher in inner_matchers gets to bind.
// Subsequent matchers are not run.
// If no inner matchers match, then nothing is bound to manager.
//
bool InnerMatchAny(const Symbol &symbol,
                   const std::vector<Matcher> &inner_matchers,
                   BoundSymbolManager *manager);

// Returns true if one of inner_matchers matches
//
// Every matching inner_matcher binds symbols to manager. The order of these
// binds is the order in which matchers appear in inner_matchers.
// If no inner matchers match, then nothing is bound to manager.
//
bool InnerMatchEachOf(const Symbol &symbol,
                      const std::vector<Matcher> &inner_matchers,
                      BoundSymbolManager *manager);

// Returns true if inner_matcher does not match.
// Returns false if inner_matcher does match.
//
// Inner matchers should contain exactly one inner matcher.
//
// No symbols are bound to manager regardless of outcome.
//
bool InnerMatchUnless(const Symbol &symbol,
                      const std::vector<Matcher> &inner_matchers,
                      BoundSymbolManager *manager);

}  // namespace matcher
}  // namespace verible

#endif  // VERIBLE_COMMON_ANALYSIS_MATCHER_INNER_MATCH_HANDLERS_H_
