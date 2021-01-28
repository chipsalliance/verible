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

#include "verilog/analysis/lint_rule_registry.h"

#include <stddef.h>

#include <map>
#include <memory>
#include <string>
#include <utility>

#include "absl/strings/match.h"
#include "absl/strings/string_view.h"
#include "common/analysis/line_lint_rule.h"
#include "common/analysis/lint_rule_status.h"
#include "common/analysis/syntax_tree_lint_rule.h"
#include "common/analysis/text_structure_lint_rule.h"
#include "common/analysis/token_stream_lint_rule.h"
#include "common/text/concrete_syntax_leaf.h"
#include "common/text/concrete_syntax_tree.h"
#include "common/text/syntax_tree_context.h"
#include "common/text/text_structure.h"
#include "common/text/token_info.h"
#include "gtest/gtest.h"
#include "verilog/analysis/descriptions.h"

namespace verilog {
namespace analysis {
namespace {

using verible::LineLintRule;
using verible::SyntaxTreeLintRule;
using verible::TextStructureLintRule;
using verible::TokenStreamLintRule;

// Fake SyntaxTreeLintRule that does nothing.
class TreeRuleBase : public SyntaxTreeLintRule {
 public:
  using rule_type = SyntaxTreeLintRule;
  void HandleLeaf(const verible::SyntaxTreeLeaf& leaf,
                  const verible::SyntaxTreeContext& context) override {}
  void HandleNode(const verible::SyntaxTreeNode& node,
                  const verible::SyntaxTreeContext& context) override {}
  verible::LintRuleStatus Report() const override {
    return verible::LintRuleStatus();
  }
};

class TreeRule1 : public TreeRuleBase {
 public:
  using rule_type = SyntaxTreeLintRule;
  static absl::string_view Name() { return "test-rule-1"; }
  static std::string GetDescription(DescriptionType) { return "TreeRule1"; }
};

class TreeRule2 : public TreeRuleBase {
 public:
  using rule_type = SyntaxTreeLintRule;
  static absl::string_view Name() { return "test-rule-2"; }
  static std::string GetDescription(DescriptionType) { return "TreeRule2"; }
};

VERILOG_REGISTER_LINT_RULE(TreeRule1);
VERILOG_REGISTER_LINT_RULE(TreeRule2);

// Verifies that a known syntax tree rule is registered.
TEST(LintRuleRegistryTest, ContainsTreeRuleTrue) {
  EXPECT_TRUE(IsRegisteredLintRule("test-rule-2"));
}

// Verifies that an unknown syntax tree rule is not found.
TEST(LintRuleRegistryTest, ContainsTreeRuleFalse) {
  EXPECT_FALSE(IsRegisteredLintRule("invalid-id"));
}

// Verifies that a nonexistent syntax tree rule yields a nullptr.
TEST(LintRuleRegistryTest, CreateTreeLintRuleInvalid) {
  EXPECT_EQ(CreateSyntaxTreeLintRule("invalid-id"), nullptr);
}

// Verifies that a registered syntax tree rule is properly created.
TEST(LintRuleRegistryTest, CreateTreeLintRuleValid) {
  auto any_rule = CreateSyntaxTreeLintRule("test-rule-1");
  EXPECT_NE(any_rule, nullptr);
  auto rule_1 = dynamic_cast<TreeRule1*>(any_rule.get());
  EXPECT_NE(rule_1, nullptr);
}

// Verifies that GetAllRuleDescriptionsHelpFlag correctly gets the descriptions
// for a SyntaxTreeLintRule.
TEST(GetAllRuleDescriptionsHelpFlagTest, SyntaxRuleValid) {
  const auto rule_map = GetAllRuleDescriptionsHelpFlag();
  EXPECT_EQ(rule_map.size(), 5);
  const auto it = rule_map.find("test-rule-1");
  ASSERT_NE(it, rule_map.end());
  EXPECT_EQ(it->second.description, "TreeRule1");
}

class TokenRuleBase : public TokenStreamLintRule {
 public:
  using rule_type = TokenStreamLintRule;
  void HandleToken(const verible::TokenInfo&) override {}
  verible::LintRuleStatus Report() const override {
    return verible::LintRuleStatus();
  }
};

class TokenRule1 : public TokenRuleBase {
 public:
  using rule_type = TokenStreamLintRule;
  static absl::string_view Name() { return "token-rule-1"; }
  static std::string GetDescription(DescriptionType) { return "TokenRule1"; }
};

VERILOG_REGISTER_LINT_RULE(TokenRule1);

// Verifies that a known token stream rule is registered.
TEST(LintRuleRegistryTest, ContainsTokenRuleTrue) {
  EXPECT_TRUE(IsRegisteredLintRule("token-rule-1"));
}

// Verifies that a nonexistent token stream rule yields a nullptr.
TEST(LintRuleRegistryTest, CreateTokenLintRuleInvalid) {
  EXPECT_EQ(CreateTokenStreamLintRule("invalid-id"), nullptr);
}

// Verifies that a known token stream rule is properly created.
TEST(LintRuleRegistryTest, CreateTokenLintRuleValid) {
  auto any_rule = CreateTokenStreamLintRule("token-rule-1");
  EXPECT_NE(any_rule, nullptr);
  auto rule_1 = dynamic_cast<TokenRule1*>(any_rule.get());
  EXPECT_NE(rule_1, nullptr);
}

// Verifies that GetAllRuleDescriptionsHelpFlag correctly gets the descriptions
// for a TokenStreamLintRule.
TEST(GetAllRuleDescriptionsHelpFlagTest, TokenRuleValid) {
  const auto rule_map = GetAllRuleDescriptionsHelpFlag();
  EXPECT_EQ(rule_map.size(), 5);
  const auto it = rule_map.find("token-rule-1");
  ASSERT_NE(it, rule_map.end());
  EXPECT_EQ(it->second.description, "TokenRule1");
}

class LineRule1 : public LineLintRule {
 public:
  using rule_type = LineLintRule;
  static absl::string_view Name() { return "line-rule-1"; }
  static std::string GetDescription(DescriptionType) { return "LineRule1"; }

  void HandleLine(absl::string_view) override {}
  verible::LintRuleStatus Report() const override {
    return verible::LintRuleStatus();
  }
};

VERILOG_REGISTER_LINT_RULE(LineRule1);

// Verifies that a known line-based rule is registered.
TEST(LintRuleRegistryTest, ContainsLineRuleTrue) {
  EXPECT_TRUE(IsRegisteredLintRule("line-rule-1"));
}

// Verifies that a nonexistent line-based rule yields a nullptr.
TEST(LintRuleRegistryTest, CreateLineLintRuleInvalid) {
  EXPECT_EQ(CreateLineLintRule("invalid-id"), nullptr);
}

// Verifies that a known line-based rule is properly created.
TEST(LintRuleRegistryTest, CreateLineLintRuleValid) {
  auto any_rule = CreateLineLintRule("line-rule-1");
  EXPECT_NE(any_rule, nullptr);
  auto rule_1 = dynamic_cast<LineRule1*>(any_rule.get());
  EXPECT_NE(rule_1, nullptr);
}

// Verifies that GetAllRuleDescriptionsHelpFlag correctly gets the descriptions
// for a LineLintRule.
TEST(GetAllRuleDescriptionsHelpFlagTest, LineRuleValid) {
  const auto rule_map = GetAllRuleDescriptionsHelpFlag();
  EXPECT_EQ(rule_map.size(), 5);
  const auto it = rule_map.find("line-rule-1");
  ASSERT_NE(it, rule_map.end());
  EXPECT_EQ(it->second.description, "LineRule1");
}

class TextRule1 : public TextStructureLintRule {
 public:
  using rule_type = TextStructureLintRule;
  static absl::string_view Name() { return "text-rule-1"; }
  static std::string GetDescription(DescriptionType) { return "TextRule1"; }

  void Lint(const verible::TextStructureView&, absl::string_view) override {}
  verible::LintRuleStatus Report() const override {
    return verible::LintRuleStatus();
  }
};

VERILOG_REGISTER_LINT_RULE(TextRule1);

// Verifies that a known text-structure-based rule is registered.
TEST(LintRuleRegistryTest, ContainsTextRuleTrue) {
  EXPECT_TRUE(IsRegisteredLintRule("text-rule-1"));
}

// Verifies that a nonexistent text-structure-based rule yields a nullptr.
TEST(LintRuleRegistryTest, CreateTextLintRuleInvalid) {
  EXPECT_EQ(CreateTextStructureLintRule("invalid-id"), nullptr);
}

// Verifies that a known text-structure-based rule is properly created.
TEST(LintRuleRegistryTest, CreateTextLintRuleValid) {
  auto any_rule = CreateTextStructureLintRule("text-rule-1");
  EXPECT_NE(any_rule, nullptr);
  auto rule_1 = dynamic_cast<TextRule1*>(any_rule.get());
  EXPECT_NE(rule_1, nullptr);
}

TEST(LintRuleRegistryTest, ConfigureFactoryCreatedRule) {
  auto any_rule = CreateTextStructureLintRule("text-rule-1");
  EXPECT_NE(any_rule, nullptr);
  // Test configuration of freshly instantiated rule.
  absl::Status status = any_rule->Configure("");
  EXPECT_TRUE(status.ok()) << status.message();
  status = any_rule->Configure("bogus");
  EXPECT_FALSE(status.ok());
  EXPECT_TRUE(absl::StrContains(status.message(), "not support configuration"));
}

// Verifies that GetAllRuleDescriptionsHelpFlag correctly gets the descriptions
// for a TextStructureLintRule.
TEST(GetAllRuleDescriptionsHelpFlagTest, TextRuleValid) {
  const auto rule_map = GetAllRuleDescriptionsHelpFlag();
  EXPECT_EQ(rule_map.size(), 5);
  const auto it = rule_map.find("text-rule-1");
  ASSERT_NE(it, rule_map.end());
  EXPECT_EQ(it->second.description, "TextRule1");
}

}  // namespace
}  // namespace analysis
}  // namespace verilog
