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

#include "verible/verilog/analysis/lint-rule-registry.h"

#include <map>
#include <memory>
#include <utility>

#include "absl/status/status.h"
#include "absl/strings/match.h"
#include "absl/strings/string_view.h"
#include "gtest/gtest.h"
#include "verible/common/analysis/line-lint-rule.h"
#include "verible/common/analysis/lint-rule-status.h"
#include "verible/common/analysis/syntax-tree-lint-rule.h"
#include "verible/common/analysis/text-structure-lint-rule.h"
#include "verible/common/analysis/token-stream-lint-rule.h"
#include "verible/common/text/concrete-syntax-leaf.h"
#include "verible/common/text/concrete-syntax-tree.h"
#include "verible/common/text/syntax-tree-context.h"
#include "verible/common/text/text-structure.h"
#include "verible/common/text/token-info.h"
#include "verible/verilog/analysis/descriptions.h"

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
  void HandleLeaf(const verible::SyntaxTreeLeaf &leaf,
                  const verible::SyntaxTreeContext &context) final {}
  void HandleNode(const verible::SyntaxTreeNode &node,
                  const verible::SyntaxTreeContext &context) final {}
  verible::LintRuleStatus Report() const final {
    return verible::LintRuleStatus();
  }
};

class TreeRule1 : public TreeRuleBase {
 public:
  using rule_type = SyntaxTreeLintRule;
  static const LintRuleDescriptor &GetDescriptor() {
    static const LintRuleDescriptor d{
        .name = "test-rule-1",
        .desc = "TreeRule1",
    };
    return d;
  }
};

class TreeRule2 : public TreeRuleBase {
 public:
  using rule_type = SyntaxTreeLintRule;
  static const LintRuleDescriptor &GetDescriptor() {
    static const LintRuleDescriptor d{
        .name = "test-rule-2",
        .desc = "TreeRule2",
    };
    return d;
  }
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
#if defined(__GXX_RTTI)
  auto rule_1 = dynamic_cast<TreeRule1 *>(any_rule.get());
  EXPECT_NE(rule_1, nullptr);
#endif
}

// Verifies that GetAllRuleDescriptionsHelpFlag correctly gets the descriptions
// for a SyntaxTreeLintRule.
TEST(GetAllRuleDescriptions, SyntaxRuleValid) {
  const auto rule_map = GetAllRuleDescriptions();
  EXPECT_EQ(rule_map.size(), 5);
  const auto it = rule_map.find("test-rule-1");
  ASSERT_NE(it, rule_map.end());
  EXPECT_EQ(it->second.descriptor.desc, "TreeRule1");
}

class TokenRuleBase : public TokenStreamLintRule {
 public:
  using rule_type = TokenStreamLintRule;
  void HandleToken(const verible::TokenInfo &) final {}
  verible::LintRuleStatus Report() const final {
    return verible::LintRuleStatus();
  }
};

class TokenRule1 : public TokenRuleBase {
 public:
  using rule_type = TokenStreamLintRule;
  static const LintRuleDescriptor &GetDescriptor() {
    static const LintRuleDescriptor d{
        .name = "token-rule-1",
        .desc = "TokenRule1",
    };
    return d;
  }
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
#if defined(__GXX_RTTI)
  auto rule_1 = dynamic_cast<TokenRule1 *>(any_rule.get());
  EXPECT_NE(rule_1, nullptr);
#endif
}

// Verifies that GetAllRuleDescriptionsHelpFlag correctly gets the descriptions
// for a TokenStreamLintRule.
TEST(GetAllRuleDescriptions, TokenRuleValid) {
  const auto rule_map = GetAllRuleDescriptions();
  EXPECT_EQ(rule_map.size(), 5);
  const auto it = rule_map.find("token-rule-1");
  ASSERT_NE(it, rule_map.end());
  EXPECT_EQ(it->second.descriptor.desc, "TokenRule1");
}

class LineRule1 : public LineLintRule {
 public:
  using rule_type = LineLintRule;
  static const LintRuleDescriptor &GetDescriptor() {
    static const LintRuleDescriptor d{
        .name = "line-rule-1",
        .desc = "LineRule1",
    };
    return d;
  }

  void HandleLine(absl::string_view) final {}
  verible::LintRuleStatus Report() const final {
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
#if defined(__GXX_RTTI)
  auto rule_1 = dynamic_cast<LineRule1 *>(any_rule.get());
  EXPECT_NE(rule_1, nullptr);
#endif
}

// Verifies that GetAllRuleDescriptionsHelpFlag correctly gets the descriptions
// for a LineLintRule.
TEST(GetAllRuleDescriptions, LineRuleValid) {
  const auto rule_map = GetAllRuleDescriptions();
  EXPECT_EQ(rule_map.size(), 5);
  const auto it = rule_map.find("line-rule-1");
  ASSERT_NE(it, rule_map.end());
  EXPECT_EQ(it->second.descriptor.desc, "LineRule1");
}

class TextRule1 : public TextStructureLintRule {
 public:
  using rule_type = TextStructureLintRule;
  static const LintRuleDescriptor &GetDescriptor() {
    static const LintRuleDescriptor d{
        .name = "text-rule-1",
        .desc = "TextRule1",
    };
    return d;
  }

  void Lint(const verible::TextStructureView &, absl::string_view) final {}
  verible::LintRuleStatus Report() const final {
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
#if defined(__GXX_RTTI)
  auto rule_1 = dynamic_cast<TextRule1 *>(any_rule.get());
  EXPECT_NE(rule_1, nullptr);
#endif
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
TEST(GetAllRuleDescriptions, TextRuleValid) {
  const auto rule_map = GetAllRuleDescriptions();
  EXPECT_EQ(rule_map.size(), 5);
  const auto it = rule_map.find("text-rule-1");
  ASSERT_NE(it, rule_map.end());
  EXPECT_EQ(it->second.descriptor.desc, "TextRule1");
}

}  // namespace
}  // namespace analysis
}  // namespace verilog
