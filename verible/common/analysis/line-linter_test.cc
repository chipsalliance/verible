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

#include "verible/common/analysis/line-linter.h"

#include <cstddef>
#include <memory>
#include <set>
#include <vector>

#include "absl/strings/string_view.h"
#include "gmock/gmock.h"
#include "gtest/gtest.h"
#include "verible/common/analysis/line-lint-rule.h"
#include "verible/common/analysis/lint-rule-status.h"
#include "verible/common/text/token-info.h"

namespace verible {
namespace {

using testing::IsEmpty;
using testing::SizeIs;

// Example lint rule for the purposes of testing LineLinter.
// Blank lines are considered bad for demonstration purposes.
class BlankLineRule : public LineLintRule {
 public:
  BlankLineRule() = default;

  void HandleLine(absl::string_view line) final {
    if (line.empty()) {
      const TokenInfo token(0, line);
      violations_.insert(LintViolation(token, "some reason"));
    }
  }

  LintRuleStatus Report() const final { return LintRuleStatus(violations_); }

 private:
  std::set<LintViolation> violations_;
};

std::unique_ptr<LineLintRule> MakeBlankLineRule() {
  return std::unique_ptr<LineLintRule>(new BlankLineRule);
}

// This test verifies that LineLinter works with no rules.
TEST(LineLinterTest, NoRules) {
  std::vector<absl::string_view> lines;
  LineLinter linter;
  linter.Lint(lines);
  std::vector<LintRuleStatus> statuses = linter.ReportStatus();
  EXPECT_THAT(statuses, IsEmpty());
}

// This test verifies that LineLinter works with a single rule.
TEST(LineLinterTest, OneRuleAcceptsLines) {
  std::vector<absl::string_view> lines{"abc", "def"};
  LineLinter linter;
  linter.AddRule(MakeBlankLineRule());
  linter.Lint(lines);
  std::vector<LintRuleStatus> statuses = linter.ReportStatus();
  EXPECT_THAT(statuses, SizeIs(1));
  EXPECT_TRUE(statuses[0].isOk());
  EXPECT_THAT(statuses[0].violations, IsEmpty());
}

// This test verifies that LineLinter can find violations.
TEST(LineLinterTest, OneRuleRejectsLine) {
  std::vector<absl::string_view> lines{"abc", "", "def"};
  LineLinter linter;
  linter.AddRule(MakeBlankLineRule());
  linter.Lint(lines);
  std::vector<LintRuleStatus> statuses = linter.ReportStatus();
  EXPECT_THAT(statuses, SizeIs(1));
  EXPECT_FALSE(statuses[0].isOk());
  EXPECT_THAT(statuses[0].violations, SizeIs(1));
}

// Mock rule that rejects empty files.
class EmptyFileRule : public LineLintRule {
 public:
  EmptyFileRule() = default;

  void HandleLine(absl::string_view line) final { ++lines_; }

  void Finalize() final {
    if (lines_ == 0) {
      violations_.insert(
          LintViolation(TokenInfo::EOFToken(), "insufficient bytes"));
    }
  }

  LintRuleStatus Report() const final { return LintRuleStatus(violations_); }

  size_t lines_ = 0;

 private:
  std::set<LintViolation> violations_;
};

std::unique_ptr<LineLintRule> MakeEmptyFileRule() {
  return std::unique_ptr<LineLintRule>(new EmptyFileRule);
}

// This test verifies that LineLinter calls Finalize without error.
TEST(LineLinterTest, FinalizeAccepts) {
  std::vector<absl::string_view> lines{"x"};
  LineLinter linter;
  linter.AddRule(MakeEmptyFileRule());
  linter.Lint(lines);
  std::vector<LintRuleStatus> statuses = linter.ReportStatus();
  EXPECT_THAT(statuses, SizeIs(1));
  EXPECT_TRUE(statuses[0].isOk());
  EXPECT_THAT(statuses[0].violations, IsEmpty());
}

// This test verifies that LineLinter can report an error during Finalize.
TEST(LineLinterTest, FinalizeRejects) {
  std::vector<absl::string_view> lines;
  LineLinter linter;
  linter.AddRule(MakeEmptyFileRule());
  linter.Lint(lines);
  std::vector<LintRuleStatus> statuses = linter.ReportStatus();
  EXPECT_THAT(statuses, SizeIs(1));
  EXPECT_FALSE(statuses[0].isOk());
  EXPECT_THAT(statuses[0].violations, SizeIs(1));
}

}  // namespace
}  // namespace verible
