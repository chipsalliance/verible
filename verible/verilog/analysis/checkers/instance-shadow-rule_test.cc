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

#include "verible/verilog/analysis/checkers/instance-shadow-rule.h"

#include <initializer_list>
#include <memory>
#include <string_view>

#include "absl/status/status.h"
#include "gtest/gtest.h"
#include "verible/common/analysis/linter-test-utils.h"
#include "verible/common/analysis/syntax-tree-linter-test-utils.h"
#include "verible/common/analysis/syntax-tree-linter.h"
#include "verible/common/util/logging.h"
#include "verible/verilog/analysis/verilog-analyzer.h"
#include "verible/verilog/parser/verilog-token-enum.h"

namespace verilog {
namespace analysis {
namespace {

using verible::LintTestCase;
using verible::RunLintTestCases;

TEST(InstanceShadowingTest, FunctionPass) {
  const std::initializer_list<LintTestCase> kInstanceShadowingTestCases = {
      {""},
      {"module foo; logic a; endmodule"},
      {"module foo; logic a, b, c;  endmodule:foo;"},
      {"module foo; logic a; class boo; endclass:boo; endmodule:foo;"},
      {"module foo; logic a; begin boo; bit var1; end:boo; endmodule:foo;"},
      {"module foo; logic a; begin boo;", "if(a) begin baz\n",
       "$display(\"\t Value of i = %0d\",i);", "\nend:baz", "\nend:boo;",
       "endmodule:foo;"},
      {"module foo; logic a; begin boo;",
       "loop: for(int i=0;i<5;i++) begin baz\n",
       "$display(\"\t Value of i = %0d\",i);", "\nend:baz", "\nend:boo;",
       "endmodule:foo;"},
      {
          "module foo;\n",
          "genvar i;\n",
          "generate\n",
          "initial begin\n",
          "for(int i=0;i<5;i++) $display(\"\t i = %d\",i);\n",
          "end\n",
          "endgenerate\n",
          "endmodule:foo;\n",
      },
      {
          "module foo;\n",
          "initial begin\n",
          "int a;\n",
          "end\n",
          "initial begin\n",
          "int a;\n",
          "end\n",
          "endmodule:foo;\n",
      },
      {
          "module foo;\n",
          "initial begin\n",
          "int a;\n",
          "end\n",
          "int a;\n",
          "endmodule:foo;\n",
      },
      {
          "interface i;\n",
          "logic a;\n",
          "modport m (input a);\n",
          "endinterface\n",
      },
      {
          "interface i;\n",
          "logic a;\n",
          "modport mm(clocking a);\n",
          "endinterface\n",
      },

  };
  RunLintTestCases<VerilogAnalyzer, InstanceShadowRule>(
      kInstanceShadowingTestCases);
}

TEST(InstanceShadowingTest, FunctionFailures) {
  constexpr int kToken = SymbolIdentifier;
  // clang-format off
  const std::initializer_list<LintTestCase> kInstanceShadowingTestCases = {
      {"module foo; logic ", {kToken, "foo"}, "; endmodule:foo;"},
      {"module foo; logic a; logic ", {kToken, "a"}, "; endmodule:foo;"},
      {"module foo; logic a; class ",
       {kToken, "foo"},
       "; endclass:foo; endmodule:foo;"},
      {"module foo; logic a; class boo; logic ",
       {kToken, "boo"},
       "; endclass:boo; endmodule:foo;"},
      {"module foo; logic a; class boo; logic ",
       {kToken, "foo"},
       "; endclass:boo; endmodule:foo;"},
      {
          "module foo;\n",
          "int i;\n",
          "initial begin\n",
          "  for(int ", {kToken, "i"}, "=0;i<5;i++) $display(\"i = %d\",i);\n",
          "end\n",
          "endmodule:foo;\n",
      },
      {
          "module foo;\n",
          "int a;\n",
          "initial begin\n",
          "  int ", {kToken, "a"}, ";\n"
          "end\n",
          "endmodule:foo;\n",
      },
      {
          "module foo;\n",
          "initial begin\n",
          "  int a;\n",
          "  int ", {kToken, "a"},";\n",
          "end\n",
          "endmodule:foo;\n",
      },
      {
        "function automatic int foo (input bit in);\n",
        "  bit ", {kToken, "in"}, ";\n",
        "endfunction\n"
      },
      {
        "function automatic int foo (input bit in, "
        "                            input bit ",  {kToken, "in"}, ");\n",
        "                            bit out;\n",
        "endfunction\n"
      },
      // Currently, we do not recongize the "K&R-Style" declare+define pattern
      {
        "module foo(a, b);"
        " input wire ", {kToken, "a"}, ";"  // Warning despite this actually ok
        " output reg ", {kToken, "b"}, ";"
        "endmodule"
      },
  };
  // clang-format on
  RunLintTestCases<VerilogAnalyzer, InstanceShadowRule>(
      kInstanceShadowingTestCases);
}

TEST(InstanceShadowingTest, CorrectLocationTest) {
  constexpr int kToken = SymbolIdentifier;
  const std::initializer_list<LintTestCase> kInstanceShadowingTestCases = {
      {"module ", {kToken, "foo"}, "; logic foo;\n endmodule:foo;"},
      {"module foo; logic ", {kToken, "a"}, "; logic a; endmodule:foo;"},
  };
  auto rule_generator = [&]() -> std::unique_ptr<InstanceShadowRule> {
    std::unique_ptr<InstanceShadowRule> instance(new InstanceShadowRule());
    absl::Status config_status = instance->Configure("");
    CHECK(config_status.ok()) << config_status.message();
    return instance;
  };
  for (const auto &test : kInstanceShadowingTestCases) {
    VerilogAnalyzer analyzer(test.code, "<<inline-test>>");
    absl::Status unused_parser_status = analyzer.Analyze();
    verible::SyntaxTreeLinter linter_;
    linter_.AddRule(rule_generator());
    linter_.Lint(*ABSL_DIE_IF_NULL(analyzer.Data().SyntaxTree()));
    CHECK_EQ(linter_.ReportStatus().size(), 1);
    CHECK_EQ(linter_.ReportStatus()[0].violations.size(), 1);

    // Report detailed differences, if any.
    const std::string_view base_text = analyzer.Data().Contents();
    std::string_view foo = test.FindImportantTokens(base_text)[0].text();
    std::string_view bar =
        linter_.ReportStatus()[0].violations.begin()->token.text();
    ASSERT_TRUE(foo == bar);
  }
}

}  // namespace
}  // namespace analysis
}  // namespace verilog
