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

#include "verible/common/analysis/token-stream-linter.h"

#include <memory>
#include <set>
#include <vector>

#include "absl/strings/string_view.h"
#include "gmock/gmock.h"
#include "gtest/gtest.h"
#include "verible/common/analysis/lint-rule-status.h"
#include "verible/common/analysis/token-stream-lint-rule.h"
#include "verible/common/text/token-info.h"
#include "verible/common/text/token-stream-view.h"

namespace verible {
namespace {

using testing::IsEmpty;
using testing::SizeIs;

// Example lint rule for the purposes of testing TokenStreamLinter.
class ForbidTokenRule : public TokenStreamLintRule {
 public:
  explicit ForbidTokenRule(int n) : target_(n) {}

  void HandleToken(const TokenInfo &token) final {
    if (token.token_enum() == target_) {
      violations_.insert(LintViolation(token, "some reason"));
    }
  }

  LintRuleStatus Report() const final { return LintRuleStatus(violations_); }

 private:
  std::set<LintViolation> violations_;
  int target_;
};

std::unique_ptr<TokenStreamLintRule> MakeRuleN(int n) {
  return std::unique_ptr<TokenStreamLintRule>(new ForbidTokenRule(n));
}

// This test verifies that TokenStreamLinter works with no rules.
TEST(TokenStreamLinterTest, NoRules) {
  const TokenSequence tokens = {TokenInfo::EOFToken()};  // EOF token only
  TokenStreamLinter linter;
  linter.Lint(tokens);
  std::vector<LintRuleStatus> statuses = linter.ReportStatus();
  EXPECT_THAT(statuses, IsEmpty());
}

// This test verifies that TokenStreamLinter works with a single rule.
TEST(TokenStreamLinterTest, OneRuleAcceptsEmptyStream) {
  const TokenSequence tokens = {TokenInfo::EOFToken()};  // EOF token only
  TokenStreamLinter linter;
  linter.AddRule(MakeRuleN(4));
  linter.Lint(tokens);
  std::vector<LintRuleStatus> statuses = linter.ReportStatus();
  EXPECT_THAT(statuses, SizeIs(1));
  EXPECT_TRUE(statuses[0].isOk());
  EXPECT_THAT(statuses[0].violations, IsEmpty());
}

// This test verifies that TokenStreamLinter can find violations.
TEST(TokenStreamLinterTest, OneRuleRejectsTokenStream) {
  const absl::string_view text;
  const TokenSequence tokens = {TokenInfo(1, text), TokenInfo(4, text),
                                TokenInfo(2, text),
                                TokenInfo::EOFToken()};  // EOF token only
  TokenStreamLinter linter;
  linter.AddRule(MakeRuleN(4));
  linter.Lint(tokens);
  std::vector<LintRuleStatus> statuses = linter.ReportStatus();
  EXPECT_THAT(statuses, SizeIs(1));
  EXPECT_FALSE(statuses[0].isOk());
  EXPECT_THAT(statuses[0].violations, SizeIs(1));
}

}  // namespace
}  // namespace verible
