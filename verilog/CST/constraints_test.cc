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

#include "verilog/CST/constraints.h"

#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "absl/strings/string_view.h"
#include "common/analysis/syntax_tree_search_test_utils.h"
#include "common/text/concrete_syntax_leaf.h"
#include "common/text/concrete_syntax_tree.h"
#include "common/text/symbol.h"
#include "common/text/text_structure.h"
#include "common/text/token_info.h"
#include "common/util/casts.h"
#include "common/util/logging.h"
#include "gmock/gmock.h"
#include "gtest/gtest.h"
#include "verilog/CST/match_test_utils.h"
#include "verilog/CST/verilog_nonterminals.h"
#include "verilog/analysis/verilog_analyzer.h"
#include "verilog/parser/verilog_token_enum.h"

#undef ASSERT_OK
#define ASSERT_OK(value) ASSERT_TRUE((value).ok())

namespace verilog {
namespace {

using verible::SyntaxTreeSearchTestCase;
using verible::TextStructureView;

TEST(FindAllConstraintDeclarationsTest, BasicTests) {
  constexpr int kTag = 1;  // don't care
  const SyntaxTreeSearchTestCase kTestCases[] = {
      {"module foo; logic a; endmodule"},
      {"class foo; rand logic a; endclass"},
      {"class foo; rand logic a; ",
       {kTag, "constraint Bar { a < 16; }"},
       " endclass"},
      {"class foo; rand logic a; ",
       {kTag, "constraint b { a >= 16; }"},
       "; ",
       {kTag, "constraint c { a <= 20; }"},
       "; endclass"},
      {"class foo; rand logic a; ",
       {kTag, "constraint b { a >= 16; }"},
       "; ",
       {kTag, "constraint c { a <= 20; }"},
       "; endclass; "
       "class bar; rand logic x; ",
       {kTag, "constraint y { x == 10; }"},
       "; endclass"},
  };
  for (const auto& test : kTestCases) {
    TestVerilogSyntaxRangeMatches(
        __FUNCTION__, test, [](const TextStructureView& text_structure) {
          const auto& root = text_structure.SyntaxTree();
          return FindAllConstraintDeclarations(*root);
        });
  }
}

TEST(IsOutOfLineConstraintDefinitionTest, BasicTests) {
  constexpr std::pair<absl::string_view, bool> kTestCases[] = {
      {"class foo; rand logic a; constraint Bar { a < 16; } endclass", false},
      {"constraint classname::constraint_c { a <= b; }", true},
  };
  for (const auto& test : kTestCases) {
    VerilogAnalyzer analyzer(test.first, "");
    ASSERT_OK(analyzer.Analyze());
    const auto& root = analyzer.Data().SyntaxTree();

    std::vector<verible::TreeSearchMatch> constraint_declarations =
        FindAllConstraintDeclarations(*root);
    EXPECT_EQ(constraint_declarations.size(), 1);

    const bool out_of_line =
        IsOutOfLineConstraintDefinition(*constraint_declarations.front().match);
    EXPECT_EQ(out_of_line, test.second);
  }
}

// Tests that GetSymbolIdentifierFromConstraintDeclaration correctly returns
// the token of the symbol identifier.
TEST(GetSymbolIdentifierFromConstraintDeclarationTest, BasicTests) {
  const std::pair<std::string, absl::string_view> kTestCases[] = {
      {"class foo; rand logic a; constraint Bar { a < 16; } endclass", "Bar"},
      {"class foo; rand logic a; constraint b { a >= 16; } endclass", "b"},
      {"class foo; rand logic a; constraint stH { a == 16; } endclass", "stH"},
  };
  for (const auto& test : kTestCases) {
    VerilogAnalyzer analyzer(test.first, "");
    ASSERT_OK(analyzer.Analyze());
    const auto& root = analyzer.Data().SyntaxTree();
    std::vector<verible::TreeSearchMatch> constraint_declarations =
        FindAllConstraintDeclarations(*root);

    const auto name_token = GetSymbolIdentifierFromConstraintDeclaration(
        *constraint_declarations.front().match);
    EXPECT_EQ(name_token->text(), test.second);
  }
}

}  // namespace
}  // namespace verilog
