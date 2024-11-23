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

#include "verible/verilog/CST/expression.h"

#include <memory>
#include <utility>
#include <vector>

#include "gtest/gtest.h"
#include "verible/common/analysis/syntax-tree-search-test-utils.h"
#include "verible/common/analysis/syntax-tree-search.h"
#include "verible/common/text/concrete-syntax-tree.h"
#include "verible/common/text/symbol.h"
#include "verible/common/text/text-structure.h"
#include "verible/common/text/token-info.h"
#include "verible/common/text/tree-utils.h"
#include "verible/common/util/logging.h"
#include "verible/verilog/CST/match-test-utils.h"
#include "verible/verilog/CST/verilog-matchers.h"
#include "verible/verilog/CST/verilog-nonterminals.h"
#include "verible/verilog/analysis/verilog-analyzer.h"
#include "verible/verilog/analysis/verilog-excerpt-parse.h"
#include "verible/verilog/preprocessor/verilog-preprocess.h"

namespace verilog {
namespace {

static constexpr VerilogPreprocess::Config kDefaultPreprocess;

using verible::SyntaxTreeSearchTestCase;
using verible::TextStructureView;
using verible::TreeSearchMatch;

TEST(IsZeroTest, NonZero) {
  const char *kTestCases[] = {
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
    const auto analyzer_ptr =
        AnalyzeVerilogExpression(code, "<file>", kDefaultPreprocess);
    const auto &node = ABSL_DIE_IF_NULL(analyzer_ptr)->SyntaxTree();
    const auto tag = node->Tag();
    EXPECT_EQ(tag.kind, verible::SymbolKind::kNode);
    EXPECT_EQ(NodeEnum(tag.tag), NodeEnum::kExpression);
    EXPECT_FALSE(IsZero(*node));
  }
}

TEST(IsZeroTest, Zero) {
  const char *kTestCases[] = {
      "0",
      "00",
      "00000",
      "'0",
  };
  for (auto code : kTestCases) {
    const auto analyzer_ptr =
        AnalyzeVerilogExpression(code, "<file>", kDefaultPreprocess);
    const auto &node = ABSL_DIE_IF_NULL(analyzer_ptr)->SyntaxTree();
    const auto tag = node->Tag();
    EXPECT_EQ(tag.kind, verible::SymbolKind::kNode);
    EXPECT_EQ(NodeEnum(tag.tag), NodeEnum::kExpression);
    EXPECT_TRUE(IsZero(*node));
  }
}

TEST(ConstantIntegerValueTest, NotInteger) {
  const char *kTestCases[] = {
      "a",
      "1+1",
      "(2)",
  };
  for (auto code : kTestCases) {
    const auto analyzer_ptr =
        AnalyzeVerilogExpression(code, "<file>", kDefaultPreprocess);
    const auto &node = ABSL_DIE_IF_NULL(analyzer_ptr)->SyntaxTree();
    const auto tag = node->Tag();
    EXPECT_EQ(tag.kind, verible::SymbolKind::kNode);
    EXPECT_EQ(NodeEnum(tag.tag), NodeEnum::kExpression);
    int value;
    EXPECT_FALSE(ConstantIntegerValue(*node, &value));
  }
}

TEST(ConstantIntegerValueTest, IsInteger) {
  const std::pair<const char *, int> kTestCases[] = {
      {"0", 0},
      {"1", 1},
      {"666", 666},
  };
  for (auto test : kTestCases) {
    const auto analyzer_ptr =
        AnalyzeVerilogExpression(test.first, "<file>", kDefaultPreprocess);
    const auto &node = ABSL_DIE_IF_NULL(analyzer_ptr)->SyntaxTree();
    const auto tag = node->Tag();
    EXPECT_EQ(tag.kind, verible::SymbolKind::kNode);
    EXPECT_EQ(NodeEnum(tag.tag), NodeEnum::kExpression);
    int value;
    EXPECT_TRUE(ConstantIntegerValue(*node, &value));
    EXPECT_EQ(value, test.second);
  }
}

TEST(AssociativeBinaryExpressionsTest, FlatTree) {
  constexpr int kTag = 1;  // value doesn't matter
  const SyntaxTreeSearchTestCase kTestCases[] = {
      {""},
      {"module m; endmodule\n"},
      {"module m;\ninitial begin end\nendmodule"},
      {"class   c;  endclass\n"},
      {"function  f;  endfunction\n"},
      {"function  f;\n", "a = ", {kTag, "b + c"}, "; endfunction\n"},
      {"function  f;\n", "a = ", {kTag, "b + c + d"}, "; endfunction\n"},
      {"parameter product P = ", {kTag, "q*r"}, ";\n"},
      {"parameter product P = ", {kTag, "q*r*s"}, ";\n"},
      {"function  f;\n", "a = ", {kTag, "b | c | d | e"}, "; endfunction\n"},
      {"function  f;\n", "a = ", {kTag, "b & c & d & e"}, "; endfunction\n"},
      {"function  f;\n", "a = ", {kTag, "b || c || d || e"}, "; endfunction\n"},
      {"function  f;\n", "a = ", {kTag, "b && c && d && e"}, "; endfunction\n"},
      {"function  f;\n", "a = ", {kTag, "b ^ c ^ d ^ e"}, "; endfunction\n"},
      {"function  f;\n", "a = ", {kTag, "b ~^ c ~^ d ~^ e"}, "; endfunction\n"},
  };
  for (const auto &test : kTestCases) {
    TestVerilogSyntaxRangeMatches(
        __FUNCTION__, test, [](const TextStructureView &text_structure) {
          const auto &root = text_structure.SyntaxTree();
          return FindAllBinaryOperations(*ABSL_DIE_IF_NULL(root));
        });
  }
}

TEST(AssociativeBinaryExpressionsTest, ThreeFlatOperands) {
  constexpr int kTag = 1;  // value doesn't matter
  const SyntaxTreeSearchTestCase kTestCases[] = {
      {"function  f;\n", "a = ", {kTag, "b | c | d"}, "; endfunction\n"},
      {"function  f;\n",
       "a = ",
       {kTag, "b(x) | c(y) | d(z)"},
       "; endfunction\n"},
      {"function  f;\n",
       "a = ",
       {kTag, "b(x) ^ c(y) ^ d(z)"},
       "; endfunction\n"},
      {"function  f;\n",
       "a = ",
       {kTag, "b(x) + c(y) + d(z)"},
       "; endfunction\n"},
      {"function  f;\n", "a = g(", {kTag, "b | c | d"}, "); endfunction\n"},
      {"function  f;\n",
       "a = g(",
       {kTag, "b|c|d"},
       ", ",
       {kTag, "x*y*z"},
       "); endfunction\n"},
  };
  for (const auto &test : kTestCases) {
    TestVerilogSyntaxRangeMatches(
        __FUNCTION__, test, [](const TextStructureView &text_structure) {
          const auto &root = text_structure.SyntaxTree();
          auto matches = FindAllBinaryOperations(*ABSL_DIE_IF_NULL(root));
          for (const auto &match : matches) {
            // "A op B op C" is 5 sibling tokens, due to flattening
            EXPECT_EQ(verible::SymbolCastToNode(*ABSL_DIE_IF_NULL(match.match))
                          .size(),
                      5);
          }
          return matches;
        });
  }
}

TEST(GetConditionExpressionPredicateTest, Various) {
  constexpr int kTag = 1;  // value doesn't matter
  const SyntaxTreeSearchTestCase kTestCases[] = {
      {""},
      {"module m; endmodule\n"},
      {"module m;\ninitial begin end\nendmodule"},
      {"class   c;  endclass\n"},
      {"function  f;  endfunction\n"},
      {"function  f;\n", "a = ", {kTag, "b"}, " ? c : d", "; endfunction\n"},
      {"module m;\n",
       "  assign foo = ",
       {kTag, "condition_a"},
       " ? b : c",
       ";\n",
       "endmodule\n"},
      {"module m;\n",
       "parameter foo = ",
       {kTag, "condition_a"},
       "? a : b",
       ";\nendmodule"},
      {"module m;\n",
       "always @(posedge clk) begin\n left <= ",
       {kTag, "condition_a"},
       "? a : b",
       "; \nend",
       "\nendmodule"},
      {"function f;\n g = h(",
       {kTag, "condition_a"},
       "? a : b",
       "); \nendfunction"},
      {"module m;\n",
       "always @(posedge clk)\n",
       "case (",
       {kTag, "condition_a"},
       "? a : b",
       ")\n",
       "default :;\n",
       "endcase\n",
       "\nendmodule"},
  };
  for (const auto &test : kTestCases) {
    TestVerilogSyntaxRangeMatches(
        __FUNCTION__, test, [](const TextStructureView &text_structure) {
          const auto &root = text_structure.SyntaxTree();
          const auto exprs =
              FindAllConditionExpressions(*ABSL_DIE_IF_NULL(root));

          std::vector<TreeSearchMatch> predicates;
          for (const auto &expr : exprs) {
            const auto *predicate =
                GetConditionExpressionPredicate(*expr.match);
            if (predicate != nullptr) {
              predicates.push_back(
                  TreeSearchMatch{predicate, {/* ignored context */}});
            } else {
              EXPECT_NE(predicate, nullptr)
                  << "predicate:\n"
                  << verible::RawTreePrinter(*expr.match);
            }
          }
          return predicates;
        });
  }
}

TEST(GetConditionExpressionTrueCaseTest, Various) {
  constexpr int kTag = 1;  // value doesn't matter
  const SyntaxTreeSearchTestCase kTestCases[] = {
      {""},
      {"module m; endmodule\n"},
      {"module m;\ninitial begin end\nendmodule"},
      {"class   c;  endclass\n"},
      {"function  f;  endfunction\n"},
      {"function  f;\n",
       "a = ",
       "b ? ",
       {kTag, "c"},
       " : d",
       "; endfunction\n"},
      {"function  f;\n",
       "a = ",
       "b ? ",
       {kTag, "(c + d)"},
       " : d",
       "; endfunction\n"},
      {"function  f;\n",
       "a = ",
       "b ? ",
       {kTag, "(c << d)"},
       " : d",
       "; endfunction\n"},
      {"module m;\n",
       "  assign foo = ",
       "condition_a ? ",
       {kTag, "b"},
       " : c",
       ";\n",
       "endmodule\n"},
      {"module m;\n",
       "parameter foo = ",
       "condition_a ? ",
       {kTag, "a"},
       " : b",
       ";\nendmodule"},
      {"module m;\n",
       "always @(posedge clk) begin\n left <= ",
       "condition_a ? ",
       {kTag, "a"},
       " : b",
       "; \nend",
       "\nendmodule"},
      {"function f;\n g = h(",
       "condition_a ? ",
       {kTag, "a"},
       " : b",
       "); \nendfunction"},
      {"module m;\n",
       "always @(posedge clk)\n",
       "case (",
       "condition_a ? ",
       {kTag, "a"},
       " : b",
       ")\n",
       "default :;\n",
       "endcase\n",
       "\nendmodule"},
  };
  for (const auto &test : kTestCases) {
    TestVerilogSyntaxRangeMatches(
        __FUNCTION__, test, [](const TextStructureView &text_structure) {
          const auto &root = text_structure.SyntaxTree();
          const auto exprs =
              FindAllConditionExpressions(*ABSL_DIE_IF_NULL(root));

          std::vector<TreeSearchMatch> predicates;
          for (const auto &expr : exprs) {
            const auto *predicate = GetConditionExpressionTrueCase(*expr.match);
            if (predicate != nullptr) {
              predicates.push_back(
                  TreeSearchMatch{predicate, {/* ignored context */}});
            } else {
              EXPECT_NE(predicate, nullptr)
                  << "predicate:\n"
                  << verible::RawTreePrinter(*expr.match);
            }
          }
          return predicates;
        });
  }
}

TEST(GetConditionExpressionFalseCaseTest, Various) {
  constexpr int kTag = 1;  // value doesn't matter
  const SyntaxTreeSearchTestCase kTestCases[] = {
      {""},
      {"module m; endmodule\n"},
      {"module m;\ninitial begin end\nendmodule"},
      {"class   c;  endclass\n"},
      {"function  f;  endfunction\n"},
      {"function  f;\n",
       "a = ",
       "b ? ",
       "c : ",
       {kTag, "d"},
       "; endfunction\n"},
      {"function  f;\n",
       "a = ",
       "b ? ",
       "c : ",
       {kTag, "(c + d)"},
       "; endfunction\n"},
      {"function  f;\n",
       "a = ",
       "b ? ",
       "c : ",
       {kTag, "(c << d)"},
       "; endfunction\n"},
      {"module m;\n",
       "  assign foo = ",
       "condition_a ? ",
       "b : ",
       {kTag, "c"},
       ";\n",
       "endmodule\n"},
      {"module m;\n",
       "parameter foo = ",
       "condition_a ? ",
       "a : ",
       {kTag, "b"},
       ";\nendmodule"},
      {"module m;\n",
       "always @(posedge clk) begin\n left <= ",
       "condition_a ? ",
       "a : ",
       {kTag, "b"},
       "; \nend",
       "\nendmodule"},
      {"function f;\n g = h(",
       "condition_a ? ",
       "a : ",
       {kTag, "b"},
       "); \nendfunction"},
      {"module m;\n",
       "always @(posedge clk)\n",
       "case (",
       "condition_a ? ",
       "a : ",
       {kTag, "b"},
       ")\n",
       "default :;\n",
       "endcase\n",
       "\nendmodule"},
  };
  for (const auto &test : kTestCases) {
    TestVerilogSyntaxRangeMatches(
        __FUNCTION__, test, [](const TextStructureView &text_structure) {
          const auto &root = text_structure.SyntaxTree();
          const auto exprs =
              FindAllConditionExpressions(*ABSL_DIE_IF_NULL(root));

          std::vector<TreeSearchMatch> predicates;
          for (const auto &expr : exprs) {
            const auto *predicate =
                GetConditionExpressionFalseCase(*expr.match);
            if (predicate != nullptr) {
              predicates.push_back(
                  TreeSearchMatch{predicate, {/* ignored context */}});
            } else {
              EXPECT_NE(predicate, nullptr)
                  << "predicate:\n"
                  << verible::RawTreePrinter(*expr.match);
            }
          }
          return predicates;
        });
  }
}

TEST(GetUnaryPrefixOperator, Exprs) {
  const std::pair<const char *, const char *> kTestCases[] = {
      {"-(2)", "-"},    {"-1", "-"},        {"&1", "&"},
      {"666", nullptr}, {"1 + 2", nullptr}, {"!1", "!"},
  };
  for (auto test : kTestCases) {
    const auto analyzer_ptr =
        AnalyzeVerilogExpression(test.first, "<file>", kDefaultPreprocess);
    const auto &node = ABSL_DIE_IF_NULL(analyzer_ptr)->SyntaxTree();
    const auto tag = node->Tag();
    EXPECT_EQ(tag.kind, verible::SymbolKind::kNode);
    EXPECT_EQ(NodeEnum(tag.tag), NodeEnum::kExpression);
    const verible::Symbol *last_node = DescendThroughSingletons(*node);

    if (test.second) {
      const verible::SyntaxTreeNode &unary_expr =
          verible::SymbolCastToNode(*last_node);
      EXPECT_EQ(NodeEnum(unary_expr.Tag().tag),
                NodeEnum::kUnaryPrefixExpression);
      EXPECT_EQ(test.second, GetUnaryPrefixOperator(unary_expr)->text());
    } else {
      EXPECT_NE(NodeEnum(last_node->Tag().tag),
                NodeEnum::kUnaryPrefixExpression);
      EXPECT_FALSE(GetUnaryPrefixOperand(*last_node));
    }
  }
}

TEST(GetUnaryPrefixOperand, Exprs) {
  const std::pair<const char *, const char *> kTestCases[] = {
      {"-1", ""}, {"&1", ""}, {"666", nullptr}, {"1 + 2", nullptr}, {"!1", ""},
  };
  for (auto test : kTestCases) {
    const auto analyzer_ptr =
        AnalyzeVerilogExpression(test.first, "<file>", kDefaultPreprocess);
    const auto &node = ABSL_DIE_IF_NULL(analyzer_ptr)->SyntaxTree();
    const auto tag = node->Tag();
    EXPECT_EQ(tag.kind, verible::SymbolKind::kNode);
    EXPECT_EQ(NodeEnum(tag.tag), NodeEnum::kExpression);
    const verible::Symbol *last_node = DescendThroughSingletons(*node);

    if (test.second) {
      const verible::SyntaxTreeNode &unary_expr =
          verible::SymbolCastToNode(*last_node);
      EXPECT_EQ(NodeEnum(unary_expr.Tag().tag),
                NodeEnum::kUnaryPrefixExpression);
      EXPECT_TRUE(GetUnaryPrefixOperand(unary_expr));
    } else {
      EXPECT_NE(NodeEnum(last_node->Tag().tag),
                NodeEnum::kUnaryPrefixExpression);
      EXPECT_FALSE(GetUnaryPrefixOperand(*last_node));
    }
  }
}

TEST(FindAllConditionExpressionsTest, Various) {
  constexpr int kTag = 1;  // value doesn't matter
  const SyntaxTreeSearchTestCase kTestCases[] = {
      {""},
      {"module m; endmodule\n"},
      {"module m;\ninitial begin end\nendmodule"},
      {"class   c;  endclass\n"},
      {"function  f;  endfunction\n"},
      {"function  f;\n", "a = ", {kTag, "b ? c : d"}, "; endfunction\n"},
      {"module m;\n",
       "  assign foo = ",
       {kTag, "condition_a ? b : c"},
       ";\n",
       "endmodule\n"},
      {"module m;\n",
       "parameter foo = ",
       {kTag, "condition_a? a : b"},
       ";\nendmodule"},
      {"module m;\n",
       "always @(posedge clk) begin\n left <= ",
       {kTag, "condition_a? a : b"},
       "; \nend",
       "\nendmodule"},
      {"function f;\n g = h(",
       {kTag, "condition_a? a : b"},
       "); \nendfunction"},
      {"module m;\n",
       "always @(posedge clk)\n",
       "case (",
       {kTag, "condition_a? a : b"},
       ")\n",
       "default :;\n",
       "endcase\n",
       "\nendmodule"},
  };
  for (const auto &test : kTestCases) {
    TestVerilogSyntaxRangeMatches(
        __FUNCTION__, test, [](const TextStructureView &text_structure) {
          const auto &root = text_structure.SyntaxTree();
          return FindAllConditionExpressions(*ABSL_DIE_IF_NULL(root));
        });
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
  for (const auto &test : kTestCases) {
    TestVerilogSyntaxRangeMatches(
        __FUNCTION__, test, [](const TextStructureView &text_structure) {
          const auto &root = text_structure.SyntaxTree();
          return FindAllReferenceFullExpressions(*ABSL_DIE_IF_NULL(root));
        });
  }
}

TEST(ReferenceIsSimpleTest, Simple) {
  const char *kTestCases[] = {
      "a",
      "bbb",
      "z1",
      "_y",
  };
  for (auto code : kTestCases) {
    const auto analyzer_ptr =
        AnalyzeVerilogExpression(code, "<file>", kDefaultPreprocess);
    const auto &node = ABSL_DIE_IF_NULL(analyzer_ptr)->SyntaxTree();
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
    for (const auto &ref : refs) {
      const verible::TokenInfo *token = ReferenceIsSimpleIdentifier(*ref.match);
      ASSERT_NE(token, nullptr) << "reference: " << code;
      EXPECT_EQ(token->text(), code);
    }
  }
}

TEST(ReferenceIsSimpleTest, NotSimple) {
  const char *kTestCases[] = {
      "a[0]", "a[4][6]", "bbb[3:0]", "x.y",  "x.y.z",  "x[0].y[1].z[2]",
      "x()",  "x.y()",   "x()[0]",   "x(1)", "f(0,1)", "j[9].k(3, 2, 1)",
  };
  for (auto code : kTestCases) {
    VLOG(1) << __FUNCTION__ << " test: " << code;
    const auto analyzer_ptr =
        AnalyzeVerilogExpression(code, "<file>", kDefaultPreprocess);
    const auto &node = ABSL_DIE_IF_NULL(analyzer_ptr)->SyntaxTree();
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
    for (const auto &ref : refs) {
      VLOG(1) << "match: " << verible::StringSpanOfSymbol(*ref.match);
      EXPECT_FALSE(ReferenceIsSimpleIdentifier(*ref.match))
          << "reference: " << code;
    }
  }
}

TEST(GetIncrementDecrementOperatorTest, Various) {
  constexpr int kTag = 1;  // value doesn't matter
  const SyntaxTreeSearchTestCase kTestCases[] = {
      {""},
      {"module m; endmodule\n"},
      {"module m;\ninitial begin end\nendmodule"},
      {"module m;\nalways_comb begin\n"
       "a",
       {kTag, "++"},
       ";\nend\nendmodule"},
      {"module m;\nalways_comb begin\n",
       {kTag, "++"},
       "a;"
       "\nend\nendmodule"},
      {"module m;\nalways_comb begin\n"
       "somelargename",
       {kTag, "++"},
       ";\nend\nendmodule"},
      {"module m;\nalways_comb begin\n",
       {kTag, "++"},
       "somelargename;\n"
       "end\nendmodule"},
      {"module m;\nalways_comb begin\nk = a + 2;\nend\nendmodule"},
  };
  for (const auto &test : kTestCases) {
    TestVerilogSyntaxRangeMatches(
        __FUNCTION__, test, [](const TextStructureView &text_structure) {
          const auto &root = text_structure.SyntaxTree();
          const auto exprs = verible::SearchSyntaxTree(
              *ABSL_DIE_IF_NULL(root), NodekIncrementDecrementExpression());

          std::vector<TreeSearchMatch> operators;
          for (const auto &expr : exprs) {
            const auto *operator_ = GetIncrementDecrementOperator(*expr.match);
            operators.push_back(
                TreeSearchMatch{operator_, {/* ignored context */}});
          }
          return operators;
        });
  }
}

TEST(GetIncrementDecrementOperandTest, Various) {
  constexpr int kTag = 1;  // value doesn't matter
  const SyntaxTreeSearchTestCase kTestCases[] = {
      {""},
      {"module m; endmodule\n"},
      {"module m;\ninitial begin end\nendmodule"},
      {"module m;\n"
       "always_comb begin\n",
       {kTag, "a"},
       "++;\nend\nendmodule"},
      {"module m;\n"
       "always_comb begin\n"
       "++",
       {kTag, "a"},
       ";\nend\nendmodule"},
      {"module m;\n"
       "always_comb begin\n",
       {kTag, "somelargename"},
       "++;\nend\nendmodule"},
      {"module m;\n"
       "always_comb begin\n++",
       {kTag, "somelargename"},
       ";\nend\nendmodule"},
      {"module m;\n"
       "always_comb begin\n"
       "k = a + 2;\n"
       "end\nendmodule"},
  };
  for (const auto &test : kTestCases) {
    TestVerilogSyntaxRangeMatches(
        __FUNCTION__, test, [](const TextStructureView &text_structure) {
          const auto &root = text_structure.SyntaxTree();
          const auto exprs = verible::SearchSyntaxTree(
              *ABSL_DIE_IF_NULL(root), NodekIncrementDecrementExpression());

          std::vector<TreeSearchMatch> operands;
          for (const auto &expr : exprs) {
            const auto *operand = GetIncrementDecrementOperand(*expr.match);
            operands.push_back(
                TreeSearchMatch{operand, {/* ignored context */}});
          }
          return operands;
        });
  }
}

}  // namespace
}  // namespace verilog
