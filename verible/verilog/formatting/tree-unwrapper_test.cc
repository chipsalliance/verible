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

#include "verible/verilog/formatting/tree-unwrapper.h"

#include <cstddef>
#include <initializer_list>
#include <iostream>
#include <memory>
#include <sstream>
#include <string>
#include <utility>
#include <vector>

#include "absl/status/status.h"
#include "absl/strings/str_join.h"
#include "absl/strings/string_view.h"
#include "gtest/gtest.h"
#include "verible/common/formatting/format-token.h"
#include "verible/common/formatting/token-partition-tree.h"
#include "verible/common/formatting/unwrapped-line.h"
#include "verible/common/text/text-structure.h"
#include "verible/common/util/container-iterator-range.h"
#include "verible/common/util/logging.h"
#include "verible/common/util/spacer.h"
#include "verible/common/util/tree-operations.h"
#include "verible/common/util/vector-tree.h"
#include "verible/verilog/analysis/verilog-analyzer.h"
#include "verible/verilog/formatting/format-style.h"

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

  explicit ExpectedUnwrappedLine(int s) : indentation_spaces(s) {}

  ExpectedUnwrappedLine(
      int s, std::initializer_list<absl::string_view> expected_tokens)
      : indentation_spaces(s), tokens(expected_tokens) {}

  void ShowUnwrappedLineDifference(std::ostream *stream,
                                   const UnwrappedLine &uwline) const;
  bool EqualsUnwrappedLine(std::ostream *stream,
                           const UnwrappedLine &uwline) const;
};

// Human readable ExpectedUnwrappedLined which outputs indentation and line.
// Mimic operator << (ostream&, const UnwrappedLine&).
std::ostream &operator<<(std::ostream &stream,
                         const ExpectedUnwrappedLine &expected_uwline) {
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
using ExpectedUnwrappedLineTree = verible::VectorTree<ExpectedUnwrappedLine>;

void ValidateExpectedTreeNode(const ExpectedUnwrappedLineTree &etree) {
  // At each tree node, there should either be expected tokens in the node's
  // value, or node's children, but not both.
  CHECK(etree.Value().tokens.empty() != is_leaf(etree))
      << "Node should not contain both tokens and children @"
      << verible::NodePath(etree);
}

// Make sure the expect-tree is well-formed.
void ValidateExpectedTree(const ExpectedUnwrappedLineTree &etree) {
  ApplyPreOrder(etree, ValidateExpectedTreeNode);
}

// Outputs the unwrapped line followed by this expected unwrapped line
void ExpectedUnwrappedLine::ShowUnwrappedLineDifference(
    std::ostream *stream, const UnwrappedLine &uwline) const {
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
    std::ostream *stream, const UnwrappedLine &uwline) const {
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
  const char *test_name;

  // The source code for testing must be syntactically correct.
  absl::string_view source_code;

  // The reference values and structure of UnwrappedLines to expect.
  ExpectedUnwrappedLineTree expected_unwrapped_lines;

  template <typename... Args>
  TreeUnwrapperTestData(const char *name, absl::string_view code,
                        Args &&...nodes)
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
bool VerifyUnwrappedLines(std::ostream *stream,
                          const verible::TokenPartitionTree &uwlines,
                          const TreeUnwrapperTestData &test_case) {
  std::ostringstream first_diff_stream;
  const auto diff = verible::DeepEqual(
      uwlines, test_case.expected_unwrapped_lines,
      [&first_diff_stream](const UnwrappedLine &actual,
                           const ExpectedUnwrappedLine &expect) {
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
    const std::string first_diff = first_diff_stream.str();
    if (!first_diff.empty()) {
      // The Value()s at these nodes are different.
      *stream << "value difference: " << first_diff;
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
  void MakeTree(absl::string_view content) {
    analyzer_ = std::make_unique<VerilogAnalyzer>(content, "TEST_FILE");
    absl::Status status = ABSL_DIE_IF_NULL(analyzer_)->Analyze();
    EXPECT_OK(status) << "Rejected code: " << std::endl << content;

    // Since source code is required to be valid, this error-handling is just
    // to help debug the test case construction
    if (!status.ok()) {
      constexpr bool with_diagnostic_context = false;
      const std::vector<std::string> syntax_error_messages(
          analyzer_->LinterTokenErrorMessages(with_diagnostic_context));
      for (const auto &message : syntax_error_messages) {
        std::cout << message << std::endl;
      }
    }
  }
  // Creates a TreeUnwrapper populated with a concrete syntax tree and
  // token stream view from the file input
  std::unique_ptr<TreeUnwrapper> CreateTreeUnwrapper(
      absl::string_view source_code) {
    MakeTree(source_code);
    const verible::TextStructureView &text_structure_view = analyzer_->Data();
    unwrapper_data_ =
        std::make_unique<UnwrapperData>(text_structure_view.TokenStream());

    return std::make_unique<TreeUnwrapper>(
        text_structure_view, style_, unwrapper_data_->preformatted_tokens);
  }

  // The VerilogAnalyzer to produce a concrete syntax tree of raw Verilog code
  std::unique_ptr<VerilogAnalyzer> analyzer_;

  // Support data that needs to outlive the TreeUnwrappers that use it.
  std::unique_ptr<UnwrapperData> unwrapper_data_;

  // Style configuration.
  FormatStyle style_;
};

// Test that TreeUnwrapper produces the correct UnwrappedLines from an empty
// file
TEST_F(TreeUnwrapperTest, UnwrapEmptyFile) {
  const absl::string_view source_code;

  auto tree_unwrapper = CreateTreeUnwrapper(source_code);
  tree_unwrapper->Unwrap();

  const auto lines = tree_unwrapper->FullyPartitionedUnwrappedLines();
  EXPECT_TRUE(lines.empty())  // Blank line removed.
      << "Unexpected unwrapped line: " << lines.front();
}

// Test that TreeUnwrapper produces the correct UnwrappedLines from a blank
// line.
TEST_F(TreeUnwrapperTest, UnwrapBlankLineOnly) {
  const absl::string_view source_code = "\n";

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
ExpectedUnwrappedLineTree N(int spaces, Args &&...nodes) {
  return ExpectedUnwrappedLineTree(ExpectedUnwrappedLine(spaces),
                                   std::forward<Args>(nodes)...);
}

// L is for leaf, which is the only type of node that should list tokens
ExpectedUnwrappedLineTree L(int spaces,
                            std::initializer_list<absl::string_view> tokens) {
  return ExpectedUnwrappedLineTree(ExpectedUnwrappedLine(spaces, tokens));
}

// Node function aliases for readability.
// Can't use const auto& Alias = N; because N is overloaded.
#define ModuleDeclaration N
#define ModuleHeader N
#define ModulePortList N
#define ModuleParameterList N
#define ModuleItemList N
#define MacroArgList N
#define InterfaceDeclaration N
#define Instantiation N
#define DataDeclaration N
#define InstanceList N
#define PortActualList N
#define StatementList N
#define ClassDeclaration N
#define ClassHeader N
#define ClassItemList N
#define ClassParameterList N
#define FunctionDeclaration N
#define FunctionHeader L
#define TaskDeclaration N
#define TaskHeader L
#define TFPortList N
#define PackageDeclaration N
#define PackageItemList N
#define EnumItemList N
#define StructUnionMemberList N
#define PropertyDeclaration N
#define PropertyItemList N
#define VarDeclList N
#define CovergroupDeclaration N
#define CovergroupHeader N
#define CovergroupItemList N
#define CoverpointItemList N
#define CrossItemList N
#define SequenceDeclaration N
#define SequenceItemList N
#define ConstraintDeclaration N
#define ConstraintItemList N
#define ConstraintExpressionList N
#define DistItemList N
#define LoopHeader N
#define ForSpec N
#define CaseItemList N
#define FlowControl N  // for loops and conditional whole constructs
#define UdpBody N
#define ParBlock N
#define UDPDeclaration N

// Test data for unwrapping Verilog modules
// Test case format: test name, source code, ExpectedUnwrappedLines
const TreeUnwrapperTestData kUnwrapModuleTestCases[] = {
    {
        "empty module",
        "module foo ();"
        "endmodule",
        ModuleDeclaration(0, L(0, {"module", "foo", "(", ")", ";"}),
                          L(0, {"endmodule"})),
    },
    {
        "empty module with one port comment",
        "module foo (\n"
        "//comment\n"
        ");"
        "endmodule",
        ModuleDeclaration(0,
                          ModuleHeader(0,                             //
                                       L(0, {"module", "foo", "("}),  //
                                       L(2, {"//comment"}),           //
                                       L(0, {")", ";"})),
                          L(0, {"endmodule"})),
    },

    {
        "empty module extra spaces",  // verifying space-insensitivity
        "  module\tfoo   (\t) ;    "
        "endmodule   ",
        ModuleDeclaration(0, L(0, {"module", "foo", "(", ")", ";"}),
                          L(0, {"endmodule"})),
    },

    {
        "empty module extra newlines",  // verifying space-insensitivity
        "module foo (\n\n);\n"
        "endmodule\n",
        ModuleDeclaration(0, L(0, {"module", "foo", "(", ")", ";"}),
                          L(0, {"endmodule"})),
    },

    {
        "module with port declarations",
        "module foo ("
        "input bar,"
        "output baz"
        ");"
        "endmodule",
        ModuleDeclaration(
            0,
            ModuleHeader(0, L(0, {"module", "foo", "("}),
                         ModulePortList(2, L(2, {"input", "bar", ","}),
                                        L(2, {"output", "baz"})),
                         L(0, {")", ";"})),
            L(0, {"endmodule"})),
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
        ModuleDeclaration(
            0,
            ModuleHeader(
                0, L(0, {"module", "foo", "("}),
                ModulePortList(2, L(0, {"`ifndef", "FOO"}),
                               L(2, {"input", "bar", ","}), L(0, {"`endif"}),
                               L(2, {"output", "baz"})),
                L(0, {")", ";"})),
            L(0, {"endmodule"})),
    },

    {
        "module with conditional multiple port declarations",
        "module foo ("
        "`ifndef FOO\n"
        "input bar1,"
        "input bar2,"
        "`endif\n"
        "output baz"
        ");"
        "endmodule",
        ModuleDeclaration(
            0,
            ModuleHeader(
                0, L(0, {"module", "foo", "("}),
                ModulePortList(2, L(0, {"`ifndef", "FOO"}),
                               // conditional and unconditional port
                               // declarations are direct token partition tree
                               // siblings.
                               L(2, {"input", "bar1", ","}),  //
                               L(2, {"input", "bar2", ","}),  //
                               L(0, {"`endif"}), L(2, {"output", "baz"})),
                L(0, {")", ";"})),
            L(0, {"endmodule"})),
    },

    {
        "module with conditional multiple port declarations, with else branch",
        "module foo ("
        "`ifndef FOO\n"
        "input bar1,"
        "input bar2,\n"
        "`else\n"
        "input zar1,"
        "input zar2,"
        "`endif\n"
        "output baz"
        ");"
        "endmodule",
        ModuleDeclaration(
            0,
            ModuleHeader(0, L(0, {"module", "foo", "("}),
                         ModulePortList(2, L(0, {"`ifndef", "FOO"}),
                                        // conditional and unconditional port
                                        // declarations are direct token
                                        // partition tree siblings.
                                        L(2, {"input", "bar1", ","}),  //
                                        L(2, {"input", "bar2", ","}),  //
                                        L(0, {"`else"}),               //
                                        L(2, {"input", "zar1", ","}),  //
                                        L(2, {"input", "zar2", ","}),  //
                                        L(0, {"`endif"}),              //
                                        L(2, {"output", "baz"})),
                         L(0, {")", ";"})),
            L(0, {"endmodule"})),
    },

    {
        "module with conditional multiple port declarations, with elsif branch",
        "module foo ("
        "`ifndef FOO\n"
        "input bar1,"
        "input bar2,\n"
        "`elsif BAR\n"
        "input zar1,"
        "input zar2,"
        "`endif\n"
        "output baz"
        ");"
        "endmodule",
        ModuleDeclaration(
            0,
            ModuleHeader(0, L(0, {"module", "foo", "("}),
                         ModulePortList(2, L(0, {"`ifndef", "FOO"}),
                                        // conditional and unconditional port
                                        // declarations are direct token
                                        // partition tree siblings.
                                        L(2, {"input", "bar1", ","}),  //
                                        L(2, {"input", "bar2", ","}),  //
                                        L(0, {"`elsif", "BAR"}),       //
                                        L(2, {"input", "zar1", ","}),  //
                                        L(2, {"input", "zar2", ","}),  //
                                        L(0, {"`endif"}),              //
                                        L(2, {"output", "baz"})),
                         L(0, {")", ";"})),
            L(0, {"endmodule"})),
    },

    {
        "module with nested conditional multiple port declarations",
        "module foo ("
        "`ifndef FOO\n"
        "`ifdef ZOO\n"
        "input bar1,"
        "input bar2,"
        "`endif\n"
        "`endif\n"
        "output baz"
        ");"
        "endmodule",
        ModuleDeclaration(
            0,
            ModuleHeader(0, L(0, {"module", "foo", "("}),
                         ModulePortList(2,                         //
                                        L(0, {"`ifndef", "FOO"}),  //
                                        L(0, {"`ifdef", "ZOO"}),   //
                                        // conditional and unconditional port
                                        // declarations are direct token
                                        // partition tree siblings.
                                        L(2, {"input", "bar1", ","}),  //
                                        L(2, {"input", "bar2", ","}),  //
                                        L(0, {"`endif"}),              //
                                        L(0, {"`endif"}),              //
                                        L(2, {"output", "baz"})),
                         L(0, {")", ";"})),
            L(0, {"endmodule"})),
    },

    {
        "module with `include port declarations",
        "module foo ("
        "`include \"ports.svh\"\n"
        ");"
        "endmodule",
        ModuleDeclaration(
            0,
            ModuleHeader(0,  //
                         L(0, {"module", "foo", "("}),
                         // TODO(b/149503062): un-indent `include
                         L(2, {"`include", "\"ports.svh\""}), L(0, {")", ";"})),
            L(0, {"endmodule"})),
    },

    {
        "module with parameters",
        "module foo #("
        "parameter bar =1,"
        "localparam baz =2"
        ");"
        "endmodule",
        ModuleDeclaration(
            0,
            ModuleHeader(0, L(0, {"module", "foo", "#", "("}),
                         ModuleParameterList(
                             2, L(2, {"parameter", "bar", "=", "1", ","}),
                             L(2, {"localparam", "baz", "=", "2"})),
                         L(0, {")", ";"})),
            L(0, {"endmodule"})),
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
        ModuleDeclaration(
            0,
            ModuleHeader(
                0, L(0, {"module", "foo", "#", "("}),
                ModuleParameterList(2, L(0, {"`ifdef", "FOO"}),
                                    L(2, {"parameter", "bar", "=", "1", ","}),
                                    L(0, {"`endif"}),
                                    L(2, {"localparam", "baz", "=", "2"})),
                L(0, {")", ";"})),
            L(0, {"endmodule"})),
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
        ModuleDeclaration(
            0,
            ModuleHeader(0, L(0, {"module", "foo", "#", "("}),
                         ModuleParameterList(
                             2, L(2, {"parameter", "bar", "=", "1", ","}),
                             L(2, {"localparam", "baz", "=", "2"})),
                         L(0, {")", "("}),
                         ModulePortList(2, L(2, {"input", "yar", ","}),
                                        L(2, {"output", "gar"})),
                         L(0, {")", ";"})),
            L(0, {"endmodule"})),
    },

    {
        "module with parameters and empty ports",
        "module foo #("
        "parameter bar =1,"
        "localparam baz =2"
        ") ();"
        "endmodule",
        ModuleDeclaration(
            0,
            ModuleHeader(0, L(0, {"module", "foo", "#", "("}),
                         ModuleParameterList(
                             2, L(2, {"parameter", "bar", "=", "1", ","}),
                             L(2, {"localparam", "baz", "=", "2"})),
                         L(0, {")", "(", ")", ";"})),
            L(0, {"endmodule"})),
    },

    {
        "module with parameters and EOL comment before first param",
        "module foo #(//comment\n"
        "parameter bar =1,"
        "localparam baz =2"
        ") ();"
        "endmodule",
        ModuleDeclaration(
            0,
            ModuleHeader(0, L(0, {"module", "foo", "#", "(", "//comment"}),
                         ModuleParameterList(
                             2, L(2, {"parameter", "bar", "=", "1", ","}),
                             L(2, {"localparam", "baz", "=", "2"})),
                         L(0, {")", "(", ")", ";"})),
            L(0, {"endmodule"})),
    },

    {
        "module with parameters and EOL comment after first param",
        "module foo #("
        "parameter bar =1,//comment\n"
        "localparam baz =2"
        ") ();"
        "endmodule",
        ModuleDeclaration(
            0,
            ModuleHeader(
                0, L(0, {"module", "foo", "#", "("}),
                ModuleParameterList(
                    2, L(2, {"parameter", "bar", "=", "1", ",", "//comment"}),
                    L(2, {"localparam", "baz", "=", "2"})),
                L(0, {")", "(", ")", ";"})),
            L(0, {"endmodule"})),
    },

    {
        "module with parameters and EOL comment after first param",
        "module foo #("
        "parameter bar =1,"
        "localparam baz =2//comment\n"
        ") ();"
        "endmodule",
        ModuleDeclaration(
            0,
            ModuleHeader(
                0, L(0, {"module", "foo", "#", "("}),
                ModuleParameterList(
                    2, L(2, {"parameter", "bar", "=", "1", ","}),
                    L(2, {"localparam", "baz", "=", "2", "//comment"})),
                L(0, {")", "(", ")", ";"})),
            L(0, {"endmodule"})),
    },

    {
        "module with parameters and initializer list",
        "module foo;"
        "localparam logic [63:0] baz[24] = '{"
        "64'h1,"
        "64'h2,"
        "64'h3"
        "};"
        "endmodule",
        ModuleDeclaration(
            0, L(0, {"module", "foo", ";"}),
            N(1,
              L(1, {"localparam", "logic", "[", "63", ":", "0", "]", "baz", "[",
                    "24", "]", "=", "'{"}),
              L(3, {"64", "'h", "1", ","}), L(3, {"64", "'h", "2", ","}),
              L(3, {"64", "'h", "3"}), L(1, {"}", ";"})),
            L(0, {"endmodule"})),
    },

    {
        "module with parameters, initializer list and comments",
        "module foo;"
        "localparam logic [63:0] baz[24] = '{"
        "64'h0, // comment 0\n"
        "64'h1, // comment 1\n"
        "64'h3 // comment 3\n"
        "};"
        "endmodule",
        ModuleDeclaration(
            0, L(0, {"module", "foo", ";"}),
            N(1,
              L(1, {"localparam", "logic", "[", "63", ":", "0", "]", "baz", "[",
                    "24", "]", "=", "'{"}),
              L(3, {"64", "'h", "0", ",", "// comment 0"}),
              L(3, {"64", "'h", "1", ",", "// comment 1"}),
              L(3, {"64", "'h", "3", "// comment 3"}), L(1, {"}", ";"})),
            L(0, {"endmodule"})),
    },

    {
        "module with header import",
        "module foo import p_pkg::*;\n"
        "(qux);"
        "endmodule",
        ModuleDeclaration(
            0,
            ModuleHeader(0,                                          //
                         L(0, {"module", "foo"}),                    //
                         L(1, {"import", "p_pkg", "::", "*", ";"}),  //
                         L(0, {"("}),                                //
                         L(2, {"qux"}),                              //
                         L(0, {")", ";"})),                          //
            L(0, {"endmodule"})),
    },

    {
        "module with header import, multiple in one declaration",
        "module foo import p_pkg::*, q_pkg::qux;\n"
        "(qux);"
        "endmodule",
        ModuleDeclaration(0,
                          ModuleHeader(0,                        //
                                       L(0, {"module", "foo"}),  //
                                       L(1, {"import", "p_pkg", "::", "*", ",",
                                             "q_pkg", "::", "qux", ";"}),  //
                                       L(0, {"("}),                        //
                                       L(2, {"qux"}),                      //
                                       L(0, {")", ";"})),                  //
                          L(0, {"endmodule"})),
    },

    {
        "module with header import, multiple in separate declarations",
        "module foo import p_pkg::*; import q_pkg::qux;\n"
        "(qux);"
        "endmodule",
        ModuleDeclaration(
            0,
            ModuleHeader(0,                                               //
                         L(0, {"module", "foo"}),                         //
                         N(1,                                             //
                           L(1, {"import", "p_pkg", "::", "*", ";"}),     //
                           L(1, {"import", "q_pkg", "::", "qux", ";"})),  //
                         L(0, {"("}),                                     //
                         L(2, {"qux"}),                                   //
                         L(0, {")", ";"})),                               //
            L(0, {"endmodule"})),
    },

    {
        "module with header import before parameters",
        "module foo import p_pkg::*;\n"
        "#(int w = 2)"
        "(qux);"
        "endmodule",
        ModuleDeclaration(
            0,
            ModuleHeader(0,                                          //
                         L(0, {"module", "foo"}),                    //
                         L(1, {"import", "p_pkg", "::", "*", ";"}),  //
                         L(0, {"#", "("}),                           //
                         L(2, {"int", "w", "=", "2"}),               //
                         L(0, {")", "("}),                           //
                         L(2, {"qux"}),                              //
                         L(0, {")", ";"})),                          //
            L(0, {"endmodule"})),
    },

    {
        "two modules with end-labels",
        "module foo ();"
        "endmodule : foo "
        "module zoo;"
        "endmodule : zoo",
        ModuleDeclaration(0, L(0, {"module", "foo", "(", ")", ";"}),
                          L(0, {"endmodule", ":", "foo"})),
        ModuleDeclaration(0, L(0, {"module", "zoo", ";"}),
                          L(0, {"endmodule", ":", "zoo"})),
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
        ModuleDeclaration(
            0,
            ModuleHeader(0, L(0, {"module", "addf", "("}),
                         N(2,                  //
                           L(2, {"a", ","}),   //
                           L(2, {"b", ","}),   //
                           L(2, {"ci", ","}),  //
                           L(2, {"s", ","}),   //
                           L(2, {"co"})),
                         L(0, {")", ";"})),
            ModuleItemList(
                1, L(1, {"input", "a", ",", "b", ",", "ci", ";"}),
                L(1, {"output", "s", ",", "co", ";"}),
                N(1,
                  L(1, {"always", "@", "(", "a", ",", "b", ",", "ci", ")",
                        "begin"}),
                  StatementList(
                      2,
                      L(2, {"s", "=", "(", "a", "^", "b", "^", "ci", ")", ";"}),
                      L(2, {"co", "=", "(", "a", "&", "b", ")", ";"})),
                  L(1, {"end"}))),
            L(0, {"endmodule"})),
    },

    {
        "module with always construct, single statement",
        "module m;\n"
        "always @(b, c) "
        "  s = y;"
        "endmodule",
        ModuleDeclaration(0,  //
                          L(0, {"module", "m", ";"}),
                          N(1, L(1, {"always", "@", "(", "b", ",", "c", ")"}),
                            L(2, {"s", "=", "y", ";"})),
                          L(0, {"endmodule"})),
    },

    {
        "module with always construct, single statement, conditional",
        "module m;\n"
        "always @(b, c)"
        "  if (expr) s = y;"
        "endmodule",
        ModuleDeclaration(
            0,  //
            L(0, {"module", "m", ";"}),
            N(1,  //
              L(1, {"always", "@", "(", "b", ",", "c", ")"}),
              L(2, {"if", "(", "expr", ")"}), L(3, {"s", "=", "y", ";"})),
            L(0, {"endmodule"})),
    },

    {
        "module with always construct, single statement, conditional with else",
        "module m;\n"
        "always @(b, c)"
        "  if (expr) s = y; else t = v;"
        "endmodule",
        ModuleDeclaration(
            0,  //
            L(0, {"module", "m", ";"}),
            N(1,  //
              L(1, {"always", "@", "(", "b", ",", "c", ")"}),
              N(2,  //
                L(2, {"if", "(", "expr", ")"}), L(3, {"s", "=", "y", ";"})),
              N(2,  //
                L(2, {"else"}), L(3, {"t", "=", "v", ";"}))),
            L(0, {"endmodule"})),
    },

    {
        "module with always construct, single statement, if-else-if",
        "module m;\n"
        "always @(b, c)"
        "  if (expr) s = y; else if (j) t = v;"
        "endmodule",
        ModuleDeclaration(
            0,  //
            L(0, {"module", "m", ";"}),
            N(1,  //
              L(1, {"always", "@", "(", "b", ",", "c", ")"}),
              N(2,  //
                L(2, {"if", "(", "expr", ")"}), L(3, {"s", "=", "y", ";"})),
              N(2,  //
                L(2, {"else", "if", "(", "j", ")"}),
                L(3, {"t", "=", "v", ";"}))),
            L(0, {"endmodule"})),
    },

    {
        "module with always construct, single statement, if-else-if-else",
        "module m;\n"
        "always @(b, c)"
        "  if (expr) s = y; else if (j) t = v; else r=0;"
        "endmodule",
        ModuleDeclaration(
            0,  //
            L(0, {"module", "m", ";"}),
            N(1,  //
              L(1, {"always", "@", "(", "b", ",", "c", ")"}),
              N(2,  //
                L(2, {"if", "(", "expr", ")"}), L(3, {"s", "=", "y", ";"})),
              N(2,  //
                L(2, {"else", "if", "(", "j", ")"}),
                L(3, {"t", "=", "v", ";"})),
              N(2,  //
                L(2, {"else"}), L(3, {"r", "=", "0", ";"}))),
            L(0, {"endmodule"})),
    },

    {
        "module with always construct, single statement, loop",
        "module m;\n"
        "always @(b, c)"
        "  for (;;) s = y;"
        "endmodule",
        ModuleDeclaration(0,  //
                          L(0, {"module", "m", ";"}),
                          N(1,  //
                            L(1, {"always", "@", "(", "b", ",", "c", ")"}),
                            N(2,  //
                              L(2, {"for", "("}),
                              N(4, L(4, {";"}), L(4, {";"})), L(2, {")"})),
                            L(3, {"s", "=", "y", ";"})),
                          L(0, {"endmodule"})),
    },

    {
        "module with always construct, single statement, foreach",
        "module m;\n"
        "always @(b, c)"
        "  foreach (a[i]) s = y;"
        "endmodule",
        ModuleDeclaration(0,  //
                          L(0, {"module", "m", ";"}),
                          N(1,  //
                            L(1, {"always", "@", "(", "b", ",", "c", ")"}),
                            L(2, {"foreach", "(", "a", "[", "i", "]", ")"}),
                            L(3, {"s", "=", "y", ";"})),
                          L(0, {"endmodule"})),
    },

    {
        "module with always construct, single statement, repeat",
        "module m;\n"
        "always @(b, c)"
        "  repeat (expr) s = y;"
        "endmodule",
        ModuleDeclaration(
            0,  //
            L(0, {"module", "m", ";"}),
            N(1,  //
              L(1, {"always", "@", "(", "b", ",", "c", ")"}),
              L(2, {"repeat", "(", "expr", ")"}), L(3, {"s", "=", "y", ";"})),
            L(0, {"endmodule"})),
    },

    {
        "module with always construct, single statement, while",
        "module m;\n"
        "always @(b, c)"
        "  while (expr) s = y;"
        "endmodule",
        ModuleDeclaration(
            0,  //
            L(0, {"module", "m", ";"}),
            N(1,  //
              L(1, {"always", "@", "(", "b", ",", "c", ")"}),
              L(2, {"while", "(", "expr", ")"}), L(3, {"s", "=", "y", ";"})),
            L(0, {"endmodule"})),
    },

    {
        "module with always construct, single statement, do-while",
        "module m;\n"
        "always @(b, c)"
        "  do s = y;  while (expr) ;"
        "endmodule",
        ModuleDeclaration(
            0,  //
            L(0, {"module", "m", ";"}),
            N(1,                                                             //
              L(1, {"always", "@", "(", "b", ",", "c", ")"}), L(2, {"do"}),  //
              L(3, {"s", "=", "y", ";"}),                                    //
              L(2, {"while", "(", "expr", ")", ";"})),
            L(0, {"endmodule"})),
    },

    {
        "module with always construct, single statement, forever",
        "module m;\n"
        "always @*"
        "  forever love(u);"
        "endmodule",
        ModuleDeclaration(0,  //
                          L(0, {"module", "m", ";"}),
                          N(1,                           //
                            L(1, {"always", "@", "*"}),  //
                            L(2, {"forever"}),           //
                            N(3,                         //
                              L(3, {"love", "("}), L(5, {"u", ")", ";"}))),
                          L(0, {"endmodule"})),
    },

    {
        "module with always construct, single statement, case",
        "module m;\n"
        "always @(b, c)"
        "  case (e) x: s = y;"
        "  endcase "
        "endmodule",
        ModuleDeclaration(0,  //
                          L(0, {"module", "m", ";"}),
                          N(1,  //
                            L(1, {"always", "@", "(", "b", ",", "c", ")"}),
                            L(2, {"case", "(", "e", ")"}),
                            N(3,                 //
                              L(3, {"x", ":"}),  //
                              L(3, {"s", "=", "y", ";"})),
                            L(2, {"endcase"})),

                          L(0, {"endmodule"})),
    },

    {
        "module with kModuleItemList and kDataDeclarations",
        "module tryme;"
        "foo1 a;"
        "foo2 b();"
        "endmodule",
        ModuleDeclaration(0, L(0, {"module", "tryme", ";"}),
                          ModuleItemList(1,
                                         // fused single instance
                                         L(1, {"foo1", "a", ";"}),
                                         // fused single instance
                                         L(1, {"foo2", "b", "(", ")", ";"})),
                          L(0, {"endmodule"})),
    },

    {
        "module with multi-instance () in single declaration",
        "module multi_inst;"
        "foo aa(), bb();"
        "endmodule",
        ModuleDeclaration(
            0, L(0, {"module", "multi_inst", ";"}),
            Instantiation(1, L(1, {"foo"}),  // instantiation type
                          InstanceList(3, L(3, {"aa", "(", ")", ","}),
                                       L(3, {"bb", "(", ")", ";"}))),
            L(0, {"endmodule"})),
    },

    {
        "module with multi-variable in single declaration",
        "module multi_inst;"
        "foo aa, bb;"
        "endmodule",
        ModuleDeclaration(0, L(0, {"module", "multi_inst", ";"}),
                          Instantiation(1, L(1, {"foo"}),  // instantiation type
                                        InstanceList(3, L(3, {"aa", ","}),
                                                     L(3, {"bb", ";"}))),
                          L(0, {"endmodule"})),
    },

    {
        "module with multi-variable with assignments in single declaration",
        "module multi_inst;"
        "foo aa = 1, bb = 2;"
        "endmodule",
        ModuleDeclaration(
            0, L(0, {"module", "multi_inst", ";"}),
            Instantiation(1, L(1, {"foo"}),  // instantiation type
                          InstanceList(3, L(3, {"aa", "=", "1", ","}),
                                       L(3, {"bb", "=", "2", ";"}))),
            L(0, {"endmodule"})),
    },

    {
        "module with instantiations with parameterized types (positional)",
        "module tryme;"
        "foo #(1) a;"
        "bar #(2, 3) b();"
        "endmodule",
        ModuleDeclaration(
            0, L(0, {"module", "tryme", ";"}),
            ModuleItemList(
                1,
                // These are both single instances, fused with type partition.
                L(1, {"foo", "#", "(", "1", ")",  //
                      "a", ";"}),
                L(1, {"bar", "#", "(", "2", ",", "3", ")",  //
                      "b", "(", ")", ";"})),
            L(0, {"endmodule"})),
    },

    {
        "module with instantiations with single-parameterized types (named)",
        "module tryme;"
        "foo #(.N(1)) a;"
        "bar #(.M(2)) b();"
        "endmodule",
        ModuleDeclaration(
            0, L(0, {"module", "tryme", ";"}),
            ModuleItemList(1,
                           // single instances fused with instantiation type
                           Instantiation(1,                                //
                                         L(1, {"foo", "#", "("}),          //
                                         L(3, {".", "N", "(", "1", ")"}),  //
                                         L(1, {")", "a", ";"})),
                           Instantiation(1,                                //
                                         L(1, {"bar", "#", "("}),          //
                                         L(3, {".", "M", "(", "2", ")"}),  //
                                         L(1, {")", "b", "(", ")", ";"}))  //
                           ),
            L(0, {"endmodule"})),
    },

    {
        "module with instantiations with multi-parameterized types (named)",
        "module tryme;"
        "foo #(.N(1), .M(4)) a;"
        "endmodule",
        ModuleDeclaration(
            0, L(0, {"module", "tryme", ";"}),
            // single instances fused with instantiation type
            Instantiation(1, L(1, {"foo", "#", "("}),
                          N(3,  //
                            L(3, {".", "N", "(", "1", ")", ","}),
                            // note how comma is attached to above partition
                            L(3, {".", "M", "(", "4", ")"})),
                          L(1, {")", "a", ";"})),
            L(0, {"endmodule"})),
    },

    {
        "module with parameterized instantiations with comment before first "
        "param",
        "module tryme;"
        "foo #(//comment\n.N(5),.M(6)) a;"
        "endmodule",
        ModuleDeclaration(0, L(0, {"module", "tryme", ";"}),
                          Instantiation(1, L(1, {"foo", "#", "(", "//comment"}),
                                        N(3,  //
                                          L(3, {".", "N", "(", "5", ")", ","}),
                                          L(3, {".", "M", "(", "6", ")"})),
                                        L(1, {")", "a", ";"})),
                          L(0, {"endmodule"})),
    },

    {
        "module with parameterized instantiations with parameter EOL comment",
        "module tryme;"
        "foo #(.N(5), //comment\n.M(6)) a;"
        "endmodule",
        ModuleDeclaration(
            0, L(0, {"module", "tryme", ";"}),
            Instantiation(1, L(1, {"foo", "#", "("}),
                          N(3,  //
                            L(3, {".", "N", "(", "5", ")", ",", "//comment"}),
                            L(3, {".", "M", "(", "6", ")"})),
                          L(1, {")", "a", ";"})),
            L(0, {"endmodule"})),
    },

    {
        "module with parameterized instantiations with EOL comment (last "
        "param)",
        "module tryme;"
        "foo #(.N(5),.M(6)//comment\n) a;"
        "endmodule",
        ModuleDeclaration(
            0, L(0, {"module", "tryme", ";"}),
            Instantiation(1, L(1, {"foo", "#", "("}),
                          N(3,  //
                            L(3, {".", "N", "(", "5", ")", ","}),
                            L(3, {".", "M", "(", "6", ")", "//comment"})),
                          L(1, {")", "a", ";"})),
            L(0, {"endmodule"})),
    },

    {
        "module instance with named parameter interleaved among EOL comments",
        "module tryme;"
        "foo #(//c1\n//c2\n.N(5), //c3\n//c4\n.M(6)//c5\n//c6\n) a;"
        "endmodule",
        ModuleDeclaration(
            0, L(0, {"module", "tryme", ";"}),
            Instantiation(1, L(1, {"foo", "#", "(", "//c1"}),
                          N(3,                                             //
                            L(3, {"//c2"}),                                //
                            L(3, {".", "N", "(", "5", ")", ",", "//c3"}),  //
                            L(3, {"//c4"}),                                //
                            L(3, {".", "M", "(", "6", ")", "//c5"}),       //
                            L(3, {"//c6"})),                               //
                          L(1, {")", "a", ";"})),
            L(0, {"endmodule"})),
    },

    {
        "module with single instance and positional port actuals",
        "module got_ports;"
        "foo c(y, z);"
        "endmodule",
        ModuleDeclaration(
            0, L(0, {"module", "got_ports", ";"}),
            Instantiation(1, L(1, {"foo", "c", "("}),
                          PortActualList(3, L(3, {"y", ","}), L(3, {"z"})),
                          L(1, {")", ";"})),
            L(0, {"endmodule"})),
    },

    {
        "module with instances and various port actuals",
        "module got_ports;"
        "foo c(x, y, z);"
        "foo d(.x(x), .y(y), .w(z));"
        "foo e(x, a, .y(y), .w(z));"
        "endmodule",
        ModuleDeclaration(
            0, L(0, {"module", "got_ports", ";"}),
            ModuleItemList(
                1,
                Instantiation(1, L(1, {"foo", "c", "("}),
                              PortActualList(3, L(3, {"x", ","}),
                                             L(3, {"y", ","}), L(3, {"z"})),
                              L(1, {")", ";"})  // TODO(fangism): attach to 'z'?
                              ),
                Instantiation(
                    1, L(1, {"foo", "d", "("}),
                    PortActualList(3, L(3, {".", "x", "(", "x", ")", ","}),
                                   L(3, {".", "y", "(", "y", ")", ","}),
                                   L(3, {".", "w", "(", "z", ")"})),
                    L(1, {")", ";"})),
                Instantiation(
                    1, L(1, {"foo", "e", "("}),
                    PortActualList(3, L(3, {"x", ","}), L(3, {"a", ","}),
                                   L(3, {".", "y", "(", "y", ")", ","}),
                                   L(3, {".", "w", "(", "z", ")"})),
                    L(1, {")", ";"}))),
            L(0, {"endmodule"})),
    },

    {
        "module with instances with ifdef in ports",
        "module ifdef_ports;"
        "foo bar(\n"
        "`ifdef BAZ\n"
        "`endif\n"
        ");"
        "endmodule",
        ModuleDeclaration(0, L(0, {"module", "ifdef_ports", ";"}),
                          Instantiation(1, L(1, {"foo", "bar", "("}),
                                        PortActualList(3,  //
                                                       L(0, {"`ifdef", "BAZ"}),
                                                       L(0, {"`endif"})),
                                        L(1, {")", ";"})),
                          L(0, {"endmodule"})),
    },

    {
        "module with instances with ifdef-else in ports",
        "module ifdef_else_ports;"
        "foo bar(\n"
        "`ifdef BAZ\n"
        "`else\n"
        "`endif\n"
        ");"
        "endmodule",
        ModuleDeclaration(
            0, L(0, {"module", "ifdef_else_ports", ";"}),
            Instantiation(1, L(1, {"foo", "bar", "("}),
                          PortActualList(3,  //
                                         L(0, {"`ifdef", "BAZ"}),
                                         L(0, {"`else"}), L(0, {"`endif"})),
                          L(1, {")", ";"})),
            L(0, {"endmodule"})),
    },

    {
        "module with instances with ifndef in ports",
        "module ifndef_ports;"
        "foo bar(\n"
        "`ifndef BAZ\n"
        "`endif\n"
        ");"
        "endmodule",
        ModuleDeclaration(0, L(0, {"module", "ifndef_ports", ";"}),
                          Instantiation(1, L(1, {"foo", "bar", "("}),
                                        PortActualList(3,  //
                                                       L(0, {"`ifndef", "BAZ"}),
                                                       L(0, {"`endif"})),
                                        L(1, {")", ";"})),
                          L(0, {"endmodule"})),
    },

    {
        "module with instances with actuals and ifdef in ports",
        "module ifdef_ports;"
        "foo bar(\n"
        ".a(a),\n"  // with comma
        "`ifdef BAZ\n"
        "`endif\n"
        ");"
        "endmodule",
        ModuleDeclaration(
            0, L(0, {"module", "ifdef_ports", ";"}),
            Instantiation(
                1, L(1, {"foo", "bar", "("}),
                PortActualList(3,  //
                               L(3, {".", "a", "(", "a", ")", ","}),
                               L(0, {"`ifdef", "BAZ"}), L(0, {"`endif"})),
                L(1, {")", ";"})),
            L(0, {"endmodule"})),
    },

    {
        "module with instances with actuals and ifdef in ports (no comma)",
        "module ifdef_ports;"
        "foo bar(\n"
        ".a(a)\n"  // no comma
        "`ifdef BAZ\n"
        "`endif\n"
        ");"
        "endmodule",
        ModuleDeclaration(
            0, L(0, {"module", "ifdef_ports", ";"}),
            Instantiation(
                1, L(1, {"foo", "bar", "("}),
                PortActualList(3,  //
                               L(3, {".", "a", "(", "a", ")"}),
                               L(0, {"`ifdef", "BAZ"}), L(0, {"`endif"})),
                L(1, {")", ";"})),
            L(0, {"endmodule"})),
    },

    {
        "module with instances with ifdef and actuals in ports",
        "module ifdef_ports;"
        "foo bar(\n"
        "`ifdef BAZ\n"
        "`endif\n"
        ".a(a)\n"
        ");"
        "endmodule",
        ModuleDeclaration(
            0, L(0, {"module", "ifdef_ports", ";"}),
            Instantiation(
                1, L(1, {"foo", "bar", "("}),
                PortActualList(3,  //
                               L(0, {"`ifdef", "BAZ"}), L(0, {"`endif"}),
                               L(3, {".", "a", "(", "a", ")"})),
                L(1, {")", ";"})),
            L(0, {"endmodule"})),
    },

    {
        "module with instances with ifdef conditional port",
        "module ifdef_ports;"
        "foo bar(\n"
        "`ifdef BAZ\n"
        ".a(a)\n"
        "`endif\n"
        ");"
        "endmodule",
        ModuleDeclaration(
            0, L(0, {"module", "ifdef_ports", ";"}),
            Instantiation(1, L(1, {"foo", "bar", "("}),
                          PortActualList(3,  //
                                         L(0, {"`ifdef", "BAZ"}),
                                         L(3, {".", "a", "(", "a", ")"}),
                                         L(0, {"`endif"})),
                          L(1, {")", ";"})),
            L(0, {"endmodule"})),
    },

    {
        "module with instances with commented named ports",
        "module named_ports;"
        "foo bar(\n"
        ".a(a),\n"
        "//.aa(aa),\n"
        ".aaa(aaa)\n"
        ");"
        "endmodule",
        ModuleDeclaration(
            0, L(0, {"module", "named_ports", ";"}),
            Instantiation(1, L(1, {"foo", "bar", "("}),
                          PortActualList(3,  //
                                         L(3, {".", "a", "(", "a", ")", ","}),
                                         L(3, {"//.aa(aa),"}),
                                         L(3, {".", "aaa", "(", "aaa", ")"})),
                          L(1, {")", ";"})),
            L(0, {"endmodule"})),
    },

    {
        "module interface ports",
        "module foo ("
        "interface bar_if, interface baz_if);"
        "endmodule",
        ModuleDeclaration(
            0,
            ModuleHeader(0, L(0, {"module", "foo", "("}),
                         ModulePortList(2, L(2, {"interface", "bar_if", ","}),
                                        L(2, {"interface", "baz_if"})),
                         L(0, {")", ";"})),
            L(0, {"endmodule"})),
    },

    {
        "module cast with constant functions",
        "module cast_with_constant_functions;"
        "foo dut("
        ".bus_in({brn::Num_blocks{$bits(dbg::bus_t)'(0)}}),"
        ".bus_mid({brn::Num_bits{$clog2(dbg::bus_t)'(1)}}),"
        ".bus_out(out));"
        "endmodule",
        ModuleDeclaration(
            0, L(0, {"module", "cast_with_constant_functions", ";"}),
            Instantiation(
                1,  //
                L(1, {"foo", "dut", "("}),
                PortActualList(
                    3,  //
                    N(3,
                      L(3, {".", "bus_in", "(", "{", "brn", "::", "Num_blocks",
                            "{", "$bits", "("}),
                      L(5, {"dbg", "::", "bus_t"}),
                      L(3, {")", "'", "(", "0", ")", "}", "}", ")", ","})),
                    N(3,
                      L(3, {".", "bus_mid", "(", "{", "brn", "::", "Num_bits",
                            "{", "$clog2", "("}),
                      L(5, {"dbg", "::", "bus_t"}),
                      L(3, {")", "'", "(", "1", ")", "}", "}", ")", ","})),
                    L(3, {".", "bus_out", "(", "out", ")"})),
                L(1, {")", ";"})),
            L(0, {"endmodule"})),
    },

    {
        "module direct assignment",
        "module addf (\n"
        "input a, input b, input ci,\n"
        "output s, output co);\n"
        "assign s = (a^b^ci);\n"
        "assign co = (a&b)|(a&ci)|(b&ci);\n"
        "endmodule",
        ModuleDeclaration(
            0,
            ModuleHeader(0, L(0, {"module", "addf", "("}),
                         ModulePortList(2, L(2, {"input", "a", ","}),
                                        L(2, {"input", "b", ","}),
                                        L(2, {"input", "ci", ","}),
                                        L(2, {"output", "s", ","}),
                                        L(2, {"output", "co"})),
                         L(0, {")", ";"})),
            ModuleItemList(1,
                           L(1, {"assign", "s", "=", "(", "a", "^", "b", "^",
                                 "ci", ")", ";"}),
                           L(1, {"assign", "co", "=", "(", "a",  "&",  "b",
                                 ")",      "|",  "(", "a", "&",  "ci", ")",
                                 "|",      "(",  "b", "&", "ci", ")",  ";"})),
            L(0, {"endmodule"})),
    },

    {
        "module multiple assignments",
        "module foob;\n"
        "assign s = a, y[0] = b[1], z.z = c -jkl;\n"  // multiple assignments
        "endmodule",
        ModuleDeclaration(
            0, L(0, {"module", "foob", ";"}),
            // TODO(fangism): subpartition multiple assignments.
            N(1, L(1, {"assign", "s", "=", "a", ","}),
              L(3, {"y", "[", "0", "]", "=", "b", "[", "1", "]", ","}),
              L(3, {"z", ".", "z", "=", "c", "-", "jkl", ";"})),
            L(0, {"endmodule"})),
    },

    {
        "module multiple assignments as module item",
        "module foob;\n"
        "assign `BIT_ASSIGN_MACRO(l1, r1)\n"  // as module item
        "endmodule",
        ModuleDeclaration(
            0, L(0, {"module", "foob", ";"}),
            N(1, L(1, {"assign", "`BIT_ASSIGN_MACRO", "("}),
              MacroArgList(5, L(5, {"l1", ","}), L(5, {"r1", ")"}))),
            L(0, {"endmodule"})),
    },

    {
        "module multiple assignments as module item with semicolon",
        "module foob;\n"
        "assign `BIT_ASSIGN_MACRO(l1, r1);\n"  // as module item
        "endmodule",
        ModuleDeclaration(
            0, L(0, {"module", "foob", ";"}),
            N(1, L(1, {"assign", "`BIT_ASSIGN_MACRO", "("}),
              MacroArgList(5, L(5, {"l1", ","}), L(5, {"r1", ")", ";"}))),
            L(0, {"endmodule"})),
    },

    {
        "module multiple assignments as module item II",
        "module foob;\n"
        "initial begin\n"
        "assign `BIT_ASSIGN_MACRO(l1, r1)\n"  // as statement item
        "end\n"
        "endmodule",
        ModuleDeclaration(
            0, L(0, {"module", "foob", ";"}),
            ModuleItemList(
                1, L(1, {"initial", "begin"}),
                StatementList(
                    2, L(2, {"assign", "`BIT_ASSIGN_MACRO", "("}),
                    MacroArgList(6, L(6, {"l1", ","}), L(6, {"r1", ")"}))),
                L(1, {"end"})),
            L(0, {"endmodule"})),
    },

    {
        "module multiple assignments with macro-call rvalue",
        "module foob;\n"
        "initial begin\n"
        "assign z1 = `RVALUE(l1, r1);\n"  // as statement item
        "end\n"
        "endmodule",
        ModuleDeclaration(
            0, L(0, {"module", "foob", ";"}),
            ModuleItemList(
                1, L(1, {"initial", "begin"}),
                N(2, L(2, {"assign", "z1", "=", "`RVALUE", "("}),
                  MacroArgList(6, L(6, {"l1", ","}), L(6, {"r1", ")", ";"}))),
                L(1, {"end"})),
            L(0, {"endmodule"})),
    },

    {
        "module with labeled statements",
        "module labeled_statements;\n"
        "initial begin\n"
        "  a = 0;\n"
        "  foo: b = 0;\n"  // with label
        "end\n"
        "endmodule",
        ModuleDeclaration(
            0, L(0, {"module", "labeled_statements", ";"}),
            ModuleItemList(1, L(1, {"initial", "begin"}),
                           StatementList(2, L(2, {"a", "=", "0", ";"}),
                                         N(2, L(2, {"foo", ":"}),
                                           L(2, {"b", "=", "0", ";"}))),
                           L(1, {"end"})),
            L(0, {"endmodule"})),
    },

    {
        "module with labeled statement block",
        "module labeled_block;\n"
        "initial begin\n"
        "  a = 0;\n"
        "  foo: begin\n"  // labeled begin-end block
        "    b = 9;\n"
        "  end\n"
        "end\n"
        "endmodule",
        ModuleDeclaration(
            0, L(0, {"module", "labeled_block", ";"}),
            ModuleItemList(
                1,  //
                L(1, {"initial", "begin"}),
                StatementList(2,  //
                              L(2, {"a", "=", "0", ";"}),
                              N(2,  //
                                L(2, {"foo", ":", "begin"}),
                                L(3, {"b", "=", "9", ";"}), L(2, {"end"}))),
                L(1, {"end"})),
            L(0, {"endmodule"})),
    },

    {
        "module with static variable",
        "module static_variable;\n"
        "initial begin\n"
        "  static int a = 0;\n"
        "end\n"
        "endmodule",
        ModuleDeclaration(0, L(0, {"module", "static_variable", ";"}),
                          ModuleItemList(1, L(1, {"initial", "begin"}),
                                         N(2, L(2, {"static"}), L(2, {"int"}),
                                           L(2, {"a", "=", "0", ";"})),
                                         L(1, {"end"})),
                          L(0, {"endmodule"})),
    },

    {
        "module with static and automatic variables",
        "module static_automatic;\n"
        "initial begin\n"
        "  static int a = 0;\n"
        "  automatic byte b = 1;\n"
        "end\n"
        "endmodule",
        ModuleDeclaration(
            0, L(0, {"module", "static_automatic", ";"}),
            ModuleItemList(
                1, L(1, {"initial", "begin"}),
                StatementList(2,
                              N(2, L(2, {"static"}), L(2, {"int"}),
                                L(2, {"a", "=", "0", ";"})),
                              N(2, L(2, {"automatic"}), L(2, {"byte"}),
                                L(2, {"b", "=", "1", ";"}))),
                L(1, {"end"})),
            L(0, {"endmodule"})),
    },
    {
        "module with multiple static variables in one declaration",
        "module multi_static;\n"
        "initial begin\n"
        "  static int a, b, c = 0;\n"
        "end\n"
        "endmodule",
        ModuleDeclaration(0, L(0, {"module", "multi_static", ";"}),
                          ModuleItemList(1, L(1, {"initial", "begin"}),
                                         N(2,                   //
                                           L(2, {"static"}),    //
                                           L(2, {"int"}),       //
                                           N(4,                 //
                                             L(4, {"a", ","}),  //
                                             L(4, {"b", ","}),  //
                                             L(4, {"c", "=", "0", ";"}))),
                                         L(1, {"end"})),
                          L(0, {"endmodule"})),
    },
    {
        "module with multiple initialized static variables",
        "module multi_static;\n"
        "initial begin\n"
        "  static int a = 1, b = 0;\n"
        "end\n"
        "endmodule",
        ModuleDeclaration(0, L(0, {"module", "multi_static", ";"}),
                          ModuleItemList(1, L(1, {"initial", "begin"}),
                                         N(2,                 //
                                           L(2, {"static"}),  //
                                           L(2, {"int"}),     //
                                           N(4, L(4, {"a", "=", "1", ","}),
                                             L(4, {"b", "=", "0", ";"}))),
                                         L(1, {"end"})),
                          L(0, {"endmodule"})),
    },
    {
        "module with const variable",
        "module const_variable;\n"
        "initial begin\n"
        "  const int a = 0;\n"
        "end\n"
        "endmodule",
        ModuleDeclaration(
            0, L(0, {"module", "const_variable", ";"}),
            ModuleItemList(1, L(1, {"initial", "begin"}),
                           N(2,  //
                                 // TODO(fangism): merge qualifiers with type.
                             L(2, {"const"}),  //
                             L(2, {"int"}),    //
                             L(2, {"a", "=", "0", ";"})),
                           L(1, {"end"})),
            L(0, {"endmodule"})),
    },
    {
        "module with const automatic variable",
        "module const_variable;\n"
        "initial begin\n"
        "  automatic const int a = 0;\n"
        "end\n"
        "endmodule",
        ModuleDeclaration(
            0, L(0, {"module", "const_variable", ";"}),
            ModuleItemList(1, L(1, {"initial", "begin"}),
                           N(2,
                             // TODO(fangism): merge qualifiers with type.
                             L(2, {"automatic", "const"}),  //
                             L(2, {"int"}),                 //
                             L(2, {"a", "=", "0", ";"})),
                           L(1, {"end"})),
            L(0, {"endmodule"})),
    },
    {
        "module with variable using multiple qualifiers",
        "module qualified;\n"
        "initial begin\n"
        "  const var automatic int a = 0;\n"
        "end\n"
        "endmodule",
        ModuleDeclaration(
            0, L(0, {"module", "qualified", ";"}),
            ModuleItemList(1, L(1, {"initial", "begin"}),
                           N(2,                                    //
                             L(2, {"const", "var", "automatic"}),  //
                             L(2, {"int"}),                        //
                             L(2, {"a", "=", "0", ";"})),
                           L(1, {"end"})),
            L(0, {"endmodule"})),
    },

    {
        "module with block generate statements",
        "module block_generate;\n"
        "generate\n"
        "endgenerate\n"
        "endmodule\n",
        ModuleDeclaration(
            0, L(0, {"module", "block_generate", ";"}),
            ModuleItemList(1, L(1, {"generate"}), L(1, {"endgenerate"})),
            L(0, {"endmodule"})),
    },

    {
        "module with block generate statements and macro call item",
        "module block_generate;\n"
        "`ASSERT(blah)\n"
        "generate\n"
        "endgenerate\n"
        "endmodule\n",
        ModuleDeclaration(
            0, L(0, {"module", "block_generate", ";"}),
            ModuleItemList(1,
                           N(1,                       //
                             L(1, {"`ASSERT", "("}),  //
                             L(3, {"blah", ")"})),
                           N(1,  //
                             L(1, {"generate"}), L(1, {"endgenerate"}))),
            L(0, {"endmodule"})),
    },

    {
        "module with conditional generate blocks, null statements",
        "module conditionals;\n"
        "if (foo) ;\n"
        "if (bar) ;\n"
        "endmodule",
        ModuleDeclaration(0, L(0, {"module", "conditionals", ";"}),
                          ModuleItemList(1, L(1, {"if", "(", "foo", ")", ";"}),
                                         L(1, {"if", "(", "bar", ")", ";"})),
                          L(0, {"endmodule"})),
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
        ModuleDeclaration(
            0, L(0, {"module", "conditionals", ";"}),
            ModuleItemList(
                1,
                FlowControl(1, L(1, {"if", "(", "foo", ")", "begin"}),
                            L(2, {"a", "aa", ";"}),  //
                            L(1, {"end"})),          //
                FlowControl(1, L(1, {"if", "(", "bar", ")", "begin"}),
                            L(2, {"b", "bb", ";"}),  //
                            L(1, {"end"}))),
            L(0, {"endmodule"})),
    },

    {
        "module with conditional generate statement blocks with labels",
        "module zv;\n"
        "if (x) begin\n"
        "end : l1\n"
        "else begin\n"
        "end\n"
        "endmodule;\n",
        ModuleDeclaration(0, L(0, {"module", "zv", ";"}),
                          FlowControl(1,    //
                                      N(1,  //
                                        L(1, {"if", "(", "x", ")", "begin"}),
                                        L(1, {"end", ":", "l1"})),
                                      N(1,                        //
                                        L(1, {"else", "begin"}),  //
                                        L(1, {"end"}))),
                          L(0, {"endmodule"})),
        L(0, {";"}),
    },

    {
        "module with conditional generate statement blocks",
        "module zw;\n"
        "if (x) begin\n"
        "end\n"
        "else begin\n"
        "end\n"
        "endmodule;\n",
        ModuleDeclaration(
            0, L(0, {"module", "zw", ";"}),
            FlowControl(1, L(1, {"if", "(", "x", ")", "begin"}),
                        N(1,  //
                          L(1, {"end", "else", "begin"}), L(1, {"end"}))),
            L(0, {"endmodule"})),
        L(0, {";"}),
    },

    {
        "module with conditional generate single-statements",
        "module zx;\n"
        "if (x) assign z=y;\n"
        "else assign x=y;\n"
        "endmodule;\n",
        ModuleDeclaration(0, L(0, {"module", "zx", ";"}),
                          FlowControl(1,                              //
                                      N(1,                            //
                                        L(1, {"if", "(", "x", ")"}),  //
                                        L(2, {"assign", "z", "=", "y", ";"})),
                                      N(1,               //
                                        L(1, {"else"}),  //
                                        L(2, {"assign", "x", "=", "y", ";"}))),
                          L(0, {"endmodule"})),
        L(0, {";"}),
    },

    {
        "module with conditional generate chained else-if, single-statements",
        "module zx;\n"
        "if (x) assign z=y;\n"
        "else if (r) assign z=w;\n"
        "else assign x=y;\n"
        "endmodule;\n",
        ModuleDeclaration(0, L(0, {"module", "zx", ";"}),
                          FlowControl(1,                              //
                                      N(1,                            //
                                        L(1, {"if", "(", "x", ")"}),  //
                                        L(2, {"assign", "z", "=", "y", ";"})),
                                      N(1,                                    //
                                        L(1, {"else", "if", "(", "r", ")"}),  //
                                        L(2, {"assign", "z", "=", "w", ";"})),
                                      N(1,               //
                                        L(1, {"else"}),  //
                                        L(2, {"assign", "x", "=", "y", ";"}))),
                          L(0, {"endmodule"})),
        L(0, {";"}),
    },

    {
        "module with conditional-else generate statement blocks with labels",
        "module zy;\n"
        "if (x) begin:z1\n"
        "assign x=y;\n"
        "end\n"
        "else\n"
        "if (y) begin:z2\n"
        "assign z=y;\n"
        "end\n"
        "endmodule;\n",
        ModuleDeclaration(
            0, L(0, {"module", "zy", ";"}),
            FlowControl(1,                                                  //
                        N(1,                                                //
                          L(1, {"if", "(", "x", ")", "begin", ":", "z1"}),  //
                          L(2, {"assign", "x", "=", "y", ";"})),            //
                        N(1,                                                //
                          L(1, {"end", "else", "if", "(", "y", ")", "begin",
                                ":", "z2"}),
                          L(2, {"assign", "z", "=", "y", ";"}),  //
                          L(1, {"end"}))),
            L(0, {"endmodule"})),
        L(0, {";"}),
    },

    {
        "module with conditional generate statement blocks with labels",
        "module zz;\n"
        "if (x) begin:z1\n"
        "assign x=y;\n"
        "end\n"
        "if (y) begin:z2\n"
        "assign z=y;\n"
        "end\n"
        "endmodule;\n",
        ModuleDeclaration(
            0, L(0, {"module", "zz", ";"}),
            ModuleItemList(
                1,
                FlowControl(1,  //
                            L(1, {"if", "(", "x", ")", "begin", ":", "z1"}),
                            L(2, {"assign", "x", "=", "y", ";"}),  //
                            L(1, {"end"})),
                FlowControl(1,  //
                            L(1, {"if", "(", "y", ")", "begin", ":", "z2"}),
                            L(2, {"assign", "z", "=", "y", ";"}),  //
                            L(1, {"end"}))),
            L(0, {"endmodule"})),
        L(0, {";"}),
    },

    {
        "module with conditional generate block and macro call item",
        "module conditional_generate_macros;\n"
        "if (foo) begin\n"
        "`COVER()\n"
        "`ASSERT()\n"
        "end\n"
        "endmodule\n",
        ModuleDeclaration(
            0, L(0, {"module", "conditional_generate_macros", ";"}),
            FlowControl(1, L(1, {"if", "(", "foo", ")", "begin"}),
                        ModuleItemList(2, L(2, {"`COVER", "(", ")"}),
                                       L(2, {"`ASSERT", "(", ")"})),
                        L(1, {"end"})),
            L(0, {"endmodule"})),
    },

    {
        "module with conditional generate block and comments",
        "module conditional_generate_comments;\n"
        "if (foo) begin\n"
        "// comment1\n"
        "// comment2\n"
        "end\n"
        "endmodule\n",
        ModuleDeclaration(
            0, L(0, {"module", "conditional_generate_comments", ";"}),
            FlowControl(
                1, L(1, {"if", "(", "foo", ")", "begin"}),
                ModuleItemList(2, L(2, {"// comment1"}), L(2, {"// comment2"})),
                L(1, {"end"})),
            L(0, {"endmodule"})),
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
        "module with single loop generate with null statement body",
        "module loop_generate;\n"
        "for (genvar x=1;x<N;++x)\n"
        "  ;\n"
        "endmodule",
        ModuleDeclaration(
            0, L(0, {"module", "loop_generate", ";"}),
            FlowControl(
                1,
                LoopHeader(1, L(1, {"for", "("}),
                           ForSpec(3,                                     //
                                   L(3, {"genvar", "x", "=", "1", ";"}),  //
                                   L(3, {"x", "<", "N", ";"}),            //
                                   L(3, {"++", "x"})),
                           L(1, {")"})),
                L(2, {";"})),
            L(0, {"endmodule"})),
    },

    {
        "module with single loop generate statement",
        "module loop_generate;\n"
        "for (genvar x=1;x<N;++x) begin\n"
        "  a aa;\n"
        "end\n"
        "endmodule",
        ModuleDeclaration(
            0, L(0, {"module", "loop_generate", ";"}),
            ModuleItemList(
                1,
                LoopHeader(1, L(1, {"for", "("}),
                           ForSpec(3,                                     //
                                   L(3, {"genvar", "x", "=", "1", ";"}),  //
                                   L(3, {"x", "<", "N", ";"}),            //
                                   L(3, {"++", "x"})),
                           L(1, {")", "begin"})),
                L(2, {"a", "aa", ";"}), L(1, {"end"})),
            L(0, {"endmodule"})),
    },

    {
        "module with loop generate continuous assignments",
        "module loop_generate_assign;\n"
        "for (genvar x=1;x<N;++x) begin"
        "  assign x = y;assign y = z;"
        "end\n"
        "endmodule",
        ModuleDeclaration(
            0, L(0, {"module", "loop_generate_assign", ";"}),
            ModuleItemList(
                1,
                LoopHeader(
                    1, L(1, {"for", "("}),
                    ForSpec(3, L(3, {"genvar", "x", "=", "1", ";"}),
                            L(3, {"x", "<", "N", ";"}), L(3, {"++", "x"})),
                    L(1, {")", "begin"})),
                ModuleItemList(2,  //
                               L(2, {"assign", "x", "=", "y", ";"}),
                               L(2, {"assign", "y", "=", "z", ";"})),
                L(1, {"end"})),
            L(0, {"endmodule"})),
    },

    {
        "module with standalone genvar statement",
        "module loop_standalone_genvar;\n"
        "genvar i;"
        "for (i=1;i<N;++i) begin\n"
        "end\n"
        "endmodule",
        ModuleDeclaration(
            0, L(0, {"module", "loop_standalone_genvar", ";"}),
            ModuleItemList(
                1, L(1, {"genvar", "i", ";"}),
                FlowControl(1,
                            LoopHeader(1, L(1, {"for", "("}),
                                       ForSpec(3, L(3, {"i", "=", "1", ";"}),
                                               L(3, {"i", "<", "N", ";"}),
                                               L(3, {"++", "i"})),
                                       L(1, {")", "begin"})),
                            L(1, {"end"}))),
            L(0, {"endmodule"})),
    },

    {
        "module with multiple arguments to genvar statement",
        "module loop_multiarg_genvar;\n"
        "genvar i,j;"
        "for (i=1;i<N;++i) begin\n"
        "end\n"
        "for (j=N;j>0;--j) begin\n"
        "end\n"
        "endmodule",
        ModuleDeclaration(
            0, L(0, {"module", "loop_multiarg_genvar", ";"}),
            ModuleItemList(
                1, L(1, {"genvar", "i", ",", "j", ";"}),
                FlowControl(1,
                            LoopHeader(1, L(1, {"for", "("}),
                                       ForSpec(3, L(3, {"i", "=", "1", ";"}),
                                               L(3, {"i", "<", "N", ";"}),
                                               L(3, {"++", "i"})),
                                       L(1, {")", "begin"})),
                            L(1, {"end"})),
                FlowControl(1,
                            LoopHeader(1, L(1, {"for", "("}),
                                       ForSpec(3, L(3, {"j", "=", "N", ";"}),
                                               L(3, {"j", ">", "0", ";"}),
                                               L(3, {"--", "j"})),
                                       L(1, {")", "begin"})),
                            L(1, {"end"}))),
            L(0, {"endmodule"})),
    },

    {
        "module with multiple genvar statements",
        "module loop_multi_genvar;\n"
        "genvar i;"
        "genvar j;"
        "for (i=1;i<N;++i) begin\n"
        "end\n"
        "for (j=N;j>0;--j) begin\n"
        "end\n"
        "endmodule",
        ModuleDeclaration(
            0, L(0, {"module", "loop_multi_genvar", ";"}),
            ModuleItemList(
                1, L(1, {"genvar", "i", ";"}), L(1, {"genvar", "j", ";"}),
                FlowControl(1,
                            LoopHeader(1, L(1, {"for", "("}),
                                       ForSpec(3, L(3, {"i", "=", "1", ";"}),
                                               L(3, {"i", "<", "N", ";"}),
                                               L(3, {"++", "i"})),
                                       L(1, {")", "begin"})),
                            L(1, {"end"})),
                FlowControl(1,
                            LoopHeader(1, L(1, {"for", "("}),
                                       ForSpec(3, L(3, {"j", "=", "N", ";"}),
                                               L(3, {"j", ">", "0", ";"}),
                                               L(3, {"--", "j"})),
                                       L(1, {")", "begin"})),
                            L(1, {"end"}))),
            L(0, {"endmodule"})),
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
        ModuleDeclaration(
            0, L(0, {"module", "loop_generates", ";"}),
            ModuleItemList(
                1,
                FlowControl(1,
                            LoopHeader(1, L(1, {"for", "("}),
                                       ForSpec(3, L(3, {"x", "=", "1", ";"}),
                                               L(3, {";"})),
                                       L(1, {")", "begin"})),
                            L(2, {"a", "aa", ";"}), L(1, {"end"})),  //
                FlowControl(1,
                            LoopHeader(1, L(1, {"for", "("}),
                                       ForSpec(3, L(3, {"y", "=", "0", ";"}),
                                               L(3, {";"})),
                                       L(1, {")", "begin"})),
                            L(2, {"b", "bb", ";"}), L(1, {"end"}))),
            L(0, {"endmodule"})),
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
        ModuleDeclaration(
            0, L(0, {"module", "multi_cases", ";"}),
            ModuleItemList(1,
                           FlowControl(1, L(1, {"case", "(", "foo", ")"}),
                                       N(2, L(2, {"A", ":"}),  //
                                         L(2, {"a", "aa", ";"})),
                                       L(1, {"endcase"})),
                           FlowControl(1, L(1, {"case", "(", "bar", ")"}),
                                       N(2, L(2, {"B", ":"}),  //
                                         L(2, {"b", "bb", ";"})),
                                       L(1, {"endcase"}))),
            L(0, {"endmodule"})),
    },

    {
        "module with case generate statements, and comments",
        "module multi_cases;\n"
        "case (foo)//c1\n"
        "//c2\n"
        "  A: a aa;//c3\n"
        "//c4\n"
        "endcase\n"
        "endmodule",
        ModuleDeclaration(
            0, L(0, {"module", "multi_cases", ";"}),
            FlowControl(1, L(1, {"case", "(", "foo", ")", "//c1"}),
                        N(2,                      //
                          L(2, {"//c2"}),         //
                          N(2, L(2, {"A", ":"}),  //
                            L(2, {"a", "aa", ";", "//c3"})),
                          L(2, {"//c4"})  //
                          ),
                        L(1, {"endcase"})),
            L(0, {"endmodule"})),
    },

    {
        "module with case generate block statements",
        "module case_block;\n"
        "case (foo)\n"
        "  A: begin\n"
        "    a aa;\n"
        "  end\n"
        "endcase\n"
        "endmodule",
        ModuleDeclaration(0, L(0, {"module", "case_block", ";"}),
                          FlowControl(1, L(1, {"case", "(", "foo", ")"}),
                                      N(2,                          //
                                        L(2, {"A", ":", "begin"}),  //
                                        L(3, {"a", "aa", ";"}),     //
                                        L(2, {"end"})),
                                      L(1, {"endcase"})),
                          L(0, {"endmodule"})),
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
        ModuleDeclaration(
            0, L(0, {"module", "case_statements", ";"}),
            ModuleItemList(
                1, L(1, {"always_comb", "begin"}),
                FlowControl(
                    2, L(2, {"case", "(", "blah", ".", "blah", ")"}),
                    CaseItemList(
                        3,  //
                        // TODO(fangism): may want to wrap case item statements
                        N(3, L(3, {"aaa", ",", "bbb", ":"}),  //
                          L(3, {"x", "=", "y", ";"})),        //
                        N(3, L(3, {"ccc", ",", "ddd", ":"}),  //
                          L(3, {"w", "=", "z", ";"}))         //
                        ),
                    L(2, {"endcase"})),
                L(1, {"end"})),
            L(0, {"endmodule"})),
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
        ModuleDeclaration(
            0, L(0, {"module", "case_statements", ";"}),
            ModuleItemList(
                1, L(1, {"initial", "begin"}),
                FlowControl(
                    2, L(2, {"case", "(", "blah", ".", "blah", ")"}),
                    CaseItemList(3,
                                 N(3, L(3, {"aaa", ",", "bbb", ":"}),
                                   L(3, {"x", "=", "`YYY", "(", ")", ";"})),
                                 N(3, L(3, {"default", ":"}),
                                   L(3, {"w", "=", "`ZZZ", "(", ")", ";"}))),
                    L(2, {"endcase"})),
                L(1, {"end"})),
            L(0, {"endmodule"})),
    },

    {
        "module case/default statements with begin-end blocks",
        "module case_statements;\n"  // case statements
        "initial begin\n"
        "  case (blah.blah)\n"
        "    aaa,bbb: begin x = Y; end\n"
        "    default: begin w = Z; end\n"
        "  endcase\n"
        "end\n"
        "endmodule",
        ModuleDeclaration(
            0, L(0, {"module", "case_statements", ";"}),
            ModuleItemList(
                1, L(1, {"initial", "begin"}),
                FlowControl(
                    2, L(2, {"case", "(", "blah", ".", "blah", ")"}),
                    CaseItemList(3,
                                 N(3,  //
                                   L(3, {"aaa", ",", "bbb", ":", "begin"}),
                                   L(4, {"x", "=", "Y", ";"}), L(3, {"end"})),
                                 N(3,  //
                                   L(3, {"default", ":", "begin"}),
                                   L(4, {"w", "=", "Z", ";"}), L(3, {"end"}))),
                    L(2, {"endcase"})),
                L(1, {"end"})),
            L(0, {"endmodule"})),
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
        ModuleDeclaration(
            0, L(0, {"module", "multiple_case_statements", ";"}),
            ModuleItemList(
                1, L(1, {"always_comb", "begin"}),
                StatementList(
                    2,
                    FlowControl(2,
                                L(2, {"case", "(", "blah", ".", "blah", ")"}),
                                N(3, L(3, {"aaa", ",", "bbb", ":"}),
                                  L(3, {"x", "=", "y", ";"})),
                                L(2, {"endcase"})),
                    FlowControl(2,
                                L(2, {"case", "(", "blah", ".", "blah", ")"}),
                                N(3, L(3, {"ccc", ",", "ddd", ":"}),
                                  L(3, {"w", "=", "z", ";"})),
                                L(2, {"endcase"}))),
                L(1, {"end"})),
            L(0, {"endmodule"})),
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
        ModuleDeclaration(
            0, L(0, {"module", "multiple_initial_final_statements", ";"}),
            ModuleItemList(1, L(1, {"begin"}), L(1, {"end"}),
                           N(1, L(1, {"initial", "begin"}), L(1, {"end"})),
                           N(1, L(1, {"final", "begin"}), L(1, {"end"}))),
            L(0, {"endmodule"})),
    },

    {
        "module with two consecutive clocking declarations",
        "module mcd;\n"
        "  clocking cb @(posedge clk);\n"
        "  endclocking\n"
        "  clocking cb2 @(posedge clk);\n"
        "  endclocking\n"
        "endmodule",
        ModuleDeclaration(0, L(0, {"module", "mcd", ";"}),
                          ModuleItemList(1,    //
                                         N(1,  //
                                           L(1, {"clocking", "cb", "@", "(",
                                                 "posedge", "clk", ")", ";"}),
                                           L(1, {"endclocking"})),
                                         N(1,  //
                                           L(1, {"clocking", "cb2", "@", "(",
                                                 "posedge", "clk", ")", ";"}),
                                           L(1, {"endclocking"}))),
                          L(0, {"endmodule"})),
    },

    {
        "module containing clocking declaration with ports",
        "module mcd;\n"
        "  clocking cb @(posedge clk);\n"
        "    input a;\n"
        "    output b;\n"
        "  endclocking\n"
        "endmodule",
        ModuleDeclaration(
            0, L(0, {"module", "mcd", ";"}),
            N(1,  //
              L(1, {"clocking", "cb", "@", "(", "posedge", "clk", ")", ";"}),
              TFPortList(2,  //
                         L(2, {"input", "a", ";"}), L(2, {"output", "b", ";"})),
              L(1, {"endclocking"})),
            L(0, {"endmodule"})),
    },

    {
        "module with DPI import declarations",
        "module mdi;"
        "import   \"DPI-C\" function int add();"
        "import \"DPI-C\"  function int  sleep( input int secs );"
        "endmodule",
        ModuleDeclaration(
            0, L(0, {"module", "mdi", ";"}),
            ModuleItemList(1,  //
                           L(1, {"import", "\"DPI-C\"", "function", "int",
                                 "add", "(", ")", ";"}),
                           N(1,  //
                             L(1, {"import", "\"DPI-C\"", "function", "int",
                                   "sleep", "("}),
                             L(2, {"input", "int", "secs"}), L(1, {")", ";"}))),
            L(0, {"endmodule"})),
    },

    {
        "module with comment inside continuous assignment",
        "module m;\n"
        "// comment1\n"
        "assign aaaaa = (bbbbb != ccccc) &\n"
        "// comment2\n"
        "(ddddd | (eeeee & ffffff));\n"
        "endmodule\n",
        ModuleDeclaration(
            0, L(0, {"module", "m", ";"}),
            ModuleItemList(1,                      //
                           L(1, {"// comment1"}),  //
                           N(1,
                             L(1, {"assign", "aaaaa", "=", "(", "bbbbb",
                                   "!=", "ccccc", ")", "&"}),  //
                             L(3, {"// comment2"}),            //
                             L(3, {"(", "ddddd", "|", "(", "eeeee", "&",
                                   "ffffff", ")", ")", ";"}))),  //
            L(0, {"endmodule"})),
    },

    {
        "module with pair of procedural continuous assignment statements",
        "module proc_cont_assigner;\n"
        "always begin\n"
        "assign x1 = y1;\n"
        "assign x2 = y2;\n"
        "end\n"
        "endmodule\n",
        ModuleDeclaration(0, L(0, {"module", "proc_cont_assigner", ";"}),
                          N(1, L(1, {"always", "begin"}),
                            N(2, L(2, {"assign", "x1", "=", "y1", ";"}),
                              L(2, {"assign", "x2", "=", "y2", ";"})),
                            L(1, {"end"})),
                          L(0, {"endmodule"})),
    },

    {
        "module with pair of procedural continuous force statements",
        "module proc_cont_forcer;\n"
        "always begin\n"
        "force x1 = y1;\n"
        "force x2 = y2;\n"
        "end\n"
        "endmodule\n",
        ModuleDeclaration(0, L(0, {"module", "proc_cont_forcer", ";"}),
                          N(1, L(1, {"always", "begin"}),
                            N(2, L(2, {"force", "x1", "=", "y1", ";"}),
                              L(2, {"force", "x2", "=", "y2", ";"})),
                            L(1, {"end"})),
                          L(0, {"endmodule"})),
    },

    {
        "module procedural continuous force statements, macro rvalues",
        "module proc_cont_forcer;\n"
        "always begin\n"
        "force x1 = `y1();\n"
        "force x2 = `y2(f, g);\n"
        "end\n"
        "endmodule\n",
        ModuleDeclaration(
            0, L(0, {"module", "proc_cont_forcer", ";"}),
            N(1, L(1, {"always", "begin"}),
              N(2, L(2, {"force", "x1", "=", "`y1", "(", ")", ";"}),
                N(2, L(2, {"force", "x2", "=", "`y2", "("}),
                  N(6, L(6, {"f", ","}), L(6, {"g", ")", ";"})))),
              L(1, {"end"})),
            L(0, {"endmodule"})),
    },

    {
        "module with pair of procedural continuous de-assignment statements",
        "module proc_cont_deassigner;\n"
        "always begin\n"
        "deassign x1 ;\n"
        "deassign x2 ;\n"
        "end\n"
        "endmodule\n",
        ModuleDeclaration(0, L(0, {"module", "proc_cont_deassigner", ";"}),
                          N(1, L(1, {"always", "begin"}),
                            N(2, L(2, {"deassign", "x1", ";"}),
                              L(2, {"deassign", "x2", ";"})),
                            L(1, {"end"})),
                          L(0, {"endmodule"})),
    },

    {
        "module with pair of procedural continuous release statements",
        "module proc_cont_releaser;\n"
        "always begin\n"
        "release x1 ;\n"
        "release x2 ;\n"
        "end\n"
        "endmodule\n",
        ModuleDeclaration(
            0, L(0, {"module", "proc_cont_releaser", ";"}),
            N(1, L(1, {"always", "begin"}),
              N(2, L(2, {"release", "x1", ";"}), L(2, {"release", "x2", ";"})),
              L(1, {"end"})),
            L(0, {"endmodule"})),
    },

    {
        "module with various procedural continuous assignment statements",
        "module proc_cont_assigner;\n"
        "always begin\n"
        "assign x1 = y1;\n"
        "deassign x2;\n"
        "force x3 = y3;\n"
        "release x4;\n"
        "end\n"
        "endmodule\n",
        ModuleDeclaration(0, L(0, {"module", "proc_cont_assigner", ";"}),
                          N(1, L(1, {"always", "begin"}),
                            N(2, L(2, {"assign", "x1", "=", "y1", ";"}),
                              L(2, {"deassign", "x2", ";"}),
                              L(2, {"force", "x3", "=", "y3", ";"}),
                              L(2, {"release", "x4", ";"})),
                            L(1, {"end"})),
                          L(0, {"endmodule"})),
    },

    {
        "module using disable statements labelled begin.",
        "module disable_self;\n"
        "  always begin : block\n"
        "    disable disable_self.block;\n"
        "  end\n"
        "endmodule\n",
        ModuleDeclaration(
            0, L(0, {"module", "disable_self", ";"}),
            ModuleItemList(1, L(1, {"always", "begin", ":", "block"}),
                           L(2, {"disable", "disable_self", ".", "block", ";"}),
                           L(1, {"end"})),
            L(0, {"endmodule"})),
    },

    {
        "module using disable statements",
        "module disable_other;\n"
        "  always begin\n"
        "    disable disable_other.block;\n"
        "  end\n"
        "endmodule\n",
        ModuleDeclaration(0, L(0, {"module", "disable_other", ";"}),
                          ModuleItemList(1, L(1, {"always", "begin"}),
                                         L(2, {"disable", "disable_other", ".",
                                               "block", ";"}),
                                         L(1, {"end"})),
                          L(0, {"endmodule"})),
    },

    {
        "module with simple immediate assertion statement, inside initial",
        "module m_assert; initial assert (x); endmodule",
        ModuleDeclaration(0,  //
                          L(0, {"module", "m_assert", ";"}),
                          N(1,                  //
                            L(1, {"initial"}),  //
                            L(2, {"assert", "(", "x", ")", ";"})),
                          L(0, {"endmodule"})),
    },
    {
        "module with simple immediate assertion statement, inside final",
        "module m_assert; final assert (z); endmodule",
        ModuleDeclaration(0,  //
                          L(0, {"module", "m_assert", ";"}),
                          N(1,                //
                            L(1, {"final"}),  //
                            L(2, {"assert", "(", "z", ")", ";"})),
                          L(0, {"endmodule"})),
    },
    {
        "module with simple immediate assertion statement, inside always",
        "module m_assert; always_comb assert (y); endmodule",
        ModuleDeclaration(0,  //
                          L(0, {"module", "m_assert", ";"}),
                          N(1,                      //
                            L(1, {"always_comb"}),  //
                            L(2, {"assert", "(", "y", ")", ";"})),
                          L(0, {"endmodule"})),
    },
    {
        "module: simple immediate assertion statement, inside initial block",
        "module m_assert; initial begin assert (x); end endmodule",
        ModuleDeclaration(0,  //
                          L(0, {"module", "m_assert", ";"}),
                          N(1,                                     //
                            L(1, {"initial", "begin"}),            //
                            L(2, {"assert", "(", "x", ")", ";"}),  //
                            L(1, {"end"})),
                          L(0, {"endmodule"})),
    },
    {
        "module: simple immediate assertion statement, inside final block",
        "module m_assert; final begin assert (x); end endmodule",
        ModuleDeclaration(0,  //
                          L(0, {"module", "m_assert", ";"}),
                          N(1,                                     //
                            L(1, {"final", "begin"}),              //
                            L(2, {"assert", "(", "x", ")", ";"}),  //
                            L(1, {"end"})),
                          L(0, {"endmodule"})),
    },
    {
        "module: simple immediate assertion statement, inside always block",
        "module m_assert; always_comb begin assert (x); end endmodule",
        ModuleDeclaration(0,  //
                          L(0, {"module", "m_assert", ";"}),
                          N(1,                                     //
                            L(1, {"always_comb", "begin"}),        //
                            L(2, {"assert", "(", "x", ")", ";"}),  //
                            L(1, {"end"})),
                          L(0, {"endmodule"})),
    },

    {
        "module: simple initial statement with function call",
        "module m;initial aa(bb,cc,dd,ee);endmodule",
        ModuleDeclaration(0, L(0, {"module", "m", ";"}),
                          N(1, L(1, {"initial"}),
                            N(2, L(2, {"aa", "("}),
                              N(4, L(4, {"bb", ","}), L(4, {"cc", ","}),
                                L(4, {"dd", ","}), L(4, {"ee", ")", ";"})))),
                          L(0, {"endmodule"})),
    },
    {
        "module: expressions and function calls inside if-statement headers",
        "module m;"
        "initial begin "
        "if (aa(bb) == cc(dd)) a = b;"
        "if (xx()) b = a;"
        "end "
        "endmodule",
        ModuleDeclaration(0, L(0, {"module", "m", ";"}),
                          N(1, L(1, {"initial", "begin"}),
                            N(2,
                              N(2,
                                N(2, L(2, {"if", "(", "aa", "("}), L(6, {"bb"}),
                                  L(4, {")", "==", "cc", "("}), L(6, {"dd"}),
                                  L(4, {")", ")"})),
                                L(3, {"a", "=", "b", ";"})),
                              N(2, L(2, {"if", "(", "xx", "(", ")", ")"}),
                                L(3, {"b", "=", "a", ";"}))),
                            L(1, {"end"})),
                          L(0, {"endmodule"})),
    },
    {
        "module: fuction with two arguments inside if-statement headers",
        "module m;"
        "initial begin "
        "if (aa(bb, cc)) x = y;"
        "end "
        "endmodule",
        ModuleDeclaration(
            0, L(0, {"module", "m", ";"}),
            N(1, L(1, {"initial", "begin"}),
              N(2,
                N(2, L(2, {"if", "(", "aa", "("}),
                  N(6, L(6, {"bb", ","}), L(6, {"cc"})), L(4, {")", ")"})),
                L(3, {"x", "=", "y", ";"})),
              L(1, {"end"})),
            L(0, {"endmodule"})),
    },
    {
        "module: kMethodCallExtension inside if-statement headers",
        "module m;"
        "initial begin "
        "if (aa.bb(cc)) x = y;"
        "end "
        "endmodule",
        ModuleDeclaration(0, L(0, {"module", "m", ";"}),
                          N(1, L(1, {"initial", "begin"}),
                            N(2,
                              N(2, L(2, {"if", "(", "aa", ".", "bb", "("}),
                                L(6, {"cc"}), L(4, {")", ")"})),
                              L(3, {"x", "=", "y", ";"})),
                            L(1, {"end"})),
                          L(0, {"endmodule"})),
    },
    {
        "module: initial statement with object method call",
        "module m; initial a.b(a,b,c); endmodule",
        ModuleDeclaration(0, L(0, {"module", "m", ";"}),
                          N(1, L(1, {"initial"}),
                            N(2, L(2, {"a", ".", "b", "("}),
                              N(4, L(4, {"a", ","}), L(4, {"b", ","}),
                                L(4, {"c", ")", ";"})))),
                          L(0, {"endmodule"})),
    },
    {
        "module: initial statement with method call on indexed object",
        "module m; initial a[i].b(a,b,c); endmodule",
        ModuleDeclaration(0, L(0, {"module", "m", ";"}),
                          N(1, L(1, {"initial"}),
                            N(2, L(2, {"a", "[", "i", "]", ".", "b", "("}),
                              N(4, L(4, {"a", ","}), L(4, {"b", ","}),
                                L(4, {"c", ")", ";"})))),
                          L(0, {"endmodule"})),
    },
    {
        "module: initial statement with method call on function returned "
        "object",
        "module m; initial a(d,e,f).b(a,b,c); endmodule",
        ModuleDeclaration(
            0, L(0, {"module", "m", ";"}),
            N(1, L(1, {"initial"}),
              N(2, L(2, {"a", "("}),
                N(4, L(4, {"d", ","}), L(4, {"e", ","}), L(4, {"f"})),
                L(2, {")", ".", "b", "("}),
                N(4, L(4, {"a", ","}), L(4, {"b", ","}),
                  L(4, {"c", ")", ";"})))),
            L(0, {"endmodule"})),
    },
    {
        "module: initial statement with indexed access to function returned "
        "object",
        "module m; initial a(a,b,c)[i]; endmodule",
        ModuleDeclaration(
            0, L(0, {"module", "m", ";"}),
            N(1, L(1, {"initial"}),
              N(2, L(2, {"a", "("}),
                N(4, L(4, {"a", ","}), L(4, {"b", ","}), L(4, {"c"})),
                L(2, {")", "[", "i", "]", ";"}))),
            L(0, {"endmodule"})),
    },

    {
        "module: method call with no arguments on an object",
        "module m; initial foo.bar();endmodule",
        ModuleDeclaration(
            0, L(0, {"module", "m", ";"}),
            N(1, L(1, {"initial"}), L(2, {"foo", ".", "bar", "(", ")", ";"})),
            L(0, {"endmodule"})),
    },
    {
        "module: method call with one argument on an object",
        "module m; initial foo.bar(aa);endmodule",
        ModuleDeclaration(
            0, L(0, {"module", "m", ";"}),
            N(1, L(1, {"initial"}),
              N(2, L(2, {"foo", ".", "bar", "("}), L(4, {"aa", ")", ";"}))),
            L(0, {"endmodule"})),
    },
    {
        "module: method call with two arguments on an object",
        "module m; initial foo.bar(aa,bb);endmodule",
        ModuleDeclaration(0, L(0, {"module", "m", ";"}),
                          N(1, L(1, {"initial"}),
                            N(2, L(2, {"foo", ".", "bar", "("}),
                              N(4, L(4, {"aa", ","}), L(4, {"bb", ")", ";"})))),
                          L(0, {"endmodule"})),
    },
    {
        "module: method call with three arguments on an object",
        "module m; initial foo.bar(aa,bb,cc);endmodule",
        ModuleDeclaration(0, L(0, {"module", "m", ";"}),
                          N(1, L(1, {"initial"}),
                            N(2, L(2, {"foo", ".", "bar", "("}),
                              N(4, L(4, {"aa", ","}), L(4, {"bb", ","}),
                                L(4, {"cc", ")", ";"})))),
                          L(0, {"endmodule"})),
    },

    // specify block tests
    {
        "module with empty specify block",
        "module specify_m;\n"
        "  specify\n"
        "  endspecify\n"
        "endmodule\n",
        ModuleDeclaration(0,                                   //
                          L(0, {"module", "specify_m", ";"}),  //
                          N(1,                                 //
                            L(1, {"specify"}),                 //
                            L(1, {"endspecify"})),
                          L(0, {"endmodule"})),
    },
    {
        "module with empty specify block with comment",
        "module specify_m;\n"
        "  specify\n"
        "//comment\n"
        "  endspecify\n"
        "endmodule\n",
        ModuleDeclaration(0,                                   //
                          L(0, {"module", "specify_m", ";"}),  //
                          N(1,                                 //
                            L(1, {"specify"}),                 //
                            L(2, {"//comment"}),               //
                            L(1, {"endspecify"})),
                          L(0, {"endmodule"})),
    },
    {
        "module with empty specify block with comments",
        "module specify_m;\n"
        "  specify\n"
        "//comment 1\n"
        "//comment 2\n"
        "  endspecify\n"
        "endmodule\n",
        ModuleDeclaration(0,                                   //
                          L(0, {"module", "specify_m", ";"}),  //
                          N(1,                                 //
                            L(1, {"specify"}),                 //
                            N(2,                               //
                              L(2, {"//comment 1"}),           //
                              L(2, {"//comment 2"})),          //
                            L(1, {"endspecify"})),
                          L(0, {"endmodule"})),
    },
    {
        "module with empty specify block with one timing spec",
        "module specify_m;\n"
        "  specify\n"
        "$setup(posedge x, posedge y, tt);\n"
        "  endspecify\n"
        "endmodule\n",
        ModuleDeclaration(0,                                   //
                          L(0, {"module", "specify_m", ";"}),  //
                          N(1,                                 //
                            L(1, {"specify"}),                 //
                            L(2, {"$setup", "(", "posedge", "x", ",", "posedge",
                                  "y", ",", "tt", ")", ";"}),
                            L(1, {"endspecify"})),
                          L(0, {"endmodule"})),
    },
    {
        "module with empty specify block with two timing specs",
        "module specify_m;\n"
        "  specify\n"
        "$setup(posedge x, posedge y, tt);\n"
        "$hold(posedge y, posedge x, tw);\n"
        "  endspecify\n"
        "endmodule\n",
        ModuleDeclaration(0,                                   //
                          L(0, {"module", "specify_m", ";"}),  //
                          N(1,                                 //
                            L(1, {"specify"}),                 //
                            N(2,                               //
                              L(2, {"$setup", "(", "posedge", "x", ",",
                                    "posedge", "y", ",", "tt", ")", ";"}),
                              L(2, {"$hold", "(", "posedge", "y", ",",
                                    "posedge", "x", ",", "tw", ")", ";"})),
                            L(1, {"endspecify"})),
                          L(0, {"endmodule"})),
    },
    {
        "module with empty specify block with conditional timing specs",
        "module specify_m;\n"
        "  specify\n"
        "`ifdef FOO\n"
        "$setup(posedge x, posedge y, tt);\n"
        "`else\n"
        "$hold(posedge y, posedge x, tw);\n"
        "`endif\n"
        "  endspecify\n"
        "endmodule\n",
        ModuleDeclaration(0,                                   //
                          L(0, {"module", "specify_m", ";"}),  //
                          N(1,                                 //
                            L(1, {"specify"}),                 //
                            N(2,                               //
                              L(0, {"`ifdef", "FOO"}),         //
                              L(2, {"$setup", "(", "posedge", "x", ",",
                                    "posedge", "y", ",", "tt", ")", ";"}),
                              L(0, {"`else"}),  //
                              L(2, {"$hold", "(", "posedge", "y", ",",
                                    "posedge", "x", ",", "tw", ")", ";"}),
                              L(0, {"`endif"})),  //
                            L(1, {"endspecify"})),
                          L(0, {"endmodule"})),
    },
};

// Test that TreeUnwrapper produces the correct UnwrappedLines from module tests
TEST_F(TreeUnwrapperTest, UnwrapModuleTests) {
  for (const auto &test_case : kUnwrapModuleTestCases) {
    VLOG(1) << "Test: " << test_case.test_name;
    auto tree_unwrapper = CreateTreeUnwrapper(test_case.source_code);
    const auto *uwline_tree = tree_unwrapper->Unwrap();
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
        ModuleDeclaration(0,  //
                          L(0, {"module", "foo", "(", ")", ";"}),
                          L(0, {"endmodule"})),  //
        L(0, {"// end comment"}),                // comment on own line
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
        ModuleDeclaration(0,  //
                          L(0, {"module", "foo", "(", ")", ";"}),
                          L(0, {"endmodule"})),  //
        L(0, {"// comment2"}),                   // comment on own line
        L(0, {"// comment3"}),                   //
        ModuleDeclaration(0,                     //
                          L(0, {"module", "bar", "(", ")", ";"}),
                          L(0, {"endmodule"})),  //
        L(0, {"// comment4"}),                   // comment on own line
    },

    {
        "module item comments only",
        "module foo ();\n"
        "// item comment 1\n"
        "// item comment 2\n"
        "endmodule\n",
        ModuleDeclaration(0,  //
                          L(0, {"module", "foo", "(", ")", ";"}),
                          ModuleItemList(1, L(1, {"// item comment 1"}),  //
                                         L(1, {"// item comment 2"})      //
                                         ),
                          L(0, {"endmodule"})),
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
        ModuleDeclaration(
            0,
            ModuleHeader(0, L(0, {"module", "foo", "(", "// non-port comment"}),
                         ModulePortList(2, L(2, {"// port comment 1"}),
                                        L(2, {"// port comment 2"})),
                         L(0, {")", ";", "// header trailing comment"})),
            ModuleItemList(1, L(1, {"// item comment 1"}),  //
                           L(1, {"// item comment 2"})      //
                           ),
            L(0, {"endmodule"})),
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
        ModuleDeclaration(
            0,  //
            L(0,
              {"module", "foo", "(", ")", ";", "// comment at end of module"}),
            L(0, {"endmodule"})),  // comment separated to next line
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
        ModuleDeclaration(0, L(0, {"module", "foo", "(", ")", ";"}),
                          L(0, {"endmodule"})),
    },

    {
        "module with end of line comments in empty ports",
        "module foo ( // comment1\n"
        "// comment2\n"
        "// comment3\n"
        "); // comment4\n"
        "endmodule // endmodule comment\n",
        ModuleDeclaration(
            0,
            ModuleHeader(
                0, L(0, {"module", "foo", "(", "// comment1"}),
                ModulePortList(2, L(2, {"// comment2"}), L(2, {"// comment3"})),
                L(0, {")", ";", "// comment4"})),
            L(0, {"endmodule", "// endmodule comment"})),
    },

    {
        "module with end of line comments",
        "module foo ( // module foo ( comment!\n"
        "input bar, // input bar, comment\n"
        "output baz // output baz comment\n"
        "); // ); comment\n"
        "endmodule // endmodule comment\n",
        ModuleDeclaration(
            0,
            ModuleHeader(
                0, L(0, {"module", "foo", "(", "// module foo ( comment!"}),
                ModulePortList(
                    2, L(2, {"input", "bar", ",", "// input bar, comment"}),
                    L(2, {"output", "baz", "// output baz comment"})),
                L(0, {")", ";", "// ); comment"})),
            L(0, {"endmodule", "// endmodule comment"})),
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
        ClassDeclaration(
            0, L(0, {"class", "foo", ";", "// class foo; comment"}),
            ClassItemList(1, L(1, {"// comment on its own line"}),
                          L(1, {"// one more comment"}),
                          L(1, {"// and one last comment"}),
                          L(1, {"import", "fedex_pkg", "::", "box", ";"}),
                          L(1, {"// new comment for fun"}),
                          L(1, {"import", "fedex_pkg", "::", "*", ";"})),
            L(0, {"endclass", "// endclass comment"})),
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
        ModuleDeclaration(
            0,
            ModuleHeader(
                0, L(0, {"module", "foo", "(", "/* comment1 */"}),
                ModulePortList(2,
                               L(2, {"input", "/* comment2 */", "bar", ",",
                                     "/*comment3 */"}),
                               L(2, {"output", "/* comment4 */", "baz"})),
                L(0, {")", "/* comment5 */", ";"})),
            L(0, {"/* comment6 */"}), L(0, {"endmodule"})),
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
        ModuleDeclaration(
            0,
            ModuleHeader(
                0,
                L(0, {"module", "/* comment1 */", "foo", "(", "// comment2"}),
                ModulePortList(2,
                               L(2, {"input", "bar", ",", "/* comment3 */",
                                     "// comment4"}),
                               L(2, {"output", "baz", "// comment5"})),
                L(0, {")", ";", "// comment6"})),
            L(0, {"/* comment7 */"}), L(0, {"endmodule", "//comment8"})),
    },
};

// Test that TreeUnwrapper produces the correct UnwrappedLines from code with
// comments
TEST_F(TreeUnwrapperTest, UnwrapCommentsTests) {
  for (const auto &test_case : kUnwrapCommentsTestCases) {
    VLOG(1) << "Test: " << test_case.test_name;
    auto tree_unwrapper = CreateTreeUnwrapper(test_case.source_code);
    const auto *uwline_tree = tree_unwrapper->Unwrap();
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
        N(0, L(0, {"`uvm_object_utils_begin", "("}), L(2, {"l0", ")"})),
        N(1, L(1, {"`uvm_field_int", "("}),
          N(3, L(3, {"l1a", ","}), L(3, {"UVM_DEFAULT", ")"}))),
        N(1, L(1, {"`uvm_field_int", "("}),
          N(3, L(3, {"l1b", ","}), L(3, {"UVM_DEFAULT", ")"}))),
        L(0, {"`uvm_object_utils_end"}),
    },

    {
        "simple uvm field utils test case",
        "`uvm_field_utils_begin(l0)\n"
        "`uvm_field_int(l1a, UVM_DEFAULT)\n"
        "`uvm_field_int(l1b, UVM_DEFAULT)\n"
        "`uvm_field_utils_end\n",
        N(0, L(0, {"`uvm_field_utils_begin", "("}), L(2, {"l0", ")"})),
        N(1, L(1, {"`uvm_field_int", "("}),
          N(3, L(3, {"l1a", ","}), L(3, {"UVM_DEFAULT", ")"}))),
        N(1, L(1, {"`uvm_field_int", "("}),
          N(3, L(3, {"l1b", ","}), L(3, {"UVM_DEFAULT", ")"}))),
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
        N(0, L(0, {"`uvm_object_utils_begin", "("}), L(2, {"l0", ")"})),
        N(1, L(1, {"`uvm_field_int", "("}),
          N(3, L(3, {"l1a", ","}), L(3, {"UVM_DEFAULT", ")"}))),
        N(1, L(1, {"`uvm_object_utils_begin", "("}), L(3, {"l1", ")"})),
        N(2, L(2, {"`uvm_field_int", "("}),
          N(4, L(4, {"l2a", ","}), L(4, {"UVM_DEFAULT", ")"}))),
        N(2, L(2, {"`uvm_object_utils_begin", "("}), L(4, {"l2", ")"})),
        N(3, L(3, {"`uvm_field_int", "("}),
          N(5, L(5, {"l3a", ","}), L(5, {"UVM_DEFAULT", ")"}))),
        L(2, {"`uvm_object_utils_end"}),
        L(1, {"`uvm_object_utils_end"}),
        N(1, L(1, {"`uvm_field_int", "("}),
          N(3, L(3, {"l1b", ","}), L(3, {"UVM_DEFAULT", ")"}))),
        L(0, {"`uvm_object_utils_end"}),
    },

    {
        "missing uvm.*end macro test case",
        "`uvm_field_utils_begin(l0)\n"
        "`uvm_field_int(l1a, UVM_DEFAULT)\n"
        "`uvm_field_int(l1b, UVM_DEFAULT)\n",
        N(0, L(0, {"`uvm_field_utils_begin", "("}), L(2, {"l0", ")"})),
        N(0, L(0, {"`uvm_field_int", "("}),
          N(2, L(2, {"l1a", ","}), L(2, {"UVM_DEFAULT", ")"}))),
        N(0, L(0, {"`uvm_field_int", "("}),
          N(2, L(2, {"l1b", ","}), L(2, {"UVM_DEFAULT", ")"}))),
    },

    {
        "missing uvm.*begin macro test case",
        "`uvm_field_int(l1a, UVM_DEFAULT)\n"
        "`uvm_field_int(l1b, UVM_DEFAULT)\n"
        "`uvm_field_utils_end\n",
        N(0, L(0, {"`uvm_field_int", "("}),
          N(2, L(2, {"l1a", ","}), L(2, {"UVM_DEFAULT", ")"}))),
        N(0, L(0, {"`uvm_field_int", "("}),
          N(2, L(2, {"l1b", ","}), L(2, {"UVM_DEFAULT", ")"}))),
        L(0, {"`uvm_field_utils_end"}),
    },

    {
        "uvm macro statement test, with semicolon on same line",
        "task t;\n"
        "`uvm_error(foo, bar);\n"
        "endtask\n",
        TaskDeclaration(0, TaskHeader(0, {"task", "t", ";"}),
                        N(1,                          //
                          L(1, {"`uvm_error", "("}),  //
                          N(3,                        //
                            L(3, {"foo", ","}),       //
                            L(3, {"bar", ")", ";"}))),
                        L(0, {"endtask"})),
    },
    {
        "uvm macro statement test, detached null statement semicolon",
        "task t;\n"
        "`uvm_error(foo, bar)\n"
        ";\n"
        "endtask\n",
        TaskDeclaration(
            0, TaskHeader(0, {"task", "t", ";"}),
            StatementList(1,
                          N(1,                          //
                            L(1, {"`uvm_error", "("}),  //
                            N(3,                        //
                              L(3, {"foo", ","}),       //
                              L(3, {"bar", ")"}))),     //
                          L(1, {";"})  // null statement stays detached
                          ),
            L(0, {"endtask"})),
    },
};

// Test that TreeUnwrapper produces the correct UnwrappedLines from code with
// uvm macros
TEST_F(TreeUnwrapperTest, UnwrapUvmTests) {
  for (const auto &test_case : kUnwrapUvmTestCases) {
    VLOG(1) << "Test: " << test_case.test_name;
    auto tree_unwrapper = CreateTreeUnwrapper(test_case.source_code);
    const auto *uwline_tree = tree_unwrapper->Unwrap();
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
        ClassDeclaration(0, L(0, {"class", "Foo", ";"}), L(0, {"endclass"})),
    },

    {
        "virtual class",
        "virtual class automatic Foo; endclass",
        ClassDeclaration(0, L(0, {"virtual", "class", "automatic", "Foo", ";"}),
                         L(0, {"endclass"})),
    },

    {
        "extends class",
        "class Foo extends Bar #(x,y,z); endclass",
        ClassDeclaration(0,
                         L(0, {"class", "Foo", "extends", "Bar", "#", "(", "x",
                               ",", "y", ",", "z", ")", ";"}),
                         L(0, {"endclass"})),
    },
    {
        "extends class with type parameters",
        "class Foo extends Bar #(.x(x),.y(y)); endclass",
        ClassDeclaration(
            0,
            ClassHeader(0,  //
                        L(0, {"class", "Foo", "extends", "Bar", "#", "("}),
                        N(2,  //
                          L(2, {".", "x", "(", "x", ")", ","}),
                          L(2, {".", "y", "(", "y", ")"})),
                        L(0, {")", ";"})),
            L(0, {"endclass"})),
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
        ClassDeclaration(
            0, L(0, {"class", "Foo", ";"}),
            ClassItemList(
                1, L(1, {"integer", "sizer", ";"}),
                FunctionDeclaration(
                    1,
                    N(1, L(1, {"function", "new", "("}),
                      L(3, {"integer", "size", ")", ";"})),
                    StatementList(2, L(2, {"begin"}),
                                  L(3, {"this", ".", "size", "=", "size", ";"}),
                                  L(2, {"end"})),
                    L(1, {"endfunction"})),
                TaskDeclaration(
                    1, TaskHeader(1, {"task", "print", "(", ")", ";"}),
                    StatementList(2, L(2, {"begin"}),
                                  N(3, N(3, L(3, {"$write"}), L(3, {"("})),
                                    L(5, {"\"Hello, world!\"", ")", ";"})),
                                  L(2, {"end"})),
                    L(1, {"endtask"}))),
            L(0, {"endclass"})),
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
        ClassDeclaration(
            0, L(0, {"class", "c", ";", "// c is for cookie"}),
            ClassItemList(
                1, L(1, {"// f is for false"}),
                FunctionDeclaration(
                    1,
                    N(1, L(1, {"function", "f", "("}),
                      L(3, {"integer", "size", ")", ";"})),
                    L(1, {"endfunction"})),  //
                L(1, {"// t is for true"}),
                TaskDeclaration(1,
                                TaskHeader(1, {"task", "t", "(", ")", ";"}),  //
                                L(1, {"endtask"})),
                L(1, {"// class is about to end"})),
            L(0, {"endclass"})),
    },

    {
        "class import declarations",
        "class foo;\n"
        "  import fedex_pkg::box;\n"
        "  import fedex_pkg::*;\n"
        "endclass",
        ClassDeclaration(
            0, L(0, {"class", "foo", ";"}),
            ClassItemList(1, L(1, {"import", "fedex_pkg", "::", "box", ";"}),
                          L(1, {"import", "fedex_pkg", "::", "*", ";"})),
            L(0, {"endclass"})),
    },

    {
        "class macros as class item",
        "class macros_as_class_item;\n"
        " `uvm_warning()\n"
        " `uvm_error(  )\n"
        " `uvm_func_new(\n)\n"
        "endclass",
        ClassDeclaration(0, L(0, {"class", "macros_as_class_item", ";"}),
                         ClassItemList(1, L(1, {"`uvm_warning", "(", ")"}),
                                       L(1, {"`uvm_error", "(", ")"}),
                                       L(1, {"`uvm_func_new", "(", ")"})),
                         L(0, {"endclass"})),
    },

    {
        "class macro unwrapping",
        "class macro_unwrapping;\n"
        " `MACRO_CALL(\n"
        " // verilog_syntax: parse-as-statements\n"
        " int count;\n"
        " if(cfg) begin\n"
        " count = 1;\n"
        " end)\n"
        "endclass",
        ClassDeclaration(
            0, L(0, {"class", "macro_unwrapping", ";"}),
            N(1, L(1, {"`MACRO_CALL", "("}),
              N(3, L(3, {"// verilog_syntax: parse-as-statements"}),
                N(3, L(3, {"int", "count", ";"}),
                  FlowControl(3, L(3, {"if", "(", "cfg", ")", "begin"}),
                              L(4, {"count", "=", "1", ";"}),
                              L(3, {"end", ")"}))))),
            L(0, {"endclass"})),
    },
    {
        "class macro unwrapping with comment",
        "class macro_unwrapping_with_comment;\n"
        " `MACRO_CALL(\n"
        " // verilog_syntax: parse-as-statements\n"
        " int count;\n"
        " if(cfg) begin\n"
        " // parsed comment\n"
        " count = 1;\n"
        " end)\n"
        "endclass",
        ClassDeclaration(
            0, L(0, {"class", "macro_unwrapping_with_comment", ";"}),
            N(1, L(1, {"`MACRO_CALL", "("}),
              N(3, L(3, {"// verilog_syntax: parse-as-statements"}),
                N(3, L(3, {"int", "count", ";"}),
                  FlowControl(3, L(3, {"if", "(", "cfg", ")", "begin"}),
                              N(4, L(4, {"// parsed comment"}),
                                L(4, {"count", "=", "1", ";"})),
                              L(3, {"end", ")"}))))),
            L(0, {"endclass"})),
    },

    {
        "class with parameters as class items",
        "class params_as_class_item;\n"
        "  parameter N = 2;\n"
        "  parameter reg P = '1;\n"
        "  localparam M = f(glb::arr[N]) + 1;\n"
        "  localparam M = $f(glb::arr[N]) + 1;\n"
        "endclass",
        ClassDeclaration(
            0, L(0, {"class", "params_as_class_item", ";"}),
            ClassItemList(1, L(1, {"parameter", "N", "=", "2", ";"}),
                          L(1, {"parameter", "reg", "P", "=", "'1", ";"}),
                          N(1, L(1, {"localparam", "M", "=", "f", "("}),
                            L(3, {"glb", "::", "arr", "[", "N", "]"}),
                            L(1, {")", "+", "1", ";"})),
                          N(1, L(1, {"localparam", "M", "=", "$f", "("}),
                            L(3, {"glb", "::", "arr", "[", "N", "]"}),
                            L(1, {")", "+", "1", ";"}))),
            L(0, {"endclass"})),
    },

    {
        "class with events",
        "class event_calendar;\n"
        "  event birthday;\n"
        "  event first_date, anniversary;\n"
        "  event revolution[4:0], independence[2:0];\n"
        "endclass",
        ClassDeclaration(
            0, L(0, {"class", "event_calendar", ";"}),
            ClassItemList(
                1, L(1, {"event", "birthday", ";"}),
                DataDeclaration(1, L(1, {"event"}),
                                InstanceList(3,  //
                                             L(3, {"first_date", ","}),
                                             L(3, {"anniversary", ";"}))),
                DataDeclaration(
                    1, L(1, {"event"}),
                    InstanceList(
                        3,  //
                        L(3, {"revolution", "[", "4", ":", "0", "]", ","}),
                        L(3, {"independence", "[", "2", ":", "0", "]", ";"})))),
            L(0, {"endclass"})),
    },

    {
        "class with associative array declaration",
        "class Driver;\n"
        "  Packet pNP [*];\n"
        "  Packet pNP1 [*];\n"
        "endclass",
        ClassDeclaration(0, L(0, {"class", "Driver", ";"}),
                         ClassItemList(1, L(1, {"Packet", "pNP", "[*]", ";"}),
                                       L(1, {"Packet", "pNP1", "[*]", ";"})),
                         L(0, {"endclass"})),
    },

    {
        "class member declarations",
        "class fields_with_modifiers;\n"
        "  const data_type_or_module_type foo1 = 4'hf;\n"
        "  static data_type_or_module_type foo3, foo4;\n"
        "endclass",
        ClassDeclaration(
            0, L(0, {"class", "fields_with_modifiers", ";"}),
            ClassItemList(
                1,
                DataDeclaration(
                    1,  //
                        // TODO(b/149343440): merge qualifiers and type
                        // partitions together.
                    L(1, {"const"}), L(1, {"data_type_or_module_type"}),
                    L(1,  // TODO(b/149344110): should indent to level 3
                      {"foo1", "=", "4", "'h", "f", ";"})),
                DataDeclaration(
                    1,
                    // TODO(b/149343440): merge qualifiers and type
                    // partitions together.
                    L(1, {"static"}), L(1, {"data_type_or_module_type"}),
                    InstanceList(3,  //
                                 L(3, {"foo3", ","}), L(3, {"foo4", ";"})))),
            L(0, {"endclass"})),
    },

    {
        "class with preprocessing",
        "class pp_class;\n"
        "`ifdef DEBUGGER\n"
        "`ifdef VERBOSE\n"  // nested, empty
        "`endif\n"
        "`endif\n"
        "endclass",
        ClassDeclaration(
            0, L(0, {"class", "pp_class", ";"}),
            ClassItemList(1, L(0, {"`ifdef", "DEBUGGER"}),
                          N(1,  // nested ifdef
                            L(0, {"`ifdef", "VERBOSE"}), L(0, {"`endif"})),
                          L(0, {"`endif"})),
            L(0, {"endclass"})),
    },

    {
        "consecutive `define's",
        "class pp_class;\n"
        "`define FOO BAR\n"
        "`define BAR FOO+1\n"
        "endclass",
        ClassDeclaration(0, L(0, {"class", "pp_class", ";"}),
                         ClassItemList(1, L(1, {"`define", "FOO", "BAR"}),
                                       L(1, {"`define", "BAR", "FOO+1"})),
                         L(0, {"endclass"})),
    },

    {
        "class pure virtual tasks",
        "class myclass;\n"
        "pure virtual task pure_task1;\n"
        "pure virtual task pure_task2(arg_type arg);\n"
        "endclass",
        ClassDeclaration(
            0, L(0, {"class", "myclass", ";"}),
            ClassItemList(
                1,  //
                L(1, {"pure", "virtual", "task", "pure_task1", ";"}),
                N(1,  //
                  L(1, {"pure", "virtual", "task", "pure_task2", "("}),
                  L(3, {"arg_type", "arg", ")", ";"}))),
            L(0, {"endclass"})),
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
        ClassDeclaration(
            0, L(0, {"class", "outerclass", ";"}),
            ClassDeclaration(
                1, L(1, {"class", "innerclass", ";"}),
                ClassDeclaration(
                    2, L(2, {"class", "reallyinnerclass", ";"}),
                    TaskDeclaration(3, TaskHeader(3, {"task", "subtask", ";"}),
                                    L(3, {"endtask"})),
                    L(2, {"endclass"})),
                L(1, {"endclass"})),
            L(0, {"endclass"})),
    },

    {
        "class with protected members",
        "class protected_stuff;\n"
        "  protected int count;\n"
        "endclass",
        ClassDeclaration(
            0, L(0, {"class", "protected_stuff", ";"}),
            DataDeclaration(1,
                            // TODO(b/149343440): merge qualifiers and type
                            // partitions together.
                            L(1, {"protected"}), L(1, {"int"}),
                            // TODO(b/149344110): indent variable to level 3
                            L(1, {"count", ";"})),
            L(0, {"endclass"})),
    },

    {
        "class with virtual function",
        "class myclass;\n"
        "virtual function integer subroutine;\n"
        "  input a;\n"
        "  subroutine = a+42;\n"
        "endfunction\n"
        "endclass",
        ClassDeclaration(
            0, L(0, {"class", "myclass", ";"}),
            FunctionDeclaration(
                1, L(1, {"virtual", "function", "integer", "subroutine", ";"}),
                L(2, {"input", "a", ";"}),
                L(2, {"subroutine", "=", "a", "+", "42", ";"}),
                L(1, {"endfunction"})),
            L(0, {"endclass"})),
    },

    {
        "class constructor",
        "class constructible;\n"
        "function new ();\n"
        "endfunction\n"
        "endclass",
        ClassDeclaration(
            0, L(0, {"class", "constructible", ";"}),
            FunctionDeclaration(
                1, FunctionHeader(1, {"function", "new", "(", ")", ";"}),
                L(1, {"endfunction"})),
            L(0, {"endclass"})),
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
        ClassDeclaration(
            0, L(0, {"class", "myclass", ";"}),
            FunctionDeclaration(
                1,
                L(1, {"function", "apkg", "::", "num_t", "apply_all", "(", ")",
                      ";"}),
                StatementList(
                    2,
                    FlowControl(
                        2,
                        L(2, {"foreach", "(", "this", ".", "foo", "[", "i", "]",
                              ")", "begin"}),
                        StatementList(
                            3,
                            N(3, L(3, {"y", "=", "{"}),
                              N(4, L(4, {"y", ","}),
                                L(4, {"this", ".", "foo", "[", "i", "]"})),
                              L(3, {"}", ";"})),
                            N(3, L(3, {"z", "=", "{"}),
                              N(4, L(4, {"z", ","}),
                                L(4, {"super", ".", "bar", "[", "i", "]"})),
                              L(3, {"}", ";"}))),
                        L(2, {"end"})),
                    FlowControl(
                        2,
                        L(2, {"foreach", "(", "this", ".", "foo", "[", "i", "]",
                              ")", "begin"}),
                        StatementList(
                            3,
                            N(3, L(3, {"y", "=", "{"}),
                              N(4, L(4, {"y", ","}),
                                L(4, {"this", ".", "foo", "[", "i", "]"})),
                              L(3, {"}", ";"})),
                            N(3, L(3, {"z", "=", "{"}),
                              N(4, L(4, {"z", ","}),
                                L(4, {"super", ".", "bar", "[", "i", "]"})),
                              L(3, {"}", ";"}))),
                        L(2, {"end"}))),
                L(1, {"endfunction"})),
            L(0, {"endclass"})),
    },

    {
        "class with empty constraint",
        "class Foo; constraint empty_c { } endclass",
        ClassDeclaration(0, L(0, {"class", "Foo", ";"}),
                         L(1, {"constraint", "empty_c", "{", "}"}),
                         L(0, {"endclass"})),
    },

    {
        "class with constraint, simple expression",
        "class Foo; constraint empty_c { c < d; } endclass",
        ClassDeclaration(
            0, L(0, {"class", "Foo", ";"}),
            ConstraintDeclaration(1, L(1, {"constraint", "empty_c", "{"}),
                                  L(2, {"c", "<", "d", ";"}), L(1, {"}"})),
            L(0, {"endclass"})),
    },

    {
        "class with empty constraint, only comments",
        "class Foo; constraint empty_c { //c1\n"
        "//c2\n"
        "} endclass",
        ClassDeclaration(
            0, L(0, {"class", "Foo", ";"}),
            ConstraintDeclaration(
                1, L(1, {"constraint", "empty_c", "{", "//c1"}),  //
                L(2, {"//c2"}),                                   //
                L(1, {"}"})),
            L(0, {"endclass"})),
    },

    {
        "class with multiple constraint declarations",
        "class Foo; constraint empty1_c { } constraint empty2_c {} endclass",
        ClassDeclaration(
            0, L(0, {"class", "Foo", ";"}),
            ClassItemList(1, L(1, {"constraint", "empty1_c", "{", "}"}),
                          L(1, {"constraint", "empty2_c", "{", "}"})),
            L(0, {"endclass"})),
    },

    {
        "class with constraints",
        "class Foo; constraint bar_c { unique {baz}; } endclass",
        ClassDeclaration(
            0, L(0, {"class", "Foo", ";"}),
            ConstraintDeclaration(1, L(1, {"constraint", "bar_c", "{"}),
                                  L(2, {"unique", "{", "baz", "}", ";"}),
                                  L(1, {"}"})),
            L(0, {"endclass"})),
    },

    {
        "class with constraints, multiple constraint expressions",
        "class Foo; constraint bar_c { soft z == y; unique {baz}; } endclass",
        ClassDeclaration(
            0, L(0, {"class", "Foo", ";"}),
            ConstraintDeclaration(
                1, L(1, {"constraint", "bar_c", "{"}),
                ConstraintItemList(2, L(2, {"soft", "z", "==", "y", ";"}),
                                   L(2, {"unique", "{", "baz", "}", ";"})),
                L(1, {"}"})),
            L(0, {"endclass"})),
    },

    {
        "class with conditional constraint set, constraint expression list",
        "class Foo; constraint if_c { if (z) { soft x == y; } } endclass",
        ClassDeclaration(
            0, L(0, {"class", "Foo", ";"}),
            ConstraintDeclaration(1, L(1, {"constraint", "if_c", "{"}),
                                  N(2, L(2, {"if", "(", "z", ")", "{"}),  //
                                    L(3, {"soft", "x", "==", "y", ";"}),  //
                                    L(2, {"}"})),
                                  L(1, {"}"})),
            L(0, {"endclass"})),
    },

    {
        "class with conditional constraint set, constraint exprs and comments",
        "class Foo; constraint if_c { if (z) { //comment-w\n"
        "//comment-x\n"
        "soft x == y; //comment-y\n"
        "//comment-z\n"
        "} } endclass",
        ClassDeclaration(
            0, L(0, {"class", "Foo", ";"}),
            ConstraintDeclaration(
                1, L(1, {"constraint", "if_c", "{"}),
                N(2, L(2, {"if", "(", "z", ")", "{", "//comment-w"}),    //
                  N(3, L(3, {"//comment-x"}),                            //
                    L(3, {"soft", "x", "==", "y", ";", "//comment-y"}),  //
                    L(3, {"//comment-z"})),
                  L(2, {"}"})),
                L(1, {"}"})),
            L(0, {"endclass"})),
    },

    {
        "class with nested conditional constraint set",
        "class Foo; constraint if_c { "
        "if (z) { if (p) { soft x == y; }} "
        "} endclass",
        ClassDeclaration(
            0, L(0, {"class", "Foo", ";"}),
            ConstraintDeclaration(
                1, L(1, {"constraint", "if_c", "{"}),
                ConstraintItemList(
                    2, L(2, {"if", "(", "z", ")", "{"}),
                    ConstraintExpressionList(
                        3, L(3, {"if", "(", "p", ")", "{"}),
                        L(4, {"soft", "x", "==", "y", ";"}), L(3, {"}"})),
                    L(2, {"}"})),
                L(1, {"}"})),
            L(0, {"endclass"})),
    },

    {
        "class with foreach constraint sets",
        "class Foo; constraint if_c { "
        "foreach (z) { soft x == y; } "
        "foreach (w) { soft y == z; } "
        "} endclass",
        ClassDeclaration(
            0, L(0, {"class", "Foo", ";"}),
            ConstraintDeclaration(
                1, L(1, {"constraint", "if_c", "{"}),
                ConstraintItemList(
                    2,
                    N(2,  //
                      L(2, {"foreach", "(", "z", ")", "{"}),
                      L(3, {"soft", "x", "==", "y", ";"}), L(2, {"}"})),
                    N(2,  //
                      L(2, {"foreach", "(", "w", ")", "{"}),
                      L(3, {"soft", "y", "==", "z", ";"}), L(2, {"}"}))),
                L(1, {"}"})),
            L(0, {"endclass"})),
    },

    {
        "class with constraints, implication constraint expressions",
        "class Foo; constraint bar_c { "
        " z < y -> { unique {baz}; }"
        " a > b -> { soft p == q; }"
        " } endclass",
        ClassDeclaration(
            0, L(0, {"class", "Foo", ";"}),
            ConstraintDeclaration(
                1, L(1, {"constraint", "bar_c", "{"}),
                ConstraintItemList(
                    2,
                    N(2,  //
                      L(2, {"z", "<", "y", "->", "{"}),
                      L(3, {"unique", "{", "baz", "}", ";"}), L(2, {"}"})),
                    N(2,  //
                      L(2, {"a", ">", "b", "->", "{"}),
                      L(3, {"soft", "p", "==", "q", ";"}), L(2, {"}"}))),
                L(1, {"}"})),
            L(0, {"endclass"})),
    },

    {
        "class with constraints, distribution list",
        "class Foo; constraint bar_c { "
        " timer_enable dist { [0:9] :/ 20, 10 :/ 80 };"
        " } endclass",
        ClassDeclaration(
            0, L(0, {"class", "Foo", ";"}),
            ClassItemList(
                1, L(1, {"constraint", "bar_c", "{"}),
                ConstraintItemList(
                    2, L(2, {"timer_enable", "dist", "{"}),
                    DistItemList(
                        3, L(3, {"[", "0", ":", "9", "]", ":/", "20", ","}),
                        L(3, {"10", ":/", "80"})),
                    L(2, {"}", ";"})),
                L(1, {"}"})),
            L(0, {"endclass"})),
    },

    {
        "class with constraints, distribution list, with comments",
        "class Foo; constraint bar_c { "
        " timer_enable dist { //c1\n"
        "//c2\n"
        "[0:9] :/ 20, //c3\n"
        "//c4\n"
        "10 :/ 80 //c5\n"
        "//c6\n"
        "};"
        " } endclass",
        ClassDeclaration(
            0, L(0, {"class", "Foo", ";"}),
            ClassItemList(1, L(1, {"constraint", "bar_c", "{"}),
                          ConstraintItemList(
                              2, L(2, {"timer_enable", "dist", "{", "//c1"}),
                              DistItemList(3,               //
                                           L(3, {"//c2"}),  //
                                           L(3, {"[", "0", ":", "9", "]", ":/",
                                                 "20", ",", "//c3"}),
                                           L(3, {"//c4"}),  //
                                           L(3, {"10", ":/", "80", "//c5"}),
                                           L(3, {"//c6"})),
                              L(2, {"}", ";"})),
                          L(1, {"}"})),
            L(0, {"endclass"})),
    },

    {
        "class with empty parameter list",
        "class Foo #(); endclass",
        ClassDeclaration(0, L(0, {"class", "Foo", "#", "(", ")", ";"}),
                         L(0, {"endclass"})),
    },

    {
        "class with one parameter list",
        "class Foo #(type a = b); endclass",
        ClassDeclaration(
            0,
            ClassHeader(0, L(0, {"class", "Foo", "#", "("}),
                        L(2, {"type", "a", "=", "b"}), L(0, {")", ";"})),
            L(0, {"endclass"})),
    },

    {
        "class with multiple parameter list",
        "class Foo #(type a = b, type c = d, type e = f); endclass",
        ClassDeclaration(0,
                         ClassHeader(0, L(0, {"class", "Foo", "#", "("}),
                                     ClassParameterList(
                                         2, L(2, {"type", "a", "=", "b", ","}),
                                         L(2, {"type", "c", "=", "d", ","}),
                                         L(2, {"type", "e", "=", "f"})),
                                     L(0, {")", ";"})),
                         L(0, {"endclass"})),
    },
};

// Test that TreeUnwrapper produces the correct UnwrappedLines from class tests
TEST_F(TreeUnwrapperTest, UnwrapClassTests) {
  for (const auto &test_case : kClassTestCases) {
    VLOG(1) << "Test: " << test_case.test_name;
    auto tree_unwrapper = CreateTreeUnwrapper(test_case.source_code);
    const auto *uwline_tree = tree_unwrapper->Unwrap();
    EXPECT_TRUE(VerifyUnwrappedLines(&std::cout, *ABSL_DIE_IF_NULL(uwline_tree),
                                     test_case));
  }
}

const TreeUnwrapperTestData kUnwrapPackageTestCases[] = {
    {
        "empty package",
        "package foo_pkg;"
        "endpackage",
        PackageDeclaration(0, L(0, {"package", "foo_pkg", ";"}),
                           L(0, {"endpackage"})),
    },

    {
        "empty packages with end-labels",
        "package foo_pkg;"
        "endpackage : foo_pkg "
        "package bar_pkg;"
        "endpackage : bar_pkg",
        PackageDeclaration(0, L(0, {"package", "foo_pkg", ";"}),
                           L(0, {"endpackage", ":", "foo_pkg"})),
        PackageDeclaration(0, L(0, {"package", "bar_pkg", ";"}),
                           L(0, {"endpackage", ":", "bar_pkg"})),
    },

    {
        "in package, implicit-type data declaration, singleton",
        "package p ;a;endpackage",
        PackageDeclaration(0, L(0, {"package", "p", ";"}),
                           L(1, {"a", ";"}),  // implicit type
                           L(0, {"endpackage"})),
    },
    {
        "in package, two implicit-type data declaration",
        "package p;a;b;endpackage",
        PackageDeclaration(0, L(0, {"package", "p", ";"}),
                           PackageItemList(1,                 //
                                           L(1, {"a", ";"}),  //
                                           L(1, {"b", ";"})),
                           L(0, {"endpackage"})),
    },
    {
        "in package, implicit-type data declaration, two variables",
        "package p;a,b;\tendpackage",
        PackageDeclaration(0, L(0, {"package", "p", ";"}),
                           DataDeclaration(1,                 //
                                           L(1, {"a", ","}),  //
                                           L(1, {"b", ";"})),
                           L(0, {"endpackage"})),
    },

    {
        "package with one parameter declaration",
        "package foo_pkg;"
        "parameter size=4;"
        "endpackage",
        PackageDeclaration(0, L(0, {"package", "foo_pkg", ";"}),
                           L(1, {"parameter", "size", "=", "4", ";"}),
                           L(0, {"endpackage"})),
    },

    {
        "package with one localparam declaration",
        "package foo_pkg;"
        "localparam size=2;"
        "endpackage",
        PackageDeclaration(0, L(0, {"package", "foo_pkg", ";"}),
                           L(1, {"localparam", "size", "=", "2", ";"}),
                           L(0, {"endpackage"})),
    },

    {
        "package with two type declarations",
        "package foo_pkg;"
        "typedef enum {X=0,Y=1} bar_t;"
        "typedef enum {A=0,B=1} foo_t;"
        "endpackage",
        PackageDeclaration(
            0, L(0, {"package", "foo_pkg", ";"}),
            PackageItemList(1,
                            N(1,  //
                              L(1, {"typedef", "enum", "{"}),
                              EnumItemList(2, L(2, {"X", "=", "0", ","}),
                                           L(2, {"Y", "=", "1"})),
                              L(1, {"}", "bar_t", ";"})),  //
                            N(1,                           //
                              L(1, {"typedef", "enum", "{"}),
                              EnumItemList(2, L(2, {"A", "=", "0", ","}),
                                           L(2, {"B", "=", "1"})),
                              L(1, {"}", "foo_t", ";"}))),
            L(0, {"endpackage"})),
    },
    {
        "package with typedef declaration on type with named parameters",
        "package foo_pkg;"
        "typedef goo_pkg::baz_t #(.X(X),.Y(Y)) bar_t;"
        "endpackage",
        PackageDeclaration(
            0,  //
            L(0, {"package", "foo_pkg", ";"}),
            N(1,  //
              L(1, {"typedef", "goo_pkg", "::", "baz_t", "#", "("}),
              N(3,                                     //
                L(3, {".", "X", "(", "X", ")", ","}),  //
                L(3, {".", "Y", "(", "Y", ")"})),
              L(1, {")", "bar_t", ";"})),  //
            L(0, {"endpackage"})),
    },
    {
        "package with net_type_declarations",
        "package foo_pkg;"
        "nettype shortreal foo;"
        "nettype bar[1:0] baz with quux;"
        "endpackage",
        PackageDeclaration(
            0, L(0, {"package", "foo_pkg", ";"}),
            PackageItemList(1, L(1, {"nettype", "shortreal", "foo", ";"}),
                            L(1, {"nettype", "bar", "[", "1", ":", "0", "]",
                                  "baz", "with", "quux", ";"})),
            L(0, {"endpackage"})),
    },

    {
        "package with function declaration, commented",
        "package foo_pkg; \n"
        "// function description\n"
        "function automatic void bar();"
        "endfunction "
        " endpackage\n",
        PackageDeclaration(
            0, L(0, {"package", "foo_pkg", ";"}),
            PackageItemList(
                1, L(1, {"// function description"}),
                FunctionDeclaration(1,
                                    L(1, {"function", "automatic", "void",
                                          "bar", "(", ")", ";"}),
                                    L(1, {"endfunction"}))),
            L(0, {"endpackage"})),
    },

    {
        "package with class declaration, commented",
        "package foo_pkg; \n"
        "// class description\n"
        "class classy;"
        "endclass "
        " endpackage\n",
        PackageDeclaration(
            0, L(0, {"package", "foo_pkg", ";"}),
            PackageItemList(1, L(1, {"// class description"}),
                            ClassDeclaration(1, L(1, {"class", "classy", ";"}),
                                             L(1, {"endclass"}))),
            L(0, {"endpackage"})),
    },

    {
        "package with class and function declaration, commented",
        "package foo_pkg; \n"
        "// class description\n"
        "class classy;\n"
        "// function description\n"
        "function automatic void bar();"
        "endfunction "
        "endclass "
        " endpackage\n",
        PackageDeclaration(
            0, L(0, {"package", "foo_pkg", ";"}),
            PackageItemList(
                1, L(1, {"// class description"}),
                ClassDeclaration(
                    1, L(1, {"class", "classy", ";"}),
                    ClassItemList(2, L(2, {"// function description"}),
                                  FunctionDeclaration(
                                      2,
                                      L(2, {"function", "automatic", "void",
                                            "bar", "(", ")", ";"}),
                                      L(2, {"endfunction"}))),
                    L(1, {"endclass"}))),
            L(0, {"endpackage"})),
    },
};

// Test that TreeUnwrapper produces correct UnwrappedLines from package tests
TEST_F(TreeUnwrapperTest, UnwrapPackageTests) {
  for (const auto &test_case : kUnwrapPackageTestCases) {
    auto tree_unwrapper = CreateTreeUnwrapper(test_case.source_code);
    const auto *uwline_tree = tree_unwrapper->Unwrap();
    EXPECT_TRUE(VerifyUnwrappedLines(&std::cout, *ABSL_DIE_IF_NULL(uwline_tree),
                                     test_case));
  }
}

const TreeUnwrapperTestData kDescriptionTestCases[] = {
    {
        "implicit-type data declaration, singleton",
        "a;",
        L(0, {"a", ";"}),
    },
    {
        "implicit-type data declaration, singleton, with var keyword",
        "\t var a;",
        L(0, {"var", "a", ";"}),
    },
    {
        "two implicit-type data declaration",
        "a;b;",
        L(0, {"a", ";"}),
        L(0, {"b", ";"}),
    },
    {
        "implicit-type data declaration, two variables",
        "a,b;",
        DataDeclaration(0,                 //
                        L(0, {"a", ","}),  //
                        L(0, {"b", ";"})),
    },
    {
        "implicit-type data declaration, two variables, with var keyword",
        "var a,b;",
        DataDeclaration(0,                   //
                        L(0, {"var"}),       //
                        N(2,                 //
                          L(2, {"a", ","}),  //
                          L(2, {"b", ";"}))),
    },
    {
        "one bind declaration",
        "bind foo bar#(.x(y)) baz(.clk(clk));",
        N(0,  // kBindDeclaration
          L(0, {"bind", "foo", "bar", "#", "("}),
          L(2, {".", "x", "(", "y", ")"}),
          N(0, L(0, {")", "baz", "("}),           //
            L(2, {".", "clk", "(", "clk", ")"}),  //
            L(0, {")", ";"}))  // ';' is attached to end of bind directive
          ),
    },

    {"multiple bind declarations",
     "bind foo bar baz();"
     "bind goo car caz();",
     L(0, {"bind", "foo", "bar", "baz", "(", ")", ";"}),
     L(0, {"bind", "goo", "car", "caz", "(", ")", ";"})},

    {
        "multi-instance bind declaration",
        "bind foo bar baz1(), baz2();",
        N(0,  // kBindDeclaration
          L(0, {"bind", "foo", "bar", "baz1", "(", ")", ","}),  //
          L(0, {"baz2", "(", ")",
                ";"})  // TODO(fangism): what should be this indentation?
          ),
    },
};

// Test that TreeUnwrapper produces correct UnwrappedLines from package tests
TEST_F(TreeUnwrapperTest, DescriptionTests) {
  for (const auto &test_case : kDescriptionTestCases) {
    auto tree_unwrapper = CreateTreeUnwrapper(test_case.source_code);
    const auto *uwline_tree = tree_unwrapper->Unwrap();
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
        N(0, L(0, {"`FOO", "("}), L(2, {"baz", ")"})),
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
        PackageDeclaration(
            0, L(0, {"package", "includer", ";"}),
            PackageItemList(1, L(1, {"`include", "\"header1.vh\""}),
                            L(1, {"`include", "\"path/header2.svh\""})),
            L(0, {"endpackage", ":", "includer"})),
    },

    {
        "`defines's inside package should be indented as package items",
        "package definer;\n"
        "`define BAR\n"
        "`undef BAR\n"
        "endpackage : definer\n",
        PackageDeclaration(0, L(0, {"package", "definer", ";"}),
                           PackageItemList(1, L(1, {"`define", "BAR", ""}),
                                           L(1, {"`undef", "BAR"})),
                           L(0, {"endpackage", ":", "definer"})),
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
        PackageDeclaration(
            0, L(0, {"package", "ifdeffer", ";"}),
            PackageItemList(
                1, L(1, {"parameter", "three", "=", "3", ";"}),
                L(0, {"`ifdef", "FOUR"}),
                L(1, {"parameter", "size", "=", "4", ";"}),
                L(0, {"`elsif", "FIVE"}),
                L(1, {"parameter", "size", "=", "5", ";"}), L(0, {"`else"}),
                L(1, {"parameter", "size", "=", "6", ";"}), L(0, {"`endif"}),
                L(1, {"parameter", "foo", "=", "7", ";"})),
            L(0, {"endpackage", ":", "ifdeffer"})),
    },

    {
        "new partition after `else",
        "`ifdef FOO\n"
        "`fine\n"
        "`else\n"
        "`error\n"
        "`endif\n",
        L(0, {"`ifdef", "FOO"}),
        L(0, {"`fine"}),
        L(0, {"`else"}),
        L(0, {"`error"}),
        L(0, {"`endif"}),
    },

    {
        "new partition after `else with EOL comment",
        "`ifdef FOO\n"
        "`fine\n"
        "`else  // not good\n"
        "`error\n"
        "`endif\n",
        L(0, {"`ifdef", "FOO"}),
        L(0, {"`fine"}),
        L(0, {"`else", "// not good"}),
        L(0, {"`error"}),
        L(0, {"`endif"}),
    },

    {
        "new partition after `else with block comment",
        "`ifdef FOO\n"
        "`fine\n"
        "`else  /* not good */\n"
        "`error\n"
        "`endif\n",
        L(0, {"`ifdef", "FOO"}),
        L(0, {"`fine"}),
        L(0, {"`else", "/* not good */"}),
        L(0, {"`error"}),
        L(0, {"`endif"}),
    },

    {
        "lone macro call, no semicolon",
        "`FOO()\n",
        L(0, {"`FOO", "(", ")"}),
    },

    {
        "lone macro call, with semicolon",
        "`FOO();\n",
        L(0, {"`FOO", "(", ")", ";"}),
    },

    {
        "lone macro call, with space before semicolon",
        "`FOO() ;\n",
        L(0, {"`FOO", "(", ")", ";"}),
    },

    {
        "macro call with one argument and with semicolon",
        "`FOO(arg);\n",
        N(0, L(0, {"`FOO", "("}), L(2, {"arg", ")", ";"})),
    },

    {
        "macro call with one argument and with space before semicolon",
        "`FOO(arg) ;\n",
        N(0, L(0, {"`FOO", "("}), L(2, {"arg", ")", ";"})),
    },

    {
        "macro call with comments in argument list",
        "`FOO(aa, //aa\nbb , // bb\ncc)\n",
        N(0, L(0, {"`FOO", "("}),
          N(2,                           //
            L(2, {"aa", ",", "//aa"}),   //
            L(2, {"bb", ",", "// bb"}),  //
            L(2, {"cc", ")"}))),
    },

    {
        "macro call with comment before first argument",
        "`FOO(// aa\naa, // bb\nbb, // cc\ncc)\n",
        N(0, L(0, {"`FOO", "(", "// aa"}),
          N(2,                           //
            L(2, {"aa", ",", "// bb"}),  //
            L(2, {"bb", ",", "// cc"}),  //
            L(2, {"cc", ")"}))),
    },

    {
        "macro call with argument including comment",
        "`FOO(aa, bb,\n// cc\ndd)\n",
        N(0, L(0, {"`FOO", "("}),
          N(2,                  //
            L(2, {"aa", ","}),  //
            L(2, {"bb", ","}),  //
            L(2, {"// cc"}),    // indented to same level as surrounding args
            L(2, {"dd", ")"}))),
    },

    {
        "macro call with argument including trailing EOL comment",
        "`FOO(aa, bb, // cc\ndd)\n",
        N(0, L(0, {"`FOO", "("}),
          N(2,                             //
            L(2, {"aa", ","}),             //
            L(2, {"bb", ",", {"// cc"}}),  //
            L(2, {"dd", ")"}))),
    },

    {
        "lone macro item",
        "`FOO\n",
        L(0, {"`FOO"}),
    },

    {
        "two macro items",
        "`FOO\n"
        "`BAR\n",
        L(0, {"`FOO"}),
        L(0, {"`BAR"}),
    },

    {
        "top-level assert macro with property_spec inside argument list",
        "`ASSERT("
        "    MioWarl_A,"
        "    padctrl.reg2hw.mio_pads[mio_sel].qe |=>"
        "        !(|padctrl.mio_attr_q[mio_sel][padctrl_reg_pkg::AttrDw-1:2]),"
        "    clk_i, !rst_ni)\n",
        N(0,  //
          L(0, {"`ASSERT", "("}),
          N(2,  //
            L(2, {"MioWarl_A", ","}), L(2, {"padctrl",    ".",
                                            "reg2hw",     ".",
                                            "mio_pads",   "[",
                                            "mio_sel",    "]",
                                            ".",          "qe",
                                            "|=>",        "!",
                                            "(",          "|",
                                            "padctrl",    ".",
                                            "mio_attr_q", "[",
                                            "mio_sel",    "]",
                                            "[",          "padctrl_reg_pkg",
                                            "::",         "AttrDw",
                                            "-",          "1",
                                            ":",          "2",
                                            "]",          ")",
                                            ","}),
            L(2, {"clk_i", ","}), L(2, {"!", "rst_ni", ")"}))),
    },

    {
        "assert macro embedded in module with property_spec inside arugment "
        "list",
        "module foo;"
        "  `ASSERT("
        "      MioWarl_A,"
        "      padctrl.reg2hw.mio_pads[mio_sel].qe |=>"
        "          "
        "!(|padctrl.mio_attr_q[mio_sel][padctrl_reg_pkg::AttrDw-1:2]),"
        "      clk_i, !rst_ni)\n"
        "endmodule\n",
        ModuleDeclaration(  //
            0,              //
            L(0, {"module", "foo", ";"}),
            N(1,  //
              L(1, {"`ASSERT", "("}),
              N(3,                         //
                L(3, {"MioWarl_A", ","}),  //
                L(3, {"padctrl",    ".",
                      "reg2hw",     ".",
                      "mio_pads",   "[",
                      "mio_sel",    "]",
                      ".",          "qe",
                      "|=>",        "!",
                      "(",          "|",
                      "padctrl",    ".",
                      "mio_attr_q", "[",
                      "mio_sel",    "]",
                      "[",          "padctrl_reg_pkg",
                      "::",         "AttrDw",
                      "-",          "1",
                      ":",          "2",
                      "]",          ")",
                      ","}),
                L(3, {"clk_i", ","}), L(3, {"!", "rst_ni", ")"}))),
            L(0, {"endmodule"})),
    },

    {
        "assert macro embedded in initial block with property_spec inside "
        "argument list",
        "module foo;"
        "  initial begin"
        "    `ASSERT("
        "        MioWarl_A,"
        "        padctrl.reg2hw.mio_pads[mio_sel].qe |=>"
        "            "
        "!(|padctrl.mio_attr_q[mio_sel][padctrl_reg_pkg::AttrDw-1:2]),"
        "        clk_i, !rst_ni)\n"
        "  end\n"
        "endmodule\n",
        ModuleDeclaration(  //
            0,              //
            L(0, {"module", "foo", ";"}),
            N(1,  //
              L(1, {"initial", "begin"}),
              N(2,  //
                L(2, {"`ASSERT", "("}),
                N(4,  //
                  L(4, {"MioWarl_A", ","}),
                  L(4, {"padctrl",    ".",
                        "reg2hw",     ".",
                        "mio_pads",   "[",
                        "mio_sel",    "]",
                        ".",          "qe",
                        "|=>",        "!",
                        "(",          "|",
                        "padctrl",    ".",
                        "mio_attr_q", "[",
                        "mio_sel",    "]",
                        "[",          "padctrl_reg_pkg",
                        "::",         "AttrDw",
                        "-",          "1",
                        ":",          "2",
                        "]",          ")",
                        ","}),
                  L(4, {"clk_i", ","}), L(4, {"!", "rst_ni", ")"}))),
              L(1, {"end"})),
            L(0, {"endmodule"})),
    },

    {
        "`include's inside module should be flushed left",
        "module includer;\n"
        "`include \"header1.vh\"\n"
        "`include \"path/header2.svh\"\n"
        "endmodule : includer\n",
        ModuleDeclaration(
            0, L(0, {"module", "includer", ";"}),
            ModuleItemList(1, L(1, {"`include", "\"header1.vh\""}),
                           L(1, {"`include", "\"path/header2.svh\""})),
            L(0, {"endmodule", ":", "includer"})),
    },

    {
        "`defines's inside module should be flushed left",
        "module definer;\n"
        "`define BAR\n"
        "`undef BAR\n"
        "endmodule : definer\n",
        ModuleDeclaration(0, L(0, {"module", "definer", ";"}),
                          ModuleItemList(1, L(1, {"`define", "BAR", ""}),
                                         L(1, {"`undef", "BAR"})),
                          L(0, {"endmodule", ":", "definer"})),
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
        ModuleDeclaration(
            0, L(0, {"module", "foo", ";"}),
            ModuleItemList(1, L(1, {"always_comb", "begin"}),
                           StatementList(2,                           //
                                         L(2, {"x", "=", "y", ";"}),  //
                                         L(0, {"`ifdef", "FOO"}),     //
                                         L(2, {"z", "=", "0", ";"}),  //
                                         L(0, {"`endif"}),            //
                                         L(2, {"w", "=", "z", ";"})),
                           L(1, {"end"})),
            L(0, {"endmodule"})),
    },

    {
        "new partition after `else in module",
        "module foo;\n"
        "always_comb begin\n"
        "  x = y;\n"
        "`ifdef FOO\n"
        "  z = 0;\n"
        "`else\n"
        "  x = z;\n"
        "`endif\n"
        "  w = z;\n"
        "end\n"
        "endmodule",
        ModuleDeclaration(
            0, L(0, {"module", "foo", ";"}),
            ModuleItemList(1, L(1, {"always_comb", "begin"}),
                           StatementList(2,                           //
                                         L(2, {"x", "=", "y", ";"}),  //
                                         L(0, {"`ifdef", "FOO"}),     //
                                         L(2, {"z", "=", "0", ";"}),  //
                                         L(0, {"`else"}),             //
                                         L(2, {"x", "=", "z", ";"}),  //
                                         L(0, {"`endif"}),            //
                                         L(2, {"w", "=", "z", ";"})),
                           L(1, {"end"})),
            L(0, {"endmodule"})),
    },

    {
        "new partition after `else with EOL comment in module",
        "module foo;\n"
        "always_comb begin\n"
        "  x = y;\n"
        "`ifdef FOO\n"
        "  z = 0;\n"
        "`else  // FOO not defined\n"
        "  x = z;\n"
        "`endif\n"
        "  w = z;\n"
        "end\n"
        "endmodule",
        ModuleDeclaration(
            0, L(0, {"module", "foo", ";"}),
            ModuleItemList(
                1, L(1, {"always_comb", "begin"}),
                StatementList(2,                                      //
                              L(2, {"x", "=", "y", ";"}),             //
                              L(0, {"`ifdef", "FOO"}),                //
                              L(2, {"z", "=", "0", ";"}),             //
                              L(0, {"`else", "// FOO not defined"}),  //
                              L(2, {"x", "=", "z", ";"}),             //
                              L(0, {"`endif"}),                       //
                              L(2, {"w", "=", "z", ";"})),
                L(1, {"end"})),
            L(0, {"endmodule"})),
    },

    {
        "new partition after `else with block comment in module",
        "module foo;\n"
        "always_comb begin\n"
        "  x = y;\n"
        "`ifdef FOO\n"
        "  z = 0;\n"
        "`else  /* z is available */\n"
        "  x = z;\n"
        "`endif\n"
        "  w = z;\n"
        "end\n"
        "endmodule",
        ModuleDeclaration(
            0, L(0, {"module", "foo", ";"}),
            ModuleItemList(
                1, L(1, {"always_comb", "begin"}),
                StatementList(2,                                        //
                              L(2, {"x", "=", "y", ";"}),               //
                              L(0, {"`ifdef", "FOO"}),                  //
                              L(2, {"z", "=", "0", ";"}),               //
                              L(0, {"`else", "/* z is available */"}),  //
                              L(2, {"x", "=", "z", ";"}),               //
                              L(0, {"`endif"}),                         //
                              L(2, {"w", "=", "z", ";"})),              //
                L(1, {"end"})),
            L(0, {"endmodule"})),
    },

    {
        "`ifdefs's inside module should flush left, even with leading comment",
        "module foo;\n"
        "// comment\n"
        "`ifdef SIM\n"
        "  wire w;\n"
        "`endif\n"
        "endmodule",
        ModuleDeclaration(
            0, L(0, {"module", "foo", ";"}),
            ModuleItemList(1, L(1, {"// comment"}), L(0, {"`ifdef", "SIM"}),
                           L(1, {"wire", "w", ";"}), L(0, {"`endif"})),
            L(0, {"endmodule"})),
    },

    {
        "`ifndefs's inside module should flush left, even with leading comment",
        "module foo;\n"
        "// comment\n"
        "`ifndef SIM\n"
        "`endif\n"
        "  wire w;\n"
        "endmodule",
        ModuleDeclaration(
            0, L(0, {"module", "foo", ";"}),
            ModuleItemList(1, L(1, {"// comment"}), L(0, {"`ifndef", "SIM"}),
                           L(0, {"`endif"}), L(1, {"wire", "w", ";"})),
            L(0, {"endmodule"})),
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
        ModuleDeclaration(
            0, L(0, {"module", "foo", ";"}),
            ModuleItemList(1, L(1, {"// comment1"}), L(0, {"`ifdef", "SIM"}),
                           L(1, {"// comment2"}), L(0, {"`elsif", "SYN"}),
                           L(1, {"// comment3"}), L(0, {"`else"}),
                           L(1, {"// comment4"}), L(0, {"`endif"}),
                           L(1, {"// comment5"})),
            L(0, {"endmodule"})),
    },

    {
        "Partitioning formal argument `define",
        "`define FOO(BAR)\n",
        N(0, L(0, {"`define", "FOO", "("}), L(2, {"BAR", ")", ""})),
    },

    {
        "Partitioning formal argument `define with body definition",
        "`define FOO(BAR) body_def\n",
        N(0, L(0, {"`define", "FOO", "("}), L(2, {"BAR", ")", "body_def"})),
    },

    {
        "Partitioning formal arguments in `define",
        "`define FOO(BAR1, BAR2, BAR3)\n",
        N(0, L(0, {"`define", "FOO", "("}),
          N(2, L(2, {"BAR1", ","}), L(2, {"BAR2", ","}),
            L(2, {"BAR3", ")", ""}))),
    },

    {
        "Partitioning formal arguments in `define with body definition",
        "`define FOO(BAR1, BAR2, BAR3) definition_body\n",
        N(0, L(0, {"`define", "FOO", "("}),
          N(2, L(2, {"BAR1", ","}), L(2, {"BAR2", ","}),
            L(2, {"BAR3", ")", "definition_body"}))),
    },

    {
        "Partitioning formal arguments in consecutive `define's",
        "`define FOO1(BAR1, BAR2, BAR3)\n"
        "`define FOO2(BAR1, BAR2, BAR3)\n",
        N(0, L(0, {"`define", "FOO1", "("}),
          N(2, L(2, {"BAR1", ","}), L(2, {"BAR2", ","}),
            L(2, {"BAR3", ")", ""}))),
        N(0, L(0, {"`define", "FOO2", "("}),
          N(2, L(2, {"BAR1", ","}), L(2, {"BAR2", ","}),
            L(2, {"BAR3", ")", ""}))),
    },

    {
        "Partitioning formal arguments in consecutive `define's with body def",
        "`define FOO1(BAR1, BAR2, BAR3) definition_body\n"
        "`define FOO2(BAR1, BAR2, BAR3)\n",
        N(0, L(0, {"`define", "FOO1", "("}),
          N(2, L(2, {"BAR1", ","}), L(2, {"BAR2", ","}),
            L(2, {"BAR3", ")", "definition_body"}))),
        N(0, L(0, {"`define", "FOO2", "("}),
          N(2, L(2, {"BAR1", ","}), L(2, {"BAR2", ","}),
            L(2, {"BAR3", ")", ""}))),
    },

    {
        "Partitioning formal arguments in consecutive `define's with body def",
        "`define FOO1(BAR1, BAR2, BAR3)\n"
        "`define FOO2(BAR1, BAR2, BAR3) definition_body\n",
        N(0, L(0, {"`define", "FOO1", "("}),
          N(2, L(2, {"BAR1", ","}), L(2, {"BAR2", ","}),
            L(2, {"BAR3", ")", ""}))),
        N(0, L(0, {"`define", "FOO2", "("}),
          N(2, L(2, {"BAR1", ","}), L(2, {"BAR2", ","}),
            L(2, {"BAR3", ")", "definition_body"}))),
    },

    {
        "Partitioning formal arguments in consecutive `define's with body def",
        "`define FOO1(BAR1, BAR2, BAR3) definition_body1\n"
        "`define FOO2(BAR1, BAR2, BAR3) definition_body2\n",
        N(0, L(0, {"`define", "FOO1", "("}),
          N(2, L(2, {"BAR1", ","}), L(2, {"BAR2", ","}),
            L(2, {"BAR3", ")", "definition_body1"}))),
        N(0, L(0, {"`define", "FOO2", "("}),
          N(2, L(2, {"BAR1", ","}), L(2, {"BAR2", ","}),
            L(2, {"BAR3", ")", "definition_body2"}))),
    },

    {
        "Partitioning formal argument with default value in `define",
        "`define FOO(BAR1=default_val)\n",
        N(0, L(0, {"`define", "FOO", "("}),
          L(2, {"BAR1", "=", "default_val", ")", ""})),
    },

    {
        "Partitioning formal arguments with default value in `define with "
        "body definition",
        "`define FOO(BAR1, BAR2=default_val) definition_body\n",
        N(0, L(0, {"`define", "FOO", "("}),
          N(2, L(2, {"BAR1", ","}),
            L(2, {"BAR2", "=", "default_val", ")", "definition_body"}))),
    },

    // TODO(fangism): decide/test/support indenting preprocessor directives
    // nested inside `ifdefs.  Should `define inside `ifdef be indented?
};

// Test for correct UnwrappedLines for preprocessor directives.
TEST_F(TreeUnwrapperTest, UnwrapPreprocessorTests) {
  for (const auto &test_case : kUnwrapPreprocessorTestCases) {
    auto tree_unwrapper = CreateTreeUnwrapper(test_case.source_code);
    const auto *uwline_tree = tree_unwrapper->Unwrap();
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
        InterfaceDeclaration(0, L(0, {"interface", "foo_if", ";"}),
                             L(0, {"endinterface"})),
    },

    {
        "empty interface, empty params",
        "interface foo_if#( );"
        "endinterface",
        InterfaceDeclaration(0,
                             L(0, {"interface", "foo_if", "#", "(", ")", ";"}),
                             L(0, {"endinterface"})),
    },

    {
        "empty interface, empty params, with comment",
        "interface foo_if#(\n"
        "//comment\n"
        ");"
        "endinterface",
        InterfaceDeclaration(0,                                          //
                             N(0,                                        //
                               L(0, {"interface", "foo_if", "#", "("}),  //
                               L(2, {"//comment"}), L(0, {")", ";"})),
                             L(0, {"endinterface"})),
    },

    {
        "empty interface, empty ports",
        "interface foo_if( );"
        "endinterface",
        InterfaceDeclaration(0, L(0, {"interface", "foo_if", "(", ")", ";"}),
                             L(0, {"endinterface"})),
    },

    {
        "empty interface, empty params with comment, empty ports",
        "interface foo_if#(\n"
        "//comment\n"
        ")( );"
        "endinterface",
        InterfaceDeclaration(0,                                          //
                             N(0,                                        //
                               L(0, {"interface", "foo_if", "#", "("}),  //
                               L(2, {"//comment"}), L(0, {")", "(", ")", ";"})),
                             L(0, {"endinterface"})),
    },

    {
        "empty interface, one param type, empty ports",
        "interface foo_if#(\n"
        "parameter type T = bit\n"
        ")( );"
        "endinterface",
        InterfaceDeclaration(0,                                          //
                             N(0,                                        //
                               L(0, {"interface", "foo_if", "#", "("}),  //
                               L(2, {"parameter", "type", "T", "=", "bit"}),
                               L(0, {")", "(", ")", ";"})),
                             L(0, {"endinterface"})),
    },

    {
        "empty interfaces with end-labels",
        "interface foo_if;"
        "endinterface : foo_if "
        "interface bar_if;"
        "endinterface : bar_if",
        InterfaceDeclaration(0, L(0, {"interface", "foo_if", ";"}),
                             L(0, {"endinterface", ":", "foo_if"})),
        InterfaceDeclaration(0, L(0, {"interface", "bar_if", ";"}),
                             L(0, {"endinterface", ":", "bar_if"})),
    },

    {
        "interface with one parameter declaration",
        "interface foo_if;"
        "parameter size=4;"
        "endinterface",
        InterfaceDeclaration(0, L(0, {"interface", "foo_if", ";"}),
                             L(1, {"parameter", "size", "=", "4", ";"}),
                             L(0, {"endinterface"})),
    },

    {
        "interface with one localparam declaration",
        "interface foo_if;"
        "localparam size=2;"
        "endinterface",
        InterfaceDeclaration(0, L(0, {"interface", "foo_if", ";"}),
                             L(1, {"localparam", "size", "=", "2", ";"}),
                             L(0, {"endinterface"})),
    },

    // modport declarations
    {
        "interface with modport declarations",
        "interface foo_if;"
        "modport mp1 (output a, input b);"
        "modport mp2 (output c, input d);"
        "endinterface",
        InterfaceDeclaration(
            0, L(0, {"interface", "foo_if", ";"}),
            ModuleItemList(
                1,  //
                N(1, L(1, {"modport", "mp1", "("}), L(3, {"output", "a", ","}),
                  L(3, {"input", "b"}),  //
                  L(1, {")", ";"})),
                N(1, L(1, {"modport", "mp2", "("}), L(3, {"output", "c", ","}),
                  L(3, {"input", "d"}),  //
                  L(1, {")", ";"}))),
            L(0, {"endinterface"})),
    },
    {
        "interface with modport TF ports",
        "interface foo_if;"
        "modport mp1 (output a, input b, import c);"
        "endinterface",
        InterfaceDeclaration(0, L(0, {"interface", "foo_if", ";"}),
                             N(1, L(1, {"modport", "mp1", "("}),  //
                               L(3, {"output", "a", ","}),        //
                               L(3, {"input", "b", ","}),         //
                               L(3, {"import", "c"}),             //
                               L(1, {")", ";"})),
                             L(0, {"endinterface"})),
    },
    {
        "interface with more modport ports",
        "interface foo_if;"
        "modport mp1 (output a1, a2, input b1, b2, import c1, c2);"
        "endinterface",
        InterfaceDeclaration(0, L(0, {"interface", "foo_if", ";"}),
                             N(1, L(1, {"modport", "mp1", "("}),
                               L(3, {"output", "a1", ",", "a2", ","}),
                               L(3, {"input", "b1", ",", "b2", ","}),
                               L(3, {"import", "c1", ",", "c2"}),  //
                               L(1, {")", ";"})),
                             L(0, {"endinterface"})),
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
        InterfaceDeclaration(
            0, L(0, {"interface", "foo_if", ";"}),
            N(1, L(1, {"modport", "mp1", "("}),
              N(3, L(3, {"// Our output"}), L(3, {"output", "a", ","})),
              N(3, L(3, {"/* Inputs */"}),
                L(3, {"input", "b1", ",", "b_f", "/*last*/", ","})),
              L(3, {"import", "c"}),  //
              L(1, {")", ";"})),
            L(0, {"endinterface"})),
    },
};

// Test that TreeUnwrapper produces correct UnwrappedLines from interface tests
TEST_F(TreeUnwrapperTest, UnwrapInterfaceTests) {
  for (const auto &test_case : kUnwrapInterfaceTestCases) {
    auto tree_unwrapper = CreateTreeUnwrapper(test_case.source_code);
    const auto *uwline_tree = tree_unwrapper->Unwrap();
    EXPECT_TRUE(VerifyUnwrappedLines(&std::cout, *ABSL_DIE_IF_NULL(uwline_tree),
                                     test_case));
  }
}

const TreeUnwrapperTestData kUnwrapTaskTestCases[] = {
    {
        "empty task",
        "task foo;"
        "endtask",
        TaskDeclaration(0, TaskHeader(0, {"task", "foo", ";"}),
                        L(0, {"endtask"})),
    },

    {
        "two empty tasks",
        "task foo;"
        "endtask "
        "task bar;"
        "endtask",
        TaskDeclaration(0, TaskHeader(0, {"task", "foo", ";"}),
                        L(0, {"endtask"})),
        TaskDeclaration(0, TaskHeader(0, {"task", "bar", ";"}),
                        L(0, {"endtask"})),
    },

    {
        "empty task, statement comment",
        "task foo;\n"
        "// statement comment\n"
        "endtask",
        TaskDeclaration(0, TaskHeader(0, {"task", "foo", ";"}),
                        L(1, {"// statement comment"}), L(0, {"endtask"})),
    },

    {
        "empty task, empty ports, statement comment",
        "task foo();\n"
        "// statement comment\n"
        "endtask",
        TaskDeclaration(0, TaskHeader(0, {"task", "foo", "(", ")", ";"}),
                        L(1, {"// statement comment"}), L(0, {"endtask"})),
    },

    {
        "empty task with qualifier",
        "task automatic foo;"
        "endtask",
        TaskDeclaration(0, TaskHeader(0, {"task", "automatic", "foo", ";"}),
                        L(0, {"endtask"})),
    },

    {
        "task with empty formal arguments",
        "task foo();"
        "endtask",
        TaskDeclaration(0, TaskHeader(0, {"task", "foo", "(", ")", ";"}),
                        L(0, {"endtask"})),
    },

    {
        "task with formal argument",
        "task foo(string name);"
        "endtask",
        TaskDeclaration(0,
                        N(0, L(0, {"task", "foo", "("}),
                          L(2, {"string", "name", ")", ";"})),
                        L(0, {"endtask"})),
    },

    {
        "task with multiple formal arguments",
        "task foo(string name, int a);"
        "endtask",
        TaskDeclaration(0,
                        N(0, L(0, {"task", "foo", "("}),
                          TFPortList(2,                              //
                                     L(2, {"string", "name", ","}),  //
                                     L(2, {"int", "a", ")", ";"}))),
                        L(0, {"endtask"})),
    },

    {
        "task with local variable",
        "task foo;"
        "int return_value;"
        "endtask",
        TaskDeclaration(0, TaskHeader(0, {"task", "foo", ";"}),
                        L(1, {"int", "return_value", ";"}), L(0, {"endtask"})),
    },

    {
        "in task, implicit-type data declaration, singleton",
        "task t ;a;endtask",
        TaskDeclaration(0, TaskHeader(0, {"task", "t", ";"}), L(1, {"a", ";"}),
                        L(0, {"endtask"})),
    },
    {
        "in task, two implicit-type data declaration",
        "task t;a;b;endtask",
        TaskDeclaration(0, TaskHeader(0, {"task", "t", ";"}),
                        StatementList(1,                 //
                                      L(1, {"a", ";"}),  //
                                      L(1, {"b", ";"})),
                        L(0, {"endtask"})),
    },

    {
        "task with multiple local variables in single declaration",
        "task foo;"
        "int r1, r2;"
        "endtask",
        TaskDeclaration(0, TaskHeader(0, {"task", "foo", ";"}),
                        DataDeclaration(1, L(1, {"int"}),     //
                                        N(3,                  //
                                          L(3, {"r1", ","}),  //
                                          L(3, {"r2", ";"})   //
                                          )),
                        L(0, {"endtask"})),
    },

    {
        "task with local variable and qualifier",
        "task foo;"
        "static int return_value;"
        "endtask",
        TaskDeclaration(0, TaskHeader(0, {"task", "foo", ";"}),
                        DataDeclaration(1, L(1, {"static"}), L(1, {"int"}),
                                        L(1, {"return_value", ";"})),
                        L(0, {"endtask"})),
    },

    {
        "task with subtask call",
        "task foo;"
        "$makeitso(x);"
        "endtask",
        TaskDeclaration(
            0, TaskHeader(0, {"task", "foo", ";"}),
            N(1, N(1, L(1, {"$makeitso"}), L(1, {"("})), L(3, {"x", ")", ";"})),
            L(0, {"endtask"})),
    },

    {
        "task with assignment to call expression",
        "task foo;"
        "y = makeitso(x);"
        "endtask",
        TaskDeclaration(0, TaskHeader(0, {"task", "foo", ";"}),
                        N(1, L(1, {"y", "=", "makeitso", "("}), L(3, {"x"}),
                          L(1, {")", ";"})),
                        L(0, {"endtask"})),
    },

    {
        "task with system call inside if header",
        "task t;"
        "if (!$cast(ssssssssssssssss, vvvvvvvvvv, gggggggg)) begin "
        "end "
        "endtask : t",
        TaskDeclaration(0, TaskHeader(0, {"task", "t", ";"}),
                        N(1,
                          N(1, L(1, {"if", "(", "!", "$cast", "("}),
                            N(5, L(5, {"ssssssssssssssss", ","}),
                              L(5, {"vvvvvvvvvv", ","}), L(5, {"gggggggg"})),
                            L(3, {")", ")", "begin"})),
                          L(1, {"end"})),
                        L(0, {"endtask", ":", "t"})),
    },

    {
        "task with nested subtask call and arguments passed by name",
        "task t;"
        "if (!$cast(ssssssssssssssss, vvvvvvvvvv.gggggggg("
        ".ppppppp(ppppppp),"
        ".yyyyy(\"xxxxxxxxxxxxx\")"
        "))) begin "
        "end "
        "endtask : t",
        TaskDeclaration(
            0, TaskHeader(0, {"task", "t", ";"}),
            N(1,
              N(1, L(1, {"if", "(", "!", "$cast", "("}),
                N(5, L(5, {"ssssssssssssssss", ","}),
                  L(5, {"vvvvvvvvvv", ".", "gggggggg", "("}),
                  N(7, L(7, {".", "ppppppp", "(", "ppppppp", ")", ","}),
                    L(7, {".", "yyyyy", "(", "\"xxxxxxxxxxxxx\"", ")"})),
                  L(5, {")"})),
                L(3, {")", ")", "begin"})),
              L(1, {"end"})),
            L(0, {"endtask", ":", "t"})),
    },

    {
        "task with delayed assignment",
        "task foo;"
        "#100 "
        "bar = 13;"
        "endtask",
        TaskDeclaration(0, TaskHeader(0, {"task", "foo", ";"}),
                        N(1,  // delayed assignment
                          L(1, {"#", "100"}), L(2, {"bar", "=", "13", ";"})),
                        L(0, {"endtask"})),
    },

    {
        "task with multiple nonblocking assignments",
        "task t; a<=b; c<=d; endtask",
        TaskDeclaration(0, TaskHeader(0, {"task", "t", ";"}),
                        StatementList(1, L(1, {"a", "<=", "b", ";"}),
                                      L(1, {"c", "<=", "d", ";"})),
                        L(0, {"endtask"})),
    },

    {
        "task with arithmetic assignment operators",
        "task t; a=b; c+=d; e-=f; endtask",
        TaskDeclaration(0, TaskHeader(0, {"task", "t", ";"}),
                        StatementList(1, L(1, {"a", "=", "b", ";"}),
                                      L(1, {"c", "+=", "d", ";"}),
                                      L(1, {"e", "-=", "f", ";"})),
                        L(0, {"endtask"})),
    },

    {
        "task with arithmetic assignment operators",
        "task t; a*=b; c=d; e/=f; endtask",
        TaskDeclaration(0, TaskHeader(0, {"task", "t", ";"}),
                        StatementList(1, L(1, {"a", "*=", "b", ";"}),
                                      L(1, {"c", "=", "d", ";"}),
                                      L(1, {"e", "/=", "f", ";"})),
                        L(0, {"endtask"})),
    },

    {
        "task with modulus assignment operator",
        "task t; a%=b; c=d; endtask",
        TaskDeclaration(0, TaskHeader(0, {"task", "t", ";"}),
                        StatementList(1, L(1, {"a", "%=", "b", ";"}),
                                      L(1, {"c", "=", "d", ";"})),
                        L(0, {"endtask"})),
    },

    {
        "task with bitwise assignment operators",
        "task t; a&=b; c|=d; e^=f; endtask",
        TaskDeclaration(0, TaskHeader(0, {"task", "t", ";"}),
                        StatementList(1, L(1, {"a", "&=", "b", ";"}),
                                      L(1, {"c", "|=", "d", ";"}),
                                      L(1, {"e", "^=", "f", ";"})),
                        L(0, {"endtask"})),
    },

    {
        "task with logical shift assignment operators",
        "task t; a<<=b; c>>=d; endtask",
        TaskDeclaration(0, TaskHeader(0, {"task", "t", ";"}),
                        StatementList(1, L(1, {"a", "<<=", "b", ";"}),
                                      L(1, {"c", ">>=", "d", ";"})),
                        L(0, {"endtask"})),
    },

    {
        "task with arithmetic shift assignment operators",
        "task t; a<<<=b; c>>>=d; endtask",
        TaskDeclaration(0, TaskHeader(0, {"task", "t", ";"}),
                        StatementList(1, L(1, {"a", "<<<=", "b", ";"}),
                                      L(1, {"c", ">>>=", "d", ";"})),
                        L(0, {"endtask"})),
    },

    {
        "task with empty fork-join pairs",
        "task forkit;"
        "fork join fork join "
        "endtask",
        TaskDeclaration(
            0, TaskHeader(0, {"task", "forkit", ";"}),
            StatementList(1, ParBlock(1, L(1, {"fork"}), L(1, {"join"})),
                          ParBlock(1, L(1, {"fork"}), L(1, {"join"}))),
            L(0, {"endtask"})),
    },

    {
        "task with empty fork-join pairs, labeled",
        "task forkit;"
        "fork:a join:a fork:b join:b "
        "endtask",
        TaskDeclaration(0, TaskHeader(0, {"task", "forkit", ";"}),
                        StatementList(1,
                                      ParBlock(1, L(1, {"fork", ":", "a"}),
                                               L(1, {"join", ":", "a"})),
                                      ParBlock(1, L(1, {"fork", ":", "b"}),
                                               L(1, {"join", ":", "b"}))),
                        L(0, {"endtask"})),
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
        TaskDeclaration(
            0, TaskHeader(0, {"task", "forkit", ";"}),
            StatementList(1,
                          ParBlock(1, L(1, {"fork"}), L(2, {"// comment1"}),  //
                                   L(1, {"join"})),                           //
                          ParBlock(1, L(1, {"fork"}),                         //
                                   L(2, {"// comment2"}),                     //
                                   L(1, {"join"}))),
            L(0, {"endtask"})),
    },

    {
        "task with fork-join",
        "task foo;"
        "fork "
        "int value;"
        "join "
        "endtask",
        TaskDeclaration(0, TaskHeader(0, {"task", "foo", ";"}),
                        ParBlock(1, L(1, {"fork"}), L(2, {"int", "value", ";"}),
                                 L(1, {"join"})),
                        L(0, {"endtask"})),
    },

    {
        "task with fork-join-disable",
        "task foo;"
        "fork "
        "int value;"
        "join "
        "disable fork;"
        "endtask",
        TaskDeclaration(
            0, TaskHeader(0, {"task", "foo", ";"}),
            StatementList(1,
                          ParBlock(1, L(1, {"fork"}),
                                   L(2, {"int", "value", ";"}), L(1, {"join"})),
                          L(1, {"disable", "fork", ";"})),
            L(0, {"endtask"})),
    },

    {
        "task with fork-join_any-disable",
        "task foo;"
        "fork "
        "join_any "
        "disable fork;"
        "endtask",
        TaskDeclaration(
            0, TaskHeader(0, {"task", "foo", ";"}),
            StatementList(1, ParBlock(1, L(1, {"fork"}), L(1, {"join_any"})),
                          L(1, {"disable", "fork", ";"})),
            L(0, {"endtask"})),
    },

    {
        "task with disable",
        "task foo;"
        "disable other;"
        "endtask",
        TaskDeclaration(0, TaskHeader(0, {"task", "foo", ";"}),
                        L(1, {"disable", "other", ";"}), L(0, {"endtask"})),
    },

    {
        "task with sequential-block inside parallel-block",
        "task fj; fork foo(); begin end join endtask",
        TaskDeclaration(0, TaskHeader(0, {"task", "fj", ";"}),
                        ParBlock(1, L(1, {"fork"}),
                                 StatementList(2,                             //
                                               L(2, {"foo", "(", ")", ";"}),  //
                                               N(2, L(2, {"begin"}),          //
                                                 L(2, {"end"}))),
                                 L(1, {"join"})),
                        L(0, {"endtask"})),
    },

    // TODO(fangism): "task with while loop and single statement"

    {
        "task with while loop and block statement",
        "task foo;"
        "while (1) begin "
        "$makeitso(x);"
        "end "
        "endtask",
        TaskDeclaration(0, TaskHeader(0, {"task", "foo", ";"}),
                        FlowControl(1, L(1, {"while", "(", "1", ")", "begin"}),
                                    N(2, N(2, L(2, {"$makeitso"}), L(2, {"("})),
                                      L(4, {"x", ")", ";"})),
                                    L(1, {"end"})),
                        L(0, {"endtask"})),
    },

    {
        "task with formal parameters declared in-body",
        "task automatic clean_up;"
        "input logic addr;"
        "input logic mask;"
        "endtask",
        TaskDeclaration(0,
                        TaskHeader(0, {"task", "automatic", "clean_up", ";"}),
                        StatementList(1, L(1, {"input", "logic", "addr", ";"}),
                                      L(1, {"input", "logic", "mask", ";"})),
                        L(0, {"endtask"})),
    },

    {
        "task inside class with delayed assignment",
        "class c; task automatic waiter;"
        "#0 z = v; endtask endclass",
        ClassDeclaration(
            0, L(0, {"class", "c", ";"}),
            TaskDeclaration(1, L(1, {"task", "automatic", "waiter", ";"}),
                            N(2,  // delayed assignment
                              L(2, {"#", "0"}), L(3, {"z", "=", "v", ";"})),
                            L(1, {"endtask"})),
            L(0, {"endclass"})),
    },

    {
        "task inside class with labeled assignment",
        "class c; task automatic waiter;"
        "foo: z = v; endtask endclass",
        ClassDeclaration(
            0, L(0, {"class", "c", ";"}),
            TaskDeclaration(1, L(1, {"task", "automatic", "waiter", ";"}),
                            N(2,  // labeled assignment
                              L(2, {"foo", ":"}), L(2, {"z", "=", "v", ";"})),
                            L(1, {"endtask"})),
            L(0, {"endclass"})),
    },

    {
        "task inside class with labeled and delayed assignment",
        "class c; task automatic waiter;"
        "foo: #1 z = v; endtask endclass",
        ClassDeclaration(
            0, L(0, {"class", "c", ";"}),
            TaskDeclaration(
                1, L(1, {"task", "automatic", "waiter", ";"}),
                N(2,                   // labeled and delayed assignment
                  L(2, {"foo", ":"}),  //
                  N(2, L(2, {"#", "1"}), L(3, {"z", "=", "v", ";"}))),
                L(1, {"endtask"})),
            L(0, {"endclass"})),
    },

    {
        "task inside class with procedural timing control statement "
        "and null-statement",
        "class c; task automatic waiter;"
        "#0; return; endtask endclass",
        ClassDeclaration(
            0, L(0, {"class", "c", ";"}),
            TaskDeclaration(
                1, L(1, {"task", "automatic", "waiter", ";"}),
                StatementList(2,                      //
                              L(2, {"#", "0", ";"}),  // timing control
                              L(2, {"return", ";"})),
                L(1, {"endtask"})),
            L(0, {"endclass"})),
    },

    {
        "task with simple event control statement and null-statement",
        "class c; task automatic clocker;"
        "@(posedge clk); endtask endclass",
        ClassDeclaration(
            0, L(0, {"class", "c", ";"}),
            TaskDeclaration(
                1, L(1, {"task", "automatic", "clocker", ";"}),
                L(2, {"@", "(", "posedge", "clk", ")", ";"}),  // event control
                L(1, {"endtask"})),
            L(0, {"endclass"})),
    },

    {
        "task with multiple event control statements",
        "class c; task automatic clocker;"
        "@(posedge clk); @(negedge clk); endtask endclass",
        ClassDeclaration(
            0, L(0, {"class", "c", ";"}),
            TaskDeclaration(
                1, L(1, {"task", "automatic", "clocker", ";"}),
                StatementList(2,  //
                              L(2, {"@", "(", "posedge", "clk", ")", ";"}),
                              L(2, {"@", "(", "negedge", "clk", ")", ";"})),
                L(1, {"endtask"})),
            L(0, {"endclass"})),
    },

    {
        "task with repeated event control statement and null-statement",
        "class c; task automatic clocker;"
        "repeat (2) @(posedge clk); endtask endclass",
        ClassDeclaration(
            0, L(0, {"class", "c", ";"}),
            TaskDeclaration(1, L(1, {"task", "automatic", "clocker", ";"}),
                            FlowControl(2,  //
                                        L(2, {"repeat", "(", "2", ")"}),
                                        L(3, {"@", "(", "posedge", "clk", ")",
                                              ";"})  // event control
                                        ),
                            L(1, {"endtask"})),
            L(0, {"endclass"})),
    },

    {
        "task with multiple repeat event control statements",
        "class c; task automatic clocker;"
        "repeat (2) @(posedge clk);"
        "repeat (4) @(negedge clk); endtask endclass",
        ClassDeclaration(
            0, L(0, {"class", "c", ";"}),
            TaskDeclaration(
                1, L(1, {"task", "automatic", "clocker", ";"}),
                StatementList(
                    2,              //
                    FlowControl(2,  //
                                L(2, {"repeat", "(", "2", ")"}),
                                L(3, {"@", "(", "posedge", "clk", ")", ";"})),
                    FlowControl(2,  //
                                L(2, {"repeat", "(", "4", ")"}),
                                L(3, {"@", "(", "negedge", "clk", ")", ";"}))),
                L(1, {"endtask"})),
            L(0, {"endclass"})),
    },

    {
        "task with nested repeated event control statements",
        "class c; task automatic clocker;"
        "repeat (n) repeat (m) @(posedge clk); endtask endclass",
        ClassDeclaration(
            0, L(0, {"class", "c", ";"}),
            TaskDeclaration(1, L(1, {"task", "automatic", "clocker", ";"}),
                            FlowControl(2,  //
                                        L(2, {"repeat", "(", "n", ")"}),
                                        N(3,  //
                                          L(3, {"repeat", "(", "m", ")"}),
                                          L(4, {"@", "(", "posedge", "clk", ")",
                                                ";"})  // single null-statement
                                          )),
                            L(1, {"endtask"})),
            L(0, {"endclass"})),
    },

    {
        "task with nested if statements, single-statement body",
        "class c; task automatic iffer;"
        "if (n) if (m) y = x; endtask endclass",
        ClassDeclaration(
            0, L(0, {"class", "c", ";"}),
            TaskDeclaration(1, L(1, {"task", "automatic", "iffer", ";"}),
                            FlowControl(2,  //
                                        L(2, {"if", "(", "n", ")"}),
                                        N(3,  //
                                          L(3, {"if", "(", "m", ")"}),
                                          L(4, {"y", "=", "x",
                                                ";"})  // single statement body
                                          )),
                            L(1, {"endtask"})),
            L(0, {"endclass"})),
    },

    {
        "task with assert statements, null action",
        "task t; Fire(); assert (x); assert(y); endtask",
        TaskDeclaration(0, L(0, {"task", "t", ";"}),
                        StatementList(1,  //
                                      L(1, {"Fire", "(", ")", ";"}),
                                      L(1, {"assert", "(", "x", ")", ";"}),
                                      L(1, {"assert", "(", "y", ")", ";"})),
                        L(0, {"endtask"})),
    },
    {
        "task with assert statements, non-null action",
        "task t; Fire(); assert (x) g(); assert(y) h(); endtask",
        TaskDeclaration(0, L(0, {"task", "t", ";"}),
                        StatementList(1,  //
                                      L(1, {"Fire", "(", ")", ";"}),
                                      N(1,                                //
                                        L(1, {"assert", "(", "x", ")"}),  //
                                        L(2, {"g", "(", ")", ";"})),
                                      N(1,                                //
                                        L(1, {"assert", "(", "y", ")"}),  //
                                        L(2, {"h", "(", ")", ";"}))),
                        L(0, {"endtask"})),
    },
    {
        "task with assert-else statements, empty assert body, with else action",
        "task t; assert (x) else g(); assert(y) else h(); endtask",
        TaskDeclaration(0, L(0, {"task", "t", ";"}),
                        StatementList(1,    //
                                      N(1,  //
                                        L(1, {"assert", "(", "x", ")"}),
                                        N(1,               //
                                          L(1, {"else"}),  //
                                          L(2, {"g", "(", ")", ";"}))),
                                      N(1,  //
                                        L(1, {"assert", "(", "y", ")"}),
                                        N(1,               //
                                          L(1, {"else"}),  //
                                          L(2, {"h", "(", ")", ";"})))),
                        L(0, {"endtask"})),
    },
    {
        "task with assert-else statements, with body/else action block",
        "task t; assert (x) else begin g(); end "
        "assert(y) begin jk(); end else h(); endtask",
        TaskDeclaration(
            0, L(0, {"task", "t", ";"}),
            StatementList(1,    //
                          N(1,  //
                            L(1, {"assert", "(", "x", ")"}),
                            N(1,                           //
                              L(1, {"else", "begin"}),     //
                              L(2, {"g", "(", ")", ";"}),  //
                              L(1, {"end"}))),
                          N(1,    //
                            N(1,  //
                              L(1, {"assert", "(", "y", ")", "begin"}),
                              L(2, {"jk", "(", ")", ";"})),  //
                            N(1,                             //
                              L(1, {"end", "else"}),         //
                              L(2, {"h", "(", ")", ";"})))),
            L(0, {"endtask"})),
    },
    {
        "task with assert-else statement, with action blocks in both clauses",
        "task t; assert(y) begin jk(); end else begin h(); end endtask",
        TaskDeclaration(0, L(0, {"task", "t", ";"}),
                        N(1,    //
                          N(1,  //
                            L(1, {"assert", "(", "y", ")", "begin"}),
                            L(2, {"jk", "(", ")", ";"})),    //
                          N(1,                               //
                            L(1, {"end", "else", "begin"}),  //
                            L(2, {"h", "(", ")", ";"}),      //
                            L(1, {"end"}))),
                        L(0, {"endtask"})),
    },

    {
        "task with assume statements, null action",
        "task t; Fire(); assume (x); assume(y); endtask",
        TaskDeclaration(0, L(0, {"task", "t", ";"}),
                        StatementList(1,  //
                                      L(1, {"Fire", "(", ")", ";"}),
                                      L(1, {"assume", "(", "x", ")", ";"}),
                                      L(1, {"assume", "(", "y", ")", ";"})),
                        L(0, {"endtask"})),
    },
    {
        "task with assume statements, non-null action",
        "task t; Fire(); assume (x) g(); assume(y) h(); endtask",
        TaskDeclaration(
            0, L(0, {"task", "t", ";"}),
            StatementList(
                1,  //
                L(1, {"Fire", "(", ")", ";"}),
                N(1,  //
                  L(1, {"assume", "(", "x", ")"}), L(2, {"g", "(", ")", ";"})),
                N(1,  //
                  L(1, {"assume", "(", "y", ")"}), L(2, {"h", "(", ")", ";"}))),
            L(0, {"endtask"})),
    },
    {
        "task with assume-else statements, empty assume body, with else action",
        "task t; assume (x) else g(); assume(y) else h(); endtask",
        TaskDeclaration(0, L(0, {"task", "t", ";"}),
                        StatementList(1,    //
                                      N(1,  //
                                        L(1, {"assume", "(", "x", ")"}),
                                        N(1,               //
                                          L(1, {"else"}),  //
                                          L(2, {"g", "(", ")", ";"}))),
                                      N(1,  //
                                        L(1, {"assume", "(", "y", ")"}),
                                        N(1,               //
                                          L(1, {"else"}),  //
                                          L(2, {"h", "(", ")", ";"})))),
                        L(0, {"endtask"})),
    },
    {
        "task with assume-else statements, with body/else action block",
        "task t; assume (x) else begin g(); end "
        "assume(y) begin jk(); end else h(); endtask",
        TaskDeclaration(
            0, L(0, {"task", "t", ";"}),
            StatementList(1,    //
                          N(1,  //
                            L(1, {"assume", "(", "x", ")"}),
                            N(1,                           //
                              L(1, {"else", "begin"}),     //
                              L(2, {"g", "(", ")", ";"}),  //
                              L(1, {"end"}))),
                          N(1,    //
                            N(1,  //
                              L(1, {"assume", "(", "y", ")", "begin"}),
                              L(2, {"jk", "(", ")", ";"})),  //
                            N(1,                             //
                              L(1, {"end", "else"}),         //
                              L(2, {"h", "(", ")", ";"})))),
            L(0, {"endtask"})),
    },
    {
        "task with assume-else statement, with action blocks in both clauses",
        "task t; assume(y) begin jk(); end else begin h(); end endtask",
        TaskDeclaration(0, L(0, {"task", "t", ";"}),
                        N(1,    //
                          N(1,  //
                            L(1, {"assume", "(", "y", ")", "begin"}),
                            L(2, {"jk", "(", ")", ";"})),    //
                          N(1,                               //
                            L(1, {"end", "else", "begin"}),  //
                            L(2, {"h", "(", ")", ";"}),      //
                            L(1, {"end"}))),
                        L(0, {"endtask"})),
    },

    {
        "task with cover statements, null action",
        "task t; Fire(); cover (x); cover(y); endtask",
        TaskDeclaration(0, L(0, {"task", "t", ";"}),
                        StatementList(1,  //
                                      L(1, {"Fire", "(", ")", ";"}),
                                      L(1, {"cover", "(", "x", ")", ";"}),
                                      L(1, {"cover", "(", "y", ")", ";"})),
                        L(0, {"endtask"})),
    },
    {
        "task with cover statements, non-null action",
        "task t; Fire(); cover (x) g(); cover(y) h(); endtask",
        TaskDeclaration(0, L(0, {"task", "t", ";"}),
                        StatementList(1,  //
                                      L(1, {"Fire", "(", ")", ";"}),
                                      N(1,                               //
                                        L(1, {"cover", "(", "x", ")"}),  //
                                        L(2, {"g", "(", ")", ";"})),
                                      N(1,                               //
                                        L(1, {"cover", "(", "y", ")"}),  //
                                        L(2, {"h", "(", ")", ";"}))),
                        L(0, {"endtask"})),
    },
    {
        "task with cover statements, action block",
        "task t; Fire(); cover (x) begin g();end "
        "cover(y) begin h(); end endtask",
        TaskDeclaration(
            0, L(0, {"task", "t", ";"}),
            StatementList(1,  //
                          L(1, {"Fire", "(", ")", ";"}),
                          N(1,                                        //
                            L(1, {"cover", "(", "x", ")", "begin"}),  //
                            L(2, {"g", "(", ")", ";"}),               //
                            L(1, {"end"})),
                          N(1,                                        //
                            L(1, {"cover", "(", "y", ")", "begin"}),  //
                            L(2, {"h", "(", ")", ";"}),               //
                            L(1, {"end"}))),
            L(0, {"endtask"})),
    },

    {
        "task with wait statements, null action",
        "task t; wait (a==b); wait(c<d); endtask",
        TaskDeclaration(
            0, TaskHeader(0, {"task", "t", ";"}),
            StatementList(1,  //
                          L(1, {"wait", "(", "a", "==", "b", ")", ";"}),
                          L(1, {"wait", "(", "c", "<", "d", ")", ";"})),
            L(0, {"endtask"})),
    },

    {
        "task with wait statements, non-null action",
        "task t; wait (a==b) run(); wait(c<d) walk(); endtask",
        TaskDeclaration(
            0, TaskHeader(0, {"task", "t", ";"}),
            StatementList(1,    //
                          N(1,  //
                            L(1, {"wait", "(", "a", "==", "b", ")"}),
                            L(2, {"run", "(", ")", ";"})),
                          N(1,  //
                            L(1, {"wait", "(", "c", "<", "d", ")"}),
                            L(2, {"walk", "(", ")", ";"}))),
            L(0, {"endtask"})),
    },
    {
        "task with wait statements, action block",
        "task t; wait (a==b) begin run(); end "
        "wait(c<d) begin walk(); end endtask",
        TaskDeclaration(
            0, TaskHeader(0, {"task", "t", ";"}),
            StatementList(1,    //
                          N(1,  //
                            L(1, {"wait", "(", "a", "==", "b", ")", "begin"}),
                            L(2, {"run", "(", ")", ";"}),  //
                            L(1, {"end"})),
                          N(1,  //
                            L(1, {"wait", "(", "c", "<", "d", ")", "begin"}),
                            L(2, {"walk", "(", ")", ";"}),  //
                            L(1, {"end"}))),
            L(0, {"endtask"})),
    },

    {
        "task with wait fork statements",
        "task t; wait fork ; wait fork; endtask",
        TaskDeclaration(0, TaskHeader(0, {"task", "t", ";"}),
                        StatementList(1,  //
                                      L(1, {"wait", "fork", ";"}),
                                      L(1, {"wait", "fork", ";"})),
                        L(0, {"endtask"})),
    },

    {
        "task with assert property statements, null action",
        "task t; assert property (x); assert property(y); endtask",
        TaskDeclaration(
            0, L(0, {"task", "t", ";"}),
            StatementList(1,  //
                          L(1, {"assert", "property", "(", "x", ")", ";"}),
                          L(1, {"assert", "property", "(", "y", ")", ";"})),
            L(0, {"endtask"})),
    },
    {
        "task with assert property statements, non-null action",
        "task t; assert property (x) g(); assert property(y) h(); endtask",
        TaskDeclaration(
            0, L(0, {"task", "t", ";"}),
            StatementList(1,                                              //
                          N(1,                                            //
                            L(1, {"assert", "property", "(", "x", ")"}),  //
                            L(2, {"g", "(", ")", ";"})),
                          N(1,                                            //
                            L(1, {"assert", "property", "(", "y", ")"}),  //
                            L(2, {"h", "(", ")", ";"}))),
            L(0, {"endtask"})),
    },
    {
        "task with assert-else property statements, empty assert body, with "
        "else action",
        "task t; assert property (x) else g(); assert property(y) else h(); "
        "endtask",
        TaskDeclaration(
            0, L(0, {"task", "t", ";"}),
            StatementList(1,    //
                          N(1,  //
                            L(1, {"assert", "property", "(", "x", ")"}),
                            N(1,               //
                              L(1, {"else"}),  //
                              L(2, {"g", "(", ")", ";"}))),
                          N(1,  //
                            L(1, {"assert", "property", "(", "y", ")"}),
                            N(1,               //
                              L(1, {"else"}),  //
                              L(2, {"h", "(", ")", ";"})))),
            L(0, {"endtask"})),
    },
    {
        "task with assert-else property statements, with body/else action "
        "block",
        "task t; assert property (x) else begin g(); end "
        "assert property(y) begin jk(); end else h(); endtask",
        TaskDeclaration(
            0, L(0, {"task", "t", ";"}),
            StatementList(
                1,    //
                N(1,  //
                  L(1, {"assert", "property", "(", "x", ")"}),
                  N(1,                           //
                    L(1, {"else", "begin"}),     //
                    L(2, {"g", "(", ")", ";"}),  //
                    L(1, {"end"}))),
                N(1,    //
                  N(1,  //
                    L(1, {"assert", "property", "(", "y", ")", "begin"}),
                    L(2, {"jk", "(", ")", ";"})),  //
                  N(1,                             //
                    L(1, {"end", "else"}),         //
                    L(2, {"h", "(", ")", ";"})))),
            L(0, {"endtask"})),
    },
    {
        "task with assert-else property statement, with action blocks in both "
        "clauses",
        "task t; assert property (y) begin jk(); end else begin h(); end "
        "endtask",
        TaskDeclaration(
            0, L(0, {"task", "t", ";"}),
            N(1,    //
              N(1,  //
                L(1, {"assert", "property", "(", "y", ")", "begin"}),
                L(2, {"jk", "(", ")", ";"})),    //
              N(1,                               //
                L(1, {"end", "else", "begin"}),  //
                L(2, {"h", "(", ")", ";"}),      //
                L(1, {"end"}))),
            L(0, {"endtask"})),
    },

    {
        "task with assume property statements, null action",
        "task t; assume property (x); assume property(y); endtask",
        TaskDeclaration(
            0, L(0, {"task", "t", ";"}),
            StatementList(1,  //
                          L(1, {"assume", "property", "(", "x", ")", ";"}),
                          L(1, {"assume", "property", "(", "y", ")", ";"})),
            L(0, {"endtask"})),
    },
    {
        "task with assume property statements, non-null action",
        "task t; assume property (x) g(); assume property(y) h(); endtask",
        TaskDeclaration(
            0, L(0, {"task", "t", ";"}),
            StatementList(1,                                              //
                          N(1,                                            //
                            L(1, {"assume", "property", "(", "x", ")"}),  //
                            L(2, {"g", "(", ")", ";"})),
                          N(1,                                            //
                            L(1, {"assume", "property", "(", "y", ")"}),  //
                            L(2, {"h", "(", ")", ";"}))),
            L(0, {"endtask"})),
    },
    {
        "task with assume-else property statements, empty assume body, with "
        "else action",
        "task t; assume property (x) else g(); assume property(y) else h(); "
        "endtask",
        TaskDeclaration(
            0, L(0, {"task", "t", ";"}),
            StatementList(1,    //
                          N(1,  //
                            L(1, {"assume", "property", "(", "x", ")"}),
                            N(1,               //
                              L(1, {"else"}),  //
                              L(2, {"g", "(", ")", ";"}))),
                          N(1,  //
                            L(1, {"assume", "property", "(", "y", ")"}),
                            N(1,               //
                              L(1, {"else"}),  //
                              L(2, {"h", "(", ")", ";"})))),
            L(0, {"endtask"})),
    },
    {
        "task with assume-else property statements, with body/else action "
        "block",
        "task t; assume property (x) else begin g(); end "
        "assume property(y) begin jk(); end else h(); endtask",
        TaskDeclaration(
            0, L(0, {"task", "t", ";"}),
            StatementList(
                1,    //
                N(1,  //
                  L(1, {"assume", "property", "(", "x", ")"}),
                  N(1,                           //
                    L(1, {"else", "begin"}),     //
                    L(2, {"g", "(", ")", ";"}),  //
                    L(1, {"end"}))),
                N(1,    //
                  N(1,  //
                    L(1, {"assume", "property", "(", "y", ")", "begin"}),
                    L(2, {"jk", "(", ")", ";"})),  //
                  N(1,                             //
                    L(1, {"end", "else"}),         //
                    L(2, {"h", "(", ")", ";"})))),
            L(0, {"endtask"})),
    },
    {
        "task with assume-else property statement, with action blocks in both "
        "clauses",
        "task t; assume property (y) begin jk(); end else begin h(); end "
        "endtask",
        TaskDeclaration(
            0, L(0, {"task", "t", ";"}),
            N(1,    //
              N(1,  //
                L(1, {"assume", "property", "(", "y", ")", "begin"}),
                L(2, {"jk", "(", ")", ";"})),    //
              N(1,                               //
                L(1, {"end", "else", "begin"}),  //
                L(2, {"h", "(", ")", ";"}),      //
                L(1, {"end"}))),
            L(0, {"endtask"})),
    },

    {
        "task with expect property statements, null action",
        "task t; expect (x); expect (y); endtask",
        TaskDeclaration(0, L(0, {"task", "t", ";"}),
                        StatementList(1,  //
                                      L(1, {"expect", "(", "x", ")", ";"}),
                                      L(1, {"expect", "(", "y", ")", ";"})),
                        L(0, {"endtask"})),
    },
    {
        "task with expect property statements, non-null action",
        "task t; expect (x) g(); expect (y) h(); endtask",
        TaskDeclaration(0, L(0, {"task", "t", ";"}),
                        StatementList(1,                                  //
                                      N(1,                                //
                                        L(1, {"expect", "(", "x", ")"}),  //
                                        L(2, {"g", "(", ")", ";"})),
                                      N(1,                                //
                                        L(1, {"expect", "(", "y", ")"}),  //
                                        L(2, {"h", "(", ")", ";"}))),
                        L(0, {"endtask"})),
    },
    {
        "task with expect-else property statements, empty expect body, with "
        "else action",
        "task t; expect (x) else g(); expect (y) else h(); "
        "endtask",
        TaskDeclaration(0, L(0, {"task", "t", ";"}),
                        StatementList(1,    //
                                      N(1,  //
                                        L(1, {"expect", "(", "x", ")"}),
                                        N(1,               //
                                          L(1, {"else"}),  //
                                          L(2, {"g", "(", ")", ";"}))),
                                      N(1,  //
                                        L(1, {"expect", "(", "y", ")"}),
                                        N(1,               //
                                          L(1, {"else"}),  //
                                          L(2, {"h", "(", ")", ";"})))),
                        L(0, {"endtask"})),
    },
    {
        "task with expect-else property statements, with body/else action "
        "block",
        "task t; expect (x) else begin g(); end "
        "expect (y) begin jk(); end else h(); endtask",
        TaskDeclaration(
            0, L(0, {"task", "t", ";"}),
            StatementList(1,    //
                          N(1,  //
                            L(1, {"expect", "(", "x", ")"}),
                            N(1,                           //
                              L(1, {"else", "begin"}),     //
                              L(2, {"g", "(", ")", ";"}),  //
                              L(1, {"end"}))),
                          N(1,    //
                            N(1,  //
                              L(1, {"expect", "(", "y", ")", "begin"}),
                              L(2, {"jk", "(", ")", ";"})),  //
                            N(1,                             //
                              L(1, {"end", "else"}),         //
                              L(2, {"h", "(", ")", ";"})))),
            L(0, {"endtask"})),
    },
    {
        "task with expect-else property statement, with action blocks in both "
        "clauses",
        "task t; expect (y) begin jk(); end else begin h(); end "
        "endtask",
        TaskDeclaration(0, L(0, {"task", "t", ";"}),
                        N(1,    //
                          N(1,  //
                            L(1, {"expect", "(", "y", ")", "begin"}),
                            L(2, {"jk", "(", ")", ";"})),    //
                          N(1,                               //
                            L(1, {"end", "else", "begin"}),  //
                            L(2, {"h", "(", ")", ";"}),      //
                            L(1, {"end"}))),
                        L(0, {"endtask"})),
    },

    {
        "task with cover property statements, null action",
        "task t; cover property (x); cover property(y); endtask",
        TaskDeclaration(
            0, L(0, {"task", "t", ";"}),
            StatementList(1,  //
                          L(1, {"cover", "property", "(", "x", ")", ";"}),
                          L(1, {"cover", "property", "(", "y", ")", ";"})),
            L(0, {"endtask"})),
    },
    {
        "task with cover property statements, non-null action",
        "task t; cover property (x) g(); cover property(y) h(); endtask",
        TaskDeclaration(
            0, L(0, {"task", "t", ";"}),
            StatementList(1,                                             //
                          N(1,                                           //
                            L(1, {"cover", "property", "(", "x", ")"}),  //
                            L(2, {"g", "(", ")", ";"})),
                          N(1,                                           //
                            L(1, {"cover", "property", "(", "y", ")"}),  //
                            L(2, {"h", "(", ")", ";"}))),
            L(0, {"endtask"})),
    },
    {
        "task with cover property statements, with block action "
        "block",
        "task t; cover property (x) begin g(); end "
        "cover property(y) begin jk(); end endtask",
        TaskDeclaration(
            0, L(0, {"task", "t", ";"}),
            StatementList(1,    //
                          N(1,  //
                            L(1, {"cover", "property", "(", "x", ")", "begin"}),
                            L(2, {"g", "(", ")", ";"}),  //
                            L(1, {"end"})),
                          N(1,  //
                            L(1, {"cover", "property", "(", "y", ")", "begin"}),
                            L(2, {"jk", "(", ")", ";"}),  //
                            L(1, {"end"}))),              //
            L(0, {"endtask"})),
    },

    {
        "task with cover sequence statements, null action",
        "task t; cover sequence (x); cover sequence(y); endtask",
        TaskDeclaration(
            0, L(0, {"task", "t", ";"}),
            StatementList(1,  //
                          L(1, {"cover", "sequence", "(", "x", ")", ";"}),
                          L(1, {"cover", "sequence", "(", "y", ")", ";"})),
            L(0, {"endtask"})),
    },
    {
        "task with cover sequence statements, non-null action",
        "task t; cover sequence (x) g(); cover sequence(y) h(); endtask",
        TaskDeclaration(
            0, L(0, {"task", "t", ";"}),
            StatementList(1,                                             //
                          N(1,                                           //
                            L(1, {"cover", "sequence", "(", "x", ")"}),  //
                            L(2, {"g", "(", ")", ";"})),
                          N(1,                                           //
                            L(1, {"cover", "sequence", "(", "y", ")"}),  //
                            L(2, {"h", "(", ")", ";"}))),
            L(0, {"endtask"})),
    },
    {
        "task with cover sequence statements, with block action "
        "block",
        "task t; cover sequence (x) begin g(); end "
        "cover sequence(y) begin jk(); end endtask",
        TaskDeclaration(
            0, L(0, {"task", "t", ";"}),
            StatementList(1,    //
                          N(1,  //
                            L(1, {"cover", "sequence", "(", "x", ")", "begin"}),
                            L(2, {"g", "(", ")", ";"}),  //
                            L(1, {"end"})),
                          N(1,  //
                            L(1, {"cover", "sequence", "(", "y", ")", "begin"}),
                            L(2, {"jk", "(", ")", ";"}),  //
                            L(1, {"end"}))),              //
            L(0, {"endtask"})),
    },

    // TODO(fangism): test calls to UVM macros
};

// Test that TreeUnwrapper produces correct UnwrappedLines from task tests
TEST_F(TreeUnwrapperTest, UnwrapTaskTests) {
  for (const auto &test_case : kUnwrapTaskTestCases) {
    auto tree_unwrapper = CreateTreeUnwrapper(test_case.source_code);
    const auto *uwline_tree = tree_unwrapper->Unwrap();
    EXPECT_TRUE(VerifyUnwrappedLines(&std::cout, *ABSL_DIE_IF_NULL(uwline_tree),
                                     test_case));
  }
}

const TreeUnwrapperTestData kUnwrapFunctionTestCases[] = {
    {
        "empty function",
        "function foo;"
        "endfunction : foo",
        FunctionDeclaration(0, FunctionHeader(0, {"function", "foo", ";"}),
                            L(0, {"endfunction", ":", "foo"})),
    },

    {
        "empty function, comment statement",
        "function foo;// foo does x\n"
        "// statement comment\n"
        "endfunction : foo",
        FunctionDeclaration(
            0, FunctionHeader(0, {"function", "foo", ";", "// foo does x"}),
            L(1, {"// statement comment"}),  //
            L(0, {"endfunction", ":", "foo"})),
    },

    {
        "two empty functions",
        "function funk;"
        "endfunction : funk "
        "function foo;"
        "endfunction : foo",
        FunctionDeclaration(0, FunctionHeader(0, {"function", "funk", ";"}),
                            L(0, {"endfunction", ":", "funk"})),
        FunctionDeclaration(0, FunctionHeader(0, {"function", "foo", ";"}),
                            L(0, {"endfunction", ":", "foo"})),
    },

    {
        "empty function, empty ports, comment statement",
        "function foo();// foo\n"
        "// statement comment\n"
        "endfunction : foo",
        FunctionDeclaration(
            0, FunctionHeader(0, {"function", "foo", "(", ")", ";", "// foo"}),
            L(1, {"// statement comment"}), L(0, {"endfunction", ":", "foo"})),
    },

    {
        "function with empty formal arguments",
        "function void foo();"
        "endfunction",
        FunctionDeclaration(
            0, FunctionHeader(0, {"function", "void", "foo", "(", ")", ";"}),
            L(0, {"endfunction"})),
    },

    {
        "function with formal argument",
        "function foo(string name);"
        "endfunction : foo",
        FunctionDeclaration(0,
                            N(0, L(0, {"function", "foo", "("}),
                              L(2, {"string", "name", ")", ";"})),
                            L(0, {"endfunction", ":", "foo"})),
    },

    {
        "function with multiple formal arguments",
        "function foo(string name, int a);"
        "endfunction",
        FunctionDeclaration(0,
                            N(0, L(0, {"function", "foo", "("}),
                              TFPortList(2,                              //
                                         L(2, {"string", "name", ","}),  //
                                         L(2, {"int", "a", ")", ";"}))),
                            L(0, {"endfunction"})),
    },

    {
        "function with local variable",
        "function foo;"
        "int value;"
        "endfunction",
        FunctionDeclaration(0, FunctionHeader(0, {"function", "foo", ";"}),
                            L(1, {"int", "value", ";"}), L(0, {"endfunction"})),
    },
    {
        "function with only one variable declaration and comments",
        "function foo;// foo does x\n"
        "//comment1\n"
        "int bar; //comment2\n"
        "//comment3\n"
        "endfunction : foo",
        FunctionDeclaration(
            0, FunctionHeader(0, {"function", "foo", ";", "// foo does x"}),
            N(1,                                        //
              L(1, {"//comment1"}),                     //
              L(1, {"int", "bar", ";", "//comment2"}),  //
              L(1, {"//comment3"})),                    //
            L(0, {"endfunction", ":", "foo"})),
    },

    {
        "in function, implicit-type data declaration, singleton",
        "function f ;a;endfunction",
        FunctionDeclaration(0, FunctionHeader(0, {"function", "f", ";"}),
                            L(1, {"a", ";"}), L(0, {"endfunction"})),
    },
    {
        "in function, two implicit-type data declaration",
        "function f;a;b;endfunction",
        FunctionDeclaration(0, FunctionHeader(0, {"function", "f", ";"}),
                            StatementList(1,                 //
                                          L(1, {"a", ";"}),  //
                                          L(1, {"b", ";"})),
                            L(0, {"endfunction"})),
    },

    {
        "function with assignment to call expression",
        "function foo;"
        "y = twister(x, 1);"
        "endfunction",
        FunctionDeclaration(
            0, FunctionHeader(0, {"function", "foo", ";"}),
            N(1, L(1, {"y", "=", "twister", "("}),
              N(3, L(3, {"x", ","}), L(3, {"1"})), L(1, {")", ";"})),
            L(0, {"endfunction"})),
    },

    {
        "function with multiple statements",
        "function foo;"
        "y = twister(x, 1);"
        "z = twister(x, 2);"
        "endfunction",
        FunctionDeclaration(
            0, FunctionHeader(0, {"function", "foo", ";"}),
            StatementList(
                1,
                N(1, L(1, {"y", "=", "twister", "("}),
                  N(3, L(3, {"x", ","}), L(3, {"1"})), L(1, {")", ";"})),
                N(1, L(1, {"z", "=", "twister", "("}),
                  N(3, L(3, {"x", ","}), L(3, {"2"})), L(1, {")", ";"}))),
            L(0, {"endfunction"})),
    },

    {
        "function with foreach block with multiple statements",
        "function foo;"
        "foreach (x[i]) begin "
        "y = twister(x[i], 1);"
        "z = twister(x[i], 2);"
        "end "
        "endfunction",
        FunctionDeclaration(
            0, FunctionHeader(0, {"function", "foo", ";"}),
            FlowControl(
                1, L(1, {"foreach", "(", "x", "[", "i", "]", ")", "begin"}),
                StatementList(
                    2,
                    N(2, L(2, {"y", "=", "twister", "("}),
                      N(4, L(4, {"x", "[", "i", "]", ","}), L(4, {"1"})),
                      L(2, {")", ";"})),
                    N(2, L(2, {"z", "=", "twister", "("}),
                      N(4, L(4, {"x", "[", "i", "]", ","}), L(4, {"2"})),
                      L(2, {")", ";"}))),
                L(1, {"end"})),
            L(0, {"endfunction"})),
    },

    {
        "function with foreach block with single statements",
        "function foo;"
        "foreach (x[i]) y = twister(x[i], 1);"
        "endfunction",
        FunctionDeclaration(
            0, FunctionHeader(0, {"function", "foo", ";"}),
            FlowControl(1,  //
                        L(1, {"foreach", "(", "x", "[", "i", "]", ")"}),
                        N(2, L(2, {"y", "=", "twister", "("}),
                          N(4, L(4, {"x", "[", "i", "]", ","}), L(4, {"1"})),
                          L(2, {")", ";"}))),
            L(0, {"endfunction"})),
    },

    {
        "function with nested foreach block with single statements",
        "function foo;"
        "foreach (x[i]) foreach(j[k]) y = x;"
        "endfunction",
        FunctionDeclaration(
            0, FunctionHeader(0, {"function", "foo", ";"}),
            FlowControl(1,                                                  //
                        L(1, {"foreach", "(", "x", "[", "i", "]", ")"}),    //
                        N(2,                                                //
                          L(2, {"foreach", "(", "j", "[", "k", "]", ")"}),  //
                          L(3, {"y", "=", "x", ";"}))),
            L(0, {"endfunction"})),
    },

    {
        "function with assignment to macro call",
        "function foo;"
        "y = `TWISTER(x, y);"
        "endfunction",
        FunctionDeclaration(
            0, FunctionHeader(0, {"function", "foo", ";"}),
            N(1, L(1, {"y", "=", "`TWISTER", "("}),
              MacroArgList(3, L(3, {"x", ","}), L(3, {"y", ")", ";"}))),
            L(0, {"endfunction"})),
    },

    {
        "function with array formal parameters and return statement",
        "function automatic logic checkit ("
        "input logic [4:0] a,"
        "input logic [4:0] b);"
        "return a ^ b;"
        "endfunction",
        FunctionDeclaration(
            0,
            N(0,  //
              L(0, {"function", "automatic", "logic", "checkit", "("}),
              TFPortList(
                  2,
                  L(2, {"input", "logic", "[", "4", ":", "0", "]", "a", ","}),
                  L(2, {"input", "logic", "[", "4", ":", "0", "]", "b", ")",
                        ";"}))),
            L(1, {"return", "a", "^", "b", ";"}), L(0, {"endfunction"})),
    },

    {
        "function with formal parameters declared in-body",
        "function automatic index_t make_index;"
        "input logic [1:0] addr;"
        "input mode_t mode;"
        "input logic [2:0] hash_mask;"
        "endfunction",
        FunctionDeclaration(
            0,
            FunctionHeader(
                0, {"function", "automatic", "index_t", "make_index", ";"}),
            StatementList(
                1,
                L(1, {"input", "logic", "[", "1", ":", "0", "]", "addr", ";"}),
                L(1, {"input", "mode_t", "mode", ";"}),
                L(1, {"input", "logic", "[", "2", ":", "0", "]", "hash_mask",
                      ";"})),
            L(0, {"endfunction"})),
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
        FunctionDeclaration(
            0, FunctionHeader(0, {"function", "foo", ";"}),
            StatementList(1,
                          FlowControl(1,  //
                                      L(1, {"if", "(", "zz", ")", "begin"}),
                                      L(2, {"return", "0", ";"}),  //
                                      L(1, {"end"})),              //
                          FlowControl(1, L(1, {"if", "(", "yy", ")", "begin"}),
                                      L(2, {"return", "1", ";"}),  //
                                      L(1, {"end"}))),
            L(0, {"endfunction"})),
    },

    {
        "function with back-to-back if statements, single-statement body",
        "function foo;"
        "if (zz) "
        "return 0;"
        "if (yy) "
        "return 1;"
        "endfunction",
        FunctionDeclaration(
            0, FunctionHeader(0, {"function", "foo", ";"}),
            StatementList(1,
                          FlowControl(1,  //
                                      L(1, {"if", "(", "zz", ")"}),
                                      L(2, {"return", "0", ";"})),
                          FlowControl(1,  //
                                      L(1, {"if", "(", "yy", ")"}),
                                      L(2, {"return", "1", ";"}))),
            L(0, {"endfunction"})),
    },

    {
        "function with back-to-back if statements, null body",
        "function foo;"
        "if (zz);"
        "if (yy);"
        "return 1;"
        "endfunction",
        FunctionDeclaration(0, FunctionHeader(0, {"function", "foo", ";"}),
                            StatementList(1, L(1, {"if", "(", "zz", ")", ";"}),
                                          L(1, {"if", "(", "yy", ")", ";"}),
                                          L(1, {"return", "1", ";"})),
                            L(0, {"endfunction"})),
    },

    {
        "function with back-to-back if-else statements, null bodies",
        "function foo;"
        "if (zz); else ;"  // yes, this is syntactically legal
        "if (yy); else ;"
        "return 1;"
        "endfunction",
        FunctionDeclaration(
            0, FunctionHeader(0, {"function", "foo", ";"}),
            StatementList(1,
                          FlowControl(1,                                  //
                                      L(1, {"if", "(", "zz", ")", ";"}),  //
                                      L(1, {"else", ";"})),
                          FlowControl(1,                                  //
                                      L(1, {"if", "(", "yy", ")", ";"}),  //
                                      L(1, {"else", ";"})),
                          L(1, {"return", "1", ";"})),
            L(0, {"endfunction"})),
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
        FunctionDeclaration(
            0, FunctionHeader(0, {"function", "foo", ";"}),
            FlowControl(1,                                        //
                        N(1,                                      //
                          L(1, {"if", "(", "zz", ")", "begin"}),  //
                          L(2, {"return", "0", ";"})),            //
                        N(1,                                      //
                          L(1, {"end", "else", "begin"}),         //
                          L(2, {"return", "1", ";"}),             //
                          L(1, {"end"}))),
            L(0, {"endfunction"})),
    },

    {
        "function with if-else branches, single-statements",
        "function foo;"
        "if (zz) "
        "return 0;"
        "else "
        "return 1;"
        "endfunction",
        FunctionDeclaration(
            0, FunctionHeader(0, {"function", "foo", ";"}),
            FlowControl(1,                               //
                        N(1,                             //
                          L(1, {"if", "(", "zz", ")"}),  // same level
                          L(2, {"return", "0", ";"})),   //
                        N(1,                             //
                          L(1, {"else"}),                // same level
                          L(2, {"return", "1", ";"}))),
            L(0, {"endfunction"})),
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
        FunctionDeclaration(
            0, FunctionHeader(0, {"function", "foo", ";"}),
            FlowControl(1,    //
                        N(1,  //
                          L(1, {"if", "(", "zz", ")", "begin"}),
                          L(2, {"return", "0", ";"})),
                        N(1,  //
                          L(1, {"end", "else", "if", "(", "yy", ")", "begin"}),
                          L(2, {"return", "1", ";"}),  //
                          L(1, {"end"}))),
            L(0, {"endfunction"})),
    },

    {
        "function with else-if branches, single-statements",
        "function foo;"
        "if (zz) "
        "return 0;"
        "else if (yy) "
        "return 1;"
        "endfunction",
        FunctionDeclaration(
            0, FunctionHeader(0, {"function", "foo", ";"}),
            FlowControl(1,                               //
                        N(1,                             //
                          L(1, {"if", "(", "zz", ")"}),  // same level
                          L(2, {"return", "0", ";"})),
                        N(1,                                     //
                          L(1, {"else", "if", "(", "yy", ")"}),  // same level
                          L(2, {"return", "1", ";"}))),
            L(0, {"endfunction"})),
    },

    {
        "function with two else-if branches, single-statements",
        "function foo;"
        "if (zz) "
        "return 0;"
        "else if (yy) "
        "return 1;"
        "else if (xx) "
        "return 2;"
        "endfunction",
        FunctionDeclaration(
            0, FunctionHeader(0, {"function", "foo", ";"}),
            FlowControl(1,                               //
                        N(1,                             //
                          L(1, {"if", "(", "zz", ")"}),  // same level
                          L(2, {"return", "0", ";"})),
                        N(1,                                     //
                          L(1, {"else", "if", "(", "yy", ")"}),  // same level
                          L(2, {"return", "1", ";"})),
                        N(1,                                     //
                          L(1, {"else", "if", "(", "xx", ")"}),  // same level
                          L(2, {"return", "2", ";"}))),
            L(0, {"endfunction"})),
    },

    {
        "function with else-if branches, trailing else branch, "
        "single-statements",
        "function foo;"
        "if (zz) "
        "return 0;"
        "else if (yy) "
        "return 1;"
        "else "
        "return 2;"
        "endfunction",
        FunctionDeclaration(
            0, FunctionHeader(0, {"function", "foo", ";"}),
            FlowControl(1,                                       //
                        N(1,                                     //
                          L(1, {"if", "(", "zz", ")"}),          // same level
                          L(2, {"return", "0", ";"})),           //
                        N(1,                                     //
                          L(1, {"else", "if", "(", "yy", ")"}),  // same level
                          L(2, {"return", "1", ";"})),           //
                        N(1,                                     //
                          L(1, {"else"}),                        // same level
                          L(2, {"return", "2", ";"}))),
            L(0, {"endfunction"})),
    },

    {
        "function with many else-if branches, single-statements",
        "function foo;"
        "if (zz) "
        "return 0;"
        "else if (yy) "
        "return 1;"
        "else if (xx) "
        "return 2;"
        "else if (ww) "
        "return 3;"
        "else if (vv) "
        "return 4;"
        "endfunction",
        FunctionDeclaration(
            0, FunctionHeader(0, {"function", "foo", ";"}),
            FlowControl(1,                               //
                        N(1,                             //
                          L(1, {"if", "(", "zz", ")"}),  // same level
                          L(2, {"return", "0", ";"})),
                        N(1,                                     //
                          L(1, {"else", "if", "(", "yy", ")"}),  // same level
                          L(2, {"return", "1", ";"})),
                        N(1,                                     //
                          L(1, {"else", "if", "(", "xx", ")"}),  // same level
                          L(2, {"return", "2", ";"})),
                        N(1,                                     //
                          L(1, {"else", "if", "(", "ww", ")"}),  // same level
                          L(2, {"return", "3", ";"})),
                        N(1,                                     //
                          L(1, {"else", "if", "(", "vv", ")"}),  // same level
                          L(2, {"return", "4", ";"}))),
            L(0, {"endfunction"})),
    },

    {
        "function with else-if branches, labeled begin and end",
        "function foo;"
        "if (zz) begin : label1 "
        "return 0;"
        "end : label1 "
        "else if (yy) begin : label2 "
        "return 1;"
        "end : label2 "
        "endfunction",
        FunctionDeclaration(
            0, FunctionHeader(0, {"function", "foo", ";"}),
            FlowControl(
                1,
                N(1,  //
                  L(1, {"if", "(", "zz", ")", "begin", ":", "label1"}),
                  L(2, {"return", "0", ";"}),     //
                  L(1, {"end", ":", "label1"})),  //
                N(1,                              //
                  L(1, {"else", "if", "(", "yy", ")", "begin", ":", "label2"}),
                  L(2, {"return", "1", ";"}),  //
                  L(1, {"end", ":", "label2"}))),
            L(0, {"endfunction"})),
    },

    {
        "function with else-if-else branches, labeled begin and end",
        "function foo;"
        "if (zz) begin : label1 "
        "return 0;"
        "end : label1 "
        "else if (yy) begin : label2 "
        "return 1;"
        "end : label2 "
        "else begin : label3 "
        "return 2;"
        "end : label3 "
        "endfunction",
        FunctionDeclaration(
            0, FunctionHeader(0, {"function", "foo", ";"}),
            FlowControl(
                1,
                N(1,  //
                  L(1, {"if", "(", "zz", ")", "begin", ":", "label1"}),
                  L(2, {"return", "0", ";"}),     //
                  L(1, {"end", ":", "label1"})),  //
                N(1,                              //
                  L(1, {"else", "if", "(", "yy", ")", "begin", ":", "label2"}),
                  L(2, {"return", "1", ";"}),              //
                  L(1, {"end", ":", "label2"})),           //
                N(1,                                       //
                  L(1, {"else", "begin", ":", "label3"}),  //
                  L(2, {"return", "2", ";"}),              //
                  L(1, {"end", ":", "label3"}))),
            L(0, {"endfunction"})),
    },

    {
        "function with assertion statements, null-statements",
        "function foo;"
        "assert (b); "
        "assert (c); "
        "endfunction",
        FunctionDeclaration(0, FunctionHeader(0, {"function", "foo", ";"}),
                            N(1,                                      //
                              L(1, {"assert", "(", "b", ")", ";"}),   //
                              L(1, {"assert", "(", "c", ")", ";"})),  //
                            L(0, {"endfunction"})),
    },

    {
        "function with deferred assertion statements, null-statements",
        "function foo;"
        "assert final(b); "
        "assert #0(c); "
        "endfunction",
        FunctionDeclaration(
            0, FunctionHeader(0, {"function", "foo", ";"}),
            N(1,                                                //
              L(1, {"assert", "final", "(", "b", ")", ";"}),    //
              L(1, {"assert", "#", "0", "(", "c", ")", ";"})),  //
            L(0, {"endfunction"})),
    },

    {
        "function with assert-else branches in begin/end",
        "function foo;"
        "assert (zz) begin "
        "return 0;"
        "end else begin "
        "return 1;"
        "end "
        "endfunction",
        FunctionDeclaration(
            0, FunctionHeader(0, {"function", "foo", ";"}),
            FlowControl(1,                                            //
                        N(1,                                          //
                          L(1, {"assert", "(", "zz", ")", "begin"}),  //
                          L(2, {"return", "0", ";"})),                //
                        N(1,                                          //
                          L(1, {"end", "else", "begin"}),             //
                          L(2, {"return", "1", ";"}),                 //
                          L(1, {"end"}))),
            L(0, {"endfunction"})),
    },

    {
        "function with assert-else branches, null assert-clause-body",
        "function foo;"
        "assert (zz) "
        "else "
        "foo();"
        "endfunction",
        FunctionDeclaration(0, FunctionHeader(0, {"function", "foo", ";"}),
                            FlowControl(1,  //
                                        L(1, {"assert", "(", "zz", ")"}),
                                        N(1,               //
                                          L(1, {"else"}),  // same level
                                          L(2, {"foo", "(", ")", ";"}))),
                            L(0, {"endfunction"})),
    },

    {
        "function with assert-else branches, single-statements",
        "function foo;"
        "assert (zz) "
        "return 0;"
        "else "
        "return 1;"
        "endfunction",
        FunctionDeclaration(
            0, FunctionHeader(0, {"function", "foo", ";"}),
            FlowControl(1,                                   //
                        N(1,                                 //
                          L(1, {"assert", "(", "zz", ")"}),  // same level
                          L(2, {"return", "0", ";"})),       //
                        N(1,                                 //
                          L(1, {"else"}),                    // same level
                          L(2, {"return", "1", ";"}))),
            L(0, {"endfunction"})),
    },

    {
        "function with assume statements, null-statements",
        "function foo;"
        "assume (b); "
        "assume (c); "
        "endfunction",
        FunctionDeclaration(0, FunctionHeader(0, {"function", "foo", ";"}),
                            N(1,                                      //
                              L(1, {"assume", "(", "b", ")", ";"}),   //
                              L(1, {"assume", "(", "c", ")", ";"})),  //
                            L(0, {"endfunction"})),
    },

    {
        "function with deferred assume statements, null-statements",
        "function foo;"
        "assume final(b); "
        "assume #0(c); "
        "endfunction",
        FunctionDeclaration(
            0, FunctionHeader(0, {"function", "foo", ";"}),
            N(1,                                                //
              L(1, {"assume", "final", "(", "b", ")", ";"}),    //
              L(1, {"assume", "#", "0", "(", "c", ")", ";"})),  //
            L(0, {"endfunction"})),
    },

    {
        "function with assume-else branches in begin/end",
        "function foo;"
        "assume (zz) begin "
        "return 0;"
        "end else begin "
        "return 1;"
        "end "
        "endfunction",
        FunctionDeclaration(
            0, FunctionHeader(0, {"function", "foo", ";"}),
            FlowControl(1,                                            //
                        N(1,                                          //
                          L(1, {"assume", "(", "zz", ")", "begin"}),  //
                          L(2, {"return", "0", ";"})),                //
                        N(1,                                          //
                          L(1, {"end", "else", "begin"}),             //
                          L(2, {"return", "1", ";"}),                 //
                          L(1, {"end"}))),
            L(0, {"endfunction"})),
    },

    {
        "function with assume-else branches, null assume-clause-body",
        "function foo;"
        "assume (zz) "
        "else "
        "foo();"
        "endfunction",
        FunctionDeclaration(0, FunctionHeader(0, {"function", "foo", ";"}),
                            FlowControl(1,  //
                                        L(1, {"assume", "(", "zz", ")"}),
                                        N(1,               //
                                          L(1, {"else"}),  // same level
                                          L(2, {"foo", "(", ")", ";"}))),
                            L(0, {"endfunction"})),
    },

    {
        "function with assume-else branches, single-statements",
        "function foo;"
        "assume (zz) "
        "return 0;"
        "else "
        "return 1;"
        "endfunction",
        FunctionDeclaration(
            0, FunctionHeader(0, {"function", "foo", ";"}),
            FlowControl(1,                                   //
                        N(1,                                 //
                          L(1, {"assume", "(", "zz", ")"}),  // same level
                          L(2, {"return", "0", ";"})),       //
                        N(1,                                 //
                          L(1, {"else"}),                    // same level
                          L(2, {"return", "1", ";"}))),
            L(0, {"endfunction"})),
    },

    {
        "function with for loop",
        "function foo;"
        "for (x=0;x<N;++x) begin "
        "return 1;"
        "end "
        "endfunction",
        FunctionDeclaration(
            0, FunctionHeader(0, {"function", "foo", ";"}),
            FlowControl(1,
                        LoopHeader(1, L(1, {"for", "("}),
                                   ForSpec(3, L(3, {"x", "=", "0", ";"}),
                                           L(3, {"x", "<", "N", ";"}),
                                           L(3, {"++", "x"})),
                                   L(1, {")", "begin"})),  //
                        L(2, {"return", "1", ";"}),        //
                        L(1, {"end"})),
            L(0, {"endfunction"})),
    },

    {
        "function with for loop, null-statements",
        "function foo;"
        "for (;;); "
        "endfunction",
        FunctionDeclaration(
            0, FunctionHeader(0, {"function", "foo", ";"}),
            FlowControl(1,
                        LoopHeader(1, L(1, {"for", "("}),
                                   ForSpec(3, L(3, {";"}), L(3, {";"})),
                                   L(1, {")"})),  //
                        L(2, {";"})),             //
            L(0, {"endfunction"})),
    },

    {
        "function with for loop, single-statement body",
        "function foo;"
        "for (x=0;x<N;++x) y=x;"
        "endfunction",
        FunctionDeclaration(
            0, FunctionHeader(0, {"function", "foo", ";"}),
            FlowControl(1,
                        LoopHeader(1, L(1, {"for", "("}),
                                   ForSpec(3, L(3, {"x", "=", "0", ";"}),
                                           L(3, {"x", "<", "N", ";"}),
                                           L(3, {"++", "x"})),
                                   L(1, {")"})),
                        L(2, {"y", "=", "x", ";"})),
            L(0, {"endfunction"})),
    },

    {
        "function with for loop with function call in initializer",
        "function void looper();\n"
        "  for (int i=f(m); i>=0; i--) begin\n"
        "  end\n"
        "endfunction\n",
        FunctionDeclaration(
            0, FunctionHeader(0, {"function", "void", "looper", "(", ")", ";"}),
            FlowControl(
                1,
                LoopHeader(1, L(1, {"for", "("}),
                           ForSpec(3,                                    //
                                   N(3,                                  //
                                     L(3, {"int", "i", "=", "f", "("}),  //
                                     L(5, {"m"}),                        //
                                     L(3, {")", ";"})                    //
                                     ),
                                   L(3, {"i", ">=", "0", ";"}),  //
                                   L(3, {"i", "--"})),
                           L(1, {")", "begin"})),
                L(1, {"end"})),
            L(0, {"endfunction"})),
    },

    {
        "function with for loop with function call in condition",
        "function void looper();\n"
        "  for (int i = 0; i < f(m); i++) begin\n"
        "  end\n"
        "endfunction\n",
        FunctionDeclaration(
            0, FunctionHeader(0, {"function", "void", "looper", "(", ")", ";"}),
            FlowControl(1,
                        LoopHeader(1, L(1, {"for", "("}),
                                   ForSpec(3, L(3, {"int", "i", "=", "0", ";"}),
                                           N(3,                           //
                                             L(3, {"i", "<", "f", "("}),  //
                                             L(5, {"m"}),                 //
                                             L(3, {")", ";"})             //
                                             ),
                                           L(3, {"i", "++"})),
                                   L(1, {")", "begin"})),
                        L(1, {"end"})),
            L(0, {"endfunction"})),
    },

    {
        "function with for loop, labeled begin and end",
        "function foo;"
        "for (x=0;x<N;++x) begin:yyy "
        "return 1;"
        "end:yyy "
        "endfunction",
        FunctionDeclaration(
            0, FunctionHeader(0, {"function", "foo", ";"}),
            FlowControl(1,
                        LoopHeader(1, L(1, {"for", "("}),
                                   ForSpec(3, L(3, {"x", "=", "0", ";"}),
                                           L(3, {"x", "<", "N", ";"}),
                                           L(3, {"++", "x"})),
                                   L(1, {")", "begin", ":", "yyy"})),  //
                        L(2, {"return", "1", ";"}),                    //
                        L(1, {"end", ":", "yyy"})),
            L(0, {"endfunction"})),
    },

    {
        "function with forever loop, block statement body",
        "function foo;"
        "forever begin "
        "return 1;"
        "end "
        "endfunction",
        FunctionDeclaration(
            0, FunctionHeader(0, {"function", "foo", ";"}),
            FlowControl(1, L(1, {"forever", "begin"}),
                        L(2, {"return", "1", ";"}), L(1, {"end"})),
            L(0, {"endfunction"})),
    },

    {
        "function with forever loop, single-statement body",
        "function foo;"
        "forever break; "
        "endfunction",
        FunctionDeclaration(0, FunctionHeader(0, {"function", "foo", ";"}),
                            FlowControl(1,                  //
                                        L(1, {"forever"}),  //
                                        L(2, {"break", ";"})),
                            L(0, {"endfunction"})),
    },

    {
        "function with repeat loop, block statement body",
        "function foo;"
        "repeat (2) begin "
        "return 1;"
        "end "
        "endfunction",
        FunctionDeclaration(
            0, FunctionHeader(0, {"function", "foo", ";"}),
            FlowControl(1,  //
                        L(1, {"repeat", "(", "2", ")", "begin"}),
                        L(2, {"return", "1", ";"}), L(1, {"end"})),
            L(0, {"endfunction"})),
    },

    {
        "function with repeat loop, single-statement body",
        "function foo;"
        "repeat (2) continue; "
        "endfunction",
        FunctionDeclaration(0, FunctionHeader(0, {"function", "foo", ";"}),
                            FlowControl(1,                                //
                                        L(1, {"repeat", "(", "2", ")"}),  //
                                        L(2, {"continue", ";"})),
                            L(0, {"endfunction"})),
    },

    {
        "function with while loop, block statement body",
        "function foo;"
        "while (x) begin "
        "return 1;"
        "end "
        "endfunction",
        FunctionDeclaration(
            0, FunctionHeader(0, {"function", "foo", ";"}),
            FlowControl(1, L(1, {"while", "(", "x", ")", "begin"}),
                        L(2, {"return", "1", ";"}), L(1, {"end"})),
            L(0, {"endfunction"})),
    },

    {
        "function with while loop, single-statement body",
        "function foo;"
        "while (e) coyote(sooper_genius); "
        "endfunction",
        FunctionDeclaration(0, FunctionHeader(0, {"function", "foo", ";"}),
                            FlowControl(1, L(1, {"while", "(", "e", ")"}),
                                        N(2, L(2, {"coyote", "("}),
                                          L(4, {"sooper_genius", ")", ";"}))),
                            L(0, {"endfunction"})),
    },

    {
        "function with nested while loop, single-statement body",
        "function foo;"
        "while (e) while (e) coyote(sooper_genius); "
        "endfunction",
        FunctionDeclaration(
            0, FunctionHeader(0, {"function", "foo", ";"}),
            FlowControl(1,  //
                        L(1, {"while", "(", "e", ")"}),
                        N(2,  //
                          L(2, {"while", "(", "e", ")"}),
                          N(3, L(3, {"coyote", "("}),
                            L(5, {"sooper_genius", ")", ";"})))),
            L(0, {"endfunction"})),
    },

    {
        "function with do-while loop, null-statement",
        "function foo;"
        "do;"
        "while (y);"
        "endfunction",
        FunctionDeclaration(0, FunctionHeader(0, {"function", "foo", ";"}),
                            FlowControl(1,             //
                                        L(1, {"do"}),  //
                                        L(2, {";"}),   //
                                        L(1, {"while", "(", "y", ")", ";"})),
                            L(0, {"endfunction"})),
    },

    {
        "function with do-while loop",
        "function foo;"
        "do begin "
        "return 1;"
        "end while (y);"
        "endfunction",
        FunctionDeclaration(
            0, FunctionHeader(0, {"function", "foo", ";"}),
            FlowControl(1, L(1, {"do", "begin"}), L(2, {"return", "1", ";"}),
                        L(1, {"end", "while", "(", "y", ")", ";"})),
            L(0, {"endfunction"})),
    },

    {
        "function with do-while loop, single-statement",
        "function foo;"
        "do --y;"
        "while (y);"
        "endfunction",
        FunctionDeclaration(0, FunctionHeader(0, {"function", "foo", ";"}),
                            FlowControl(1,                       //
                                        L(1, {"do"}),            //
                                        L(2, {"--", "y", ";"}),  //
                                        L(1, {"while", "(", "y", ")", ";"})),
                            L(0, {"endfunction"})),
    },

    {
        "function with foreach loop",
        "function foo;"
        "foreach (x[k]) begin "
        "return 1;"
        "end "
        "endfunction",
        FunctionDeclaration(
            0, FunctionHeader(0, {"function", "foo", ";"}),
            FlowControl(
                1, L(1, {"foreach", "(", "x", "[", "k", "]", ")", "begin"}),
                L(2, {"return", "1", ";"}), L(1, {"end"})),
            L(0, {"endfunction"})),
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
        FunctionDeclaration(
            0, FunctionHeader(0, {"function", "foo", ";"}),
            StatementList(
                1,
                FlowControl(1,             //
                            LoopHeader(1,  //
                                       L(1, {"for", "("}),
                                       ForSpec(3, L(3, {";"}), L(3, {";"})),
                                       L(1, {")", "begin"})),  //
                            L(2, {"return", "0", ";"}),        //
                            L(1, {"end"})                      //
                            ),
                FlowControl(1,  //
                            LoopHeader(1, L(1, {"for", "("}),
                                       ForSpec(3, L(3, {";"}), L(3, {";"})),
                                       L(1, {")", "begin"})),  //
                            L(2, {"return", "1", ";"}),        //
                            L(1, {"end"}))),
            L(0, {"endfunction"})),
    },

    {
        "function with case statement, with comments",
        "function foo_case;"
        "case (y) \n"
        "//c1\n"
        "k1: return 0;\n"
        "//c2\n"
        "k2: return 1;\n"
        "//c3\n"
        "endcase "
        "endfunction",
        FunctionDeclaration(
            0, FunctionHeader(0, {"function", "foo_case", ";"}),
            FlowControl(1, L(1, {"case", "(", "y", ")"}),
                        CaseItemList(2,                    //
                                     L(2, {"//c1"}),       //
                                     N(2,                  //
                                       L(2, {"k1", ":"}),  //
                                       L(2, {"return", "0", ";"})),
                                     L(2, {"//c2"}),       //
                                     N(2,                  //
                                       L(2, {"k2", ":"}),  //
                                       L(2, {"return", "1", ";"})),
                                     L(2, {"//c3"})  //
                                     ),              //
                        L(1, {"endcase"})),          //
            L(0, {"endfunction"})),
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
        FunctionDeclaration(
            0, FunctionHeader(0, {"function", "foo_case", ";"}),
            StatementList(
                1,
                FlowControl(1, L(1, {"case", "(", "y", ")"}),
                            CaseItemList(2,                    //
                                         N(2,                  //
                                           L(2, {"k1", ":"}),  //
                                           L(2, {"return", "0", ";"})),
                                         N(2,                  //
                                           L(2, {"k2", ":"}),  //
                                           L(2, {"return", "1", ";"}))),
                            L(1, {"endcase"})),                //
                FlowControl(1, L(1, {"case", "(", "z", ")"}),  //
                            CaseItemList(2,                    //
                                         N(2,                  //
                                           L(2, {"k3", ":"}),  //
                                           L(2, {"return", "0", ";"})),
                                         N(2,                  //
                                           L(2, {"k4", ":"}),  //
                                           L(2, {"return", "1", ";"}))),
                            L(1, {"endcase"}))),
            L(0, {"endfunction"})),
    },

    {
        "function with case block statements",
        "function foo_case_block;"
        "case (y) "
        "k1: begin return 0; end "
        "endcase "
        "endfunction",
        FunctionDeclaration(
            0, FunctionHeader(0, {"function", "foo_case_block", ";"}),
            FlowControl(1, L(1, {"case", "(", "y", ")"}),
                        N(2,                           //
                          L(2, {"k1", ":", "begin"}),  //
                          L(3, {"return", "0", ";"}), L(2, {"end"})),
                        L(1, {"endcase"})),  //
            L(0, {"endfunction"})),
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
        FunctionDeclaration(
            0, FunctionHeader(0, {"function", "foo_case_inside", ";"}),
            StatementList(
                1,
                FlowControl(1, L(1, {"case", "(", "y", ")", "inside"}),
                            CaseItemList(2,                    //
                                         N(2,                  //
                                           L(2, {"k1", ":"}),  //
                                           L(2, {"return", "0", ";"})),
                                         N(2,                  //
                                           L(2, {"k2", ":"}),  //
                                           L(2, {"return", "1", ";"}))),
                            L(1, {"endcase"})),                       //
                FlowControl(1,                                        //
                            L(1, {"case", "(", "z", ")", "inside"}),  //
                            CaseItemList(2,                           //
                                         N(2,                         //
                                           L(2, {"k3", ":"}),         //
                                           L(2, {"return", "0", ";"})),
                                         N(2,                  //
                                           L(2, {"k4", ":"}),  //
                                           L(2, {"return", "1", ";"}))),
                            L(1, {"endcase"}))),
            L(0, {"endfunction"})),
    },

    {
        "function with case inside blocks",
        "function foo_case_inside_block;"
        "case (y) inside "
        "k2: begin return 1; end "
        "endcase "
        "endfunction",
        FunctionDeclaration(
            0, FunctionHeader(0, {"function", "foo_case_inside_block", ";"}),
            FlowControl(1, L(1, {"case", "(", "y", ")", "inside"}),
                        N(2,                           //
                          L(2, {"k2", ":", "begin"}),  //
                          L(3, {"return", "1", ";"}),  //
                          L(2, {"end"})),
                        L(1, {"endcase"})),  //
            L(0, {"endfunction"})),
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
        FunctionDeclaration(
            0, FunctionHeader(0, {"function", "foo_case_pattern", ";"}),
            StatementList(
                1,
                FlowControl(1, L(1, {"case", "(", "y", ")", "matches"}),
                            CaseItemList(2,                          //
                                         N(2,                        //
                                           L(2, {".", "foo", ":"}),  //
                                           L(2, {"return", "0", ";"})),
                                         N(2,                  //
                                           L(2, {".*", ":"}),  //
                                           L(2, {"return", "1", ";"}))),
                            L(1, {"endcase"})),                        //
                FlowControl(1,                                         //
                            L(1, {"case", "(", "z", ")", "matches"}),  //
                            CaseItemList(2,                            //
                                         N(2,                          //
                                           L(2, {".", "foo", ":"}),    //
                                           L(2, {"return", "0", ";"})),
                                         N(2,                  //
                                           L(2, {".*", ":"}),  //
                                           L(2, {"return", "1", ";"}))),
                            L(1, {"endcase"}))),
            L(0, {"endfunction"})),
    },

    {
        "function with case pattern blocks",
        "function foo_case_pattern_block;"
        "case (y) matches "
        ".foo: begin return 0; end "
        "endcase "
        "endfunction",
        FunctionDeclaration(
            0, FunctionHeader(0, {"function", "foo_case_pattern_block", ";"}),
            FlowControl(1, L(1, {"case", "(", "y", ")", "matches"}),
                        N(2,                                 //
                          L(2, {".", "foo", ":", "begin"}),  //
                          L(3, {"return", "0", ";"}),        //
                          L(2, {"end"})),                    //
                        L(1, {"endcase"})),
            L(0, {"endfunction"})),
    },

    {
        "function with randcase statements",
        "function foo_randcase;"
        "randcase "
        "k1: return 0;"
        "k2: return 1;"
        "endcase "
        "randcase "
        "k3: return 0;"
        "k4: return 1;"
        "endcase "
        "endfunction",
        FunctionDeclaration(
            0, FunctionHeader(0, {"function", "foo_randcase", ";"}),
            StatementList(
                1,
                FlowControl(1, L(1, {"randcase"}),
                            CaseItemList(2,                    //
                                         N(2,                  //
                                           L(2, {"k1", ":"}),  //
                                           L(2, {"return", "0", ";"})),
                                         N(2,                  //
                                           L(2, {"k2", ":"}),  //
                                           L(2, {"return", "1", ";"}))),
                            L(1, {"endcase"})),                //
                FlowControl(1, L(1, {"randcase"}),             //
                            CaseItemList(2,                    //
                                         N(2,                  //
                                           L(2, {"k3", ":"}),  //
                                           L(2, {"return", "0", ";"})),
                                         N(2,                  //
                                           L(2, {"k4", ":"}),  //
                                           L(2, {"return", "1", ";"}))),
                            L(1, {"endcase"}))),
            L(0, {"endfunction"})),
    },

    {
        "function with array formal parameters and return statement",
        "function automatic logic checkit ("
        "input logic [4:0] a,"
        "input logic [4:0] b);"
        "return a ^ b;"
        "endfunction",
        FunctionDeclaration(
            0,
            N(0,  //
              L(0, {"function", "automatic", "logic", "checkit", "("}),
              TFPortList(
                  2,
                  L(2, {"input", "logic", "[", "4", ":", "0", "]", "a", ","}),
                  L(2, {"input", "logic", "[", "4", ":", "0", "]", "b", ")",
                        ";"}))),
            L(1, {"return", "a", "^", "b", ";"}), L(0, {"endfunction"})),
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
        ClassDeclaration(
            0, L(0, {"class", "foo", ";"}),
            FunctionDeclaration(
                1,
                N(1,                               //
                  L(1, {"function", "new", "("}),  //
                  L(3, {"string", "name", ")", ";"})),
                StatementList(
                    2,
                    N(2, L(2, {"super", ".", "new", "("}),
                      L(4, {"name", ")", ";"})),
                    FlowControl(
                        2,
                        L(2,
                          {"foreach", "(", "bar", "[", "j", "]", ")", "begin"}),
                        StatementList(3,
                                      L(3, {"bar", "[", "j", "]", "=", "new",
                                            "(", ")", ";"}),
                                      L(3, {"bar", "[", "j", "]", ".", "x", "=",
                                            "new", "(", ")", ";"})),
                        L(2, {"end"}))),
                L(1, {"endfunction"})),
            L(0, {"endclass"})),
    },

    {
        "function with randomize-with call with comments",
        "function f;\n"
        "s = std::randomize() with {\n"
        "// comment1\n"
        "a == e;\n"
        "// comment2\n"
        "};  \n"
        "endfunction\n",
        FunctionDeclaration(
            0,  //
            L(0, {"function", "f", ";"}),
            N(1,  //
              L(1, {"s", "=", "std::randomize", "(", ")", "with", "{"}),
              N(2,                            //
                L(2, {"// comment1"}),        //
                L(2, {"a", "==", "e", ";"}),  //
                L(2, {"// comment2"})),       //
              L(1, {"}", ";"})),
            L(0, {"endfunction"})),
    },
    {
        "function with randomize-with call with leading comment",
        "function f;\n"
        "s = std::randomize() with {\n"
        "// comment\n"
        "a == e;\n"
        "if (x) {\n"
        "a;\n"
        "}\n"
        "};  \n"
        "endfunction\n",
        FunctionDeclaration(
            0,  //
            L(0, {"function", "f", ";"}),
            N(1,  //
              L(1, {"s", "=", "std::randomize", "(", ")", "with", "{"}),
              N(2,                     //
                L(2, {"// comment"}),  //
                L(2, {"a", "==", "e", ";"}),
                N(2,                                 //
                  L(2, {"if", "(", "x", ")", "{"}),  //
                  L(3, {"a", ";"}),                  //
                  L(2, {"}"}))),
              L(1, {"}", ";"})),
            L(0, {"endfunction"})),
    },

    {
        "function with function call inside if statement header",
        "function foo;"
        "if(aa(bb,cc));"
        "endfunction",
        FunctionDeclaration(
            0, L(0, {"function", "foo", ";"}),
            N(1, L(1, {"if", "(", "aa", "("}),
              N(5, L(5, {"bb", ","}), L(5, {"cc"})), L(3, {")", ")", ";"})),
            L(0, {"endfunction"})),
    },

    {
        "function with function call inside if statement header and with "
        "begin-end block",
        "function foo;"
        "if (aa(bb,cc,dd,ee))"
        "begin end "
        "endfunction",
        FunctionDeclaration(0, L(0, {"function", "foo", ";"}),
                            N(1,
                              N(1, L(1, {"if", "(", "aa", "("}),
                                N(5, L(5, {"bb", ","}), L(5, {"cc", ","}),
                                  L(5, {"dd", ","}), L(5, {"ee"})),
                                L(3, {")", ")", "begin"})),
                              L(1, {"end"})),
                            L(0, {"endfunction"})),
    },

    {
        "function with kMethodCallExtension inside if statement header and "
        "with begin-end block",
        "function foo;"
        "if (aa.bb(cc,dd,ee))"
        "begin end "
        "endfunction",
        FunctionDeclaration(
            0, L(0, {"function", "foo", ";"}),
            N(1,
              N(1, L(1, {"if", "(", "aa", ".", "bb", "("}),
                N(5, L(5, {"cc", ","}), L(5, {"dd", ","}), L(5, {"ee"})),
                L(3, {")", ")", "begin"})),
              L(1, {"end"})),
            L(0, {"endfunction"})),
    },

    {
        "nested kMethodCallExtension calls - one level",
        "function foo;"
        "aa.bb(cc.dd(a1), ee.ff(a2));"
        "endfunction",
        FunctionDeclaration(0, L(0, {"function", "foo", ";"}),
                            N(1, L(1, {"aa", ".", "bb", "("}),
                              N(3, L(3, {"cc", ".", "dd", "("}), L(5, {"a1"}),
                                L(3, {")", ","}), L(3, {"ee", ".", "ff", "("}),
                                L(5, {"a2"}), L(3, {")", ")", ";"}))),
                            L(0, {"endfunction"})),
    },

    {
        "nested kMethodCallExtension calls - two level",
        "function foo;"
        "aa.bb(cc.dd(a1.b1(a2), b1), ee.ff(c1, d1));"
        "endfunction",
        FunctionDeclaration(
            0, L(0, {"function", "foo", ";"}),
            N(1, L(1, {"aa", ".", "bb", "("}),
              N(3, L(3, {"cc", ".", "dd", "("}),
                N(5, L(5, {"a1", ".", "b1", "("}), L(7, {"a2"}),
                  L(5, {")", ","}), L(5, {"b1"})),
                L(3, {")", ","}), L(3, {"ee", ".", "ff", "("}),
                N(5, L(5, {"c1", ","}), L(5, {"d1"})), L(3, {")", ")", ";"}))),
            L(0, {"endfunction"})),
    },
};

// Test that TreeUnwrapper produces correct UnwrappedLines from function tests
TEST_F(TreeUnwrapperTest, UnwrapFunctionTests) {
  for (const auto &test_case : kUnwrapFunctionTestCases) {
    VLOG(4) << "==== kUnwrapFunctionTests ====\n" << test_case.source_code;
    auto tree_unwrapper = CreateTreeUnwrapper(test_case.source_code);
    const auto *uwline_tree = tree_unwrapper->Unwrap();
    EXPECT_TRUE(VerifyUnwrappedLines(&std::cout, *ABSL_DIE_IF_NULL(uwline_tree),
                                     test_case));
  }
}

const TreeUnwrapperTestData kUnwrapStructTestCases[] = {
    {
        "simple struct typedef one member",
        "typedef struct {int a;} foo;",
        N(0, L(0, {"typedef", "struct", "{"}), L(1, {"int", "a", ";"}),
          L(0, {"}", "foo", ";"})),
    },

    {
        "simple struct typedef multiple members",
        "typedef struct {"
        "int a;"
        "logic [3:0] b;"
        "} foo;",
        N(0, L(0, {"typedef", "struct", "{"}),
          StructUnionMemberList(
              1, L(1, {"int", "a", ";"}),
              L(1, {"logic", "[", "3", ":", "0", "]", "b", ";"})),
          L(0, {"}", "foo", ";"})),
    },
};

// Test that TreeUnwrapper produces correct UnwrappedLines from structs
TEST_F(TreeUnwrapperTest, UnwrapStructTests) {
  for (const auto &test_case : kUnwrapStructTestCases) {
    auto tree_unwrapper = CreateTreeUnwrapper(test_case.source_code);
    const auto *uwline_tree = tree_unwrapper->Unwrap();
    EXPECT_TRUE(VerifyUnwrappedLines(&std::cout, *ABSL_DIE_IF_NULL(uwline_tree),
                                     test_case));
  }
}

const TreeUnwrapperTestData kUnwrapUnionTestCases[] = {
    {
        "simple union typedef one member",
        "typedef union {int a;} foo;",
        N(0, L(0, {"typedef", "union", "{"}), L(1, {"int", "a", ";"}),
          L(0, {"}", "foo", ";"})),
    },

    {
        "simple union typedef multiple members",
        "typedef union {"
        "int a;"
        "logic [3:0] b;"
        "} foo;",
        N(0, L(0, {"typedef", "union", "{"}),
          StructUnionMemberList(
              1, L(1, {"int", "a", ";"}),
              L(1, {"logic", "[", "3", ":", "0", "]", "b", ";"})),
          L(0, {"}", "foo", ";"})),
    },
};

// Test that TreeUnwrapper produces correct UnwrappedLines from unions
TEST_F(TreeUnwrapperTest, UnwrapUnionTests) {
  for (const auto &test_case : kUnwrapUnionTestCases) {
    auto tree_unwrapper = CreateTreeUnwrapper(test_case.source_code);
    const auto *uwline_tree = tree_unwrapper->Unwrap();
    EXPECT_TRUE(VerifyUnwrappedLines(&std::cout, *ABSL_DIE_IF_NULL(uwline_tree),
                                     test_case));
  }
}

const TreeUnwrapperTestData kUnwrapEnumTestCases[] = {
    {
        "simple enum typedef, one member",
        "typedef enum { one=1 } foo_e;",
        N(0, L(0, {"typedef", "enum", "{"}), L(1, {"one", "=", "1"}),
          L(0, {"}", "foo_e", ";"})),
    },

    {
        "simple enum typedef multiple members",
        "typedef enum logic {"
        "one=1,"
        "two=2"
        "} foo_e;",
        N(0, L(0, {"typedef", "enum", "logic", "{"}),
          EnumItemList(1, L(1, {"one", "=", "1", ","}),
                       L(1, {"two", "=", "2"})),
          L(0, {"}", "foo_e", ";"})),
    },

    {
        "Comment after enum member should attach",
        "typedef enum logic {\n"
        "one=1,   // foo\n"
        "two,     // bar\n"
        "three=3  // baz\n"
        "} foo_e;",
        N(0, L(0, {"typedef", "enum", "logic", "{"}),
          EnumItemList(1, L(1, {"one", "=", "1", ",", "// foo"}),
                       L(1, {"two", ",", "// bar"}),
                       L(1, {"three", "=", "3", "// baz"})),
          L(0, {"}", "foo_e", ";"})),
    },

    {
        "In-line and single line comments should be kept",
        "typedef enum {//c1\n"
        "//c2\n"
        "one=1,  //c3\n"
        "//c4\n"
        "two=2  //c5\n"
        "//c6\n"
        "} x;\n",
        N(0, L(0, {"typedef", "enum", "{", "//c1"}),
          EnumItemList(1, L(1, {"//c2"}),                     //
                       L(1, {"one", "=", "1", ",", "//c3"}),  //
                       L(1, {"//c4"}),                        //
                       L(1, {"two", "=", "2", "//c5"}),       //
                       L(1, {"//c6"})),                       //
          L(0, {"}", "x", ";"})),
    }};

// Test that TreeUnwrapper produces correct UnwrappedLines from structs
TEST_F(TreeUnwrapperTest, UnwrapEnumTests) {
  for (const auto &test_case : kUnwrapEnumTestCases) {
    auto tree_unwrapper = CreateTreeUnwrapper(test_case.source_code);
    const auto *uwline_tree = tree_unwrapper->Unwrap();
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
        PropertyDeclaration(0, L(0, {"property", "myprop", ";"}),
                            L(1, {"a", "<", "b"}), L(0, {"endproperty"})),
    },

    {
        "simple property declaration, terminal semicolon",
        "property myprop;"
        "a < b;"
        "endproperty",
        PropertyDeclaration(0, L(0, {"property", "myprop", ";"}),
                            L(1, {"a", "<", "b", ";"}), L(0, {"endproperty"})),
    },

    {
        "simple property declaration, with assertion variable declaration",
        "property myprop;"
        "pkg::thing_t thing;"
        "a < b "
        "endproperty",
        PropertyDeclaration(0, L(0, {"property", "myprop", ";"}),
                            L(1, {"pkg", "::", "thing_t", "thing", ";"}),
                            L(1, {"a", "<", "b"}), L(0, {"endproperty"})),
    },

    {
        "simple property spec inside parentheses",
        "program tst;"
        "initial begin "
        "expect(a|=>b)xx;"
        "end "
        "endprogram",
        ModuleDeclaration(
            0,  // doubles as program declaration for now
            L(0, {"program", "tst", ";"}),
            FlowControl(1,  //
                        L(1, {"initial", "begin"}),
                        N(2,                                            //
                          L(2, {"expect", "(", "a", "|=>", "b", ")"}),  //
                          L(3, {"xx", ";"})),
                        L(1, {"end"})),
            L(0, {"endprogram"})),
    },

    {
        "two property declarations",
        "property myprop1;"
        "a < b "
        "endproperty "
        "property myprop2;"
        "a > b "
        "endproperty",
        PropertyDeclaration(0, L(0, {"property", "myprop1", ";"}),
                            L(1, {"a", "<", "b"}), L(0, {"endproperty"})),
        PropertyDeclaration(0, L(0, {"property", "myprop2", ";"}),
                            L(1, {"a", ">", "b"}), L(0, {"endproperty"})),
    },

    {
        "two property declarations, with end-labels",
        "property myprop1;"
        "a < b "
        "endproperty : myprop1 "
        "property myprop2;"
        "a > b "
        "endproperty : myprop2",
        PropertyDeclaration(0, L(0, {"property", "myprop1", ";"}),
                            L(1, {"a", "<", "b"}),
                            L(0, {"endproperty", ":", "myprop1"})),
        PropertyDeclaration(0, L(0, {"property", "myprop2", ";"}),
                            L(1, {"a", ">", "b"}),
                            L(0, {"endproperty", ":", "myprop2"})),
    },

    {
        "simple property declaration, two ports",
        "property myprop(int foo, int port);"
        "a < b "
        "endproperty",
        PropertyDeclaration(0,
                            L(0, {"property", "myprop", "(", "int", "foo", ",",
                                  "int", "port", ")", ";"}),
                            L(1, {"a", "<", "b"}), L(0, {"endproperty"})),
    },

    {
        "property declaration inside package",
        "package pkg;"
        "property myprop;"
        "a < b "
        "endproperty "
        "endpackage",
        PackageDeclaration(
            0, L(0, {"package", "pkg", ";"}),
            PropertyDeclaration(1, L(1, {"property", "myprop", ";"}),
                                L(2, {"a", "<", "b"}), L(1, {"endproperty"})),
            L(0, {"endpackage"})),
    },

    {
        "property declaration inside module",
        "module pkg;"
        "property myprop;"
        "a < b "
        "endproperty "
        "endmodule",
        ModuleDeclaration(
            0, L(0, {"module", "pkg", ";"}),
            PropertyDeclaration(1, L(1, {"property", "myprop", ";"}),
                                L(2, {"a", "<", "b"}), L(1, {"endproperty"})),
            L(0, {"endmodule"})),
    },
    /* TODO(b/145241765): fix property-case parsing
    {
        "property declaration with property case statement",
        "module m;"
        "property p;"
        "case (g) h:a < b; i:c<d endcase "
        "endproperty "
        "endmodule",
        ModuleDeclaration(0,
        ModuleHeader(0, L(0, {"module", "m", ";"})),
        PropertyDeclaration(1, L(1, {"property", "p", ";"}),
                       PropertyItemList(
                           2, L(2, {"case", "(", "g", ")"}),
                           CaseItemList(3, L(3, {"h", ":", "a", "<", "b", ";"}),
                                        L(3, {"i", ":", "c", "<", "d", ";"})),
                           L(2, {"endcase"})),
                       L(1, {"endproperty"})),
        L(0, {"endmodule"})),
    },
    */
};

// Test that TreeUnwrapper produces correct UnwrappedLines from properties
TEST_F(TreeUnwrapperTest, UnwrapPropertyTests) {
  for (const auto &test_case : kUnwrapPropertyTestCases) {
    auto tree_unwrapper = CreateTreeUnwrapper(test_case.source_code);
    const auto *uwline_tree = tree_unwrapper->Unwrap();
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
        CovergroupDeclaration(
            0,
            CovergroupHeader(0, L(0, {"covergroup", "cg", "("}),
                             L(2, {"string", "s"}), L(0, {")", ";"})),
            L(0, {"endgroup"})),
        CovergroupDeclaration(
            0,
            CovergroupHeader(0, L(0, {"covergroup", "cg2", "("}),
                             L(2, {"string", "s"}), L(0, {")", ";"})),
            L(0, {"endgroup"})),
    },

    {
        "covergroup declaration with options",
        "covergroup cg(string s);"
        "option.name = cg_name;"
        "option.per_instance=1;"
        "endgroup ",
        CovergroupDeclaration(
            0,
            CovergroupHeader(0, L(0, {"covergroup", "cg", "("}),
                             L(2, {"string", "s"}), L(0, {")", ";"})),
            CovergroupItemList(
                1, L(1, {"option", ".", "name", "=", "cg_name", ";"}),
                L(1, {"option", ".", "per_instance", "=", "1", ";"})),
            L(0, {"endgroup"})),
    },

    {
        "covergroup declaration with coverpoints",
        "covergroup cg(string s);"
        "q_cp : coverpoint cp;"
        "q_cp2 : coverpoint cp2;"
        "endgroup ",
        CovergroupDeclaration(
            0,
            CovergroupHeader(0, L(0, {"covergroup", "cg", "("}),
                             L(2, {"string", "s"}), L(0, {")", ";"})),
            CovergroupItemList(1, L(1, {"q_cp", ":", "coverpoint", "cp", ";"}),
                               L(1, {"q_cp2", ":", "coverpoint", "cp2", ";"})),
            L(0, {"endgroup"})),
    },

    {
        "coverpoint with bins",
        "covergroup cg(string s);"
        "q_cp : coverpoint cp {"
        "  bins foo = {bar};"
        "  bins zoo = {pig};"
        "}"
        "endgroup ",
        CovergroupDeclaration(
            0,
            CovergroupHeader(0, L(0, {"covergroup", "cg", "("}),
                             L(2, {"string", "s"}), L(0, {")", ";"})),
            CovergroupItemList(
                1, L(1, {"q_cp", ":", "coverpoint", "cp", "{"}),
                CoverpointItemList(
                    2, L(2, {"bins", "foo", "=", "{", "bar", "}", ";"}),
                    L(2, {"bins", "zoo", "=", "{", "pig", "}", ";"})),
                L(1, {"}"})),
            L(0, {"endgroup"})),
    },

    {
        "covergroup declaration with crosses",
        "covergroup cg(string s);"
        "x_cross : cross s1, s2;"
        "x_cross2 : cross s2, s1;"
        "endgroup ",
        CovergroupDeclaration(
            0,
            CovergroupHeader(0, L(0, {"covergroup", "cg", "("}),
                             L(2, {"string", "s"}), L(0, {")", ";"})),
            CovergroupItemList(
                1, L(1, {"x_cross", ":", "cross", "s1", ",", "s2", ";"}),
                L(1, {"x_cross2", ":", "cross", "s2", ",", "s1", ";"})),
            L(0, {"endgroup"})),
    },

    {
        "cover crosses with bins",
        "covergroup cg(string s);"
        "x_cross : cross s1, s2{"
        "  bins a = binsof(x) intersect {d};"
        "  bins b = binsof(y) intersect {e, f};"
        "}"
        "endgroup ",
        CovergroupDeclaration(
            0,
            CovergroupHeader(0, L(0, {"covergroup", "cg", "("}),
                             L(2, {"string", "s"}), L(0, {")", ";"})),
            CovergroupItemList(
                1, L(1, {"x_cross", ":", "cross", "s1", ",", "s2", "{"}),
                CrossItemList(
                    2,
                    N(2,
                      L(2, {"bins", "a", "=", "binsof", "(", "x", ")",
                            "intersect", "{"}),
                      L(3, {"d"}), L(2, {"}", ";"})),
                    N(2,
                      L(2, {"bins", "b", "=", "binsof", "(", "y", ")",
                            "intersect", "{"}),
                      N(3, L(3, {"e", ","}), L(3, {"f"})), L(2, {"}", ";"}))),
                L(1, {"}"})),
            L(0, {"endgroup"})),
    },
    {
        "covergroup declaration with a function",
        "covergroup cg(string s) with function sample(bit pending);"
        "endgroup ",
        CovergroupDeclaration(
            0,
            CovergroupHeader(0, L(0, {"covergroup", "cg", "("}),
                             L(2, {"string", "s"}),
                             L(0, {")", "with", "function", "sample", "("}),
                             L(2, {"bit", "pending"}), L(0, {")", ";"})),
            L(0, {"endgroup"})),
    },
};

// Test that TreeUnwrapper produces correct UnwrappedLines from covergroups
TEST_F(TreeUnwrapperTest, UnwrapCovergroupTests) {
  for (const auto &test_case : kUnwrapCovergroupTestCases) {
    auto tree_unwrapper = CreateTreeUnwrapper(test_case.source_code);
    const auto *uwline_tree = tree_unwrapper->Unwrap();
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
        SequenceDeclaration(0, L(0, {"sequence", "myseq", ";"}),
                            L(1, {"a", "<", "b"}), L(0, {"endsequence"})),
    },

    {
        "simple sequence declaration, terminal semicolon",
        "sequence myseq;"
        "a < b;"
        "endsequence",
        SequenceDeclaration(0, L(0, {"sequence", "myseq", ";"}),
                            L(1, {"a", "<", "b", ";"}), L(0, {"endsequence"})),
    },

    {
        "simple sequence declaration, with assertion variable declaration",
        "sequence myseq;"
        "foo bar;"
        "a < b "
        "endsequence",
        SequenceDeclaration(0, L(0, {"sequence", "myseq", ";"}),
                            L(1, {"foo", "bar", ";"}), L(1, {"a", "<", "b"}),
                            L(0, {"endsequence"})),
    },

    {
        "two sequence declarations",
        "sequence myseq;"
        "a < b "
        "endsequence "
        "sequence myseq2;"
        "a > b "
        "endsequence",
        SequenceDeclaration(0, L(0, {"sequence", "myseq", ";"}),
                            L(1, {"a", "<", "b"}), L(0, {"endsequence"})),
        SequenceDeclaration(0, L(0, {"sequence", "myseq2", ";"}),
                            L(1, {"a", ">", "b"}), L(0, {"endsequence"})),
    },

    {
        "two sequence declarations, with end labels",
        "sequence myseq;"
        "a < b "
        "endsequence : myseq "
        "sequence myseq2;"
        "a > b "
        "endsequence : myseq2",
        SequenceDeclaration(0, L(0, {"sequence", "myseq", ";"}),
                            L(1, {"a", "<", "b"}),
                            L(0, {"endsequence", ":", "myseq"})),
        SequenceDeclaration(0, L(0, {"sequence", "myseq2", ";"}),
                            L(1, {"a", ">", "b"}),
                            L(0, {"endsequence", ":", "myseq2"})),
    },
};

// Test that TreeUnwrapper produces correct UnwrappedLines from sequences
TEST_F(TreeUnwrapperTest, UnwrapSequenceTests) {
  for (const auto &test_case : kUnwrapSequenceTestCases) {
    auto tree_unwrapper = CreateTreeUnwrapper(test_case.source_code);
    const auto *uwline_tree = tree_unwrapper->Unwrap();
    EXPECT_TRUE(VerifyUnwrappedLines(&std::cout, *ABSL_DIE_IF_NULL(uwline_tree),
                                     test_case));
  }
}

const TreeUnwrapperTestData kUnwrapPrimitivesTestCases[] = {
    {
        "one input combinatorial UDP",
        "primitive comb(o, i);\n"
        "  output o;\n"
        "  input i;\n"
        "  table\n"
        "    1 : 0;\n"
        "    0 : 1;\n"
        "  endtable\n"
        "endprimitive",
        UDPDeclaration(
            0, L(0, {"primitive", "comb", "(", "o", ",", "i", ")", ";"}),
            L(1, {"output", "o", ";"}), L(1, {"input", "i", ";"}),
            UdpBody(1, L(1, {"table"}), L(2, {"1", ":", "0", ";"}),
                    L(2, {"0", ":", "1", ";"}), L(1, {"endtable"})),
            L(0, {"endprimitive"})),
    },
    {
        "double input UDP",
        "primitive comb2(o, s, r); "
        "output o; "
        "input s; "
        "input r; "
        "table "
        "1 ? : 0; "
        "? 1 : 1; "
        "endtable "
        "endprimitive ",
        UDPDeclaration(0,
                       L(0, {"primitive", "comb2", "(", "o", ",", "s", ",", "r",
                             ")", ";"}),
                       L(1, {"output", "o", ";"}), L(1, {"input", "s", ";"}),
                       L(1, {"input", "r", ";"}),
                       UdpBody(1,                                //
                               L(1, {"table"}),                  //
                               L(2, {"1", "?", ":", "0", ";"}),  //
                               L(2, {"?", "1", ":", "1", ";"}),  //
                               L(1, {"endtable"})),
                       L(0, {"endprimitive"})),
    },
    {
        "double input UDP with comments",
        "primitive comb2(o, s, r); "
        "output /* only one */ o;\n"
        "// inputs section\n"
        "input s; "
        "input r; // two of them\n"
        "table "
        "1 ? : 0; "
        "? 1 : 1; "
        "endtable "
        "endprimitive ",
        UDPDeclaration(
            0,
            L(0,
              {"primitive", "comb2", "(", "o", ",", "s", ",", "r", ")", ";"}),
            L(1, {"output", "/* only one */", "o", ";"}),
            N(1, L(1, {"// inputs section"}), L(1, {"input", "s", ";"})),
            L(1, {"input", "r", ";", "// two of them"}),
            UdpBody(1,                                //
                    L(1, {"table"}),                  //
                    L(2, {"1", "?", ":", "0", ";"}),  //
                    L(2, {"?", "1", ":", "1", ";"}),  //
                    L(1, {"endtable"})),
            L(0, {"endprimitive"})),
    },
    {
        "10-input UDP",
        "primitive comb10(o, i0, i1, i2, i3, i4, i5, i6, i7, i8, i9); "
        "  output o; "
        "  input i0, i1, i2, i3, i4, i5, i6, i7, i8, i9; "
        "  table "
        "    0 ? ? ? ? ? ? ? ? 0 : 0;"
        "    1 ? ? ? ? ? ? ? ? 0 : 1;"
        "    1 ? ? ? ? ? ? ? ? 1 : 1;"
        "    0 ? ? ? ? ? ? ? ? 1 : 0;"
        "  endtable "
        "endprimitive ",
        UDPDeclaration(
            0, L(0, {"primitive", "comb10", "(",  "o",  ",",  "i0", ",",
                     "i1",        ",",      "i2", ",",  "i3", ",",  "i4",
                     ",",         "i5",     ",",  "i6", ",",  "i7", ",",
                     "i8",        ",",      "i9", ")",  ";"}),
            L(1, {"output", "o", ";"}),
            L(1, {"input", "i0", ",",  "i1", ",",  "i2", ",",
                  "i3",    ",",  "i4", ",",  "i5", ",",  "i6",
                  ",",     "i7", ",",  "i8", ",",  "i9", ";"}),
            UdpBody(1, L(1, {"table"}),
                    L(2, {"0", "?", "?", "?", "?", "?", "?", "?", "?", "0", ":",
                          "0", ";"}),
                    L(2, {"1", "?", "?", "?", "?", "?", "?", "?", "?", "0", ":",
                          "1", ";"}),
                    L(2, {"1", "?", "?", "?", "?", "?", "?", "?", "?", "1", ":",
                          "1", ";"}),
                    L(2, {"0", "?", "?", "?", "?", "?", "?", "?", "?", "1", ":",
                          "0", ";"}),
                    L(1, {"endtable"})),
            L(0, {"endprimitive"})),
    },
    {
        "level-sensitive sequential UDP",
        "primitive level_seq(o, s, r); "
        "output o; "
        "reg o; "
        "input s; "
        "input r; "
        "table "
        "1 ? : ? : 0; "
        "? 1 : 0 : -; "
        "endtable "
        "endprimitive ",
        UDPDeclaration(0,
                       L(0, {"primitive", "level_seq", "(", "o", ",", "s", ",",
                             "r", ")", ";"}),
                       L(1, {"output", "o", ";"}), L(1, {"reg", "o", ";"}),
                       L(1, {"input", "s", ";"}), L(1, {"input", "r", ";"}),
                       UdpBody(1, L(1, {"table"}),                         //
                               L(2, {"1", "?", ":", "?", ":", "0", ";"}),  //
                               L(2, {"?", "1", ":", "0", ":", "-", ";"}),  //
                               L(1, {"endtable"})),
                       L(0, {"endprimitive"})),
    },
    {
        "sequential UDP with comments",
        "primitive level_seq(o, s, r); "
        "output o; "
        "reg o; "
        "input s; "
        "input r; "
        "table\n"
        "// r s state next\n"
        "1 /* rst */ ? : ? : 0; "
        "? 1 /* set */ : 0 : -; // no change here\n"
        "endtable "
        "endprimitive ",
        UDPDeclaration(
            0,
            L(0, {"primitive", "level_seq", "(", "o", ",", "s", ",", "r", ")",
                  ";"}),
            L(1, {"output", "o", ";"}), L(1, {"reg", "o", ";"}),
            L(1, {"input", "s", ";"}), L(1, {"input", "r", ";"}),
            UdpBody(1, L(1, {"table"}),             //
                    N(2,                            //
                      L(2, {"// r s state next"}),  //
                      L(2, {"1", "/* rst */", "?", ":", "?", ":", "0", ";"})),
                    L(2, {"?", "1", "/* set */", ":", "0", ":", "-", ";",
                          "// no change here"}),
                    L(1, {"endtable"})),
            L(0, {"endprimitive"})),
    },
    {
        "edge-sensitive sequential UDP",
        "primitive edge_seq(o, c, d); "
        "  output o; "
        "  reg o; "
        "  input c; "
        "  input d; "
        "  table "
        "      (01) 0 : ? :  0; "
        "      (01) 1 : ? :  1; "
        "      (0?) 1 : 1 :  1; "
        "      (0?) 0 : 0 :  0; "
        "      (?0) ? : ? :  -; "
        "       ?  (?\?) : ? :  -; "
        "  endtable "
        "endprimitive ",
        UDPDeclaration(
            0,
            L(0, {"primitive", "edge_seq", "(", "o", ",", "c", ",", "d", ")",
                  ";"}),
            L(1, {"output", "o", ";"}), L(1, {"reg", "o", ";"}),
            L(1, {"input", "c", ";"}), L(1, {"input", "d", ";"}),
            UdpBody(1, L(1, {"table"}),  //
                    L(2, {"(01)", "0", ":", "?", ":", "0", ";"}),
                    L(2, {"(01)", "1", ":", "?", ":", "1", ";"}),
                    L(2, {"(0?)", "1", ":", "1", ":", "1", ";"}),
                    L(2, {"(0?)", "0", ":", "0", ":", "0", ";"}),
                    L(2, {"(?0)", "?", ":", "?", ":", "-", ";"}),
                    L(2, {"?", "(?\?)", ":", "?", ":", "-", ";"}),  //
                    L(1, {"endtable"})),
            L(0, {"endprimitive"})),
    },
    {
        "mixed sensitivity sequential UDP",
        "primitive mixed(o, clk, j, k, preset, clear); "
        "  output o; "
        "  reg o; "
        "  input c; "
        "  input j, k; "
        "  input preset, clear; "
        "  table "
        "    ?  ??  01  : ? :  1 ; "
        "    ?  ??  *1  : 1 :  1 ; "
        "    ?  ??  10  : ? :  0 ; "
        "    ?  ??  1*  : 0 :  0 ; "
        "    r  00  00  : 0 :  1 ; "
        "    r  00  11  : ? :  - ; "
        "    r  01  11  : ? :  0 ; "
        "    r  10  11  : ? :  1 ; "
        "    r  11  11  : 0 :  1 ; "
        "    r  11  11  : 1 :  0 ; "
        "    f  ??  ??  : ? :  - ; "
        "    b  *?  ??  : ? :  - ; "
        "    b  ?*  ??  : ? :  - ; "
        "  endtable "
        "endprimitive ",
        UDPDeclaration(
            0,
            L(0, {"primitive", "mixed", "(", "o", ",", "clk", ",", "j", ",",
                  "k", ",", "preset", ",", "clear", ")", ";"}),
            L(1, {"output", "o", ";"}), L(1, {"reg", "o", ";"}),
            L(1, {"input", "c", ";"}), L(1, {"input", "j", ",", "k", ";"}),
            L(1, {"input", "preset", ",", "clear", ";"}),
            UdpBody(1, L(1, {"table"}),  //
                    L(2, {"?", "?", "?", "0", "1", ":", "?", ":", "1", ";"}),
                    L(2, {"?", "?", "?", "*", "1", ":", "1", ":", "1", ";"}),
                    L(2, {"?", "?", "?", "1", "0", ":", "?", ":", "0", ";"}),
                    L(2, {"?", "?", "?", "1", "*", ":", "0", ":", "0", ";"}),
                    L(2, {"r", "0", "0", "0", "0", ":", "0", ":", "1", ";"}),
                    L(2, {"r", "0", "0", "1", "1", ":", "?", ":", "-", ";"}),
                    L(2, {"r", "0", "1", "1", "1", ":", "?", ":", "0", ";"}),
                    L(2, {"r", "1", "0", "1", "1", ":", "?", ":", "1", ";"}),
                    L(2, {"r", "1", "1", "1", "1", ":", "0", ":", "1", ";"}),
                    L(2, {"r", "1", "1", "1", "1", ":", "1", ":", "0", ";"}),
                    L(2, {"f", "?", "?", "?", "?", ":", "?", ":", "-", ";"}),
                    L(2, {"b", "*", "?", "?", "?", ":", "?", ":", "-", ";"}),
                    L(2, {"b", "?", "*", "?", "?", ":", "?", ":", "-", ";"}),
                    L(1, {"endtable"})),
            L(0, {"endprimitive"})),
    },

    // primitive gate instantiation tests
    {
        "single primitive gate instantiation",
        "module m;\n"
        "xor x0(a, b, c);\n"
        "endmodule\n",
        ModuleDeclaration(0, L(0, {"module", "m", ";"}),
                          N(1,                         //
                            L(1, {"xor", "x0", "("}),  //
                            N(3,                       //
                              L(3, {"a", ","}),        //
                              L(3, {"b", ","}),        //
                              L(3, {"c"})),            //
                            L(1, {")", ";"})),
                          L(0, {"endmodule"})),
    },
    {
        "two primitive gate instantiations",
        "module m;\n"
        "and x0(a, b, c);\n"
        "or x1(a, b, d);\n"
        "endmodule\n",
        ModuleDeclaration(0, L(0, {"module", "m", ";"}),
                          ModuleItemList(1,
                                         N(1,                         //
                                           L(1, {"and", "x0", "("}),  //
                                           N(3,                       //
                                             L(3, {"a", ","}),        //
                                             L(3, {"b", ","}),        //
                                             L(3, {"c"})),            //
                                           L(1, {")", ";"})),
                                         N(1,                        //
                                           L(1, {"or", "x1", "("}),  //
                                           N(3,                      //
                                             L(3, {"a", ","}),       //
                                             L(3, {"b", ","}),       //
                                             L(3, {"d"})),           //
                                           L(1, {")", ";"}))),
                          L(0, {"endmodule"})),
    },
};

// Test that TreeUnwrapper produces correct UnwrappedLines from primitives
TEST_F(TreeUnwrapperTest, UnwrapPrimitivesTests) {
  for (const auto &test_case : kUnwrapPrimitivesTestCases) {
    auto tree_unwrapper = CreateTreeUnwrapper(test_case.source_code);
    const auto *uwline_tree = tree_unwrapper->Unwrap();
    EXPECT_TRUE(VerifyUnwrappedLines(&std::cout, *ABSL_DIE_IF_NULL(uwline_tree),
                                     test_case));
  }
}
}  // namespace
}  // namespace formatter
}  // namespace verilog
