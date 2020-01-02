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

#include "common/analysis/syntax_tree_search.h"

#include <functional>
#include <memory>
#include <vector>

#include "common/analysis/matcher/bound_symbol_manager.h"
#include "common/analysis/matcher/matcher.h"
#include "common/text/concrete_syntax_leaf.h"
#include "common/text/concrete_syntax_tree.h"
#include "common/text/symbol.h"
#include "common/text/syntax_tree_context.h"
#include "common/text/tree_context_visitor.h"

namespace verible {
namespace {

using matcher::BoundSymbolManager;

// SyntaxTreeSearcher collects node that match specified criteria
// from a syntax tree.  Prefer to use the SearchSyntaxTree() function
// over this class.
class SyntaxTreeSearcher : public TreeContextVisitor {
 public:
  SyntaxTreeSearcher(
      const matcher::Matcher& m,
      std::function<bool(const SyntaxTreeContext&)> context_predicate)
      : matcher_(m), context_predicate_(context_predicate) {}

  void Search(const Symbol& root) { root.Accept(this); }

  const std::vector<TreeSearchMatch> Matches() const { return matches_; }

 private:
  void CheckSymbol(const Symbol&);
  void Visit(const SyntaxTreeLeaf& leaf) override;
  void Visit(const SyntaxTreeNode& node) override;

  // Main matcher that finds a particular type of tree node.
  const verible::matcher::Matcher matcher_;

  // Predicate that further qualifies the matches of interest.
  const std::function<bool(const SyntaxTreeContext&)> context_predicate_;

  // Accumulated set of matches.
  std::vector<TreeSearchMatch> matches_;
};

// Checks if leaf matches critera.
void SyntaxTreeSearcher::CheckSymbol(const Symbol& symbol) {
  BoundSymbolManager manager;
  if (matcher_.Matches(symbol, &manager)) {
    if (context_predicate_(Context())) {
      matches_.push_back(TreeSearchMatch{&symbol, Context()});
    }
  }
}
//
// Checks if leaf matches critera.
void SyntaxTreeSearcher::Visit(const SyntaxTreeLeaf& leaf) {
  CheckSymbol(leaf);
}

// Checks if node matches criteria.
// Then recursively search subtree.
void SyntaxTreeSearcher::Visit(const SyntaxTreeNode& node) {
  CheckSymbol(node);
  TreeContextVisitor::Visit(node);
}

}  // namespace

std::vector<TreeSearchMatch> SearchSyntaxTree(
    const Symbol& root, const verible::matcher::Matcher& matcher,
    std::function<bool(const SyntaxTreeContext&)> context_predicate) {
  SyntaxTreeSearcher searcher(matcher, context_predicate);
  searcher.Search(root);
  return searcher.Matches();
}

std::vector<TreeSearchMatch> SearchSyntaxTree(
    const Symbol& root, const verible::matcher::Matcher& matcher) {
  return SearchSyntaxTree(root, matcher,
                          [](const SyntaxTreeContext&) { return true; });
}

}  // namespace verible
