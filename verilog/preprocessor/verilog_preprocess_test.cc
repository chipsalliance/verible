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

#include "verilog/preprocessor/verilog_preprocess.h"

#include <algorithm>
#include <map>
#include <vector>

#include "absl/status/status.h"
#include "absl/strings/string_view.h"
#include "common/text/macro_definition.h"
#include "common/text/token_info.h"
#include "common/util/container_util.h"
#include "gmock/gmock.h"
#include "gtest/gtest.h"
#include "verilog/analysis/verilog_analyzer.h"
#include "verilog/parser/verilog_token_enum.h"

namespace verilog {
namespace {

using testing::ElementsAre;
using testing::Pair;
using testing::StartsWith;

using verible::container::FindOrNull;

class PreprocessorTester {
 public:
  PreprocessorTester(absl::string_view text,
                     const VerilogPreprocess::Config& config)
      : analyzer_(text, "<<inline-file>>", config),
        status_(analyzer_.Analyze()) {}

  explicit PreprocessorTester(absl::string_view text)
      : PreprocessorTester(text, VerilogPreprocess::Config()) {}

  const VerilogPreprocessData& PreprocessorData() const {
    return analyzer_.PreprocessorData();
  }

  const verible::TextStructureView& Data() const { return analyzer_.Data(); }

  const absl::Status& Status() const { return status_; }

  const VerilogAnalyzer& Analyzer() const { return analyzer_; }

 private:
  VerilogAnalyzer analyzer_;
  const absl::Status status_;
};

struct FailTest {
  absl::string_view input;
  int offset;
};
TEST(VerilogPreprocessTest, InvalidPreprocessorInputs) {
  const FailTest test_cases[] = {
      {"`define\n", 8},                      // unterminated macro definition
      {"\n\n`define\n", 10},                 // unterminated macro definition
      {"`define 789\n", 8},                  // expect identifier for macro name
      {"`define 789 non-sense\n", 8},        // expect identifier for macro name
      {"`define 789 \\\nnon-sense\n", 8},    // expect identifier for macro name
      {"`define FOO(\n", 13},                // unterminated parameter list
      {"`define FOO(234\n", 12},             // invalid parameter name
      {"`define FOO(234)\n", 12},            // invalid parameter name
      {"`define FOO(aaa\n", 16},             // unterminated parameter list
      {"`define FOO(aaa;\n", 15},            // bad parameter separator
      {"`define FOO(aaa bbb\n", 16},         // bad parameter separator
      {"`define FOO(aaa bbb)\n", 16},        // bad parameter separator
      {"`define FOO(aaa+bbb)\n", 15},        // bad parameter separator
      {"`define FOO(aaa.zzz\n", 15},         // bad parameter separator
      {"`define FOO(aaa.zzz)\n", 15},        // bad parameter separator
      {"`define FOO(aaa,\n", 17},            // unterminated parameter list
      {"`define FOO(aaa,)\n", 16},           // missing parameter name
      {"`define FOO(,,)\n", 12},             // missing parameter name
      {"`define FOO(aaa, 345)\n", 17},       // invalid parameter name
      {"`define FOO(aaa=\n", 17},            // unterminated default parameter
      {"`define FOO(aaa =\n", 18},           // unterminated default parameter
      {"`define FOO(aaa = 9\n", 20},         // expecting ',' or ')'
      {"`define FOO(aaa = 9, bbb =\n", 27},  // unterminated parameter list
      {"`define FOO(aa = 9, bb = 2\n", 27},  // expecting ',' or ')'
  };
  for (const auto& test_case : test_cases) {
    PreprocessorTester tester(test_case.input);
    EXPECT_FALSE(tester.Status().ok())
        << "Expected preprocess to fail on invalid input: \"" << test_case.input
        << "\"";
    const auto& rejected_tokens = tester.Analyzer().GetRejectedTokens();
    ASSERT_FALSE(rejected_tokens.empty())
        << "on invalid input: \"" << test_case.input << "\"";
    const int rejected_token_offset =
        rejected_tokens[0].token_info.left(tester.Analyzer().Data().Contents());
    EXPECT_EQ(rejected_token_offset, test_case.offset)
        << "on invalid input: \"" << test_case.input << "\"";
  }
}

#define EXPECT_PARSE_OK()                                                \
  do {                                                                   \
    EXPECT_TRUE(tester.Status().ok()) << "Unexpected analyzer failure."; \
    EXPECT_TRUE(tester.PreprocessorData().errors.empty());               \
    EXPECT_TRUE(tester.Analyzer().GetRejectedTokens().empty());          \
  } while (false)

// Verify that VerilogPreprocess works without any directives.
TEST(VerilogPreprocessTest, WorksWithoutDefinitions) {
  absl::string_view test_cases[] = {
      "",
      "\n",
      "module foo;\nendmodule\n",
      "module foo(input x, output y);\nendmodule\n",
  };
  for (const auto& test_case : test_cases) {
    PreprocessorTester tester(test_case);
    EXPECT_PARSE_OK();

    const auto& definitions = tester.PreprocessorData().macro_definitions;
    EXPECT_TRUE(definitions.empty());
  }
}

TEST(VerilogPreprocessTest, OneMacroDefinitionNoParamsNoValue) {
  absl::string_view test_cases[] = {
      "`define FOOOO\n",
      "`define     FOOOO\n",
      "module foo;\nendmodule\n"
      "`define FOOOO\n",
      "`define FOOOO\n"
      "module foo;\nendmodule\n",
  };
  for (const auto& test_case : test_cases) {
    PreprocessorTester tester(test_case);
    EXPECT_PARSE_OK();

    const auto& definitions = tester.PreprocessorData().macro_definitions;
    EXPECT_THAT(definitions, ElementsAre(Pair("FOOOO", testing::_)));
    auto macro = FindOrNull(definitions, "FOOOO");
    ASSERT_NE(macro, nullptr);
    EXPECT_EQ(macro->DefinitionText().text(), "");
    EXPECT_FALSE(macro->IsCallable());
    EXPECT_TRUE(macro->Parameters().empty());
  }
}

TEST(VerilogPreprocessTest, OneMacroDefinitionNoParamsSimpleValue) {
  PreprocessorTester tester(
      "module foo;\nendmodule\n"
      "`define FOOOO \"bar\"\n");
  EXPECT_PARSE_OK();

  const auto& definitions = tester.PreprocessorData().macro_definitions;
  EXPECT_THAT(definitions, ElementsAre(Pair("FOOOO", testing::_)));
  auto macro = FindOrNull(definitions, "FOOOO");
  ASSERT_NE(macro, nullptr);
  EXPECT_EQ(macro->DefinitionText().text(), "\"bar\"");
  EXPECT_FALSE(macro->IsCallable());
  EXPECT_TRUE(macro->Parameters().empty());
}

TEST(VerilogPreprocessTest, OneMacroDefinitionOneParamWithValue) {
  PreprocessorTester tester(
      "module foo;\nendmodule\n"
      "`define FOOOO(x) (x+1)\n");
  EXPECT_PARSE_OK();

  const auto& definitions = tester.PreprocessorData().macro_definitions;
  EXPECT_THAT(definitions, ElementsAre(Pair("FOOOO", testing::_)));
  auto macro = FindOrNull(definitions, "FOOOO");
  ASSERT_NE(macro, nullptr);
  EXPECT_EQ(macro->DefinitionText().text(), "(x+1)");
  EXPECT_TRUE(macro->IsCallable());
  const auto& params = macro->Parameters();
  EXPECT_EQ(params.size(), 1);
  const auto& param(params[0]);
  EXPECT_EQ(param.name.text(), "x");
  EXPECT_FALSE(param.HasDefaultText());
}

TEST(VerilogPreprocessTest, OneMacroDefinitionOneParamDefaultWithValue) {
  PreprocessorTester tester(
      "module foo;\nendmodule\n"
      "`define FOOOO(x=22) (x+3)\n");
  EXPECT_PARSE_OK();

  const auto& definitions = tester.PreprocessorData().macro_definitions;
  EXPECT_THAT(definitions, ElementsAre(Pair("FOOOO", testing::_)));
  auto macro = FindOrNull(definitions, "FOOOO");
  ASSERT_NE(macro, nullptr);
  EXPECT_EQ(macro->DefinitionText().text(), "(x+3)");
  EXPECT_TRUE(macro->IsCallable());
  const auto& params = macro->Parameters();
  EXPECT_EQ(params.size(), 1);
  const auto& param(params[0]);
  EXPECT_EQ(param.name.text(), "x");
  EXPECT_TRUE(param.HasDefaultText());
  EXPECT_EQ(param.default_value.text(), "22");
}

TEST(VerilogPreprocessTest, TwoMacroDefinitions) {
  PreprocessorTester tester(
      "`define BAAAAR(y, z) (y*z)\n"
      "`define FOOOO(x=22) (x+3)\n");
  EXPECT_PARSE_OK();

  const auto& definitions = tester.PreprocessorData().macro_definitions;
  EXPECT_THAT(definitions, ElementsAre(Pair("BAAAAR", testing::_),
                                       Pair("FOOOO", testing::_)));
  {
    auto macro = FindOrNull(definitions, "BAAAAR");
    ASSERT_NE(macro, nullptr);
    EXPECT_TRUE(macro->IsCallable());
    const auto& params = macro->Parameters();
    EXPECT_EQ(params.size(), 2);
  }
  {
    auto macro = FindOrNull(definitions, "FOOOO");
    ASSERT_NE(macro, nullptr);
    EXPECT_TRUE(macro->IsCallable());
    const auto& params = macro->Parameters();
    EXPECT_EQ(params.size(), 1);
  }
}

TEST(VerilogPreprocessTest, UndefMacro) {
  PreprocessorTester tester(
      "`define FOO 42\n"
      "`undef FOO");
  EXPECT_PARSE_OK();

  const auto& definitions = tester.PreprocessorData().macro_definitions;
  EXPECT_EQ(definitions.size(), 0);
}

TEST(VerilogPreprocessTest, UndefNonexistentMacro) {
  PreprocessorTester tester("`undef FOO");
  EXPECT_PARSE_OK();

  const auto& definitions = tester.PreprocessorData().macro_definitions;
  EXPECT_EQ(definitions.size(), 0);
  EXPECT_TRUE(tester.PreprocessorData().warnings.empty());  // not a problem.
}

TEST(VerilogPreprocessTest, RedefineMacroWarning) {
  PreprocessorTester tester(
      "`define FOO 1\n"
      "`define FOO 2\n");
  EXPECT_PARSE_OK();

  const auto& definitions = tester.PreprocessorData().macro_definitions;
  EXPECT_EQ(definitions.size(), 1);

  const auto& warnings = tester.PreprocessorData().warnings;
  EXPECT_EQ(warnings.size(), 1);
  EXPECT_EQ(warnings.front().error_message, "Re-defining macro");
}

// We might have different modes later, in which we remove the define tokens
// from the stream. Document the current default which registeres all the
// defines, but also does not filter out the define calls.
TEST(VerilogPreprocessTest, DefaultPreprocessorKeepsDefineInStream) {
  PreprocessorTester tester(
      "`define FOO\n"
      "`define BAR(x) (x)\n"
      "module x(); endmodule\n");
  EXPECT_PARSE_OK();

  const auto& definitions = tester.PreprocessorData().macro_definitions;
  EXPECT_EQ(definitions.size(), 2);

  // The original `define tokens are still in the stream
  const auto& token_stream = tester.Data().GetTokenStreamView();
  const int count_defines =
      std::count_if(token_stream.begin(), token_stream.end(),
                    [](verible::TokenSequence::const_iterator t) {
                      return t->token_enum() == verilog_tokentype::PP_define;
                    });
  EXPECT_EQ(count_defines, 2);
}

struct BranchFailTest {
  absl::string_view input;
  int offset;
  absl::string_view expected_error;
};
TEST(VerilogPreprocessTest, IncompleteOrUnbalancedIfdef) {
  const BranchFailTest test_cases[] = {
      {"`endif", 0, "Unmatched `endif"},
      {"`else", 0, "Unmatched `else"},
      {"`elsif FOO", 0, "Unmatched `elsif"},
      {"`ifdef", 6, "unexpected EOF where expecting macro name"},
      {"`ifdef FOO\n`endif\n`endif", 18, "Unmatched `endif"},
      {"`ifdef FOO\n`endif\n`else", 18, "Unmatched `else"},
      {"`ifdef FOO\n`endif\n`elsif BAR", 18, "Unmatched `elsif"},
      {"`ifdef FOO\n`else\n`else", 17, "Duplicate `else"},
      {"`ifdef FOO\n`else\n`elsif BAR", 17, "`elsif after `else"},
      {"`ifdef FOO\n`ifdef BAR`else\n`else", 27, "Duplicate `else"},
      {"`ifdef FOO\n`else\n`ifdef BAR\n`endif", 11, "Unterminated preprocess"},
      {"`ifdef FOO\n`elsif BAR\n", 11, "Unterminated preprocessing"},
      {"`ifdef FOO\n`elsif BAR\n`else\n", 22, "Unterminated preprocessing"},
  };
  for (const BranchFailTest& test : test_cases) {
    PreprocessorTester tester(
        test.input, VerilogPreprocess::Config({.filter_branches = true}));

    EXPECT_FALSE(tester.Status().ok());
    ASSERT_GE(tester.PreprocessorData().errors.size(), 1);
    const auto& error = tester.PreprocessorData().errors.front();
    EXPECT_THAT(error.error_message, StartsWith(test.expected_error));
    const int error_token_offset =
        error.token_info.left(tester.Analyzer().Data().Contents());
    EXPECT_EQ(error_token_offset, test.offset) << "Input: " << test.input;
  }
}

struct RawAndFiltered {
  absl::string_view description;
  absl::string_view pp_input;
  absl::string_view equivalent;
};
TEST(VerilogPreprocess, FilterPPBranches) {
  const RawAndFiltered test_cases[] = {
      {"[** Defined macro taking ifdef branch **]",
       R"(
`define FOO 1
`ifdef FOO
  module bar();
`else
  module quux();
`endif
  endmodule)",
       // ...equivalent to
       R"(
`define FOO 1
 module bar();
 endmodule)"},

      {"[** Undefined macro taking else branch **]",
       R"(
`ifdef FOO
  module bar();
`else
  module quux();
`endif
  endmodule)",
       // ...equivalent to
       R"(
module quux();
endmodule)"},

      {"[** Undefined macro taking else branch. defined value `undef-ed **]",
       R"(
`define FOO
`undef FOO
`ifdef FOO
  module bar();
`else
  module quux();
`endif
  endmodule)",
       // ...equivalent to
       R"(
`define FOO
`undef FOO
module quux();
endmodule)"},

      {"[** Negative logic: Defined macro taking ifndef-else branch **]",
       R"(
`define FOO 1
`ifndef FOO
  module bar();
`else
  module quux();
`endif
  endmodule)",
       // ...equivalent to
       R"(
`define FOO 1
module quux();
endmodule)"},

      {"[** Negative logic: Undefined macro taking ifndef branch **]",
       R"(
`ifndef FOO
  module bar();
`else
  module quux();
`endif
  endmodule)",
       // ...equivalent to
       R"(
module bar();
endmodule)"},

      {"[** Elsif: choice of first branch **]",
       R"(
`define FOO 1
`ifdef FOO
  module foo(); endmodule
`elsif BAR
  module bar(); endmodule
`endif)",
       // ... equivalent to
       R"(
`define FOO 1
module foo(); endmodule)"},

      {"[** Elsif: choice of elsif branch **]",
       R"(
`define BAR 1
`ifdef FOO
  module foo(); endmodule
`elsif BAR
  module bar(); endmodule
`endif)",
       // ... equivalent to
       R"(
`define BAR 1
module bar(); endmodule)"},

      {"[** Elsif: no branch chosen **]",
       R"(
`define BAZ 1
`ifdef FOO
  module foo(); endmodule
`elsif BAR
  module bar(); endmodule
`endif)",
       // ... equivalent to
       R"(
`define BAZ 1
)"},

      {"[** Elsif: only first (`ifdef) matching branch chosen **]",
       R"(
`define FOO 1
`define BAR 1
`define BAZ 1
`ifdef FOO
  module foo(); endmodule
`elsif BAR
  module bar(); endmodule
`elsif BAZ
  module baz(); endmodule
`endif)",
       // ... equivalent to
       R"(
`define FOO 1
`define BAR 1
`define BAZ 1
module foo(); endmodule
)"},

      {"[** Elsif: only first (`elsif) matching branch chosen **]",
       R"(
`define BAR 1
`define BAZ 1
`define QUUX 1

`ifdef FOO
module foo(); endmodule
`elsif BAR
module bar(); endmodule
`elsif BAZ
module baz(); endmodule
`elsif QUUX
module quux(); endmodule
`endif)",
       // ... equivalent to
       R"(
`define BAR 1
`define BAZ 1
`define QUUX 1
module bar(); endmodule
)"},

      {"[** Nested conditions **]",
       R"(
`define BAR 1
`ifdef FOO
  module foo(); endmodule
 `ifdef BAR
   module foo_bar(); endmodule
 `else
   module foo_nonbar(); endmodule
 `endif
  module post_foo(); endmodule
`else
  module nonfoo(); endmodule
 `ifdef BAR
   module nonfoo_bar(); endmodule
 `else
   module nonfoo_nonbar(); endmodule;
 `endif
  module post_nonfoo(); endmodule
`endif)",
       // ... equivalent to
       R"(
`define BAR 1
module nonfoo(); endmodule
module nonfoo_bar(); endmodule
module post_nonfoo(); endmodule)"},

      {"[** Meta-def: Macro defined in branch controls another branch **]",
       R"(
`ifdef FOO
  `define BAR 1
  `undef FOOBAR
`else
  `define BAZ 1
  `undef FOOQUX
`endif

`ifdef BAR
module bar(); endmodule
`endif
`ifdef BAZ
module baz(); endmodule
`endif)",
       // ...equivalent to
       R"(
`define BAZ 1
`undef FOOQUX
module baz(); endmodule
)"}};

  for (const RawAndFiltered& test : test_cases) {
    PreprocessorTester with_filter(
        test.pp_input, VerilogPreprocess::Config({.filter_branches = true}));
    EXPECT_TRUE(with_filter.Status().ok())
        << with_filter.Status() << " " << test.description;
    PreprocessorTester equivalent(
        test.equivalent, VerilogPreprocess::Config({.filter_branches = false}));
    EXPECT_TRUE(equivalent.Status().ok())
        << equivalent.Status() << " " << test.description;

    const auto& filtered_stream = with_filter.Data().GetTokenStreamView();
    const auto& equivalent_stream = equivalent.Data().GetTokenStreamView();
    EXPECT_GT(filtered_stream.size(), 0) << test.description;
    EXPECT_EQ(filtered_stream.size(), equivalent_stream.size())
        << test.description;
    auto filtered_it = filtered_stream.begin();
    auto equivalent_it = equivalent_stream.begin();
    while (filtered_it != filtered_stream.end() &&
           equivalent_it != equivalent_stream.end()) {
      EXPECT_EQ((*filtered_it)->text(), (*equivalent_it)->text())
          << test.description;
      EXPECT_EQ((*filtered_it)->token_enum(), (*equivalent_it)->token_enum())
          << test.description;
      ++filtered_it;
      ++equivalent_it;
    }
  }
}

TEST(VerilogPreprocessTest, MacroExpansion) {
  const RawAndFiltered test_cases[] = {
      {"[** Multi-tokens macros being correctly parsed **]",
       R"(
`define ASSIGN1 =1
`define ASSIGN0 =0
module foo;
wire x`ASSIGN1;
wire y `ASSIGN0;
endmodule)",
       // ...equivalent to
       R"(
`define ASSIGN1 =1
`define ASSIGN0 =0
module foo;
wire x =1;
wire y =0;
endmodule)"},

      {"[** Multi-tokens macros not empty after undefing **]",
       R"(
`define XWIRE wire x
`define YWIRE wire y
module foo;
`XWIRE = 1;
`YWIRE = 0;
endmodule
`undef XWIRE
`undef YWIRE)",
       // ...equivalent to
       R"(
`define XWIRE wire x
`define YWIRE wire y
module foo;
wire x = 1;
wire y = 0;
endmodule
`undef XWIRE
`undef YWIRE)"},

      {"[** Macros that contain other macro calls, redefining the inner macro "
       "**]",
       R"(
`define XWIRE wire x
`define YWIRE wire y
`define ASSIGN1XWIRE `XWIRE = 1;
`define ASSIGN0YWIRE `YWIRE = 0;
module foo;
`ASSIGN1XWIRE
`ASSIGN0YWIRE
`define XWIRE wire new_x_wire
`ASSIGN1XWIRE
endmodule)",
       // ...equivalent to
       R"(
`define XWIRE wire x
`define YWIRE wire y
`define ASSIGN1XWIRE `XWIRE = 1;
`define ASSIGN0YWIRE `YWIRE = 0;
module foo;
wire x = 1;
wire y = 0;
`define XWIRE wire new_x_wire
wire new_x_wire = 1;
endmodule)"},

      {"[** Macros contatining back to back macro calls **]",
       R"(
`define XWIRE wire x
`define YWIRE wire y
`define ASSIGN1 = 1
`define ASSIGN0 = 0
`define ASSIGN1XWIRE `XWIRE `ASSIGN1;
`define ASSIGN0YWIRE `YWIRE `ASSIGN0;
module foo;
`ASSIGN1XWIRE
`ASSIGN0YWIRE
`define XWIRE wire new_x_wire
`ASSIGN1XWIRE
endmodule)",
       // ...equivalent to
       R"(
`define XWIRE wire x
`define YWIRE wire y
`define ASSIGN1 = 1
`define ASSIGN0 = 0
`define ASSIGN1XWIRE `XWIRE `ASSIGN1;
`define ASSIGN0YWIRE `YWIRE `ASSIGN0;
module foo;
wire x = 1;
wire y = 0;
`define XWIRE wire new_x_wire
wire new_x_wire = 1;
endmodule)"},

      {"[** Macros with formal parameters, expanded with both default value, "
       "and actual passed value **]",
       R"(
`define LSb(n=2) [n-1:0]
module testcase_ppMacro;
localparam int A = 123;
wire a = A`LSb();
wire b = A`LSb(5);
wire c = A[5-1:0]; 
endmodule)",
       // ...equivalent to
       R"(
`define LSb(n=2) [n-1:0]
module testcase_ppMacro;
localparam int A = 123;
wire a = A[2-1:0];
wire b = A[5-1:0];
wire c = A[5-1:0]; 
endmodule)"},

      {"[** Actual parameter is another macro call **]",
       R"(
`define FOO a
`define A(n) n
`define B(n=x) n ,y
module m;
wire `A(xyz);
wire `B(`FOO);
endmodule
`undef B
`undef A
`undef FOO)",
       // ...equivalent to
       R"(
`define FOO a
`define A(n) n
`define B(n=x) n ,y
module m;
wire xyz;
wire a ,y;
endmodule
`undef B
`undef A
`undef FOO)"},

      {"[** Multiple parameter macros (From 2017 SV-LRM) **]",
       R"(
`define MACRO1(a=5,b="B",c) $display(a,,b,,c);
`define MACRO2(a=5, b, c="C") $display(a,,b,,c);
`define MACRO3(a=5, b=0, c="C") $display(a,,b,,c);
module m;
`MACRO1 ( , 2, 3 )
`MACRO1 ( 1 , , 3 ) 
`MACRO1 ( , 2, )
`MACRO2 (1, , 3)
`MACRO2 (, 2, )
`MACRO2 (, 2)
`MACRO3 ( 1 )
`MACRO3 ()
endmodule
`undef MACRO)",
       // ...equivalent to
       R"(
`define MACRO1(a=5,b="B",c) $display(a,,b,,c);
`define MACRO2(a=5, b, c="C") $display(a,,b,,c);
`define MACRO3(a=5, b=0, c="C") $display(a,,b,,c);
module m;
$display(5,,2,,3);
$display(1,,"B",,3);
$display(5,,2,,);
$display(1,,,,3);
$display(5,,2,,"C");
$display(5,,2,,"C");
$display(1,,0,,"C");
$display(5,,0,,"C");
endmodule
`undef MACRO)"},

      {"[** Nested callable macros **]",
       R"(
`define MACRO1(n) real x=n;
`define MACRO2(m) real y=m; `MACRO1(1)
module foo;
`MACRO1(2)
`MACRO2(3)
endmodule
`undef MACRO1
`undef MACRO2)",
       // ...equivalent to
       R"(
`define MACRO1(n) real x=n;
`define MACRO2(m) real y=m; `MACRO1(1)
module foo;
real x=2;
real y=3; real x=1;
endmodule
`undef MACRO1
`undef MACRO2)"}

  };

  for (const auto& test_case : test_cases) {
    PreprocessorTester expanded(
        test_case.pp_input, VerilogPreprocess::Config({.expand_macros = true}));
    EXPECT_TRUE(expanded.Status().ok())
        << expanded.Status() << " " << test_case.description;
    PreprocessorTester equivalent(
        test_case.equivalent,
        VerilogPreprocess::Config({.expand_macros = false}));
    EXPECT_TRUE(equivalent.Status().ok())
        << equivalent.Status() << " " << test_case.description;
    const auto& expanded_stream = expanded.Data().GetTokenStreamView();
    const auto& equivalent_stream = equivalent.Data().GetTokenStreamView();
    EXPECT_GT(expanded_stream.size(), 0) << test_case.description;
    EXPECT_EQ(expanded_stream.size(), equivalent_stream.size())
        << test_case.description;
    auto expanded_it = expanded_stream.begin();
    auto equivalent_it = equivalent_stream.begin();
    while (expanded_it != expanded_stream.end() &&
           equivalent_it != equivalent_stream.end()) {
      EXPECT_EQ((*expanded_it)->text(), (*equivalent_it)->text())
          << test_case.description;
      EXPECT_EQ((*expanded_it)->token_enum(), (*equivalent_it)->token_enum())
          << test_case.description;
      ++expanded_it;
      ++equivalent_it;
    }
  }
}

}  // namespace
}  // namespace verilog
