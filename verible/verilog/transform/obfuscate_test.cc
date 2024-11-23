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
#include <utility>

#include "absl/status/status.h"
#include "absl/strings/string_view.h"
#include "gtest/gtest.h"
#include "verible/common/strings/obfuscator.h"

namespace verilog {
namespace {

using verible::IdentifierObfuscator;

static std::string ExpectNeverToBeCalled(absl::string_view) {
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
  const std::pair<absl::string_view, absl::string_view> kTestCases[] = {
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
  const absl::string_view kTestCases[] = {
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

}  // namespace
}  // namespace verilog
