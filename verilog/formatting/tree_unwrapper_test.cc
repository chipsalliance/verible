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

#include "verilog/formatting/tree_unwrapper.h"

#include <initializer_list>
#include <iostream>
#include <memory>
#include <sstream>
#include <string>
#include <utility>
#include <vector>

#include "gmock/gmock.h"
#include "gtest/gtest.h"
#include "absl/memory/memory.h"
#include "absl/strings/str_join.h"
#include "absl/strings/string_view.h"
#include "common/formatting/basic_format_style.h"
#include "common/formatting/format_token.h"
#include "common/formatting/token_partition_tree.h"
#include "common/formatting/unwrapped_line.h"
#include "common/text/text_structure.h"
#include "common/util/container_iterator_range.h"
#include "common/util/logging.h"
#include "common/util/spacer.h"
#include "common/util/status.h"
#include "common/util/vector_tree.h"
#include "verilog/analysis/verilog_analyzer.h"

#undef EXPECT_OK
#define EXPECT_OK(value) EXPECT_TRUE((value).ok())

namespace verilog {
namespace formatter {
namespace {

using verible::UnwrappedLine;

// Contains the expected token sequence and indentation for an UnwrappedLine
struct ExpectedUnwrappedLine {
  int indentation_spaces;
  std::vector<absl::string_view> tokens;  // includes comments

  explicit ExpectedUnwrappedLine(int s) : indentation_spaces(s), tokens() {}

  ExpectedUnwrappedLine(
      int s, std::initializer_list<absl::string_view> expected_tokens)
      : indentation_spaces(s), tokens(expected_tokens) {}

  void ShowUnwrappedLineDifference(std::ostream* stream,
                                   const UnwrappedLine& uwline) const;
  bool EqualsUnwrappedLine(std::ostream* stream,
                           const UnwrappedLine& uwline) const;
};

// Human readable ExpectedUnwrappedLined which outputs indentation and line.
// Mimic operator << (ostream&, const UnwrappedLine&).
std::ostream& operator<<(std::ostream& stream,
                         const ExpectedUnwrappedLine& expected_uwline) {
  stream << verible::Spacer(expected_uwline.indentation_spaces,
                            UnwrappedLine::kIndentationMarker)
         << '[';
  if (expected_uwline.tokens.empty()) {
    // Empty really means don't-care -- this is not a leaf level
    // UnwrappedLine, but rather, an enclosing level.
    stream << "<auto>";
  } else {
    stream << absl::StrJoin(expected_uwline.tokens.begin(),
                            expected_uwline.tokens.end(), " ",
                            absl::StreamFormatter());
  }
  return stream << ']';
}

// This tree type will be 'diff-ed' against a VectorTree<UnwrappedLine>.
typedef verible::VectorTree<ExpectedUnwrappedLine> ExpectedUnwrappedLineTree;

void ValidateExpectedTreeNode(const ExpectedUnwrappedLineTree& etree) {
  // At each tree node, there should either be expected tokens in the node's
  // value, or node's children, but not both.
  CHECK(etree.Value().tokens.empty() != etree.Children().empty())
      << "Node should not contain both tokens and children @"
      << verible::NodePath(etree);
}

// Make sure the expect-tree is well-formed.
void ValidateExpectedTree(const ExpectedUnwrappedLineTree& etree) {
  etree.ApplyPreOrder(ValidateExpectedTreeNode);
}

// Outputs the unwrapped line followed by this expected unwrapped line
void ExpectedUnwrappedLine::ShowUnwrappedLineDifference(
    std::ostream* stream, const UnwrappedLine& uwline) const {
  *stream << std::endl
          << "unwrapped line: " << std::endl
          << '\"' << uwline << '\"' << std::endl;
  *stream << "expected: " << std::endl;
  *stream << '\"' << *this << '\"' << std::endl;
}

// Helper method to compare ExpectedUnwrappedLine to UnwrappedLine by checking
// sizes (number of tokens), each token sequentially, and indentation.
// Outputs differences to stream
bool ExpectedUnwrappedLine::EqualsUnwrappedLine(
    std::ostream* stream, const UnwrappedLine& uwline) const {
  VLOG(4) << __FUNCTION__;
  bool equal = true;
  // If the expected token array is empty, don't check because tokens
  // are expected in children nodes.
  if (!tokens.empty()) {
    // Check that the size of the UnwrappedLine (number of tokens) is correct.
    if (uwline.Size() != tokens.size()) {
      *stream << "error: unwrapped line size incorrect" << std::endl;
      *stream << "unwrapped line has: " << uwline.Size() << " tokens, ";
      *stream << "expected: " << tokens.size() << std::endl;
      ShowUnwrappedLineDifference(stream, uwline);
      equal = false;
    } else {
      // Only compare the text of each token, and none of the other TokenInfo
      // fields. Stops at first unmatched token
      // TODO(fangism): rewrite this using std::mismatch
      for (size_t i = 0; i < uwline.Size(); i++) {
        absl::string_view uwline_token = uwline.TokensRange()[i].Text();
        absl::string_view expected_token = tokens[i];
        if (uwline_token != expected_token) {
          *stream << "error: unwrapped line token #" << i + 1
                  << " does not match expected token" << std::endl;
          *stream << "unwrapped line token is: \"" << uwline_token << "\""
                  << std::endl;
          *stream << "expected: \"" << expected_token << "\"" << std::endl;
          equal = false;
        }
      }
    }
  }

  // Check that the indentation spaces of the UnwrappedLine is correct
  if (uwline.IndentationSpaces() != indentation_spaces) {
    *stream << "error: unwrapped line indentation incorrect" << std::endl;
    *stream << "indentation spaces: " << uwline.IndentationSpaces()
            << std::endl;
    *stream << "expected indentation spaces: " << indentation_spaces
            << std::endl;
    equal = false;
  }
  if (!equal) {
    ShowUnwrappedLineDifference(stream, uwline);
    return false;
  }
  return true;
}

// Contains test cases for files with the UnwrappedLines that should be
// produced from TreeUnwrapper.Unwrap()
struct TreeUnwrapperTestData {
  const char* test_name;

  // The source code for testing must be syntactically correct.
  std::string source_code;

  // The reference values and structure of UnwrappedLines to expect.
  ExpectedUnwrappedLineTree expected_unwrapped_lines;

  template <typename... Args>
  TreeUnwrapperTestData(const char* name, std::string code, Args&&... nodes)
      : test_name(name),
        source_code(code),
        expected_unwrapped_lines(ExpectedUnwrappedLine{0},
                                 std::forward<Args>(nodes)...) {
    // The root node is always at level 0.
    ValidateExpectedTree(expected_unwrapped_lines);
  }
};

// Iterates through UnwrappedLines and expected lines and verifies that they
// are equal
bool VerifyUnwrappedLines(std::ostream* stream,
                          const verible::VectorTree<UnwrappedLine>& uwlines,
                          const TreeUnwrapperTestData& test_case) {
  std::ostringstream first_diff_stream;
  const auto diff = verible::DeepEqual(
      uwlines, test_case.expected_unwrapped_lines,
      [&first_diff_stream](const UnwrappedLine& actual,
                           const ExpectedUnwrappedLine& expect) {
        return expect.EqualsUnwrappedLine(&first_diff_stream, actual);
      });

  if (diff.left != nullptr) {
    *stream << "error: test case: " << test_case.test_name << std::endl;
    *stream << "first difference at subnode " << verible::NodePath(*diff.left)
            << std::endl;
    *stream << "expected:\n" << *diff.right << std::endl;
    *stream << "but got :\n"
            << verible::TokenPartitionTreePrinter(*diff.left) << std::endl;
    const auto left_children = diff.left->Children().size();
    const auto right_children = diff.right->Children().size();
    EXPECT_EQ(left_children, right_children) << "code:\n"
                                             << test_case.source_code;
    if (first_diff_stream.str().length()) {
      // The Value()s at these nodes are different.
      *stream << "value difference: " << first_diff_stream.str();
    }
    return false;
  }
  return true;
}

// Test fixture used to handle the VerilogAnalyzer which produces the
// concrete syntax tree and token stream that TreeUnwrapper uses to produce
// UnwrappedLines
class TreeUnwrapperTest : public ::testing::Test {
 protected:
  TreeUnwrapperTest() {
    style_.indentation_spaces = 1;
    style_.wrap_spaces = 2;
  }

  // Takes a string representation of a verilog file and creates a
  // VerilogAnalyzer which holds a concrete syntax tree and token stream view
  // of the file.
  void MakeTree(const std::string& content) {
    analyzer_ = absl::make_unique<VerilogAnalyzer>(content, "TEST_FILE");
    verible::util::Status status = ABSL_DIE_IF_NULL(analyzer_)->Analyze();
    EXPECT_OK(status) << "Rejected code: " << std::endl << content;

    // Since source code is required to be valid, this error-handling is just
    // to help debug the test case construction
    if (!status.ok()) {
      const std::vector<std::string> syntax_error_messages(
          analyzer_->LinterTokenErrorMessages());
      for (const auto& message : syntax_error_messages) {
        std::cout << message << std::endl;
      }
    }
  }
  // Creates a TreeUnwrapper populated with a concrete syntax tree and
  // token stream view from the file input
  std::unique_ptr<TreeUnwrapper> CreateTreeUnwrapper(
      const std::string& source_code) {
    MakeTree(source_code);
    const verible::TextStructureView& text_structure_view = analyzer_->Data();
    unwrapper_data_ =
        absl::make_unique<UnwrapperData>(text_structure_view.TokenStream());

    return absl::make_unique<TreeUnwrapper>(
        text_structure_view, style_, unwrapper_data_->preformatted_tokens);
  }

  // The VerilogAnalyzer to produce a concrete syntax tree of raw Verilog code
  std::unique_ptr<VerilogAnalyzer> analyzer_;

  // Support data that needs to outlive the TreeUnwrappers that use it.
  std::unique_ptr<UnwrapperData> unwrapper_data_;

  // Style configuration.
  verible::BasicFormatStyle style_;
};

// Test that TreeUnwrapper produces the correct UnwrappedLines from an empty
// file
TEST_F(TreeUnwrapperTest, UnwrapEmptyFile) {
  const std::string source_code = "";

  auto tree_unwrapper = CreateTreeUnwrapper(source_code);
  tree_unwrapper->Unwrap();

  const auto lines = tree_unwrapper->FullyPartitionedUnwrappedLines();
  EXPECT_TRUE(lines.empty())  // Blank line removed.
      << "Unexpected unwrapped line: " << lines.front();
}

// Test that TreeUnwrapper produces the correct UnwrappedLines from a blank
// line.
TEST_F(TreeUnwrapperTest, UnwrapBlankLineOnly) {
  const std::string source_code = "\n";

  auto tree_unwrapper = CreateTreeUnwrapper(source_code);
  tree_unwrapper->Unwrap();

  const auto lines = tree_unwrapper->FullyPartitionedUnwrappedLines();
  // TODO(b/140277909): preserve blank lines
  EXPECT_TRUE(lines.empty())  // Blank line removed.
      << "Unexpected unwrapped line: " << lines.front();
}

// TODO(korzhacke): Test CollectFilteredTokens directly

// ExpectedUnwrappedLine tree builder functions

// N is for node
template <typename... Args>
ExpectedUnwrappedLineTree N(int spaces, Args&&... nodes) {
  return ExpectedUnwrappedLineTree(ExpectedUnwrappedLine(spaces),
                                   std::forward<Args>(nodes)...);
}

// L is for leaf, which is the only type of node that should list tokens
ExpectedUnwrappedLineTree L(int spaces,
                            std::initializer_list<absl::string_view> tokens) {
  return ExpectedUnwrappedLineTree(ExpectedUnwrappedLine(spaces, tokens));
}

// NL composes the above node and leaf where the indentation level is the same.
// This is useful for contexts like single-statement lists.
ExpectedUnwrappedLineTree NL(int spaces,
                             std::initializer_list<absl::string_view> tokens) {
  return N(spaces, L(spaces, tokens));
}

// Node function aliases for readability.
// Can't use const auto& Alias = N; because N is overloaded.
#define ModuleHeader N
#define ModulePortList N
#define ModuleParameterList N
#define ModuleItemList N
#define MacroArgList N
#define Instantiation N
#define DataDeclaration N
#define InstanceList N
#define PortActualList N
#define StatementList N
#define ClassHeader N
#define ClassItemList N
#define ClassParameterList N
#define FunctionHeader N
#define TaskHeader N
#define TFPortList N
#define PackageItemList N
#define EnumItemList N
#define StructUnionMemberList N
#define PropertyItemList N
#define VarDeclList N
#define CovergroupHeader N
#define CovergroupItemList N
#define CoverpointItemList N
#define CrossItemList N
#define SequenceItemList N
#define ConstraintItemList N
#define ConstraintExpressionList N
#define DistItemList N
#define LoopHeader N
#define ForSpec N
#define CaseItemList N
#define FlowControl N  // for loops and conditional whole constructs

// Test data for unwrapping Verilog modules
// Test case format: test name, source code, ExpectedUnwrappedLines
const TreeUnwrapperTestData kUnwrapModuleTestCases[] = {
    {
        "empty module",
        "module foo ();"
        "endmodule",
        ModuleHeader(0, L(0, {"module", "foo", "("}), L(0, {")", ";"})),
        L(0, {"endmodule"}),
    },

    {
        "empty module extra spaces",  // verifying space-insensitivity
        "  module\tfoo   (\t) ;    "
        "endmodule   ",
        ModuleHeader(0, L(0, {"module", "foo", "("}), L(0, {")", ";"})),
        L(0, {"endmodule"}),
    },

    {
        "empty module extra newlines",  // verifying space-insensitivity
        "module foo (\n\n);\n"
        "endmodule\n",
        ModuleHeader(0, L(0, {"module", "foo", "("}), L(0, {")", ";"})),
        L(0, {"endmodule"}),
    },

    {
        "module with port declarations",
        "module foo ("
        "input bar,"
        "output baz"
        ");"
        "endmodule",
        ModuleHeader(0, L(0, {"module", "foo", "("}),
                     ModulePortList(2, L(2, {"input", "bar", ","}),
                                    L(2, {"output", "baz"})),
                     L(0, {")", ";"})),
        L(0, {"endmodule"}),
    },

    {
        "module with conditional port declarations",
        "module foo ("
        "`ifndef FOO\n"
        "input bar,"
        "`endif\n"
        "output baz"
        ");"
        "endmodule",
        ModuleHeader(0, L(0, {"module", "foo", "("}),
                     ModulePortList(2, L(0, {"`ifndef", "FOO"}),
                                    L(2, {"input", "bar", ","}),
                                    L(0, {"`endif"}), L(2, {"output", "baz"})),
                     L(0, {")", ";"})),
        L(0, {"endmodule"}),
    },

    {
        "module with parameters",
        "module foo #("
        "parameter bar =1,"
        "localparam baz =2"
        ");"
        "endmodule",
        ModuleHeader(
            0, L(0, {"module", "foo", "#", "("}),
            ModuleParameterList(2, L(2, {"parameter", "bar", "=", "1", ","}),
                                L(2, {"localparam", "baz", "=", "2"})),
            L(0, {")", ";"})),
        L(0, {"endmodule"}),
    },

    {
        "module with conditional parameters",
        "module foo #("
        "`ifdef FOO\n"
        "parameter bar =1,"
        "`endif\n"
        "localparam baz =2"
        ");"
        "endmodule",
        ModuleHeader(
            0, L(0, {"module", "foo", "#", "("}),
            ModuleParameterList(2, L(0, {"`ifdef", "FOO"}),
                                L(2, {"parameter", "bar", "=", "1", ","}),
                                L(0, {"`endif"}),
                                L(2, {"localparam", "baz", "=", "2"})),
            L(0, {")", ";"})),
        L(0, {"endmodule"}),
    },

    {
        "module with parameters and ports",
        "module foo #("
        "parameter bar =1,"
        "localparam baz =2"
        ") ("
        "input yar,"
        "output gar"
        ");"
        "endmodule",
        ModuleHeader(
            0, L(0, {"module", "foo", "#", "("}),
            ModuleParameterList(2, L(2, {"parameter", "bar", "=", "1", ","}),
                                L(2, {"localparam", "baz", "=", "2"})),
            L(0, {")", "("}),
            ModulePortList(2, L(2, {"input", "yar", ","}),
                           L(2, {"output", "gar"})),
            L(0, {")", ";"})),
        L(0, {"endmodule"}),
    },

    {
        "two modules with end-labels",
        "module foo ();"
        "endmodule : foo "
        "module zoo;"
        "endmodule : zoo",
        ModuleHeader(0, L(0, {"module", "foo", "("}), L(0, {")", ";"})),
        L(0, {"endmodule", ":", "foo"}),
        ModuleHeader(0, L(0, {"module", "zoo", ";"})),
        L(0, {"endmodule", ":", "zoo"}),
    },

    {
        "module with K&R-style ports with always/begin/end "
        "(kModulePortDeclaration)",
        "module addf (a, b, ci, s, co);"
        "input a, b, ci;"
        "output s, co;"
        "always @(a, b, ci) begin"
        "  s = (a^b^ci);"
        "  co = (a&b);"
        "end "
        "endmodule",
        ModuleHeader(0, L(0, {"module", "addf", "("}),
                     ModulePortList(2, L(2, {"a", ",", "b", ",", "ci", ",", "s",
                                             ",", "co"})),
                     L(0, {")", ";"})),
        ModuleItemList(
            1, L(1, {"input", "a", ",", "b", ",", "ci", ";"}),
            L(1, {"output", "s", ",", "co", ";"}),
            L(1, {"always", "@", "(", "a", ",", "b", ",", "ci", ")", "begin"}),
            StatementList(
                2, NL(2, {"s", "=", "(", "a", "^", "b", "^", "ci", ")", ";"}),
                NL(2, {"co", "=", "(", "a", "&", "b", ")", ";"})),
            L(1, {"end"})),
        L(0, {"endmodule"}),
    },

    {
        "module with kModuleItemList and kDataDeclarations",
        "module tryme;"
        "foo1 a;"
        "foo2 b();"
        "endmodule",
        ModuleHeader(0, L(0, {"module", "tryme", ";"})),
        ModuleItemList(1,
                       Instantiation(1,  // fused single instance
                                     L(1, {"foo1", "a", ";"})),
                       Instantiation(1, L(1, {"foo2", "b", "(", ")", ";"}))),
        L(0, {"endmodule"}),
    },

    {
        "module with multi-instance () in single declaration",
        "module multi_inst;"
        "foo aa(), bb();"
        "endmodule",
        ModuleHeader(0, L(0, {"module", "multi_inst", ";"})),
        ModuleItemList(
            1, Instantiation(1, NL(1, {"foo"}),  // instantiation type
                             InstanceList(3, NL(3, {"aa", "(", ")", ","}),
                                          NL(3, {"bb", "(", ")", ";"})))),
        L(0, {"endmodule"}),
    },

    {
        "module with multi-variable in single declaration",
        "module multi_inst;"
        "foo aa, bb;"
        "endmodule",
        ModuleHeader(0, L(0, {"module", "multi_inst", ";"})),
        ModuleItemList(
            1, Instantiation(
                   1, NL(1, {"foo"}),  // instantiation type
                   InstanceList(3, NL(3, {"aa", ","}), NL(3, {"bb", ";"})))),
        L(0, {"endmodule"}),
    },

    {
        "module with multi-variable with assignments in single declaration",
        "module multi_inst;"
        "foo aa = 1, bb = 2;"
        "endmodule",
        ModuleHeader(0, L(0, {"module", "multi_inst", ";"})),
        ModuleItemList(
            1, Instantiation(1, NL(1, {"foo"}),  // instantiation type
                             InstanceList(3, NL(3, {"aa", "=", "1", ","}),
                                          NL(3, {"bb", "=", "2", ";"})))),
        L(0, {"endmodule"}),
    },

    {
        "module with instantiations with parameterized types (positional)",
        "module tryme;"
        "foo #(1) a;"
        "bar #(2, 3) b();"
        "endmodule",
        ModuleHeader(0, L(0, {"module", "tryme", ";"})),
        ModuleItemList(
            1,
            // These are both single instances, fused with type partition.
            Instantiation(1, L(1, {"foo", "#", "(", "1", ")", "a", ";"})),
            Instantiation(1, L(1, {"bar", "#", "(", "2", ",", "3", ")", "b",
                                   "(", ")", ";"}))),
        L(0, {"endmodule"}),
    },

    {
        "module with instantiations with single-parameterized types (named)",
        "module tryme;"
        "foo #(.N(1)) a;"
        "bar #(.M(2)) b();"
        "endmodule",
        ModuleHeader(0, L(0, {"module", "tryme", ";"})),
        ModuleItemList(1,
                       // single instances fused with instantiation type
                       Instantiation(1, L(1, {"foo", "#", "("}),
                                     NL(3, {".", "N", "(", "1", ")"}),  //
                                     L(1, {")", "a", ";"})),
                       Instantiation(1, L(1, {"bar", "#", "("}),
                                     NL(3, {".", "M", "(", "2", ")"}),  //
                                     L(1, {")", "b", "(", ")", ";"}))   //
                       ),
        L(0, {"endmodule"}),
    },

    {
        "module with instantiations with multi-parameterized types (named)",
        "module tryme;"
        "foo #(.N(1), .M(4)) a;"
        "endmodule",
        ModuleHeader(0, L(0, {"module", "tryme", ";"})),
        ModuleItemList(
            1,
            // single instances fused with instantiation type
            Instantiation(1, L(1, {"foo", "#", "("}),
                          N(3, L(3, {".", "N", "(", "1", ")", ","}),
                            // note how comma is attached to above partition
                            L(3, {".", "M", "(", "4", ")"})),
                          L(1, {")", "a", ";"}))),
        L(0, {"endmodule"}),
    },

    {
        "module with instances and various port actuals",
        "module got_ports;"
        "foo c(x, y, z);"
        "foo d(.x(x), .y(y), .w(z));"
        "foo e(x, a, .y(y), .w(z));"
        "endmodule",
        ModuleHeader(0, L(0, {"module", "got_ports", ";"})),
        ModuleItemList(
            1,
            Instantiation(1, L(1, {"foo", "c", "("}),
                          PortActualList(3, L(3, {"x", ","}), L(3, {"y", ","}),
                                         L(3, {"z"})),
                          L(1, {")", ";"})  // TODO(fangism): attach to 'z'?
                          ),
            Instantiation(
                1, L(1, {"foo", "d", "("}),
                PortActualList(3, L(3, {".", "x", "(", "x", ")", ","}),
                               L(3, {".", "y", "(", "y", ")", ","}),
                               L(3, {".", "w", "(", "z", ")"})),
                L(1, {")", ";"})),
            Instantiation(1, L(1, {"foo", "e", "("}),
                          PortActualList(3, L(3, {"x", ","}), L(3, {"a", ","}),
                                         L(3, {".", "y", "(", "y", ")", ","}),
                                         L(3, {".", "w", "(", "z", ")"})),
                          L(1, {")", ";"}))),
        L(0, {"endmodule"}),
    },

    {
        "module interface ports",
        "module foo ("
        "interface bar_if, interface baz_if);"
        "endmodule",
        ModuleHeader(0, L(0, {"module", "foo", "("}),
                     ModulePortList(2, L(2, {"interface", "bar_if", ","}),
                                    L(2, {"interface", "baz_if"})),
                     L(0, {")", ";"})),
        L(0, {"endmodule"}),
    },

    {
        "module cast with constant functions",
        "module cast_with_constant_functions;"
        "foo dut("
        ".bus_in({brn::Num_blocks{$bits(dbg::bus_t)'(0)}}),"
        ".bus_mid({brn::Num_bits{$clog2(dbg::bus_t)'(1)}}),"
        ".bus_out(out));"
        "endmodule",
        ModuleHeader(0, L(0, {"module", "cast_with_constant_functions", ";"})),
        ModuleItemList(
            1,
            Instantiation(
                1,  //
                L(1, {"foo", "dut", "("}),
                PortActualList(
                    3,  //
                    L(3, {".",          "bus_in", "(",     "{", "brn", "::",
                          "Num_blocks", "{",      "$bits", "(", "dbg", "::",
                          "bus_t",      ")",      "'",     "(", "0",   ")",
                          "}",          "}",      ")",     ","}),
                    L(3, {".",        "bus_mid", "(",      "{", "brn", "::",
                          "Num_bits", "{",       "$clog2", "(", "dbg", "::",
                          "bus_t",    ")",       "'",      "(", "1",   ")",
                          "}",        "}",       ")",      ","}),
                    L(3, {".", "bus_out", "(", "out", ")"})),
                L(1, {")", ";"}))),
        L(0, {"endmodule"}),
    },

    {
        "module direct assignment",
        "module addf (\n"
        "input a, input b, input ci,\n"
        "output s, output co);\n"
        "assign s = (a^b^ci);\n"
        "assign co = (a&b)|(a&ci)|(b&ci);\n"
        "endmodule",
        ModuleHeader(0, L(0, {"module", "addf", "("}),
                     ModulePortList(
                         2, L(2, {"input", "a", ","}),
                         L(2, {"input", "b", ","}), L(2, {"input", "ci", ","}),
                         L(2, {"output", "s", ","}), L(2, {"output", "co"})),
                     L(0, {")", ";"})),
        ModuleItemList(
            1,
            NL(1, {"assign", "s", "=", "(", "a", "^", "b", "^", "ci", ")", ";"}),
            NL(1, {"assign", "co", "=", "(", "a", "&", "b", ")",  "|", "(", "a",
                  "&",      "ci", ")", "|", "(", "b", "&", "ci", ")", ";"})),
        L(0, {"endmodule"}),
    },

    {
        "module multiple assignments",
        "module foob;\n"
        "assign s = a, y[0] = b[1], z.z = c -jkl;\n"  // multiple assignments
        "endmodule",
        ModuleHeader(0, L(0, {"module", "foob", ";"})),
        ModuleItemList(1, NL(1, {"assign", "s", "=", "a", ",", "y",   "[", "0",
                                "]",      "=", "b", "[", "1", "]",   ",", "z",
                                ".",      "z", "=", "c", "-", "jkl", ";"})),
        L(0, {"endmodule"}),
    },

    {
        "module multiple assignments as module item",
        "module foob;\n"
        "assign `BIT_ASSIGN_MACRO(l1, r1)\n"  // as module item
        "endmodule",
        ModuleHeader(0, L(0, {"module", "foob", ";"})),
        ModuleItemList(
                1,
                N(1, L(1, {"assign", "`BIT_ASSIGN_MACRO", "("}),
                    MacroArgList(2, L(2, {"l1", ",", "r1"})),
                    L(1, {")"}))),
        L(0, {"endmodule"}),
    },

    {
        "module multiple assignments as module item with semicolon",
        "module foob;\n"
        "assign `BIT_ASSIGN_MACRO(l1, r1);\n"  // as module item
        "endmodule",
        ModuleHeader(0, L(0, {"module", "foob", ";"})),
        ModuleItemList(
                1,
                N(1, L(1, {"assign", "`BIT_ASSIGN_MACRO", "("}),
                    MacroArgList(2, L(2, {"l1", ",", "r1"})),
                    L(1, {")", ";"}))),
        L(0, {"endmodule"}),
    },

    {
        "module multiple assignments as module item II",
        "module foob;\n"
        "initial begin\n"
        "assign `BIT_ASSIGN_MACRO(l1, r1)\n"  // as statement item
        "end\n"
        "endmodule",
        ModuleHeader(0, L(0, {"module", "foob", ";"})),
        ModuleItemList(1, L(1, {"initial", "begin"}),
                       StatementList(2, N(2, L(2, {"assign", "`BIT_ASSIGN_MACRO", "("}),
                                            MacroArgList(3, L(3, {"l1", ",", "r1"})),
                                            L(2, {")"}))),
                       L(1, {"end"})),
        L(0, {"endmodule"}),
    },

    {
        "module with labeled statements",
        "module labeled_statements;\n"
        "initial begin\n"
        "  a = 0;\n"
        "  foo: b = 0;\n"  // with label
        "end\n"
        "endmodule",
        ModuleHeader(0, L(0, {"module", "labeled_statements", ";"})),
        ModuleItemList(1, L(1, {"initial", "begin"}),
                       StatementList(2, NL(2, {"a", "=", "0", ";"}),
                                     NL(2, {"foo", ":", "b", "=", "0", ";"})),
                       L(1, {"end"})),
        L(0, {"endmodule"}),
    },

    {
        "module with block generate statements",
        "module block_generate;\n"
        "generate\n"
        "endgenerate\n"
        "endmodule\n",
        ModuleHeader(0, L(0, {"module", "block_generate", ";"})),
        ModuleItemList(1, L(1, {"generate"}), L(1, {"endgenerate"})),
        L(0, {"endmodule"}),
    },

    {
        "module with block generate statements and macro call item",
        "module block_generate;\n"
        "`ASSERT(blah)\n"
        "generate\n"
        "endgenerate\n"
        "endmodule\n",
        ModuleHeader(0, L(0, {"module", "block_generate", ";"})),
        ModuleItemList(
                1,
                N(1, L(1, {"`ASSERT", "("}),
                    MacroArgList(2, L(2, {"blah"})),
                    L(1, {")"})
                    ),
                L(1, {"generate"}), L(1, {"endgenerate"})),
        L(0, {"endmodule"}),
    },

    {
        "module with conditional generate statement blocks",
        "module conditionals;\n"
        "if (foo) begin\n"
        "  a aa;\n"
        "end\n"
        "if (bar) begin\n"
        "  b bb;\n"
        "end\n"
        "endmodule",
        ModuleHeader(0, L(0, {"module", "conditionals", ";"})),
        ModuleItemList(
            1, L(1, {"if", "(", "foo", ")", "begin"}),
            ModuleItemList(2, Instantiation(2, L(2, {"a", "aa", ";"}))),
            L(1, {"end"}),  //
            L(1, {"if", "(", "bar", ")", "begin"}),
            ModuleItemList(2, Instantiation(2, L(2, {"b", "bb", ";"}))),
            L(1, {"end"})),
        L(0, {"endmodule"}),
    },

    {
        "module with conditional generate block and macro call item",
        "module conditional_generate_macros;\n"
        "if (foo) begin\n"
        "`COVER()\n"
        "`ASSERT()\n"
        "end\n"
        "endmodule\n",
        ModuleHeader(0, L(0, {"module", "conditional_generate_macros", ";"})),
        ModuleItemList(1, L(1, {"if", "(", "foo", ")", "begin"}),
                       ModuleItemList(
                           2,
                           N(2, L(2, {"`COVER", "("}), L(2, {")"})),
                           N(2, L(2, {"`ASSERT", "("}), L(2, {")"}))),
                       L(1, {"end"})),
        L(0, {"endmodule"}),
    },

    {
        "module with conditional generate block and comments",
        "module conditional_generate_comments;\n"
        "if (foo) begin\n"
        "// comment1\n"
        "// comment2\n"
        "end\n"
        "endmodule\n",
        ModuleHeader(0, L(0, {"module", "conditional_generate_comments", ";"})),
        ModuleItemList(
            1, L(1, {"if", "(", "foo", ")", "begin"}),
            ModuleItemList(2, L(2, {"// comment1"}), L(2, {"// comment2"})),
            L(1, {"end"})),
        L(0, {"endmodule"}),
    },

    /* TODO(fangism): Adding another level of non-indented nesting may be needed
     * to handle the following single-statement conditional form gracefully.
    {
        "module with conditional generate single statements",
        "module conditionals;\n"
        "if (foo) a aa;\n"
        "if (bar) b bb;\n"
        "endmodule",
    },
    */

    {
        "module with single loop generate statement",
        "module loop_generate;\n"
        "for (genvar x=1;x<N;++x) begin\n"
        "  a aa;\n"
        "end\n"
        "endmodule",
        ModuleHeader(0, L(0, {"module", "loop_generate", ";"})),
        ModuleItemList(
            1,
            LoopHeader(1, L(1, {"for", "("}),
                       ForSpec(3, L(3, {"genvar", "x", "=", "1", ";"}),
                               L(3, {"x", "<", "N", ";"}), L(3, {"++", "x"})),
                       L(1, {")", "begin"})),
            ModuleItemList(2,  //
                           Instantiation(2, L(2, {"a", "aa", ";"}))),
            L(1, {"end"})),
        L(0, {"endmodule"}),
    },

    {
        "module with loop generate continuous assignments",
        "module loop_generate_assign;\n"
        "for (genvar x=1;x<N;++x) begin"
        "  assign x = y;assign y = z;"
        "end\n"
        "endmodule",
        ModuleHeader(0, L(0, {"module", "loop_generate_assign", ";"})),
        ModuleItemList(
            1,
            LoopHeader(1, L(1, {"for", "("}),
                       ForSpec(3, L(3, {"genvar", "x", "=", "1", ";"}),
                               L(3, {"x", "<", "N", ";"}), L(3, {"++", "x"})),
                       L(1, {")", "begin"})),
            ModuleItemList(2,  //
                           NL(2, {"assign", "x", "=", "y", ";"}),
                           NL(2, {"assign", "y", "=", "z", ";"})),
            L(1, {"end"})),
        L(0, {"endmodule"}),
    },

    {
        "module with multiple loop generate statements",
        "module loop_generates;\n"
        "for (x=1;;) begin\n"
        "  a aa;\n"
        "end\n"
        "for (y=0;;) begin\n"
        "  b bb;\n"
        "end\n"
        "endmodule",
        ModuleHeader(0, L(0, {"module", "loop_generates", ";"})),
        ModuleItemList(
            1,
            LoopHeader(1, L(1, {"for", "("}),
                       ForSpec(3, L(3, {"x", "=", "1", ";"}), L(3, {";"})),
                       L(1, {")", "begin"})),
            ModuleItemList(2,                //
                           Instantiation(2,  //
                                         L(2, {"a", "aa", ";"}))),
            L(1, {"end"}),  //
            LoopHeader(1, L(1, {"for", "("}),
                       ForSpec(3, L(3, {"y", "=", "0", ";"}), L(3, {";"})),
                       L(1, {")", "begin"})),
            ModuleItemList(2,                //
                           Instantiation(2,  //
                                         L(2, {"b", "bb", ";"}))),
            L(1, {"end"})),
        L(0, {"endmodule"}),
    },

    {
        "module with case generate statements",
        "module multi_cases;\n"
        "case (foo)\n"
        "  A: a aa;\n"
        "endcase\n"
        "case (bar)\n"
        "  B: b bb;\n"
        "endcase\n"
        "endmodule",
        ModuleHeader(0, L(0, {"module", "multi_cases", ";"})),
        ModuleItemList(1, L(1, {"case", "(", "foo", ")"}),
                       CaseItemList(2,
                                    // TODO(fangism): merge label prefix to
                                    // following subtree if it fits
                                    L(2, {"A", ":"}),
                                    Instantiation(2, L(2, {"a", "aa", ";"}))),
                       L(1, {"endcase"}), L(1, {"case", "(", "bar", ")"}),
                       CaseItemList(2, L(2, {"B", ":"}),
                                    Instantiation(2, L(2, {"b", "bb", ";"}))),
                       L(1, {"endcase"})),
        L(0, {"endmodule"}),
    },

    {
        "module case statements",
        "module case_statements;\n"  // case statements
        "always_comb begin\n"
        "  case (blah.blah)\n"
        "    aaa,bbb: x = y;\n"
        "    ccc,ddd: w = z;\n"
        "  endcase\n"
        "end\n"
        "endmodule",
        ModuleHeader(0, L(0, {"module", "case_statements", ";"})),
        ModuleItemList(
            1, L(1, {"always_comb", "begin"}),
            StatementList(
                2,
                FlowControl(
                    2, L(2, {"case", "(", "blah", ".", "blah", ")"}),
                    CaseItemList(
                        3, L(3, {"aaa", ",", "bbb", ":", "x", "=", "y", ";"}),
                        L(3, {"ccc", ",", "ddd", ":", "w", "=", "z", ";"})),
                    L(2, {"endcase"}))),
            L(1, {"end"})),
        L(0, {"endmodule"}),
    },

    {
        "module case/default statements with macro call rvalue",
        "module case_statements;\n"  // case statements
        "initial begin\n"
        "  case (blah.blah)\n"
        "    aaa,bbb: x = `YYY();\n"
        "    default: w = `ZZZ();\n"
        "  endcase\n"
        "end\n"
        "endmodule",
        ModuleHeader(0, L(0, {"module", "case_statements", ";"})),
        ModuleItemList(
            1, L(1, {"initial", "begin"}),
            StatementList(
                2, FlowControl(2, L(2, {"case", "(", "blah", ".", "blah", ")"}),
                               CaseItemList(
                                   3,
                                   L(3, {"aaa", ",", "bbb", ":", "x", "=", "`YYY", "("}),
                                   L(3, {")", ";"}),
                                   L(3, {"default", ":", "w", "=", "`ZZZ", "("}),
                                   L(3, { ")", ";"})),
                               L(2, {"endcase"}))),
            L(1, {"end"})),
        L(0, {"endmodule"}),
    },

    {
        "module multiple case statements",
        "module multiple_case_statements;\n"
        "always_comb begin\n"
        "  case (blah.blah)\n"
        "    aaa,bbb: x = y;\n"
        "  endcase\n"
        "  case (blah.blah)\n"
        "    ccc,ddd: w = z;\n"
        "  endcase\n"
        "end\n"
        "endmodule",
        ModuleHeader(0, L(0, {"module", "multiple_case_statements", ";"})),
        ModuleItemList(
            1, L(1, {"always_comb", "begin"}),
            StatementList(
                2,
                FlowControl(2, L(2, {"case", "(", "blah", ".", "blah", ")"}),
                            CaseItemList(3, L(3, {"aaa", ",", "bbb", ":", "x",
                                                  "=", "y", ";"})),
                            L(2, {"endcase"})),
                FlowControl(2, L(2, {"case", "(", "blah", ".", "blah", ")"}),
                            CaseItemList(3, L(3, {"ccc", ",", "ddd", ":", "w",
                                                  "=", "z", ";"})),
                            L(2, {"endcase"}))),
            L(1, {"end"})),
        L(0, {"endmodule"}),
    },

    {
        "module multiple initial/final statements",
        "module multiple_initial_final_statements;\n"
        "begin end\n"
        "initial begin\n"
        "end\n"
        "final begin\n"
        "end\n"
        "endmodule",
        ModuleHeader(
            0, L(0, {"module", "multiple_initial_final_statements", ";"})),
        ModuleItemList(1, L(1, {"begin"}), L(1, {"end"}),
                       L(1, {"initial", "begin"}), L(1, {"end"}),
                       L(1, {"final", "begin"}), L(1, {"end"})),
        L(0, {"endmodule"}),
    },

    {
        "module with clocking declarations",
        "module mcd;"
        "clocking cb @(posedge clk); endclocking "
        "clocking cb2 @(posedge clk); endclocking endmodule",
        ModuleHeader(0, L(0, {"module", "mcd", ";"})),
        ModuleItemList(
            1,  //
            L(1, {"clocking", "cb", "@", "(", "posedge", "clk", ")", ";"}),
            L(1, {"endclocking"}),
            L(1, {"clocking", "cb2", "@", "(", "posedge", "clk", ")", ";"}),
            L(1, {"endclocking"})),
        L(0, {"endmodule"}),
    },

    {
        "module with DPI import declarations",
        "module mdi;"
        "import   \"DPI-C\" function int add();"
        "import \"DPI-C\"  function int  sleep( input int secs );"
        "endmodule",
        ModuleHeader(0, L(0, {"module", "mdi", ";"})),
        ModuleItemList(
            1,  //
            L(1,
              {"import", "\"DPI-C\"", "function", "int", "add", "(", ")", ";"}),
            L(1, {"import", "\"DPI-C\"", "function", "int", "sleep", "("}),
            TFPortList(3, L(3, {"input", "int", "secs"})), L(1, {")", ";"})),
        L(0, {"endmodule"}),
    },
};

// Test that TreeUnwrapper produces the correct UnwrappedLines from module tests
TEST_F(TreeUnwrapperTest, UnwrapModuleTests) {
  for (const auto& test_case : kUnwrapModuleTestCases) {
    VLOG(1) << "Test: " << test_case.test_name;
    auto tree_unwrapper = CreateTreeUnwrapper(test_case.source_code);
    const auto* uwline_tree = tree_unwrapper->Unwrap();
    EXPECT_TRUE(VerifyUnwrappedLines(&std::cout, *ABSL_DIE_IF_NULL(uwline_tree),
                                     test_case));
  }
}

// Test data for unwrapping Verilog code with comments.
// Test case format: test name, source code, ExpectedUnwrappedLines
const TreeUnwrapperTestData kUnwrapCommentsTestCases[] = {
    // The UnwrappedLine keeps all of these comments, but mark them as
    // must-break

    {
        "single end of line comment test",
        "// comment\n",
        L(0, {"// comment"}),
    },

    {
        "single block comment test, no newline",
        "/* comment */",  // no newline
        L(0, {"/* comment */"}),
    },

    {
        "single block comment test, with newline",
        "/* comment */  \n",  // no newline
        L(0, {"/* comment */"}),
    },

    {
        "Indented offset first comment",
        "\n\n\n\n       /* comment */",
        L(0, {"/* comment */"}),
    },

    {
        "Indented offset multiple comments",
        "\n\n\n\n       /* comment */"
        "\n\n\n\n\n\n         // last comment\n",
        L(0, {"/* comment */"}),
        L(0, {"// last comment"}),
    },

    {
        "multiple comments",
        "// comment0\n"
        "/* comment1 *//*comment2*/ /*comment3*/ // comment4\n",
        L(0, {"// comment0"}),
        L(0, {"/* comment1 */", "/*comment2*/", "/*comment3*/", "// comment4"}),
    },

    {
        "simple module comments",
        "// start comment\n"
        "module foo (); endmodule\n"
        "// end comment\n",
        L(0, {"// start comment"}),
        ModuleHeader(0, L(0, {"module", "foo", "("}), L(0, {")", ";"})),
        L(0, {"endmodule"}),       //
        L(0, {"// end comment"}),  // comment on own line
    },

    {
        "two modules surrounded by comments",
        "// comment1\n"
        "module foo (); endmodule\n"
        "// comment2\n\n"
        "// comment3\n"
        "module bar (); endmodule\n"
        "// comment4\n",
        L(0, {"// comment1"}),
        ModuleHeader(0, L(0, {"module", "foo", "("}), L(0, {")", ";"})),
        L(0, {"endmodule"}),    //
        L(0, {"// comment2"}),  // comment on own line
        L(0, {"// comment3"}),  //
        ModuleHeader(0, L(0, {"module", "bar", "("}), L(0, {")", ";"})),
        L(0, {"endmodule"}),    //
        L(0, {"// comment4"}),  // comment on own line
    },

    {
        "module item comments only",
        "module foo ();\n"
        "// item comment 1\n"
        "// item comment 2\n"
        "endmodule\n",
        ModuleHeader(0, L(0, {"module", "foo", "("}), L(0, {")", ";"})),
        ModuleItemList(1, L(1, {"// item comment 1"}),  //
                       L(1, {"// item comment 2"})      //
                       ),
        L(0, {"endmodule"}),
    },

    {
        "module item and ports comments only",
        "  // humble module\n"
        "  module foo ( // non-port comment\n"
        "// port comment 1\n"
        "// port comment 2\n"
        ");  // header trailing comment\n"
        "// item comment 1\n"
        "// item comment 2\n"
        "endmodule\n",
        L(0, {"// humble module"}),
        ModuleHeader(0, L(0, {"module", "foo", "(", "// non-port comment"}),
                     ModulePortList(2, L(2, {"// port comment 1"}),
                                    L(2, {"// port comment 2"})),
                     L(0, {")", ";", "// header trailing comment"})),
        ModuleItemList(1, L(1, {"// item comment 1"}),  //
                       L(1, {"// item comment 2"})      //
                       ),
        L(0, {"endmodule"}),
    },

    {
        "offset tokens with comments",
        "// start comment\n"
        "module \n\n"
        "foo \n\n\n\n"
        "()\n\n\n\n"
        "; // comment at end of module\n"
        "endmodule\n"
        "// end comment\n",
        L(0, {"// start comment"}),
        ModuleHeader(0, L(0, {"module", "foo", "("}),
                     L(0, {")", ";", "// comment at end of module"})),
        L(0, {"endmodule"}),  // comment separated to next line
        L(0, {"// end comment"}),
    },

    {
        "multiple starting comments split",
        "// comment 1\n"
        "\n"
        "// comment 2\n"
        "module foo();"
        "endmodule",
        L(0, {"// comment 1"}),  //
        L(0, {"// comment 2"}),
        ModuleHeader(0, L(0, {"module", "foo", "("}), L(0, {")", ";"})),
        L(0, {"endmodule"}),
    },

    {
        "module with end of line comments in empty ports",
        "module foo ( // comment1\n"
        "// comment2\n"
        "// comment3\n"
        "); // comment4\n"
        "endmodule // endmodule comment\n",
        ModuleHeader(
            0, L(0, {"module", "foo", "(", "// comment1"}),
            ModulePortList(2, L(2, {"// comment2"}), L(2, {"// comment3"})),
            L(0, {")", ";", "// comment4"})),
        L(0, {"endmodule", "// endmodule comment"}),
    },

    {
        "module with end of line comments",
        "module foo ( // module foo ( comment!\n"
        "input bar, // input bar, comment\n"
        "output baz // output baz comment\n"
        "); // ); comment\n"
        "endmodule // endmodule comment\n",
        ModuleHeader(
            0, L(0, {"module", "foo", "(", "// module foo ( comment!"}),
            ModulePortList(2,
                           L(2, {"input", "bar", ",", "// input bar, comment"}),
                           L(2, {"output", "baz", "// output baz comment"})),
            L(0, {")", ";", "// ); comment"})),
        L(0, {"endmodule", "// endmodule comment"}),
    },

    // If there exists a newline between two comments, start the comment on
    // its own UnwrappedLine
    {
        "class with end of line comments spanning multiple lines",
        "class foo; // class foo; comment\n"
        "// comment on its own line\n"
        "// one more comment\n"
        "// and one last comment\n"
        "  import fedex_pkg::box;\n"
        "// new comment for fun\n"
        "  import fedex_pkg::*;\n"
        "endclass // endclass comment\n",
        ClassHeader(0, L(0, {"class", "foo", ";", "// class foo; comment"})),
        ClassItemList(1, L(1, {"// comment on its own line"}),
                      L(1, {"// one more comment"}),
                      L(1, {"// and one last comment"}),
                      L(1, {"import", "fedex_pkg", "::", "box", ";"}),
                      L(1, {"// new comment for fun"}),
                      L(1, {"import", "fedex_pkg", "::", "*", ";"})),
        L(0, {"endclass", "// endclass comment"}),
    },

    // The UnwrappedLine keeps all of these comments but does not mark them
    // as must-break
    {
        "module with in-line comments",
        "module foo ( /* comment1 */"
        " input /* comment2 */ bar,"
        "/*comment3 */ output /* comment4 */ baz"
        ") /* comment5 */;\n"
        "/* comment6 */endmodule\n",
        ModuleHeader(0, L(0, {"module", "foo", "(", "/* comment1 */"}),
                     ModulePortList(2,
                                    L(2, {"input", "/* comment2 */", "bar", ",",
                                          "/*comment3 */"}),
                                    L(2, {"output", "/* comment4 */", "baz"})),
                     L(0, {")", "/* comment5 */", ";"})),
        L(0, {"/* comment6 */"}),
        L(0, {"endmodule"}),
    },

    // This test case mixes types of comments to ensure they are in the
    // correct UnwrappedLines
    {
        "module with end of line and in-line comments",
        "module /* comment1 */ foo ( // comment2\n"
        "input bar,/* comment3 */ // comment4\n"
        "output baz // comment5\n"
        "); // comment6\n"
        "/* comment7 */ endmodule //comment8\n",
        ModuleHeader(
            0, L(0, {"module", "/* comment1 */", "foo", "(", "// comment2"}),
            ModulePortList(
                2, L(2, {"input", "bar", ",", "/* comment3 */", "// comment4"}),
                L(2, {"output", "baz", "// comment5"})),
            L(0, {")", ";", "// comment6"})),
        L(0, {"/* comment7 */"}),
        L(0, {"endmodule", "//comment8"}),
    },
};

// Test that TreeUnwrapper produces the correct UnwrappedLines from code with
// comments
TEST_F(TreeUnwrapperTest, UnwrapCommentsTests) {
  for (const auto& test_case : kUnwrapCommentsTestCases) {
    VLOG(1) << "Test: " << test_case.test_name;
    auto tree_unwrapper = CreateTreeUnwrapper(test_case.source_code);
    const auto* uwline_tree = tree_unwrapper->Unwrap();
    EXPECT_TRUE(VerifyUnwrappedLines(&std::cout, *ABSL_DIE_IF_NULL(uwline_tree),
                                     test_case))
        << "code:\n"
        << test_case.source_code;
  }
}

// Test data for unwrapping Verilog `uvm.* macros
// Test case format: test name, source code, ExpectedUnwrappedLines
const TreeUnwrapperTestData kUnwrapUvmTestCases[] = {
    {
        "simple uvm test case",
        "`uvm_object_utils_begin(l0)\n"
        "`uvm_field_int(l1a, UVM_DEFAULT)\n"
        "`uvm_field_int(l1b, UVM_DEFAULT)\n"
        "`uvm_object_utils_end\n",
        L(0, {"`uvm_object_utils_begin", "(", "l0", ")"}),
        L(1, {"`uvm_field_int", "(", "l1a", ",", "UVM_DEFAULT", ")"}),
        L(1, {"`uvm_field_int", "(", "l1b", ",", "UVM_DEFAULT", ")"}),
        L(0, {"`uvm_object_utils_end"}),
    },

    {
        "simple uvm field utils test case",
        "`uvm_field_utils_begin(l0)\n"
        "`uvm_field_int(l1a, UVM_DEFAULT)\n"
        "`uvm_field_int(l1b, UVM_DEFAULT)\n"
        "`uvm_field_utils_end\n",
        L(0, {"`uvm_field_utils_begin", "(", "l0", ")"}),
        L(1, {"`uvm_field_int", "(", "l1a", ",", "UVM_DEFAULT", ")"}),
        L(1, {"`uvm_field_int", "(", "l1b", ",", "UVM_DEFAULT", ")"}),
        L(0, {"`uvm_field_utils_end"}),
    },

    {
        "nested uvm test case",
        "`uvm_object_utils_begin(l0)\n"
        "`uvm_field_int(l1a, UVM_DEFAULT)\n"
        "`uvm_object_utils_begin(l1)\n"
        "`uvm_field_int(l2a, UVM_DEFAULT)\n"
        "`uvm_object_utils_begin(l2)\n"
        "`uvm_field_int(l3a, UVM_DEFAULT)\n"
        "`uvm_object_utils_end\n"
        "`uvm_object_utils_end\n"
        "`uvm_field_int(l1b, UVM_DEFAULT)\n"
        "`uvm_object_utils_end\n",
        L(0, {"`uvm_object_utils_begin", "(", "l0", ")"}),
        L(1, {"`uvm_field_int", "(", "l1a", ",", "UVM_DEFAULT", ")"}),
        L(1, {"`uvm_object_utils_begin", "(", "l1", ")"}),
        L(2, {"`uvm_field_int", "(", "l2a", ",", "UVM_DEFAULT", ")"}),
        L(2, {"`uvm_object_utils_begin", "(", "l2", ")"}),
        L(3, {"`uvm_field_int", "(", "l3a", ",", "UVM_DEFAULT", ")"}),
        L(2, {"`uvm_object_utils_end"}),
        L(1, {"`uvm_object_utils_end"}),
        L(1, {"`uvm_field_int", "(", "l1b", ",", "UVM_DEFAULT", ")"}),
        L(0, {"`uvm_object_utils_end"}),
    },

    {
        "missing uvm.*end macro test case",
        "`uvm_field_utils_begin(l0)\n"
        "`uvm_field_int(l1a, UVM_DEFAULT)\n"
        "`uvm_field_int(l1b, UVM_DEFAULT)\n",
        L(0, {"`uvm_field_utils_begin", "(", "l0", ")"}),
        L(0, {"`uvm_field_int", "(", "l1a", ",", "UVM_DEFAULT", ")"}),
        L(0, {"`uvm_field_int", "(", "l1b", ",", "UVM_DEFAULT", ")"}),
    },

    {
        "missing uvm.*begin macro test case",
        "`uvm_field_int(l1a, UVM_DEFAULT)\n"
        "`uvm_field_int(l1b, UVM_DEFAULT)\n"
        "`uvm_field_utils_end\n",
        L(0, {"`uvm_field_int", "(", "l1a", ",", "UVM_DEFAULT", ")"}),
        L(0, {"`uvm_field_int", "(", "l1b", ",", "UVM_DEFAULT", ")"}),
        L(0, {"`uvm_field_utils_end"}),
    },
};

// Test that TreeUnwrapper produces the correct UnwrappedLines from code with
// uvm macros
TEST_F(TreeUnwrapperTest, UnwrapUvmTests) {
  for (const auto& test_case : kUnwrapUvmTestCases) {
    VLOG(1) << "Test: " << test_case.test_name;
    auto tree_unwrapper = CreateTreeUnwrapper(test_case.source_code);
    const auto* uwline_tree = tree_unwrapper->Unwrap();
    EXPECT_TRUE(VerifyUnwrappedLines(&std::cout, *ABSL_DIE_IF_NULL(uwline_tree),
                                     test_case))
        << "code:\n"
        << test_case.source_code;
  }
}

// Test data for unwrapping Verilog classes
// Test case format: test name, source code, ExpectedUnwrappedLine
const TreeUnwrapperTestData kClassTestCases[] = {
    {
        "empty class",
        "class Foo; endclass",
        ClassHeader(0, L(0, {"class", "Foo", ";"})),
        L(0, {"endclass"}),
    },

    {
        "virtual class",
        "virtual class automatic Foo; endclass",
        ClassHeader(0, L(0, {"virtual", "class", "automatic", "Foo", ";"})),
        L(0, {"endclass"}),
    },

    {
        "extends class",
        "class Foo extends Bar #(x,y,z); endclass",
        ClassHeader(0, L(0, {"class", "Foo", "extends", "Bar", "#", "(", "x",
                             ",", "y", ",", "z", ")", ";"})),
        L(0, {"endclass"}),
    },

    {
        "class with function and task",
        "class Foo;\n"
        "integer sizer;\n"
        "function new (integer size);\n"
        "  begin\n"
        "    this.size = size;\n"
        "  end\n"
        "endfunction\n"
        "task print();\n"
        "  begin\n"
        "    $write(\"Hello, world!\");\n"
        "  end\n"
        "endtask\n"
        "endclass",
        ClassHeader(0, L(0, {"class", "Foo", ";"})),
        ClassItemList(
            1, DataDeclaration(1, L(1, {"integer", "sizer", ";"})),
            FunctionHeader(1, L(1, {"function", "new", "("}),
                           TFPortList(3, L(3, {"integer", "size"})),
                           L(1, {")", ";"})),
            StatementList(
                2, L(2, {"begin"}),
                StatementList(3, NL(3, {"this", ".", "size", "=", "size", ";"})),
                L(2, {"end"})),
            L(1, {"endfunction"}),
            TaskHeader(1, L(1, {"task", "print", "(", ")", ";"})),
            StatementList(
                2, L(2, {"begin"}),
                StatementList(
                    3, NL(3, {"$write", "(", "\"Hello, world!\"", ")", ";"})),
                L(2, {"end"})),
            L(1, {"endtask"})),
        L(0, {"endclass"}),
    },

    {
        "class with function and task and comments",
        "class c; // c is for cookie\n"
        "// f is for false\n"
        "function f (integer size);\n"
        "endfunction\n"
        "// t is for true\n"
        "task t();\n"
        "endtask\n"
        "// class is about to end\n"
        "endclass",
        ClassHeader(0, L(0, {"class", "c", ";", "// c is for cookie"})),
        ClassItemList(1, L(1, {"// f is for false"}),
                      FunctionHeader(1, L(1, {"function", "f", "("}),
                                     TFPortList(3, L(3, {"integer", "size"})),
                                     L(1, {")", ";"})),
                      L(1, {"endfunction"}), L(1, {"// t is for true"}),
                      TaskHeader(1, L(1, {"task", "t", "(", ")", ";"})),
                      L(1, {"endtask"}), L(1, {"// class is about to end"})),
        L(0, {"endclass"}),
    },

    {
        "class import declarations",
        "class foo;\n"
        "  import fedex_pkg::box;\n"
        "  import fedex_pkg::*;\n"
        "endclass",
        ClassHeader(0, L(0, {"class", "foo", ";"})),
        ClassItemList(1, L(1, {"import", "fedex_pkg", "::", "box", ";"}),
                      L(1, {"import", "fedex_pkg", "::", "*", ";"})),
        L(0, {"endclass"}),
    },

    {
        "class macros as class item",
        "class macros_as_class_item;\n"
        " `uvm_warning()\n"
        " `uvm_error(  )\n"
        " `uvm_func_new(\n)\n"
        "endclass",
        ClassHeader(0, L(0, {"class", "macros_as_class_item", ";"})),
        ClassItemList(
                1,
                N(1, L(1, {"`uvm_warning", "("}),
                     L(1, {")"})),
                N(1, L(1, {"`uvm_error", "("}),
                     L(1, {")"})),
                N(1, L(1, {"`uvm_func_new", "("}),
                     L(1, {")"}))),
        L(0, {"endclass"}),
    },

    {
        "class with parameters as class items",
        "class params_as_class_item;\n"
        "  parameter N = 2;\n"
        "  parameter reg P = '1;\n"
        "  localparam M = f(glb::arr[N]) + 1;\n"
        "endclass",
        ClassHeader(0, L(0, {"class", "params_as_class_item", ";"})),
        ClassItemList(1, L(1, {"parameter", "N", "=", "2", ";"}),
                      L(1, {"parameter", "reg", "P", "=", "'1", ";"}),
                      L(1, {"localparam", "M", "=", "f", "(", "glb",
                            "::", "arr", "[", "N", "]", ")", "+", "1", ";"})),
        L(0, {"endclass"}),
    },

    {
        "class with events",
        "class event_calendar;\n"
        "  event birthday;\n"
        "  event first_date, anniversary;\n"
        "  event revolution[4:0], independence[2:0];\n"
        "endclass",
        ClassHeader(0, L(0, {"class", "event_calendar", ";"})),
        ClassItemList(
            1, DataDeclaration(1, L(1, {"event", "birthday", ";"})),
            DataDeclaration(
                1, L(1, {"event", "first_date", ",", "anniversary", ";"})),
            DataDeclaration(
                1, L(1, {"event", "revolution", "[", "4", ":", "0", "]", ",",
                         "independence", "[", "2", ":", "0", "]", ";"}))),
        L(0, {"endclass"}),
    },

    {
        "class with associative array declaration",
        "class Driver;\n"
        "  Packet pNP [*];\n"
        "  Packet pNP1 [*];\n"
        "endclass",
        ClassHeader(0, L(0, {"class", "Driver", ";"})),
        ClassItemList(1,
                      DataDeclaration(1, L(1, {"Packet", "pNP", "[*]", ";"})),
                      DataDeclaration(1, L(1, {"Packet", "pNP1", "[*]", ";"}))),
        L(0, {"endclass"}),
    },

    {
        "class member declarations",
        "class fields_with_modifiers;\n"
        "  const data_type_or_module_type foo1 = 4'hf;\n"
        "  static data_type_or_module_type foo3, foo4;\n"
        "endclass",
        ClassHeader(0, L(0, {"class", "fields_with_modifiers", ";"})),
        ClassItemList(
            1,
            DataDeclaration(1, L(1, {"const", "data_type_or_module_type",
                                     "foo1", "=", "4", "'h", "f", ";"})),
            DataDeclaration(1, L(1, {"static", "data_type_or_module_type",
                                     "foo3", ",", "foo4", ";"}))),
        L(0, {"endclass"}),
    },

    {
        "class with preprocessing",
        "class pp_class;\n"
        "`ifdef DEBUGGER\n"
        "`ifdef VERBOSE\n"  // nested, empty
        "`endif\n"
        "`endif\n"
        "endclass",
        ClassHeader(0, L(0, {"class", "pp_class", ";"})),
        ClassItemList(1, L(0, {"`ifdef", "DEBUGGER"}),
                      L(0, {"`ifdef", "VERBOSE"}), L(0, {"`endif"}),
                      L(0, {"`endif"})),
        L(0, {"endclass"}),
    },

    {
        "consecutive `define's",
        "class pp_class;\n"
        "`define FOO BAR\n"
        "`define BAR FOO+1\n"
        "endclass",
        ClassHeader(0, L(0, {"class", "pp_class", ";"})),
        ClassItemList(1, L(1, {"`define", "FOO", "BAR"}),
                      L(1, {"`define", "BAR", "FOO+1"})),
        L(0, {"endclass"}),
    },

    {
        "class pure virtual tasks",
        "class myclass;\n"
        "pure virtual task pure_task1;\n"
        "pure virtual task pure_task2(arg_type arg);\n"
        "endclass",
        ClassHeader(0, L(0, {"class", "myclass", ";"})),
        ClassItemList(1, L(1, {"pure", "virtual", "task", "pure_task1", ";"}),
                      L(1, {"pure", "virtual", "task", "pure_task2", "("}),
                      TFPortList(3, L(3, {"arg_type", "arg"})),
                      L(1, {")", ";"})),
        L(0, {"endclass"}),
    },

    {
        "nested classes",
        "class outerclass;\n"
        "  class innerclass;\n"
        "    class reallyinnerclass;\n"
        "      task subtask;\n"
        "      endtask\n"
        "    endclass\n"
        "  endclass\n"
        "endclass",
        ClassHeader(0, L(0, {"class", "outerclass", ";"})),
        ClassItemList(
            1, ClassHeader(1, L(1, {"class", "innerclass", ";"})),
            ClassItemList(
                2, ClassHeader(2, L(2, {"class", "reallyinnerclass", ";"})),
                ClassItemList(3, TaskHeader(3, L(3, {"task", "subtask", ";"})),
                              L(3, {"endtask"})),
                L(2, {"endclass"})),
            L(1, {"endclass"})),
        L(0, {"endclass"}),
    },

    {
        "class with protected members",
        "class protected_stuff;\n"
        "  protected int count;\n"
        "endclass",
        ClassHeader(0, L(0, {"class", "protected_stuff", ";"})),
        ClassItemList(
            1, DataDeclaration(1, L(1, {"protected", "int", "count", ";"}))),
        L(0, {"endclass"}),
    },

    {
        "class with virtual function",
        "class myclass;\n"
        "virtual function integer subroutine;\n"
        "  input a;\n"
        "  subroutine = a+42;\n"
        "endfunction\n"
        "endclass",
        ClassHeader(0, L(0, {"class", "myclass", ";"})),
        ClassItemList(
            1,
            FunctionHeader(
                1, L(1, {"virtual", "function", "integer", "subroutine", ";"})),
            TFPortList(2, L(2, {"input", "a", ";"})),
            StatementList(2, NL(2, {"subroutine", "=", "a", "+", "42", ";"})),
            L(1, {"endfunction"})),
        L(0, {"endclass"}),
    },

    {
        "class constructor",
        "class constructible;\n"
        "function new ();\n"
        "endfunction\n"
        "endclass",
        ClassHeader(0, L(0, {"class", "constructible", ";"})),
        ClassItemList(
            1, FunctionHeader(1, L(1, {"function", "new", "(", ")", ";"})),
            L(1, {"endfunction"})),
        L(0, {"endclass"}),
    },

    {
        "class foreach",
        "class myclass;\n"
        "function apkg::num_t apply_all();\n"
        "  foreach (this.foo[i]) begin\n"
        "    y = {y, this.foo[i]};\n"
        "    z = {z, super.bar[i]};\n"
        "  end\n"
        "  foreach (this.foo[i]) begin\n"
        "    y = {y, this.foo[i]};\n"
        "    z = {z, super.bar[i]};\n"
        "  end\n"
        "endfunction\n"
        "endclass",
        ClassHeader(0, L(0, {"class", "myclass", ";"})),
        ClassItemList(
            1,
            FunctionHeader(1, L(1, {"function", "apkg", "::", "num_t",
                                    "apply_all", "(", ")", ";"})),
            StatementList(
                2,
                FlowControl(
                    2,
                    L(2, {"foreach", "(", "this", ".", "foo", "[", "i", "]",
                          ")", "begin"}),
                    StatementList(3,
                                  NL(3, {"y", "=", "{", "y", ",", "this", ".",
                                        "foo", "[", "i", "]", "}", ";"}),
                                  NL(3, {"z", "=", "{", "z", ",", "super", ".",
                                        "bar", "[", "i", "]", "}", ";"})),
                    L(2, {"end"})),
                FlowControl(
                    2,
                    L(2, {"foreach", "(", "this", ".", "foo", "[", "i", "]",
                          ")", "begin"}),
                    StatementList(3,
                                  NL(3, {"y", "=", "{", "y", ",", "this", ".",
                                        "foo", "[", "i", "]", "}", ";"}),
                                  NL(3, {"z", "=", "{", "z", ",", "super", ".",
                                        "bar", "[", "i", "]", "}", ";"})),
                    L(2, {"end"}))),
            L(1, {"endfunction"})),
        L(0, {"endclass"}),
    },

    {
        "class with empty constraint",
        "class Foo; constraint empty_c { } endclass",
        ClassHeader(0, L(0, {"class", "Foo", ";"})),
        ClassItemList(1, L(1, {"constraint", "empty_c", "{", "}"})),
        L(0, {"endclass"}),
    },

    {
        "class with constraint, simple expression",
        "class Foo; constraint empty_c { c < d; } endclass",
        ClassHeader(0, L(0, {"class", "Foo", ";"})),
        ClassItemList(1, L(1, {"constraint", "empty_c", "{"}),
                      ConstraintItemList(2, L(2, {"c", "<", "d", ";"})),
                      L(1, {"}"})),
        L(0, {"endclass"}),
    },

    {
        "class with multiple constraint declarations",
        "class Foo; constraint empty1_c { } constraint empty2_c {} endclass",
        ClassHeader(0, L(0, {"class", "Foo", ";"})),
        ClassItemList(1, L(1, {"constraint", "empty1_c", "{", "}"}),
                      L(1, {"constraint", "empty2_c", "{", "}"})),
        L(0, {"endclass"}),
    },

    {
        "class with constraints",
        "class Foo; constraint bar_c { unique {baz}; } endclass",
        ClassHeader(0, L(0, {"class", "Foo", ";"})),
        ClassItemList(
            1, L(1, {"constraint", "bar_c", "{"}),
            ConstraintItemList(2, L(2, {"unique", "{", "baz", "}", ";"})),
            L(1, {"}"})),
        L(0, {"endclass"}),
    },

    {
        "class with constraints, multiple constraint expressions",
        "class Foo; constraint bar_c { soft z == y; unique {baz}; } endclass",
        ClassHeader(0, L(0, {"class", "Foo", ";"})),
        ClassItemList(
            1, L(1, {"constraint", "bar_c", "{"}),
            ConstraintItemList(2, L(2, {"soft", "z", "==", "y", ";"}),
                               L(2, {"unique", "{", "baz", "}", ";"})),
            L(1, {"}"})),
        L(0, {"endclass"}),
    },

    {
        "class with conditional constraint set, constraint expression list",
        "class Foo; constraint if_c { if (z) { soft x == y; } } endclass",
        ClassHeader(0, L(0, {"class", "Foo", ";"})),
        ClassItemList(
            1, L(1, {"constraint", "if_c", "{"}),
            ConstraintItemList(2, L(2, {"if", "(", "z", ")", "{"}),
                               ConstraintExpressionList(
                                   3, L(3, {"soft", "x", "==", "y", ";"})),
                               L(2, {"}"})),
            L(1, {"}"})),
        L(0, {"endclass"}),
    },

    {
        "class with nested conditional constraint set",
        "class Foo; constraint if_c { "
        "if (z) { if (p) { soft x == y; }} "
        "} endclass",
        ClassHeader(0, L(0, {"class", "Foo", ";"})),
        ClassItemList(
            1, L(1, {"constraint", "if_c", "{"}),
            ConstraintItemList(2, L(2, {"if", "(", "z", ")", "{"}),
                               ConstraintExpressionList(
                                   3, L(3, {"if", "(", "p", ")", "{"}),
                                   ConstraintExpressionList(
                                       4, L(4, {"soft", "x", "==", "y", ";"})),
                                   L(3, {"}"})),
                               L(2, {"}"})),
            L(1, {"}"})),
        L(0, {"endclass"}),
    },

    {
        "class with foreach constraint sets",
        "class Foo; constraint if_c { "
        "foreach (z) { soft x == y; } "
        "foreach (w) { soft y == z; } "
        "} endclass",
        ClassHeader(0, L(0, {"class", "Foo", ";"})),
        ClassItemList(1, L(1, {"constraint", "if_c", "{"}),
                      ConstraintItemList(
                          2, L(2, {"foreach", "(", "z", ")", "{"}),
                          ConstraintExpressionList(
                              3, L(3, {"soft", "x", "==", "y", ";"})),
                          L(2, {"}"}), L(2, {"foreach", "(", "w", ")", "{"}),
                          ConstraintExpressionList(
                              3, L(3, {"soft", "y", "==", "z", ";"})),
                          L(2, {"}"})),
                      L(1, {"}"})),
        L(0, {"endclass"}),
    },

    {
        "class with constraints, implication constraint expressions",
        "class Foo; constraint bar_c { "
        " z < y -> { unique {baz}; }"
        " a > b -> { soft p == q; }"
        " } endclass",
        ClassHeader(0, L(0, {"class", "Foo", ";"})),
        ClassItemList(
            1, L(1, {"constraint", "bar_c", "{"}),
            ConstraintItemList(
                2, L(2, {"z", "<", "y", "->", "{"}),
                ConstraintItemList(3, L(3, {"unique", "{", "baz", "}", ";"})),
                L(2, {"}"}), L(2, {"a", ">", "b", "->", "{"}),
                ConstraintItemList(3, L(3, {"soft", "p", "==", "q", ";"})),
                L(2, {"}"})),
            L(1, {"}"})),
        L(0, {"endclass"}),
    },

    {
        "class with constraints, distribution list",
        "class Foo; constraint bar_c { "
        " timer_enable dist { [0:9] :/ 20, 10 :/ 80 };"
        " } endclass",
        ClassHeader(0, L(0, {"class", "Foo", ";"})),
        ClassItemList(
            1, L(1, {"constraint", "bar_c", "{"}),
            ConstraintItemList(
                2, L(2, {"timer_enable", "dist", "{"}),
                DistItemList(3,
                             L(3, {"[", "0", ":", "9", "]", ":/", "20", ","}),
                             L(3, {"10", ":/", "80"})),
                L(2, {"}", ";"})),
            L(1, {"}"})),
        L(0, {"endclass"}),
    },

    {
        "class with empty parameter list",
        "class Foo #(); endclass",
        ClassHeader(0, L(0, {"class", "Foo", "#", "(", ")", ";"})),
        L(0, {"endclass"}),
    },

    {
        "class with one parameter list",
        "class Foo #(type a = b); endclass",
        ClassHeader(0, L(0, {"class", "Foo", "#", "("}),
                    ClassParameterList(2, L(2, {"type", "a", "=", "b"})),
                    L(0, {")", ";"})),
        L(0, {"endclass"}),
    },

    {
        "class with multiple parameter list",
        "class Foo #(type a = b, type c = d, type e = f); endclass",
        ClassHeader(0, L(0, {"class", "Foo", "#", "("}),
                    ClassParameterList(2, L(2, {"type", "a", "=", "b", ","}),
                                       L(2, {"type", "c", "=", "d", ","}),
                                       L(2, {"type", "e", "=", "f"})),
                    L(0, {")", ";"})),
        L(0, {"endclass"}),
    },
};

// Test that TreeUnwrapper produces the correct UnwrappedLines from class tests
TEST_F(TreeUnwrapperTest, UnwrapClassTests) {
  for (const auto& test_case : kClassTestCases) {
    VLOG(1) << "Test: " << test_case.test_name;
    auto tree_unwrapper = CreateTreeUnwrapper(test_case.source_code);
    const auto* uwline_tree = tree_unwrapper->Unwrap();
    EXPECT_TRUE(VerifyUnwrappedLines(&std::cout, *ABSL_DIE_IF_NULL(uwline_tree),
                                     test_case));
  }
}

const TreeUnwrapperTestData kUnwrapPackageTestCases[] = {
    {
        "empty package",
        "package foo_pkg;"
        "endpackage",
        L(0, {"package", "foo_pkg", ";"}),
        L(0, {"endpackage"}),
    },

    {
        "empty packages with end-labels",
        "package foo_pkg;"
        "endpackage : foo_pkg "
        "package bar_pkg;"
        "endpackage : bar_pkg",
        L(0, {"package", "foo_pkg", ";"}),
        L(0, {"endpackage", ":", "foo_pkg"}),
        L(0, {"package", "bar_pkg", ";"}),
        L(0, {"endpackage", ":", "bar_pkg"}),
    },

    {
        "package with one parameter declaration",
        "package foo_pkg;"
        "parameter size=4;"
        "endpackage",
        L(0, {"package", "foo_pkg", ";"}),
        PackageItemList(1, L(1, {"parameter", "size", "=", "4", ";"})),
        L(0, {"endpackage"}),
    },

    {
        "package with one localparam declaration",
        "package foo_pkg;"
        "localparam size=2;"
        "endpackage",
        L(0, {"package", "foo_pkg", ";"}),
        PackageItemList(1, L(1, {"localparam", "size", "=", "2", ";"})),
        L(0, {"endpackage"}),
    },

    {
        "package with two type declarations",
        "package foo_pkg;"
        "typedef enum {X=0,Y=1} bar_t;"
        "typedef enum {A=0,B=1} foo_t;"
        "endpackage",
        L(0, {"package", "foo_pkg", ";"}),
        PackageItemList(
            1, L(1, {"typedef", "enum", "{"}),
            EnumItemList(2, L(2, {"X", "=", "0", ","}), L(2, {"Y", "=", "1"})),
            L(1, {"}", "bar_t", ";"}),  //
            L(1, {"typedef", "enum", "{"}),
            EnumItemList(2, L(2, {"A", "=", "0", ","}), L(2, {"B", "=", "1"})),
            L(1, {"}", "foo_t", ";"})),
        L(0, {"endpackage"}),
    },

    // TODO(fangism): test packages with class declaration
};

// Test that TreeUnwrapper produces correct UnwrappedLines from package tests
TEST_F(TreeUnwrapperTest, UnwrapPackageTests) {
  for (const auto& test_case : kUnwrapPackageTestCases) {
    auto tree_unwrapper = CreateTreeUnwrapper(test_case.source_code);
    const auto* uwline_tree = tree_unwrapper->Unwrap();
    EXPECT_TRUE(VerifyUnwrappedLines(&std::cout, *ABSL_DIE_IF_NULL(uwline_tree),
                                     test_case));
  }
}

const TreeUnwrapperTestData kDescriptionTestCases[] = {
    {
        "one bind declaration",
        "bind foo bar#(.x(y)) baz(.clk(clk));",
        N(0,  // kBindDeclaration
          L(0, {"bind", "foo", "bar", "#", "("}),
          NL(2, {".", "x", "(", "y", ")"}), L(0, {")"}),
          InstanceList(
              2,                                                      //
              N(2,                                                    //
                L(2, {"baz", "("}),                                   //
                ModulePortList(4,                                     //
                               L(4, {".", "clk", "(", "clk", ")"})),  //
                L(2, {")", ";"})  // ';' is attached to end of bind directive
                ))),
    },

    {"multiple bind declarations",
     "bind foo bar baz();"
     "bind goo car caz();",
     N(0,  // kBindDeclaration
       L(0, {"bind", "foo", "bar"}),
       InstanceList(2, NL(2, {"baz", "(", ")", ";"}))),
     N(0,  // kBindDeclaration
       L(0, {"bind", "goo", "car"}),
       InstanceList(2, NL(2, {"caz", "(", ")", ";"})))},

    {
        "multi-instance bind declaration",
        "bind foo bar baz1(), baz2();",
        N(0,  // kBindDeclaration
          L(0, {"bind", "foo", "bar"}),
          InstanceList(2,                               //
                       NL(2, {"baz1", "(", ")", ","}),  //
                       NL(2, {"baz2", "(", ")", ";"})   //
                       )),
    },
};

// Test that TreeUnwrapper produces correct UnwrappedLines from package tests
TEST_F(TreeUnwrapperTest, DescriptionTests) {
  for (const auto& test_case : kDescriptionTestCases) {
    auto tree_unwrapper = CreateTreeUnwrapper(test_case.source_code);
    const auto* uwline_tree = tree_unwrapper->Unwrap();
    EXPECT_TRUE(VerifyUnwrappedLines(&std::cout, *ABSL_DIE_IF_NULL(uwline_tree),
                                     test_case));
  }
}

const TreeUnwrapperTestData kUnwrapPreprocessorTestCases[] = {
    {
        "consecutive `include's",
        "`include \"header1.vh\"\n"
        "`include \"path/header2.svh\"\n",
        L(0, {"`include", "\"header1.vh\""}),
        L(0, {"`include", "\"path/header2.svh\""}),
    },

    {
        "consecutive `define's",
        "`define FOO BAR\n"
        "`define BAR FOO+1\n",
        L(0, {"`define", "FOO", "BAR"}),
        L(0, {"`define", "BAR", "FOO+1"}),
    },

    {
        "consecutive `define's multiline",
        "`define FOO BAR \\\n"
        "  NONE\n"
        "`define BAR FOO+1 \\\n"
        "    -1\n",
        L(0, {"`define", "FOO", "BAR \\\n  NONE"}),
        L(0, {"`define", "BAR", "FOO+1 \\\n    -1"}),
    },

    {
        "`define's followed by top-level macro call",
        "`define FOO BAR\n"
        "`FOO(baz)\n",
        L(0, {"`define", "FOO", "BAR"}),
        L(0, {"`FOO", "(", "baz", ")"}),
    },

    {
        "consecutive `undef's",
        "`undef FOO\n"
        "`undef BAR\n",
        L(0, {"`undef", "FOO"}),
        L(0, {"`undef", "BAR"}),
    },

    {
        "preprocessor conditionals `ifdef's",
        "`ifdef BAR\n"
        "`else\n"
        "`endif\n"
        "`ifdef FOO\n"
        "`elsif XYZ\n"
        "`endif\n",
        L(0, {"`ifdef", "BAR"}),
        L(0, {"`else"}),
        L(0, {"`endif"}),
        L(0, {"`ifdef", "FOO"}),
        L(0, {"`elsif", "XYZ"}),
        L(0, {"`endif"}),
    },

    {
        "`define's surrounded by comments",
        "// leading comment\n"
        "`define FOO BAR\n"
        "`define BAR FOO+1\n"
        "// trailing comment\n",
        L(0, {"// leading comment"}),
        L(0, {"`define", "FOO", "BAR"}),
        L(0, {"`define", "BAR", "FOO+1"}),
        L(0, {"// trailing comment"}),
    },

    {
        "preprocessor conditionals `ifndef's",
        "`ifndef BAR\n"
        "`else\n"
        "`endif\n"
        "`ifndef FOO\n"
        "`elsif XYZ\n"
        "`endif\n",
        L(0, {"`ifndef", "BAR"}),
        L(0, {"`else"}),
        L(0, {"`endif"}),
        L(0, {"`ifndef", "FOO"}),
        L(0, {"`elsif", "XYZ"}),
        L(0, {"`endif"}),
    },

    {
        "`include's inside package should be indented as package items",
        "package includer;\n"
        "`include \"header1.vh\"\n"
        "`include \"path/header2.svh\"\n"
        "endpackage : includer\n",
        L(0, {"package", "includer", ";"}),
        PackageItemList(1, L(1, {"`include", "\"header1.vh\""}),
                        L(1, {"`include", "\"path/header2.svh\""})),
        L(0, {"endpackage", ":", "includer"}),
    },

    {
        "`defines's inside package should be indented as package items",
        "package definer;\n"
        "`define BAR\n"
        "`undef BAR\n"
        "endpackage : definer\n",
        L(0, {"package", "definer", ";"}),
        PackageItemList(1, L(1, {"`define", "BAR", ""}),
                        L(1, {"`undef", "BAR"})),
        L(0, {"endpackage", ":", "definer"}),
    },

    {
        "`ifdefs's inside package should be flushed left, but not items",
        "package ifdeffer;\n"
        "parameter three=3;"
        "`ifdef FOUR\n"
        "parameter size=4;"
        "`elsif FIVE\n"
        "parameter size=5;"
        "`else\n"
        "parameter size=6;"
        "`endif\n"
        "parameter foo=7;"
        "endpackage : ifdeffer\n",
        L(0, {"package", "ifdeffer", ";"}),
        PackageItemList(
            1, L(1, {"parameter", "three", "=", "3", ";"}),
            L(0, {"`ifdef", "FOUR"}),
            L(1, {"parameter", "size", "=", "4", ";"}),
            L(0, {"`elsif", "FIVE"}),
            L(1, {"parameter", "size", "=", "5", ";"}), L(0, {"`else"}),
            L(1, {"parameter", "size", "=", "6", ";"}), L(0, {"`endif"}),
            L(1, {"parameter", "foo", "=", "7", ";"})),
        L(0, {"endpackage", ":", "ifdeffer"}),
    },

    {
        "`include's inside module should be flushed left",
        "module includer;\n"
        "`include \"header1.vh\"\n"
        "`include \"path/header2.svh\"\n"
        "endmodule : includer\n",
        ModuleHeader(0, L(0, {"module", "includer", ";"})),
        ModuleItemList(1, L(1, {"`include", "\"header1.vh\""}),
                       L(1, {"`include", "\"path/header2.svh\""})),
        L(0, {"endmodule", ":", "includer"}),
    },

    {
        "`defines's inside module should be flushed left",
        "module definer;\n"
        "`define BAR\n"
        "`undef BAR\n"
        "endmodule : definer\n",
        ModuleHeader(0, L(0, {"module", "definer", ";"})),
        ModuleItemList(1, L(1, {"`define", "BAR", ""}),
                       L(1, {"`undef", "BAR"})),
        L(0, {"endmodule", ":", "definer"}),
    },

    {
        "`ifdefs's inside module should be flushed left, but not items",
        "module foo;\n"
        "always_comb begin\n"
        "  x = y;\n"
        "`ifdef FOO\n"
        "  z = 0;\n"
        "`endif\n"
        "  w = z;\n"
        "end\n"
        "endmodule",
        ModuleHeader(0, L(0, {"module", "foo", ";"})),
        ModuleItemList(
            1, L(1, {"always_comb", "begin"}),
            StatementList(2, NL(2, {"x", "=", "y", ";"}),
                          L(0, {"`ifdef", "FOO"}), NL(2, {"z", "=", "0", ";"}),
                          L(0, {"`endif"}), NL(2, {"w", "=", "z", ";"})),
            L(1, {"end"})),
        L(0, {"endmodule"}),
    },

    {
        "`ifdefs's inside module should flush left, even with leading comment",
        "module foo;\n"
        "// comment\n"
        "`ifdef SIM\n"
        "  wire w;\n"
        "`endif\n"
        "endmodule",
        ModuleHeader(0, L(0, {"module", "foo", ";"})),
        ModuleItemList(1, L(1, {"// comment"}), L(0, {"`ifdef", "SIM"}),
                       L(1, {"wire", "w", ";"}), L(0, {"`endif"})),
        L(0, {"endmodule"}),
    },

    {
        "`ifndefs's inside module should flush left, even with leading comment",
        "module foo;\n"
        "// comment\n"
        "`ifndef SIM\n"
        "`endif\n"
        "  wire w;\n"
        "endmodule",
        ModuleHeader(0, L(0, {"module", "foo", ";"})),
        ModuleItemList(1, L(1, {"// comment"}), L(0, {"`ifndef", "SIM"}),
                       L(0, {"`endif"}), L(1, {"wire", "w", ";"})),
        L(0, {"endmodule"}),
    },

    {
        "module items with preprocessor conditionals and comments",
        "module foo;\n"
        "// comment1\n"
        "`ifdef SIM\n"
        "// comment2\n"
        "`elsif SYN\n"
        "// comment3\n"
        "`else\n"
        "// comment4\n"
        "`endif\n"
        "// comment5\n"
        "endmodule",
        ModuleHeader(0, L(0, {"module", "foo", ";"})),
        ModuleItemList(1, L(1, {"// comment1"}), L(0, {"`ifdef", "SIM"}),
                       L(1, {"// comment2"}), L(0, {"`elsif", "SYN"}),
                       L(1, {"// comment3"}), L(0, {"`else"}),
                       L(1, {"// comment4"}), L(0, {"`endif"}),
                       L(1, {"// comment5"})),
        L(0, {"endmodule"}),
    },

    // TODO(fangism): decide/test/support indenting preprocessor directives
    // nested inside `ifdefs.  Should `define inside `ifdef be indented?
};

// Test for correct UnwrappedLines for preprocessor directives.
TEST_F(TreeUnwrapperTest, UnwrapPreprocessorTests) {
  for (const auto& test_case : kUnwrapPreprocessorTestCases) {
    auto tree_unwrapper = CreateTreeUnwrapper(test_case.source_code);
    const auto* uwline_tree = tree_unwrapper->Unwrap();
    EXPECT_TRUE(VerifyUnwrappedLines(&std::cout, *ABSL_DIE_IF_NULL(uwline_tree),
                                     test_case));
  }
}

// Interface declarations are structured just like module declarations.
const TreeUnwrapperTestData kUnwrapInterfaceTestCases[] = {
    {
        "empty interface",
        "interface foo_if;"
        "endinterface",
        ModuleHeader(0, L(0, {"interface", "foo_if", ";"})),
        L(0, {"endinterface"}),
    },

    {
        "empty interfaces with end-labels",
        "interface foo_if;"
        "endinterface : foo_if "
        "interface bar_if;"
        "endinterface : bar_if",
        ModuleHeader(0, L(0, {"interface", "foo_if", ";"})),
        L(0, {"endinterface", ":", "foo_if"}),
        ModuleHeader(0, L(0, {"interface", "bar_if", ";"})),
        L(0, {"endinterface", ":", "bar_if"}),
    },

    {
        "interface with one parameter declaration",
        "interface foo_if;"
        "parameter size=4;"
        "endinterface",
        ModuleHeader(0, L(0, {"interface", "foo_if", ";"})),
        ModuleItemList(1, L(1, {"parameter", "size", "=", "4", ";"})),
        L(0, {"endinterface"}),
    },

    {
        "interface with one localparam declaration",
        "interface foo_if;"
        "localparam size=2;"
        "endinterface",
        ModuleHeader(0, L(0, {"interface", "foo_if", ";"})),
        ModuleItemList(1, L(1, {"localparam", "size", "=", "2", ";"})),
        L(0, {"endinterface"}),
    },

    // modport declarations
    {
        "interface with modport declarations",
        "interface foo_if;"
        "modport mp1 (output a, input b);"
        "modport mp2 (output c, input d);"
        "endinterface",
        ModuleHeader(0, L(0, {"interface", "foo_if", ";"})),
        ModuleItemList(1,  //
                       N(1, L(1, {"modport", "mp1", "("}),
                         N(3, L(3, {"output", "a", ","})),
                         N(3, L(3, {"input", "b"})), L(1, {")", ";"})),
                       N(1, L(1, {"modport", "mp2", "("}),
                         N(3, L(3, {"output", "c", ","})),
                         N(3, L(3, {"input", "d"})), L(1, {")", ";"}))),
        L(0, {"endinterface"}),
    },
    {
        "interface with modport TF ports",
        "interface foo_if;"
        "modport mp1 (output a, input b, import c);"
        "endinterface",
        ModuleHeader(0, L(0, {"interface", "foo_if", ";"})),
        ModuleItemList(
            1,  //
            N(1, L(1, {"modport", "mp1", "("}),
              N(3, L(3, {"output", "a", ","})), N(3, L(3, {"input", "b", ","})),
              N(3, L(3, {"import", "c"})), L(1, {")", ";"}))),
        L(0, {"endinterface"}),
    },
    {
        "interface with more modport ports",
        "interface foo_if;"
        "modport mp1 (output a1, a2, input b1, b2, import c1, c2);"
        "endinterface",
        ModuleHeader(0, L(0, {"interface", "foo_if", ";"})),
        ModuleItemList(
            1,  //
            N(1, L(1, {"modport", "mp1", "("}),
              N(3, L(3, {"output", "a1", ",", "a2", ","})),
              N(3, L(3, {"input", "b1", ",", "b2", ","})),
              N(3, L(3, {"import", "c1", ",", "c2"})), L(1, {")", ";"}))),
        L(0, {"endinterface"}),
    },
    {
        "interface with modport and comments between ports",
        "interface foo_if;"
        " modport mp1(\n"
        "  // Our output\n"
        "     output a,\n"
        "  /* Inputs */\n"
        "      input b1, b_f /*last*/,"
        "  import c\n"
        "  );\n"
        "endinterface",
        ModuleHeader(0, L(0, {"interface", "foo_if", ";"})),
        ModuleItemList(
            1,  //
            N(1, L(1, {"modport", "mp1", "("}),
              N(3, L(3, {"// Our output"}), L(3, {"output", "a", ","})),
              N(3, L(3, {"/* Inputs */"}),
                L(3, {"input", "b1", ",", "b_f", "/*last*/", ","})),
              N(3, L(3, {"import", "c"})), L(1, {")", ";"}))),
        L(0, {"endinterface"}),
    },
};

// Test that TreeUnwrapper produces correct UnwrappedLines from interface tests
TEST_F(TreeUnwrapperTest, UnwrapInterfaceTests) {
  for (const auto& test_case : kUnwrapInterfaceTestCases) {
    auto tree_unwrapper = CreateTreeUnwrapper(test_case.source_code);
    const auto* uwline_tree = tree_unwrapper->Unwrap();
    EXPECT_TRUE(VerifyUnwrappedLines(&std::cout, *ABSL_DIE_IF_NULL(uwline_tree),
                                     test_case));
  }
}

const TreeUnwrapperTestData kUnwrapTaskTestCases[] = {
    {
        "empty task",
        "task foo;"
        "endtask",
        TaskHeader(0, L(0, {"task", "foo", ";"})),
        L(0, {"endtask"}),
    },

    {
        "two empty tasks",
        "task foo;"
        "endtask "
        "task bar;"
        "endtask",
        TaskHeader(0, L(0, {"task", "foo", ";"})),
        L(0, {"endtask"}),
        TaskHeader(0, L(0, {"task", "bar", ";"})),
        L(0, {"endtask"}),
    },

    {
        "empty task, statement comment",
        "task foo;\n"
        "// statement comment\n"
        "endtask",
        TaskHeader(0, L(0, {"task", "foo", ";"})),
        StatementList(1, L(1, {"// statement comment"})),
        L(0, {"endtask"}),
    },

    {
        "empty task, empty ports, statement comment",
        "task foo();\n"
        "// statement comment\n"
        "endtask",
        TaskHeader(0, L(0, {"task", "foo", "(", ")", ";"})),
        StatementList(1, L(1, {"// statement comment"})),
        L(0, {"endtask"}),
    },

    {
        "empty task with qualifier",
        "task automatic foo;"
        "endtask",
        TaskHeader(0, L(0, {"task", "automatic", "foo", ";"})),
        L(0, {"endtask"}),
    },

    {
        "task with empty formal arguments",
        "task foo();"
        "endtask",
        TaskHeader(0, L(0, {"task", "foo", "(", ")", ";"})),
        L(0, {"endtask"}),
    },

    {
        "task with formal argument",
        "task foo(string name);"
        "endtask",
        TaskHeader(0, L(0, {"task", "foo", "("}),
                   TFPortList(2, L(2, {"string", "name"})), L(0, {")", ";"})),
        L(0, {"endtask"}),
    },

    {
        "task with multiple formal arguments",
        "task foo(string name, int a);"
        "endtask",
        TaskHeader(
            0, L(0, {"task", "foo", "("}),
            TFPortList(2, L(2, {"string", "name", ","}), L(2, {"int", "a"})),
            L(0, {")", ";"})),
        L(0, {"endtask"}),
    },

    {
        "task with local variable",
        "task foo;"
        "int return_value;"
        "endtask",
        TaskHeader(0, L(0, {"task", "foo", ";"})),
        StatementList(1,
                      DataDeclaration(1, L(1, {"int", "return_value", ";"}))),
        L(0, {"endtask"}),
    },

    {
        "task with multiple local variables in single declaration",
        "task foo;"
        "int r1, r2;"
        "endtask",
        TaskHeader(0, L(0, {"task", "foo", ";"})),
        StatementList(1, DataDeclaration(1, NL(1, {"int"}),     //
                                         N(3,                   //
                                           NL(3, {"r1", ","}),  //
                                           NL(3, {"r2", ";"})   //
                                           ))),
        L(0, {"endtask"}),
    },

    {
        "task with subtask call",
        "task foo;"
        "$makeitso(x);"
        "endtask",
        TaskHeader(0, L(0, {"task", "foo", ";"})),
        StatementList(1, NL(1, {"$makeitso", "(", "x", ")", ";"})),
        L(0, {"endtask"}),
    },

    {
        "task with assignment to call expression",
        "task foo;"
        "y = makeitso(x);"
        "endtask",
        TaskHeader(0, L(0, {"task", "foo", ";"})),
        StatementList(1, NL(1, {"y", "=", "makeitso", "(", "x", ")", ";"})),
        L(0, {"endtask"}),
    },

    {
        "task with delayed assignment",
        "task foo;"
        "#100 "
        "bar = 13;"
        "endtask",
        TaskHeader(0, L(0, {"task", "foo", ";"})),
        StatementList(1, NL(1, {"#", "100", "bar", "=", "13", ";"})),
        L(0, {"endtask"}),
    },

    {
        "task with multiple nonblocking assignments",
        "task t; a<=b; c<=d; endtask",
        TaskHeader(0, L(0, {"task", "t", ";"})),
        StatementList(1, NL(1, {"a", "<=", "b", ";"}),
                      NL(1, {"c", "<=", "d", ";"})),
        L(0, {"endtask"}),
    },

    {
        "task with empty fork-join pairs",
        "task forkit;"
        "fork join fork join "
        "endtask",
        TaskHeader(0, L(0, {"task", "forkit", ";"})),
        StatementList(1, L(1, {"fork"}), L(1, {"join"}), L(1, {"fork"}),
                      L(1, {"join"})),
        L(0, {"endtask"}),
    },

    {
        "task with empty fork-join pairs, labeled",
        "task forkit;"
        "fork:a join:a fork:b join:b "
        "endtask",
        TaskHeader(0, L(0, {"task", "forkit", ";"})),
        StatementList(1, L(1, {"fork", ":", "a"}), L(1, {"join", ":", "a"}),
                      L(1, {"fork", ":", "b"}), L(1, {"join", ":", "b"})),
        L(0, {"endtask"}),
    },

    {
        "task with fork-join around comments",
        "task forkit;"
        "fork\n"
        "// comment1\n"
        "join\n"
        "fork\n"
        "// comment2\n"
        "join "
        "endtask",
        TaskHeader(0, L(0, {"task", "forkit", ";"})),
        StatementList(1, L(1, {"fork"}),
                      StatementList(2, L(2, {"// comment1"})),  //
                      L(1, {"join"}),                           //
                      L(1, {"fork"}),                           //
                      StatementList(2, L(2, {"// comment2"})),  //
                      L(1, {"join"})),
        L(0, {"endtask"}),
    },

    {
        "task with fork-join",
        "task foo;"
        "fork "
        "int value;"
        "join "
        "endtask",
        TaskHeader(0, L(0, {"task", "foo", ";"})),
        StatementList(
            1, L(1, {"fork"}),
            StatementList(2, DataDeclaration(2, L(2, {"int", "value", ";"}))),
            L(1, {"join"})),
        L(0, {"endtask"}),
    },

    {
        "task with sequential-block inside parallel-block",
        "task fj; fork foo(); begin end join endtask",
        TaskHeader(0, L(0, {"task", "fj", ";"})),
        StatementList(1, L(1, {"fork"}),
                      StatementList(2,                             //
                                    NL(2, {"foo", "(", ")", ";"}), //
                                    L(2, {"begin"}),               //
                                    L(2, {"end"})),
                      L(1, {"join"})),
        L(0, {"endtask"}),
    },

    // TODO(fangism): "task with while loop and single statement"

    {
        "task with while loop and block statement",
        "task foo;"
        "while (1) begin "
        "$makeitso(x);"
        "end "
        "endtask",
        TaskHeader(0, L(0, {"task", "foo", ";"})),
        StatementList(1, FlowControl(1, L(1, {"while", "(", "1", ")", "begin"}),
                                     StatementList(2, NL(2, {"$makeitso", "(",
                                                            "x", ")", ";"})),
                                     L(1, {"end"}))),
        L(0, {"endtask"}),
    },

    {
        "task with formal parameters declared in-body",
        "task automatic clean_up;"
        "input logic addr;"
        "input logic mask;"
        "endtask",
        TaskHeader(0, L(0, {"task", "automatic", "clean_up", ";"})),
        StatementList(1, L(1, {"input", "logic", "addr", ";"}),
                      L(1, {"input", "logic", "mask", ";"})),
        L(0, {"endtask"}),
    },

    {
        "task with delayed assignment",
        "class c; task automatic waiter;"
        "#0 z = v; endtask endclass",
        ClassHeader(0, L(0, {"class", "c", ";"})),
        ClassItemList(
            1, TaskHeader(1, L(1, {"task", "automatic", "waiter", ";"})),
            StatementList(
                2,                                      //
                NL(2, {"#", "0", "z", "=", "v", ";"})),  // delayed assignment
            L(1, {"endtask"})),
        L(0, {"endclass"}),
    },

    {
        "task with labeled assignment",
        "class c; task automatic waiter;"
        "foo: z = v; endtask endclass",
        ClassHeader(0, L(0, {"class", "c", ";"})),
        ClassItemList(
            1, TaskHeader(1, L(1, {"task", "automatic", "waiter", ";"})),
            StatementList(
                2,                                        //
                NL(2, {"foo", ":", "z", "=", "v", ";"})),  // labeled assignment
            L(1, {"endtask"})),
        L(0, {"endclass"}),
    },

    {
        "task with labeled and delayed assignment",
        "class c; task automatic waiter;"
        "foo: #1 z = v; endtask endclass",
        ClassHeader(0, L(0, {"class", "c", ";"})),
        ClassItemList(
            1, TaskHeader(1, L(1, {"task", "automatic", "waiter", ";"})),
            StatementList(2,  //
                          NL(2, {"foo", ":", "#", "1", "z", "=", "v",
                                ";"})),  // labeled and delayed assignment
            L(1, {"endtask"})),
        L(0, {"endclass"}),
    },

    {
        "task with procedural timing control statement and null-statement",
        "class c; task automatic waiter;"
        "#0; return; endtask endclass",
        ClassHeader(0, L(0, {"class", "c", ";"})),
        ClassItemList(1,
                      TaskHeader(1, L(1, {"task", "automatic", "waiter", ";"})),
                      StatementList(2,                      //
                                    NL(2, {"#", "0", ";"}),  // timing control
                                    NL(2, {"return", ";"})),
                      L(1, {"endtask"})),
        L(0, {"endclass"}),
    },

    {
        "task with simple event control statement and null-statement",
        "class c; task automatic clocker;"
        "@(posedge clk); endtask endclass",
        ClassHeader(0, L(0, {"class", "c", ";"})),
        ClassItemList(
            1, TaskHeader(1, L(1, {"task", "automatic", "clocker", ";"})),
            StatementList(
                2,                                            //
                NL(2, {"@", "(", "posedge", "clk", ")", ";"})  // event control
                ),
            L(1, {"endtask"})),
        L(0, {"endclass"}),
    },

    {
        "task with multiple event control statements",
        "class c; task automatic clocker;"
        "@(posedge clk); @(negedge clk); endtask endclass",
        ClassHeader(0, L(0, {"class", "c", ";"})),
        ClassItemList(
            1, TaskHeader(1, L(1, {"task", "automatic", "clocker", ";"})),
            StatementList(2,  //
                          NL(2, {"@", "(", "posedge", "clk", ")", ";"}),
                          NL(2, {"@", "(", "negedge", "clk", ")", ";"})),
            L(1, {"endtask"})),
        L(0, {"endclass"}),
    },

    {
        "task with repeated event control statement and null-statement",
        "class c; task automatic clocker;"
        "repeat (2) @(posedge clk); endtask endclass",
        ClassHeader(0, L(0, {"class", "c", ";"})),
        ClassItemList(
            1, TaskHeader(1, L(1, {"task", "automatic", "clocker", ";"})),
            StatementList(2,              //
                          FlowControl(2,  //
                                      L(2, {"repeat", "(", "2", ")"}),
                                      NL(3, {"@", "(", "posedge", "clk", ")",
                                             ";"})  // event control
                                      )),
            L(1, {"endtask"})),
        L(0, {"endclass"}),
    },

    {
        "task with multiple repeat event control statements",
        "class c; task automatic clocker;"
        "repeat (2) @(posedge clk);"
        "repeat (4) @(negedge clk); endtask endclass",
        ClassHeader(0, L(0, {"class", "c", ";"})),
        ClassItemList(
            1, TaskHeader(1, L(1, {"task", "automatic", "clocker", ";"})),
            StatementList(
                2,              //
                FlowControl(2,  //
                            L(2, {"repeat", "(", "2", ")"}),
                            NL(3, {"@", "(", "posedge", "clk", ")", ";"})),
                FlowControl(2,  //
                            L(2, {"repeat", "(", "4", ")"}),
                            NL(3, {"@", "(", "negedge", "clk", ")", ";"}))),
            L(1, {"endtask"})),
        L(0, {"endclass"}),
    },

    {
        "task with nested repeated event control statements",
        "class c; task automatic clocker;"
        "repeat (n) repeat (m) @(posedge clk); endtask endclass",
        ClassHeader(0, L(0, {"class", "c", ";"})),
        ClassItemList(
            1, TaskHeader(1, L(1, {"task", "automatic", "clocker", ";"})),
            StatementList(2,              //
                          FlowControl(2,  //
                                      L(2, {"repeat", "(", "n", ")", "repeat",
                                            "(", "m", ")"}),
                                      NL(3, {"@", "(", "posedge", "clk", ")",
                                             ";"})  // single null-statement
                                      )),
            L(1, {"endtask"})),
        L(0, {"endclass"}),
    },

    {
        "task with nested if statements, single-statement body",
        "class c; task automatic iffer;"
        "if (n) if (m) y = x; endtask endclass",
        ClassHeader(0, L(0, {"class", "c", ";"})),
        ClassItemList(
            1, TaskHeader(1, L(1, {"task", "automatic", "iffer", ";"})),
            StatementList(
                2,  //
                FlowControl(
                    2,  //
                    L(2, {"if", "(", "n", ")", "if", "(", "m", ")"}),
                    NL(3, {"y", "=", "x", ";"})  // single statement body
                    )),
            L(1, {"endtask"})),
        L(0, {"endclass"}),
    },

    {
        "task with assert statements",
        "task t; Fire(); assert (x); assert(y); endtask",
        TaskHeader(0, L(0, {"task", "t", ";"})),
        StatementList(1,  //
                      NL(1, {"Fire", "(", ")", ";"}),
                      NL(1, {"assert", "(", "x", ")", ";"}),
                      NL(1, {"assert", "(", "y", ")", ";"})),
        L(0, {"endtask"}),
    },

    {
        "task with wait statements",
        "task t; wait (a==b); wait(c<d); endtask",
        TaskHeader(0, L(0, {"task", "t", ";"})),
        StatementList(1,  //
                      NL(1, {"wait", "(", "a", "==", "b", ")", ";"}),
                      NL(1, {"wait", "(", "c", "<", "d", ")", ";"})),
        L(0, {"endtask"}),
    },

    {
        "task with wait fork statements",
        "task t; wait fork ; wait fork; endtask",
        TaskHeader(0, L(0, {"task", "t", ";"})),
        StatementList(1,  //
                      NL(1, {"wait", "fork", ";"}), NL(1, {"wait", "fork", ";"})),
        L(0, {"endtask"}),
    },

    // TODO(fangism): test calls to UVM macros
};

// Test that TreeUnwrapper produces correct UnwrappedLines from task tests
TEST_F(TreeUnwrapperTest, UnwrapTaskTests) {
  for (const auto& test_case : kUnwrapTaskTestCases) {
    auto tree_unwrapper = CreateTreeUnwrapper(test_case.source_code);
    const auto* uwline_tree = tree_unwrapper->Unwrap();
    EXPECT_TRUE(VerifyUnwrappedLines(&std::cout, *ABSL_DIE_IF_NULL(uwline_tree),
                                     test_case));
  }
}

const TreeUnwrapperTestData kUnwrapFunctionTestCases[] = {
    {
        "empty function",
        "function foo;"
        "endfunction : foo",
        FunctionHeader(0, L(0, {"function", "foo", ";"})),
        L(0, {"endfunction", ":", "foo"}),
    },

    {
        "empty function, comment statement",
        "function foo;// foo does x\n"
        "// statement comment\n"
        "endfunction : foo",
        FunctionHeader(0, L(0, {"function", "foo", ";", "// foo does x"})),
        StatementList(1, L(1, {"// statement comment"})),
        L(0, {"endfunction", ":", "foo"}),
    },

    {
        "two empty functions",
        "function funk;"
        "endfunction : funk "
        "function foo;"
        "endfunction : foo",
        FunctionHeader(0, L(0, {"function", "funk", ";"})),
        L(0, {"endfunction", ":", "funk"}),
        FunctionHeader(0, L(0, {"function", "foo", ";"})),
        L(0, {"endfunction", ":", "foo"}),
    },

    {
        "empty function, empty ports, comment statement",
        "function foo();// foo\n"
        "// statement comment\n"
        "endfunction : foo",
        FunctionHeader(0, L(0, {"function", "foo", "(", ")", ";", "// foo"})),
        StatementList(1, L(1, {"// statement comment"})),
        L(0, {"endfunction", ":", "foo"}),
    },

    {
        "function with empty formal arguments",
        "function void foo();"
        "endfunction",
        FunctionHeader(0, L(0, {"function", "void", "foo", "(", ")", ";"})),
        L(0, {"endfunction"}),
    },

    {
        "function with formal argument",
        "function foo(string name);"
        "endfunction : foo",
        FunctionHeader(0, L(0, {"function", "foo", "("}),
                       TFPortList(2, L(2, {"string", "name"})),
                       L(0, {")", ";"})),
        L(0, {"endfunction", ":", "foo"}),
    },

    {
        "function with multiple formal arguments",
        "function foo(string name, int a);"
        "endfunction",
        FunctionHeader(
            0, L(0, {"function", "foo", "("}),
            TFPortList(2, L(2, {"string", "name", ","}), L(2, {"int", "a"})),
            L(0, {")", ";"})),
        L(0, {"endfunction"}),
    },

    {
        "function with local variable",
        "function foo;"
        "int value;"
        "endfunction",
        FunctionHeader(0, L(0, {"function", "foo", ";"})),
        StatementList(1, DataDeclaration(1, L(1, {"int", "value", ";"}))),
        L(0, {"endfunction"}),
    },

    {
        "function with assignment to call expression",
        "function foo;"
        "y = twister(x, 1);"
        "endfunction",
        FunctionHeader(0, L(0, {"function", "foo", ";"})),
        StatementList(
            1, NL(1, {"y", "=", "twister", "(", "x", ",", "1", ")", ";"})),
        L(0, {"endfunction"}),
    },

    {
        "function with multiple statements",
        "function foo;"
        "y = twister(x, 1);"
        "z = twister(x, 2);"
        "endfunction",
        FunctionHeader(0, L(0, {"function", "foo", ";"})),
        StatementList(
            1, NL(1, {"y", "=", "twister", "(", "x", ",", "1", ")", ";"}),
            NL(1, {"z", "=", "twister", "(", "x", ",", "2", ")", ";"})),
        L(0, {"endfunction"}),
    },

    {
        "function with foreach block with multiple statements",
        "function foo;"
        "foreach (x[i]) begin "
        "y = twister(x[i], 1);"
        "z = twister(x[i], 2);"
        "end "
        "endfunction",
        FunctionHeader(0, L(0, {"function", "foo", ";"})),
        StatementList(
            1, FlowControl(
                   1, L(1, {"foreach", "(", "x", "[", "i", "]", ")", "begin"}),
                   StatementList(2,
                                 NL(2, {"y", "=", "twister", "(", "x", "[", "i",
                                       "]", ",", "1", ")", ";"}),
                                 NL(2, {"z", "=", "twister", "(", "x", "[", "i",
                                       "]", ",", "2", ")", ";"})),
                   L(1, {"end"}))),
        L(0, {"endfunction"}),
    },

    {
        "function with foreach block with single statements",
        "function foo;"
        "foreach (x[i]) y = twister(x[i], 1);"
        "endfunction",
        FunctionHeader(0, L(0, {"function", "foo", ";"})),
        StatementList(
            1, FlowControl(1,  //
                           L(1, {"foreach", "(", "x", "[", "i", "]", ")"}),
                           NL(2, {"y", "=", "twister", "(", "x", "[", "i", "]",
                                  ",", "1", ")", ";"}))),
        L(0, {"endfunction"}),
    },

    {
        "function with nested foreach block with single statements",
        "function foo;"
        "foreach (x[i]) foreach(j[k]) y = x;"
        "endfunction",
        FunctionHeader(0, L(0, {"function", "foo", ";"})),
        StatementList(
            1, FlowControl(1,                                                //
                           L(1, {"foreach", "(", "x", "[", "i", "]", ")",    //
                                 "foreach", "(", "j", "[", "k", "]", ")"}),  //
                           NL(2, {"y", "=", "x", ";"}))),
        L(0, {"endfunction"}),
    },

    {
        "function with assignment to macro call",
        "function foo;"
        "y = `TWISTER(x, y);"
        "endfunction",
        FunctionHeader(0, L(0, {"function", "foo", ";"})),
        StatementList(
            1,
            N(1, L(1, {"y", "=", "`TWISTER", "("}),
              MacroArgList(2, L(2, {"x", ",", "y"})),
              L(1, {")", ";"}))),
        L(0, {"endfunction"}),
    },

    {
        "function with array formal parameters and return statement",
        "function automatic logic checkit ("
        "input logic [4:0] a,"
        "input logic [4:0] b);"
        "return a ^ b;"
        "endfunction",
        FunctionHeader(
            0, L(0, {"function", "automatic", "logic", "checkit", "("}),
            TFPortList(
                2, L(2, {"input", "logic", "[", "4", ":", "0", "]", "a", ","}),
                L(2, {"input", "logic", "[", "4", ":", "0", "]", "b"})),
            L(0, {")", ";"})),
        StatementList(1, NL(1, {"return", "a", "^", "b", ";"})),
        L(0, {"endfunction"}),
    },

    {
        "function with formal parameters declared in-body",
        "function automatic index_t make_index;"
        "input logic [1:0] addr;"
        "input mode_t mode;"
        "input logic [2:0] hash_mask;"
        "endfunction",
        FunctionHeader(
            0, L(0, {"function", "automatic", "index_t", "make_index", ";"})),
        StatementList(
            1, L(1, {"input", "logic", "[", "1", ":", "0", "]", "addr", ";"}),
            L(1, {"input", "mode_t", "mode", ";"}),
            L(1,
              {"input", "logic", "[", "2", ":", "0", "]", "hash_mask", ";"})),
        L(0, {"endfunction"}),
    },

    {
        "function with back-to-back if statements",
        "function foo;"
        "if (zz) begin "
        "return 0;"
        "end "
        "if (yy) begin "
        "return 1;"
        "end "
        "endfunction",
        FunctionHeader(0, L(0, {"function", "foo", ";"})),
        StatementList(1,
                      FlowControl(1, L(1, {"if", "(", "zz", ")", "begin"}),
                                  StatementList(2, NL(2, {"return", "0", ";"})),
                                  L(1, {"end"})),  //
                      FlowControl(1, L(1, {"if", "(", "yy", ")", "begin"}),
                                  StatementList(2, NL(2, {"return", "1", ";"})),
                                  L(1, {"end"}))),
        L(0, {"endfunction"}),
    },

    {
        "function with back-to-back if statements, single-statement body",
        "function foo;"
        "if (zz) "
        "return 0;"
        "if (yy) "
        "return 1;"
        "endfunction",
        FunctionHeader(0, L(0, {"function", "foo", ";"})),
        StatementList(1,
                      FlowControl(1,  //
                                  L(1, {"if", "(", "zz", ")"}),
                                  NL(2, {"return", "0", ";"})),
                      FlowControl(1,  //
                                  L(1, {"if", "(", "yy", ")"}),
                                  NL(2, {"return", "1", ";"}))),
        L(0, {"endfunction"}),
    },

    {
        "function with if-else branches in begin/end",
        "function foo;"
        "if (zz) begin "
        "return 0;"
        "end else begin "
        "return 1;"
        "end "
        "endfunction",
        FunctionHeader(0, L(0, {"function", "foo", ";"})),
        StatementList(1,
                      FlowControl(1,  //
                                  L(1, {"if", "(", "zz", ")", "begin"}),
                                  StatementList(2, NL(2, {"return", "0", ";"})),
                                  L(1, {"end", "else", "begin"}),
                                  StatementList(2, NL(2, {"return", "1", ";"})),
                                  L(1, {"end"}))),
        L(0, {"endfunction"}),
    },

    {
        "function with if-else branches, single-statements",
        "function foo;"
        "if (zz) "
        "return 0;"
        "else "
        "return 1;"
        "endfunction",
        FunctionHeader(0, L(0, {"function", "foo", ";"})),
        StatementList(1,
                      FlowControl(1,  //
                                  L(1, {"if", "(", "zz", ")"}),
                                  NL(2, {"return", "0", ";"}), L(1, {"else"}),
                                  NL(2, {"return", "1", ";"}))),
        L(0, {"endfunction"}),
    },

    {
        "function with else-if branches in begin/end",
        "function foo;"
        "if (zz) begin "
        "return 0;"
        "end else if (yy) begin "
        "return 1;"
        "end "
        "endfunction",
        FunctionHeader(0, L(0, {"function", "foo", ";"})),
        StatementList(
            1,
            FlowControl(1,  //
                        L(1, {"if", "(", "zz", ")", "begin"}),
                        StatementList(2, NL(2, {"return", "0", ";"})),
                        L(1, {"end", "else", "if", "(", "yy", ")", "begin"}),
                        StatementList(2, NL(2, {"return", "1", ";"})),
                        L(1, {"end"}))),
        L(0, {"endfunction"}),
    },

    {
        "function with else-if branches, single-statements",
        "function foo;"
        "if (zz) "
        "return 0;"
        "else if (yy) "
        "return 1;"
        "endfunction",
        FunctionHeader(0, L(0, {"function", "foo", ";"})),
        StatementList(1,
                      FlowControl(1,  //
                                  L(1, {"if", "(", "zz", ")"}),
                                  NL(2, {"return", "0", ";"}),
                                  L(1, {"else", "if", "(", "yy", ")"}),
                                  NL(2, {"return", "1", ";"}))),
        L(0, {"endfunction"}),
    },

    {
        "function with for loop",
        "function foo;"
        "for (x=0;x<N;++x) begin "
        "return 1;"
        "end "
        "endfunction",
        FunctionHeader(0, L(0, {"function", "foo", ";"})),
        StatementList(
            1, FlowControl(1,
                           LoopHeader(1, L(1, {"for", "("}),
                                      ForSpec(3, L(3, {"x", "=", "0", ";"}),
                                              L(3, {"x", "<", "N", ";"}),
                                              L(3, {"++", "x"})),
                                      L(1, {")", "begin"})),
                           StatementList(2, NL(2, {"return", "1", ";"})),
                           L(1, {"end"}))),
        L(0, {"endfunction"}),
    },

    {
        "function with for loop, single-statement body",
        "function foo;"
        "for (x=0;x<N;++x) y=x;"
        "endfunction",
        FunctionHeader(0, L(0, {"function", "foo", ";"})),
        StatementList(
            1, FlowControl(1,
                           LoopHeader(1, L(1, {"for", "("}),
                                      ForSpec(3, L(3, {"x", "=", "0", ";"}),
                                              L(3, {"x", "<", "N", ";"}),
                                              L(3, {"++", "x"})),
                                      L(1, {")"})),
                           NL(2, {"y", "=", "x", ";"}))),
        L(0, {"endfunction"}),
    },

    {
        "function with for loop, labeled begin and end",
        "function foo;"
        "for (x=0;x<N;++x) begin:yyy "
        "return 1;"
        "end:yyy "
        "endfunction",
        FunctionHeader(0, L(0, {"function", "foo", ";"})),
        StatementList(
            1, FlowControl(1,
                           LoopHeader(1, L(1, {"for", "("}),
                                      ForSpec(3, L(3, {"x", "=", "0", ";"}),
                                              L(3, {"x", "<", "N", ";"}),
                                              L(3, {"++", "x"})),
                                      L(1, {")", "begin", ":", "yyy"})),
                           StatementList(2, NL(2, {"return", "1", ";"})),
                           L(1, {"end", ":", "yyy"}))),
        L(0, {"endfunction"}),
    },

    {
        "function with forever loop, block statement body",
        "function foo;"
        "forever begin "
        "return 1;"
        "end "
        "endfunction",
        FunctionHeader(0, L(0, {"function", "foo", ";"})),
        StatementList(1,
                      FlowControl(1, L(1, {"forever", "begin"}),
                                  StatementList(2, NL(2, {"return", "1", ";"})),
                                  L(1, {"end"}))),
        L(0, {"endfunction"}),
    },

    {
        "function with forever loop, single-statement body",
        "function foo;"
        "forever break; "
        "endfunction",
        FunctionHeader(0, L(0, {"function", "foo", ";"})),
        StatementList(1,
                      FlowControl(1,                  //
                                  L(1, {"forever"}),  //
                                  NL(2, {"break", ";"}))),
        L(0, {"endfunction"}),
    },

    {
        "function with repeat loop, block statement body",
        "function foo;"
        "repeat (2) begin "
        "return 1;"
        "end "
        "endfunction",
        FunctionHeader(0, L(0, {"function", "foo", ";"})),
        StatementList(1,
                      FlowControl(1,  //
                                  L(1, {"repeat", "(", "2", ")", "begin"}),
                                  StatementList(2, NL(2, {"return", "1", ";"})),
                                  L(1, {"end"}))),
        L(0, {"endfunction"}),
    },

    {
        "function with repeat loop, single-statement body",
        "function foo;"
        "repeat (2) continue; "
        "endfunction",
        FunctionHeader(0, L(0, {"function", "foo", ";"})),
        StatementList(1, FlowControl(1,  //
                                     L(1, {"repeat", "(", "2", ")"}),
                                     NL(2, {"continue", ";"}))),
        L(0, {"endfunction"}),
    },

    {
        "function with while loop, block statement body",
        "function foo;"
        "while (x) begin "
        "return 1;"
        "end "
        "endfunction",
        FunctionHeader(0, L(0, {"function", "foo", ";"})),
        StatementList(1,
                      FlowControl(1, L(1, {"while", "(", "x", ")", "begin"}),
                                  StatementList(2, NL(2, {"return", "1", ";"})),
                                  L(1, {"end"}))),
        L(0, {"endfunction"}),
    },

    {
        "function with while loop, single-statement body",
        "function foo;"
        "while (e) coyote(sooper_genius); "
        "endfunction",
        FunctionHeader(0, L(0, {"function", "foo", ";"})),
        StatementList(
            1, FlowControl(1, L(1, {"while", "(", "e", ")"}),
                           NL(2, {"coyote", "(", "sooper_genius", ")", ";"}))),
        L(0, {"endfunction"}),
    },

    {
        "function with nested while loop, single-statement body",
        "function foo;"
        "while (e) while (e) coyote(sooper_genius); "
        "endfunction",
        FunctionHeader(0, L(0, {"function", "foo", ";"})),
        StatementList(
            1, FlowControl(
                   1, L(1, {"while", "(", "e", ")", "while", "(", "e", ")"}),
                   NL(2, {"coyote", "(", "sooper_genius", ")", ";"}))),
        L(0, {"endfunction"}),
    },

    {
        "function with do-while loop, null-statement",
        "function foo;"
        "do;"
        "while (y);"
        "endfunction",
        FunctionHeader(0, L(0, {"function", "foo", ";"})),
        StatementList(
            1, FlowControl(1, L(1, {"do", ";", "while", "(", "y", ")", ";"}))),
        L(0, {"endfunction"}),
    },

    {
        "function with do-while loop",
        "function foo;"
        "do begin "
        "return 1;"
        "end while (y);"
        "endfunction",
        FunctionHeader(0, L(0, {"function", "foo", ";"})),
        StatementList(1,
                      FlowControl(1, L(1, {"do", "begin"}),
                                  StatementList(2, NL(2, {"return", "1", ";"})),
                                  L(1, {"end", "while", "(", "y", ")", ";"}))),
        L(0, {"endfunction"}),
    },

    {
        "function with do-while loop, single-statement",
        "function foo;"
        "do --y;"
        "while (y);"
        "endfunction",
        FunctionHeader(0, L(0, {"function", "foo", ";"})),
        StatementList(1, FlowControl(1, L(1, {"do"}), NL(2, {"--", "y", ";"}),
                                     L(1, {"while", "(", "y", ")", ";"}))),
        L(0, {"endfunction"}),
    },

    {
        "function with foreach loop",
        "function foo;"
        "foreach (x[k]) begin "
        "return 1;"
        "end "
        "endfunction",
        FunctionHeader(0, L(0, {"function", "foo", ";"})),
        StatementList(
            1,
            FlowControl(
                1, L(1, {"foreach", "(", "x", "[", "k", "]", ")", "begin"}),
                StatementList(2, NL(2, {"return", "1", ";"})), L(1, {"end"}))),
        L(0, {"endfunction"}),
    },

    {
        "function with loops",
        "function foo;"
        "for (;;) begin "
        "return 0;"
        "end "
        "for (;;) begin "
        "return 1;"
        "end "
        "endfunction",
        FunctionHeader(0, L(0, {"function", "foo", ";"})),
        StatementList(
            1,
            FlowControl(1,
                        LoopHeader(1, L(1, {"for", "("}),
                                   ForSpec(3, L(3, {";"}), L(3, {";"})),
                                   L(1, {")", "begin"})),  //
                        StatementList(2, NL(2, {"return", "0", ";"})),
                        L(1, {"end"})  //
                        ),
            FlowControl(1,
                        LoopHeader(1, L(1, {"for", "("}),
                                   ForSpec(3, L(3, {";"}), L(3, {";"})),
                                   L(1, {")", "begin"})),  //
                        StatementList(2, NL(2, {"return", "1", ";"})),
                        L(1, {"end"}))),
        L(0, {"endfunction"}),
    },

    {
        "function with case statements",
        "function foo_case;"
        "case (y) "
        "k1: return 0;"
        "k2: return 1;"
        "endcase "
        "case (z) "
        "k3: return 0;"
        "k4: return 1;"
        "endcase "
        "endfunction",
        FunctionHeader(0, L(0, {"function", "foo_case", ";"})),
        StatementList(
            1,
            FlowControl(1, L(1, {"case", "(", "y", ")"}),
                        CaseItemList(2,  //
                                     L(2, {"k1", ":", "return", "0", ";"}),
                                     L(2, {"k2", ":", "return", "1", ";"})),
                        L(1, {"endcase"})),                //
            FlowControl(1, L(1, {"case", "(", "z", ")"}),  //
                        CaseItemList(2,                    //
                                     L(2, {"k3", ":", "return", "0", ";"}),
                                     L(2, {"k4", ":", "return", "1", ";"})),
                        L(1, {"endcase"}))),
        L(0, {"endfunction"}),
    },

    {
        "function with case inside statements",
        "function foo_case_inside;"
        "case (y) inside "
        "k1: return 0;"
        "k2: return 1;"
        "endcase "
        "case (z) inside "
        "k3: return 0;"
        "k4: return 1;"
        "endcase "
        "endfunction",
        FunctionHeader(0, L(0, {"function", "foo_case_inside", ";"})),
        StatementList(
            1,
            FlowControl(1, L(1, {"case", "(", "y", ")", "inside"}),
                        CaseItemList(2,  //
                                     L(2, {"k1", ":", "return", "0", ";"}),
                                     L(2, {"k2", ":", "return", "1", ";"})),
                        L(1, {"endcase"})),                       //
            FlowControl(1,                                        //
                        L(1, {"case", "(", "z", ")", "inside"}),  //
                        CaseItemList(2,                           //
                                     L(2, {"k3", ":", "return", "0", ";"}),
                                     L(2, {"k4", ":", "return", "1", ";"})),
                        L(1, {"endcase"}))),
        L(0, {"endfunction"}),
    },

    {
        "function with case pattern statements",
        "function foo_case_pattern;"
        "case (y) matches "
        ".foo: return 0;"
        ".*: return 1;"
        "endcase "
        "case (z) matches "
        ".foo: return 0;"
        ".*: return 1;"
        "endcase "
        "endfunction",
        FunctionHeader(0, L(0, {"function", "foo_case_pattern", ";"})),
        StatementList(
            1,
            FlowControl(
                1, L(1, {"case", "(", "y", ")", "matches"}),
                CaseItemList(2,  //
                             L(2, {".", "foo", ":", "return", "0", ";"}),
                             L(2, {".*", ":", "return", "1", ";"})),
                L(1, {"endcase"})),  //
            FlowControl(
                1,                                         //
                L(1, {"case", "(", "z", ")", "matches"}),  //
                CaseItemList(2,                            //
                             L(2, {".", "foo", ":", "return", "0", ";"}),
                             L(2, {".*", ":", "return", "1", ";"})),
                L(1, {"endcase"}))),
        L(0, {"endfunction"}),
    },

    {
        "function with array formal parameters and return statement",
        "function automatic logic checkit ("
        "input logic [4:0] a,"
        "input logic [4:0] b);"
        "return a ^ b;"
        "endfunction",
        FunctionHeader(
            0, L(0, {"function", "automatic", "logic", "checkit", "("}),
            TFPortList(
                2, L(2, {"input", "logic", "[", "4", ":", "0", "]", "a", ","}),
                L(2, {"input", "logic", "[", "4", ":", "0", "]", "b"})),
            L(0, {")", ";"})),
        StatementList(1, NL(1, {"return", "a", "^", "b", ";"})),
        L(0, {"endfunction"}),
    },

    {
        "function (class method) constructor with foreach",
        "class foo;"
        "function new(string name);"
        "super.new(name);"
        "foreach (bar[j]) begin "
        "bar[j] = new();"
        "bar[j].x = new();"
        "end "
        "endfunction "
        "endclass",
        ClassHeader(0, L(0, {"class", "foo", ";"})),
        ClassItemList(
            1,
            FunctionHeader(1, L(1, {"function", "new", "("}),
                           TFPortList(3, L(3, {"string", "name"})),
                           L(1, {")", ";"})),
            StatementList(
                2, NL(2, {"super", ".", "new", "(", "name", ")", ";"}),
                FlowControl(
                    2,
                    L(2, {"foreach", "(", "bar", "[", "j", "]", ")", "begin"}),
                    StatementList(
                        3,
                        NL(3, {"bar", "[", "j", "]", "=", "new", "(", ")", ";"}),
                        NL(3, {"bar", "[", "j", "]", ".", "x", "=", "new", "(",
                              ")", ";"})),
                    L(2, {"end"}))),
            L(1, {"endfunction"})),
        L(0, {"endclass"}),
    },
};

// Test that TreeUnwrapper produces correct UnwrappedLines from function tests
TEST_F(TreeUnwrapperTest, UnwrapFunctionTests) {
  for (const auto& test_case : kUnwrapFunctionTestCases) {
    auto tree_unwrapper = CreateTreeUnwrapper(test_case.source_code);
    const auto* uwline_tree = tree_unwrapper->Unwrap();
    EXPECT_TRUE(VerifyUnwrappedLines(&std::cout, *ABSL_DIE_IF_NULL(uwline_tree),
                                     test_case));
  }
}

const TreeUnwrapperTestData kUnwrapStructTestCases[] = {
    {
        "simple struct typedef one member",
        "typedef struct {int a;} foo;",
        L(0, {"typedef", "struct", "{"}),
        StructUnionMemberList(1, L(1, {"int", "a", ";"})),
        L(0, {"}", "foo", ";"}),
    },

    {
        "simple struct typedef multiple members",
        "typedef struct {"
        "int a;"
        "logic [3:0] b;"
        "} foo;",
        L(0, {"typedef", "struct", "{"}),
        StructUnionMemberList(
            1, L(1, {"int", "a", ";"}),
            L(1, {"logic", "[", "3", ":", "0", "]", "b", ";"})),
        L(0, {"}", "foo", ";"}),
    },
};

// Test that TreeUnwrapper produces correct UnwrappedLines from structs
TEST_F(TreeUnwrapperTest, UnwrapStructTests) {
  for (const auto& test_case : kUnwrapStructTestCases) {
    auto tree_unwrapper = CreateTreeUnwrapper(test_case.source_code);
    const auto* uwline_tree = tree_unwrapper->Unwrap();
    EXPECT_TRUE(VerifyUnwrappedLines(&std::cout, *ABSL_DIE_IF_NULL(uwline_tree),
                                     test_case));
  }
}

const TreeUnwrapperTestData kUnwrapUnionTestCases[] = {
    {
        "simple union typedef one member",
        "typedef union {int a;} foo;",
        L(0, {"typedef", "union", "{"}),
        StructUnionMemberList(1, L(1, {"int", "a", ";"})),
        L(0, {"}", "foo", ";"}),
    },

    {
        "simple union typedef multiple members",
        "typedef union {"
        "int a;"
        "logic [3:0] b;"
        "} foo;",
        L(0, {"typedef", "union", "{"}),
        StructUnionMemberList(
            1, L(1, {"int", "a", ";"}),
            L(1, {"logic", "[", "3", ":", "0", "]", "b", ";"})),
        L(0, {"}", "foo", ";"}),
    },
};

// Test that TreeUnwrapper produces correct UnwrappedLines from unions
TEST_F(TreeUnwrapperTest, UnwrapUnionTests) {
  for (const auto& test_case : kUnwrapUnionTestCases) {
    auto tree_unwrapper = CreateTreeUnwrapper(test_case.source_code);
    const auto* uwline_tree = tree_unwrapper->Unwrap();
    EXPECT_TRUE(VerifyUnwrappedLines(&std::cout, *ABSL_DIE_IF_NULL(uwline_tree),
                                     test_case));
  }
}

const TreeUnwrapperTestData kUnwrapEnumTestCases[] = {
    {
        "simple enum typedef, one member",
        "typedef enum { one=1 } foo_e;",
        L(0, {"typedef", "enum", "{"}),
        EnumItemList(1, L(1, {"one", "=", "1"})),
        L(0, {"}", "foo_e", ";"}),
    },

    {
        "simple enum typedef multiple members",
        "typedef enum logic {"
        "one=1,"
        "two=2"
        "} foo_e;",
        L(0, {"typedef", "enum", "logic", "{"}),
        EnumItemList(1, L(1, {"one", "=", "1", ","}), L(1, {"two", "=", "2"})),
        L(0, {"}", "foo_e", ";"}),
    },
};

// Test that TreeUnwrapper produces correct UnwrappedLines from structs
TEST_F(TreeUnwrapperTest, UnwrapEnumTests) {
  for (const auto& test_case : kUnwrapEnumTestCases) {
    auto tree_unwrapper = CreateTreeUnwrapper(test_case.source_code);
    const auto* uwline_tree = tree_unwrapper->Unwrap();
    EXPECT_TRUE(VerifyUnwrappedLines(&std::cout, *ABSL_DIE_IF_NULL(uwline_tree),
                                     test_case));
  }
}

const TreeUnwrapperTestData kUnwrapPropertyTestCases[] = {
    {
        "simple property declaration",
        "property myprop;"
        "a < b "
        "endproperty",
        L(0, {"property", "myprop", ";"}),
        PropertyItemList(1, L(1, {"a", "<", "b"})),
        L(0, {"endproperty"}),
    },

    {
        "simple property declaration, terminal semicolon",
        "property myprop;"
        "a < b;"
        "endproperty",
        L(0, {"property", "myprop", ";"}),
        PropertyItemList(1, L(1, {"a", "<", "b", ";"})),
        L(0, {"endproperty"}),
    },

    {
        "simple property declaration, with assertion variable declaration",
        "property myprop;"
        "pkg::thing_t thing;"
        "a < b "
        "endproperty",
        L(0, {"property", "myprop", ";"}),
        VarDeclList(1, L(1, {"pkg", "::", "thing_t", "thing", ";"})),
        PropertyItemList(1, L(1, {"a", "<", "b"})),
        L(0, {"endproperty"}),
    },

    {
        "two property declarations",
        "property myprop1;"
        "a < b "
        "endproperty "
        "property myprop2;"
        "a > b "
        "endproperty",
        L(0, {"property", "myprop1", ";"}),
        PropertyItemList(1, L(1, {"a", "<", "b"})),
        L(0, {"endproperty"}),
        L(0, {"property", "myprop2", ";"}),
        PropertyItemList(1, L(1, {"a", ">", "b"})),
        L(0, {"endproperty"}),
    },

    {
        "two property declarations, with end-labels",
        "property myprop1;"
        "a < b "
        "endproperty : myprop1 "
        "property myprop2;"
        "a > b "
        "endproperty : myprop2",
        L(0, {"property", "myprop1", ";"}),
        PropertyItemList(1, L(1, {"a", "<", "b"})),
        L(0, {"endproperty", ":", "myprop1"}),
        L(0, {"property", "myprop2", ";"}),
        PropertyItemList(1, L(1, {"a", ">", "b"})),
        L(0, {"endproperty", ":", "myprop2"}),
    },

    {
        "simple property declaration, two ports",
        "property myprop(int foo, int port);"
        "a < b "
        "endproperty",
        L(0, {"property", "myprop", "(", "int", "foo", ",", "int", "port", ")",
              ";"}),
        PropertyItemList(1, L(1, {"a", "<", "b"})),
        L(0, {"endproperty"}),
    },

    {
        "property declaration inside package",
        "package pkg;"
        "property myprop;"
        "a < b "
        "endproperty "
        "endpackage",
        L(0, {"package", "pkg", ";"}),
        PackageItemList(1, L(1, {"property", "myprop", ";"}),
                        PropertyItemList(2, L(2, {"a", "<", "b"})),
                        L(1, {"endproperty"})),
        L(0, {"endpackage"}),
    },

    {
        "property declaration inside module",
        "module pkg;"
        "property myprop;"
        "a < b "
        "endproperty "
        "endmodule",
        ModuleHeader(0, L(0, {"module", "pkg", ";"})),
        ModuleItemList(1, L(1, {"property", "myprop", ";"}),
                       PropertyItemList(2, L(2, {"a", "<", "b"})),
                       L(1, {"endproperty"})),
        L(0, {"endmodule"}),
    },
    /* TODO(b/145241765): fix property-case parsing
    {
        "property declaration with property case statement",
        "module m;"
        "property p;"
        "case (g) h:a < b; i:c<d endcase "
        "endproperty "
        "endmodule",
        ModuleHeader(0, L(0, {"module", "m", ";"})),
        ModuleItemList(1, L(1, {"property", "p", ";"}),
                       PropertyItemList(
                           2, L(2, {"case", "(", "g", ")"}),
                           CaseItemList(3, L(3, {"h", ":", "a", "<", "b", ";"}),
                                        L(3, {"i", ":", "c", "<", "d", ";"})),
                           L(2, {"endcase"})),
                       L(1, {"endproperty"})),
        L(0, {"endmodule"}),
    },
    */
};

// Test that TreeUnwrapper produces correct UnwrappedLines from properties
TEST_F(TreeUnwrapperTest, UnwrapPropertyTests) {
  for (const auto& test_case : kUnwrapPropertyTestCases) {
    auto tree_unwrapper = CreateTreeUnwrapper(test_case.source_code);
    const auto* uwline_tree = tree_unwrapper->Unwrap();
    EXPECT_TRUE(VerifyUnwrappedLines(&std::cout, *ABSL_DIE_IF_NULL(uwline_tree),
                                     test_case));
  }
}

const TreeUnwrapperTestData kUnwrapCovergroupTestCases[] = {
    {
        "empty covergroup declarations",
        "covergroup cg(string s);"
        "endgroup "
        "covergroup cg2(string s);"
        "endgroup ",
        CovergroupHeader(0, L(0, {"covergroup", "cg", "("}),
                         TFPortList(2, L(2, {"string", "s"})),
                         L(0, {")", ";"})),
        L(0, {"endgroup"}),
        CovergroupHeader(0, L(0, {"covergroup", "cg2", "("}),
                         TFPortList(2, L(2, {"string", "s"})),
                         L(0, {")", ";"})),
        L(0, {"endgroup"}),
    },

    {
        "covergroup declaration with options",
        "covergroup cg(string s);"
        "option.name = cg_name;"
        "option.per_instance=1;"
        "endgroup ",
        CovergroupHeader(0, L(0, {"covergroup", "cg", "("}),
                         TFPortList(2, L(2, {"string", "s"})),
                         L(0, {")", ";"})),
        CovergroupItemList(
            1, L(1, {"option", ".", "name", "=", "cg_name", ";"}),
            L(1, {"option", ".", "per_instance", "=", "1", ";"})),
        L(0, {"endgroup"}),
    },

    {
        "covergroup declaration with coverpoints",
        "covergroup cg(string s);"
        "q_cp : coverpoint cp;"
        "q_cp2 : coverpoint cp2;"
        "endgroup ",
        CovergroupHeader(0, L(0, {"covergroup", "cg", "("}),
                         TFPortList(2, L(2, {"string", "s"})),
                         L(0, {")", ";"})),
        CovergroupItemList(1, L(1, {"q_cp", ":", "coverpoint", "cp", ";"}),
                           L(1, {"q_cp2", ":", "coverpoint", "cp2", ";"})),
        L(0, {"endgroup"}),
    },

    {
        "coverpoint with bins",
        "covergroup cg(string s);"
        "q_cp : coverpoint cp {"
        "  bins foo = {bar};"
        "  bins zoo = {pig};"
        "}"
        "endgroup ",
        CovergroupHeader(0, L(0, {"covergroup", "cg", "("}),
                         TFPortList(2, L(2, {"string", "s"})),
                         L(0, {")", ";"})),
        CovergroupItemList(
            1, L(1, {"q_cp", ":", "coverpoint", "cp", "{"}),
            CoverpointItemList(
                2, L(2, {"bins", "foo", "=", "{", "bar", "}", ";"}),
                L(2, {"bins", "zoo", "=", "{", "pig", "}", ";"})),
            L(1, {"}"})),
        L(0, {"endgroup"}),
    },

    {
        "covergroup declaration with crosses",
        "covergroup cg(string s);"
        "x_cross : cross s1, s2;"
        "x_cross2 : cross s2, s1;"
        "endgroup ",
        CovergroupHeader(0, L(0, {"covergroup", "cg", "("}),
                         TFPortList(2, L(2, {"string", "s"})),
                         L(0, {")", ";"})),
        CovergroupItemList(
            1, L(1, {"x_cross", ":", "cross", "s1", ",", "s2", ";"}),
            L(1, {"x_cross2", ":", "cross", "s2", ",", "s1", ";"})),
        L(0, {"endgroup"}),
    },

    {
        "cover crosses with bins",
        "covergroup cg(string s);"
        "x_cross : cross s1, s2{"
        "  bins a = binsof(x) intersect {d};"
        "  bins b = binsof(y) intersect {e};"
        "}"
        "endgroup ",
        CovergroupHeader(0, L(0, {"covergroup", "cg", "("}),
                         TFPortList(2, L(2, {"string", "s"})),
                         L(0, {")", ";"})),
        CovergroupItemList(
            1, L(1, {"x_cross", ":", "cross", "s1", ",", "s2", "{"}),
            CrossItemList(2,
                          L(2, {"bins", "a", "=", "binsof", "(", "x", ")",
                                "intersect", "{", "d", "}", ";"}),
                          L(2, {"bins", "b", "=", "binsof", "(", "y", ")",
                                "intersect", "{", "e", "}", ";"})),
            L(1, {"}"})),
        L(0, {"endgroup"}),
    },
    {
        "covergroup declaration with a function",
        "covergroup cg(string s) with function sample(bit pending);"
        "endgroup ",
        CovergroupHeader(0, L(0, {"covergroup", "cg", "("}),
                         TFPortList(2, L(2, {"string", "s"})),
                         L(0, {")", "with", "function", "sample", "("}),
                         N(2, L(2, {"bit", "pending"})), L(0, {")", ";"})),
        L(0, {"endgroup"}),
    },
};

// Test that TreeUnwrapper produces correct UnwrappedLines from covergroups
TEST_F(TreeUnwrapperTest, UnwrapCovergroupTests) {
  for (const auto& test_case : kUnwrapCovergroupTestCases) {
    auto tree_unwrapper = CreateTreeUnwrapper(test_case.source_code);
    const auto* uwline_tree = tree_unwrapper->Unwrap();
    EXPECT_TRUE(VerifyUnwrappedLines(&std::cout, *ABSL_DIE_IF_NULL(uwline_tree),
                                     test_case));
  }
}

const TreeUnwrapperTestData kUnwrapSequenceTestCases[] = {
    {
        "simple sequence declaration",
        "sequence myseq;"
        "a < b "
        "endsequence",
        L(0, {"sequence", "myseq", ";"}),
        SequenceItemList(1, L(1, {"a", "<", "b"})),
        L(0, {"endsequence"}),
    },

    {
        "simple sequence declaration, terminal semicolon",
        "sequence myseq;"
        "a < b;"
        "endsequence",
        L(0, {"sequence", "myseq", ";"}),
        SequenceItemList(1, L(1, {"a", "<", "b", ";"})),
        L(0, {"endsequence"}),
    },

    {
        "simple sequence declaration, with assertion variable declaration",
        "sequence myseq;"
        "foo bar;"
        "a < b "
        "endsequence",
        L(0, {"sequence", "myseq", ";"}),
        VarDeclList(1, L(1, {"foo", "bar", ";"})),
        SequenceItemList(1, L(1, {"a", "<", "b"})),
        L(0, {"endsequence"}),
    },

    {
        "two sequence declarations",
        "sequence myseq;"
        "a < b "
        "endsequence "
        "sequence myseq2;"
        "a > b "
        "endsequence",
        L(0, {"sequence", "myseq", ";"}),
        SequenceItemList(1, L(1, {"a", "<", "b"})),
        L(0, {"endsequence"}),
        L(0, {"sequence", "myseq2", ";"}),
        SequenceItemList(1, L(1, {"a", ">", "b"})),
        L(0, {"endsequence"}),
    },

    {
        "two sequence declarations, with end labels",
        "sequence myseq;"
        "a < b "
        "endsequence : myseq "
        "sequence myseq2;"
        "a > b "
        "endsequence : myseq2",
        L(0, {"sequence", "myseq", ";"}),
        SequenceItemList(1, L(1, {"a", "<", "b"})),
        L(0, {"endsequence", ":", "myseq"}),
        L(0, {"sequence", "myseq2", ";"}),
        SequenceItemList(1, L(1, {"a", ">", "b"})),
        L(0, {"endsequence", ":", "myseq2"}),
    },
};

// Test that TreeUnwrapper produces correct UnwrappedLines from sequences
TEST_F(TreeUnwrapperTest, UnwrapSequenceTests) {
  for (const auto& test_case : kUnwrapSequenceTestCases) {
    auto tree_unwrapper = CreateTreeUnwrapper(test_case.source_code);
    const auto* uwline_tree = tree_unwrapper->Unwrap();
    EXPECT_TRUE(VerifyUnwrappedLines(&std::cout, *ABSL_DIE_IF_NULL(uwline_tree),
                                     test_case));
  }
}

}  // namespace
}  // namespace formatter
}  // namespace verilog
