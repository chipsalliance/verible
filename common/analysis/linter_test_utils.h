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

#ifndef VERIBLE_COMMON_ANALYSIS_LINTER_TEST_UTILS_H_
#define VERIBLE_COMMON_ANALYSIS_LINTER_TEST_UTILS_H_

#include <functional>
#include <initializer_list>
#include <iosfwd>
#include <memory>
#include <set>
#include <sstream>

#include "gtest/gtest.h"
#include "absl/status/status.h"
#include "absl/strings/str_cat.h"
#include "absl/strings/string_view.h"
#include "common/analysis/lint_rule_status.h"
#include "common/text/token_info_test_util.h"

namespace verible {

// LintTestCase is a struct for describing a chunk of text and where
// a linter should fail on it.  See TokenInfoTestData for original
// concept.
// This has the same limitations as TokenInfoTestData, such as
// the inability to express nested findings, which requires a tree
// representation of expected data.
// TODO(fangism): upgrade to nest-able expected findings tree structure.
struct LintTestCase : public TokenInfoTestData {
  // Forwarding constructor to base class.
  LintTestCase(std::initializer_list<ExpectedTokenInfo> fragments)
      : TokenInfoTestData(fragments) {}

  // Compare the set of expected findings against actual findings.
  // Detailed differences are written to diffstream.
  // 'base' is the full text buffer that was analyzed, and is used to
  // calculate byte offsets in diagnostics.
  // Returns true if every element is an exact match to the expected set.
  // TODO(b/141875806): Take a symbol translator function to produce a
  // human-readable, language-specific enum name.
  bool ExactMatchFindings(const std::set<LintViolation>& found_violations,
                          absl::string_view base,
                          std::ostream* diffstream) const;
};

template <typename RuleType>
using LintRuleGenerator = std::function<std::unique_ptr<RuleType>()>;

// General template that runs lint rule tests depending on the rule type.
// Only defined by specializations in *linter_test_utils.h.
template <class RuleType>
class LintRunner;

// Tests that LintTestCase test has expected violations under make_rule
// Expects test.code to be accepted by AnalyzerType.
template <class AnalyzerType, class RuleType>
void RunLintTestCase(const LintTestCase& test,
                     const LintRuleGenerator<RuleType>& make_rule,
                     absl::string_view filename) {
  // All linters start by parsing to yield a TextStructure.
  AnalyzerType analyzer(test.code, filename);
  absl::Status unused_parser_status = analyzer.Analyze();

  // Instantiate a linter that runs a single rule to analyze text.
  LintRunner<RuleType> lint_runner(make_rule());
  const LintRuleStatus rule_status = lint_runner.Run(analyzer.Data(), filename);
  const auto& violations(rule_status.violations);

  // Report detailed differences, if any.
  const absl::string_view base_text = analyzer.Data().Contents();
  std::ostringstream diffs;
  EXPECT_TRUE(test.ExactMatchFindings(violations, base_text, &diffs))
      << absl::StrCat("code:\n", test.code, "\nDiffs:\n", diffs.str(), "\n");
}

// Accepts an array of LintTestCases and tests them all on a linter containing
// rule generated by make_rule with a particular configuration.
template <class AnalyzerType, class RuleClass>
void RunConfiguredLintTestCases(
    std::initializer_list<LintTestCase> tests, absl::string_view configuration,
    absl::string_view filename = "<<inline-test>>") {
  typedef typename RuleClass::rule_type rule_type;
  auto rule_generator = [&configuration]() -> std::unique_ptr<rule_type> {
    std::unique_ptr<rule_type> instance(new RuleClass());
    absl::Status config_status = instance->Configure(configuration);
    CHECK(config_status.ok()) << config_status.message();
    return instance;
  };
  for (const auto& test : tests) {
    RunLintTestCase<AnalyzerType, rule_type>(test, rule_generator, filename);
  }
}

template <class AnalyzerType, class RuleClass>
void RunLintTestCases(std::initializer_list<LintTestCase> tests,
                      absl::string_view filename = "<<inline-test>>") {
  RunConfiguredLintTestCases<AnalyzerType, RuleClass>(tests, "", filename);
}
}  // namespace verible

#endif  // VERIBLE_COMMON_ANALYSIS_LINTER_TEST_UTILS_H_
