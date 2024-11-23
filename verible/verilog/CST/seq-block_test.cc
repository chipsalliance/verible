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

#include "verible/verilog/CST/seq-block.h"

#include <vector>

#include "gtest/gtest.h"
#include "verible/common/analysis/syntax-tree-search.h"
#include "verible/common/text/symbol.h"
#include "verible/common/text/text-structure.h"
#include "verible/common/util/logging.h"
#include "verible/verilog/CST/verilog-matchers.h"
#include "verible/verilog/CST/verilog-nonterminals.h"
#include "verible/verilog/analysis/verilog-analyzer.h"

#undef ASSERT_OK
#define ASSERT_OK(value) ASSERT_TRUE((value).ok())

namespace verilog {
namespace {

static std::vector<verible::TreeSearchMatch> FindAllBeginStatements(
    const verible::Symbol &root) {
  return SearchSyntaxTree(root, NodekBegin());
}

TEST(GetMatchingEndTest, Simple) {
  VerilogAnalyzer analyzer(R"(
module foo;
initial begin
end
endmodule)",
                           "");

  ASSERT_OK(analyzer.Analyze());
  const auto &root = analyzer.Data().SyntaxTree();

  const auto begin_statements = FindAllBeginStatements(*root);
  ASSERT_EQ(begin_statements.size(), 1);

  const auto matchingEnd =
      GetMatchingEnd(*begin_statements[0].match, begin_statements[0].context);
  EXPECT_EQ(NodeEnum(matchingEnd->Tag().tag), NodeEnum::kEnd);
}

TEST(GetBeginLabelTokenInfoTest, Single) {
  VerilogAnalyzer analyzer(R"(
module foo;
initial begin : begin_label
end
endmodule)",
                           "");

  ASSERT_OK(analyzer.Analyze());
  const auto &root = analyzer.Data().SyntaxTree();

  const auto begin_statements = FindAllBeginStatements(*root);
  ASSERT_EQ(begin_statements.size(), 1);

  const auto *beginToken = GetBeginLabelTokenInfo(*begin_statements[0].match);
  EXPECT_EQ(ABSL_DIE_IF_NULL(beginToken)->text(), "begin_label");
}

TEST(GetBeginLabelTokenInfoTest, SingleNoLabel) {
  VerilogAnalyzer analyzer(R"(
module foo;
initial begin
end : begin_has_no_label
endmodule)",
                           "");

  ASSERT_OK(analyzer.Analyze());
  const auto &root = analyzer.Data().SyntaxTree();

  const auto begin_statements = FindAllBeginStatements(*root);
  ASSERT_EQ(begin_statements.size(), 1);

  const auto *beginToken = GetBeginLabelTokenInfo(*begin_statements[0].match);
  EXPECT_EQ(beginToken, nullptr);
}

TEST(GetBeginLabelTokenInfoTest, GenerateBlockPrefixLabel) {
  VerilogAnalyzer analyzer(R"(
module foo;
if (1) prefix_label : begin
end
endmodule)",
                           "");

  ASSERT_OK(analyzer.Analyze());
  const auto &root = analyzer.Data().SyntaxTree();

  const auto begin_statements = FindAllBeginStatements(*root);
  ASSERT_EQ(begin_statements.size(), 1);

  const auto *beginToken = GetBeginLabelTokenInfo(*begin_statements[0].match);
  EXPECT_EQ(ABSL_DIE_IF_NULL(beginToken)->text(), "prefix_label");
}

TEST(GetEndLabelTokenInfoTest, SingleNoLabel) {
  VerilogAnalyzer analyzer(R"(
module foo;
initial begin : end_has_no_label
end
endmodule)",
                           "");

  ASSERT_OK(analyzer.Analyze());
  const auto &root = analyzer.Data().SyntaxTree();
  const auto begin_statements = FindAllBeginStatements(*root);

  ASSERT_EQ(begin_statements.size(), 1);

  const auto *matchingEnd =
      GetMatchingEnd(*begin_statements[0].match, begin_statements[0].context);
  ASSERT_EQ(NodeEnum(matchingEnd->Tag().tag), NodeEnum::kEnd);

  const auto *endToken = GetEndLabelTokenInfo(*matchingEnd);
  EXPECT_EQ(endToken, nullptr);
}

TEST(GetEndLabelTokenInfoTest, Single) {
  VerilogAnalyzer analyzer(R"(
module foo;
initial begin
end : end_label
endmodule)",
                           "");

  ASSERT_OK(analyzer.Analyze());
  const auto &root = analyzer.Data().SyntaxTree();

  const auto begin_statements = FindAllBeginStatements(*root);
  ASSERT_EQ(begin_statements.size(), 1);

  const auto *matchingEnd =
      GetMatchingEnd(*begin_statements[0].match, begin_statements[0].context);
  ASSERT_EQ(NodeEnum(matchingEnd->Tag().tag), NodeEnum::kEnd);

  const auto *endToken = GetEndLabelTokenInfo(*matchingEnd);
  EXPECT_EQ(ABSL_DIE_IF_NULL(endToken)->text(), "end_label");
}

TEST(GetMatchingEndTest, Complex) {
  VerilogAnalyzer analyzer(R"(
module foo;
initial begin : outer_begin_label
for(int i = 0; i < 5; ++i)
begin : inner_begin_label
end :  inner_end_label
end : outer_end_label
endmodule)",
                           "");

  ASSERT_OK(analyzer.Analyze());
  const auto &root = analyzer.Data().SyntaxTree();

  const auto begin_statements = FindAllBeginStatements(*root);
  ASSERT_EQ(begin_statements.size(), 2);

  const auto *matchingOuterEnd =
      GetMatchingEnd(*begin_statements[0].match, begin_statements[0].context);
  ASSERT_EQ(NodeEnum(matchingOuterEnd->Tag().tag), NodeEnum::kEnd);

  const auto *matchingInnerEnd =
      GetMatchingEnd(*begin_statements[1].match, begin_statements[1].context);
  ASSERT_EQ(NodeEnum(matchingInnerEnd->Tag().tag), NodeEnum::kEnd);

  const auto &outerBeginToken =
      *ABSL_DIE_IF_NULL(GetBeginLabelTokenInfo(*begin_statements[0].match));

  const auto &innerBeginToken =
      *ABSL_DIE_IF_NULL(GetBeginLabelTokenInfo(*begin_statements[1].match));

  const auto &outerEndToken =
      *ABSL_DIE_IF_NULL(GetEndLabelTokenInfo(*matchingOuterEnd));

  const auto &innerEndToken =
      *ABSL_DIE_IF_NULL(GetEndLabelTokenInfo(*matchingInnerEnd));

  EXPECT_EQ(outerBeginToken.text(), "outer_begin_label");
  EXPECT_EQ(outerEndToken.text(), "outer_end_label");
  EXPECT_EQ(innerBeginToken.text(), "inner_begin_label");
  EXPECT_EQ(innerEndToken.text(), "inner_end_label");
}

}  // namespace
}  // namespace verilog
