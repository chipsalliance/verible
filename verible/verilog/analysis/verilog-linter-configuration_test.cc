// Copyright 2017-2023 The Verible Authors.
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

#include "verible/verilog/analysis/verilog-linter-configuration.h"

#include <iosfwd>
#include <map>
#include <string>
#include <string_view>
#include <type_traits>
#include <utility>
#include <vector>

#include "absl/strings/str_cat.h"
#include "gmock/gmock.h"
#include "gtest/gtest.h"
#include "verible/common/analysis/line-lint-rule.h"
#include "verible/common/analysis/lint-rule-status.h"
#include "verible/common/analysis/syntax-tree-lint-rule.h"
#include "verible/common/analysis/text-structure-lint-rule.h"
#include "verible/common/analysis/token-stream-lint-rule.h"
#include "verible/common/strings/line-column-map.h"
#include "verible/common/text/concrete-syntax-leaf.h"
#include "verible/common/text/concrete-syntax-tree.h"
#include "verible/common/text/syntax-tree-context.h"
#include "verible/common/text/text-structure.h"
#include "verible/common/text/token-info.h"
#include "verible/common/text/tree-builder-test-util.h"
#include "verible/verilog/analysis/default-rules.h"
#include "verible/verilog/analysis/descriptions.h"
#include "verible/verilog/analysis/lint-rule-registry.h"
#include "verible/verilog/analysis/verilog-linter.h"

namespace verilog {
namespace {

using analysis::LintRuleDescriptor;
using verible::LineLintRule;
using verible::SyntaxTreeLintRule;
using verible::TextStructureLintRule;
using verible::TextStructureView;
using verible::TokenStreamLintRule;

using testing::IsEmpty;
using testing::SizeIs;

class TestRuleBase : public SyntaxTreeLintRule {
 public:
  void HandleLeaf(const verible::SyntaxTreeLeaf &leaf,
                  const verible::SyntaxTreeContext &context) final {}
  void HandleNode(const verible::SyntaxTreeNode &node,
                  const verible::SyntaxTreeContext &context) final {}
  verible::LintRuleStatus Report() const final {
    return verible::LintRuleStatus();
  }
};

class TestRule1 : public TestRuleBase {
 public:
  using rule_type = SyntaxTreeLintRule;
  static const LintRuleDescriptor &GetDescriptor() {
    static const LintRuleDescriptor d{
        .name = "test-rule-1",
        .desc = "TestRule1",
    };
    return d;
  }
};

class TestRule2 : public TestRuleBase {
 public:
  using rule_type = SyntaxTreeLintRule;
  static const LintRuleDescriptor &GetDescriptor() {
    static const LintRuleDescriptor d{
        .name = "test-rule-2",
        .desc = "TestRule2",
    };
    return d;
  }
};

class TestRule3 : public TokenStreamLintRule {
 public:
  using rule_type = TokenStreamLintRule;
  static const LintRuleDescriptor &GetDescriptor() {
    static const LintRuleDescriptor d{
        .name = "test-rule-3",
        .desc = "TestRule3",
    };
    return d;
  }

  void HandleToken(const verible::TokenInfo &) final {}

  verible::LintRuleStatus Report() const final {
    return verible::LintRuleStatus();
  }
};

class TestRule4 : public LineLintRule {
 public:
  using rule_type = LineLintRule;
  static const LintRuleDescriptor &GetDescriptor() {
    static const LintRuleDescriptor d{
        .name = "test-rule-4",
        .desc = "TestRule4",
    };
    return d;
  }

  void HandleLine(std::string_view) final {}

  verible::LintRuleStatus Report() const final {
    return verible::LintRuleStatus();
  }
};

class TestRule5 : public TextStructureLintRule {
 public:
  using rule_type = TextStructureLintRule;
  static const LintRuleDescriptor &GetDescriptor() {
    static const LintRuleDescriptor d{
        .name = "test-rule-5",
        .desc = "TestRule5",
    };
    return d;
  }

  void Lint(const TextStructureView &, std::string_view) final {}

  verible::LintRuleStatus Report() const final {
    return verible::LintRuleStatus();
  }
};

VERILOG_REGISTER_LINT_RULE(TestRule1);
VERILOG_REGISTER_LINT_RULE(TestRule2);
VERILOG_REGISTER_LINT_RULE(TestRule3);
VERILOG_REGISTER_LINT_RULE(TestRule4);
VERILOG_REGISTER_LINT_RULE(TestRule5);

// Dummy text structure with a single empty root node for syntax tree.
class FakeTextStructureView : public TextStructureView {
 public:
  FakeTextStructureView() : TextStructureView("") {
    syntax_tree_ = verible::Node();
  }
};

// Don't care about line numbers for these tests.
static const verible::LineColumnMap dummy_map("");

// Don't care about file name for these tests.
static constexpr std::string_view filename;

TEST(ProjectPolicyTest, MatchesAnyPath) {
  struct TestCase {
    ProjectPolicy policy;
    std::string_view filename;
    const char *expected_match;
  };
  const TestCase kTestCases[] = {
      {{"policyX", {}, {}, {}, {}, {}}, "filename", nullptr},
      {{"policyX", {"file"}, {}, {}, {}, {}}, "filename", "file"},
      {{"policyX", {"not-a-match"}, {}, {}, {}, {}}, "filename", nullptr},
      {{"policyX", {"xxxx", "yyyy"}, {}, {}, {}, {}}, "file/name.txt", nullptr},
      {{"policyX", {"xxxx", "name"}, {}, {}, {}, {}}, "file/name.txt", "name"},
      {{"policyX", {"xxxx", "file"}, {}, {}, {}, {}}, "file/name.txt", "file"},
      {{"policyX", {"name", "file"}, {}, {}, {}, {}}, "file/name.txt", "name"},
  };
  for (const auto &test : kTestCases) {
    const char *match = test.policy.MatchesAnyPath(test.filename);
    if (test.expected_match != nullptr) {
      EXPECT_EQ(std::string_view(match), test.expected_match);
    } else {
      EXPECT_EQ(match, nullptr);
    }
  }
}

TEST(ProjectPolicyTest, MatchesAnyExclusions) {
  struct TestCase {
    ProjectPolicy policy;
    std::string_view filename;
    const char *expected_match;
  };
  const TestCase kTestCases[] = {
      {{"policyX", {}, {}, {}, {}, {}}, "filename", nullptr},
      {{"policyX", {}, {"file"}, {}, {}, {}}, "filename", "file"},
      {{"policyX", {}, {"not-a-match"}, {}, {}, {}}, "filename", nullptr},
      {{"policyX", {}, {"xxxx", "yyyy"}, {}, {}, {}}, "file/name.txt", nullptr},
      {{"policyX", {}, {"xxxx", "name"}, {}, {}, {}}, "file/name.txt", "name"},
      {{"policyX", {}, {"xxxx", "file"}, {}, {}, {}}, "file/name.txt", "file"},
      {{"policyX", {}, {"name", "file"}, {}, {}, {}}, "file/name.txt", "name"},
  };
  for (const auto &test : kTestCases) {
    const char *match = test.policy.MatchesAnyExclusions(test.filename);
    if (test.expected_match != nullptr) {
      EXPECT_EQ(std::string_view(match), test.expected_match);
    } else {
      EXPECT_EQ(match, nullptr);
    }
  }
}

TEST(ProjectPolicyTest, IsValid) {
  const std::pair<ProjectPolicy, bool> kTestCases[] = {
      {{"policyX", {"path"}, {}, {"owner"}, {"test-rule-1"}, {}}, true},
      {{"policyX", {"path"}, {}, {"owner"}, {}, {"test-rule-1"}}, true},
      {{"policyX", {"path"}, {}, {"owner"}, {"test-rule-1"}, {"test-rule-2"}},
       true},
      {{"policyX", {"path"}, {}, {"owner"}, {"not-a-test-rule"}, {}}, false},
      {{"policyX", {"path"}, {}, {"owner"}, {}, {"not-a-test-rule"}}, false},
      {{"policyX",
        {"path"},
        {},
        {"owner"},
        {"test-rule-1", "not-a-test-rule"},
        {}},
       false},
      {{"policyX",
        {"path"},
        {},
        {"owner"},
        {},
        {"not-a-test-rule", "test-rule-1"}},
       false},
  };
  for (const auto &test : kTestCases) {
    EXPECT_EQ(test.first.IsValid(), test.second);
  }
}

TEST(ProjectPolicyTest, ListPathGlobs) {
  const std::pair<ProjectPolicy, std::string_view> kTestCases[] = {
      {{"policyX", {}, {}, {}, {}, {}}, ""},
      {{"policyX", {"path"}, {}, {}, {}, {}}, "*path*"},
      {{"policyX", {"path1", "path2"}, {}, {}, {}, {}}, "*path1* | *path2*"},
      {{"policyX", {"pa/th1", "pa/th2"}, {}, {}, {}, {}},
       "*pa/th1* | *pa/th2*"},
  };
  for (const auto &test : kTestCases) {
    EXPECT_EQ(test.first.ListPathGlobs(), test.second);
  }
}

// Confirms that each syntax tree rule yields a set of results.
TEST(VerilogSyntaxTreeLinterConfigurationTest, AddsExpectedNumber) {
  LinterConfiguration config;
  EXPECT_FALSE(config.RuleIsOn("test-rule-1"));
  EXPECT_FALSE(config.RuleIsOn("test-rule-2"));

  config.TurnOn("test-rule-1");
  EXPECT_TRUE(config.RuleIsOn("test-rule-1"));
  EXPECT_FALSE(config.RuleIsOn("test-rule-2"));

  config.TurnOn("test-rule-2");
  EXPECT_TRUE(config.RuleIsOn("test-rule-1"));
  EXPECT_TRUE(config.RuleIsOn("test-rule-2"));
  EXPECT_THAT(config.ActiveRuleIds(), SizeIs(2));

  VerilogLinter linter;
  EXPECT_TRUE(linter.Configure(config, filename).ok());
  FakeTextStructureView text_structure;
  linter.Lint(text_structure, filename);

  auto status = linter.ReportStatus(dummy_map, text_structure.Contents());
  EXPECT_THAT(status, SizeIs(2));
}

// Confirms that each token stream rule yields a set of results.
TEST(VerilogTokenStreamLinterConfigurationTest, AddsExpectedNumber) {
  LinterConfiguration config;
  EXPECT_FALSE(config.RuleIsOn("test-rule-3"));
  config.TurnOn("test-rule-3");
  EXPECT_TRUE(config.RuleIsOn("test-rule-3"));
  EXPECT_THAT(config.ActiveRuleIds(), SizeIs(1));

  VerilogLinter linter;
  EXPECT_TRUE(linter.Configure(config, filename).ok());
  FakeTextStructureView text_structure;
  linter.Lint(text_structure, filename);

  auto status = linter.ReportStatus(dummy_map, text_structure.Contents());
  EXPECT_THAT(status, SizeIs(1));
}

// Confirms that each line-based rule yields a set of results.
TEST(VerilogLineLinterConfigurationTest, AddsExpectedNumber) {
  LinterConfiguration config;
  EXPECT_FALSE(config.RuleIsOn("test-rule-4"));
  config.TurnOn("test-rule-4");
  EXPECT_TRUE(config.RuleIsOn("test-rule-4"));
  EXPECT_THAT(config.ActiveRuleIds(), SizeIs(1));

  VerilogLinter linter;
  EXPECT_TRUE(linter.Configure(config, filename).ok());
  FakeTextStructureView text_structure;
  linter.Lint(text_structure, filename);

  auto status = linter.ReportStatus(dummy_map, text_structure.Contents());
  EXPECT_THAT(status, SizeIs(1));
}

// Confirms that each text-structure rule yields a set of results.
TEST(VerilogTextStructureLinterConfigurationTest, AddsExpectedNumber) {
  LinterConfiguration config;
  EXPECT_FALSE(config.RuleIsOn("test-rule-5"));
  config.TurnOn("test-rule-5");
  EXPECT_TRUE(config.RuleIsOn("test-rule-5"));
  EXPECT_THAT(config.ActiveRuleIds(), SizeIs(1));

  VerilogLinter linter;
  EXPECT_TRUE(linter.Configure(config, filename).ok());
  FakeTextStructureView text_structure;
  linter.Lint(text_structure, filename);

  auto status = linter.ReportStatus(dummy_map, text_structure.Contents());
  EXPECT_THAT(status, SizeIs(1));
}

// Verifies that turning on-off rules works.
TEST(VerilogSyntaxTreeLinterConfigurationTest, TurnOnTurnOff) {
  LinterConfiguration config;
  EXPECT_THAT(config.ActiveRuleIds(), IsEmpty());
  config.TurnOn("test-rule-1");
  EXPECT_TRUE(config.RuleIsOn("test-rule-1"));
  config.TurnOff("test-rule-1");
  EXPECT_FALSE(config.RuleIsOn("test-rule-1"));
  EXPECT_THAT(config.ActiveRuleIds(), IsEmpty());

  VerilogLinter linter;
  EXPECT_TRUE(linter.Configure(config, filename).ok());
  FakeTextStructureView text_structure;
  linter.Lint(text_structure, filename);

  auto status = linter.ReportStatus(dummy_map, text_structure.Contents());
  EXPECT_THAT(status, IsEmpty());
}

TEST(LinterConfigurationTest, ComparisonOperatorSameElement) {
  LinterConfiguration config1, config2;
  EXPECT_EQ(config1, config2);
  config1.TurnOn("rule-x");
  EXPECT_NE(config1, config2);
  config2.TurnOn("rule-x");
  EXPECT_EQ(config1, config2);
  config1.TurnOff("rule-x");
  EXPECT_NE(config1, config2);
  config2.TurnOff("rule-x");
  EXPECT_EQ(config1, config2);
}

TEST(LinterConfigurationTest, ComparisonSameDifferentElement) {
  LinterConfiguration config1, config2;
  config1.TurnOn("rule-x");
  EXPECT_NE(config1, config2);
  config2.TurnOn("rule-y");
  EXPECT_NE(config1, config2);
  config1.TurnOff("rule-x");
  EXPECT_NE(config1, config2);
  config2.TurnOff("rule-y");
  EXPECT_EQ(config1, config2);
}

TEST(LinterConfigurationTest, StreamOperator) {
  LinterConfiguration config;
  {
    std::ostringstream stream;
    stream << config;
    EXPECT_EQ(stream.str(), "{  }");
  }
  config.TurnOn("rule-abc");
  {
    std::ostringstream stream;
    stream << config;
    EXPECT_EQ(stream.str(), "{ rule-abc }");
  }
  config.TurnOn("rule-xyz");
  {
    std::ostringstream stream;
    stream << config;
    EXPECT_EQ(stream.str(), "{ rule-abc, rule-xyz }");
  }
  config.TurnOff("rule-abc");
  {
    std::ostringstream stream;
    stream << config;
    EXPECT_EQ(stream.str(), "{ rule-xyz }");
  }
  config.TurnOff("rule-xyz");
  {
    std::ostringstream stream;
    stream << config;
    EXPECT_EQ(stream.str(), "{  }");
  }
}

TEST(VerilogSyntaxTreeLinterConfigurationTest, DefaultEmpty) {
  LinterConfiguration config;
  EXPECT_THAT(config.ActiveRuleIds(), IsEmpty());

  VerilogLinter linter;
  EXPECT_TRUE(linter.Configure(config, filename).ok());
  FakeTextStructureView text_structure;
  linter.Lint(text_structure, filename);

  auto status = linter.ReportStatus(dummy_map, text_structure.Contents());
  EXPECT_THAT(status, IsEmpty());
}

TEST(VerilogSyntaxTreeLinterConfigurationTest, UseRuleSetAll) {
  LinterConfiguration config;
  config.UseRuleSet(RuleSet::kAll);

  auto expected_size = analysis::RegisteredSyntaxTreeRulesNames().size() +
                       analysis::RegisteredTokenStreamRulesNames().size() +
                       analysis::RegisteredTextStructureRulesNames().size() +
                       analysis::RegisteredLineRulesNames().size();
  EXPECT_THAT(config.ActiveRuleIds(), SizeIs(expected_size));

  VerilogLinter linter;
  EXPECT_TRUE(linter.Configure(config, filename).ok());
  FakeTextStructureView text_structure;
  linter.Lint(text_structure, filename);

  auto status = linter.ReportStatus(dummy_map, text_structure.Contents());
  EXPECT_THAT(status, SizeIs(expected_size));
}

TEST(VerilogSyntaxTreeLinterConfigurationTest, UseRuleSetNone) {
  LinterConfiguration config;
  config.UseRuleSet(RuleSet::kNone);
  EXPECT_THAT(config.ActiveRuleIds(), IsEmpty());

  VerilogLinter linter;
  EXPECT_TRUE(linter.Configure(config, filename).ok());
  FakeTextStructureView text_structure;
  linter.Lint(text_structure, filename);

  auto status = linter.ReportStatus(dummy_map, text_structure.Contents());
  EXPECT_THAT(status, IsEmpty());
}

TEST(VerilogSyntaxTreeLinterConfigurationTest, NoneResets) {
  LinterConfiguration config;
  config.TurnOn("test-rule-1");
  config.TurnOn("test-rule-2");
  config.TurnOn("test-rule-3");
  config.TurnOn("test-rule-4");
  config.UseRuleSet(RuleSet::kNone);
  EXPECT_THAT(config.ActiveRuleIds(), IsEmpty());

  VerilogLinter linter;
  EXPECT_TRUE(linter.Configure(config, filename).ok());
  FakeTextStructureView text_structure;
  linter.Lint(text_structure, filename);

  auto status = linter.ReportStatus(dummy_map, text_structure.Contents());
  EXPECT_THAT(status, IsEmpty());
}

TEST(VerilogSyntaxTreeLinterConfigurationTest, UseRuleSetDefault) {
  LinterConfiguration config;
  config.UseRuleSet(RuleSet::kDefault);

  VerilogLinter linter;
  EXPECT_TRUE(linter.Configure(config, filename).ok());
  FakeTextStructureView text_structure;
  linter.Lint(text_structure, filename);

  auto status = linter.ReportStatus(dummy_map, text_structure.Contents());
  auto expected_size = std::extent_v<decltype(analysis::kDefaultRuleSet)>;
  EXPECT_THAT(config.ActiveRuleIds(), SizeIs(expected_size));
  EXPECT_THAT(status, SizeIs(expected_size));
}

// Tests that empty policy doesn't cause any change in configuration.
TEST(LinterConfigurationUseProjectPolicyTest, BlankPolicyBlankFilename) {
  LinterConfiguration config, default_config;
  ProjectPolicy policy;
  config.UseProjectPolicy(policy, "");
  EXPECT_EQ(config, default_config);
}

// Test single rule can be enabled with path matching.
TEST(LinterConfigurationUseProjectPolicyTest, EnableRule) {
  LinterConfiguration config;
  ProjectPolicy policy{"policyX", {"path"}, {}, {"owner"}, {}, {"wanted-rule"}};
  EXPECT_FALSE(config.RuleIsOn("wanted-rule"));
  config.UseProjectPolicy(policy, "some/path/foo");
  EXPECT_TRUE(config.RuleIsOn("wanted-rule"));
}

// Test that rule is not enabled because path does not match.
TEST(LinterConfigurationUseProjectPolicyTest, EnableFilePathNotMatched) {
  LinterConfiguration config;
  ProjectPolicy policy{"policyX", {"not-gonna-match"}, {}, {"owner"},
                       {},        {"wanted-rule"}};
  EXPECT_FALSE(config.RuleIsOn("wanted-rule"));
  config.UseProjectPolicy(policy, "some/path/foo");
  EXPECT_FALSE(config.RuleIsOn("wanted-rule"));
}

// Test single rule can be disabled with path matching.
TEST(LinterConfigurationUseProjectPolicyTest, DisableRule) {
  LinterConfiguration config;
  config.TurnOn("unwanted-rule");
  ProjectPolicy policy{"policyX", {"path"},          {},
                       {"owner"}, {"unwanted-rule"}, {}};
  EXPECT_TRUE(config.RuleIsOn("unwanted-rule"));
  config.UseProjectPolicy(policy, "some/path/foo");
  EXPECT_FALSE(config.RuleIsOn("unwanted-rule"));
}

// Test that rule remains enabled because path does not match.
TEST(LinterConfigurationUseProjectPolicyTest, DisableRulePathNotMatched) {
  LinterConfiguration config;
  config.TurnOn("unwanted-rule");
  ProjectPolicy policy{"policyX", {"does-not-match"}, {},
                       {"owner"}, {"unwanted-rule"},  {}};
  EXPECT_TRUE(config.RuleIsOn("unwanted-rule"));
  config.UseProjectPolicy(policy, "some/path/foo");
  EXPECT_TRUE(config.RuleIsOn("unwanted-rule"));
}

// Test that enabling a rule takes precedence over disabling.
TEST(LinterConfigurationUseProjectPolicyTest, EnableRuleWins) {
  LinterConfiguration config;
  // Same rule is disabled and enabled.
  ProjectPolicy policy{"policyX", {"path"},        {},
                       {"owner"}, {"wanted-rule"}, {"wanted-rule"}};
  EXPECT_FALSE(config.RuleIsOn("wanted-rule"));
  config.UseProjectPolicy(policy, "some/path/foo");
  EXPECT_TRUE(config.RuleIsOn("wanted-rule"));
}

//
// Tests for parse/unparse on RuleSet
//
TEST(RuleSetTest, ParseRuleSetSuccess) {
  std::string none_text = "none";
  std::string all_text = "all";
  std::string default_text = "default";

  std::string none_error;
  std::string all_error;
  std::string default_error;

  RuleSet none_destination, all_destination, default_destination;

  bool none_result = AbslParseFlag(none_text, &none_destination, &none_error);
  bool all_result = AbslParseFlag(all_text, &all_destination, &all_error);
  bool default_result =
      AbslParseFlag(default_text, &default_destination, &default_error);

  EXPECT_TRUE(none_result);
  EXPECT_EQ(none_error, "");
  EXPECT_EQ(none_destination, RuleSet::kNone);

  EXPECT_TRUE(all_result);
  EXPECT_EQ(all_error, "");
  EXPECT_EQ(all_destination, RuleSet::kAll);

  EXPECT_TRUE(default_result);
  EXPECT_EQ(default_error, "");
  EXPECT_EQ(default_destination, RuleSet::kDefault);
}

TEST(RuleSetTest, ParseRuleSetError) {
  std::string bad_text = "fdsfdfds";
  std::string error;

  RuleSet rule_result;
  bool result = AbslParseFlag(bad_text, &rule_result, &error);
  EXPECT_FALSE(result);
  EXPECT_NE(error, "");
}

TEST(RuleSetTest, UnparseRuleSetSuccess) {
  EXPECT_EQ("none", AbslUnparseFlag(RuleSet::kNone));
  EXPECT_EQ("default", AbslUnparseFlag(RuleSet::kDefault));
  EXPECT_EQ("all", AbslUnparseFlag(RuleSet::kAll));
}

//
// Tests for parse / unparse on RuleBundle
//
TEST(RuleBundleTest, UnparseRuleBundleSeveral) {
  RuleBundle bundle = {{{"flag1", {true, ""}}, {"flag2", {true, ""}}}};
  std::string expected_comma = "flag2,flag1";
  std::string expected_newline = "flag2\nflag1";

  std::string result_comma = bundle.UnparseConfiguration(',');
  EXPECT_EQ(result_comma, expected_comma);

  std::string result_newline = bundle.UnparseConfiguration('\n');
  EXPECT_EQ(result_newline, expected_newline);
}

TEST(RuleBundleTest, UnparseRuleBundleSeveralTurnOff) {
  RuleBundle bundle = {{{"flag1", {false, ""}}, {"flag2", {true, ""}}}};
  std::string expected_comma = "flag2,-flag1";
  std::string expected_newline = "flag2\n-flag1";

  std::string result_comma = bundle.UnparseConfiguration(',');
  EXPECT_EQ(result_comma, expected_comma);

  std::string result_newline = bundle.UnparseConfiguration('\n');
  EXPECT_EQ(result_newline, expected_newline);
}

TEST(RuleBundleTest, UnparseRuleBundleSeveralConfiguration) {
  RuleBundle bundle = {{{"flag1", {false, "foo"}}, {"flag2", {true, "bar"}}}};
  std::string expected_comma = "flag2=bar,-flag1=foo";
  std::string expected_newline = "flag2=bar\n-flag1=foo";

  std::string result_comma = bundle.UnparseConfiguration(',');
  EXPECT_EQ(result_comma, expected_comma);

  std::string result_newline = bundle.UnparseConfiguration('\n');
  EXPECT_EQ(result_newline, expected_newline);
}

TEST(RuleBundleTest, UnparseRuleBundleEmpty) {
  RuleBundle bundle = {};
  std::string expected;

  std::string result_comma = bundle.UnparseConfiguration(',');
  EXPECT_EQ(result_comma, expected);

  std::string result_newline = bundle.UnparseConfiguration('\n');
  EXPECT_EQ(result_newline, expected);
}

TEST(RuleBundleTest, ParseRuleBundleEmpty) {
  constexpr std::string_view text;
  RuleBundle bundle;
  std::string error;
  bool success = bundle.ParseConfiguration(text, ',', &error);
  EXPECT_TRUE(success) << error;
  EXPECT_TRUE(error.empty());
  EXPECT_TRUE(bundle.rules.empty());
}

TEST(RuleBundleTest, ParseRuleBundleAcceptSeveral) {
  // Allow for an optional '+' to enable a rule for symmetry with '-' disable
  constexpr std::string_view text = "test-rule-1,test-rule-2,+test-rule-3";
  RuleBundle bundle;
  std::string error;
  bool success = bundle.ParseConfiguration(text, ',', &error);
  ASSERT_TRUE(success) << error;
  ASSERT_THAT(bundle.rules, SizeIs(3));
  EXPECT_TRUE(error.empty());
  EXPECT_TRUE(bundle.rules["test-rule-1"].enabled);
  EXPECT_TRUE(bundle.rules["test-rule-2"].enabled);
  EXPECT_TRUE(bundle.rules["test-rule-3"].enabled);
}

TEST(RuleBundleTest, ParseRuleBundleAcceptConfiguration) {
  constexpr std::string_view text =
      "test-rule-1=foo,test-rule-2=,test-rule-3,-test-rule-4=bar";
  RuleBundle bundle;
  std::string error;
  bool success = bundle.ParseConfiguration(text, ',', &error);
  ASSERT_TRUE(success) << error;
  ASSERT_THAT(bundle.rules, SizeIs(4));
  EXPECT_TRUE(error.empty());

  EXPECT_TRUE(bundle.rules["test-rule-1"].enabled);
  EXPECT_EQ("foo", bundle.rules["test-rule-1"].configuration);

  EXPECT_TRUE(bundle.rules["test-rule-2"].enabled);
  EXPECT_TRUE(bundle.rules["test-rule-2"].configuration.empty());

  EXPECT_TRUE(bundle.rules["test-rule-3"].enabled);
  EXPECT_TRUE(bundle.rules["test-rule-3"].configuration.empty());

  EXPECT_FALSE(bundle.rules["test-rule-4"].enabled);
  EXPECT_EQ("bar", bundle.rules["test-rule-4"].configuration);
}

TEST(RuleBundleTest, ParseRuleBundleWithQuotationMarks) {
  constexpr std::string_view text =
      "test-rule-1=\"foo\",test-rule-2=\"\",test-rule-3,-test-rule-4=\"bar\"";
  RuleBundle bundle;
  std::string error;
  bool success = bundle.ParseConfiguration(text, ',', &error);
  ASSERT_TRUE(success) << error;
  ASSERT_THAT(bundle.rules, SizeIs(4));
  EXPECT_TRUE(error.empty());

  EXPECT_TRUE(bundle.rules["test-rule-1"].enabled);
  EXPECT_EQ("foo", bundle.rules["test-rule-1"].configuration);

  EXPECT_TRUE(bundle.rules["test-rule-2"].enabled);
  EXPECT_TRUE(bundle.rules["test-rule-2"].configuration.empty());

  EXPECT_TRUE(bundle.rules["test-rule-3"].enabled);
  EXPECT_TRUE(bundle.rules["test-rule-3"].configuration.empty());

  EXPECT_FALSE(bundle.rules["test-rule-4"].enabled);
  EXPECT_EQ("bar", bundle.rules["test-rule-4"].configuration);
}

TEST(RuleBundleTest, ParseRuleBundleAcceptOne) {
  constexpr std::string_view text = "test-rule-1";
  RuleBundle bundle;
  std::string error;
  bool success = bundle.ParseConfiguration(text, ',', &error);
  EXPECT_TRUE(error.empty());
  ASSERT_TRUE(success) << error;
  ASSERT_THAT(bundle.rules, SizeIs(1));
  EXPECT_TRUE(bundle.rules["test-rule-1"].enabled);
}

TEST(RuleBundleTest, ParseRuleWhitespaceAroundAllowed) {
  constexpr std::string_view text =
      "\t test-rule-1 \t, +test-rule-2=foo:bar \t";
  RuleBundle bundle;
  std::string error;
  bool success = bundle.ParseConfiguration(text, ',', &error);
  EXPECT_TRUE(error.empty());
  ASSERT_TRUE(success) << error;
  ASSERT_THAT(bundle.rules, SizeIs(2));
  EXPECT_TRUE(bundle.rules["test-rule-1"].enabled);
  EXPECT_TRUE(bundle.rules["test-rule-2"].enabled);
  EXPECT_EQ("foo:bar", bundle.rules["test-rule-2"].configuration);
}

TEST(RuleBundleTest, ParseRuleBundleAcceptSeveralTurnOff) {
  constexpr std::string_view text = "test-rule-1,-test-rule-2";
  RuleBundle bundle;
  std::string error;
  bool success = bundle.ParseConfiguration(text, ',', &error);
  ASSERT_TRUE(success) << error;
  ASSERT_THAT(bundle.rules, SizeIs(2));
  EXPECT_TRUE(error.empty());
  EXPECT_TRUE(bundle.rules["test-rule-1"].enabled);
  EXPECT_FALSE(bundle.rules["test-rule-2"].enabled);
}

TEST(RuleBundleTest, ParseRuleBundleAcceptOneTurnOff) {
  constexpr std::string_view text = "-test-rule-1";
  RuleBundle bundle;
  std::string error;
  bool success = bundle.ParseConfiguration(text, ',', &error);
  ASSERT_TRUE(success) << error;
  ASSERT_THAT(bundle.rules, SizeIs(1));
  EXPECT_TRUE(error.empty());
  EXPECT_FALSE(bundle.rules["test-rule-1"].enabled);
}

TEST(RuleBundleTest, ParseRuleBundleReject) {
  constexpr std::string_view text = "test-rule-1,bad-flag";
  RuleBundle bundle;
  std::string error;
  bool success = bundle.ParseConfiguration(text, ',', &error);
  EXPECT_FALSE(success);
  EXPECT_EQ(error, absl::StrCat(kInvalidFlagMessage, " \"bad-flag\""));
}

TEST(RuleBundleTest, ParseRuleBundleAcceptGoodRulesEvenWhenRejecting) {
  constexpr std::string_view text = "test-rule-unknown-rules\ntest-rule-1";
  {
    RuleBundle bundle;
    std::string error;
    bool success = bundle.ParseConfiguration(text, '\n', &error);
    ASSERT_TRUE(!success) << error;
    EXPECT_THAT(error, testing::HasSubstr(kInvalidFlagMessage))
        << error;  // invalid flag report
    // Enable test-rule-1 even though we saw an invalid flag
    EXPECT_TRUE(bundle.rules["test-rule-1"].enabled);
  }
}

TEST(RuleBundleTest, ParseRuleBundleAcceptMultiline) {
  constexpr std::string_view text = "test-rule-1\n-test-rule-2";
  RuleBundle bundle;
  std::string error;
  bool success = bundle.ParseConfiguration(text, '\n', &error);
  ASSERT_TRUE(success) << error;
  ASSERT_THAT(bundle.rules, SizeIs(2));
  EXPECT_TRUE(error.empty());
  EXPECT_TRUE(bundle.rules["test-rule-1"].enabled);
  EXPECT_FALSE(bundle.rules["test-rule-2"].enabled);
}

TEST(RuleBundleTest, ParseRuleBundleRejectMultiline) {
  constexpr std::string_view text = "test-rule-1\nbad-flag\n-test-rule-2";
  RuleBundle bundle;
  std::string error;
  bool success = bundle.ParseConfiguration(text, '\n', &error);
  EXPECT_FALSE(success);
  EXPECT_EQ(error, absl::StrCat(kInvalidFlagMessage, " \"bad-flag\""));
}

TEST(RuleBundleTest, ParseRuleBundleSkipComments) {
  constexpr std::string_view text =
      "    # some comment after whitespace\n"
      "# more comment\n"
      "test-rule-1\n"
      "-test-rule-2  # some comment\n"
      "+test-rule-3=bar:baz  # config-comment\n";
  {
    RuleBundle bundle;
    std::string error;
    bool success = bundle.ParseConfiguration(text, '\n', &error);
    ASSERT_TRUE(success) << error;
    ASSERT_THAT(bundle.rules, SizeIs(3));
    EXPECT_TRUE(error.empty());
    EXPECT_TRUE(bundle.rules["test-rule-1"].enabled);
    EXPECT_FALSE(bundle.rules["test-rule-2"].enabled);
    EXPECT_TRUE(bundle.rules["test-rule-3"].enabled);
    EXPECT_EQ("bar:baz", bundle.rules["test-rule-3"].configuration);
  }
}

TEST(RuleBundleTest, ParseRuleBundleIgnoreExtraComma) {
  // Multiline rules might still have a comma from the one-line
  // rule configuration. They shouldn't harm.
  constexpr std::string_view text =
      "test-rule-1,,,  \n"
      "-test-rule-2=a:b,\n"
      "+test-rule-3=bar:baz,  # config-comment\n";
  {
    RuleBundle bundle;
    std::string error;
    bool success = bundle.ParseConfiguration(text, '\n', &error);
    ASSERT_TRUE(success) << error;
    ASSERT_NE(error.find(','), std::string::npos) << error;  // warning report
    ASSERT_THAT(bundle.rules, SizeIs(3));
    EXPECT_TRUE(bundle.rules["test-rule-1"].enabled);
    EXPECT_FALSE(bundle.rules["test-rule-2"].enabled);
    EXPECT_EQ("a:b", bundle.rules["test-rule-2"].configuration);
    EXPECT_TRUE(bundle.rules["test-rule-3"].enabled);
    EXPECT_EQ("bar:baz", bundle.rules["test-rule-3"].configuration);
  }
}

TEST(RuleBundleTest, ParseRuleBundleDontWarnIfNoConfig) {
  constexpr std::string_view text = "test-rule-1,\ntest-rule-1";
  {
    RuleBundle bundle;
    std::string error;
    bool success = bundle.ParseConfiguration(text, '\n', &error);
    ASSERT_TRUE(success) << error;
    EXPECT_THAT(error, testing::Not(testing::HasSubstr(kRepeatedFlagMessage)))
        << error;  // don't warn about overriden config if there is no value
    EXPECT_TRUE(bundle.rules["test-rule-1"].enabled);
  }
}

TEST(RuleBundleTest, ParseRuleBundleWarnConfigOverride) {
  constexpr std::string_view text = "test-rule-1=a,\ntest-rule-1=b";
  {
    RuleBundle bundle;
    std::string error;
    bool success = bundle.ParseConfiguration(text, '\n', &error);
    ASSERT_TRUE(!success) << error;
    EXPECT_THAT(error, testing::HasSubstr(kRepeatedFlagMessage))
        << error;  // warning: configuration being overriden
    EXPECT_TRUE(bundle.rules["test-rule-1"].enabled);
    EXPECT_EQ("b", bundle.rules["test-rule-1"].configuration);
  }
}

// ConfigureFromOptions Tests
TEST(ConfigureFromOptionsTest, Basic) {
  LinterConfiguration config;

  LinterOptions options = {.ruleset = RuleSet::kAll,
                           .rules = RuleBundle(),
                           .config_file = "",
                           .rules_config_search = false,
                           .linting_start_file = "filename",
                           .waiver_files = "filename"};

  auto status = config.ConfigureFromOptions(options);
  EXPECT_TRUE(status.ok());
}

TEST(ConfigureFromOptionsTest, LoadFromNonExistingFile) {
  LinterConfiguration config;

  LinterOptions options = {.ruleset = RuleSet::kAll,
                           .rules = RuleBundle(),
                           .config_file = "non-existent-file.txt",
                           .rules_config_search = false,
                           .linting_start_file = "filename",
                           .waiver_files = "filename"};

  auto status = config.ConfigureFromOptions(options);
  EXPECT_FALSE(status.ok()) << status.message();
}

TEST(ConfigureFromOptionsTest, RulesNumber) {
  LinterConfiguration config;

  LinterOptions options = {.ruleset = RuleSet::kAll,
                           .rules = RuleBundle(),
                           .config_file = "",
                           .rules_config_search = false,
                           .linting_start_file = "filename",
                           .waiver_files = "filename"};

  auto status = config.ConfigureFromOptions(options);
  EXPECT_TRUE(status.ok());

  // Should enable all rules because uses kAll ruleset and an empty rulebundle
  auto expected_size = analysis::RegisteredSyntaxTreeRulesNames().size() +
                       analysis::RegisteredTokenStreamRulesNames().size() +
                       analysis::RegisteredTextStructureRulesNames().size() +
                       analysis::RegisteredLineRulesNames().size();
  EXPECT_THAT(config.ActiveRuleIds(), SizeIs(expected_size));
}

TEST(ConfigureFromOptionsTest, RulesSelective) {
  LinterConfiguration config;

  RuleBundle bundle = {
      {{analysis::RegisteredSyntaxTreeRulesNames()[0], {false, ""}}}};

  LinterOptions options = {.ruleset = RuleSet::kAll,
                           .rules = bundle,
                           .config_file = "",
                           .rules_config_search = false,
                           .linting_start_file = "filename",
                           .waiver_files = "filename"};

  auto status = config.ConfigureFromOptions(options);
  EXPECT_TRUE(status.ok());

  // Should enable all rules - 1 because uses kAll ruleset and a rulebundle
  // with one rule disabled
  auto expected_size = analysis::RegisteredSyntaxTreeRulesNames().size() +
                       analysis::RegisteredTokenStreamRulesNames().size() +
                       analysis::RegisteredTextStructureRulesNames().size() +
                       analysis::RegisteredLineRulesNames().size() - 1;

  EXPECT_THAT(config.ActiveRuleIds(), SizeIs(expected_size));
}
// TODO: LinterOptions could be refactored to store the content
// of the configuration files. After this is made it will be possible to
// test the configuration that is applied after reading the files.

}  // namespace
}  // namespace verilog
