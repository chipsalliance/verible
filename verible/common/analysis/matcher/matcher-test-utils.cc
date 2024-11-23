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

#include "verible/common/analysis/matcher/matcher-test-utils.h"

#include <map>
#include <utility>

#include "absl/strings/string_view.h"
#include "gtest/gtest.h"
#include "verible/common/analysis/matcher/bound-symbol-manager.h"
#include "verible/common/analysis/matcher/matcher.h"
#include "verible/common/text/concrete-syntax-leaf.h"
#include "verible/common/text/concrete-syntax-tree.h"
#include "verible/common/text/symbol.h"
#include "verible/common/text/tree-utils.h"
#include "verible/common/text/visitors.h"

namespace verible {
namespace matcher {

void RunMatcherTestCase(const MatcherTestCase &test) {
  BoundSymbolManager bound_symbol_manager;

  ASSERT_NE(test.root, nullptr);
  bool result = test.matcher.Matches(*test.root, &bound_symbol_manager);

  EXPECT_EQ(result, test.expected_result);

  if (test.expected_result) {
    // If test passed, then all bound symbols should precisely matched expected.
    EXPECT_EQ(test.expected_bound_nodes.size(), bound_symbol_manager.Size());

    for (const auto &expected : test.expected_bound_nodes) {
      EXPECT_TRUE(bound_symbol_manager.ContainsSymbol(expected.first));

      auto matched_symbol = bound_symbol_manager.FindSymbol(expected.first);
      ASSERT_NE(matched_symbol, nullptr);
      EXPECT_EQ(expected.second, matched_symbol->Tag());
    }
  } else {
    // If test failed, then it should not report any bound symbols
    EXPECT_EQ(bound_symbol_manager.Size(), 0);
  }
}

class MatchCounter : public TreeVisitorRecursive {
 public:
  explicit MatchCounter(const Matcher &matcher) : matcher_(matcher) {}

  int Count(const Symbol &symbol) {
    num_matches_ = 0;
    symbol.Accept(this);
    return num_matches_;
  }

  void Visit(const SyntaxTreeLeaf &leaf) final { TestSymbol(leaf); }
  void Visit(const SyntaxTreeNode &node) final { TestSymbol(node); }

 private:
  const Matcher matcher_;
  int num_matches_ = 0;

  void TestSymbol(const Symbol &symbol) {
    BoundSymbolManager manager;
    bool found_match = matcher_.Matches(symbol, &manager);

    if (found_match) {
      num_matches_++;
    }
  }
};

void ExpectMatchesInAST(const Symbol &tree, const Matcher &matcher,
                        int num_matches, absl::string_view code) {
  MatchCounter counter(matcher);
  EXPECT_EQ(num_matches, counter.Count(tree)) << "code:\n"
                                              << code << "\ntree:\n"
                                              << RawTreePrinter(tree, true);
}

}  // namespace matcher
}  // namespace verible
