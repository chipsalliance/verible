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

#ifndef VERIBLE_COMMON_ANALYSIS_MATCHER_MATCHER_BUILDERS_H_
#define VERIBLE_COMMON_ANALYSIS_MATCHER_MATCHER_BUILDERS_H_

#include <array>
#include <utility>

#include "verible/common/analysis/matcher/descent-path.h"
#include "verible/common/analysis/matcher/inner-match-handlers.h"
#include "verible/common/analysis/matcher/matcher.h"
#include "verible/common/text/symbol.h"

namespace verible {
namespace matcher {

// Basic information about tree matchers can be found in:
//   common/analysis/matcher/matcher.h
//
// This file provides utilities for programmatically constructing
// higher-level matchers.

// Collections of symbol predicates used by Matcher builders.
// Some of these are parameterized with template arguments, other are
// parameterized as functors
//
// See verible/doc/style_lint.md for example usage

template <SymbolKind Kind, typename EnumType, EnumType Tag>
bool EqualTagPredicate(const Symbol &symbol) {
  SymbolTag symbol_tag = {Kind, static_cast<int>(Tag)};
  return symbol.Tag() == symbol_tag;
}

// TODO(jeremycs): Configure match branches here
// PathMatchBuilder is a Matcher generator that is parameterized over a path.
// Instances are generally created with MakePathMatcher, which infers the
// template parameter.
//
// Note: PathMatchBuilder has trivial destructor, so it is fit for const
//       declarations at a global scope.
//
// At a high level, it walks down its path using GetAllDescendantsFromPath.
// If it finds any descedants that match the path and its inner matchers
// correctly match the descendants, then the matcher reports true.
//
// Generated matcher implements the Bind interface. The bound symbols are the
// descendants that are found along path.
// TODO(jeremycs): handle match branches...
//
// Usage:
// PathMatchBuilder DescendPath123 = MakePathMatcher(NodeTag(1), NodeTag(2),
//                                                   LeafTag(3));
// auto matcher = SomeOutMatcher(DescendPath123(...inner matchers...));
// matcher.Matches(some_tree);
//
template <int N>
class PathMatchBuilder {
  static_assert(N > 0, "Path must have at least one element");

 public:
  explicit constexpr PathMatchBuilder(const std::array<SymbolTag, N> &path)
      : path_(path) {}

  template <typename... Args>
  BindableMatcher operator()(Args... args) const {
    // Make a local reference so that lambda can perform a copy
    // This avoids lifetime issue if returned Matcher is passed outside scope
    // of this builder object.
    DescentPath local_path;
    for (auto symbol_tag : path_) local_path.push_back(symbol_tag);

    // As long as one of the inner_matchers matches against discovered
    // descendants, PathMatchBuilder also matches.
    auto predicate = [](const Symbol &symbol) { return true; };

    // The transformation that is performed on the symbol before it passed
    // off to the InnerMatchHandler.
    // Each descendant in returned vector is matched seperately.
    // TODO(jeremycs): describe match branching behavior here
    auto transformer = [local_path](const Symbol &symbol) {
      return GetAllDescendantsFromPath(symbol, local_path);
    };

    BindableMatcher matcher(predicate, InnerMatchAll, transformer);
    matcher.AddMatchers(std::forward<Args>(args)...);
    return matcher;
  }

 private:
  std::array<SymbolTag, N> path_;
};

// Helper functions to create PathMatchers.
// Length of path determine by number of params. Might need to add more or
// TODO: implement a arg... template
constexpr PathMatchBuilder<1> MakePathMatcher(const SymbolTag t1) {
  return PathMatchBuilder<1>(std::array<SymbolTag, 1>{t1});
}
constexpr PathMatchBuilder<2> MakePathMatcher(const SymbolTag t1,
                                              const SymbolTag t2) {
  return PathMatchBuilder<2>(std::array<SymbolTag, 2>{t1, t2});
}
constexpr PathMatchBuilder<3> MakePathMatcher(const SymbolTag t1,
                                              const SymbolTag t2,
                                              const SymbolTag t3) {
  return PathMatchBuilder<3>(std::array<SymbolTag, 3>{t1, t2, t3});
}
constexpr PathMatchBuilder<4> MakePathMatcher(const SymbolTag t1,
                                              const SymbolTag t2,
                                              const SymbolTag t3,
                                              const SymbolTag t4) {
  return PathMatchBuilder<4>(std::array<SymbolTag, 4>{t1, t2, t3, t4});
}

// TagMatchBuilder is a Matcher generator that is parameterized over
// Kind and Tag.
//
// Note: TagMatchBuilder has trivial destructor, so it is fit for const
//       declarations at a global scope.
//
// The generated matcher will match when the examined symbol has equal Kind and
// Equal tag and when that symbol also matches all inner matchers.
//
// Generated matcher implements the Bind interface. The bound symbols are
// the matched node.
//
// Usage:
// TagMatcherBuilder<kNode, 1> Node1;
// auto matcher = SomeOutMatcher(Node1(...inner matchers...));
// matcher.Matches(some_tree);
//
template <SymbolKind Kind, typename EnumType, EnumType Tag>
class TagMatchBuilder {
 public:
  constexpr TagMatchBuilder() = default;

  template <typename... Args>
  BindableMatcher operator()(Args... args) const {
    BindableMatcher matcher(EqualTagPredicate<Kind, EnumType, Tag>,
                            InnerMatchAll);
    matcher.AddMatchers(std::forward<Args>(args)...);
    return matcher;
  }
};

// DynamicTagMatchBuilder is a Matcher generator that takes a Kind and Tag
// at run time.
//
// Note: DynamicTagMatchBuilder has trivial destructor, so it is fit for
//       const declarations at a global scope.
//
// The generated matcher will match when the examined symbol has equal Kind and
// Equal tag and when that symbol also matches all inner matchers.
//
// Generated matcher implements the Bind interface. The bound symbols are
// the matched node.
//
// Usage:
// TagMatcherBuilder Node1(SymbolTag{SymbolKind::kNode, kNodeEnum});
// auto matcher = SomeOutMatcher(Node1(...inner matchers...));
// matcher.Matches(some_tree);
//
// TODO(fangism): Make this a degenerate case of PathMatchBuilder<1>.
class DynamicTagMatchBuilder {
 public:
  // tag is a combination of {node,leaf} and enumeration.
  explicit DynamicTagMatchBuilder(SymbolTag tag) : tag_(tag) {}

  template <typename... Args>
  BindableMatcher operator()(Args... args) const {
    BindableMatcher matcher([this](const Symbol &s) { return s.Tag() == tag_; },
                            InnerMatchAll);
    matcher.AddMatchers(std::forward<Args>(args)...);
    return matcher;
  }

 private:
  SymbolTag tag_;
};

}  // namespace matcher
}  // namespace verible

#endif  // VERIBLE_COMMON_ANALYSIS_MATCHER_MATCHER_BUILDERS_H_
