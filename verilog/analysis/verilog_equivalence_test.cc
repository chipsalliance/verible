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

#include "verilog/analysis/verilog_equivalence.h"

#include <iostream>
#include <memory>
#include <vector>

#include "gmock/gmock.h"
#include "gtest/gtest.h"
#include "absl/memory/memory.h"
#include "absl/strings/match.h"
#include "absl/strings/string_view.h"
#include "absl/types/span.h"
#include "common/text/constants.h"
#include "common/text/token_info.h"
#include "common/text/token_stream_view.h"
#include "common/util/logging.h"
#include "common/util/status.h"
#include "verilog/analysis/verilog_analyzer.h"
#include "verilog/parser/verilog_token_enum.h"

#undef EXPECT_OK
#define EXPECT_OK(value) EXPECT_TRUE((value).ok())
#undef ASSERT_OK
#define ASSERT_OK(value) ASSERT_TRUE((value).ok())

namespace verilog {
namespace {

using verible::TokenInfo;

template <typename T>
void _LexTestCases(const T& test_cases,
                   std::vector<std::unique_ptr<VerilogAnalyzer>>* analyzers) {
  for (auto test : test_cases) {
    analyzers->emplace_back(absl::make_unique<VerilogAnalyzer>(test, ""));
  }
  for (auto& analyzer : *analyzers) {
    ASSERT_OK(ABSL_DIE_IF_NULL(analyzer)->Tokenize());
  }
}

bool _EquivalentTokenStreams(const VerilogAnalyzer& a1,
                             const VerilogAnalyzer& a2,
                             std::ostream* errstream = &std::cout) {
  const auto& tokens1(a1.Data().TokenStream());
  const auto& tokens2(a2.Data().TokenStream());
  const bool eq = FormatEquivalent(tokens1, tokens2, errstream);
  // Check that commutative comparison yields same result.
  // Don't bother with the error stream.
  const bool commutative = FormatEquivalent(tokens2, tokens1);
  EXPECT_EQ(eq, commutative);
  return eq;
}

TEST(FormatEquivalentTest, Spaces) {
  const char* kTestCases[] = {
      "",
      " ",
      "\n",
      "\t",
  };
  // Lex all of the test cases once.
  std::vector<std::unique_ptr<VerilogAnalyzer>> analyzers;
  _LexTestCases(absl::MakeSpan(kTestCases), &analyzers);
  EXPECT_TRUE(_EquivalentTokenStreams(*analyzers[0], *analyzers[1]));
  EXPECT_TRUE(_EquivalentTokenStreams(*analyzers[0], *analyzers[2]));
  EXPECT_TRUE(_EquivalentTokenStreams(*analyzers[0], *analyzers[3]));
  EXPECT_TRUE(_EquivalentTokenStreams(*analyzers[1], *analyzers[2]));
  EXPECT_TRUE(_EquivalentTokenStreams(*analyzers[1], *analyzers[3]));
  EXPECT_TRUE(_EquivalentTokenStreams(*analyzers[2], *analyzers[3]));
}

TEST(FormatEquivalentTest, ShortSequences) {
  const char* kTestCases[] = {
      "1",
      "2",
      "1;",
      "1 ;",
  };
  // Lex all of the test cases once.
  std::vector<std::unique_ptr<VerilogAnalyzer>> analyzers;
  _LexTestCases(absl::MakeSpan(kTestCases), &analyzers);
  EXPECT_FALSE(_EquivalentTokenStreams(*analyzers[0], *analyzers[1]));
  EXPECT_FALSE(_EquivalentTokenStreams(*analyzers[0], *analyzers[2]));
  EXPECT_FALSE(_EquivalentTokenStreams(*analyzers[0], *analyzers[3]));
  EXPECT_FALSE(_EquivalentTokenStreams(*analyzers[1], *analyzers[2]));
  EXPECT_FALSE(_EquivalentTokenStreams(*analyzers[1], *analyzers[3]));
  EXPECT_TRUE(_EquivalentTokenStreams(*analyzers[2], *analyzers[3]));
}

TEST(FormatEquivalentTest, Identifiers) {
  const char* kTestCases[] = {
      "foo bar;",
      "   foo\t\tbar    ;   ",
      "foobar;",  // only 2 tokens
      "foo bar\n;\n",
  };
  // Lex all of the test cases once.
  std::vector<std::unique_ptr<VerilogAnalyzer>> analyzers;
  _LexTestCases(absl::MakeSpan(kTestCases), &analyzers);
  EXPECT_TRUE(_EquivalentTokenStreams(*analyzers[0], *analyzers[1]));
  EXPECT_FALSE(_EquivalentTokenStreams(*analyzers[0], *analyzers[2]));
  EXPECT_TRUE(_EquivalentTokenStreams(*analyzers[0], *analyzers[3]));
  EXPECT_FALSE(_EquivalentTokenStreams(*analyzers[1], *analyzers[2]));
  EXPECT_TRUE(_EquivalentTokenStreams(*analyzers[1], *analyzers[3]));
  EXPECT_FALSE(_EquivalentTokenStreams(*analyzers[2], *analyzers[3]));
}

TEST(FormatEquivalentTest, Keyword) {
  const char* kTestCases[] = {
      "wire foo;",
      "  wire  \n\t\t   foo  ;\n",
  };
  // Lex all of the test cases once.
  std::vector<std::unique_ptr<VerilogAnalyzer>> analyzers;
  _LexTestCases(absl::MakeSpan(kTestCases), &analyzers);
  EXPECT_TRUE(_EquivalentTokenStreams(*analyzers[0], *analyzers[1]));
}

TEST(FormatEquivalentTest, Comments) {
  const char* kTestCases[] = {
      "// comment1\n",        //
      "// comment1 \n",       //
      "//    comment1\n",     //
      "   //    comment1\n",  // same as [2]
      "// comment2\n",        //
      "/* comment1 */\n",     //
      "/*  comment1  */\n",   //
  };
  // Lex all of the test cases once.
  std::vector<std::unique_ptr<VerilogAnalyzer>> analyzers;
  auto span = absl::MakeSpan(kTestCases);
  _LexTestCases(span, &analyzers);
  // At some point in the future when token-reflowing is implemented, these
  // will need to become smarter checks.
  // For now, they only check for exact match.
  for (size_t i = 0; i < span.size(); ++i) {
    for (size_t j = i + 1; j < span.size(); ++j) {
      if (i == 2 && j == 3) {
        EXPECT_TRUE(_EquivalentTokenStreams(*analyzers[i], *analyzers[j]));
      } else {
        EXPECT_FALSE(_EquivalentTokenStreams(*analyzers[i], *analyzers[j]));
      }
    }
  }
}

TEST(FormatEquivalentTest, DiagnosticLength) {
  const char* kTestCases[] = {
      "module foo\n",
      "module foo;\n",
  };
  std::vector<std::unique_ptr<VerilogAnalyzer>> analyzers;
  auto span = absl::MakeSpan(kTestCases);
  _LexTestCases(span, &analyzers);
  {
    std::ostringstream errs;
    EXPECT_FALSE(_EquivalentTokenStreams(*analyzers[0], *analyzers[1], &errs));
    EXPECT_TRUE(absl::StartsWith(
        errs.str(), "Mismatch in token sequence lengths: 3 vs. 4"));
    EXPECT_TRUE(absl::StrContains(errs.str(), "First mismatched token [2]:"));
  }
  {
    std::ostringstream errs;
    EXPECT_FALSE(_EquivalentTokenStreams(*analyzers[1], *analyzers[0], &errs));
    EXPECT_TRUE(absl::StartsWith(
        errs.str(), "Mismatch in token sequence lengths: 4 vs. 3"));
    EXPECT_TRUE(absl::StrContains(errs.str(), "First mismatched token [2]:"));
  }
}

TEST(FormatEquivalentTest, DiagnosticLengthTrimEnd) {
  const char* kTestCases[] = {
      "module foo;",
  };
  std::vector<std::unique_ptr<VerilogAnalyzer>> analyzers;
  auto span = absl::MakeSpan(kTestCases);
  _LexTestCases(span, &analyzers);
  // Make a copy of token sequences and trim off the $end.
  verible::TokenSequence longer(analyzers[0]->Data().TokenStream());
  ASSERT_EQ(longer.back().token_enum, verible::TK_EOF);
  longer.pop_back();
  verible::TokenSequence shorter(longer);
  shorter.pop_back();
  {
    std::ostringstream errs;
    EXPECT_FALSE(FormatEquivalent(shorter, longer, &errs));
    EXPECT_TRUE(absl::StartsWith(
        errs.str(), "Mismatch in token sequence lengths: 2 vs. 3"));
    EXPECT_TRUE(
        absl::StrContains(errs.str(), "First excess token in right sequence:"));
  }
  {
    std::ostringstream errs;
    EXPECT_FALSE(FormatEquivalent(longer, shorter, &errs));
    EXPECT_TRUE(absl::StartsWith(
        errs.str(), "Mismatch in token sequence lengths: 3 vs. 2"));
    EXPECT_TRUE(
        absl::StrContains(errs.str(), "First excess token in left sequence:"));
  }
}

TEST(FormatEquivalentTest, DiagnosticMismatch) {
  const char* kTestCases[] = {
      "module foo;\n",
      "module bar;\n",
      "module foo,\n",
  };
  std::vector<std::unique_ptr<VerilogAnalyzer>> analyzers;
  auto span = absl::MakeSpan(kTestCases);
  _LexTestCases(span, &analyzers);
  {
    std::ostringstream errs;
    EXPECT_FALSE(_EquivalentTokenStreams(*analyzers[0], *analyzers[1], &errs));
    EXPECT_TRUE(absl::StartsWith(errs.str(), "First mismatched token [1]:"));
  }
  {
    std::ostringstream errs;
    EXPECT_FALSE(_EquivalentTokenStreams(*analyzers[0], *analyzers[2], &errs));
    EXPECT_TRUE(absl::StartsWith(errs.str(), "First mismatched token [2]:"));
  }
}

}  // namespace
}  // namespace verilog
