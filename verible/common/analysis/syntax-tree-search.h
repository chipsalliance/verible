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

#ifndef VERIBLE_COMMON_ANALYSIS_SYNTAX_TREE_SEARCH_H_
#define VERIBLE_COMMON_ANALYSIS_SYNTAX_TREE_SEARCH_H_

#include <functional>
#include <vector>

#include "verible/common/analysis/matcher/matcher.h"
#include "verible/common/text/symbol.h"
#include "verible/common/text/syntax-tree-context.h"

namespace verible {

// General match structure for searching a syntax tree.
//
// For testing purposes, matches can be hand-constructed like this:
//   // Let 'buffer' be a string_view of analyzed text.
//   auto leaf = Leaf(kTag, buffer.substr(...));  // tree_builder_test_util.h
//   TreeSearchMatch match{leaf.get(), {/* empty context */}};
struct TreeSearchMatch {
  // Note: The syntax tree to which the matching node belongs must outlive
  // this pointer.
  const Symbol *match;
  // A copy of the stack of syntax tree nodes that are ancestors of
  // the match node/leaf.  Note: This is needed because syntax tree nodes
  // don't have upward links to parents.
  SyntaxTreeContext context;
};

// SearchSyntaxTree collects nodes that match the specified criteria into a
// vector.  This is useful for analyses that need to look at a collection
// of related nodes together, rather than as each one is encountered.
std::vector<TreeSearchMatch> SearchSyntaxTree(
    const Symbol &root, const verible::matcher::Matcher &matcher,
    const std::function<bool(const SyntaxTreeContext &)> &context_predicate);

// This overload treats the missing context_predicate as always returning true.
std::vector<TreeSearchMatch> SearchSyntaxTree(
    const Symbol &root, const verible::matcher::Matcher &matcher);

}  // namespace verible

#endif  // VERIBLE_COMMON_ANALYSIS_SYNTAX_TREE_SEARCH_H_
