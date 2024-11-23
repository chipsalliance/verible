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

#include "verible/verilog/CST/DPI.h"

#include <vector>

#include "gtest/gtest.h"
#include "verible/common/analysis/syntax-tree-search-test-utils.h"
#include "verible/common/analysis/syntax-tree-search.h"
#include "verible/common/text/text-structure.h"
#include "verible/common/util/logging.h"
#include "verible/verilog/CST/match-test-utils.h"

#undef ASSERT_OK
#define ASSERT_OK(value) ASSERT_TRUE((value).ok())

namespace verilog {
namespace {

using verible::SyntaxTreeSearchTestCase;
using verible::TextStructureView;
using verible::TreeSearchMatch;

TEST(FindAllDPIImportsTest, CountMatches) {
  constexpr int kTag = 1;  // don't care
  const SyntaxTreeSearchTestCase kTestCases[] = {
      {""},
      {"module m;\nendmodule\n"},
      {"class c;\nendclass\n"},
      {"function f;\nendfunction\n"},
      {"package p;\nendpackage\n"},
      {"task t;\nendtask\n"},
      {"module m;\n"
       "  function int add();\n"
       "  endfunction\n"
       "endmodule\n"},
      {"module m;\n"
       "  ",
       {kTag, "import \"DPI-C\" function int add();"},
       "\n"
       "endmodule\n"},
      {"module m;\n"
       "  ",
       {kTag, "import \"DPI-C\" function int add();"},
       "\n"
       "  foo bar();\n"
       "endmodule\n"},
      {"module m;\n"
       "  ",
       {kTag, "import \"DPI-C\" function int add();"},
       "\n"
       "  foo bar();\n"
       "  ",
       {kTag, "import \"DPI-C\" function int sub(input int x, y);"},
       "\n"
       "endmodule\n"},
  };
  for (const auto &test : kTestCases) {
    TestVerilogSyntaxRangeMatches(
        __FUNCTION__, test, [](const TextStructureView &text_structure) {
          const auto &root = text_structure.SyntaxTree();
          return FindAllDPIImports(*ABSL_DIE_IF_NULL(root));
        });
  }
}

TEST(GetDPIImportPrototypeTest, Various) {
  constexpr int kTag = 1;  // value doesn't matter
  const SyntaxTreeSearchTestCase kTestCases[] = {
      // each of these test cases should match exactly one DPI import
      {"module m;\n"
       "  import \"DPI-C\" ",
       {kTag, "function void foo"},
       ";",
       "\n"
       "endmodule\n"},
      {"module m;\n"
       "  wire w;\n"
       "  import \"DPI-C\" ",
       {kTag, "function void foo"},
       ";",
       "\n"
       "  logic l;\n"
       "endmodule\n"},
      {"module m;\n"
       "  import \"DPI-C\" ",
       {kTag, "function int add()"},
       ";",
       "\n"
       "endmodule\n"},
      {"module m;\n"
       "  import   \"DPI-C\" ",
       {kTag, "function   int   add( input int x , y)"},
       ";",
       "\n"
       "endmodule\n"},
  };
  for (const auto &test : kTestCases) {
    TestVerilogSyntaxRangeMatches(
        __FUNCTION__, test, [](const TextStructureView &text_structure) {
          const auto &root = text_structure.SyntaxTree();

          const auto dpi_imports = FindAllDPIImports(*ABSL_DIE_IF_NULL(root));
          std::vector<TreeSearchMatch> prototypes;
          prototypes.reserve(dpi_imports.size());
          for (const auto &dpi_import : dpi_imports) {
            prototypes.push_back(TreeSearchMatch{
                GetDPIImportPrototype(*dpi_import.match), /* no context */});
          }
          return prototypes;
        });
  }
}

}  // namespace
}  // namespace verilog
