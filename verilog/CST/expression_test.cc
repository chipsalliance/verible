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

#include "verilog/CST/expression.h"

#include <memory>
#include <utility>

#include "gtest/gtest.h"
#include "common/analysis/syntax_tree_search.h"
#include "common/analysis/syntax_tree_search_test_utils.h"
#include "common/text/symbol.h"
#include "common/text/tree_utils.h"
#include "common/util/logging.h"
#include "verilog/CST/match_test_utils.h"
#include "verilog/CST/verilog_nonterminals.h"
#include "verilog/CST/verilog_tree_print.h"
#include "verilog/analysis/verilog_analyzer.h"
#include "verilog/analysis/verilog_excerpt_parse.h"

namespace verilog {
namespace {

using verible::SyntaxTreeSearchTestCase;
using verible::TextStructureView;
using verible::TreeSearchMatch;

TEST(IsZeroTest, NonZero) {
  const char* kTestCases[] = {
      "a",
      "a0",
      "1",
      "-1",
      // no constant expression evaluation yet
      "1-1",
      "a-a",
      "0+0",
      "(0)",
  };
  for (auto code : kTestCases) {
    const auto analyzer_ptr = AnalyzeVerilogExpression(code, "<file>");
    const auto& node = ABSL_DIE_IF_NULL(analyzer_ptr)->SyntaxTree();
    const auto tag = node->Tag();
    EXPECT_EQ(tag.kind, verible::SymbolKind::kNode);
    EXPECT_EQ(NodeEnum(tag.tag), NodeEnum::kExpression);
    EXPECT_FALSE(IsZero(*node));
  }
}

TEST(IsZeroTest, Zero) {
  const char* kTestCases[] = {
      "0",
      "00",
      "00000",
      "'0",
  };
  for (auto code : kTestCases) {
    const auto analyzer_ptr = AnalyzeVerilogExpression(code, "<file>");
    const auto& node = ABSL_DIE_IF_NULL(analyzer_ptr)->SyntaxTree();
    const auto tag = node->Tag();
    EXPECT_EQ(tag.kind, verible::SymbolKind::kNode);
    EXPECT_EQ(NodeEnum(tag.tag), NodeEnum::kExpression);
    EXPECT_TRUE(IsZero(*node));
  }
}

TEST(ConstantIntegerValueTest, NotInteger) {
  const char* kTestCases[] = {
      "a",
      "1+1",
      "(2)",
  };
  for (auto code : kTestCases) {
    const auto analyzer_ptr = AnalyzeVerilogExpression(code, "<file>");
    const auto& node = ABSL_DIE_IF_NULL(analyzer_ptr)->SyntaxTree();
    const auto tag = node->Tag();
    EXPECT_EQ(tag.kind, verible::SymbolKind::kNode);
    EXPECT_EQ(NodeEnum(tag.tag), NodeEnum::kExpression);
    int value;
    EXPECT_FALSE(ConstantIntegerValue(*node, &value));
  }
}

TEST(ConstantIntegerValueTest, IsInteger) {
  const std::pair<const char*, int> kTestCases[] = {
      {"0", 0},
      {"1", 1},
      {"666", 666},
  };
  for (auto test : kTestCases) {
    const auto analyzer_ptr = AnalyzeVerilogExpression(test.first, "<file>");
    const auto& node = ABSL_DIE_IF_NULL(analyzer_ptr)->SyntaxTree();
    const auto tag = node->Tag();
    EXPECT_EQ(tag.kind, verible::SymbolKind::kNode);
    EXPECT_EQ(NodeEnum(tag.tag), NodeEnum::kExpression);
    int value;
    EXPECT_TRUE(ConstantIntegerValue(*node, &value));
    EXPECT_EQ(value, test.second);
  }
}

TEST(FindAllReferenceExpressionsTest, Various) {
  constexpr int kTag = 1;  // value doesn't matter
  const SyntaxTreeSearchTestCase kTestCases[] = {
      {""},
      {"module m; endmodule\n"},
      {"class   c;  endclass\n"},
      {"function  f;  endfunction\n"},
      {"function  f;\n", {kTag, "a"}, " = 1;\n", "endfunction\n"},
      {"function  f;\n",
       {kTag, "a"},
       " = ",
       {kTag, "z"},
       ";\n",
       "endfunction\n"},
      {"function  f;\n",
       {kTag, "a.b"},
       " = ",
       {kTag, "z.y.x"},
       ";\n",
       "endfunction\n"},
      {"task  t;\n",
       {kTag, "a[0][0]"},
       " = ",
       {kTag, "z[1][1]"},
       ";\n",
       "endtask\n"},
      {"module m;\n"
       "  foo bar(",
       {kTag, "v.w"},
       ");\n"
       "endmodule\n"},
      {"module m;\n"
       "  foo bar(.clk(",
       {kTag, "v.w"},
       "));\n"
       "endmodule\n"},
      {"module m;\n"  // continuous assignment
       "  assign ",
       {kTag, "v"},
       " = 1'b1;\n"
       "endmodule\n"},
      {"module m;\n"  // continuous assignment
       "  assign ",
       {kTag, "v"},
       " = ",
       {kTag, "w"},
       ";\n"
       "endmodule\n"},
      {"module m;\n"  // continuous assignment
       "  assign ",
       {kTag, "v[1]"},
       " = ",
       {kTag, "w.d"},
       ";\n"
       "endmodule\n"},
      {"module m;\n"  // blocking assignment
       "  initial ",
       {kTag, "v"},
       " = 1'b1;\n"
       "endmodule\n"},
      {"module m;\n"  // blocking assignment
       "  initial ",
       {kTag, "v"},
       " =  ",
       {kTag, "zz"},
       ";\n"
       "endmodule\n"},
      {"module m;\n"  // blocking assignment
       "  initial begin\n",
       {kTag, "v[2]"},
       " =  ",
       {kTag, "zz.f"},
       ";\n"
       "  end\n"
       "endmodule\n"},
      {"module m;\n"  // non blocking assignment
       "  always @(posedge ",
       {kTag, "clk"},
       ")\n",
       {kTag, "v.g"},
       " <=  ",
       {kTag, "zz[3][4]"},
       ";\n"
       "endmodule\n"},
      {"function  f;\n",
       "  for (int i = 0; ",
       {kTag, "i"},
       "<",
       {kTag, "N"},
       "; ++",
       {kTag, "i"},
       ")\n",
       // Note: "int i = 0" is a declaration, where "i" also serves as a
       // reference in an assignment, but is not tagged as such.
       {kTag, "a"},
       " = ",
       {kTag, "z"},
       " + ",
       {kTag, "i"},
       ";\n",
       "endfunction\n"},
      // reference could contain other references like "a[a]", but the testing
      // framework doesn't support nested expected ranges... yet.
  };
  for (const auto& test : kTestCases) {
    TestVerilogSyntaxRangeMatches(
        __FUNCTION__, test, [](const TextStructureView& text_structure) {
          const auto& root = text_structure.SyntaxTree();
          return FindAllReferenceFullExpressions(*ABSL_DIE_IF_NULL(root));
        });
  }
}

TEST(ReferenceIsSimpleTest, Simple) {
  const char* kTestCases[] = {
      "a",
      "bbb",
      "z1",
      "_y",
  };
  for (auto code : kTestCases) {
    const auto analyzer_ptr = AnalyzeVerilogExpression(code, "<file>");
    const auto& node = ABSL_DIE_IF_NULL(analyzer_ptr)->SyntaxTree();
    {
      const auto status = analyzer_ptr->LexStatus();
      ASSERT_TRUE(status.ok()) << status.message();
    }
    {
      const auto status = analyzer_ptr->ParseStatus();
      ASSERT_TRUE(status.ok()) << status.message();
    }
    const std::vector<TreeSearchMatch> refs(
        FindAllReferenceFullExpressions(*ABSL_DIE_IF_NULL(node)));
    for (const auto& ref : refs) {
      const verible::TokenInfo* token = ReferenceIsSimpleIdentifier(*ref.match);
      ASSERT_NE(token, nullptr) << "reference: " << code;
      EXPECT_EQ(token->text(), code);
    }
  }
}

TEST(ReferenceIsSimpleTest, NotSimple) {
  const char* kTestCases[] = {
      "a[0]", "a[4][6]", "bbb[3:0]", "x.y",  "x.y.z",  "x[0].y[1].z[2]",
      "x()",  "x.y()",   "x()[0]",   "x(1)", "f(0,1)", "j[9].k(3, 2, 1)",
  };
  for (auto code : kTestCases) {
    VLOG(1) << __FUNCTION__ << " test: " << code;
    const auto analyzer_ptr = AnalyzeVerilogExpression(code, "<file>");
    const auto& node = ABSL_DIE_IF_NULL(analyzer_ptr)->SyntaxTree();
    {
      const auto status = analyzer_ptr->LexStatus();
      ASSERT_TRUE(status.ok()) << status.message();
    }
    {
      const auto status = analyzer_ptr->ParseStatus();
      ASSERT_TRUE(status.ok()) << status.message();
    }
    const std::vector<TreeSearchMatch> refs(
        FindAllReferenceFullExpressions(*ABSL_DIE_IF_NULL(node)));
    for (const auto& ref : refs) {
      VLOG(1) << "match: " << verible::StringSpanOfSymbol(*ref.match);
      EXPECT_FALSE(ReferenceIsSimpleIdentifier(*ref.match))
          << "reference: " << code;
    }
  }
}

}  // namespace
}  // namespace verilog
