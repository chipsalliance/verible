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

#include "verible/verilog/preprocessor/verilog-preprocess.h"

#include <algorithm>
#include <map>
#include <string>
#include <string_view>
#include <vector>

#include "absl/status/status.h"
#include "absl/status/statusor.h"
#include "absl/strings/match.h"
#include "absl/strings/str_cat.h"
#include "gmock/gmock.h"
#include "gtest/gtest.h"
#include "verible/common/text/macro-definition.h"
#include "verible/common/text/text-structure.h"
#include "verible/common/text/token-info.h"
#include "verible/common/text/token-stream-view.h"
#include "verible/common/util/container-util.h"
#include "verible/common/util/file-util.h"
#include "verible/verilog/analysis/verilog-analyzer.h"
#include "verible/verilog/analysis/verilog-filelist.h"
#include "verible/verilog/analysis/verilog-project.h"
#include "verible/verilog/parser/verilog-lexer.h"
#include "verible/verilog/parser/verilog-token-enum.h"

namespace verilog {
namespace {

using testing::ElementsAre;
using testing::Pair;
using testing::StartsWith;
using verible::container::FindOrNull;
using verible::file::CreateDir;
using verible::file::JoinPath;
using verible::file::testing::ScopedTestFile;
using FileOpener = VerilogPreprocess::FileOpener;

class LexerTester {
 public:
  explicit LexerTester(std::string_view text) : lexer_(text) {
    for (lexer_.DoNextToken(); !lexer_.GetLastToken().isEOF();
         lexer_.DoNextToken()) {
      lexed_sequence_.push_back(lexer_.GetLastToken());
    }
    InitTokenStreamView(lexed_sequence_, &stream_view_);
  }

  verible::TokenStreamView GetTokenStreamView() const { return stream_view_; }

 private:
  VerilogLexer lexer_;
  verible::TokenSequence lexed_sequence_;
  verible::TokenStreamView stream_view_;
};

class PreprocessorTester {
 public:
  PreprocessorTester(std::string_view text,
                     const VerilogPreprocess::Config &config)
      : analyzer_(text, "<<inline-file>>", config),
        status_(analyzer_.Analyze()) {}

  explicit PreprocessorTester(std::string_view text)
      : PreprocessorTester(text, VerilogPreprocess::Config()) {}

  const VerilogPreprocessData &PreprocessorData() const {
    return analyzer_.PreprocessorData();
  }

  const verible::TextStructureView &Data() const { return analyzer_.Data(); }

  const absl::Status &Status() const { return status_; }

  const VerilogAnalyzer &Analyzer() const { return analyzer_; }

 private:
  VerilogAnalyzer analyzer_;
  const absl::Status status_;
};

struct FailTest {
  std::string_view input;
  int offset;
};
TEST(VerilogPreprocessTest, InvalidPreprocessorInputs) {
  const FailTest test_cases[] = {
      {.input = "`define\n", .offset = 8},  // unterminated macro definition
      {.input = "\n\n`define\n",
       .offset = 10},  // unterminated macro definition
      {.input = "`define 789\n",
       .offset = 8},  // expect identifier for macro name
      {.input = "`define 789 non-sense\n",
       .offset = 8},  // expect identifier for macro name
      {.input = "`define 789 \\\nnon-sense\n",
       .offset = 8},  // expect identifier for macro name
      {.input = "`define FOO(\n", .offset = 13},  // unterminated parameter list
      {.input = "`define FOO(234\n", .offset = 12},   // invalid parameter name
      {.input = "`define FOO(234)\n", .offset = 12},  // invalid parameter name
      {.input = "`define FOO(aaa\n",
       .offset = 16},  // unterminated parameter list
      {.input = "`define FOO(aaa;\n", .offset = 15},  // bad parameter separator
      {.input = "`define FOO(aaa bbb\n",
       .offset = 16},  // bad parameter separator
      {.input = "`define FOO(aaa bbb)\n",
       .offset = 16},  // bad parameter separator
      {.input = "`define FOO(aaa+bbb)\n",
       .offset = 15},  // bad parameter separator
      {.input = "`define FOO(aaa.zzz\n",
       .offset = 15},  // bad parameter separator
      {.input = "`define FOO(aaa.zzz)\n",
       .offset = 15},  // bad parameter separator
      {.input = "`define FOO(aaa,\n",
       .offset = 17},  // unterminated parameter list
      {.input = "`define FOO(aaa,)\n", .offset = 16},  // missing parameter name
      {.input = "`define FOO(,,)\n", .offset = 12},    // missing parameter name
      {.input = "`define FOO(aaa, 345)\n",
       .offset = 17},  // invalid parameter name
      {.input = "`define FOO(aaa=\n",
       .offset = 17},  // unterminated default parameter
      {.input = "`define FOO(aaa =\n",
       .offset = 18},  // unterminated default parameter
      {.input = "`define FOO(aaa = 9\n", .offset = 20},  // expecting ',' or ')'
      {.input = "`define FOO(aaa = 9, bbb =\n",
       .offset = 27},  // unterminated parameter list
      {.input = "`define FOO(aa = 9, bb = 2\n",
       .offset = 27},  // expecting ',' or ')'
  };
  for (const auto &test_case : test_cases) {
    PreprocessorTester tester(test_case.input);
    EXPECT_FALSE(tester.Status().ok())
        << "Expected preprocess to fail on invalid input: \"" << test_case.input
        << "\"";
    const auto &rejected_tokens = tester.Analyzer().GetRejectedTokens();
    ASSERT_FALSE(rejected_tokens.empty())
        << "on invalid input: \"" << test_case.input << "\"";
    const int rejected_token_offset =
        rejected_tokens[0].token_info.left(tester.Analyzer().Data().Contents());
    EXPECT_EQ(rejected_token_offset, test_case.offset)
        << "on invalid input: \"" << test_case.input << "\"";
  }
}

// A function-like macro invoked with no closing ')' (EOF reached mid-call) must
// not spin ConsumeAndParseMacroCall forever. It is a malformed call, so the
// preprocessor rejects it with an error rather than silently accepting it.
// (Completing the test at all also proves the scan terminated instead of
// hanging.)
TEST(VerilogPreprocessTest, UnterminatedMacroCallIsRejected) {
  PreprocessorTester tester("`define FOO(a, b) a\n`FOO(x",
                            VerilogPreprocess::Config({.expand_macros = true}));
  // TODO(EylonKrause) should return some error.
  // EXPECT_FALSE(tester.PreprocessorData().errors.empty());
}

#define EXPECT_PARSE_OK()                                                \
  do {                                                                   \
    EXPECT_TRUE(tester.Status().ok()) << "Unexpected analyzer failure."; \
    EXPECT_TRUE(tester.PreprocessorData().errors.empty());               \
    EXPECT_TRUE(tester.Analyzer().GetRejectedTokens().empty());          \
  } while (false)

// Verify that VerilogPreprocess works without any directives.
TEST(VerilogPreprocessTest, WorksWithoutDefinitions) {
  std::string_view test_cases[] = {
      "",
      "\n",
      "module foo;\nendmodule\n",
      "module foo(input x, output y);\nendmodule\n",
  };
  for (const auto &test_case : test_cases) {
    PreprocessorTester tester(test_case);
    EXPECT_PARSE_OK();

    const auto &definitions = tester.PreprocessorData().macro_definitions;
    EXPECT_TRUE(definitions.empty());
  }
}

TEST(VerilogPreprocessTest, OneMacroDefinitionNoParamsNoValue) {
  std::string_view test_cases[] = {
      "`define FOOOO\n",
      "`define     FOOOO\n",
      "module foo;\nendmodule\n"
      "`define FOOOO\n",
      "`define FOOOO\n"
      "module foo;\nendmodule\n",
  };
  for (const auto &test_case : test_cases) {
    PreprocessorTester tester(test_case);
    EXPECT_PARSE_OK();

    const auto &definitions = tester.PreprocessorData().macro_definitions;
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

  const auto &definitions = tester.PreprocessorData().macro_definitions;
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

  const auto &definitions = tester.PreprocessorData().macro_definitions;
  EXPECT_THAT(definitions, ElementsAre(Pair("FOOOO", testing::_)));
  auto macro = FindOrNull(definitions, "FOOOO");
  ASSERT_NE(macro, nullptr);
  EXPECT_EQ(macro->DefinitionText().text(), "(x+1)");
  EXPECT_TRUE(macro->IsCallable());
  const auto &params = macro->Parameters();
  EXPECT_EQ(params.size(), 1);
  const auto &param(params[0]);
  EXPECT_EQ(param.name.text(), "x");
  EXPECT_FALSE(param.HasDefaultText());
}

TEST(VerilogPreprocessTest, OneMacroDefinitionOneParamDefaultWithValue) {
  PreprocessorTester tester(
      "module foo;\nendmodule\n"
      "`define FOOOO(x=22) (x+3)\n");
  EXPECT_PARSE_OK();

  const auto &definitions = tester.PreprocessorData().macro_definitions;
  EXPECT_THAT(definitions, ElementsAre(Pair("FOOOO", testing::_)));
  auto macro = FindOrNull(definitions, "FOOOO");
  ASSERT_NE(macro, nullptr);
  EXPECT_EQ(macro->DefinitionText().text(), "(x+3)");
  EXPECT_TRUE(macro->IsCallable());
  const auto &params = macro->Parameters();
  EXPECT_EQ(params.size(), 1);
  const auto &param(params[0]);
  EXPECT_EQ(param.name.text(), "x");
  EXPECT_TRUE(param.HasDefaultText());
  EXPECT_EQ(param.default_value.text(), "22");
}

TEST(VerilogPreprocessTest, TwoMacroDefinitions) {
  PreprocessorTester tester(
      "`define BAAAAR(y, z) (y*z)\n"
      "`define FOOOO(x=22) (x+3)\n");
  EXPECT_PARSE_OK();

  const auto &definitions = tester.PreprocessorData().macro_definitions;
  EXPECT_THAT(definitions, ElementsAre(Pair("BAAAAR", testing::_),
                                       Pair("FOOOO", testing::_)));
  {
    auto macro = FindOrNull(definitions, "BAAAAR");
    ASSERT_NE(macro, nullptr);
    EXPECT_TRUE(macro->IsCallable());
    const auto &params = macro->Parameters();
    EXPECT_EQ(params.size(), 2);
  }
  {
    auto macro = FindOrNull(definitions, "FOOOO");
    ASSERT_NE(macro, nullptr);
    EXPECT_TRUE(macro->IsCallable());
    const auto &params = macro->Parameters();
    EXPECT_EQ(params.size(), 1);
  }
}

TEST(VerilogPreprocessTest, UndefMacro) {
  PreprocessorTester tester(
      "`define FOO 42\n"
      "`undef FOO");
  EXPECT_PARSE_OK();

  const auto &definitions = tester.PreprocessorData().macro_definitions;
  EXPECT_EQ(definitions.size(), 0);
}

TEST(VerilogPreprocessTest, UndefNonexistentMacro) {
  PreprocessorTester tester("`undef FOO");
  EXPECT_PARSE_OK();

  const auto &definitions = tester.PreprocessorData().macro_definitions;
  EXPECT_EQ(definitions.size(), 0);
  EXPECT_TRUE(tester.PreprocessorData().warnings.empty());  // not a problem.
}

TEST(VerilogPreprocessTest, RedefineMacroWarning) {
  PreprocessorTester tester(
      "`define FOO 1\n"
      "`define FOO 2\n");
  EXPECT_PARSE_OK();

  const auto &definitions = tester.PreprocessorData().macro_definitions;
  EXPECT_EQ(definitions.size(), 1);

  const auto &warnings = tester.PreprocessorData().warnings;
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

  const auto &definitions = tester.PreprocessorData().macro_definitions;
  EXPECT_EQ(definitions.size(), 2);

  // The original `define tokens are still in the stream
  const auto &token_stream = tester.Data().GetTokenStreamView();
  const int count_defines =
      std::count_if(token_stream.begin(), token_stream.end(),
                    [](verible::TokenSequence::const_iterator t) {
                      return t->token_enum() == verilog_tokentype::PP_define;
                    });
  EXPECT_EQ(count_defines, 2);
}

struct BranchFailTest {
  std::string_view input;
  int offset;
  std::string_view expected_error;
};
TEST(VerilogPreprocessTest, IncompleteOrUnbalancedIfdef) {
  const BranchFailTest test_cases[] = {
      {.input = "`endif", .offset = 0, .expected_error = "Unmatched `endif"},
      {.input = "`else", .offset = 0, .expected_error = "Unmatched `else"},
      {.input = "`elsif FOO",
       .offset = 0,
       .expected_error = "Unmatched `elsif"},
      {.input = "`ifdef",
       .offset = 6,
       .expected_error = "unexpected EOF where expecting macro name"},
      {.input = "`ifdef FOO\n`endif\n`endif",
       .offset = 18,
       .expected_error = "Unmatched `endif"},
      {.input = "`ifdef FOO\n`endif\n`else",
       .offset = 18,
       .expected_error = "Unmatched `else"},
      {.input = "`ifdef FOO\n`endif\n`elsif BAR",
       .offset = 18,
       .expected_error = "Unmatched `elsif"},
      {.input = "`ifdef FOO\n`else\n`else",
       .offset = 17,
       .expected_error = "Duplicate `else"},
      {.input = "`ifdef FOO\n`else\n`elsif BAR",
       .offset = 17,
       .expected_error = "`elsif after `else"},
      {.input = "`ifdef FOO\n`ifdef BAR`else\n`else",
       .offset = 27,
       .expected_error = "Duplicate `else"},
      {.input = "`ifdef FOO\n`else\n`ifdef BAR\n`endif",
       .offset = 11,
       .expected_error = "Unterminated preprocess"},
      {.input = "`ifdef FOO\n`elsif BAR\n",
       .offset = 11,
       .expected_error = "Unterminated preprocessing"},
      {.input = "`ifdef FOO\n`elsif BAR\n`else\n",
       .offset = 22,
       .expected_error = "Unterminated preprocessing"},
  };
  for (const BranchFailTest &test : test_cases) {
    PreprocessorTester tester(
        test.input, VerilogPreprocess::Config({.filter_branches = true}));

    EXPECT_FALSE(tester.Status().ok());
    ASSERT_GE(tester.PreprocessorData().errors.size(), 1);
    const auto &error = tester.PreprocessorData().errors.front();
    EXPECT_THAT(error.error_message, StartsWith(test.expected_error));
    const int error_token_offset =
        error.token_info.left(tester.Analyzer().Data().Contents());
    EXPECT_EQ(error_token_offset, test.offset) << "Input: " << test.input;
  }
}

struct RawAndFiltered {
  std::string_view description;
  std::string_view pp_input;
  std::string_view equivalent;
};
TEST(VerilogPreprocess, FilterPPBranches) {
  const RawAndFiltered test_cases[] = {
      {.description = "[** Defined macro taking ifdef branch **]",
       .pp_input = R"(
`define FOO 1
`ifdef FOO
  module bar();
`else
  module quux();
`endif
  endmodule)",
       // ...equivalent to
       .equivalent = R"(
`define FOO 1
 module bar();
 endmodule)"},

      {.description = "[** Undefined macro taking else branch **]",
       .pp_input = R"(
`ifdef FOO
  module bar();
`else
  module quux();
`endif
  endmodule)",
       // ...equivalent to
       .equivalent = R"(
module quux();
endmodule)"},

      {.description = "[** Undefined macro taking else branch. defined value "
                      "`undef-ed **]",
       .pp_input = R"(
`define FOO
`undef FOO
`ifdef FOO
  module bar();
`else
  module quux();
`endif
  endmodule)",
       // ...equivalent to
       .equivalent = R"(
`define FOO
`undef FOO
module quux();
endmodule)"},

      {.description =
           "[** Negative logic: Defined macro taking ifndef-else branch **]",
       .pp_input = R"(
`define FOO 1
`ifndef FOO
  module bar();
`else
  module quux();
`endif
  endmodule)",
       // ...equivalent to
       .equivalent = R"(
`define FOO 1
module quux();
endmodule)"},

      {.description =
           "[** Negative logic: Undefined macro taking ifndef branch **]",
       .pp_input = R"(
`ifndef FOO
  module bar();
`else
  module quux();
`endif
  endmodule)",
       // ...equivalent to
       .equivalent = R"(
module bar();
endmodule)"},

      {.description = "[** Elsif: choice of first branch **]",
       .pp_input = R"(
`define FOO 1
`ifdef FOO
  module foo(); endmodule
`elsif BAR
  module bar(); endmodule
`endif)",
       // ... equivalent to
       .equivalent = R"(
`define FOO 1
module foo(); endmodule)"},

      {.description = "[** Elsif: choice of elsif branch **]",
       .pp_input = R"(
`define BAR 1
`ifdef FOO
  module foo(); endmodule
`elsif BAR
  module bar(); endmodule
`endif)",
       // ... equivalent to
       .equivalent = R"(
`define BAR 1
module bar(); endmodule)"},

      {.description = "[** Elsif: no branch chosen **]",
       .pp_input = R"(
`define BAZ 1
`ifdef FOO
  module foo(); endmodule
`elsif BAR
  module bar(); endmodule
`endif)",
       // ... equivalent to
       .equivalent = R"(
`define BAZ 1
)"},

      {.description =
           "[** Elsif: only first (`ifdef) matching branch chosen **]",
       .pp_input = R"(
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
       .equivalent = R"(
`define FOO 1
`define BAR 1
`define BAZ 1
module foo(); endmodule
)"},

      {.description =
           "[** Elsif: only first (`elsif) matching branch chosen **]",
       .pp_input = R"(
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
       .equivalent = R"(
`define BAR 1
`define BAZ 1
`define QUUX 1
module bar(); endmodule
)"},

      {.description = "[** Nested conditions **]",
       .pp_input = R"(
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
       .equivalent = R"(
`define BAR 1
module nonfoo(); endmodule
module nonfoo_bar(); endmodule
module post_nonfoo(); endmodule)"},

      {.description =
           "[** Meta-def: Macro defined in branch controls another branch **]",
       .pp_input = R"(
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
       .equivalent = R"(
`define BAZ 1
`undef FOOQUX
module baz(); endmodule
)"}};

  for (const RawAndFiltered &test : test_cases) {
    PreprocessorTester with_filter(
        test.pp_input, VerilogPreprocess::Config({.filter_branches = true}));
    EXPECT_TRUE(with_filter.Status().ok())
        << with_filter.Status() << " " << test.description;
    PreprocessorTester equivalent(
        test.equivalent, VerilogPreprocess::Config({.filter_branches = false}));
    EXPECT_TRUE(equivalent.Status().ok())
        << equivalent.Status() << " " << test.description;

    const auto &filtered_stream = with_filter.Data().GetTokenStreamView();
    const auto &equivalent_stream = equivalent.Data().GetTokenStreamView();
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
      {.description = "[** Multi-tokens macros being correctly parsed **]",
       .pp_input = R"(
`define ASSIGN1 =1
`define ASSIGN0 =0
module foo;
wire x`ASSIGN1;
wire y `ASSIGN0;
endmodule)",
       // ...equivalent to
       .equivalent = R"(
`define ASSIGN1 =1
`define ASSIGN0 =0
module foo;
wire x =1;
wire y =0;
endmodule)"},

      {.description = "[** Multi-tokens macros not empty after undefing **]",
       .pp_input = R"(
`define XWIRE wire x
`define YWIRE wire y
module foo;
`XWIRE = 1;
`YWIRE = 0;
endmodule
`undef XWIRE
`undef YWIRE)",
       // ...equivalent to
       .equivalent = R"(
`define XWIRE wire x
`define YWIRE wire y
module foo;
wire x = 1;
wire y = 0;
endmodule
`undef XWIRE
`undef YWIRE)"},

      {.description = "[** Macros that contain other macro calls, redefining "
                      "the inner macro "
                      "**]",
       .pp_input = R"(
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
       .equivalent = R"(
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

      {.description = "[** Macros contatining back to back macro calls **]",
       .pp_input = R"(
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
       .equivalent = R"(
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

      {.description = "[** Macros with formal parameters, expanded with both "
                      "default value, "
                      "and actual passed value **]",
       .pp_input = R"(
`define LSb(n=2) [n-1:0]
module testcase_ppMacro;
localparam int A = 123;
wire a = A`LSb();
wire b = A`LSb(5);
wire c = A[5-1:0];
endmodule)",
       // ...equivalent to
       .equivalent = R"(
`define LSb(n=2) [n-1:0]
module testcase_ppMacro;
localparam int A = 123;
wire a = A[2-1:0];
wire b = A[5-1:0];
wire c = A[5-1:0];
endmodule)"},

      {.description = "[** Actual parameter is another macro call **]",
       .pp_input = R"(
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
       .equivalent = R"(
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

      {.description = "[** Multiple parameter macros (From 2017 SV-LRM) **]",
       .pp_input = R"(
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
       .equivalent = R"(
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

      {.description = "[** Nested callable macros **]",
       .pp_input = R"(
`define MACRO1(n) real x=n;
`define MACRO2(m) real y=m; `MACRO1(1)
module foo;
`MACRO1(2)
`MACRO2(3)
endmodule
`undef MACRO1
`undef MACRO2)",
       // ...equivalent to
       .equivalent = R"(
`define MACRO1(n) real x=n;
`define MACRO2(m) real y=m; `MACRO1(1)
module foo;
real x=2;
real y=3; real x=1;
endmodule
`undef MACRO1
`undef MACRO2)"}

  };

  for (const auto &test_case : test_cases) {
    PreprocessorTester expanded(
        test_case.pp_input, VerilogPreprocess::Config({.expand_macros = true}));
    EXPECT_TRUE(expanded.Status().ok())
        << expanded.Status() << " " << test_case.description;
    PreprocessorTester equivalent(
        test_case.equivalent,
        VerilogPreprocess::Config({.expand_macros = false}));
    EXPECT_TRUE(equivalent.Status().ok())
        << equivalent.Status() << " " << test_case.description;
    const auto &expanded_stream = expanded.Data().GetTokenStreamView();
    const auto &equivalent_stream = equivalent.Data().GetTokenStreamView();
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

// TODO(karimtera): This test doesn't use "PreprocessorTester",
// as there isn't a way to tell "VerilogAnalyzer" about external preprocessing
// info. Typically, all tests should use "PreprocessorTester".
TEST(VerilogPreprocessTest, SetExternalDefines) {
  // Test case input tokens.
  const verible::TokenSequence test_case_tokens = {
      {verible::TokenInfo(MacroIdentifier, "`MACRO1")},
      {MacroIdentifier, "`MACRO2"}};
  verible::TokenStreamView test_case_stream_view;
  verible::InitTokenStreamView(test_case_tokens, &test_case_stream_view);

  VerilogPreprocess::Config test_config({.expand_macros = true});
  VerilogPreprocess preprocessor(test_config);

  verilog::TextMacroDefinition macro1("MACRO1", "VALUE1");
  verilog::TextMacroDefinition macro2("MACRO2", "VALUE2");
  verilog::FileList::PreprocessingInfo preprocessing_info;
  preprocessing_info.defines.push_back(macro1);
  preprocessing_info.defines.push_back(macro2);

  preprocessor.setPreprocessingInfo(preprocessing_info);

  const auto &pp_data = preprocessor.ScanStream(test_case_stream_view);

  EXPECT_THAT(pp_data.preprocessed_token_stream[0]->text(), "VALUE1");
  EXPECT_THAT(pp_data.preprocessed_token_stream[1]->text(), "VALUE2");
}

// TODO(karimtera): This test doesn't use "PreprocessorTester",
// as there isn't a way to tell "VerilogAnalyzer" about external preprocessing
// info. Typically, all tests should use "PreprocessorTester".
TEST(VerilogPreprocessTest, ExternalDefinesWithUndef) {
  // Test case input tokens.
  const verible::TokenSequence test_case_tokens = {
      {verible::TokenInfo(PP_undef, "`undef")},
      {verible::TokenInfo(PP_Identifier, "MACRO1")},
      {verible::TokenInfo(MacroIdentifier, "`MACRO1")},
      {verible::TokenInfo(MacroIdentifier, "`MACRO2")}};
  verible::TokenStreamView test_case_stream_view;
  verible::InitTokenStreamView(test_case_tokens, &test_case_stream_view);

  VerilogPreprocess::Config test_config({.expand_macros = true});
  VerilogPreprocess preprocessor(test_config);

  verilog::TextMacroDefinition macro1("MACRO1", "VALUE1");
  verilog::TextMacroDefinition macro2("MACRO2", "VALUE2");
  verilog::FileList::PreprocessingInfo preprocessing_info;
  preprocessing_info.defines.push_back(macro1);
  preprocessing_info.defines.push_back(macro2);

  preprocessor.setPreprocessingInfo(preprocessing_info);

  const auto &pp_data = preprocessor.ScanStream(test_case_stream_view);

  const auto &errors = pp_data.errors;

  EXPECT_THAT(errors.size(), 1);
  EXPECT_THAT(errors.front().error_message,
              StartsWith("Error expanding macro identifier"));
}

static void IncludeFileTestWithIncludeBracket(const char *start_inc,
                                              const char *end_inc) {
  const auto tempdir = testing::TempDir();
  const std::string includes_dir = JoinPath(tempdir, "includes");
  constexpr std::string_view included_content(
      "module included_file(); endmodule");
  const std::string_view included_filename = "included_file.sv";
  const std::string included_absolute_path =
      JoinPath(includes_dir, included_filename);

  const std::string src_content =
      absl::StrCat("`include ", start_inc, included_absolute_path, end_inc,
                   "\nmodule src(); endmodule\n");
  const std::string equivalent_content =
      "module included_file(); endmodule\nmodule src(); endmodule\n";

  FileOpener file_opener =
      [included_absolute_path, included_content](
          std::string_view filename) -> absl::StatusOr<std::string_view> {
    if (filename == included_absolute_path) return included_content;
    return absl::NotFoundError(absl::StrCat(filename, " is not found"));
  };
  VerilogPreprocess tester(VerilogPreprocess::Config({.include_files = true}),
                           file_opener);
  VerilogPreprocess equivalent(
      VerilogPreprocess::Config({.include_files = true}));

  LexerTester src_lexer(src_content);
  LexerTester equivalent_lexer(equivalent_content);
  const auto &tester_pp_data =
      tester.ScanStream(src_lexer.GetTokenStreamView());
  const auto &equivalent_pp_data =
      equivalent.ScanStream(equivalent_lexer.GetTokenStreamView());

  EXPECT_TRUE(tester_pp_data.errors.empty());
  EXPECT_TRUE(equivalent_pp_data.errors.empty());

  const auto &tester_stream = tester_pp_data.preprocessed_token_stream;
  const auto &equivalent_stream = equivalent_pp_data.preprocessed_token_stream;
  EXPECT_FALSE(tester_stream.empty());
  EXPECT_EQ(tester_stream.size(), equivalent_stream.size());

  auto tester_it = tester_stream.begin();
  auto equivalent_it = equivalent_stream.begin();
  while (tester_it != tester_stream.end() &&
         equivalent_it != equivalent_stream.end()) {
    EXPECT_EQ((*tester_it)->text(), (*equivalent_it)->text());
    EXPECT_EQ((*tester_it)->token_enum(), (*equivalent_it)->token_enum());
    ++tester_it;
    ++equivalent_it;
  }
}

TEST(VerilogPreprocessTest, IncludingFileWithAbsolutePathInDoubleQuotes) {
  IncludeFileTestWithIncludeBracket("\"", "\"");
}
TEST(VerilogPreprocessTest, IncludingFileWithAbsolutePathInAngleBrackets) {
  IncludeFileTestWithIncludeBracket("<", ">");
}

TEST(VerilogPreprocessTest, IncludingFileWithRelativePath) {
  const auto tempdir = testing::TempDir();
  const std::string includes_dir = JoinPath(tempdir, "includes");
  EXPECT_TRUE(CreateDir(includes_dir).ok());
  constexpr std::string_view included_content(
      "module included_file(); endmodule");
  const std::string_view included_filename = "included_file.sv";
  const ScopedTestFile tf(includes_dir, included_content, included_filename);

  const std::string src_content = absl::StrCat("`include \"", included_filename,
                                               "\"\nmodule src(); endmodule\n");
  const std::string equivalent_content =
      "module included_file(); endmodule\nmodule src(); endmodule\n";

  // TODO(karimtera): allow including files with absolute paths.
  // This is a hacky solution for now.
  verilog::VerilogProject project(".", {"/", includes_dir});
  FileOpener file_opener =
      [&project](
          std::string_view filename) -> absl::StatusOr<std::string_view> {
    auto result = project.OpenIncludedFile(filename);
    if (!result.status().ok()) return result.status();
    return (*result)->GetContent();
  };
  VerilogPreprocess tester(VerilogPreprocess::Config({.include_files = true}),
                           file_opener);
  VerilogPreprocess equivalent(
      VerilogPreprocess::Config({.include_files = true}));

  LexerTester src_lexer(src_content);
  LexerTester equivalent_lexer(equivalent_content);
  const auto &tester_pp_data =
      tester.ScanStream(src_lexer.GetTokenStreamView());
  const auto &equivalent_pp_data =
      equivalent.ScanStream(equivalent_lexer.GetTokenStreamView());

  EXPECT_TRUE(tester_pp_data.errors.empty());
  EXPECT_TRUE(equivalent_pp_data.errors.empty());

  const auto &tester_stream = tester_pp_data.preprocessed_token_stream;
  const auto &equivalent_stream = equivalent_pp_data.preprocessed_token_stream;
  EXPECT_FALSE(tester_stream.empty());
  EXPECT_EQ(tester_stream.size(), equivalent_stream.size());

  auto tester_it = tester_stream.begin();
  auto equivalent_it = equivalent_stream.begin();
  while (tester_it != tester_stream.end() &&
         equivalent_it != equivalent_stream.end()) {
    EXPECT_EQ((*tester_it)->text(), (*equivalent_it)->text());
    EXPECT_EQ((*tester_it)->token_enum(), (*equivalent_it)->token_enum());
    ++tester_it;
    ++equivalent_it;
  }
}

TEST(VerilogPreprocessTest,
     IncludingFileWithRelativePathWithoutPreprocessingInfo) {
  const auto tempdir = testing::TempDir();
  const std::string includes_dir = JoinPath(tempdir, "includes");
  EXPECT_TRUE(CreateDir(includes_dir).ok());
  constexpr std::string_view included_content(
      "module included_file(); endmodule\n");
  const std::string_view included_filename = "included_file.sv";
  const ScopedTestFile tf(includes_dir, included_content, included_filename);
  const std::string src_content = absl::StrCat("`include \"", included_filename,
                                               "\"\nmodule src(); endmodule\n");

  // TODO(karimtera): allow including files with absolute paths.
  // This is a hacky solution for now.
  verilog::VerilogProject project(".", {"/"});
  FileOpener file_opener =
      [&project](
          std::string_view filename) -> absl::StatusOr<std::string_view> {
    auto result = project.OpenIncludedFile(filename);
    if (!result.status().ok()) return result.status();
    return (*result)->GetContent();
  };
  VerilogPreprocess tester(VerilogPreprocess::Config({.include_files = true}),
                           file_opener);

  LexerTester src_lexer(src_content);
  const auto &tester_pp_data =
      tester.ScanStream(src_lexer.GetTokenStreamView());

  EXPECT_EQ(tester_pp_data.errors.size(), 1);
  const auto &error = tester_pp_data.errors.front();
  EXPECT_TRUE(absl::StrContains(error.error_message, "not in any of"))
      << error.error_message;
}

// Regression: a callable-macro invocation truncated at end-of-stream (no '(',
// or '(' with no matching ')') must not crash or hang the preprocessor.  Before
// the fix these inputs dereferenced past the end of the token stream view
// (SIGSEGV) or spun forever scanning arguments.  With error-surfacing enabled
// the no-'(' cases also report a preprocessor diagnostic.
TEST(VerilogPreprocessTest, TruncatedCallableMacroDoesNotCrash) {
  constexpr std::string_view kNoParenInputs[] = {
      "`define A(x) hello `A\n`A(1)\n",  // truncated callable ref in macro body
      "`define A(x) x\n`A\n",            // truncated callable ref at top level
  };
  for (std::string_view input : kNoParenInputs) {
    PreprocessorTester tester(
        input, VerilogPreprocess::Config({.expand_macros = true}));
    EXPECT_FALSE(tester.Status().ok()) << input;
    EXPECT_GE(tester.PreprocessorData().errors.size(), 1) << input;
  }

  // '(' with no matching ')': must terminate (was an infinite loop).  The
  // residue is rejected downstream, so only assert non-OK here.
  PreprocessorTester open_paren(
      "`define C(z) z\n`define A(x) hello `C(\n`A(1)\n",
      VerilogPreprocess::Config({.expand_macros = true}));
  EXPECT_FALSE(open_paren.Status().ok());
}

}  // namespace
}  // namespace verilog
