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

#include "verilog/analysis/descriptions.h"

#include <utility>

#include "absl/strings/string_view.h"
#include "gtest/gtest.h"

namespace verilog {
namespace analysis {
namespace {

// Tests that Codify correctly returns the string with the appropriate code
// wrappers for markdown.
TEST(CodifyTest, MarkdownTests) {
  const std::pair<absl::string_view, absl::string_view> kTestCases[] = {
      {"foo", "`foo`"},
      {"`foo", "`` `foo``"},
      {"foo::bar*", "`foo::bar*`"},
  };
  for (const auto& test : kTestCases) {
    EXPECT_EQ(Codify(test.first, DescriptionType::kMarkdown), test.second);
  }
}

// Tests that Codify correctly returns the string wrapped with single quotes, to
// denote it is code in the CLI help text.
TEST(CodifyTest, HelpRulesFlagTests) {
  const std::pair<absl::string_view, absl::string_view> kTestCases[] = {
      {"foo", "\'foo\'"},
      {"`foo", "\'`foo\'"},
      {"foo::bar*", "\'foo::bar*\'"},
  };
  for (const auto& test : kTestCases) {
    EXPECT_EQ(Codify(test.first, DescriptionType::kHelpRulesFlag), test.second);
  }
}

}  // namespace
}  // namespace analysis
}  // namespace verilog
