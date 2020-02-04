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

#include "verilog/formatting/token_annotator.h"

#include <algorithm>
#include <initializer_list>
#include <iterator>
#include <ostream>
#include <vector>

#include "gtest/gtest.h"
#include "absl/strings/str_cat.h"
#include "absl/strings/string_view.h"
#include "common/formatting/format_token.h"
#include "common/formatting/unwrapped_line.h"
#include "common/formatting/unwrapped_line_test_utils.h"
#include "common/text/syntax_tree_context.h"
#include "common/text/token_info_test_util.h"
#include "common/text/tree_builder_test_util.h"
#include "common/util/casts.h"
#include "common/util/iterator_adaptors.h"
#include "verilog/CST/verilog_nonterminals.h"
#include "verilog/formatting/format_style.h"
#include "verilog/formatting/verilog_token.h"
#include "verilog/parser/verilog_parser.h"
#include "verilog/parser/verilog_token_enum.h"

namespace verilog {
namespace formatter {

using ::verible::InterTokenInfo;
using ::verible::PreFormatToken;
using ::verible::SpacingOptions;

// Private function with external linkage from token_annotator.cc.
extern void AnnotateFormatToken(const FormatStyle& style,
                                const PreFormatToken& prev_token,
                                PreFormatToken* curr_token,
                                const verible::SyntaxTreeContext& context);

namespace {

// TODO(fangism): Move much of this boilerplate to format_token_test_util.h.

// This test structure is a subset of InterTokenInfo.
// We do not want to compare break penalties, because that would be too
// change-detector-y.
struct ExpectedInterTokenInfo {
  constexpr ExpectedInterTokenInfo(int spaces, const SpacingOptions& bd)
      : spaces_required(spaces), break_decision(bd) {}

  int spaces_required = 0;
  SpacingOptions break_decision = SpacingOptions::Undecided;

  bool operator==(const InterTokenInfo& before) const {
    return spaces_required == before.spaces_required &&
           break_decision == before.break_decision;
  }

  bool operator!=(const InterTokenInfo& before) const {
    return !(*this == before);
  }
};

std::ostream& operator<<(std::ostream& stream,
                         const ExpectedInterTokenInfo& t) {
  stream << "{\n  spaces_required: " << t.spaces_required
         << "\n  break_decision: " << t.break_decision << "\n}";
  return stream;
}

// Returns false if all ExpectedFormattingCalculations are not equal and outputs
// the first difference.
// type T is any container or range over PreFormatTokens.
template <class T>
bool CorrectExpectedFormatTokens(
    const std::vector<ExpectedInterTokenInfo>& expected, const T& tokens) {
  EXPECT_EQ(expected.size(), tokens.size())
      << "Size of expected calculations and format tokens does not match.";
  if (expected.size() != tokens.size()) {
    return false;
  }

  const auto first_mismatch =
      std::mismatch(expected.cbegin(), expected.cend(), tokens.begin(),
                    [](const ExpectedInterTokenInfo& expected,
                       const PreFormatToken& token) -> bool {
                      return expected == token.before;
                    });
  const bool all_match = first_mismatch.first == expected.cend();
  const int mismatch_position =
      std::distance(expected.begin(), first_mismatch.first);
  EXPECT_TRUE(all_match) << "mismatch at [" << mismatch_position
                         << "]: " << *first_mismatch.second->token
                         << "\nexpected: " << *first_mismatch.first
                         << "\ngot: " << first_mismatch.second->before;
  return all_match;
}

struct AnnotateFormattingInformationTestCase {
  FormatStyle style;
  int uwline_indentation;
  std::initializer_list<ExpectedInterTokenInfo> expected_calculations;
  // This exists for the sake of forwarding to the UnwrappedLineMemoryHandler.
  // When passing token sequences for testing, use the tokens that are
  // recomputed in the UnwrappedLineMemoryHandler, which re-arranges
  // tokens' text into a contiguous string buffer in memory.
  std::initializer_list<verible::TokenInfo> input_tokens;

  // TODO(fangism): static_assert(expected_calculations.size() ==
  //                              input_tokens.size());
  //     or restructure using std::pair.
};

// Print input tokens' text for debugging.
std::ostream& operator<<(
    std::ostream& stream,
    const AnnotateFormattingInformationTestCase& test_case) {
  stream << '[';
  for (const auto& token : test_case.input_tokens) stream << ' ' << token.text;
  return stream << " ]";
}

// Pre-populates context stack for testing context-sensitive annotations.
// TODO(fangism): This class is easily made language-agnostic, and could
// move into a _test_util library.
class InitializedSyntaxTreeContext : public verible::SyntaxTreeContext {
 public:
  InitializedSyntaxTreeContext(std::initializer_list<NodeEnum> ancestors) {
    // Build up a "skinny" tree from the bottom-up, much like the parser does.
    std::vector<verible::SyntaxTreeNode*> parents;
    parents.reserve(ancestors.size());
    for (const auto ancestor : verible::reversed_view(ancestors)) {
      if (root_ == nullptr) {
        root_ = verible::MakeTaggedNode(ancestor);
      } else {
        root_ = verible::MakeTaggedNode(ancestor, root_);
      }
      parents.push_back(ABSL_DIE_IF_NULL(
          verible::down_cast<verible::SyntaxTreeNode*>(root_.get())));
    }
    for (const auto* parent : verible::reversed_view(parents)) {
      Push(*parent);
    }
  }

 private:
  // Syntax tree synthesized from sequence of node enums.
  verible::SymbolPtr root_;
};

std::ostream& operator<<(std::ostream& stream,
                         const InitializedSyntaxTreeContext& context) {
  stream << "[ ";
  for (const auto* node : verible::make_range(context.begin(), context.end())) {
    stream << NodeEnumToString(NodeEnum(ABSL_DIE_IF_NULL(node)->Tag().tag))
           << " ";
  }
  return stream << ']';
}

struct AnnotateWithContextTestCase {
  FormatStyle style;
  verible::TokenInfo left_token;
  verible::TokenInfo right_token;
  InitializedSyntaxTreeContext context;
  ExpectedInterTokenInfo expected_annotation;
};

const FormatStyle DefaultStyle;

constexpr int kUnhandledSpaces = 1;
constexpr ExpectedInterTokenInfo kUnhandledSpacing{kUnhandledSpaces,
                                                   SpacingOptions::Preserve};

// This test is going to ensure that given an UnwrappedLine, the format
// tokens are propagated with the correct annotations and spaces_required.
// SpacingOptions::Preserve implies that the particular token pair combination
// was not explicitly handled and just defaulted.
// This test covers cases that are not context-sensitive.
TEST(TokenAnnotatorTest, AnnotateFormattingInfoTest) {
  const std::initializer_list<AnnotateFormattingInformationTestCase>
      kTestCases = {
          // (empty array of tokens)
          {DefaultStyle, 0, {}, {}},

          // //comment1
          // //comment2
          {DefaultStyle,
           0,
           // ExpectedInterTokenInfo:
           // spaces_required, break_decision
           {{0, SpacingOptions::Undecided},  //
            {2, SpacingOptions::MustWrap}},
           {{yytokentype::TK_EOL_COMMENT, "//comment1"},
            {yytokentype::TK_EOL_COMMENT, "//comment2"}}},

          // If there is no newline before comment, it will be appended
          // (  //comment
          {DefaultStyle,
           0,
           {{0, SpacingOptions::Undecided},  //
            {2, SpacingOptions::MustAppend}},
           {{'(', "("}, {yytokentype::TK_EOL_COMMENT, "//comment"}}},

          // [  //comment
          {DefaultStyle,
           0,
           {{0, SpacingOptions::Undecided},  //
            {2, SpacingOptions::MustAppend}},
           {{'[', "["}, {yytokentype::TK_EOL_COMMENT, "//comment"}}},

          // {  //comment
          {DefaultStyle,
           0,
           {{0, SpacingOptions::Undecided},  //
            {2, SpacingOptions::MustAppend}},
           {{'{', "{"}, {yytokentype::TK_EOL_COMMENT, "//comment"}}},

          // ,  //comment
          {DefaultStyle,
           0,
           {{0, SpacingOptions::Undecided},  //
            {2, SpacingOptions::MustAppend}},
           {{',', ","}, {yytokentype::TK_EOL_COMMENT, "//comment"}}},

          // ;  //comment
          {DefaultStyle,
           0,
           {{0, SpacingOptions::Undecided},  //
            {2, SpacingOptions::MustAppend}},
           {{';', ";"}, {yytokentype::TK_EOL_COMMENT, "//comment"}}},

          // module foo();
          {DefaultStyle,
           0,
           {{0, SpacingOptions::Undecided},
            {1, SpacingOptions::Undecided},
            {0, SpacingOptions::Undecided},
            {0, SpacingOptions::Undecided},
            {0, SpacingOptions::Undecided}},
           {{yytokentype::TK_module, "module"},
            {yytokentype::SymbolIdentifier, "foo"},
            {'(', "("},
            {')', ")"},
            {';', ";"}}},

          // module foo(a, b);
          {DefaultStyle,
           0,
           {{0, SpacingOptions::Undecided},
            {1, SpacingOptions::Undecided},
            {0, SpacingOptions::Undecided},
            {0, SpacingOptions::Undecided},  // "a"
            {0, SpacingOptions::Undecided},  // ','
            {1, SpacingOptions::Undecided},  // "b"
            {0, SpacingOptions::Undecided},
            {0, SpacingOptions::Undecided}},
           {{yytokentype::TK_module, "module"},
            {yytokentype::SymbolIdentifier, "foo"},
            {'(', "("},
            {yytokentype::SymbolIdentifier, "a"},
            {',', ","},
            {yytokentype::SymbolIdentifier, "b"},
            {')', ")"},
            {';', ";"}}},

          // module with_params #() ();
          {DefaultStyle,
           0,
           {{0, SpacingOptions::Undecided},
            {1, SpacingOptions::Undecided},   // with_params
            {1, SpacingOptions::Undecided},   // #
            {0, SpacingOptions::Undecided},   // (
            {0, SpacingOptions::Undecided},   // )
            {1, SpacingOptions::Undecided},   // (
            {0, SpacingOptions::Undecided},   // )
            {0, SpacingOptions::Undecided}},  // ;
           {{yytokentype::TK_module, "module"},
            {yytokentype::SymbolIdentifier, "with_params"},
            {'#', "#"},
            {'(', "("},
            {')', ")"},
            {'(', "("},
            {')', ")"},
            {';', ";"}}},

          // a = b[c];
          {DefaultStyle,
           0,
           {{0, SpacingOptions::Undecided},
            {1, SpacingOptions::Undecided},
            {1, SpacingOptions::Undecided},
            {0, SpacingOptions::Undecided},
            {0, SpacingOptions::Undecided},
            {0, SpacingOptions::Undecided},
            {0, SpacingOptions::Undecided}},
           {{yytokentype::SymbolIdentifier, "a"},
            {'=', "="},
            {yytokentype::SymbolIdentifier, "b"},
            {'[', "["},
            {yytokentype::SymbolIdentifier, "c"},
            {']', "]"},
            {';', ";"}}},

          // b[c][d] (multi-dimensional spacing)
          {DefaultStyle,
           0,
           {{0, SpacingOptions::Undecided},
            {0, SpacingOptions::Undecided},
            {0, SpacingOptions::Undecided},
            {0, SpacingOptions::Undecided},
            {0, SpacingOptions::Undecided},
            {0, SpacingOptions::Undecided},
            {0, SpacingOptions::Undecided}},
           {{yytokentype::SymbolIdentifier, "b"},
            {'[', "["},
            {yytokentype::SymbolIdentifier, "c"},
            {']', "]"},
            {'[', "["},
            {yytokentype::SymbolIdentifier, "d"},
            {']', "]"}}},

          // always @(posedge clk)
          {DefaultStyle,
           0,
           {{0, SpacingOptions::Undecided},   // always
            {1, SpacingOptions::Undecided},   // @
            {0, SpacingOptions::Undecided},   // (
            {0, SpacingOptions::Undecided},   // posedge
            {1, SpacingOptions::Undecided},   // clk
            {0, SpacingOptions::Undecided}},  // )
           {{yytokentype::TK_always, "always"},
            {'@', "@"},
            {'(', "("},
            {yytokentype::TK_posedge, "TK_posedge"},
            {yytokentype::SymbolIdentifier, "clk"},
            {')', ")"}}},

          // `WIDTH'(s) (casting operator)
          {DefaultStyle,
           0,
           {{0, SpacingOptions::Undecided},
            {0, SpacingOptions::Undecided},
            {0, SpacingOptions::Undecided},
            {0, SpacingOptions::Undecided},
            {0, SpacingOptions::Undecided}},
           {{yytokentype::MacroIdItem, "`WIDTH"},
            {'\'', "'"},
            {'(', "("},
            {yytokentype::SymbolIdentifier, "s"},
            {')', ")"}}},

          // string'(s) (casting operator)
          {DefaultStyle,
           0,
           {{0, SpacingOptions::Undecided},
            {0, SpacingOptions::Undecided},
            {0, SpacingOptions::Undecided},
            {0, SpacingOptions::Undecided},
            {0, SpacingOptions::Undecided}},
           {{yytokentype::TK_string, "string"},
            {'\'', "'"},
            {'(', "("},
            {yytokentype::SymbolIdentifier, "s"},
            {')', ")"}}},

          // void'(f()) (casting operator)
          {DefaultStyle,
           0,
           {{0, SpacingOptions::Undecided},
            {0, SpacingOptions::Undecided},
            {0, SpacingOptions::Undecided},
            {0, SpacingOptions::Undecided},
            {0, SpacingOptions::Undecided},
            {0, SpacingOptions::Undecided},
            {0, SpacingOptions::Undecided}},
           {{yytokentype::TK_void, "void"},
            {'\'', "'"},
            {'(', "("},
            {yytokentype::SymbolIdentifier, "f"},
            {'(', "("},
            {')', ")"},
            {')', ")"}}},

          // 12'{34}
          {DefaultStyle,
           0,
           {{0, SpacingOptions::Undecided},
            {0, SpacingOptions::Undecided},
            {0, SpacingOptions::Undecided},
            {0, SpacingOptions::Undecided},
            {0, SpacingOptions::Undecided}},
           {{yytokentype::TK_DecNumber, "12"},
            {'\'', "'"},
            {'{', "{"},
            {yytokentype::TK_DecNumber, "34"},
            {'}', "}"}}},

          // k()'(s) (casting operator)
          {DefaultStyle,
           0,
           {{0, SpacingOptions::Undecided},
            {0, SpacingOptions::Undecided},
            {0, SpacingOptions::Undecided},
            {0, SpacingOptions::Undecided},
            {0, SpacingOptions::Undecided},
            {0, SpacingOptions::Undecided},
            {0, SpacingOptions::Undecided}},
           {{yytokentype::SymbolIdentifier, "k"},
            {'(', "("},
            {')', ")"},
            {'\'', "'"},
            {'(', "("},
            {yytokentype::SymbolIdentifier, "s"},
            {')', ")"}}},

          // #1 $display
          {
              DefaultStyle,
              0,
              {{0, SpacingOptions::Undecided},
               {0, SpacingOptions::Undecided},
               {1, SpacingOptions::Undecided}},
              {{'#', "#"},
               {yytokentype::TK_DecNumber, "1"},
               {yytokentype::SystemTFIdentifier, "$display"}},
          },

          // 666 777
          {
              DefaultStyle,
              0,
              {{0, SpacingOptions::Undecided}, {1, SpacingOptions::Undecided}},
              {{yytokentype::TK_DecNumber, "666"},
               {yytokentype::TK_DecNumber, "777"}},
          },

          // 5678 dance
          {
              DefaultStyle,
              0,
              {{0, SpacingOptions::Undecided}, {1, SpacingOptions::Undecided}},
              {{yytokentype::TK_DecNumber, "5678"},
               {yytokentype::SymbolIdentifier, "dance"}},
          },

          // id 4321
          {
              DefaultStyle,
              0,
              {{0, SpacingOptions::Undecided}, {1, SpacingOptions::Undecided}},
              {{yytokentype::SymbolIdentifier, "id"},
               {yytokentype::TK_DecNumber, "4321"}},
          },

          // id1 id2
          {
              DefaultStyle,
              0,
              {{0, SpacingOptions::Undecided}, {1, SpacingOptions::Undecided}},
              {{yytokentype::SymbolIdentifier, "id1"},
               {yytokentype::SymbolIdentifier, "id2"}},
          },

          // class mate
          {
              DefaultStyle,
              0,
              {{0, SpacingOptions::Undecided}, {1, SpacingOptions::Undecided}},
              {{yytokentype::TK_class, "class"},
               {yytokentype::SymbolIdentifier, "mate"}},
          },

          // id module
          {
              DefaultStyle,
              0,
              {{0, SpacingOptions::Undecided}, {1, SpacingOptions::Undecided}},
              {{yytokentype::SymbolIdentifier, "lunar"},
               {yytokentype::TK_module, "module"}},
          },

          // class 1337
          {
              DefaultStyle,
              0,
              {{0, SpacingOptions::Undecided}, {1, SpacingOptions::Undecided}},
              {{yytokentype::TK_class, "class"},
               {yytokentype::TK_DecNumber, "1337"}},
          },

          // 987 module
          {
              DefaultStyle,
              0,
              {{0, SpacingOptions::Undecided}, {1, SpacingOptions::Undecided}},
              {{yytokentype::TK_DecNumber, "987"},
               {yytokentype::TK_module, "module"}},
          },

          // a = 16'hf00d;
          {DefaultStyle,
           0,
           {{0, SpacingOptions::Undecided},
            {1, SpacingOptions::Undecided},
            {1, SpacingOptions::Undecided},
            {0, SpacingOptions::MustAppend},
            {0, SpacingOptions::MustAppend},
            {0, SpacingOptions::Undecided}},
           {{yytokentype::SymbolIdentifier, "a"},
            {'=', "="},
            {yytokentype::TK_DecNumber, "16"},
            {yytokentype::TK_HexBase, "'h"},
            {yytokentype::TK_HexDigits, "c0ffee"},
            {';', ";"}}},

          // a = 8'b1001_0110;
          {DefaultStyle,
           0,
           {{0, SpacingOptions::Undecided},
            {1, SpacingOptions::Undecided},
            {1, SpacingOptions::Undecided},
            {0, SpacingOptions::MustAppend},
            {0, SpacingOptions::MustAppend},
            {0, SpacingOptions::Undecided}},
           {{yytokentype::SymbolIdentifier, "a"},
            {'=', "="},
            {yytokentype::TK_DecNumber, "8"},
            {yytokentype::TK_BinBase, "'b"},
            {yytokentype::TK_BinDigits, "1001_0110"},
            {';', ";"}}},

          // a = 4'd10;
          {DefaultStyle,
           0,
           {{0, SpacingOptions::Undecided},
            {1, SpacingOptions::Undecided},
            {1, SpacingOptions::Undecided},
            {0, SpacingOptions::MustAppend},
            {0, SpacingOptions::MustAppend},
            {0, SpacingOptions::Undecided}},
           {{yytokentype::SymbolIdentifier, "a"},
            {'=', "="},
            {yytokentype::TK_DecNumber, "4"},
            {yytokentype::TK_DecBase, "'d"},
            {yytokentype::TK_DecDigits, "10"},
            {';', ";"}}},

          // a = 8'o100;
          {DefaultStyle,
           0,
           {{0, SpacingOptions::Undecided},
            {1, SpacingOptions::Undecided},
            {1, SpacingOptions::Undecided},
            {0, SpacingOptions::MustAppend},
            {0, SpacingOptions::MustAppend},
            {0, SpacingOptions::Undecided}},
           {{yytokentype::SymbolIdentifier, "a"},
            {'=', "="},
            {yytokentype::TK_DecNumber, "8"},
            {yytokentype::TK_OctBase, "'o"},
            {yytokentype::TK_OctDigits, "100"},
            {';', ";"}}},

          // a = 'hc0ffee;
          {DefaultStyle,
           0,
           {{0, SpacingOptions::Undecided},
            {1, SpacingOptions::Undecided},
            {1, SpacingOptions::Undecided},
            {0, SpacingOptions::MustAppend},
            {0, SpacingOptions::Undecided}},
           {{yytokentype::SymbolIdentifier, "a"},
            {'=', "="},
            {yytokentype::TK_HexBase, "'h"},
            {yytokentype::TK_HexDigits, "c0ffee"},
            {';', ";"}}},

          // a = funk('b0, 'd'8);
          {DefaultStyle,
           0,
           {{0, SpacingOptions::Undecided},
            {1, SpacingOptions::Undecided},
            {1, SpacingOptions::Undecided},
            {0, SpacingOptions::Undecided},
            {0, SpacingOptions::Undecided},
            {0, SpacingOptions::MustAppend},
            {0, SpacingOptions::Undecided},
            {1, SpacingOptions::Undecided},
            {0, SpacingOptions::MustAppend},
            {0, SpacingOptions::Undecided},
            {0, SpacingOptions::Undecided}},
           {{yytokentype::SymbolIdentifier, "a"},
            {'=', "="},
            {yytokentype::SymbolIdentifier, "funk"},
            {'(', "("},
            {yytokentype::TK_BinBase, "'b"},
            {yytokentype::TK_BinDigits, "0"},
            {',', ","},
            {yytokentype::TK_DecBase, "'d"},
            {yytokentype::TK_DecDigits, "8"},
            {')', ")"},
            {';', ";"}}},

          // a = 'b0 + 'd9;
          {DefaultStyle,
           0,
           {{0, SpacingOptions::Undecided},
            {1, SpacingOptions::Undecided},
            {1, SpacingOptions::Undecided},
            {0, SpacingOptions::MustAppend},
            {1, SpacingOptions::Undecided},
            {1, SpacingOptions::Undecided},
            {0, SpacingOptions::MustAppend},
            {0, SpacingOptions::Undecided}},
           {{yytokentype::SymbolIdentifier, "a"},
            {'=', "="},
            {yytokentype::TK_BinBase, "'b"},
            {yytokentype::TK_BinDigits, "0"},
            {'+', "+"},
            {yytokentype::TK_DecBase, "'d"},
            {yytokentype::TK_DecDigits, "9"},
            {';', ";"}}},

          // a = {3{4'd9, 1'bz}};
          {DefaultStyle,
           0,
           {{0, SpacingOptions::Undecided},
            {1, SpacingOptions::Undecided},
            {1, SpacingOptions::Undecided},
            {0, SpacingOptions::Undecided},  //  3
            {0, SpacingOptions::Undecided},
            {0, SpacingOptions::Undecided},
            {0, SpacingOptions::MustAppend},
            {0, SpacingOptions::MustAppend},
            {0, SpacingOptions::Undecided},  //  ,
            {1, SpacingOptions::Undecided},
            {0, SpacingOptions::MustAppend},
            {0, SpacingOptions::MustAppend},  //  z
            {0, SpacingOptions::Undecided},
            {0, SpacingOptions::Undecided},
            {0, SpacingOptions::Undecided}},
           {{yytokentype::SymbolIdentifier, "a"},
            {'=', "="},
            {'{', "{"},
            {yytokentype::TK_DecDigits, "3"},
            {'{', "{"},
            {yytokentype::TK_DecDigits, "4"},
            {yytokentype::TK_DecBase, "'d"},
            {yytokentype::TK_DecDigits, "9"},
            {',', ","},
            {yytokentype::TK_DecDigits, "1"},
            {yytokentype::TK_BinBase, "'b"},
            {yytokentype::TK_XZDigits, "z"},
            {'}', "}"},
            {'}', "}"},
            {';', ";"}}},

          // a ? b : c
          // (test cases around ':' are handled in context-sensitive section)
          {
              DefaultStyle,
              0,
              {
                  {0, SpacingOptions::Undecided},  //  a
                  {1, SpacingOptions::Undecided},  //  ?
                  {1, SpacingOptions::Undecided},  //  b
              },
              {
                  {yytokentype::SymbolIdentifier, "a"},
                  {'?', "?"},
                  {yytokentype::SymbolIdentifier, "b"},
              },
          },

          // 1 ? 2 : 3
          {
              DefaultStyle,
              0,
              {
                  {0, SpacingOptions::Undecided},  //  1
                  {1, SpacingOptions::Undecided},  //  ?
                  {1, SpacingOptions::Undecided},  //  2
              },
              {
                  {yytokentype::TK_DecNumber, "1"},
                  {'?', "?"},
                  {yytokentype::TK_DecNumber, "2"},
              },
          },

          // "1" ? "2" : "3"
          {
              DefaultStyle,
              0,
              {
                  {0, SpacingOptions::Undecided},  //  "1"
                  {1, SpacingOptions::Undecided},  //  ?
                  {1, SpacingOptions::Undecided},  //  "2"
              },
              {
                  {yytokentype::TK_StringLiteral, "1"},
                  {'?', "?"},
                  {yytokentype::TK_StringLiteral, "2"},
              },
          },

          // b ? 8'o100 : '0;
          {DefaultStyle,
           0,
           {{0, SpacingOptions::Undecided},   //  b
            {1, SpacingOptions::Undecided},   //  ?
            {1, SpacingOptions::Undecided},   //  8
            {0, SpacingOptions::MustAppend},  //  'o
            {0, SpacingOptions::MustAppend},  //  100
            kUnhandledSpacing,                //  :
            {1, SpacingOptions::Undecided},   //  '0
            {0, SpacingOptions::Undecided}},  //  ;
           {{yytokentype::SymbolIdentifier, "b"},
            {'?', "?"},
            {yytokentype::TK_DecNumber, "8"},
            {yytokentype::TK_OctBase, "'o"},
            {yytokentype::TK_OctDigits, "100"},
            {':', ":"},
            {yytokentype::TK_UnBasedNumber, "'0"},
            {';', ";"}}},

          // a = (b + c);
          {DefaultStyle,
           0,
           {{0, SpacingOptions::Undecided},   // a
            {1, SpacingOptions::Undecided},   // =
            {1, SpacingOptions::Undecided},   // (
            {0, SpacingOptions::Undecided},   // b
            {1, SpacingOptions::Undecided},   // +
            {1, SpacingOptions::Undecided},   // c
            {0, SpacingOptions::Undecided},   // )
            {0, SpacingOptions::Undecided}},  // ;
           {{yytokentype::SymbolIdentifier, "a"},
            {'=', "="},
            {'(', "("},
            {yytokentype::SymbolIdentifier, "b"},
            {'+', "+"},
            {yytokentype::SymbolIdentifier, "c"},
            {')', ")"},
            {';', ";"}}},

          // function foo(name = "foo");
          {DefaultStyle,
           0,
           {{0, SpacingOptions::Undecided},   //  function
            {1, SpacingOptions::Undecided},   //  foo
            {0, SpacingOptions::Undecided},   //  (
            {0, SpacingOptions::Undecided},   //  name
            {1, SpacingOptions::Undecided},   //  =
            {1, SpacingOptions::Undecided},   //  "foo"
            {0, SpacingOptions::Undecided},   //  )
            {0, SpacingOptions::Undecided}},  //  ;
           {{yytokentype::TK_function, "function"},
            {yytokentype::SymbolIdentifier, "foo"},
            {'(', "("},
            {yytokentype::SymbolIdentifier, "name"},
            {'=', "="},
            {yytokentype::TK_StringLiteral, "\"foo\""},
            {')', ")"},
            {';', ";"}}},

          // `define FOO(name = "bar")
          {DefaultStyle,
           0,
           {{0, SpacingOptions::Undecided},   //  `define
            {1, SpacingOptions::Undecided},   //  FOO
            {0, SpacingOptions::Undecided},   //  (
            {0, SpacingOptions::Undecided},   //  name
            {1, SpacingOptions::Undecided},   //  =
            {1, SpacingOptions::Undecided},   //  "bar"
            {0, SpacingOptions::Undecided}},  //  )
           {{yytokentype::PP_define, "`define"},
            {yytokentype::SymbolIdentifier, "FOO"},
            {'(', "("},
            {yytokentype::SymbolIdentifier, "name"},
            {'=', "="},
            {yytokentype::TK_StringLiteral, "\"bar\""},
            {')', ")"}}},

          // endfunction : funk
          {DefaultStyle,
           1,
           {{0, SpacingOptions::Undecided},
            {1, SpacingOptions::Undecided},
            {1, SpacingOptions::Undecided}},
           {
               {yytokentype::TK_endfunction, "endfunction"},
               {':', ":"},
               {yytokentype::SymbolIdentifier, "funk"},
           }},

          // case (expr):
          {DefaultStyle,
           1,
           {
               {0, SpacingOptions::Undecided},
               {1, SpacingOptions::Undecided},
               {0, SpacingOptions::Undecided},
               {0, SpacingOptions::Undecided},
               {0, SpacingOptions::Undecided},
           },
           {
               {yytokentype::TK_case, "case"},
               {'(', "("},
               {yytokentype::SymbolIdentifier, "expr"},
               {')', ")"},
               {':', ":"},
           }},

          // return 0;
          {DefaultStyle,
           1,
           {
               {0, SpacingOptions::Undecided},
               {1, SpacingOptions::Undecided},
               {0, SpacingOptions::Undecided},
           },
           {
               {yytokentype::TK_return, "return"},
               {yytokentype::TK_UnBasedNumber, "0"},
               {';', ";"},
           }},

          // funk();
          {DefaultStyle,
           1,
           {
               {0, SpacingOptions::Undecided},
               {0, SpacingOptions::Undecided},
               {0, SpacingOptions::Undecided},
               {0, SpacingOptions::Undecided},
           },
           {
               {yytokentype::SymbolIdentifier, "funk"},
               {'(', "("},
               {')', ")"},
               {';', ";"},
           }},

          // funk(arg);
          {DefaultStyle,
           1,
           {
               {0, SpacingOptions::Undecided},
               {0, SpacingOptions::Undecided},
               {0, SpacingOptions::Undecided},
               {0, SpacingOptions::Undecided},
               {0, SpacingOptions::Undecided},
           },
           {
               {yytokentype::SymbolIdentifier, "funk"},
               {'(', "("},
               {yytokentype::SymbolIdentifier, "arg"},
               {')', ")"},
               {';', ";"},
           }},

          // funk("arg");
          {DefaultStyle,
           1,
           {
               {0, SpacingOptions::Undecided},
               {0, SpacingOptions::Undecided},
               {0, SpacingOptions::Undecided},
               {0, SpacingOptions::Undecided},
               {0, SpacingOptions::Undecided},
           },
           {
               {yytokentype::SymbolIdentifier, "funk"},
               {'(', "("},
               {yytokentype::TK_StringLiteral, "\"arg\""},
               {')', ")"},
               {';', ";"},
           }},

          // funk(arg1, arg2);
          {DefaultStyle,
           1,
           {
               {0, SpacingOptions::Undecided},
               {0, SpacingOptions::Undecided},
               {0, SpacingOptions::Undecided},
               {0, SpacingOptions::Undecided},
               {1, SpacingOptions::Undecided},
               {0, SpacingOptions::Undecided},
               {0, SpacingOptions::Undecided},
           },
           {
               {yytokentype::SymbolIdentifier, "funk"},
               {'(', "("},
               {yytokentype::SymbolIdentifier, "arg1"},
               {',', ","},
               {yytokentype::SymbolIdentifier, "arg2"},
               {')', ")"},
               {';', ";"},
           }},

          // instantiation with named ports
          // funky town(.f1(arg1), .f2(arg2));
          {DefaultStyle,
           1,
           {
               {0, SpacingOptions::Undecided},
               {1, SpacingOptions::Undecided},
               {0, SpacingOptions::Undecided},  // '('
               {0, SpacingOptions::Undecided},  // '.'
               {0, SpacingOptions::Undecided},  // "f1"
               {0, SpacingOptions::Undecided},  // '('
               {0, SpacingOptions::Undecided},  // "arg1"
               {0, SpacingOptions::Undecided},  // ')'
               {0, SpacingOptions::Undecided},  // ','
               {1, SpacingOptions::Undecided},  // '.'
               {0, SpacingOptions::Undecided},  // "f1"
               {0, SpacingOptions::Undecided},  // '('
               {0, SpacingOptions::Undecided},  // "arg1"
               {0, SpacingOptions::Undecided},  // ')'
               {0, SpacingOptions::Undecided},  // ')'
               {0, SpacingOptions::Undecided},  // ';'
           },
           {
               {yytokentype::SymbolIdentifier, "funky"},
               {yytokentype::SymbolIdentifier, "town"},
               {'(', "("},
               {'.', "."},
               {yytokentype::SymbolIdentifier, "f1"},
               {'(', "("},
               {yytokentype::SymbolIdentifier, "arg1"},
               {')', ")"},
               {',', ","},
               {'.', "."},
               {yytokentype::SymbolIdentifier, "f2"},
               {'(', "("},
               {yytokentype::SymbolIdentifier, "arg2"},
               {')', ")"},
               {')', ")"},
               {';', ";"},
           }},

          // `ID.`ID
          {DefaultStyle,
           1,
           {
               {0, SpacingOptions::Undecided},
               {0, SpacingOptions::Undecided},
               {0, SpacingOptions::Undecided},
           },
           {
               {yytokentype::MacroIdentifier, "`ID"},
               {'.', "."},
               {yytokentype::MacroIdentifier, "`ID"},
           }},

          // id.id
          {DefaultStyle,
           1,
           {
               {0, SpacingOptions::Undecided},
               {0, SpacingOptions::Undecided},
               {0, SpacingOptions::Undecided},
           },
           {
               {yytokentype::SymbolIdentifier, "id"},
               {'.', "."},
               {yytokentype::SymbolIdentifier, "id"},
           }},

          // super.id
          {DefaultStyle,
           1,
           {
               {0, SpacingOptions::Undecided},
               {0, SpacingOptions::Undecided},
               {0, SpacingOptions::Undecided},
           },
           {
               {yytokentype::TK_super, "super"},
               {'.', "."},
               {yytokentype::SymbolIdentifier, "id"},
           }},

          // this.id
          {DefaultStyle,
           1,
           {
               {0, SpacingOptions::Undecided},
               {0, SpacingOptions::Undecided},
               {0, SpacingOptions::Undecided},
           },
           {
               {yytokentype::TK_this, "this"},
               {'.', "."},
               {yytokentype::SymbolIdentifier, "id"},
           }},

          // option.id
          {DefaultStyle,
           1,
           {
               {0, SpacingOptions::Undecided},
               {0, SpacingOptions::Undecided},
               {0, SpacingOptions::Undecided},
           },
           {
               {yytokentype::TK_option, "option"},
               {'.', "."},
               {yytokentype::SymbolIdentifier, "id"},
           }},

          // `MACRO();
          {DefaultStyle,
           1,
           {
               {0, SpacingOptions::Undecided},
               {0, SpacingOptions::Undecided},
               {0, SpacingOptions::Undecided},
               {0, SpacingOptions::Undecided},
           },
           {
               {yytokentype::MacroCallId, "`MACRO"},
               {'(', "("},
               {yytokentype::MacroCallCloseToEndLine, ")"},
               {';', ";"},
           }},

          // `MACRO(x);
          {DefaultStyle,
           1,
           {
               {0, SpacingOptions::Undecided},
               {0, SpacingOptions::Undecided},
               {0, SpacingOptions::Undecided},
               {0, SpacingOptions::Undecided},
               {0, SpacingOptions::Undecided},
           },
           {
               {yytokentype::MacroCallId, "`MACRO"},
               {'(', "("},
               {yytokentype::SymbolIdentifier, "x"},
               {yytokentype::MacroCallCloseToEndLine, ")"},
               {';', ";"},
           }},

          // `MACRO(y, x);
          {DefaultStyle,
           1,
           {
               {0, SpacingOptions::Undecided},
               {0, SpacingOptions::Undecided},
               {0, SpacingOptions::Undecided},  // "y"
               {0, SpacingOptions::Undecided},  // ','
               {1, SpacingOptions::Undecided},  // "x"
               {0, SpacingOptions::Undecided},
               {0, SpacingOptions::Undecided},
           },
           {
               {yytokentype::MacroCallId, "`MACRO"},
               {'(', "("},
               {yytokentype::SymbolIdentifier, "y"},
               {',', ","},
               {yytokentype::SymbolIdentifier, "x"},
               {yytokentype::MacroCallCloseToEndLine, ")"},
               {';', ";"},
           }},

          // `define FOO
          // `define BAR
          {DefaultStyle,
           1,
           {
               {0, SpacingOptions::Undecided},  // `define
               {1, SpacingOptions::Undecided},  // FOO
               {0, SpacingOptions::Undecided},  // "" (empty definition body)
               {0, SpacingOptions::MustWrap},   // `define
               {1, SpacingOptions::Undecided},  // BAR
               {0, SpacingOptions::Undecided},  // "" (empty definition body)
           },
           {
               {yytokentype::PP_define, "`define"},
               {yytokentype::SymbolIdentifier, "FOO"},
               {yytokentype::PP_define_body, ""},
               {yytokentype::PP_define, "`define"},
               {yytokentype::SymbolIdentifier, "BAR"},
               {yytokentype::PP_define_body, ""},
           }},

          // `define FOO 1
          // `define BAR 2
          {DefaultStyle,
           1,
           {
               {0, SpacingOptions::Undecided},  // `define
               {1, SpacingOptions::Undecided},  // FOO
               {1, SpacingOptions::Undecided},  // 1
               {1, SpacingOptions::MustWrap},   // `define
               {1, SpacingOptions::Undecided},  // BAR
               {1, SpacingOptions::Undecided},  // 2
           },
           {
               {yytokentype::PP_define, "`define"},
               {yytokentype::PP_Identifier, "FOO"},
               {yytokentype::PP_define_body, "1"},
               {yytokentype::PP_define, "`define"},
               {yytokentype::PP_Identifier, "BAR"},
               {yytokentype::PP_define_body, "2"},
           }},

          // `define FOO()
          // `define BAR(x)
          // `define BAZ(y,z)
          {DefaultStyle,
           1,
           {
               {0, SpacingOptions::Undecided},   // `define
               {1, SpacingOptions::Undecided},   // FOO
               {0, SpacingOptions::MustAppend},  // (
               {0, SpacingOptions::Undecided},   // )
               {0, SpacingOptions::Undecided},   // "" (empty definition body)

               {0, SpacingOptions::MustWrap},    // `define
               {1, SpacingOptions::Undecided},   // BAR
               {0, SpacingOptions::MustAppend},  // (
               {0, SpacingOptions::Undecided},   // x
               {0, SpacingOptions::Undecided},   // )
               {0, SpacingOptions::Undecided},   // "" (empty definition body)

               {0, SpacingOptions::MustWrap},    // `define
               {1, SpacingOptions::Undecided},   // BAZ
               {0, SpacingOptions::MustAppend},  // (
               {0, SpacingOptions::Undecided},   // y
               {0, SpacingOptions::Undecided},   // ,
               {1, SpacingOptions::Undecided},   // z
               {0, SpacingOptions::Undecided},   // )
               {0, SpacingOptions::Undecided},   // "" (empty definition body)
           },
           {
               {yytokentype::PP_define, "`define"},
               {yytokentype::PP_Identifier, "FOO"},
               {'(', "("},
               {')', ")"},
               {yytokentype::PP_define_body, ""},

               {yytokentype::PP_define, "`define"},
               {yytokentype::PP_Identifier, "BAR"},
               {'(', "("},
               {yytokentype::SymbolIdentifier, "x"},
               {')', ")"},
               {yytokentype::PP_define_body, ""},

               {yytokentype::PP_define, "`define"},
               {yytokentype::PP_Identifier, "BAZ"},
               {'(', "("},
               {yytokentype::SymbolIdentifier, "y"},
               {',', ","},
               {yytokentype::SymbolIdentifier, "z"},
               {')', ")"},
               {yytokentype::PP_define_body, ""},
           }},

          // `define ADD(y,z) y+z
          {
              DefaultStyle,
              1,
              {
                  {0, SpacingOptions::Undecided},   // `define
                  {1, SpacingOptions::Undecided},   // ADD
                  {0, SpacingOptions::MustAppend},  // (
                  {0, SpacingOptions::Undecided},   // y
                  {0, SpacingOptions::Undecided},   // ,
                  {1, SpacingOptions::Undecided},   // z
                  {0, SpacingOptions::Undecided},   // )
                  {1, SpacingOptions::Undecided},   // "y+z"
              },
              {
                  {yytokentype::PP_define, "`define"},
                  {yytokentype::PP_Identifier, "ADD"},
                  {'(', "("},
                  {yytokentype::SymbolIdentifier, "y"},
                  {',', ","},
                  {yytokentype::SymbolIdentifier, "z"},
                  {')', ")"},
                  {yytokentype::PP_define_body, "y+z"},
              },
          },

          // function new;
          {DefaultStyle,
           1,
           {
               {0, SpacingOptions::Undecided},  // function
               {1, SpacingOptions::Undecided},  // new
               {0, SpacingOptions::Undecided},  // ;
           },
           {
               {yytokentype::TK_function, "function"},
               {yytokentype::TK_new, "new"},
               {';', ";"},
           }},

          // function new();
          {DefaultStyle,
           1,
           {
               {0, SpacingOptions::Undecided},  // function
               {1, SpacingOptions::Undecided},  // new
               {0, SpacingOptions::Undecided},  // (
               {0, SpacingOptions::Undecided},  // )
               {0, SpacingOptions::Undecided},  // ;
           },
           {
               {yytokentype::TK_function, "function"},
               {yytokentype::TK_new, "new"},
               {'(', "("},
               {')', ")"},
               {';', ";"},
           }},

          // end endfunction endclass (end* keywords)
          {DefaultStyle,
           1,
           {
               {0, SpacingOptions::Undecided},  // end
               {1, SpacingOptions::MustWrap},   // end
               {1, SpacingOptions::MustWrap},   // endfunction
               {1, SpacingOptions::MustWrap},   // endclass
               {1, SpacingOptions::MustWrap},   // endpackage
           },
           {
               {yytokentype::TK_end, "end"},
               {yytokentype::TK_end, "end"},
               {yytokentype::TK_endfunction, "endfunction"},
               {yytokentype::TK_endclass, "endclass"},
               {yytokentype::TK_endpackage, "endpackage"},
           }},
          {DefaultStyle,
           1,
           {
               {0, SpacingOptions::Undecided},  // end
               {1, SpacingOptions::MustWrap},   // end
               {1, SpacingOptions::MustWrap},   // endtask
               {1, SpacingOptions::MustWrap},   // endmodule
           },
           {
               {yytokentype::TK_end, "end"},
               {yytokentype::TK_end, "end"},
               {yytokentype::TK_endtask, "endtask"},
               {yytokentype::TK_endmodule, "endmodule"},
           }},

          // if (r == t) a.b(c);
          // else d.e(f);
          {DefaultStyle,
           1,
           {
               {0, SpacingOptions::Undecided},  // if
               {1, SpacingOptions::Undecided},  // (
               {0, SpacingOptions::Undecided},  // r
               {1, SpacingOptions::Undecided},  // ==
               {1, SpacingOptions::Undecided},  // t
               {0, SpacingOptions::Undecided},  // )
               {1, SpacingOptions::Undecided},  // a
               {0, SpacingOptions::Undecided},  // .
               {0, SpacingOptions::Undecided},  // b
               {0, SpacingOptions::Undecided},  // (
               {0, SpacingOptions::Undecided},  // c
               {0, SpacingOptions::Undecided},  // )
               {0, SpacingOptions::Undecided},  // ;

               {1, SpacingOptions::MustWrap},   // else
               {1, SpacingOptions::Undecided},  // d
               {0, SpacingOptions::Undecided},  // .
               {0, SpacingOptions::Undecided},  // e
               {0, SpacingOptions::Undecided},  // (
               {0, SpacingOptions::Undecided},  // f
               {0, SpacingOptions::Undecided},  // )
               {0, SpacingOptions::Undecided},  // ;
           },
           {
               {yytokentype::TK_if, "if"},
               {'(', "("},
               {yytokentype::SymbolIdentifier, "r"},
               {yytokentype::TK_EQ, "=="},
               {yytokentype::SymbolIdentifier, "t"},
               {')', ")"},
               {yytokentype::SymbolIdentifier, "a"},
               {'.', "."},
               {yytokentype::SymbolIdentifier, "b"},
               {'(', "("},
               {yytokentype::SymbolIdentifier, "c"},
               {')', ")"},
               {';', ";"},

               {yytokentype::TK_else, "else"},
               {yytokentype::SymbolIdentifier, "d"},
               {'.', "."},
               {yytokentype::SymbolIdentifier, "e"},
               {'(', "("},
               {yytokentype::SymbolIdentifier, "f"},
               {')', ")"},
               {';', ";"},
           }},

          // if (r == t) begin
          //   a.b(c);
          // end else begin
          //   d.e(f);
          // end
          {DefaultStyle,
           1,
           {
               {0, SpacingOptions::Undecided},  // if
               {1, SpacingOptions::Undecided},  // (
               {0, SpacingOptions::Undecided},  // r
               {1, SpacingOptions::Undecided},  // ==
               {1, SpacingOptions::Undecided},  // t
               {0, SpacingOptions::Undecided},  // )

               {1, SpacingOptions::MustAppend},  // begin
               {1, SpacingOptions::Undecided},   // a
               {0, SpacingOptions::Undecided},   // .
               {0, SpacingOptions::Undecided},   // b
               {0, SpacingOptions::Undecided},   // (
               {0, SpacingOptions::Undecided},   // c
               {0, SpacingOptions::Undecided},   // )
               {0, SpacingOptions::Undecided},   // ;
               {1, SpacingOptions::MustWrap},    // end

               {1, SpacingOptions::MustAppend},  // else

               {1, SpacingOptions::MustAppend},  // begin
               {1, SpacingOptions::Undecided},   // d
               {0, SpacingOptions::Undecided},   // .
               {0, SpacingOptions::Undecided},   // e
               {0, SpacingOptions::Undecided},   // (
               {0, SpacingOptions::Undecided},   // f
               {0, SpacingOptions::Undecided},   // )
               {0, SpacingOptions::Undecided},   // ;
               {1, SpacingOptions::MustWrap},    // end
           },
           {
               {yytokentype::TK_if, "if"},
               {'(', "("},
               {yytokentype::SymbolIdentifier, "r"},
               {yytokentype::TK_EQ, "=="},
               {yytokentype::SymbolIdentifier, "t"},
               {')', ")"},

               {yytokentype::TK_begin, "begin"},
               {yytokentype::SymbolIdentifier, "a"},
               {'.', "."},
               {yytokentype::SymbolIdentifier, "b"},
               {'(', "("},
               {yytokentype::SymbolIdentifier, "c"},
               {')', ")"},
               {';', ";"},
               {yytokentype::TK_end, "end"},

               {yytokentype::TK_else, "else"},

               {yytokentype::TK_begin, "begin"},
               {yytokentype::SymbolIdentifier, "d"},
               {'.', "."},
               {yytokentype::SymbolIdentifier, "e"},
               {'(', "("},
               {yytokentype::SymbolIdentifier, "f"},
               {')', ")"},
               {';', ";"},
               {yytokentype::TK_end, "end"},
           }},

          // various built-in function calls
          {DefaultStyle,
           1,
           {{0, SpacingOptions::Undecided}, {0, SpacingOptions::Undecided}},
           {{yytokentype::TK_and, "and"}, {'(', "("}}},
          {DefaultStyle,
           1,
           {{0, SpacingOptions::Undecided}, {0, SpacingOptions::Undecided}},
           {{yytokentype::TK_and, "assert"}, {'(', "("}}},
          {DefaultStyle,
           1,
           {{0, SpacingOptions::Undecided}, {0, SpacingOptions::Undecided}},
           {{yytokentype::TK_and, "assume"}, {'(', "("}}},
          {DefaultStyle,
           1,
           {{0, SpacingOptions::Undecided}, {0, SpacingOptions::Undecided}},
           {{yytokentype::TK_find, "find"}, {'(', "("}}},
          {DefaultStyle,
           1,
           {{0, SpacingOptions::Undecided}, {0, SpacingOptions::Undecided}},
           {{yytokentype::TK_find_index, "find_index"}, {'(', "("}}},
          {DefaultStyle,
           1,
           {{0, SpacingOptions::Undecided}, {0, SpacingOptions::Undecided}},
           {{yytokentype::TK_find_first, "find_first"}, {'(', "("}}},
          {DefaultStyle,
           1,
           {{0, SpacingOptions::Undecided}, {0, SpacingOptions::Undecided}},
           {{yytokentype::TK_find_first_index, "find_first_index"},
            {'(', "("}}},
          {DefaultStyle,
           1,
           {{0, SpacingOptions::Undecided}, {0, SpacingOptions::Undecided}},
           {{yytokentype::TK_find_last, "find_last"}, {'(', "("}}},
          {DefaultStyle,
           1,
           {{0, SpacingOptions::Undecided}, {0, SpacingOptions::Undecided}},
           {{yytokentype::TK_find_last_index, "find_last_index"}, {'(', "("}}},
          {DefaultStyle,
           1,
           {{0, SpacingOptions::Undecided}, {0, SpacingOptions::Undecided}},
           {{yytokentype::TK_min, "min"}, {'(', "("}}},
          {DefaultStyle,
           1,
           {{0, SpacingOptions::Undecided}, {0, SpacingOptions::Undecided}},
           {{yytokentype::TK_max, "max"}, {'(', "("}}},
          {DefaultStyle,
           1,
           {{0, SpacingOptions::Undecided}, {0, SpacingOptions::Undecided}},
           {{yytokentype::TK_or, "or"}, {'(', "("}}},
          {DefaultStyle,
           1,
           {{0, SpacingOptions::Undecided}, {0, SpacingOptions::Undecided}},
           {{yytokentype::TK_product, "product"}, {'(', "("}}},
          {DefaultStyle,
           1,
           {{0, SpacingOptions::Undecided}, {0, SpacingOptions::Undecided}},
           {{yytokentype::TK_randomize, "randomize"}, {'(', "("}}},
          {DefaultStyle,
           1,
           {{0, SpacingOptions::Undecided}, {0, SpacingOptions::Undecided}},
           {{yytokentype::TK_reverse, "reverse"}, {'(', "("}}},
          {DefaultStyle,
           1,
           {{0, SpacingOptions::Undecided}, {0, SpacingOptions::Undecided}},
           {{yytokentype::TK_rsort, "rsort"}, {'(', "("}}},
          {DefaultStyle,
           1,
           {{0, SpacingOptions::Undecided}, {0, SpacingOptions::Undecided}},
           {{yytokentype::TK_shuffle, "shuffle"}, {'(', "("}}},
          {DefaultStyle,
           1,
           {{0, SpacingOptions::Undecided}, {0, SpacingOptions::Undecided}},
           {{yytokentype::TK_sort, "sort"}, {'(', "("}}},
          {DefaultStyle,
           1,
           {{0, SpacingOptions::Undecided}, {0, SpacingOptions::Undecided}},
           {{yytokentype::TK_sum, "sum"}, {'(', "("}}},
          {DefaultStyle,
           1,
           {{0, SpacingOptions::Undecided}, {0, SpacingOptions::Undecided}},
           {{yytokentype::TK_unique, "unique"}, {'(', "("}}},
          {DefaultStyle,
           1,
           {{0, SpacingOptions::Undecided}, {0, SpacingOptions::Undecided}},
           {{yytokentype::TK_wait, "wait"}, {'(', "("}}},
          {DefaultStyle,
           1,
           {{0, SpacingOptions::Undecided}, {0, SpacingOptions::Undecided}},
           {{yytokentype::TK_xor, "xor"}, {'(', "("}}},

          // escaped identifier
          // baz.\FOO .bar
          {DefaultStyle,
           1,
           {
               {0, SpacingOptions::Undecided},  // baz
               {0, SpacingOptions::Undecided},  // .
               {0, SpacingOptions::Undecided},  // \FOO
               {1, SpacingOptions::Undecided},  // .
               {0, SpacingOptions::Undecided},  // bar
           },
           {
               {yytokentype::SymbolIdentifier, "baz"},
               {'.', "."},
               {yytokentype::EscapedIdentifier, "\\FOO"},
               {'.', "."},
               {yytokentype::SymbolIdentifier, "bar"},
           }},

          // escaped identifier inside macro call
          // `BAR(\FOO )
          {DefaultStyle,
           1,
           {
               {0, SpacingOptions::Undecided},  // `BAR
               {0, SpacingOptions::Undecided},  // (
               {0, SpacingOptions::Undecided},  // \FOO
               {1, SpacingOptions::Undecided},  // )
           },
           {
               {yytokentype::MacroCallId, "`BAR"},
               {'(', "("},
               {yytokentype::EscapedIdentifier, "\\FOO"},
               {')', ")"},
           }},

          // import foo_pkg::symbol;
          {DefaultStyle,
           1,
           {
               {0, SpacingOptions::Undecided},  // import
               {1, SpacingOptions::Undecided},  // foo_pkg
               {0, SpacingOptions::Undecided},  // ::
               {0, SpacingOptions::Undecided},  // symbol
               {0, SpacingOptions::Undecided},  // ;
           },
           {
               {yytokentype::TK_import, "import"},
               {yytokentype::SymbolIdentifier, "foo_pkg"},
               {yytokentype::TK_SCOPE_RES, "::"},
               {yytokentype::SymbolIdentifier, "symbol"},
               {';', ";"},
           }},

          // import foo_pkg::*;
          {DefaultStyle,
           1,
           {
               {0, SpacingOptions::Undecided},  // import
               {1, SpacingOptions::Undecided},  // foo_pkg
               {0, SpacingOptions::Undecided},  // ::
               {0, SpacingOptions::Undecided},  // *
               {0, SpacingOptions::Undecided},  // ;
           },
           {
               {yytokentype::TK_import, "import"},
               {yytokentype::SymbolIdentifier, "foo_pkg"},
               {yytokentype::TK_SCOPE_RES, "::"},
               {'*', "*"},
               {';', ";"},
           }},

          // #0; (delay, unitless integer)
          {DefaultStyle,
           1,
           {
               {0, SpacingOptions::Undecided},  // #
               {0, SpacingOptions::Undecided},  // 0
               {0, SpacingOptions::Undecided},  // ;
           },
           {
               {'#', "#"},
               {yytokentype::TK_DecNumber, "0"},
               {';', ";"},
           }},

          // #0.5; (delay, real-value)
          {DefaultStyle,
           1,
           {
               {0, SpacingOptions::Undecided},  // #
               {0, SpacingOptions::Undecided},  // 0.5
               {0, SpacingOptions::Undecided},  // ;
           },
           {
               {'#', "#"},
               {yytokentype::TK_RealTime, "0.5"},
               {';', ";"},
           }},

          // #0ns; (delay, time-literal)
          {DefaultStyle,
           1,
           {
               {0, SpacingOptions::Undecided},  // #
               {0, SpacingOptions::Undecided},  // 0ns
               {0, SpacingOptions::Undecided},  // ;
           },
           {
               {'#', "#"},
               {yytokentype::TK_TimeLiteral, "0ns"},
               {';', ";"},
           }},

          // #1step; (delay, 1step)
          {DefaultStyle,
           1,
           {
               {0, SpacingOptions::Undecided},  // #
               {0, SpacingOptions::Undecided},  // 1step
               {0, SpacingOptions::Undecided},  // ;
           },
           {
               {'#', "#"},
               {yytokentype::TK_1step, "1step"},
               {';', ";"},
           }},

          // default: ;
          {DefaultStyle,
           0,
           {
               {0, SpacingOptions::Undecided},
               {0, SpacingOptions::Undecided},
               {1, SpacingOptions::Undecided},
           },
           {
               {yytokentype::TK_default, "default"},
               {':', ":"},
               {';', ";"},
           }},

          // foo = 1 << bar;
          {DefaultStyle,
           0,
           {
               {0, SpacingOptions::Undecided},
               {1, SpacingOptions::Undecided},
               {1, SpacingOptions::Undecided},
               {1, SpacingOptions::Undecided},
               {1, SpacingOptions::Undecided},
               {0, SpacingOptions::Undecided},
           },
           {
               {yytokentype::SymbolIdentifier, "foo"},
               {'=', "="},
               {yytokentype::TK_DecNumber, "1"},
               {yytokentype::TK_LS, "<<"},
               {yytokentype::SymbolIdentifier, "bar"},
               {';', ";"},
           }},

          // foo = bar << 1;
          {DefaultStyle,
           0,
           {
               {0, SpacingOptions::Undecided},
               {1, SpacingOptions::Undecided},
               {1, SpacingOptions::Undecided},
               {1, SpacingOptions::Undecided},
               {1, SpacingOptions::Undecided},
               {0, SpacingOptions::Undecided},
           },
           {
               {yytokentype::SymbolIdentifier, "foo"},
               {'=', "="},
               {yytokentype::SymbolIdentifier, "bar"},
               {yytokentype::TK_LS, "<<"},
               {yytokentype::TK_DecNumber, "1"},
               {';', ";"},
           }},

          // foo = `BAR << 1;
          {DefaultStyle,
           0,
           {
               {0, SpacingOptions::Undecided},
               {1, SpacingOptions::Undecided},
               {1, SpacingOptions::Undecided},
               {1, SpacingOptions::Undecided},
               {1, SpacingOptions::Undecided},
               {0, SpacingOptions::Undecided},
           },
           {
               {yytokentype::SymbolIdentifier, "foo"},
               {'=', "="},
               {yytokentype::MacroIdentifier, "`BAR"},
               {yytokentype::TK_LS, "<<"},
               {yytokentype::TK_DecNumber, "1"},
               {';', ";"},
           }},

          // foo = 1 << `BAR;
          {DefaultStyle,
           0,
           {
               {0, SpacingOptions::Undecided},
               {1, SpacingOptions::Undecided},
               {1, SpacingOptions::Undecided},
               {1, SpacingOptions::Undecided},
               {1, SpacingOptions::Undecided},
               {0, SpacingOptions::Undecided},
           },
           {
               {yytokentype::SymbolIdentifier, "foo"},
               {'=', "="},
               {yytokentype::TK_DecNumber, "1"},
               {yytokentype::TK_LS, "<<"},
               {yytokentype::MacroIdentifier, "`BAR"},
               {';', ";"},
           }},

          // foo = 1 >> bar;
          {DefaultStyle,
           0,
           {
               {0, SpacingOptions::Undecided},
               {1, SpacingOptions::Undecided},
               {1, SpacingOptions::Undecided},
               {1, SpacingOptions::Undecided},
               {1, SpacingOptions::Undecided},
               {0, SpacingOptions::Undecided},
           },
           {
               {yytokentype::SymbolIdentifier, "foo"},
               {'=', "="},
               {yytokentype::TK_DecNumber, "1"},
               {yytokentype::TK_RS, ">>"},
               {yytokentype::SymbolIdentifier, "bar"},
               {';', ";"},
           }},

          // foo = bar >> 1;
          {DefaultStyle,
           0,
           {
               {0, SpacingOptions::Undecided},
               {1, SpacingOptions::Undecided},
               {1, SpacingOptions::Undecided},
               {1, SpacingOptions::Undecided},
               {1, SpacingOptions::Undecided},
               {0, SpacingOptions::Undecided},
           },
           {
               {yytokentype::SymbolIdentifier, "foo"},
               {'=', "="},
               {yytokentype::SymbolIdentifier, "bar"},
               {yytokentype::TK_RS, ">>"},
               {yytokentype::TK_DecNumber, "1"},
               {';', ";"},
           }},

          // foo = `BAR >> 1;
          {DefaultStyle,
           0,
           {
               {0, SpacingOptions::Undecided},
               {1, SpacingOptions::Undecided},
               {1, SpacingOptions::Undecided},
               {1, SpacingOptions::Undecided},
               {1, SpacingOptions::Undecided},
               {0, SpacingOptions::Undecided},
           },
           {
               {yytokentype::SymbolIdentifier, "foo"},
               {'=', "="},
               {yytokentype::MacroIdentifier, "`BAR"},
               {yytokentype::TK_RS, ">>"},
               {yytokentype::TK_DecNumber, "1"},
               {';', ";"},
           }},

          // foo = 1 >> `BAR;
          {DefaultStyle,
           0,
           {
               {0, SpacingOptions::Undecided},
               {1, SpacingOptions::Undecided},
               {1, SpacingOptions::Undecided},
               {1, SpacingOptions::Undecided},
               {1, SpacingOptions::Undecided},
               {0, SpacingOptions::Undecided},
           },
           {
               {yytokentype::SymbolIdentifier, "foo"},
               {'=', "="},
               {yytokentype::TK_DecNumber, "1"},
               {yytokentype::TK_RS, ">>"},
               {yytokentype::MacroIdentifier, "`BAR"},
               {';', ";"},
           }},
      };

  int test_index = 0;
  for (const auto& test_case : kTestCases) {
    verible::UnwrappedLineMemoryHandler handler;
    handler.CreateTokenInfos(test_case.input_tokens);
    verible::UnwrappedLine unwrapped_line(test_case.uwline_indentation,
                                          handler.GetPreFormatTokensBegin());
    handler.AddFormatTokens(&unwrapped_line);
    // The format_token_enums are not yet set by AddFormatTokens.
    for (auto& ftoken : handler.pre_format_tokens_) {
      ftoken.format_token_enum =
          GetFormatTokenType(yytokentype(ftoken.TokenEnum()));
    }

    auto& ftokens_range = handler.pre_format_tokens_;
    // nullptr buffer_start is needed because token text do not belong to the
    // same contiguous string buffer.
    // Pass an empty/fake tree, which will not be used for testing
    // context-insensitive annotation rules.
    // Since we're using the joined string buffer inside handler,
    // we need to pass an EOF token that points to the end of that buffer.
    AnnotateFormattingInformation(test_case.style, nullptr, nullptr,
                                  handler.EOFToken(), ftokens_range.begin(),
                                  ftokens_range.end());
    EXPECT_TRUE(CorrectExpectedFormatTokens(test_case.expected_calculations,
                                            ftokens_range))
        << "mismatch at test case " << test_index << " of " << kTestCases.size()
        << ", tokens " << test_case;
    ++test_index;
  }
}  // NOLINT(readability/fn_size)

// These test cases support the use of syntactic context, but it is not
// required to specify context.
TEST(TokenAnnotatorTest, AnnotateFormattingWithContextTest) {
  const std::initializer_list<AnnotateWithContextTestCase> kTestCases = {
      {
          DefaultStyle,
          {'=', "="},
          {yytokentype::TK_StringLiteral, "\"hello\""},
          {},  // any context
          {1, SpacingOptions::Undecided},
      },
      {
          DefaultStyle,
          {'=', "="},
          {yytokentype::TK_EvalStringLiteral, "`\"hello`\""},
          {},  // any context
          {1, SpacingOptions::Undecided},
      },
      // Test cases covering right token as a preprocessor directive:
      {
          DefaultStyle,
          {yytokentype::TK_EOL_COMMENT, "//comment1"},
          {yytokentype::PP_ifdef, "`ifdef"},
          {},  // any context
          {1, SpacingOptions::MustWrap},
      },
      {
          DefaultStyle,
          {yytokentype::SymbolIdentifier, "id"},
          {yytokentype::PP_ifdef, "`ifdef"},
          {},  // any context
          {1, SpacingOptions::MustWrap},
      },
      {
          DefaultStyle,
          {';', ";"},
          {yytokentype::PP_ifdef, "`ifdef"},
          {},  // any context
          {1, SpacingOptions::MustWrap},
      },
      {
          DefaultStyle,
          {yytokentype::PP_else, "`else"},
          {yytokentype::PP_ifdef, "`ifdef"},
          {},  // any context
          {1, SpacingOptions::MustWrap},
      },
      {
          DefaultStyle,
          {yytokentype::PP_endif, "`endif"},
          {yytokentype::PP_ifdef, "`ifdef"},
          {},  // any context
          {1, SpacingOptions::MustWrap},
      },
      {
          DefaultStyle,
          {yytokentype::TK_EOL_COMMENT, "//comment1"},
          {yytokentype::PP_ifndef, "`ifndef"},
          {},  // any context
          {1, SpacingOptions::MustWrap},
      },
      {
          DefaultStyle,
          {yytokentype::SymbolIdentifier, "id"},
          {yytokentype::PP_ifndef, "`ifndef"},
          {},  // any context
          {1, SpacingOptions::MustWrap},
      },
      {
          DefaultStyle,
          {';', ";"},
          {yytokentype::PP_ifndef, "`ifndef"},
          {},  // any context
          {1, SpacingOptions::MustWrap},
      },
      {
          DefaultStyle,
          {SymbolIdentifier, "ID"},
          {yytokentype::PP_else, "`else"},
          {},  // any context
          {1, SpacingOptions::MustWrap},
      },
      {
          DefaultStyle,
          {';', ";"},
          {yytokentype::PP_else, "`else"},
          {},  // any context
          {1, SpacingOptions::MustWrap},
      },
      {
          DefaultStyle,
          {yytokentype::PP_endif, "`endif"},
          {yytokentype::PP_else, "`else"},
          {},  // any context
          {1, SpacingOptions::MustWrap},
      },
      {
          DefaultStyle,
          {yytokentype::PP_include, "`include"},
          {TK_StringLiteral, "\"lost/file.svh\""},
          {},                             // any context
          {1, SpacingOptions::Undecided}, /* or MustAppend? */
      },
      {
          DefaultStyle,
          {yytokentype::PP_include, "`include"},
          {TK_EvalStringLiteral, "`\"lost/file.svh`\""},
          {},                             // any context
          {1, SpacingOptions::Undecided}, /* or MustAppend? */
      },
      {
          DefaultStyle,
          {TK_StringLiteral, "\"lost/file.svh\""},
          {yytokentype::PP_include, "`include"},
          {},  // any context
          {1, SpacingOptions::MustWrap},
      },
      {
          DefaultStyle,
          {yytokentype::PP_else, "`else"},
          {yytokentype::PP_include, "`include"},
          {},  // any context
          {1, SpacingOptions::MustWrap},
      },
      {
          DefaultStyle,
          {';', ";"},
          {yytokentype::PP_include, "`include"},
          {},  // any context
          {1, SpacingOptions::MustWrap},
      },
      {
          DefaultStyle,
          {SymbolIdentifier, "ID"},
          {yytokentype::PP_include, "`include"},
          {},  // any context
          {1, SpacingOptions::MustWrap},
      },
      {
          DefaultStyle,
          {TK_StringLiteral, "\"lost/file.svh\""},
          {yytokentype::PP_define, "`define"},
          {},  // any context
          {1, SpacingOptions::MustWrap},
      },
      {
          DefaultStyle,
          {yytokentype::PP_else, "`else"},
          {yytokentype::PP_define, "`define"},
          {},  // any context
          {1, SpacingOptions::MustWrap},
      },
      {
          DefaultStyle,
          {';', ";"},
          {yytokentype::PP_define, "`define"},
          {},  // any context
          {1, SpacingOptions::MustWrap},
      },
      {
          DefaultStyle,
          {SymbolIdentifier, "ID"},
          {yytokentype::PP_define, "`define"},
          {},  // any context
          {1, SpacingOptions::MustWrap},
      },
      {
          DefaultStyle,
          {TK_StringLiteral, "\"lost/file.svh\""},
          {yytokentype::PP_undef, "`undef"},
          {},  // any context
          {1, SpacingOptions::MustWrap},
      },
      {
          DefaultStyle,
          {yytokentype::PP_else, "`else"},
          {yytokentype::PP_undef, "`undef"},
          {},  // any context
          {1, SpacingOptions::MustWrap},
      },
      {
          DefaultStyle,
          {';', ";"},
          {yytokentype::PP_undef, "`undef"},
          {},  // any context
          {1, SpacingOptions::MustWrap},
      },
      {
          DefaultStyle,
          {SymbolIdentifier, "ID"},
          {yytokentype::PP_undef, "`undef"},
          {},  // any context
          {1, SpacingOptions::MustWrap},
      },
      {
          DefaultStyle,
          {yytokentype::TK_endfunction, "endfunction"},
          {yytokentype::PP_undef, "`undef"},
          {},  // any context
          {1, SpacingOptions::MustWrap},
      },
      {
          DefaultStyle,
          {yytokentype::TK_end, "end"},
          {yytokentype::PP_undef, "`undef"},
          {},  // any context
          {1, SpacingOptions::MustWrap},
      },
      {
          DefaultStyle,
          {yytokentype::MacroCallCloseToEndLine, ")"},
          {yytokentype::PP_undef, "`undef"},
          {},  // any context
          {1, SpacingOptions::MustWrap},
      },

      // right token = MacroCallId or MacroIdentifier
      {
          DefaultStyle,
          {SymbolIdentifier, "ID"},
          {yytokentype::MacroCallId, "`uvm_foo_macro"},
          {},  // any context
          {1, SpacingOptions::Undecided},
      },
      {
          DefaultStyle,
          {SymbolIdentifier, "ID"},
          {yytokentype::MacroIdentifier, "`uvm_foo_id"},
          {},  // any context
          {1, SpacingOptions::Undecided},
      },
      {
          DefaultStyle,
          {';', ";"},
          {yytokentype::MacroCallId, "`uvm_foo_macro"},
          {},  // any context
          {1, SpacingOptions::Undecided},
      },
      {
          DefaultStyle,
          {';', ";"},
          {yytokentype::MacroIdentifier, "`uvm_foo_id"},
          {},  // any context
          {1, SpacingOptions::Undecided},
      },
      {
          DefaultStyle,
          {TK_EOL_COMMENT, "//comment"},
          {yytokentype::MacroCallId, "`uvm_foo_macro"},
          {},  // any context
          {1, SpacingOptions::MustWrap},
      },
      {
          DefaultStyle,
          {TK_EOL_COMMENT, "//comment"},
          {yytokentype::MacroIdentifier, "`uvm_foo_id"},
          {},  // any context
          {1, SpacingOptions::MustWrap},
      },
      {
          DefaultStyle,
          {TK_COMMENT_BLOCK, "/*comment*/"},
          {yytokentype::MacroCallId, "`uvm_foo_macro"},
          {},  // any context
          {1, SpacingOptions::Undecided},
      },
      {
          DefaultStyle,
          {TK_COMMENT_BLOCK, "/*comment*/"},
          {yytokentype::MacroIdentifier, "`uvm_foo_id"},
          {},  // any context
          {1, SpacingOptions::Undecided},
      },
      {
          DefaultStyle,
          {PP_else, "`else"},
          {yytokentype::MacroCallId, "`uvm_foo_macro"},
          {},  // any context
          {1, SpacingOptions::MustWrap},
      },
      {
          DefaultStyle,
          {PP_else, "`else"},
          {yytokentype::MacroIdentifier, "`uvm_foo_id"},
          {},  // any context
          {1, SpacingOptions::MustWrap},
      },
      {
          DefaultStyle,
          {PP_endif, "`endif"},
          {yytokentype::MacroCallId, "`uvm_foo_macro"},
          {},  // any context
          {1, SpacingOptions::MustWrap},
      },
      {
          DefaultStyle,
          {PP_endif, "`endif"},
          {yytokentype::MacroIdentifier, "`uvm_foo_id"},
          {},  // any context
          {1, SpacingOptions::MustWrap},
      },
      {
          DefaultStyle,
          {yytokentype::MacroCallId, "`uvm_foo_macro"},
          {yytokentype::MacroCallId, "`uvm_foo_macro"},
          {},  // any context
          {1, SpacingOptions::Undecided},
      },
      {
          DefaultStyle,
          {yytokentype::MacroCallId, "`uvm_foo_macro"},
          {yytokentype::MacroIdentifier, "`uvm_foo_id"},
          {},  // any context
          {1, SpacingOptions::Undecided},
      },
      {
          DefaultStyle,
          {yytokentype::MacroIdentifier, "`uvm_foo_id"},
          {yytokentype::MacroCallId, "`uvm_foo_macro"},
          {},  // any context
          {1, SpacingOptions::Undecided},
      },
      {
          DefaultStyle,
          {yytokentype::MacroIdentifier, "`uvm_foo_id"},
          {yytokentype::MacroIdentifier, "`uvm_foo_id"},
          {},  // any context
          {1, SpacingOptions::Undecided},
      },
      {
          DefaultStyle,
          {')', ")"},
          {yytokentype::MacroCallId, "`uvm_foo_macro"},
          {},  // any context
          {1, SpacingOptions::Undecided},
      },
      {
          DefaultStyle,
          {')', ")"},
          {yytokentype::MacroIdentifier, "`uvm_foo_id"},
          {},  // any context
          {1, SpacingOptions::Undecided},
      },
      {
          DefaultStyle,
          {yytokentype::MacroCallCloseToEndLine, ")"},
          {yytokentype::MacroCallId, "`uvm_foo_macro"},
          {},  // any context
          {1, SpacingOptions::MustWrap},
      },
      {
          DefaultStyle,
          {yytokentype::MacroCallCloseToEndLine, ")"},
          {yytokentype::MacroIdentifier, "`uvm_foo_id"},
          {},  // any context
          {1, SpacingOptions::MustWrap},
      },
      {
          DefaultStyle,
          {yytokentype::MacroCallCloseToEndLine, ")"},
          {';', ";"},
          {},  // any context
          {0, SpacingOptions::Undecided},
      },

      // Without context, default is to treat '-' as binary.
      {
          DefaultStyle,
          {'-', "-"},                         // left token
          {yytokentype::TK_DecNumber, "42"},  // right token
          {},                                 // context
          // ExpectedInterTokenInfo:
          // spaces_required, break_decision
          {1, SpacingOptions::Undecided},
      },
      {
          DefaultStyle,
          {'-', "-"},
          {yytokentype::TK_DecNumber, "42"},
          {NodeEnum::kBinaryExpression},
          {1, SpacingOptions::Undecided},
      },

      // Handle '-' as a unary prefix expression.
      {
          DefaultStyle,
          {'-', "-"},                          // left token
          {yytokentype::TK_DecNumber, "42"},   // right token
          {NodeEnum::kUnaryPrefixExpression},  // context
          {0, SpacingOptions::MustAppend},
      },
      {
          DefaultStyle,
          {'-', "-"},
          {yytokentype::SymbolIdentifier, "xyz"},
          {NodeEnum::kUnaryPrefixExpression},
          {0, SpacingOptions::MustAppend},
      },
      {
          DefaultStyle,
          {'-', "-"},
          {'(', "("},
          {NodeEnum::kUnaryPrefixExpression},
          {0, SpacingOptions::MustAppend},
      },
      {
          DefaultStyle,
          {'-', "-"},
          {yytokentype::MacroIdItem, "`FOO"},
          {NodeEnum::kUnaryPrefixExpression},
          {0, SpacingOptions::MustAppend},
      },

      // Handle '&' as binary
      {
          DefaultStyle,
          {'&', "&"},
          {'~', "~"},
          {},  // unspecified context
          {1, SpacingOptions::Undecided},
      },

      // Handle '&' as unary
      {
          DefaultStyle,
          {'&', "&"},
          {yytokentype::TK_DecNumber, "42"},
          {NodeEnum::kUnaryPrefixExpression},
          {0, SpacingOptions::MustAppend},
      },
      {
          DefaultStyle,
          {'&', "&"},
          {yytokentype::SymbolIdentifier, "foo"},
          {NodeEnum::kUnaryPrefixExpression},
          {0, SpacingOptions::MustAppend},
      },
      {
          DefaultStyle,
          {'&', "&"},
          {'(', "("},
          {NodeEnum::kUnaryPrefixExpression},
          {0, SpacingOptions::MustAppend},
      },
      {
          DefaultStyle,
          {'&', "&"},
          {'{', "{"},
          {NodeEnum::kUnaryPrefixExpression},
          {0, SpacingOptions::MustAppend},
      },

      // Handle '|' as binary
      {
          DefaultStyle,
          {'|', "|"},
          {'~', "~"},
          {},  // unspecified context
          {1, SpacingOptions::Undecided},
      },

      // Handle '|' as unary
      {
          DefaultStyle,
          {'|', "|"},
          {yytokentype::TK_DecNumber, "42"},
          {NodeEnum::kUnaryPrefixExpression},
          {0, SpacingOptions::MustAppend},
      },
      {
          DefaultStyle,
          {'|', "|"},
          {yytokentype::SymbolIdentifier, "foo"},
          {NodeEnum::kUnaryPrefixExpression},
          {0, SpacingOptions::MustAppend},
      },
      {
          DefaultStyle,
          {'|', "|"},
          {'(', "("},
          {NodeEnum::kUnaryPrefixExpression},
          {0, SpacingOptions::MustAppend},
      },
      {
          DefaultStyle,
          {'|', "|"},
          {'{', "{"},
          {NodeEnum::kUnaryPrefixExpression},
          {0, SpacingOptions::MustAppend},
      },

      // Handle '^' as binary
      {
          DefaultStyle,
          {'^', "^"},
          {'~', "~"},
          {},  // unspecified context
          {1, SpacingOptions::Undecided},
      },

      // Handle '^' as unary
      {
          DefaultStyle,
          {'^', "^"},
          {yytokentype::TK_DecNumber, "42"},
          {NodeEnum::kUnaryPrefixExpression},
          {0, SpacingOptions::MustAppend},
      },
      {
          DefaultStyle,
          {'^', "^"},
          {yytokentype::SymbolIdentifier, "foo"},
          {NodeEnum::kUnaryPrefixExpression},
          {0, SpacingOptions::MustAppend},
      },
      {
          DefaultStyle,
          {'^', "^"},
          {'(', "("},
          {NodeEnum::kUnaryPrefixExpression},
          {0, SpacingOptions::MustAppend},
      },
      {
          DefaultStyle,
          {'^', "^"},
          {'{', "{"},
          {NodeEnum::kUnaryPrefixExpression},
          {0, SpacingOptions::MustAppend},
      },

      // Test '~' unary token
      {
          DefaultStyle,
          {'~', "~"},
          {'(', "("},
          {NodeEnum::kUnaryPrefixExpression},
          {0, SpacingOptions::MustAppend},
      },
      {
          DefaultStyle,
          {'~', "~"},
          {yytokentype::SymbolIdentifier, "foo"},
          {NodeEnum::kUnaryPrefixExpression},
          {0, SpacingOptions::MustAppend},
      },

      // Two unary operators
      {
          DefaultStyle,
          {'~', "~"},
          {'~', "~"},
          {NodeEnum::kUnaryPrefixExpression},
          {0, SpacingOptions::MustAppend},
      },

      // Inside dimension ranges, force space preservation if not around ':'
      {
          DefaultStyle,
          {'*', "*"},
          {yytokentype::SymbolIdentifier, "foo"},
          {},
          {1, SpacingOptions::Undecided},
      },
      {
          DefaultStyle,
          {yytokentype::SymbolIdentifier, "foo"},
          {'*', "*"},
          {},
          {1, SpacingOptions::Undecided},
      },
      {
          DefaultStyle,
          {'*', "*"},
          {yytokentype::SymbolIdentifier, "foo"},
          {NodeEnum::kDimensionRange},
          {1, SpacingOptions::Preserve},
      },
      {
          DefaultStyle,
          {yytokentype::SymbolIdentifier, "foo"},
          {'*', "*"},
          {NodeEnum::kDimensionRange},
          {1, SpacingOptions::Preserve},
      },
      {
          DefaultStyle,
          {':', ":"},
          {yytokentype::SymbolIdentifier, "foo"},
          {NodeEnum::kDimensionRange},
          {0, SpacingOptions::Undecided},
      },
      {
          DefaultStyle,
          {yytokentype::SymbolIdentifier, "foo"},
          {':', ":"},
          {NodeEnum::kDimensionRange},
          {0, SpacingOptions::Undecided},
      },

      // spacing between ranges of multi-dimension arrays
      {
          DefaultStyle,
          {']', "]"},
          {'[', "["},
          {},  // any context
          {0, SpacingOptions::Undecided},
      },

      // spacing before first '[' of packed arrays in declarations
      {
          DefaultStyle,
          {yytokentype::TK_logic, "logic"},
          {'[', "["},
          {},  // unspecified context
          {0, SpacingOptions::Undecided},
      },
      {
          DefaultStyle,
          {yytokentype::SymbolIdentifier, "mytype1"},
          {'[', "["},
          {},  // unspecified context, this covers index expressions
          {0, SpacingOptions::Undecided},
      },
      {
          DefaultStyle,
          {yytokentype::TK_logic, "logic"},
          {'[', "["},
          {NodeEnum::kPackedDimensions},
          {1, SpacingOptions::Undecided},
      },
      {
          DefaultStyle,
          {yytokentype::SymbolIdentifier, "mytype2"},
          {'[', "["},
          {NodeEnum::kPackedDimensions},
          {1, SpacingOptions::Undecided},
      },
      {
          DefaultStyle,
          {yytokentype::SymbolIdentifier, "id1"},
          {'[', "["},
          {NodeEnum::kPackedDimensions, NodeEnum::kExpression},
          {0, SpacingOptions::Undecided},
      },

      // spacing after last ']' of packed arrays in declarations
      {
          DefaultStyle,
          {']', "]"},
          {yytokentype::SymbolIdentifier, "id_a"},
          {},  // unspecified context
          {1, SpacingOptions::Undecided},
      },
      {
          DefaultStyle,
          {']', "]"},
          {yytokentype::SymbolIdentifier, "id_b"},
          {NodeEnum::kUnqualifiedId},
          {1, SpacingOptions::Undecided},
      },
      {
          DefaultStyle,
          {']', "]"},
          {yytokentype::SymbolIdentifier, "id_c"},
          {NodeEnum::kDataTypeImplicitBasicIdDimensions,
           NodeEnum::kUnqualifiedId},
          {1, SpacingOptions::Undecided},
      },

      // "foo ()" in "module foo();"
      {
          DefaultStyle,
          {yytokentype::SymbolIdentifier, "foo"},
          {'(', "("},
          {/* unspecified context */},
          {0, SpacingOptions::Undecided},
      },
      {
          DefaultStyle,
          {yytokentype::SymbolIdentifier, "foo"},
          {'(', "("},
          {NodeEnum::kModuleHeader},
          {1, SpacingOptions::Undecided},
      },

      // "a(" in "foo bar (.a(b));": instantiation with named ports
      {
          DefaultStyle,
          {yytokentype::SymbolIdentifier, "foo"},
          {'(', "("},
          {NodeEnum::kGateInstance},
          {1, SpacingOptions::Undecided},
      },
      {
          DefaultStyle,
          {yytokentype::SymbolIdentifier, "foo"},
          {'(', "("},
          {NodeEnum::kActualNamedPort},
          {0, SpacingOptions::Undecided},
      },
      {
          DefaultStyle,
          {yytokentype::SymbolIdentifier, "foo"},
          {'(', "("},
          {NodeEnum::kGateInstance, NodeEnum::kActualNamedPort},
          {0, SpacingOptions::Undecided},
      },
      {
          DefaultStyle,
          {yytokentype::SymbolIdentifier, "foo"},
          {'(', "("},
          {NodeEnum::kModuleHeader, NodeEnum::kPort},
          {0, SpacingOptions::Undecided},
      },

      // cases for the heavily overloaded ':'

      // ':' on the right, anything else on the left
      {
          DefaultStyle,
          {yytokentype::SymbolIdentifier, "x"},
          {':', ":"},
          {/* unspecified context */},
          kUnhandledSpacing,
      },
      {
          // a ? b : c (ternary expression)
          DefaultStyle,
          {yytokentype::SymbolIdentifier, "b"},
          {':', ":"},
          {NodeEnum::kTernaryExpression},
          {1, SpacingOptions::Undecided},
      },
      {
          // a ? 111 : c (ternary expression)
          DefaultStyle,
          {yytokentype::TK_DecNumber, "111"},
          {':', ":"},
          {NodeEnum::kTernaryExpression},
          {1, SpacingOptions::Undecided},
      },
      {
          // a ? "1" : c (ternary expression)
          DefaultStyle,
          {yytokentype::TK_StringLiteral, "\"1\""},
          {':', ":"},
          {NodeEnum::kTernaryExpression},
          {1, SpacingOptions::Undecided},
      },
      {
          // a ? (1) : c (ternary expression)
          DefaultStyle,
          {')', ":"},
          {':', ":"},
          {NodeEnum::kTernaryExpression},
          {1, SpacingOptions::Undecided},
      },

      // ':' on the left, anything else on the right
      {
          DefaultStyle,
          {':', ":"},
          {yytokentype::SymbolIdentifier, "x"},
          {/* unspecified context */},
          {1, SpacingOptions::Undecided},
      },
      {
          // a ? b : c (ternary expression)
          DefaultStyle,
          {':', ":"},
          {yytokentype::SymbolIdentifier, "c"},
          {NodeEnum::kTernaryExpression},
          {1, SpacingOptions::Undecided},
      },
      {
          // a ? b : 7 (ternary expression)
          DefaultStyle,
          {':', ":"},
          {yytokentype::TK_DecNumber, "7"},
          {NodeEnum::kTernaryExpression},
          {1, SpacingOptions::Undecided},
      },
      {
          // a ? b : "7" (ternary expression)
          DefaultStyle,
          {':', ":"},
          {yytokentype::TK_StringLiteral, "\"7\""},
          {NodeEnum::kTernaryExpression},
          {1, SpacingOptions::Undecided},
      },
      {
          // a ? b : (7) (ternary expression)
          DefaultStyle,
          {':', ":"},
          {'(', "("},
          {NodeEnum::kTernaryExpression},
          {1, SpacingOptions::Undecided},
      },

      // ':' in labels
      // ':' before and after keywords:
      {
          // "begin :"
          DefaultStyle,
          {yytokentype::TK_begin, "begin"},
          {':', ":"},
          {/* unspecified context */},
          {1, SpacingOptions::Undecided},
      },
      {
          // ": begin"
          DefaultStyle,
          {':', ":"},
          {yytokentype::TK_begin, "begin"},
          {/* unspecified context */},
          {1, SpacingOptions::Undecided},
      },
      {
          // "fork :"
          DefaultStyle,
          {yytokentype::TK_fork, "fork"},
          {':', ":"},
          {/* unspecified context */},
          {1, SpacingOptions::Undecided},
      },
      {
          // "end :"
          DefaultStyle,
          {yytokentype::TK_end, "end"},
          {':', ":"},
          {/* unspecified context */},
          {1, SpacingOptions::Undecided},
      },
      {
          // "endclass :"
          DefaultStyle,
          {yytokentype::TK_endclass, "endclass"},
          {':', ":"},
          {/* unspecified context */},
          {1, SpacingOptions::Undecided},
      },
      {
          // "endfunction :"
          DefaultStyle,
          {yytokentype::TK_endfunction, "endfunction"},
          {':', ":"},
          {/* unspecified context */},
          {1, SpacingOptions::Undecided},
      },
      {
          // "endtask :"
          DefaultStyle,
          {yytokentype::TK_endtask, "endtask"},
          {':', ":"},
          {/* unspecified context */},
          {1, SpacingOptions::Undecided},
      },
      {
          // "endmodule :"
          DefaultStyle,
          {yytokentype::TK_endmodule, "endmodule"},
          {':', ":"},
          {/* unspecified context */},
          {1, SpacingOptions::Undecided},
      },
      {
          // "endpackage :"
          DefaultStyle,
          {yytokentype::TK_endpackage, "endpackage"},
          {':', ":"},
          {/* unspecified context */},
          {1, SpacingOptions::Undecided},
      },
      {
          // "endinterface :"
          DefaultStyle,
          {yytokentype::TK_endinterface, "endinterface"},
          {':', ":"},
          {/* unspecified context */},
          {1, SpacingOptions::Undecided},
      },
      {
          // "endproperty :"
          DefaultStyle,
          {yytokentype::TK_endproperty, "endproperty"},
          {':', ":"},
          {/* unspecified context */},
          {1, SpacingOptions::Undecided},
      },
      {
          // "endclocking :"
          DefaultStyle,
          {yytokentype::TK_endclocking, "endclocking"},
          {':', ":"},
          {/* unspecified context */},
          {1, SpacingOptions::Undecided},
      },
      // endcase and endgenerate do not get labels

      // ':' before and after label identifiers:
      {
          // "id :"
          DefaultStyle,
          {yytokentype::SymbolIdentifier, "id"},
          {':', ":"},
          {/* unspecified context */},
          kUnhandledSpacing,
      },
      {
          // "id :"
          DefaultStyle,
          {yytokentype::SymbolIdentifier, "id"},
          {':', ":"},
          {NodeEnum::kBlockIdentifier},
          {1, SpacingOptions::Undecided},
      },
      {
          // "id : begin ..."
          DefaultStyle,
          {yytokentype::SymbolIdentifier, "id"},
          {':', ":"},
          {NodeEnum::kLabeledStatement},
          {1, SpacingOptions::Undecided},
      },
      {
          // "id :"
          DefaultStyle,
          {yytokentype::SymbolIdentifier, "id"},
          {':', ":"},
          {NodeEnum::kCaseItem},
          {0, SpacingOptions::Undecided},
      },
      {
          // "id :"
          DefaultStyle,
          {yytokentype::SymbolIdentifier, "id"},
          {':', ":"},
          {NodeEnum::kCaseInsideItem},
          {0, SpacingOptions::Undecided},
      },
      {
          // "id :"
          DefaultStyle,
          {yytokentype::SymbolIdentifier, "id"},
          {':', ":"},
          {NodeEnum::kCasePatternItem},
          {0, SpacingOptions::Undecided},
      },
      {
          // "id :"
          DefaultStyle,
          {yytokentype::SymbolIdentifier, "id"},
          {':', ":"},
          {NodeEnum::kGenerateCaseItem},
          {0, SpacingOptions::Undecided},
      },
      {
          // "id :"
          DefaultStyle,
          {yytokentype::SymbolIdentifier, "id"},
          {':', ":"},
          {NodeEnum::kPropertyCaseItem},
          {0, SpacingOptions::Undecided},
      },
      {
          // "id :"
          DefaultStyle,
          {yytokentype::SymbolIdentifier, "id"},
          {':', ":"},
          {NodeEnum::kRandSequenceCaseItem},
          {0, SpacingOptions::Undecided},
      },
      {
          // ": id"
          DefaultStyle,
          {':', ":"},
          {yytokentype::SymbolIdentifier, "id"},
          {/* unspecified context */},
          {1, SpacingOptions::Undecided},
      },
      {
          // ": id"
          DefaultStyle,
          {':', ":"},
          {yytokentype::SymbolIdentifier, "id"},
          {NodeEnum::kLabel},
          {1, SpacingOptions::Undecided},
      },
      // Shift operators
      {
          // foo = 1 << width;
          DefaultStyle,
          {yytokentype::TK_DecNumber, "1"},
          {yytokentype::TK_LS, "<<"},
          {/* unspecified context */},
          {1, SpacingOptions::Undecided},
      },
      {
          // foo = 1 << width;
          DefaultStyle,
          {yytokentype::TK_LS, "<<"},
          {yytokentype::SymbolIdentifier, "width"},
          {/* unspecified context */},
          {1, SpacingOptions::Undecided},
      },
      {
          // foo = bar << 4;
          DefaultStyle,
          {yytokentype::SymbolIdentifier, "bar"},
          {yytokentype::TK_LS, "<<"},
          {/* unspecified context */},
          {1, SpacingOptions::Undecided},
      },
      {
          // foo = bar << 4;
          DefaultStyle,
          {yytokentype::TK_LS, "<<"},
          {yytokentype::TK_DecNumber, "4"},
          {/* unspecified context */},
          {1, SpacingOptions::Undecided},
      },
      {
          // foo = `VAL << 4;
          DefaultStyle,
          {yytokentype::MacroIdentifier, "`VAL"},
          {yytokentype::TK_LS, "<<"},
          {/* unspecified context */},
          {1, SpacingOptions::Undecided},
      },
      {
          // foo = bar << `SIZE;
          DefaultStyle,
          {yytokentype::TK_LS, "<<"},
          {yytokentype::MacroIdentifier, "`SIZE"},
          {/* unspecified context */},
          {1, SpacingOptions::Undecided},
      },
      {
          // foo = 1 >> width;
          DefaultStyle,
          {yytokentype::TK_DecNumber, "1"},
          {yytokentype::TK_RS, ">>"},
          {/* unspecified context */},
          {1, SpacingOptions::Undecided},
      },
      {
          // foo = 1 >> width;
          DefaultStyle,
          {yytokentype::TK_RS, ">>"},
          {yytokentype::SymbolIdentifier, "width"},
          {/* unspecified context */},
          {1, SpacingOptions::Undecided},
      },
      {
          // foo = bar >> 4;
          DefaultStyle,
          {yytokentype::SymbolIdentifier, "bar"},
          {yytokentype::TK_RS, ">>"},
          {/* unspecified context */},
          {1, SpacingOptions::Undecided},
      },
      {
          // foo = bar >> 4;
          DefaultStyle,
          {yytokentype::TK_RS, ">>"},
          {yytokentype::TK_DecNumber, "4"},
          {/* unspecified context */},
          {1, SpacingOptions::Undecided},
      },
      {
          // foo = `VAL >> 4;
          DefaultStyle,
          {yytokentype::MacroIdentifier, "`VAL"},
          {yytokentype::TK_RS, ">>"},
          {/* unspecified context */},
          {1, SpacingOptions::Undecided},
      },
      {
          // foo = bar >> `SIZE;
          DefaultStyle,
          {yytokentype::TK_RS, ">>"},
          {yytokentype::MacroIdentifier, "`SIZE"},
          {/* unspecified context */},
          {1, SpacingOptions::Undecided},
      },
      // Streaming operators
      {
          // foo = {<<{bar}};
          DefaultStyle,
          {'=', "="},
          {'{', "{"},
          {NodeEnum::kStreamingConcatenation},
          {1, SpacingOptions::Undecided},
      },
      {
          // foo = {<<{bar}};
          DefaultStyle,
          {'{', "{"},
          {yytokentype::TK_LS, "<<"},
          {NodeEnum::kStreamingConcatenation},
          {0, SpacingOptions::Undecided},
      },
      {
          // foo = {<<{bar}};
          DefaultStyle,
          {yytokentype::TK_LS, "<<"},
          {'{', "{"},
          {NodeEnum::kStreamingConcatenation},
          {0, SpacingOptions::Undecided},
      },
      {
          // foo = {<<{bar}};
          DefaultStyle,
          {'{', "{"},
          {yytokentype::SymbolIdentifier, "bar"},
          {NodeEnum::kStreamingConcatenation},
          {0, SpacingOptions::Undecided},
      },
      {
          // foo = {<<{bar}};
          DefaultStyle,
          {yytokentype::SymbolIdentifier, "bar"},
          {'}', "}"},
          {NodeEnum::kStreamingConcatenation},
          {0, SpacingOptions::Undecided},
      },
      {
          // foo = {<<4{bar}};
          DefaultStyle,
          {yytokentype::TK_LS, "<<"},
          {yytokentype::TK_DecNumber, "4"},
          {NodeEnum::kStreamingConcatenation},
          {0, SpacingOptions::Undecided},
      },
      {
          // foo = {<<4{bar}};
          DefaultStyle,
          {yytokentype::TK_DecNumber, "4"},
          {'{', "{"},
          {NodeEnum::kStreamingConcatenation},
          {0, SpacingOptions::Undecided},
      },
      {
          // foo = {<<byte{bar}};
          DefaultStyle,
          {yytokentype::TK_LS, "<<"},
          {yytokentype::TK_byte, "byte"},
          {NodeEnum::kStreamingConcatenation},
          {0, SpacingOptions::Undecided},
      },
      {
          // foo = {<<byte{bar}};
          DefaultStyle,
          {yytokentype::TK_byte, "byte"},
          {'{', "{"},
          {NodeEnum::kStreamingConcatenation},
          {0, SpacingOptions::Undecided},
      },
      {
          // foo = {<<type_t{bar}};
          DefaultStyle,
          {yytokentype::TK_LS, "<<"},
          {yytokentype::SymbolIdentifier, "type_t"},
          {NodeEnum::kStreamingConcatenation},
          {0, SpacingOptions::Undecided},
      },
      {
          // foo = {<<type_t{bar}};
          DefaultStyle,
          {yytokentype::SymbolIdentifier, "type_t"},
          {'{', "{"},
          {NodeEnum::kStreamingConcatenation},
          {0, SpacingOptions::Undecided},
      },
      {
          // foo = {<<`GET_TYPE{bar}};
          DefaultStyle,
          {yytokentype::TK_LS, "<<"},
          {yytokentype::MacroIdentifier, "`GET_TYPE"},
          {NodeEnum::kStreamingConcatenation},
          {0, SpacingOptions::Undecided},
      },
      {
          // foo = {<<`GET_TYPE{bar}};
          DefaultStyle,
          {yytokentype::MacroIdentifier, "`GET_TYPE"},
          {'{', "{"},
          {NodeEnum::kStreamingConcatenation},
          {0, SpacingOptions::Undecided},
      },
      {
          // foo = {>>{bar}};
          DefaultStyle,
          {'=', "="},
          {'{', "{"},
          {NodeEnum::kStreamingConcatenation},
          {1, SpacingOptions::Undecided},
      },
      {
          // foo = {>>{bar}};
          DefaultStyle,
          {'{', "{"},
          {yytokentype::TK_RS, ">>"},
          {NodeEnum::kStreamingConcatenation},
          {0, SpacingOptions::Undecided},
      },
      {
          // foo = {>>{bar}};
          DefaultStyle,
          {yytokentype::TK_RS, ">>"},
          {'{', "{"},
          {NodeEnum::kStreamingConcatenation},
          {0, SpacingOptions::Undecided},
      },
      {
          // foo = {>>{bar}};
          DefaultStyle,
          {'{', "{"},
          {yytokentype::SymbolIdentifier, "bar"},
          {NodeEnum::kStreamingConcatenation},
          {0, SpacingOptions::Undecided},
      },
      {
          // foo = {>>{bar}};
          DefaultStyle,
          {yytokentype::SymbolIdentifier, "bar"},
          {'}', "}"},
          {NodeEnum::kStreamingConcatenation},
          {0, SpacingOptions::Undecided},
      },
      {
          // foo = {>>4{bar}};
          DefaultStyle,
          {yytokentype::TK_RS, ">>"},
          {yytokentype::TK_DecNumber, "4"},
          {NodeEnum::kStreamingConcatenation},
          {0, SpacingOptions::Undecided},
      },
      {
          // foo = {>>4{bar}};
          DefaultStyle,
          {yytokentype::TK_DecNumber, "4"},
          {'{', "{"},
          {NodeEnum::kStreamingConcatenation},
          {0, SpacingOptions::Undecided},
      },
      {
          // foo = {>>byte{bar}};
          DefaultStyle,
          {yytokentype::TK_RS, ">>"},
          {yytokentype::TK_byte, "byte"},
          {NodeEnum::kStreamingConcatenation},
          {0, SpacingOptions::Undecided},
      },
      {
          // foo = {>>byte{bar}};
          DefaultStyle,
          {yytokentype::TK_byte, "byte"},
          {'{', "{"},
          {NodeEnum::kStreamingConcatenation},
          {0, SpacingOptions::Undecided},
      },
      {
          // foo = {>>type_t{bar}};
          DefaultStyle,
          {yytokentype::TK_RS, ">>"},
          {yytokentype::SymbolIdentifier, "type_t"},
          {NodeEnum::kStreamingConcatenation},
          {0, SpacingOptions::Undecided},
      },
      {
          // foo = {>>type_t{bar}};
          DefaultStyle,
          {yytokentype::SymbolIdentifier, "type_t"},
          {'{', "{"},
          {NodeEnum::kStreamingConcatenation},
          {0, SpacingOptions::Undecided},
      },
      {
          // foo = {>>`GET_TYPE{bar}};
          DefaultStyle,
          {yytokentype::TK_RS, ">>"},
          {yytokentype::MacroIdentifier, "`GET_TYPE"},
          {NodeEnum::kStreamingConcatenation},
          {0, SpacingOptions::Undecided},
      },
      {
          // foo = {>>`GET_TYPE{bar}};
          DefaultStyle,
          {yytokentype::MacroIdentifier, "`GET_TYPE"},
          {'{', "{"},
          {NodeEnum::kStreamingConcatenation},
          {0, SpacingOptions::Undecided},
      },
      // ':' in bit slicing and array indexing
      {
          // [1:0]
          DefaultStyle,
          {yytokentype::TK_DecNumber, "1"},
          {':', ":"},
          {NodeEnum::kSelectVariableDimension},
          // no spaces preceding ':' in unit test context
          {0, SpacingOptions::Undecided},
      },
      {
          // [1:0]
          DefaultStyle,
          {':', ":"},
          {yytokentype::TK_DecNumber, "0"},
          {NodeEnum::kSelectVariableDimension},
          // no spaces preceding ':' in unit test context
          {0, SpacingOptions::Undecided},
      },
      {
          // [a:b]
          DefaultStyle,
          {SymbolIdentifier, "a"},
          {':', ":"},
          {NodeEnum::kSelectVariableDimension},
          // no spaces preceding ':' in unit test context
          {0, SpacingOptions::Undecided},
      },
      {
          // [a:b]
          DefaultStyle,
          {':', ":"},
          {SymbolIdentifier, "b"},
          {NodeEnum::kSelectVariableDimension},
          // no spaces preceding ':' in unit test context
          {0, SpacingOptions::Undecided},
      },
      {
          // [1:0]
          DefaultStyle,
          {yytokentype::TK_DecNumber, "1"},
          {':', ":"},
          {NodeEnum::kDimensionRange},
          // no spaces preceding ':' in unit test context
          {0, SpacingOptions::Undecided},
      },
      {
          // [1:0]
          DefaultStyle,
          {':', ":"},
          {yytokentype::TK_DecNumber, "0"},
          {NodeEnum::kDimensionRange},
          // no spaces preceding ':' in unit test context
          {0, SpacingOptions::Undecided},
      },
      {
          // [a:b]
          DefaultStyle,
          {SymbolIdentifier, "a"},
          {':', ":"},
          {NodeEnum::kDimensionRange},
          // no spaces preceding ':' in unit test context
          {0, SpacingOptions::Undecided},
      },
      {
          // [a:b]
          DefaultStyle,
          {':', ":"},
          {SymbolIdentifier, "b"},
          {NodeEnum::kDimensionRange},
          // no spaces preceding ':' in unit test context
          {0, SpacingOptions::Undecided},
      },
      {
          // [1:0]
          DefaultStyle,
          {yytokentype::TK_DecNumber, "1"},
          {':', ":"},
          {NodeEnum::kDimensionSlice},
          // no spaces preceding ':' in unit test context
          {0, SpacingOptions::Undecided},
      },
      {
          // [1:0]
          DefaultStyle,
          {':', ":"},
          {yytokentype::TK_DecNumber, "0"},
          {NodeEnum::kDimensionSlice},
          // no spaces preceding ':' in unit test context
          {0, SpacingOptions::Undecided},
      },
      {
          // [a:b]
          DefaultStyle,
          {SymbolIdentifier, "a"},
          {':', ":"},
          {NodeEnum::kDimensionSlice},
          // no spaces preceding ':' in unit test context
          {0, SpacingOptions::Undecided},
      },
      {
          // [a:b]
          DefaultStyle,
          {':', ":"},
          {SymbolIdentifier, "b"},
          {NodeEnum::kDimensionSlice},
          // no spaces preceding ':' in unit test context
          {0, SpacingOptions::Undecided},
      },
      {
          // [1:0]
          DefaultStyle,
          {yytokentype::TK_DecNumber, "1"},
          {':', ":"},
          {NodeEnum::kValueRange},
          // no spaces preceding ':' in unit test context
          {1, SpacingOptions::Undecided},
      },
      {
          // [1:0]
          DefaultStyle,
          {':', ":"},
          {yytokentype::TK_DecNumber, "0"},
          {NodeEnum::kValueRange},
          // no spaces preceding ':' in unit test context
          {1, SpacingOptions::Undecided},
      },
      {
          // [a:b]
          DefaultStyle,
          {SymbolIdentifier, "a"},
          {':', ":"},
          {NodeEnum::kValueRange},
          // no spaces preceding ':' in unit test context
          {1, SpacingOptions::Undecided},
      },
      {
          // [a:b]
          DefaultStyle,
          {':', ":"},
          {SymbolIdentifier, "b"},
          {NodeEnum::kValueRange},
          // no spaces preceding ':' in unit test context
          {1, SpacingOptions::Undecided},
      },

      // name: coverpoint
      {
          DefaultStyle,
          {SymbolIdentifier, "foo_cp"},
          {':', ":"},
          {NodeEnum::kCoverPoint},
          {0, SpacingOptions::Undecided},
      },
      // coverpoint foo {
      {
          DefaultStyle,
          {SymbolIdentifier, "cpaddr"},
          {'{', "{"},
          {NodeEnum::kCoverPoint, NodeEnum::kBraceGroup},
          {1, SpacingOptions::Undecided},
      },

      // x < y (binary operator)
      {
          DefaultStyle,
          {SymbolIdentifier, "id"},
          {'<', "<"},
          {},  // default context
          {1, SpacingOptions::Undecided},
      },
      {
          DefaultStyle,
          {TK_DecNumber, "7"},
          {'<', "<"},
          {},  // default context
          {1, SpacingOptions::Undecided},
      },
      {
          DefaultStyle,
          {')', ")"},
          {'<', "<"},
          {},  // default context
          {1, SpacingOptions::Undecided},
      },
      {
          DefaultStyle,
          {'<', "<"},
          {SymbolIdentifier, "id"},
          {},  // default context
          {1, SpacingOptions::Undecided},
      },
      {
          DefaultStyle,
          {'<', "<"},
          {TK_DecNumber, "7"},
          {},  // default context
          {1, SpacingOptions::Undecided},
      },
      {
          DefaultStyle,
          {'<', "<"},
          {'(', "("},
          {},  // default context
          {1, SpacingOptions::Undecided},
      },

      // x > y (binary operator)
      {
          DefaultStyle,
          {SymbolIdentifier, "id"},
          {'>', ">"},
          {},  // default context
          {1, SpacingOptions::Undecided},
      },
      {
          DefaultStyle,
          {TK_DecNumber, "7"},
          {'>', ">"},
          {},  // default context
          {1, SpacingOptions::Undecided},
      },
      {
          DefaultStyle,
          {')', ")"},
          {'>', ">"},
          {},  // default context
          {1, SpacingOptions::Undecided},
      },
      {
          DefaultStyle,
          {'>', ">"},
          {SymbolIdentifier, "id"},
          {},  // default context
          {1, SpacingOptions::Undecided},
      },
      {
          DefaultStyle,
          {'>', ">"},
          {TK_DecNumber, "7"},
          {},  // default context
          {1, SpacingOptions::Undecided},
      },
      {
          DefaultStyle,
          {'>', ">"},
          {'(', "("},
          {},  // default context
          {1, SpacingOptions::Undecided},
      },

      // '@' on the right
      {
          DefaultStyle,
          {TK_always, "always"},
          {'@', "@"},
          {},  // default context
          {1, SpacingOptions::Undecided},
      },
      {
          DefaultStyle,
          {SymbolIdentifier, "cblock"},
          {'@', "@"},
          {},  // default context
          {1, SpacingOptions::Undecided},
      },
      // '@' on the left
      {
          DefaultStyle,
          {'@', "@"},
          {'(', "("},
          {},  // default context
          {0, SpacingOptions::Undecided},
      },
      {
          DefaultStyle,
          {'@', "@"},
          {'*', "*"},  // not a binary operator in this case
          {},          // default context
          {0, SpacingOptions::Undecided},
      },
      {
          DefaultStyle,
          {'@', "@"},
          {SymbolIdentifier, "clock_a"},
          {},  // default context
          {0, SpacingOptions::Undecided},
      },

      // '#' on the right
      {
          DefaultStyle,
          {SymbolIdentifier, "id_before_pound"},
          {'#', "#"},
          {},  // default context
               // no spaces preceding ':' in unit test context
          {1, SpacingOptions::Undecided},
      },

      // '}' on the left
      {
          DefaultStyle,
          {'}', "}"},
          {SymbolIdentifier, "id_before_open_brace"},
          {},  // default context
          {1, SpacingOptions::Undecided},
      },
      {
          DefaultStyle,
          {'}', "}"},
          {',', ","},
          {},  // default context
          {0, SpacingOptions::Undecided},
      },
      {
          DefaultStyle,
          {'}', "}"},
          {';', ";"},
          {},  // default context
          {0, SpacingOptions::Undecided},
      },
      {
          DefaultStyle,
          {'}', "}"},
          {'}', "}"},
          {},  // default context
          {0, SpacingOptions::Undecided},
      },

      // '{' on the right
      {
          DefaultStyle,
          {SymbolIdentifier, "id_before_open_brace"},
          {'{', "{"},
          {},  // default context
          {0, SpacingOptions::Undecided},
      },
      {
          DefaultStyle,
          {TK_unique, "unique"},
          {'{', "{"},
          {},  // default context
          {1, SpacingOptions::Undecided},
      },
      {
          DefaultStyle,
          {TK_with, "with"},
          {'{', "{"},
          {},  // default context
          {1, SpacingOptions::Undecided},
      },
      {
          // constraint c_id {
          DefaultStyle,
          {SymbolIdentifier, "id_before_open_brace"},
          {'{', "{"},
          {NodeEnum::kConstraintDeclaration, NodeEnum::kBraceGroup},
          {1, SpacingOptions::Undecided},
      },

      // ';' on the left
      {
          DefaultStyle,
          {';', ";"},
          {SymbolIdentifier, "id_after_semi"},
          {},  // default context
          {1, SpacingOptions::Undecided},
      },
      {
          DefaultStyle,
          {SemicolonEndOfAssertionVariableDeclarations, ";"},
          {SymbolIdentifier, "id_after_semi"},
          {},  // default context
          {1, SpacingOptions::Undecided},
      },

      // ';' on the right
      {
          DefaultStyle,
          {SymbolIdentifier, "id"},
          {';', ";"},
          {},  // default context
          {0, SpacingOptions::Undecided},
      },
      {
          DefaultStyle,
          {SymbolIdentifier, "id"},
          {SemicolonEndOfAssertionVariableDeclarations, ";"},
          {},  // default context
          {0, SpacingOptions::Undecided},
      },
      {
          DefaultStyle,
          {')', ")"},
          {';', ";"},
          {},                              // default context
          {0, SpacingOptions::Undecided},  // could be MustAppend too
      },
      {
          DefaultStyle,
          {')', ")"},
          {SemicolonEndOfAssertionVariableDeclarations, ";"},
          {},                              // default context
          {0, SpacingOptions::Undecided},  // could be MustAppend too
      },

      // keyword on right
      {
          DefaultStyle,
          {TK_DecNumber, "1"},
          {TK_begin, "begin"},
          {},  // default context
          {1, SpacingOptions::Undecided},
      },
      {
          DefaultStyle,
          {TK_begin, "begin"},
          {TK_begin, "begin"},
          {},  // default context
          {1, SpacingOptions::Undecided},
      },
      {
          DefaultStyle,
          {TK_begin, "begin"},
          {TK_end, "end"},
          {},  // default context
          {1, SpacingOptions::MustWrap},
      },
      {
          DefaultStyle,
          {TK_end, "end"},
          {TK_begin, "begin"},
          {},  // default context
          {1, SpacingOptions::Undecided},
      },
      {
          DefaultStyle,
          {TK_default, "default"},
          {TK_clocking, "clocking"},
          {},  // default context
          {1, SpacingOptions::Undecided},
      },
      {
          DefaultStyle,
          {TK_default, "default"},
          {TK_disable, "disable"},
          {},  // default context
          {1, SpacingOptions::Undecided},
      },
      {
          DefaultStyle,
          {TK_disable, "disable"},
          {TK_iff, "iff"},
          {},  // default context
          {1, SpacingOptions::Undecided},
      },
      {
          DefaultStyle,
          {TK_disable, "disable"},
          {TK_soft, "soft"},
          {},  // default context
          {1, SpacingOptions::Undecided},
      },
      {
          DefaultStyle,
          {TK_extern, "extern"},
          {TK_forkjoin, "forkjoin"},
          {},  // default context
          {1, SpacingOptions::Undecided},
      },
      {
          DefaultStyle,
          {TK_input, "input"},
          {TK_logic, "logic"},
          {},  // default context
          {1, SpacingOptions::Undecided},
      },
      {
          DefaultStyle,
          {TK_var, "var"},
          {TK_logic, "logic"},
          {},  // default context
          {1, SpacingOptions::Undecided},
      },
      {
          DefaultStyle,
          {TK_output, "output"},
          {TK_reg, "reg"},
          {},  // default context
          {1, SpacingOptions::Undecided},
      },
      {
          DefaultStyle,
          {TK_static, "static"},
          {TK_constraint, "constraint"},
          {},  // default context
          {1, SpacingOptions::Undecided},
      },
      {
          DefaultStyle,
          {TK_parameter, "parameter"},
          {TK_type, "type"},
          {},  // default context
          {1, SpacingOptions::Undecided},
      },
      {
          DefaultStyle,
          {TK_virtual, "virtual"},
          {TK_interface, "interface"},
          {},  // default context
          {1, SpacingOptions::Undecided},
      },
      {
          DefaultStyle,
          {TK_const, "const"},
          {TK_ref, "ref"},
          {},  // default context
          {1, SpacingOptions::Undecided},
      },
      {
          DefaultStyle,
          {TK_union, "union"},
          {TK_tagged, "tagged"},
          {},  // default context
          {1, SpacingOptions::Undecided},
      },
      {
          DefaultStyle,
          {';', ";"},
          {TK_end, "end"},
          {},  // default context
          {1, SpacingOptions::MustWrap},
      },
      {
          DefaultStyle,
          {';', ";"},
          {TK_endfunction, "endfunction"},
          {},  // default context
          {1, SpacingOptions::MustWrap},
      },
      {
          DefaultStyle,
          {';', ";"},
          {TK_endtask, "endtask"},
          {},  // default context
          {1, SpacingOptions::MustWrap},
      },
      {
          DefaultStyle,
          {';', ";"},
          {TK_endclass, "endclass"},
          {},  // default context
          {1, SpacingOptions::MustWrap},
      },
      {
          DefaultStyle,
          {';', ";"},
          {TK_endpackage, "endpackage"},
          {},  // default context
          {1, SpacingOptions::MustWrap},
      },
      {
          DefaultStyle,
          {SymbolIdentifier, "nettype_id"},
          {TK_with, "with"},
          {},  // default context
          {1, SpacingOptions::Undecided},
      },
      {
          DefaultStyle,
          {SymbolIdentifier, "id"},
          {TK_until, "until"},
          {},  // default context
          {1, SpacingOptions::Undecided},
      },
      {
          DefaultStyle,
          {',', ","},
          {TK_highz0, "highz0"},
          {},  // default context
          {1, SpacingOptions::Undecided},
      },
      {
          DefaultStyle,
          {',', ","},
          {TK_highz1, "highz1"},
          {},  // default context
          {1, SpacingOptions::Undecided},
      },
      // Entries spacing in primitives
      {
          // 1 0 : ? : -;
          DefaultStyle,
          {'1', "1"},
          {'0', "0"},
          {NodeEnum::kUdpSequenceEntry},
          {1, SpacingOptions::Undecided},
      },
      {
          // 1 0 : ? : -;
          DefaultStyle,
          {'0', "0"},
          {':', ":"},
          {NodeEnum::kUdpSequenceEntry},
          {1, SpacingOptions::Undecided},
      },
      {
          // 1 0 : ? : -;
          DefaultStyle,
          {':', ":"},
          {'?', "?"},
          {NodeEnum::kUdpSequenceEntry},
          {1, SpacingOptions::Undecided},
      },
      {
          // 1 0 : ? : -;
          DefaultStyle,
          {'?', "?"},
          {':', ":"},
          {NodeEnum::kUdpSequenceEntry},
          {1, SpacingOptions::Undecided},
      },
      {
          // 1 0 : ? : -;
          DefaultStyle,
          {':', ":"},
          {'-', "-"},
          {NodeEnum::kUdpSequenceEntry},
          {1, SpacingOptions::Undecided},
      },
      {
          // 1 0 : ? : -;
          DefaultStyle,
          {'-', "-"},
          {';', ";"},
          {NodeEnum::kUdpSequenceEntry},
          {0, SpacingOptions::Undecided},
      },
      {
          // 1 0 : -;
          DefaultStyle,
          {'1', "1"},
          {'0', "0"},
          {NodeEnum::kUdpCombEntry},
          {1, SpacingOptions::Undecided},
      },
      {
          // 1 0 : -;
          DefaultStyle,
          {'0', "0"},
          {':', ":"},
          {NodeEnum::kUdpCombEntry},
          {1, SpacingOptions::Undecided},
      },
      {
          // 1 0 : -;
          DefaultStyle,
          {':', ":"},
          {'-', "-"},
          {NodeEnum::kUdpCombEntry},
          {1, SpacingOptions::Undecided},
      },
      {
          // 1 0 : -;
          DefaultStyle,
          {'-', "-"},
          {';', ";"},
          {NodeEnum::kUdpCombEntry},
          {0, SpacingOptions::Undecided},
      },
  };
  int test_index = 0;
  for (const auto& test_case : kTestCases) {
    VLOG(1) << "test_index[" << test_index << "]:";
    PreFormatToken left(&test_case.left_token);
    PreFormatToken right(&test_case.right_token);
    // Classify token type into major category
    left.format_token_enum = GetFormatTokenType(yytokentype(left.TokenEnum()));
    right.format_token_enum =
        GetFormatTokenType(yytokentype(right.TokenEnum()));

    ASSERT_NE(right.format_token_enum, FormatTokenType::eol_comment)
        << "This test does not support cases examining intertoken text. "
           "Move the test case to AnnotateBreakAroundComments instead.";

    VLOG(1) << "context: " << test_case.context;
    AnnotateFormatToken(test_case.style, left, &right, test_case.context);
    EXPECT_EQ(test_case.expected_annotation, right.before)
        << " with left=" << left.Text() << " and right=" << right.Text();
    ++test_index;
  }
}  // NOLINT(readability/fn_size)

struct AnnotateBreakAroundCommentsTestCase {
  FormatStyle style;

  int left_token_enum;
  absl::string_view left_token_string;

  absl::string_view whitespace_between;

  int right_token_enum;
  absl::string_view right_token_string;

  InitializedSyntaxTreeContext context;
  ExpectedInterTokenInfo expected_annotation;
};

TEST(TokenAnnotatorTest, AnnotateBreakAroundComments) {
  const std::initializer_list<AnnotateBreakAroundCommentsTestCase> kTestCases =
      {
          {// No comments
           DefaultStyle,
           '=',  // left token
           "=",
           "   ",                      // whitespace between
           yytokentype::TK_DecNumber,  // right token
           "0",
           {/* unspecified context */},
           {1, SpacingOptions::Undecided}},
          {// //comment1
           // //comment2
           DefaultStyle,
           yytokentype::TK_EOL_COMMENT,
           "//comment1",
           "\n",
           yytokentype::TK_EOL_COMMENT,
           "//comment2",
           {},
           {2, SpacingOptions::MustWrap}},
          {// 0 // comment
           DefaultStyle,
           yytokentype::TK_DecNumber,
           "0",
           "   ",
           yytokentype::TK_EOL_COMMENT,
           "// comment",
           {/* unspecified context */},
           {2, SpacingOptions::MustAppend}},
          {// 0// comment
           DefaultStyle,
           yytokentype::TK_DecNumber,
           "0",
           "",
           yytokentype::TK_EOL_COMMENT,
           "// comment",
           {/* unspecified context */},
           {2, SpacingOptions::MustAppend}},
          {// 0 \n  // comment
           DefaultStyle,
           yytokentype::TK_DecNumber,
           "0",
           " \n  ",
           yytokentype::TK_EOL_COMMENT,
           "// comment",
           {/* unspecified context */},
           {2, SpacingOptions::Undecided}},
          {// // comment 1 \n  // comment 2
           DefaultStyle,
           yytokentype::TK_EOL_COMMENT,
           "// comment 1",
           " \n  ",
           yytokentype::TK_EOL_COMMENT,
           "// comment 2",
           {/* unspecified context */},
           {2, SpacingOptions::MustWrap}},
          {// /* comment 1 */ \n  // comment 2
           DefaultStyle,
           yytokentype::TK_COMMENT_BLOCK,
           "/* comment 1 */",
           " \n  ",
           yytokentype::TK_EOL_COMMENT,
           "// comment 2",
           {/* unspecified context */},
           {2, SpacingOptions::Undecided}},
          {// /* comment 1 */  // comment 2
           DefaultStyle,
           yytokentype::TK_COMMENT_BLOCK,
           "/* comment 1 */",
           " ",
           yytokentype::TK_EOL_COMMENT,
           "// comment 2",
           {/* unspecified context */},
           {2, SpacingOptions::MustAppend}},
          {// ;  // comment 2
           DefaultStyle,
           ';',
           ";",
           " ",
           yytokentype::TK_EOL_COMMENT,
           "// comment 2",
           {/* unspecified context */},
           {2, SpacingOptions::MustAppend}},
          {// ; \n // comment 2
           DefaultStyle,
           ';',
           ";",
           " \n",
           yytokentype::TK_EOL_COMMENT,
           "// comment 2",
           {/* unspecified context */},
           {2, SpacingOptions::Undecided}},
          {// ,  // comment 2
           DefaultStyle,
           ',',
           ",",
           " ",
           yytokentype::TK_EOL_COMMENT,
           "// comment 2",
           {/* unspecified context */},
           {2, SpacingOptions::MustAppend}},
          {// , \n // comment 2
           DefaultStyle,
           ',',
           ",",
           "\n ",
           yytokentype::TK_EOL_COMMENT,
           "// comment 2",
           {/* unspecified context */},
           {2, SpacingOptions::Undecided}},
          {// begin  // comment 2
           DefaultStyle,
           yytokentype::TK_begin,
           "begin",
           " ",
           yytokentype::TK_EOL_COMMENT,
           "// comment 2",
           {/* unspecified context */},
           {2, SpacingOptions::MustAppend}},
          {// begin \n // comment 2
           DefaultStyle,
           yytokentype::TK_begin,
           "begin",
           "\n",
           yytokentype::TK_EOL_COMMENT,
           "// comment 2",
           {/* unspecified context */},
           {2, SpacingOptions::Undecided}},
          {// else  // comment 2
           DefaultStyle,
           yytokentype::TK_else,
           "else",
           " ",
           yytokentype::TK_EOL_COMMENT,
           "// comment 2",
           {/* unspecified context */},
           {2, SpacingOptions::MustAppend}},
          {// else \n // comment 2
           DefaultStyle,
           yytokentype::TK_else,
           "else",
           " \n  ",
           yytokentype::TK_EOL_COMMENT,
           "// comment 2",
           {/* unspecified context */},
           {2, SpacingOptions::Undecided}},
          {// end  // comment 2
           DefaultStyle,
           yytokentype::TK_end,
           "end",
           " ",
           yytokentype::TK_EOL_COMMENT,
           "// comment 2",
           {/* unspecified context */},
           {2, SpacingOptions::MustAppend}},
          {// end \n // comment 2
           DefaultStyle,
           yytokentype::TK_end,
           "end",
           "  \n ",
           yytokentype::TK_EOL_COMMENT,
           "// comment 2",
           {/* unspecified context */},
           {2, SpacingOptions::Undecided}},
          {// generate  // comment 2
           DefaultStyle,
           yytokentype::TK_generate,
           "generate",
           " ",
           yytokentype::TK_EOL_COMMENT,
           "// comment 2",
           {/* unspecified context */},
           {2, SpacingOptions::MustAppend}},
          {// generate \n // comment 2
           DefaultStyle,
           yytokentype::TK_generate,
           "generate",
           "  \n",
           yytokentype::TK_EOL_COMMENT,
           "// comment 2",
           {/* unspecified context */},
           {2, SpacingOptions::Undecided}},
          {// if  // comment 2
           DefaultStyle,
           yytokentype::TK_if,
           "if",
           " ",
           yytokentype::TK_EOL_COMMENT,
           "// comment 2",
           {/* unspecified context */},
           {2, SpacingOptions::MustAppend}},
          {// if \n\n // comment 2
           DefaultStyle,
           yytokentype::TK_if,
           "if",
           " \n\n ",
           yytokentype::TK_EOL_COMMENT,
           "// comment 2",
           {/* unspecified context */},
           {2, SpacingOptions::Undecided}},
          {
              DefaultStyle,
              yytokentype::MacroCallCloseToEndLine,
              ")",
              " ",
              yytokentype::TK_COMMENT_BLOCK,
              "/*comment*/",
              {/* unspecified context */},
              {2, SpacingOptions::Undecided},  // could be append
          },
          {
              DefaultStyle,
              yytokentype::MacroCallCloseToEndLine,
              ")",
              "\n",
              yytokentype::TK_COMMENT_BLOCK,
              "/*comment*/",
              {/* unspecified context */},
              {2, SpacingOptions::Undecided},
          },
          {
              DefaultStyle,
              yytokentype::MacroCallCloseToEndLine,
              ")",
              " ",
              yytokentype::TK_EOL_COMMENT,
              "//comment",
              {/* unspecified context */},
              {2, SpacingOptions::MustAppend},
          },
          {
              DefaultStyle,
              yytokentype::MacroCallCloseToEndLine,
              ")",
              "\n",
              yytokentype::TK_EOL_COMMENT,
              "//comment",
              {/* unspecified context */},
              {2, SpacingOptions::Undecided},
          },
          // Comments in UDP entries
          {
              // 1  /*comment*/ 0 : -;
              DefaultStyle,
              '1',
              "1",
              "",
              yytokentype::TK_COMMENT_BLOCK,
              "/* comment */",
              {NodeEnum::kUdpCombEntry},
              {2, SpacingOptions::Undecided},
          },
          {
              // 1  /*comment*/ 0 : -;
              DefaultStyle,
              yytokentype::TK_COMMENT_BLOCK,
              "/* comment */",
              "",
              '0',
              "0",
              {NodeEnum::kUdpCombEntry},
              {1, SpacingOptions::Undecided},
          },
          {
              // 1 0  // comment\n : -;
              DefaultStyle,
              '0',
              "0",
              "",
              yytokentype::TK_EOL_COMMENT,
              "// comment",
              {NodeEnum::kUdpCombEntry},
              {2, SpacingOptions::MustAppend},
          },
          {
              // 1  /*comment*/ 0 : -;
              DefaultStyle,
              '1',
              "1",
              "",
              yytokentype::TK_COMMENT_BLOCK,
              "/* comment */",
              {NodeEnum::kUdpSequenceEntry},
              {2, SpacingOptions::Undecided},
          },
          {
              // 1  /*comment*/ 0 : -;
              DefaultStyle,
              yytokentype::TK_COMMENT_BLOCK,
              "/* comment */",
              "",
              '0',
              "0",
              {NodeEnum::kUdpSequenceEntry},
              {1, SpacingOptions::Undecided},
          },
          {
              // 1 0  // comment\n : -;
              DefaultStyle,
              '0',
              "0",
              "",
              yytokentype::TK_EOL_COMMENT,
              "// comment",
              {NodeEnum::kUdpSequenceEntry},
              {2, SpacingOptions::MustAppend},
          },
          {
              // input  /* comment */ i;
              DefaultStyle,
              yytokentype::TK_input,
              "input",
              "",
              yytokentype::TK_COMMENT_BLOCK,
              "/* comment */",
              {NodeEnum::kUdpPortDeclaration},
              {2, SpacingOptions::Undecided},
          },
          {
              // input  /* comment */ i;
              DefaultStyle,
              yytokentype::TK_COMMENT_BLOCK,
              "/* comment */",
              "",
              yytokentype::SymbolIdentifier,
              "i",
              {NodeEnum::kUdpPortDeclaration},
              {1, SpacingOptions::Undecided},
          },
          {
              // input i  /* comment */;
              DefaultStyle,
              yytokentype::SymbolIdentifier,
              "i",
              "",
              yytokentype::TK_COMMENT_BLOCK,
              "/* comment */",
              {NodeEnum::kUdpPortDeclaration},
              {2, SpacingOptions::Undecided},
          },
          {
              // input i;  // comment\n
              DefaultStyle,
              ';',
              ";",
              "",
              yytokentype::TK_EOL_COMMENT,
              "// comment",
              {NodeEnum::kUdpPortDeclaration},
              {2, SpacingOptions::MustAppend},
          },
      };
  int test_index = 0;
  for (const auto& test_case : kTestCases) {
    VLOG(1) << "test_index[" << test_index << "]:";

    verible::TokenInfoTestData test_data = {
        {test_case.left_token_enum, test_case.left_token_string},
        test_case.whitespace_between,
        {test_case.right_token_enum, test_case.right_token_string}};

    auto token_vector = test_data.FindImportantTokens();
    ASSERT_EQ(token_vector.size(), 2);

    PreFormatToken left(&token_vector[0]);
    PreFormatToken right(&token_vector[1]);

    left.format_token_enum = GetFormatTokenType(yytokentype(left.TokenEnum()));
    right.format_token_enum =
        GetFormatTokenType(yytokentype(right.TokenEnum()));

    VLOG(1) << "context: " << test_case.context;
    AnnotateFormatToken(test_case.style, left, &right, test_case.context);
    EXPECT_EQ(test_case.expected_annotation, right.before)
        << "Index: " << test_index << " Context: " << test_case.context << " "
        << " with left=" << left.Text() << " and right=" << right.Text();
    ++test_index;
  }
}

}  // namespace
}  // namespace formatter
}  // namespace verilog
