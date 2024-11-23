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

#include "verible/common/analysis/text-structure-linter.h"

#include <memory>
#include <set>
#include <vector>

#include "absl/strings/match.h"
#include "absl/strings/string_view.h"
#include "gmock/gmock.h"
#include "gtest/gtest.h"
#include "verible/common/analysis/lint-rule-status.h"
#include "verible/common/analysis/text-structure-lint-rule.h"
#include "verible/common/text/text-structure.h"
#include "verible/common/text/token-info.h"

namespace verible {
namespace {

using testing::IsEmpty;
using testing::SizeIs;

// Example lint rule that uses raw contents and lines array.
class RequireHelloRule : public TextStructureLintRule {
 public:
  RequireHelloRule() = default;

  void Lint(const TextStructureView &text_structure,
            absl::string_view filename) final {
    const auto &lines = text_structure.Lines();
    const absl::string_view contents = text_structure.Contents();
    if (!lines.empty() && !absl::StartsWith(contents, "Hello")) {
      const TokenInfo token(1, lines[0]);
      violations_.emplace(token, "Text must begin with Hello");
    }
  }

  LintRuleStatus Report() const final { return LintRuleStatus(violations_); }

 private:
  std::set<LintViolation> violations_;
};

std::unique_ptr<TextStructureLintRule> MakeHelloRule() {
  return std::unique_ptr<TextStructureLintRule>(new RequireHelloRule);
}

// This test verifies that TextStructureLinter works with no rules.
TEST(TextStructureLinterTest, NoRules) {
  const TextStructureView text_structure("Hello, world!\nGoodbye world.\n");
  TextStructureLinter linter;
  linter.Lint(text_structure, "");
  std::vector<LintRuleStatus> statuses = linter.ReportStatus();
  EXPECT_THAT(statuses, IsEmpty());
}

// This test verifies that TextStructureLinter works with a single rule.
TEST(TextStructureLinterTest, OneRuleAcceptsEmptyStream) {
  const TextStructureView text_structure("Hello, world!\nGoodbye world.\n");
  TextStructureLinter linter;
  linter.AddRule(MakeHelloRule());
  linter.Lint(text_structure, "");
  std::vector<LintRuleStatus> statuses = linter.ReportStatus();
  EXPECT_THAT(statuses, SizeIs(1));
  EXPECT_TRUE(statuses[0].isOk());
  EXPECT_THAT(statuses[0].violations, IsEmpty());
}

// This test verifies that TextStructureLinter can find violations.
TEST(TextStructureLinterTest, OneRuleRejectsTextStructure) {
  const TextStructureView text_structure("Goodbye cruel world.\n");
  TextStructureLinter linter;
  linter.AddRule(MakeHelloRule());
  linter.Lint(text_structure, "");
  std::vector<LintRuleStatus> statuses = linter.ReportStatus();
  EXPECT_THAT(statuses, SizeIs(1));
  EXPECT_FALSE(statuses[0].isOk());
  EXPECT_THAT(statuses[0].violations, SizeIs(1));
}

}  // namespace
}  // namespace verible
