// Copyright 2017-2022 The Verible Authors.
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

#include "verible/verilog/analysis/flow-tree.h"

#include <vector>

#include "absl/status/status.h"
#include "absl/strings/string_view.h"
#include "gmock/gmock.h"
#include "gtest/gtest.h"
#include "verible/common/text/token-stream-view.h"
#include "verible/verilog/parser/verilog-lexer.h"

namespace verilog {
namespace {

using testing::StartsWith;

// Lexes a SystemVerilog source code, and returns a TokenSequence.
verible::TokenSequence LexToSequence(absl::string_view source_contents) {
  verible::TokenSequence lexed_sequence;
  VerilogLexer lexer(source_contents);
  for (lexer.DoNextToken(); !lexer.GetLastToken().isEOF();
       lexer.DoNextToken()) {
    if (verilog::VerilogLexer::KeepSyntaxTreeTokens(lexer.GetLastToken())) {
      lexed_sequence.push_back(lexer.GetLastToken());
    }
  }
  return lexed_sequence;
}

TEST(FlowTree, MultipleConditionalsSameMacro) {
  const absl::string_view test_case =
      R"(
    `ifdef A
      A_TRUE_1
    `else
      A_FALSE_1
    `endif

    `ifdef A
      A_TRUE_2
    `else
      A_FALSE_2
    `endif

    `ifndef A
      A_FALSE_3
    `else
      A_TRUE_3
    `endif)";

  FlowTree tree_test(LexToSequence(test_case));
  std::vector<FlowTree::Variant> variants;
  auto status =
      tree_test.GenerateVariants([&variants](const FlowTree::Variant &variant) {
        variants.push_back(variant);
        return true;
      });
  EXPECT_TRUE(status.ok());
  EXPECT_EQ(variants.size(), 2);

  const auto &used_macros = tree_test.GetUsedMacros();
  EXPECT_EQ(used_macros.size(), 1);
  EXPECT_THAT(used_macros[0]->text(), "A");

  // First variant: A is defined.
  EXPECT_TRUE(variants[0].macros_mask.test(0));
  EXPECT_TRUE(variants[0].visited.test(0));
  EXPECT_THAT(variants[0].sequence[0].text(), "A_TRUE_1");
  EXPECT_THAT(variants[0].sequence[1].text(), "A_TRUE_2");
  EXPECT_THAT(variants[0].sequence[2].text(), "A_TRUE_3");

  // Second variant: A is undefined.
  EXPECT_FALSE(variants[1].macros_mask.test(0));
  EXPECT_TRUE(variants[1].visited.test(0));
  EXPECT_THAT(variants[1].sequence[0].text(), "A_FALSE_1");
  EXPECT_THAT(variants[1].sequence[1].text(), "A_FALSE_2");
  EXPECT_THAT(variants[1].sequence[2].text(), "A_FALSE_3");
}

TEST(FlowTree, UnmatchedElses) {
  const absl::string_view test_cases[] = {
      R"(
    `elsif A
      A_TRUE
    `endif
    )",
      R"(
    `ifdef A
      A_TRUE
    `endif
    `elsif B
      B_TRUE
    `endif
    )",
      R"(
    `else
      A_TRUE
    `endif
    )",
      R"(
    `ifdef A
      `elsif B
        B_TRUE
      `endif
    `endif
    )",
      R"(
    `endif
    )"};

  for (auto test : test_cases) {
    FlowTree tree_test(LexToSequence(test));
    auto status = tree_test.GenerateVariants(
        [](const FlowTree::Variant &variant) { return true; });
    EXPECT_FALSE(status.ok());
    EXPECT_THAT(status.message(), StartsWith("ERROR: Unmatched"));
  }
}

TEST(FlowTree, UnvalidConditionals) {
  const absl::string_view test_cases[] = {
      R"(
    `ifdef A
      A_TRUE
    `elsif
      A_FALSE
      )",
      R"(
    `ifdef
      A_TRUE
    `else
      A_FALSE
      )",
      R"(
    `ifndef
      A_TRUE
    `else
      A_FALSE
    `endif
    )"};

  for (auto test : test_cases) {
    FlowTree tree_test(LexToSequence(test));
    auto status = tree_test.GenerateVariants(
        [](const FlowTree::Variant &variant) { return true; });
    EXPECT_FALSE(status.ok());
  }
}

TEST(FlowTree, UncompletedConditionals) {
  const absl::string_view test_cases[] = {
      R"(
    `ifdef A
      A_TRUE
    `else
      A_FALSE
      )",
      R"(
    `ifdef A
      A_TRUE
      `ifndef B
        B_FALSE
      `else
        B_TRUE
      `endif
    )"};

  for (auto test : test_cases) {
    FlowTree tree_test(LexToSequence(test));
    auto status = tree_test.GenerateVariants(
        [](const FlowTree::Variant &variant) { return true; });
    EXPECT_FALSE(status.ok());
    EXPECT_THAT(status.message(), StartsWith("ERROR: Uncompleted"));
  }
}

TEST(FlowTree, NestedConditionals) {
  const absl::string_view test_cases[] = {
      R"(
    `ifdef A
      `ifdef B
        A_B
      `else
        A_nB
      `endif
    `else
      nA_B_or_nA_nB
    `endif)"};

  for (auto test : test_cases) {
    FlowTree tree_test(LexToSequence(test));
    std::vector<FlowTree::Variant> variants;
    auto status = tree_test.GenerateVariants(
        [&variants](const FlowTree::Variant &variant) {
          variants.push_back(variant);
          return true;
        });
    EXPECT_TRUE(status.ok());
    EXPECT_EQ(variants.size(), 3);
    for (const auto &variant : variants) {
      EXPECT_EQ(variant.sequence.size(), 1);
      if (variant.macros_mask.test(0) == 0) {
        // Check that if A is undefined, then B is not visited.
        EXPECT_FALSE(variant.visited.test(1));
      } else {
        // Check that if A is defined, then B is visited.
        EXPECT_TRUE(variant.visited.test(1));
      }
    }
    const auto &used_macros = tree_test.GetUsedMacros();
    EXPECT_EQ(used_macros.size(), 2);
    EXPECT_THAT(used_macros[0]->text(), "A");
    EXPECT_THAT(used_macros[1]->text(), "B");
  }
}

TEST(FlowTree, MultipleElseIfs) {
  const absl::string_view test_case =
      R"(
    `ifdef A
      A_TRUE
    `elsif B
      B_TRUE
    `elsif EMPTY
    `elsif C
      C_TRUE
    `endif)";

  FlowTree tree_test(LexToSequence(test_case));
  std::vector<FlowTree::Variant> variants;
  auto status =
      tree_test.GenerateVariants([&variants](const FlowTree::Variant &variant) {
        variants.push_back(variant);
        return true;
      });
  EXPECT_TRUE(status.ok());
  EXPECT_EQ(variants.size(), 5);

  const auto &used_macros = tree_test.GetUsedMacros();
  EXPECT_EQ(used_macros.size(), 4);
  EXPECT_THAT(used_macros[0]->text(), "A");
  EXPECT_THAT(used_macros[1]->text(), "B");
  EXPECT_THAT(used_macros[2]->text(), "EMPTY");
  EXPECT_THAT(used_macros[3]->text(), "C");

  // A is defined.
  EXPECT_TRUE(variants[0].macros_mask.test(0));
  EXPECT_THAT(variants[0].sequence[0].text(), "A_TRUE");

  // B is defined.
  EXPECT_TRUE(variants[1].macros_mask.test(1));
  EXPECT_THAT(variants[1].sequence[0].text(), "B_TRUE");

  // EMPTY is defined.
  EXPECT_TRUE(variants[2].macros_mask.test(2));
  EXPECT_TRUE(variants[2].sequence.empty());

  // C is defined.
  EXPECT_TRUE(variants[3].macros_mask.test(3));
  EXPECT_THAT(variants[3].sequence[0].text(), "C_TRUE");
}

TEST(FlowTree, SwappedNegatedIfs) {
  const absl::string_view test_case =
      R"(
    `ifndef A
      A_FALSE
    `elsif B
      B_TRUE
    `endif

    `ifndef B
      B_FALSE
    `elsif A
      A_TRUE
    `endif)";

  FlowTree tree_test(LexToSequence(test_case));
  std::vector<FlowTree::Variant> variants;
  auto status =
      tree_test.GenerateVariants([&variants](const FlowTree::Variant &variant) {
        variants.push_back(variant);
        return true;
      });
  EXPECT_TRUE(status.ok());
  EXPECT_EQ(variants.size(), 4);

  const auto &used_macros = tree_test.GetUsedMacros();
  EXPECT_EQ(used_macros.size(), 2);
  EXPECT_THAT(used_macros[0]->text(), "A");
  EXPECT_THAT(used_macros[1]->text(), "B");

  EXPECT_THAT(variants[0].sequence[0].text(), "A_FALSE");
  EXPECT_THAT(variants[0].sequence[1].text(), "B_FALSE");

  EXPECT_THAT(variants[1].sequence[0].text(), "A_FALSE");

  EXPECT_THAT(variants[2].sequence[0].text(), "B_TRUE");
  EXPECT_THAT(variants[2].sequence[1].text(), "A_TRUE");

  EXPECT_THAT(variants[3].sequence[0].text(), "B_FALSE");
}

TEST(FlowTree, CompleteConditional) {
  const absl::string_view test_case =
      R"(
    `ifdef A
      A_TRUE
    `elsif B
      B_TRUE
    `elsif C
      C_TRUE
    `else
      ALL_FALSE
    `endif)";

  FlowTree tree_test(LexToSequence(test_case));
  std::vector<FlowTree::Variant> variants;
  auto status =
      tree_test.GenerateVariants([&variants](const FlowTree::Variant &variant) {
        variants.push_back(variant);
        return true;
      });
  EXPECT_TRUE(status.ok());
  EXPECT_EQ(variants.size(), 4);

  const auto &used_macros = tree_test.GetUsedMacros();
  EXPECT_EQ(used_macros.size(), 3);
  EXPECT_THAT(used_macros[0]->text(), "A");
  EXPECT_THAT(used_macros[1]->text(), "B");
  EXPECT_THAT(used_macros[2]->text(), "C");

  EXPECT_TRUE(variants[0].macros_mask.test(0));
  EXPECT_THAT(variants[0].sequence[0].text(), "A_TRUE");

  EXPECT_TRUE(variants[1].macros_mask.test(1));
  EXPECT_THAT(variants[1].sequence[0].text(), "B_TRUE");

  EXPECT_TRUE(variants[2].macros_mask.test(2));
  EXPECT_THAT(variants[2].sequence[0].text(), "C_TRUE");

  EXPECT_THAT(variants[3].sequence[0].text(), "ALL_FALSE");
}

}  // namespace
}  // namespace verilog
