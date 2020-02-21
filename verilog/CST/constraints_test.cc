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

#include "gmock/gmock.h"
#include "gtest/gtest.h"
#include "absl/strings/string_view.h"
#include "common/text/concrete_syntax_leaf.h"
#include "common/text/concrete_syntax_tree.h"
#include "common/text/symbol.h"
#include "common/text/text_structure.h"
#include "common/text/token_info.h"
#include "common/util/casts.h"
#include "common/util/logging.h"
#include "verilog/CST/verilog_nonterminals.h"
#include "verilog/analysis/verilog_analyzer.h"
#include "verilog/parser/verilog_token_enum.h"

#undef ASSERT_OK
#define ASSERT_OK(value) ASSERT_TRUE((value).ok())

namespace verilog {
namespace {

TEST(FindAllConstraintDeclarationsTest, BasicTests) {
  const std::pair<std::string, int> kTestCases[] = {
      {"module foo; logic a; endmodule", 0},
      {"class foo; rand logic a; endclass", 0},
      {"class foo; rand logic a; constraint Bar { a < 16; } endclass", 1},
      {"class foo; rand logic a; "
       "constraint b { a >= 16; }; "
       "constraint c { a <= 20; }; endclass",
       2},
      {"class foo; rand logic a; "
       "constraint b { a >= 16; }; "
       "constraint c { a <= 20; }; endclass; "
       "class bar; rand logic x; constraint y { x == 10; }; endclass",
       3},
  };
  for (const auto& test : kTestCases) {
    VerilogAnalyzer analyzer(test.first, "");
    ASSERT_OK(analyzer.Analyze());
    const auto& root = analyzer.Data().SyntaxTree();
    std::vector<verible::TreeSearchMatch> constraint_declarations =
        FindAllConstraintDeclarations(*root);

    auto constraints = FindAllConstraintDeclarations(*root);
    EXPECT_EQ(constraints.size(), test.second);
  }
}

TEST(IsOutOfLineConstraintDefinitionTest, BasicTests) {
  const std::pair<std::string, bool> kTestCases[] = {
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

    bool out_of_line =
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
    EXPECT_EQ(name_token.text, test.second);
  }
}

}  // namespace
}  // namespace verilog
