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

#include "verible/verilog/transform/obfuscate.h"

#include <sstream>
#include <string>
#include <string_view>
#include <utility>

#include "absl/status/status.h"
#include "gtest/gtest.h"
#include "verible/common/strings/obfuscator.h"

namespace verilog {
namespace {

using verible::IdentifierObfuscator;

static std::string ExpectNeverToBeCalled(std::string_view) {
  ADD_FAILURE() << "This identifier generator should've never been called";
  return "";
}

// To make these tests deterministic, obfuscation maps are pre-populated.
TEST(ObfuscateVerilogCodeTest, PreloadedSubstitutions) {
  IdentifierObfuscator ob(ExpectNeverToBeCalled);
  const std::pair<std::string, std::string> subs[] = {
      {"aaa", "AAA"},
      {"bbb", "BBB"},
      {"ccc", "CCC"},
  };
  for (const auto &sub : subs) {
    ASSERT_TRUE(ob.encode(sub.first, sub.second));
  }
  const std::pair<std::string_view, std::string_view> kTestCases[] = {
      {"", ""},  // empty file
      {"\n", "\n"},
      {"//comments unchanged\n", "//comments unchanged\n"},
      {"/*comments unchanged*/\n", "/*comments unchanged*/\n"},
      {"aaa bbb;\n", "AAA BBB;\n"},
      {"aaa bbb;\nbbb  aaa;\n", "AAA BBB;\nBBB  AAA;\n"},
      {"aaa[ccc] bbb;\n", "AAA[CCC] BBB;\n"},
      {"aaa bbb[ccc];\n", "AAA BBB[CCC];\n"},
      {"parameter int aaa = 111;\n", "parameter int AAA = 111;\n"},
      {"parameter int aaa = ccc;\n", "parameter int AAA = CCC;\n"},
      {"`aaa(bbb, ccc)\n", "`AAA(BBB, CCC)\n"},
      {"`aaa(`bbb(`ccc)))\n", "`AAA(`BBB(`CCC)))\n"},
      {"`ifdef aaa\n`elsif ccc\n`else\n`endif\n",
       "`ifdef AAA\n`elsif CCC\n`else\n`endif\n"},
      {"`define ccc bbb+aaa\n", "`define CCC BBB+AAA\n"},
      {"`define ccc `bbb+`aaa\n", "`define CCC `BBB+`AAA\n"},
      {"task bbb;\n  $aaa;\nendtask\n", "task BBB;\n  $aaa;\nendtask\n"},
      {"task bbb;\n  $aaa();\nendtask\n", "task BBB;\n  $aaa();\nendtask\n"},
      {"task bbb;\n  $aaa($ccc());\nendtask\n",
       "task BBB;\n  $aaa($ccc());\nendtask\n"},
      {"function aaa()\nreturn bbb^ccc;\nendfunction\n",
       "function AAA()\nreturn BBB^CCC;\nendfunction\n"},
      {"function aaa()\nreturn bbb(ccc);\nendfunction\n",
       "function AAA()\nreturn BBB(CCC);\nendfunction\n"},
      {"function aaa()\nreturn {bbb,ccc};\nendfunction\n",
       "function AAA()\nreturn {BBB,CCC};\nendfunction\n"},
      // token concatenation
      {"aaa``bbb;\n", "AAA``BBB;\n"},
      {"`define ccc(aaa, bbb) aaa``bbb;\n",
       "`define CCC(AAA, BBB) AAA``BBB;\n"},
      {"`ccc(aaa``bbb, bbb``aaa);\n", "`CCC(AAA``BBB, BBB``AAA);\n"},
      // comments inside defines
      {
          "`define ccc \\\n"
          "  // comment1 \\\n"
          "  // comment2\n",
          "`define CCC \\\n"
          "  // comment1 \\\n"
          "  // comment2\n",
      },
      {
          "`define ccc \\\n"
          "  aaa(); \\\n"
          "  // comment1 \\\n"
          "  bbb(); \\\n"
          "  // comment2\n",
          "`define CCC \\\n"
          "  AAA(); \\\n"
          "  // comment1 \\\n"
          "  BBB(); \\\n"
          "  // comment2\n",
      },
      {"module ccc#(parameter aaa = `aaa, parameter bbb = `bbb)();endmodule",
       "module CCC#(parameter AAA = `AAA, parameter BBB = `BBB)();endmodule"},
  };
  for (const auto &test : kTestCases) {
    std::ostringstream output;
    const auto status = ObfuscateVerilogCode(test.first, &output, &ob);
    EXPECT_TRUE(status.ok()) << "Unexpected error: " << status.message();
    EXPECT_EQ(output.str(), test.second);
  }
}

TEST(ObfuscateVerilogCodeTest, InputLexicalError) {
  const std::string_view kTestCases[] = {
      "789badid",
      "`FOO(8911badid)\n",
      "`define FOO 911badid\n",
      "`FOO(`)\n",
  };
  for (const auto &test : kTestCases) {
    IdentifierObfuscator ob(RandomEqualLengthSymbolIdentifier);
    std::ostringstream output;
    const auto status = ObfuscateVerilogCode(test, &output, &ob);
    EXPECT_FALSE(status.ok());
    EXPECT_EQ(status.code(), absl::StatusCode::kInvalidArgument)
        << "message: " << status.message();
  }
}

TEST(ObfuscateVerilogCodeTest, SeededCandidatesArePortableAndPerIdentifier) {
  auto first = SeededSymbolIdentifierGenerator(123, 12);
  auto second = SeededSymbolIdentifierGenerator(123, 12);
  auto different = SeededSymbolIdentifierGenerator(124, 12);
  const auto alpha = first("alpha");
  EXPECT_EQ(alpha, "jZJVCy4so7SJ");
  const auto beta = second("beta");
  EXPECT_EQ(second("alpha"), alpha);
  EXPECT_EQ(first("beta"), beta);
  EXPECT_NE(different("alpha"), alpha);
  auto equal_length = SeededSymbolIdentifierGenerator(0);
  EXPECT_EQ(equal_length("a").length(), 1);
  EXPECT_EQ(equal_length("long_name").length(), 9);
}

TEST(ObfuscateVerilogCodeTest, VariableLengthsRemainReversible) {
  IdentifierObfuscator ob(ExpectNeverToBeCalled);
  ASSERT_TRUE(ob.load("X LONG_MACRO\na long_argument\n").ok());
  const std::string original = "`define X(a) a+1\n`X(a)\n";
  std::ostringstream output;
  EXPECT_TRUE(ObfuscateVerilogCode(original, &output, &ob, false).ok());
  EXPECT_EQ(output.str(),
            "`define LONG_MACRO(long_argument) long_argument+1\n"
            "`LONG_MACRO(long_argument)\n");
  IdentifierObfuscator decoder(ExpectNeverToBeCalled);
  ASSERT_TRUE(decoder.load(ob.save()).ok());
  decoder.set_decode_mode(true);
  std::ostringstream decoded;
  EXPECT_TRUE(
      ObfuscateVerilogCode(output.str(), &decoded, &decoder, false).ok());
  EXPECT_EQ(decoded.str(), original);
  std::ostringstream rejected;
  EXPECT_FALSE(ObfuscateVerilogCode(original, &rejected, &ob).ok());
  EXPECT_TRUE(rejected.str().empty());
}

TEST(ObfuscateVerilogCodeTest, ExhaustionDoesNotEmitPartialOutput) {
  IdentifierObfuscator ob([](std::string_view) { return "x"; }, 3);
  std::ostringstream output;
  const auto status =
      ObfuscateVerilogCode("wire first, second;", &output, &ob, false);
  EXPECT_EQ(status.code(), absl::StatusCode::kResourceExhausted);
  EXPECT_TRUE(output.str().empty());
}

}  // namespace
}  // namespace verilog
