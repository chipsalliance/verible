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

#include "verilog/analysis/flow_tree.h"

#include <vector>

#include "absl/status/status.h"
#include "absl/strings/string_view.h"
#include "common/lexer/token_stream_adapter.h"
#include "gmock/gmock.h"
#include "gtest/gtest.h"
#include "verilog/parser/verilog_lexer.h"
#include "verilog/parser/verilog_token_enum.h"

namespace verilog {
namespace {

using testing::StartsWith;
using VariantReceiver = std::function<bool(const FlowTree::Variant& variant)>;

// Global variables for testing, 'VariantReceiver's updates them.
std::vector<FlowTree::Variant> g_variants;

// Resets the global variants vector (to be used before any test).
void ResetGlobalTestingVariables() { g_variants.clear(); }

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

// A VariantReceiver function that add variants into g_variants.
bool GetVariant(const FlowTree::Variant& variant) {
  g_variants.push_back(variant);
  return true;
}

TEST(FlowTree, MultipleConditionalsSameMacro) {
  const absl::string_view test_case =
      R"(
    `ifdef A
      A_TRUE
    `else
      A_FALSE
    `endif

    `ifdef A
      A_TRUE
    `else
      A_FALSE
    `endif

    `ifndef A
      A_FALSE
    `else
      A_TRUE
    `endif)";

  FlowTree tree_test(LexToSequence(test_case));
  ResetGlobalTestingVariables();
  auto status = tree_test.GenerateVariants(GetVariant);
  EXPECT_TRUE(status.ok());
  EXPECT_EQ(g_variants.size(), 2);
  for (const auto& variant : g_variants[0].sequence) {
    EXPECT_EQ(variant.text(), "A_TRUE");
  }
  for (const auto& variant : g_variants[1].sequence) {
    EXPECT_EQ(variant.text(), "A_FALSE");
  }
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
    ResetGlobalTestingVariables();
    auto status = tree_test.GenerateVariants(GetVariant);
    EXPECT_FALSE(status.ok());
    EXPECT_THAT(status.message(), StartsWith("ERROR: Unmatched"));
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
    ResetGlobalTestingVariables();
    auto status = tree_test.GenerateVariants(GetVariant);
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
    ResetGlobalTestingVariables();
    auto status = tree_test.GenerateVariants(GetVariant);
    EXPECT_TRUE(status.ok());
    EXPECT_EQ(g_variants.size(), 3);
    for (const auto& variant : g_variants) {
      EXPECT_EQ(variant.sequence.size(), 1);
    }
  }
}

}  // namespace
}  // namespace verilog
