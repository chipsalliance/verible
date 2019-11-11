// Copyright 2017-2019 The Verible Authors.
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
#include "common/text/symbol.h"
#include "common/util/logging.h"
#include "verilog/CST/verilog_nonterminals.h"
#include "verilog/analysis/verilog_analyzer.h"
#include "verilog/analysis/verilog_excerpt_parse.h"

namespace verilog {
namespace {

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

}  // namespace
}  // namespace verilog
