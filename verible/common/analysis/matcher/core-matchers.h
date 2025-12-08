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

#ifndef VERIBLE_COMMON_ANALYSIS_MATCHER_CORE_MATCHERS_H_
#define VERIBLE_COMMON_ANALYSIS_MATCHER_CORE_MATCHERS_H_

#include <utility>

#include "verible/common/analysis/matcher/inner-match-handlers.h"
#include "verible/common/analysis/matcher/matcher.h"
#include "verible/common/text/symbol.h"

namespace verible {
namespace matcher {

// AllOf is a variadic matcher that holds any number of inner matchers.
// It matches if every one of its inner matchers matches.
//
// Inner matchers are matched against the symbol that AllOf is matched against.
// In other words, no transformation occurs.
//
// If AllOf matches, then all of its inner matchers bound symbols are preserved.
// If it does not match, then no symobls are bound.
//
// The order of inner matchers is inconsequential; they are fully commutative.
//
// AllOf does not implement the Bind interface.
//
// Usage:
//  auto matcher = Node5(AllOf(HasNode5Child(), HasLeaf5Child()));
//
// This matches:
//  TNode(5, Leaf(5), TNode(5));
//
// And fails to match:
//  TNode(5, Leaf(5));
//  TNode(5, Node(5));
//  TNode(5, Leaf(2));
template <typename... Args>
Matcher AllOf(Args &&...args) {
  static_assert(sizeof...(args) > 0,
                "AllOf requires at least one inner matcher");

  // AllOf matcher's behavior is completely determined by its inner_matchers
  auto predicate = [](const Symbol &symbol) { return true; };

  Matcher matcher(predicate, InnerMatchAll);

  matcher.AddMatchers(std::forward<Args>(args)...);

  return matcher;
}

// AnyOf is a variadic matcher that holds any number of inner matchers.
// It matches if one of its inner matchers matches. It only binds symbols for
// the first matching inner matcher.
//
// Inner matchers are matched against the symbol that AnyOf is matched against.
// In other words, no transformation occurs.
//
// Only the first inner matcher that matchers gets to bind symbols. The
// remaining inner matchers are not tested and do not bind symbols.
// If no inner matchers match, then no symbols are bound.
//
// The order of inner matchers is inconsequential; they are fully commutative.
//
// AnyOf does not implement the Bind interface.
//
// Usage:
//  auto matcher = Node5(AnyOf(HasNode5Child(), HasLeaf5Child()));
//
// This matches:
//  TNode(5, Leaf(5), TNode(5));
//  TNode(5, Leaf(5));
//  TNode(5, Node(5));
//
// And fails to match:
//  TNode(5, Leaf(2));
template <typename... Args>
Matcher AnyOf(Args &&...args) {
  static_assert(sizeof...(args) > 0,
                "AnyOf requires at least one inner matcher");

  // AnyOf matcher's behavior is completely determined by its inner_matchers.
  auto predicate = [](const Symbol &symbol) { return true; };

  Matcher matcher(predicate, InnerMatchAny);

  matcher.AddMatchers(std::forward<Args>(args)...);

  return matcher;
}

// EachOf is a variadic matcher that holds any number of inner matchers.
// It matches if one of its inner matchers matches. Unlike AnyOf, it binds
// symbols for each matching inner matching.
//
// Inner matchers are matched against the symbol that EachOf is matched against.
// In other words, no transformation occurs.
//
// Every matching inner matcher gets to bind symbols.
// If no inner matchers matcher, then no symbols are bound.
//
// The order of inner matchers is inconsequential; they are fully commutative.
//
// EachOf does not implement the Bind interface.
//
// Usage:
//  auto matcher = Node5(EachOf(HasNode5Child(), HasLeaf5Child()));
//
// This matches:
//  TNode(5, Leaf(5), TNode(5));
//  TNode(5, Leaf(5));
//  TNode(5, Node(5));
//
// And fails to match
//  TNode(5, Leaf(2));
template <typename... Args>
Matcher EachOf(Args &&...args) {
  static_assert(sizeof...(args) > 0,
                "EachOf requires at least one inner matcher");

  // EachOf matcher's behavior is completely determined by its inner_matchers.
  auto predicate = [](const Symbol &symbol) { return true; };

  Matcher matcher(predicate, InnerMatchEachOf);

  matcher.AddMatchers(std::forward<Args>(args)...);

  return matcher;
}

// Unless is a matcher that holds a single inner matcher. It represents
// logical negation.
//
// If its inner matcher matches, then Unless does not match.
// Otherwise, if its inner matchers does not match, then Unless does matcher.
//
// Unless's inner matcher does not bind symbols in either case.
//
// Unless does not implement the Bind interface.
//
// Usage:
//  auto matcher = Node5(Unless(HasNode5Child()));
// This matches:
//  TNode(5, Leaf(5));
// And fails to match
//  TNode(5, TNode(5));
template <typename... Args>
Matcher Unless(const Matcher &inner_matcher) {
  // Unless matcher's behavior is completely determined by its inner_matcher.
  auto predicate = [](const Symbol &symbol) { return true; };

  Matcher matcher(predicate, InnerMatchUnless);

  matcher.AddMatchers(inner_matcher);

  return matcher;
}

}  // namespace matcher
}  // namespace verible

#endif  // VERIBLE_COMMON_ANALYSIS_MATCHER_CORE_MATCHERS_H_
