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

#include "verible/common/analysis/syntax-tree-linter.h"

#include <cstddef>
#include <memory>
#include <set>
#include <vector>

#include "absl/strings/string_view.h"
#include "gtest/gtest.h"
#include "verible/common/analysis/lint-rule-status.h"
#include "verible/common/analysis/syntax-tree-lint-rule.h"
#include "verible/common/text/concrete-syntax-leaf.h"
#include "verible/common/text/concrete-syntax-tree.h"
#include "verible/common/text/symbol.h"
#include "verible/common/text/syntax-tree-context.h"
#include "verible/common/text/token-info.h"
#include "verible/common/text/tree-builder-test-util.h"
#include "verible/common/util/casts.h"

namespace verible {
namespace {

// Simple rule for testing that verifies that all leafs contained in tree
// are tagged as a certain value. This value is passed in via the constructor
class AllLeavesMustBeN : public SyntaxTreeLintRule {
 public:
  // Constructor that takes n, the number that this rule is checking for
  explicit AllLeavesMustBeN(int n) : target_(n) {}

  // When handling leaf, check that it has target tag and report a violation
  // is not
  void HandleLeaf(const SyntaxTreeLeaf &leaf,
                  const SyntaxTreeContext &context) final {
    if (leaf.get().token_enum() != target_) {
      violations_.insert(LintViolation(leaf.get(), "", context));
    }
  }

  // Do not operate on nodes
  void HandleNode(const SyntaxTreeNode &node,
                  const SyntaxTreeContext &context) final {}
  LintRuleStatus Report() const final { return LintRuleStatus(violations_); }

 private:
  std::set<LintViolation> violations_;
  int target_;
};

std::unique_ptr<SyntaxTreeLintRule> MakeRuleN(int n) {
  return std::unique_ptr<SyntaxTreeLintRule>(new AllLeavesMustBeN(n));
}

TEST(SyntaxTreeLinterTest, BasicUsageCorrect) {
  const SymbolPtr root = Node(XLeaf(2), XLeaf(2), Node(XLeaf(2)), XLeaf(2));

  SyntaxTreeLinter linter;
  linter.AddRule(MakeRuleN(2));

  ASSERT_NE(root, nullptr);
  linter.Lint(*root);
  std::vector<LintRuleStatus> statuses = linter.ReportStatus();
  EXPECT_EQ(statuses.size(), 1);
  EXPECT_TRUE(statuses[0].isOk());
  EXPECT_EQ(statuses[0].violations.size(), 0);
}

TEST(SyntaxTreeLinterTest, BasicUsageFailure) {
  SymbolPtr root = Node(XLeaf(2), XLeaf(2), Node(XLeaf(2)), XLeaf(3));

  SyntaxTreeLinter linter;
  linter.AddRule(MakeRuleN(2));

  ASSERT_NE(root, nullptr);
  linter.Lint(*root);
  std::vector<LintRuleStatus> statuses = linter.ReportStatus();
  EXPECT_EQ(statuses.size(), 1);
  EXPECT_FALSE(statuses[0].isOk());
  EXPECT_EQ(statuses[0].violations.size(), 1);
}

TEST(SyntaxTreeLinterTest, MultipleRules) {
  constexpr absl::string_view text("abcd");
  SymbolPtr root =
      Node(Leaf(2, text.substr(0, 1)), Leaf(2, text.substr(1, 1)),
           Node(Leaf(2, text.substr(2, 1))), Leaf(2, text.substr(3, 1)));

  SyntaxTreeLinter linter;
  linter.AddRule(MakeRuleN(2));
  linter.AddRule(MakeRuleN(3));

  ASSERT_NE(root, nullptr);
  linter.Lint(*root);
  std::vector<LintRuleStatus> statuses = linter.ReportStatus();
  EXPECT_EQ(statuses.size(), 2);
  EXPECT_TRUE(statuses[0].isOk());
  EXPECT_FALSE(statuses[1].isOk());
  EXPECT_EQ(statuses[0].violations.size(), 0);
  EXPECT_EQ(statuses[1].violations.size(), 4);
}

// Simple testing rule that verifies that every node's leaf children have tags
// that are in ascending order
class ChildrenLeavesAscending : public SyntaxTreeLintRule {
 public:
  // Do not process leaves
  void HandleLeaf(const SyntaxTreeLeaf &leaf,
                  const SyntaxTreeContext &context) final {}

  // When handling nodes, iterate through all children and check that tags
  // are ascending. Report leafs nodes that have leaves that are out of order.
  void HandleNode(const SyntaxTreeNode &node,
                  const SyntaxTreeContext &context) final {
    int last_tag = 0;
    for (const auto &child : node.children()) {
      if (child->Kind() == SymbolKind::kLeaf) {
        const SyntaxTreeLeaf *leaf_child =
            down_cast<SyntaxTreeLeaf *>(child.get());
        const int current_tag = leaf_child->get().token_enum();
        if (current_tag >= last_tag) {
          last_tag = current_tag;
        } else {
          violations_.insert(LintViolation(leaf_child->get(), "", context));
          return;
        }
      }
    }
  }

  LintRuleStatus Report() const final { return LintRuleStatus(violations_); }

 private:
  std::set<LintViolation> violations_;
};

std::unique_ptr<SyntaxTreeLintRule> MakeAscending() {
  return std::unique_ptr<SyntaxTreeLintRule>(new ChildrenLeavesAscending());
}

TEST(SyntaxTreeLinterTest, AscendingSuccess) {
  constexpr absl::string_view text("abcde");
  SymbolPtr root =
      Node(Leaf(1, text.substr(0, 1)), Leaf(4, text.substr(1, 1)),
           Node(Leaf(2, text.substr(2, 1)), Leaf(10, text.substr(3, 1))),
           Leaf(7, text.substr(4, 1)));
  SyntaxTreeLinter linter;
  linter.AddRule(MakeAscending());
  ASSERT_NE(root.get(), nullptr);
  linter.Lint(*root.get());

  std::vector<LintRuleStatus> statuses = linter.ReportStatus();
  EXPECT_EQ(statuses.size(), 1);
  EXPECT_TRUE(statuses[0].isOk());
}

TEST(SyntaxTreeLinterTest, AscendingFailsOnce) {
  constexpr absl::string_view text("abcde");
  SymbolPtr root =
      Node(Leaf(1, text.substr(0, 1)), Leaf(4, text.substr(1, 1)),
           Node(Leaf(2, text.substr(2, 1)), Leaf(10, text.substr(3, 1))),
           Leaf(1, text.substr(4, 1)));
  SyntaxTreeLinter linter;
  linter.AddRule(MakeAscending());
  ASSERT_NE(root.get(), nullptr);
  linter.Lint(*root.get());

  std::vector<LintRuleStatus> statuses = linter.ReportStatus();
  EXPECT_EQ(statuses.size(), 1);
  EXPECT_FALSE(statuses[0].isOk());
  EXPECT_EQ(statuses[0].violations.size(), 1);
}

TEST(SyntaxTreeLinterTest, AscendingFailsTwice) {
  constexpr absl::string_view text("abcde");
  SymbolPtr root =
      Node(Leaf(1, text.substr(0, 1)), Leaf(4, text.substr(1, 1)),
           Node(Leaf(210, text.substr(2, 1)), Leaf(10, text.substr(3, 1))),
           Leaf(1, text.substr(4, 1)));
  SyntaxTreeLinter linter;
  linter.AddRule(MakeAscending());
  ASSERT_NE(root.get(), nullptr);
  linter.Lint(*root.get());

  std::vector<LintRuleStatus> statuses = linter.ReportStatus();
  EXPECT_EQ(statuses.size(), 1);
  EXPECT_FALSE(statuses[0].isOk());
  EXPECT_EQ(statuses[0].violations.size(), 2);
}

TEST(SyntaxTreeLinterTest, HeterogenousTests) {
  constexpr absl::string_view text("abcde");
  SymbolPtr root =
      Node(Leaf(1, text.substr(0, 1)), Leaf(4, text.substr(1, 1)),
           Node(Leaf(210, text.substr(2, 1)), Leaf(10, text.substr(3, 1))),
           Leaf(1, text.substr(4, 1)));
  SyntaxTreeLinter linter;
  linter.AddRule(MakeAscending());
  linter.AddRule(MakeRuleN(1));
  ASSERT_NE(root.get(), nullptr);
  linter.Lint(*root.get());

  std::vector<LintRuleStatus> statuses = linter.ReportStatus();
  EXPECT_EQ(statuses.size(), 2);
  EXPECT_FALSE(statuses[0].isOk());
  EXPECT_EQ(statuses[0].violations.size(), 2);
  EXPECT_FALSE(statuses[1].isOk());
  EXPECT_EQ(statuses[1].violations.size(), 3);
}

// Simple testing rule that verifies that each leaf's tag is the same as its
// depth in the tree.
class TagMatchesContextDepth : public SyntaxTreeLintRule {
 public:
  // When handling a leaf, check leaf's tag vs the size of its context
  void HandleLeaf(const SyntaxTreeLeaf &leaf,
                  const SyntaxTreeContext &context) final {
    if (static_cast<size_t>(leaf.get().token_enum()) != context.size()) {
      violations_.insert(LintViolation(leaf.get(), "", context));
    }
  }
  // Do not process nodes
  void HandleNode(const SyntaxTreeNode &node,
                  const SyntaxTreeContext &context) final {}

  LintRuleStatus Report() const final { return LintRuleStatus(violations_); }

 private:
  std::set<LintViolation> violations_;
};

std::unique_ptr<SyntaxTreeLintRule> MakeDepth() {
  return std::unique_ptr<SyntaxTreeLintRule>(new TagMatchesContextDepth());
}

TEST(SyntaxTreeLinterTest, DepthFails) {
  constexpr absl::string_view text("abcde");
  SymbolPtr root =
      Node(Leaf(1, text.substr(0, 1)), Leaf(4, text.substr(1, 1)),
           Node(Leaf(210, text.substr(2, 1)), Leaf(10, text.substr(3, 1))),
           Leaf(1, text.substr(4, 1)));
  SyntaxTreeLinter linter;
  linter.AddRule(MakeDepth());
  ASSERT_NE(root.get(), nullptr);
  linter.Lint(*root.get());

  std::vector<LintRuleStatus> statuses = linter.ReportStatus();
  EXPECT_EQ(statuses.size(), 1);
  EXPECT_FALSE(statuses[0].isOk());
  EXPECT_EQ(statuses[0].violations.size(), 3);
}

TEST(SyntaxTreeLinterTest, DepthSuccess) {
  constexpr absl::string_view text("abcde");
  SymbolPtr root =
      Node(Leaf(1, text.substr(0, 1)), Leaf(1, text.substr(1, 1)),
           Node(Leaf(2, text.substr(2, 1)), Leaf(2, text.substr(3, 1))),
           Leaf(1, text.substr(4, 1)));
  SyntaxTreeLinter linter;
  linter.AddRule(MakeDepth());
  ASSERT_NE(root.get(), nullptr);
  linter.Lint(*root.get());

  std::vector<LintRuleStatus> statuses = linter.ReportStatus();
  EXPECT_EQ(statuses.size(), 1);
  EXPECT_TRUE(statuses[0].isOk());
  EXPECT_EQ(statuses[0].violations.size(), 0);
}

}  // namespace
}  // namespace verible
