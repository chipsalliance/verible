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
                                const verible::SyntaxTreeContext& prev_context,
                                const verible::SyntaxTreeContext& curr_context);

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
  for (const auto& token : test_case.input_tokens)
    stream << ' ' << token.text();
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
      Push(parent);
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
  InitializedSyntaxTreeContext left_context;
  InitializedSyntaxTreeContext right_context;
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
           {{verilog_tokentype::TK_EOL_COMMENT, "//comment1"},
            {verilog_tokentype::TK_EOL_COMMENT, "//comment2"}}},

          // If there is no newline before comment, it will be appended
          // (  //comment
          {DefaultStyle,
           0,
           {{0, SpacingOptions::Undecided},  //
            {2, SpacingOptions::MustAppend}},
           {{'(', "("}, {verilog_tokentype::TK_EOL_COMMENT, "//comment"}}},

          // [  //comment
          {DefaultStyle,
           0,
           {{0, SpacingOptions::Undecided},  //
            {2, SpacingOptions::MustAppend}},
           {{'[', "["}, {verilog_tokentype::TK_EOL_COMMENT, "//comment"}}},

          // {  //comment
          {DefaultStyle,
           0,
           {{0, SpacingOptions::Undecided},  //
            {2, SpacingOptions::MustAppend}},
           {{'{', "{"}, {verilog_tokentype::TK_EOL_COMMENT, "//comment"}}},

          // ,  //comment
          {DefaultStyle,
           0,
           {{0, SpacingOptions::Undecided},  //
            {2, SpacingOptions::MustAppend}},
           {{',', ","}, {verilog_tokentype::TK_EOL_COMMENT, "//comment"}}},

          // ;  //comment
          {DefaultStyle,
           0,
           {{0, SpacingOptions::Undecided},  //
            {2, SpacingOptions::MustAppend}},
           {{';', ";"}, {verilog_tokentype::TK_EOL_COMMENT, "//comment"}}},

          // module foo();
          {DefaultStyle,
           0,
           {{0, SpacingOptions::Undecided},
            {1, SpacingOptions::Undecided},
            {0, SpacingOptions::Undecided},
            {0, SpacingOptions::Undecided},
            {0, SpacingOptions::Undecided}},
           {{verilog_tokentype::TK_module, "module"},
            {verilog_tokentype::SymbolIdentifier, "foo"},
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
           {{verilog_tokentype::TK_module, "module"},
            {verilog_tokentype::SymbolIdentifier, "foo"},
            {'(', "("},
            {verilog_tokentype::SymbolIdentifier, "a"},
            {',', ","},
            {verilog_tokentype::SymbolIdentifier, "b"},
            {')', ")"},
            {';', ";"}}},

          // module with_params #() ();
          {DefaultStyle,
           0,
           {{0, SpacingOptions::Undecided},
            {1, SpacingOptions::Undecided},   // with_params
            {1, SpacingOptions::Undecided},   // #
            {0, SpacingOptions::MustAppend},  // (
            {0, SpacingOptions::Undecided},   // )
            {1, SpacingOptions::Undecided},   // (
            {0, SpacingOptions::Undecided},   // )
            {0, SpacingOptions::Undecided}},  // ;
           {{verilog_tokentype::TK_module, "module"},
            {verilog_tokentype::SymbolIdentifier, "with_params"},
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
           {{verilog_tokentype::SymbolIdentifier, "a"},
            {'=', "="},
            {verilog_tokentype::SymbolIdentifier, "b"},
            {'[', "["},
            {verilog_tokentype::SymbolIdentifier, "c"},
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
           {{verilog_tokentype::SymbolIdentifier, "b"},
            {'[', "["},
            {verilog_tokentype::SymbolIdentifier, "c"},
            {']', "]"},
            {'[', "["},
            {verilog_tokentype::SymbolIdentifier, "d"},
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
           {{verilog_tokentype::TK_always, "always"},
            {'@', "@"},
            {'(', "("},
            {verilog_tokentype::TK_posedge, "TK_posedge"},
            {verilog_tokentype::SymbolIdentifier, "clk"},
            {')', ")"}}},

          // `WIDTH'(s) (casting operator)
          {DefaultStyle,
           0,
           {{0, SpacingOptions::Undecided},
            {0, SpacingOptions::Undecided},
            {0, SpacingOptions::Undecided},
            {0, SpacingOptions::Undecided},
            {0, SpacingOptions::Undecided}},
           {{verilog_tokentype::MacroIdItem, "`WIDTH"},
            {'\'', "'"},
            {'(', "("},
            {verilog_tokentype::SymbolIdentifier, "s"},
            {')', ")"}}},

          // string'(s) (casting operator)
          {DefaultStyle,
           0,
           {{0, SpacingOptions::Undecided},
            {0, SpacingOptions::Undecided},
            {0, SpacingOptions::Undecided},
            {0, SpacingOptions::Undecided},
            {0, SpacingOptions::Undecided}},
           {{verilog_tokentype::TK_string, "string"},
            {'\'', "'"},
            {'(', "("},
            {verilog_tokentype::SymbolIdentifier, "s"},
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
           {{verilog_tokentype::TK_void, "void"},
            {'\'', "'"},
            {'(', "("},
            {verilog_tokentype::SymbolIdentifier, "f"},
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
           {{verilog_tokentype::TK_DecNumber, "12"},
            {'\'', "'"},
            {'{', "{"},
            {verilog_tokentype::TK_DecNumber, "34"},
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
           {{verilog_tokentype::SymbolIdentifier, "k"},
            {'(', "("},
            {')', ")"},
            {'\'', "'"},
            {'(', "("},
            {verilog_tokentype::SymbolIdentifier, "s"},
            {')', ")"}}},

          // #1 $display
          {
              DefaultStyle,
              0,
              {{0, SpacingOptions::Undecided},
               {0, SpacingOptions::MustAppend},
               {1, SpacingOptions::Undecided}},
              {{'#', "#"},
               {verilog_tokentype::TK_DecNumber, "1"},
               {verilog_tokentype::SystemTFIdentifier, "$display"}},
          },

          // 666 777
          {
              DefaultStyle,
              0,
              {{0, SpacingOptions::Undecided}, {1, SpacingOptions::Undecided}},
              {{verilog_tokentype::TK_DecNumber, "666"},
               {verilog_tokentype::TK_DecNumber, "777"}},
          },

          // 5678 dance
          {
              DefaultStyle,
              0,
              {{0, SpacingOptions::Undecided}, {1, SpacingOptions::Undecided}},
              {{verilog_tokentype::TK_DecNumber, "5678"},
               {verilog_tokentype::SymbolIdentifier, "dance"}},
          },

          // id 4321
          {
              DefaultStyle,
              0,
              {{0, SpacingOptions::Undecided}, {1, SpacingOptions::Undecided}},
              {{verilog_tokentype::SymbolIdentifier, "id"},
               {verilog_tokentype::TK_DecNumber, "4321"}},
          },

          // id1 id2
          {
              DefaultStyle,
              0,
              {{0, SpacingOptions::Undecided}, {1, SpacingOptions::Undecided}},
              {{verilog_tokentype::SymbolIdentifier, "id1"},
               {verilog_tokentype::SymbolIdentifier, "id2"}},
          },

          // class mate
          {
              DefaultStyle,
              0,
              {{0, SpacingOptions::Undecided}, {1, SpacingOptions::Undecided}},
              {{verilog_tokentype::TK_class, "class"},
               {verilog_tokentype::SymbolIdentifier, "mate"}},
          },

          // id module
          {
              DefaultStyle,
              0,
              {{0, SpacingOptions::Undecided}, {1, SpacingOptions::Undecided}},
              {{verilog_tokentype::SymbolIdentifier, "lunar"},
               {verilog_tokentype::TK_module, "module"}},
          },

          // class 1337
          {
              DefaultStyle,
              0,
              {{0, SpacingOptions::Undecided}, {1, SpacingOptions::Undecided}},
              {{verilog_tokentype::TK_class, "class"},
               {verilog_tokentype::TK_DecNumber, "1337"}},
          },

          // 987 module
          {
              DefaultStyle,
              0,
              {{0, SpacingOptions::Undecided}, {1, SpacingOptions::Undecided}},
              {{verilog_tokentype::TK_DecNumber, "987"},
               {verilog_tokentype::TK_module, "module"}},
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
           {{verilog_tokentype::SymbolIdentifier, "a"},
            {'=', "="},
            {verilog_tokentype::TK_DecNumber, "16"},
            {verilog_tokentype::TK_HexBase, "'h"},
            {verilog_tokentype::TK_HexDigits, "c0ffee"},
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
           {{verilog_tokentype::SymbolIdentifier, "a"},
            {'=', "="},
            {verilog_tokentype::TK_DecNumber, "8"},
            {verilog_tokentype::TK_BinBase, "'b"},
            {verilog_tokentype::TK_BinDigits, "1001_0110"},
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
           {{verilog_tokentype::SymbolIdentifier, "a"},
            {'=', "="},
            {verilog_tokentype::TK_DecNumber, "4"},
            {verilog_tokentype::TK_DecBase, "'d"},
            {verilog_tokentype::TK_DecDigits, "10"},
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
           {{verilog_tokentype::SymbolIdentifier, "a"},
            {'=', "="},
            {verilog_tokentype::TK_DecNumber, "8"},
            {verilog_tokentype::TK_OctBase, "'o"},
            {verilog_tokentype::TK_OctDigits, "100"},
            {';', ";"}}},

          // a = 'hc0ffee;
          {DefaultStyle,
           0,
           {{0, SpacingOptions::Undecided},
            {1, SpacingOptions::Undecided},
            {1, SpacingOptions::Undecided},
            {0, SpacingOptions::MustAppend},
            {0, SpacingOptions::Undecided}},
           {{verilog_tokentype::SymbolIdentifier, "a"},
            {'=', "="},
            {verilog_tokentype::TK_HexBase, "'h"},
            {verilog_tokentype::TK_HexDigits, "c0ffee"},
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
           {{verilog_tokentype::SymbolIdentifier, "a"},
            {'=', "="},
            {verilog_tokentype::SymbolIdentifier, "funk"},
            {'(', "("},
            {verilog_tokentype::TK_BinBase, "'b"},
            {verilog_tokentype::TK_BinDigits, "0"},
            {',', ","},
            {verilog_tokentype::TK_DecBase, "'d"},
            {verilog_tokentype::TK_DecDigits, "8"},
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
           {{verilog_tokentype::SymbolIdentifier, "a"},
            {'=', "="},
            {verilog_tokentype::TK_BinBase, "'b"},
            {verilog_tokentype::TK_BinDigits, "0"},
            {'+', "+"},
            {verilog_tokentype::TK_DecBase, "'d"},
            {verilog_tokentype::TK_DecDigits, "9"},
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
           {{verilog_tokentype::SymbolIdentifier, "a"},
            {'=', "="},
            {'{', "{"},
            {verilog_tokentype::TK_DecDigits, "3"},
            {'{', "{"},
            {verilog_tokentype::TK_DecDigits, "4"},
            {verilog_tokentype::TK_DecBase, "'d"},
            {verilog_tokentype::TK_DecDigits, "9"},
            {',', ","},
            {verilog_tokentype::TK_DecDigits, "1"},
            {verilog_tokentype::TK_BinBase, "'b"},
            {verilog_tokentype::TK_XZDigits, "z"},
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
                  {verilog_tokentype::SymbolIdentifier, "a"},
                  {'?', "?"},
                  {verilog_tokentype::SymbolIdentifier, "b"},
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
                  {verilog_tokentype::TK_DecNumber, "1"},
                  {'?', "?"},
                  {verilog_tokentype::TK_DecNumber, "2"},
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
                  {verilog_tokentype::TK_StringLiteral, "1"},
                  {'?', "?"},
                  {verilog_tokentype::TK_StringLiteral, "2"},
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
           {{verilog_tokentype::SymbolIdentifier, "b"},
            {'?', "?"},
            {verilog_tokentype::TK_DecNumber, "8"},
            {verilog_tokentype::TK_OctBase, "'o"},
            {verilog_tokentype::TK_OctDigits, "100"},
            {':', ":"},
            {verilog_tokentype::TK_UnBasedNumber, "'0"},
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
           {{verilog_tokentype::SymbolIdentifier, "a"},
            {'=', "="},
            {'(', "("},
            {verilog_tokentype::SymbolIdentifier, "b"},
            {'+', "+"},
            {verilog_tokentype::SymbolIdentifier, "c"},
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
           {{verilog_tokentype::TK_function, "function"},
            {verilog_tokentype::SymbolIdentifier, "foo"},
            {'(', "("},
            {verilog_tokentype::SymbolIdentifier, "name"},
            {'=', "="},
            {verilog_tokentype::TK_StringLiteral, "\"foo\""},
            {')', ")"},
            {';', ";"}}},

          // `define FOO(name = "bar")
          {DefaultStyle,
           0,
           {{0, SpacingOptions::Undecided},   //  `define
            {1, SpacingOptions::MustAppend},  //  FOO
            {0, SpacingOptions::Undecided},   //  (
            {0, SpacingOptions::Undecided},   //  name
            {1, SpacingOptions::Undecided},   //  =
            {1, SpacingOptions::Undecided},   //  "bar"
            {0, SpacingOptions::Undecided}},  //  )
           {{verilog_tokentype::PP_define, "`define"},
            {verilog_tokentype::SymbolIdentifier, "FOO"},
            {'(', "("},
            {verilog_tokentype::SymbolIdentifier, "name"},
            {'=', "="},
            {verilog_tokentype::TK_StringLiteral, "\"bar\""},
            {')', ")"}}},

          // endfunction : funk
          {DefaultStyle,
           1,
           {{0, SpacingOptions::Undecided},
            {1, SpacingOptions::Undecided},
            {1, SpacingOptions::Undecided}},
           {
               {verilog_tokentype::TK_endfunction, "endfunction"},
               {':', ":"},
               {verilog_tokentype::SymbolIdentifier, "funk"},
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
               {verilog_tokentype::TK_case, "case"},
               {'(', "("},
               {verilog_tokentype::SymbolIdentifier, "expr"},
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
               {verilog_tokentype::TK_return, "return"},
               {verilog_tokentype::TK_UnBasedNumber, "0"},
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
               {verilog_tokentype::SymbolIdentifier, "funk"},
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
               {verilog_tokentype::SymbolIdentifier, "funk"},
               {'(', "("},
               {verilog_tokentype::SymbolIdentifier, "arg"},
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
               {verilog_tokentype::SymbolIdentifier, "funk"},
               {'(', "("},
               {verilog_tokentype::TK_StringLiteral, "\"arg\""},
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
               {verilog_tokentype::SymbolIdentifier, "funk"},
               {'(', "("},
               {verilog_tokentype::SymbolIdentifier, "arg1"},
               {',', ","},
               {verilog_tokentype::SymbolIdentifier, "arg2"},
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
               {verilog_tokentype::SymbolIdentifier, "funky"},
               {verilog_tokentype::SymbolIdentifier, "town"},
               {'(', "("},
               {'.', "."},
               {verilog_tokentype::SymbolIdentifier, "f1"},
               {'(', "("},
               {verilog_tokentype::SymbolIdentifier, "arg1"},
               {')', ")"},
               {',', ","},
               {'.', "."},
               {verilog_tokentype::SymbolIdentifier, "f2"},
               {'(', "("},
               {verilog_tokentype::SymbolIdentifier, "arg2"},
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
               {verilog_tokentype::MacroIdentifier, "`ID"},
               {'.', "."},
               {verilog_tokentype::MacroIdentifier, "`ID"},
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
               {verilog_tokentype::SymbolIdentifier, "id"},
               {'.', "."},
               {verilog_tokentype::SymbolIdentifier, "id"},
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
               {verilog_tokentype::TK_super, "super"},
               {'.', "."},
               {verilog_tokentype::SymbolIdentifier, "id"},
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
               {verilog_tokentype::TK_this, "this"},
               {'.', "."},
               {verilog_tokentype::SymbolIdentifier, "id"},
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
               {verilog_tokentype::TK_option, "option"},
               {'.', "."},
               {verilog_tokentype::SymbolIdentifier, "id"},
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
               {verilog_tokentype::MacroCallId, "`MACRO"},
               {'(', "("},
               {verilog_tokentype::MacroCallCloseToEndLine, ")"},
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
               {verilog_tokentype::MacroCallId, "`MACRO"},
               {'(', "("},
               {verilog_tokentype::SymbolIdentifier, "x"},
               {verilog_tokentype::MacroCallCloseToEndLine, ")"},
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
               {verilog_tokentype::MacroCallId, "`MACRO"},
               {'(', "("},
               {verilog_tokentype::SymbolIdentifier, "y"},
               {',', ","},
               {verilog_tokentype::SymbolIdentifier, "x"},
               {verilog_tokentype::MacroCallCloseToEndLine, ")"},
               {';', ";"},
           }},

          // `define FOO
          // `define BAR
          {DefaultStyle,
           1,
           {
               {0, SpacingOptions::Undecided},   // `define
               {1, SpacingOptions::MustAppend},  // FOO
               {0, SpacingOptions::MustAppend},  // "" (empty definition body)
               {0, SpacingOptions::MustWrap},    // `define
               {1, SpacingOptions::MustAppend},  // BAR
               {0, SpacingOptions::MustAppend},  // "" (empty definition body)
           },
           {
               {verilog_tokentype::PP_define, "`define"},
               {verilog_tokentype::SymbolIdentifier, "FOO"},
               {verilog_tokentype::PP_define_body, ""},
               {verilog_tokentype::PP_define, "`define"},
               {verilog_tokentype::SymbolIdentifier, "BAR"},
               {verilog_tokentype::PP_define_body, ""},
           }},

          // `define FOO 1
          // `define BAR 2
          {DefaultStyle,
           1,
           {
               {0, SpacingOptions::Undecided},   // `define
               {1, SpacingOptions::MustAppend},  // FOO
               {1, SpacingOptions::MustAppend},  // 1
               {1, SpacingOptions::MustWrap},    // `define
               {1, SpacingOptions::MustAppend},  // BAR
               {1, SpacingOptions::MustAppend},  // 2
           },
           {
               {verilog_tokentype::PP_define, "`define"},
               {verilog_tokentype::PP_Identifier, "FOO"},
               {verilog_tokentype::PP_define_body, "1"},
               {verilog_tokentype::PP_define, "`define"},
               {verilog_tokentype::PP_Identifier, "BAR"},
               {verilog_tokentype::PP_define_body, "2"},
           }},

          // `define FOO()
          // `define BAR(x)
          // `define BAZ(y,z)
          {DefaultStyle,
           1,
           {
               {0, SpacingOptions::Undecided},   // `define
               {1, SpacingOptions::MustAppend},  // FOO
               {0, SpacingOptions::MustAppend},  // (
               {0, SpacingOptions::Undecided},   // )
               {0, SpacingOptions::MustAppend},  // "" (empty definition body)

               {0, SpacingOptions::MustWrap},    // `define
               {1, SpacingOptions::MustAppend},  // BAR
               {0, SpacingOptions::MustAppend},  // (
               {0, SpacingOptions::Undecided},   // x
               {0, SpacingOptions::Undecided},   // )
               {0, SpacingOptions::MustAppend},  // "" (empty definition body)

               {0, SpacingOptions::MustWrap},    // `define
               {1, SpacingOptions::MustAppend},  // BAZ
               {0, SpacingOptions::MustAppend},  // (
               {0, SpacingOptions::Undecided},   // y
               {0, SpacingOptions::Undecided},   // ,
               {1, SpacingOptions::Undecided},   // z
               {0, SpacingOptions::Undecided},   // )
               {0, SpacingOptions::MustAppend},  // "" (empty definition body)
           },
           {
               {verilog_tokentype::PP_define, "`define"},
               {verilog_tokentype::PP_Identifier, "FOO"},
               {'(', "("},
               {')', ")"},
               {verilog_tokentype::PP_define_body, ""},

               {verilog_tokentype::PP_define, "`define"},
               {verilog_tokentype::PP_Identifier, "BAR"},
               {'(', "("},
               {verilog_tokentype::SymbolIdentifier, "x"},
               {')', ")"},
               {verilog_tokentype::PP_define_body, ""},

               {verilog_tokentype::PP_define, "`define"},
               {verilog_tokentype::PP_Identifier, "BAZ"},
               {'(', "("},
               {verilog_tokentype::SymbolIdentifier, "y"},
               {',', ","},
               {verilog_tokentype::SymbolIdentifier, "z"},
               {')', ")"},
               {verilog_tokentype::PP_define_body, ""},
           }},

          // `define ADD(y,z) y+z
          {
              DefaultStyle,
              1,
              {
                  {0, SpacingOptions::Undecided},   // `define
                  {1, SpacingOptions::MustAppend},  // ADD
                  {0, SpacingOptions::MustAppend},  // (
                  {0, SpacingOptions::Undecided},   // y
                  {0, SpacingOptions::Undecided},   // ,
                  {1, SpacingOptions::Undecided},   // z
                  {0, SpacingOptions::Undecided},   // )
                  {1, SpacingOptions::MustAppend},  // "y+z"
              },
              {
                  {verilog_tokentype::PP_define, "`define"},
                  {verilog_tokentype::PP_Identifier, "ADD"},
                  {'(', "("},
                  {verilog_tokentype::SymbolIdentifier, "y"},
                  {',', ","},
                  {verilog_tokentype::SymbolIdentifier, "z"},
                  {')', ")"},
                  {verilog_tokentype::PP_define_body, "y+z"},
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
               {verilog_tokentype::TK_function, "function"},
               {verilog_tokentype::TK_new, "new"},
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
               {verilog_tokentype::TK_function, "function"},
               {verilog_tokentype::TK_new, "new"},
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
               {verilog_tokentype::TK_end, "end"},
               {verilog_tokentype::TK_end, "end"},
               {verilog_tokentype::TK_endfunction, "endfunction"},
               {verilog_tokentype::TK_endclass, "endclass"},
               {verilog_tokentype::TK_endpackage, "endpackage"},
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
               {verilog_tokentype::TK_end, "end"},
               {verilog_tokentype::TK_end, "end"},
               {verilog_tokentype::TK_endtask, "endtask"},
               {verilog_tokentype::TK_endmodule, "endmodule"},
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
               {verilog_tokentype::TK_if, "if"},
               {'(', "("},
               {verilog_tokentype::SymbolIdentifier, "r"},
               {verilog_tokentype::TK_EQ, "=="},
               {verilog_tokentype::SymbolIdentifier, "t"},
               {')', ")"},
               {verilog_tokentype::SymbolIdentifier, "a"},
               {'.', "."},
               {verilog_tokentype::SymbolIdentifier, "b"},
               {'(', "("},
               {verilog_tokentype::SymbolIdentifier, "c"},
               {')', ")"},
               {';', ";"},

               {verilog_tokentype::TK_else, "else"},
               {verilog_tokentype::SymbolIdentifier, "d"},
               {'.', "."},
               {verilog_tokentype::SymbolIdentifier, "e"},
               {'(', "("},
               {verilog_tokentype::SymbolIdentifier, "f"},
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
               {verilog_tokentype::TK_if, "if"},
               {'(', "("},
               {verilog_tokentype::SymbolIdentifier, "r"},
               {verilog_tokentype::TK_EQ, "=="},
               {verilog_tokentype::SymbolIdentifier, "t"},
               {')', ")"},

               {verilog_tokentype::TK_begin, "begin"},
               {verilog_tokentype::SymbolIdentifier, "a"},
               {'.', "."},
               {verilog_tokentype::SymbolIdentifier, "b"},
               {'(', "("},
               {verilog_tokentype::SymbolIdentifier, "c"},
               {')', ")"},
               {';', ";"},
               {verilog_tokentype::TK_end, "end"},

               {verilog_tokentype::TK_else, "else"},

               {verilog_tokentype::TK_begin, "begin"},
               {verilog_tokentype::SymbolIdentifier, "d"},
               {'.', "."},
               {verilog_tokentype::SymbolIdentifier, "e"},
               {'(', "("},
               {verilog_tokentype::SymbolIdentifier, "f"},
               {')', ")"},
               {';', ";"},
               {verilog_tokentype::TK_end, "end"},
           }},

          // various built-in function calls
          {DefaultStyle,
           1,
           {{0, SpacingOptions::Undecided}, {0, SpacingOptions::Undecided}},
           {{verilog_tokentype::TK_and, "and"}, {'(', "("}}},
          {DefaultStyle,
           1,
           {{0, SpacingOptions::Undecided}, {1, SpacingOptions::Undecided}},
           {{verilog_tokentype::TK_assert, "assert"}, {'(', "("}}},
          {DefaultStyle,
           1,
           {{0, SpacingOptions::Undecided}, {1, SpacingOptions::Undecided}},
           {{verilog_tokentype::TK_assume, "assume"}, {'(', "("}}},
          {DefaultStyle,
           1,
           {{0, SpacingOptions::Undecided}, {1, SpacingOptions::Undecided}},
           {{verilog_tokentype::TK_cover, "cover"}, {'(', "("}}},
          {DefaultStyle,
           1,
           {{0, SpacingOptions::Undecided}, {1, SpacingOptions::Undecided}},
           {{verilog_tokentype::TK_expect, "expect"}, {'(', "("}}},
          {DefaultStyle,
           1,
           {{0, SpacingOptions::Undecided}, {1, SpacingOptions::Undecided}},
           {{verilog_tokentype::TK_property, "property"}, {'(', "("}}},
          {DefaultStyle,
           1,
           {{0, SpacingOptions::Undecided}, {1, SpacingOptions::Undecided}},
           {{verilog_tokentype::TK_sequence, "sequence"}, {'(', "("}}},
          {DefaultStyle,
           1,
           {{0, SpacingOptions::Undecided}, {1, SpacingOptions::Undecided}},
           {{verilog_tokentype::TK_final, "final"}, {'(', "("}}},
          {DefaultStyle,
           1,
           {{0, SpacingOptions::Undecided}, {0, SpacingOptions::Undecided}},
           {{verilog_tokentype::TK_find, "find"}, {'(', "("}}},
          {DefaultStyle,
           1,
           {{0, SpacingOptions::Undecided}, {0, SpacingOptions::Undecided}},
           {{verilog_tokentype::TK_find_index, "find_index"}, {'(', "("}}},
          {DefaultStyle,
           1,
           {{0, SpacingOptions::Undecided}, {0, SpacingOptions::Undecided}},
           {{verilog_tokentype::TK_find_first, "find_first"}, {'(', "("}}},
          {DefaultStyle,
           1,
           {{0, SpacingOptions::Undecided}, {0, SpacingOptions::Undecided}},
           {{verilog_tokentype::TK_find_first_index, "find_first_index"},
            {'(', "("}}},
          {DefaultStyle,
           1,
           {{0, SpacingOptions::Undecided}, {0, SpacingOptions::Undecided}},
           {{verilog_tokentype::TK_find_last, "find_last"}, {'(', "("}}},
          {DefaultStyle,
           1,
           {{0, SpacingOptions::Undecided}, {0, SpacingOptions::Undecided}},
           {{verilog_tokentype::TK_find_last_index, "find_last_index"},
            {'(', "("}}},
          {DefaultStyle,
           1,
           {{0, SpacingOptions::Undecided}, {0, SpacingOptions::Undecided}},
           {{verilog_tokentype::TK_min, "min"}, {'(', "("}}},
          {DefaultStyle,
           1,
           {{0, SpacingOptions::Undecided}, {0, SpacingOptions::Undecided}},
           {{verilog_tokentype::TK_max, "max"}, {'(', "("}}},
          {DefaultStyle,
           1,
           {{0, SpacingOptions::Undecided}, {0, SpacingOptions::Undecided}},
           {{verilog_tokentype::TK_or, "or"}, {'(', "("}}},
          {DefaultStyle,
           1,
           {{0, SpacingOptions::Undecided}, {0, SpacingOptions::Undecided}},
           {{verilog_tokentype::TK_product, "product"}, {'(', "("}}},
          {DefaultStyle,
           1,
           {{0, SpacingOptions::Undecided}, {0, SpacingOptions::Undecided}},
           {{verilog_tokentype::TK_randomize, "randomize"}, {'(', "("}}},
          {DefaultStyle,
           1,
           {{0, SpacingOptions::Undecided}, {0, SpacingOptions::Undecided}},
           {{verilog_tokentype::TK_reverse, "reverse"}, {'(', "("}}},
          {DefaultStyle,
           1,
           {{0, SpacingOptions::Undecided}, {0, SpacingOptions::Undecided}},
           {{verilog_tokentype::TK_rsort, "rsort"}, {'(', "("}}},
          {DefaultStyle,
           1,
           {{0, SpacingOptions::Undecided}, {0, SpacingOptions::Undecided}},
           {{verilog_tokentype::TK_shuffle, "shuffle"}, {'(', "("}}},
          {DefaultStyle,
           1,
           {{0, SpacingOptions::Undecided}, {0, SpacingOptions::Undecided}},
           {{verilog_tokentype::TK_sort, "sort"}, {'(', "("}}},
          {DefaultStyle,
           1,
           {{0, SpacingOptions::Undecided}, {0, SpacingOptions::Undecided}},
           {{verilog_tokentype::TK_sum, "sum"}, {'(', "("}}},
          {DefaultStyle,
           1,
           {{0, SpacingOptions::Undecided}, {0, SpacingOptions::Undecided}},
           {{verilog_tokentype::TK_unique, "unique"}, {'(', "("}}},
          {DefaultStyle,
           1,
           {{0, SpacingOptions::Undecided}, {0, SpacingOptions::Undecided}},
           {{verilog_tokentype::TK_wait, "wait"}, {'(', "("}}},
          {DefaultStyle,
           1,
           {{0, SpacingOptions::Undecided}, {0, SpacingOptions::Undecided}},
           {{verilog_tokentype::TK_xor, "xor"}, {'(', "("}}},

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
               {verilog_tokentype::SymbolIdentifier, "baz"},
               {'.', "."},
               {verilog_tokentype::EscapedIdentifier, "\\FOO"},
               {'.', "."},
               {verilog_tokentype::SymbolIdentifier, "bar"},
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
               {verilog_tokentype::MacroCallId, "`BAR"},
               {'(', "("},
               {verilog_tokentype::EscapedIdentifier, "\\FOO"},
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
               {verilog_tokentype::TK_import, "import"},
               {verilog_tokentype::SymbolIdentifier, "foo_pkg"},
               {verilog_tokentype::TK_SCOPE_RES, "::"},
               {verilog_tokentype::SymbolIdentifier, "symbol"},
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
               {verilog_tokentype::TK_import, "import"},
               {verilog_tokentype::SymbolIdentifier, "foo_pkg"},
               {verilog_tokentype::TK_SCOPE_RES, "::"},
               {'*', "*"},
               {';', ";"},
           }},

          // #0; (delay, unitless integer)
          {DefaultStyle,
           1,
           {
               {0, SpacingOptions::Undecided},   // #
               {0, SpacingOptions::MustAppend},  // 0
               {0, SpacingOptions::Undecided},   // ;
           },
           {
               {'#', "#"},
               {verilog_tokentype::TK_DecNumber, "0"},
               {';', ";"},
           }},

          // #0.5; (delay, real-value)
          {DefaultStyle,
           1,
           {
               {0, SpacingOptions::Undecided},   // #
               {0, SpacingOptions::MustAppend},  // 0.5
               {0, SpacingOptions::Undecided},   // ;
           },
           {
               {'#', "#"},
               {verilog_tokentype::TK_RealTime, "0.5"},
               {';', ";"},
           }},

          // #0ns; (delay, time-literal)
          {DefaultStyle,
           1,
           {
               {0, SpacingOptions::Undecided},   // #
               {0, SpacingOptions::MustAppend},  // 0ns
               {0, SpacingOptions::MustAppend},  // ;
           },
           {
               {'#', "#"},
               {verilog_tokentype::TK_TimeLiteral, "0ns"},
               {';', ";"},
           }},

          // #1step; (delay, 1step)
          {DefaultStyle,
           1,
           {
               {0, SpacingOptions::Undecided},   // #
               {0, SpacingOptions::MustAppend},  // 1step
               {0, SpacingOptions::Undecided},   // ;
           },
           {
               {'#', "#"},
               {verilog_tokentype::TK_1step, "1step"},
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
               {verilog_tokentype::TK_default, "default"},
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
               {verilog_tokentype::SymbolIdentifier, "foo"},
               {'=', "="},
               {verilog_tokentype::TK_DecNumber, "1"},
               {verilog_tokentype::TK_LS, "<<"},
               {verilog_tokentype::SymbolIdentifier, "bar"},
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
               {verilog_tokentype::SymbolIdentifier, "foo"},
               {'=', "="},
               {verilog_tokentype::SymbolIdentifier, "bar"},
               {verilog_tokentype::TK_LS, "<<"},
               {verilog_tokentype::TK_DecNumber, "1"},
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
               {verilog_tokentype::SymbolIdentifier, "foo"},
               {'=', "="},
               {verilog_tokentype::MacroIdentifier, "`BAR"},
               {verilog_tokentype::TK_LS, "<<"},
               {verilog_tokentype::TK_DecNumber, "1"},
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
               {verilog_tokentype::SymbolIdentifier, "foo"},
               {'=', "="},
               {verilog_tokentype::TK_DecNumber, "1"},
               {verilog_tokentype::TK_LS, "<<"},
               {verilog_tokentype::MacroIdentifier, "`BAR"},
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
               {verilog_tokentype::SymbolIdentifier, "foo"},
               {'=', "="},
               {verilog_tokentype::TK_DecNumber, "1"},
               {verilog_tokentype::TK_RS, ">>"},
               {verilog_tokentype::SymbolIdentifier, "bar"},
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
               {verilog_tokentype::SymbolIdentifier, "foo"},
               {'=', "="},
               {verilog_tokentype::SymbolIdentifier, "bar"},
               {verilog_tokentype::TK_RS, ">>"},
               {verilog_tokentype::TK_DecNumber, "1"},
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
               {verilog_tokentype::SymbolIdentifier, "foo"},
               {'=', "="},
               {verilog_tokentype::MacroIdentifier, "`BAR"},
               {verilog_tokentype::TK_RS, ">>"},
               {verilog_tokentype::TK_DecNumber, "1"},
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
               {verilog_tokentype::SymbolIdentifier, "foo"},
               {'=', "="},
               {verilog_tokentype::TK_DecNumber, "1"},
               {verilog_tokentype::TK_RS, ">>"},
               {verilog_tokentype::MacroIdentifier, "`BAR"},
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
          GetFormatTokenType(verilog_tokentype(ftoken.TokenEnum()));
    }

    auto& ftokens_range = handler.pre_format_tokens_;
    // nullptr buffer_start is needed because token text do not belong to the
    // same contiguous string buffer.
    // Pass an empty/fake tree, which will not be used for testing
    // context-insensitive annotation rules.
    // Since we're using the joined string buffer inside handler,
    // we need to pass an EOF token that points to the end of that buffer.
    AnnotateFormattingInformation(test_case.style, nullptr, nullptr,
                                  handler.EOFToken(), &ftokens_range);
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
          {verilog_tokentype::TK_StringLiteral, "\"hello\""},
          {},  // any context
          {},  // any context
          {1, SpacingOptions::Undecided},
      },
      {
          DefaultStyle,
          {'=', "="},
          {verilog_tokentype::TK_EvalStringLiteral, "`\"hello`\""},
          {},  // any context
          {},  // any context
          {1, SpacingOptions::Undecided},
      },
      // Test cases covering right token as a preprocessor directive:
      {
          DefaultStyle,
          {verilog_tokentype::TK_EOL_COMMENT, "//comment1"},
          {verilog_tokentype::PP_ifdef, "`ifdef"},
          {},  // any context
          {},  // any context
          {1, SpacingOptions::MustWrap},
      },
      {
          DefaultStyle,
          {verilog_tokentype::SymbolIdentifier, "id"},
          {verilog_tokentype::PP_ifdef, "`ifdef"},
          {},  // any context
          {},  // any context
          {1, SpacingOptions::MustWrap},
      },
      {
          DefaultStyle,
          {';', ";"},
          {verilog_tokentype::PP_ifdef, "`ifdef"},
          {},  // any context
          {},  // any context
          {1, SpacingOptions::MustWrap},
      },
      {
          DefaultStyle,
          {verilog_tokentype::PP_else, "`else"},
          {verilog_tokentype::PP_ifdef, "`ifdef"},
          {},  // any context
          {},  // any context
          {1, SpacingOptions::MustWrap},
      },
      {
          DefaultStyle,
          {verilog_tokentype::PP_endif, "`endif"},
          {verilog_tokentype::PP_ifdef, "`ifdef"},
          {},  // any context
          {},  // any context
          {1, SpacingOptions::MustWrap},
      },
      {
          DefaultStyle,
          {verilog_tokentype::TK_EOL_COMMENT, "//comment1"},
          {verilog_tokentype::PP_ifndef, "`ifndef"},
          {},  // any context
          {},  // any context
          {1, SpacingOptions::MustWrap},
      },
      {
          DefaultStyle,
          {verilog_tokentype::SymbolIdentifier, "id"},
          {verilog_tokentype::PP_ifndef, "`ifndef"},
          {},  // any context
          {},  // any context
          {1, SpacingOptions::MustWrap},
      },
      {
          DefaultStyle,
          {';', ";"},
          {verilog_tokentype::PP_ifndef, "`ifndef"},
          {},  // any context
          {},  // any context
          {1, SpacingOptions::MustWrap},
      },
      {
          DefaultStyle,
          {SymbolIdentifier, "ID"},
          {verilog_tokentype::PP_else, "`else"},
          {},  // any context
          {},  // any context
          {1, SpacingOptions::MustWrap},
      },
      {
          DefaultStyle,
          {';', ";"},
          {verilog_tokentype::PP_else, "`else"},
          {},  // any context
          {},  // any context
          {1, SpacingOptions::MustWrap},
      },
      {
          DefaultStyle,
          {verilog_tokentype::PP_endif, "`endif"},
          {verilog_tokentype::PP_else, "`else"},
          {},  // any context
          {},  // any context
          {1, SpacingOptions::MustWrap},
      },
      {
          DefaultStyle,
          {verilog_tokentype::PP_include, "`include"},
          {TK_StringLiteral, "\"lost/file.svh\""},
          {},                             // any context
          {},                             // any context
          {1, SpacingOptions::Undecided}, /* or MustAppend? */
      },
      {
          DefaultStyle,
          {verilog_tokentype::PP_include, "`include"},
          {TK_EvalStringLiteral, "`\"lost/file.svh`\""},
          {},                             // any context
          {},                             // any context
          {1, SpacingOptions::Undecided}, /* or MustAppend? */
      },
      {
          DefaultStyle,
          {TK_StringLiteral, "\"lost/file.svh\""},
          {verilog_tokentype::PP_include, "`include"},
          {},  // any context
          {},  // any context
          {1, SpacingOptions::MustWrap},
      },
      {
          DefaultStyle,
          {verilog_tokentype::PP_else, "`else"},
          {verilog_tokentype::PP_include, "`include"},
          {},  // any context
          {},  // any context
          {1, SpacingOptions::MustWrap},
      },
      {
          DefaultStyle,
          {';', ";"},
          {verilog_tokentype::PP_include, "`include"},
          {},  // any context
          {},  // any context
          {1, SpacingOptions::MustWrap},
      },
      {
          DefaultStyle,
          {SymbolIdentifier, "ID"},
          {verilog_tokentype::PP_include, "`include"},
          {},  // any context
          {},  // any context
          {1, SpacingOptions::MustWrap},
      },
      {
          DefaultStyle,
          {TK_StringLiteral, "\"lost/file.svh\""},
          {verilog_tokentype::PP_define, "`define"},
          {},  // any context
          {},  // any context
          {1, SpacingOptions::MustWrap},
      },
      {
          DefaultStyle,
          {verilog_tokentype::PP_else, "`else"},
          {verilog_tokentype::PP_define, "`define"},
          {},  // any context
          {},  // any context
          {1, SpacingOptions::MustWrap},
      },
      {
          DefaultStyle,
          {';', ";"},
          {verilog_tokentype::PP_define, "`define"},
          {},  // any context
          {},  // any context
          {1, SpacingOptions::MustWrap},
      },
      {
          DefaultStyle,
          {SymbolIdentifier, "ID"},
          {verilog_tokentype::PP_define, "`define"},
          {},  // any context
          {},  // any context
          {1, SpacingOptions::MustWrap},
      },
      {
          DefaultStyle,
          {verilog_tokentype::PP_define, "`define"},
          {SymbolIdentifier, "ID"},
          {},  // any context
          {},  // any context
          {1, SpacingOptions::MustAppend},
      },
      {
          DefaultStyle,
          {TK_StringLiteral, "\"lost/file.svh\""},
          {verilog_tokentype::PP_undef, "`undef"},
          {},  // any context
          {},  // any context
          {1, SpacingOptions::MustWrap},
      },
      {
          DefaultStyle,
          {verilog_tokentype::PP_else, "`else"},
          {verilog_tokentype::PP_undef, "`undef"},
          {},  // any context
          {},  // any context
          {1, SpacingOptions::MustWrap},
      },
      {
          DefaultStyle,
          {';', ";"},
          {verilog_tokentype::PP_undef, "`undef"},
          {},  // any context
          {},  // any context
          {1, SpacingOptions::MustWrap},
      },
      {
          DefaultStyle,
          {SymbolIdentifier, "ID"},
          {verilog_tokentype::PP_undef, "`undef"},
          {},  // any context
          {},  // any context
          {1, SpacingOptions::MustWrap},
      },
      {
          DefaultStyle,
          {verilog_tokentype::TK_endfunction, "endfunction"},
          {verilog_tokentype::PP_undef, "`undef"},
          {},  // any context
          {},  // any context
          {1, SpacingOptions::MustWrap},
      },
      {
          DefaultStyle,
          {verilog_tokentype::TK_end, "end"},
          {verilog_tokentype::PP_undef, "`undef"},
          {},  // any context
          {},  // any context
          {1, SpacingOptions::MustWrap},
      },
      {
          DefaultStyle,
          {verilog_tokentype::MacroCallCloseToEndLine, ")"},
          {verilog_tokentype::PP_undef, "`undef"},
          {},  // any context
          {},  // any context
          {1, SpacingOptions::MustWrap},
      },

      // macro definitions
      {
          DefaultStyle,
          {verilog_tokentype::PP_Identifier, "FOO"},
          {verilog_tokentype::PP_define_body, ""}, /* empty */
          {},                                      // any context
          {},                                      // any context
          {0, SpacingOptions::MustAppend},
      },
      {
          DefaultStyle,
          {verilog_tokentype::PP_Identifier, "FOO"},
          {verilog_tokentype::PP_define_body, "bar"},
          {},  // any context
          {},  // any context
          {1, SpacingOptions::MustAppend},
      },
      {
          DefaultStyle,
          {verilog_tokentype::PP_Identifier, "BAR"},
          {verilog_tokentype::PP_define_body, "13"},
          {},  // any context
          {},  // any context
          {1, SpacingOptions::MustAppend},
      },
      {
          DefaultStyle,
          {verilog_tokentype::PP_Identifier, "BAR"},
          {verilog_tokentype::PP_define_body, "\\\n  bar"},
          {},  // any context
          {},  // any context
          {1, SpacingOptions::MustAppend},
      },
      {
          DefaultStyle,
          {')', ")"},
          {verilog_tokentype::PP_define_body, ""}, /* empty */
          {},                                      // any context
          {},                                      // any context
          {0, SpacingOptions::MustAppend},
      },
      {
          DefaultStyle,
          {')', ")"},
          {verilog_tokentype::PP_define_body, "bar"},
          {},  // any context
          {},  // any context
          {1, SpacingOptions::MustAppend},
      },
      {
          DefaultStyle,
          {')', ")"},
          {verilog_tokentype::PP_define_body, "13"},
          {},  // any context
          {},  // any context
          {1, SpacingOptions::MustAppend},
      },
      {
          DefaultStyle,
          {')', ")"},
          {verilog_tokentype::PP_define_body, "\\\n  bar"},
          {},  // any context
          {},  // any context
          {1, SpacingOptions::MustAppend},
      },
      {
          // e.g. if (x) { ... } (in constraints)
          DefaultStyle,
          {')', ")"},
          {'{', "{"},
          {},  // any context
          {},  // any context
          {1, SpacingOptions::Undecided},
      },

      // right token = MacroCallId or MacroIdentifier
      {
          DefaultStyle,
          {SymbolIdentifier, "ID"},
          {verilog_tokentype::MacroCallId, "`uvm_foo_macro"},
          {},  // any context
          {},  // any context
          {1, SpacingOptions::Undecided},
      },
      {
          DefaultStyle,
          {SymbolIdentifier, "ID"},
          {verilog_tokentype::MacroIdentifier, "`uvm_foo_id"},
          {},  // any context
          {},  // any context
          {1, SpacingOptions::Undecided},
      },
      {
          DefaultStyle,
          {';', ";"},
          {verilog_tokentype::MacroCallId, "`uvm_foo_macro"},
          {},  // any context
          {},  // any context
          {1, SpacingOptions::Undecided},
      },
      {
          DefaultStyle,
          {';', ";"},
          {verilog_tokentype::MacroIdentifier, "`uvm_foo_id"},
          {},  // any context
          {},  // any context
          {1, SpacingOptions::Undecided},
      },
      {
          DefaultStyle,
          {TK_EOL_COMMENT, "//comment"},
          {verilog_tokentype::MacroCallId, "`uvm_foo_macro"},
          {},  // any context
          {},  // any context
          {1, SpacingOptions::MustWrap},
      },
      {
          DefaultStyle,
          {TK_EOL_COMMENT, "//comment"},
          {verilog_tokentype::MacroIdentifier, "`uvm_foo_id"},
          {},  // any context
          {},  // any context
          {1, SpacingOptions::MustWrap},
      },
      {
          DefaultStyle,
          {TK_COMMENT_BLOCK, "/*comment*/"},
          {verilog_tokentype::MacroCallId, "`uvm_foo_macro"},
          {},  // any context
          {},  // any context
          {1, SpacingOptions::Undecided},
      },
      {
          DefaultStyle,
          {TK_COMMENT_BLOCK, "/*comment*/"},
          {verilog_tokentype::MacroIdentifier, "`uvm_foo_id"},
          {},  // any context
          {},  // any context
          {1, SpacingOptions::Undecided},
      },
      {
          DefaultStyle,
          {PP_else, "`else"},
          {verilog_tokentype::MacroCallId, "`uvm_foo_macro"},
          {},  // any context
          {},  // any context
          {1, SpacingOptions::MustWrap},
      },
      {
          DefaultStyle,
          {PP_else, "`else"},
          {verilog_tokentype::MacroIdentifier, "`uvm_foo_id"},
          {},  // any context
          {},  // any context
          {1, SpacingOptions::MustWrap},
      },
      {
          DefaultStyle,
          {PP_endif, "`endif"},
          {verilog_tokentype::MacroCallId, "`uvm_foo_macro"},
          {},  // any context
          {},  // any context
          {1, SpacingOptions::MustWrap},
      },
      {
          DefaultStyle,
          {PP_endif, "`endif"},
          {verilog_tokentype::MacroIdentifier, "`uvm_foo_id"},
          {},  // any context
          {},  // any context
          {1, SpacingOptions::MustWrap},
      },
      {
          DefaultStyle,
          {verilog_tokentype::MacroCallId, "`uvm_foo_macro"},
          {verilog_tokentype::MacroCallId, "`uvm_foo_macro"},
          {},  // any context
          {},  // any context
          {1, SpacingOptions::Undecided},
      },
      {
          DefaultStyle,
          {verilog_tokentype::MacroCallId, "`uvm_foo_macro"},
          {verilog_tokentype::MacroIdentifier, "`uvm_foo_id"},
          {},  // any context
          {},  // any context
          {1, SpacingOptions::Undecided},
      },
      {
          DefaultStyle,
          {verilog_tokentype::MacroIdentifier, "`uvm_foo_id"},
          {verilog_tokentype::MacroCallId, "`uvm_foo_macro"},
          {},  // any context
          {},  // any context
          {1, SpacingOptions::Undecided},
      },
      {
          DefaultStyle,
          {verilog_tokentype::MacroIdentifier, "`uvm_foo_id"},
          {verilog_tokentype::MacroIdentifier, "`uvm_foo_id"},
          {},  // any context
          {},  // any context
          {1, SpacingOptions::Undecided},
      },
      {
          DefaultStyle,
          {')', ")"},
          {verilog_tokentype::MacroCallId, "`uvm_foo_macro"},
          {},  // any context
          {},  // any context
          {1, SpacingOptions::Undecided},
      },
      {
          DefaultStyle,
          {')', ")"},
          {verilog_tokentype::MacroIdentifier, "`uvm_foo_id"},
          {},  // any context
          {},  // any context
          {1, SpacingOptions::Undecided},
      },
      {
          DefaultStyle,
          {verilog_tokentype::MacroCallCloseToEndLine, ")"},
          {verilog_tokentype::MacroCallId, "`uvm_foo_macro"},
          {},  // any context
          {},  // any context
          {1, SpacingOptions::MustWrap},
      },
      {
          DefaultStyle,
          {verilog_tokentype::MacroCallCloseToEndLine, ")"},
          {verilog_tokentype::MacroIdentifier, "`uvm_foo_id"},
          {},  // any context
          {},  // any context
          {1, SpacingOptions::MustWrap},
      },
      {
          DefaultStyle,
          {verilog_tokentype::MacroCallCloseToEndLine, ")"},
          {';', ";"},
          {},  // any context
          {},  // any context
          {0, SpacingOptions::Undecided},
      },

      // Without context, default is to treat '-' as binary.
      {
          DefaultStyle,
          {'-', "-"},                               // left token
          {verilog_tokentype::TK_DecNumber, "42"},  // right token
          {},                                       // context
          {},                                       // context
          {1, SpacingOptions::Undecided},
      },
      {
          DefaultStyle,
          {'-', "-"},
          {verilog_tokentype::TK_DecNumber, "42"},
          {},  // context
          {NodeEnum::kBinaryExpression},
          {1, SpacingOptions::Undecided},
      },

      // Handle '-' as a unary prefix expression.
      {
          DefaultStyle,
          {'-', "-"},                               // left token
          {verilog_tokentype::TK_DecNumber, "42"},  // right token
          {},                                       // context
          {NodeEnum::kUnaryPrefixExpression},       // context
          {0, SpacingOptions::MustAppend},
      },
      {
          DefaultStyle,
          {'-', "-"},
          {verilog_tokentype::SymbolIdentifier, "xyz"},
          {},  // context
          {NodeEnum::kUnaryPrefixExpression},
          {0, SpacingOptions::MustAppend},
      },
      {
          DefaultStyle,
          {'-', "-"},
          {'(', "("},
          {},  // context
          {NodeEnum::kUnaryPrefixExpression},
          {0, SpacingOptions::MustAppend},
      },
      {
          DefaultStyle,
          {'-', "-"},
          {verilog_tokentype::MacroIdItem, "`FOO"},
          {},  // context
          {NodeEnum::kUnaryPrefixExpression},
          {0, SpacingOptions::MustAppend},
      },

      // Handle '&' as binary
      {
          DefaultStyle,
          {'&', "&"},
          {'~', "~"},
          {},  // unspecified context
          {},  // unspecified context
          {1, SpacingOptions::Undecided},
      },

      // Handle '&' as unary
      {
          DefaultStyle,
          {'&', "&"},
          {verilog_tokentype::TK_DecNumber, "42"},
          {/* any context */},
          {NodeEnum::kUnaryPrefixExpression},
          {0, SpacingOptions::MustAppend},
      },
      {
          DefaultStyle,
          {'&', "&"},
          {verilog_tokentype::SymbolIdentifier, "foo"},
          {/* any context */},
          {NodeEnum::kUnaryPrefixExpression},
          {0, SpacingOptions::MustAppend},
      },
      {
          DefaultStyle,
          {'&', "&"},
          {'(', "("},
          {/* any context */},
          {NodeEnum::kUnaryPrefixExpression},
          {0, SpacingOptions::MustAppend},
      },
      {
          DefaultStyle,
          {'&', "&"},
          {'{', "{"},
          {/* any context */},
          {NodeEnum::kUnaryPrefixExpression},
          {0, SpacingOptions::MustAppend},
      },

      // Handle '|' as binary
      {
          DefaultStyle,
          {'|', "|"},
          {'~', "~"},
          {/* any context */},
          {/* any context */},
          {1, SpacingOptions::Undecided},
      },

      // Handle '|' as unary
      {
          DefaultStyle,
          {'|', "|"},
          {verilog_tokentype::TK_DecNumber, "42"},
          {/* any context */},
          {NodeEnum::kUnaryPrefixExpression},
          {0, SpacingOptions::MustAppend},
      },
      {
          DefaultStyle,
          {'|', "|"},
          {verilog_tokentype::SymbolIdentifier, "foo"},
          {/* any context */},
          {NodeEnum::kUnaryPrefixExpression},
          {0, SpacingOptions::MustAppend},
      },
      {
          DefaultStyle,
          {'|', "|"},
          {'(', "("},
          {/* any context */},
          {NodeEnum::kUnaryPrefixExpression},
          {0, SpacingOptions::MustAppend},
      },
      {
          DefaultStyle,
          {'|', "|"},
          {'{', "{"},
          {/* any context */},
          {NodeEnum::kUnaryPrefixExpression},
          {0, SpacingOptions::MustAppend},
      },

      // Handle '^' as binary
      {
          DefaultStyle,
          {'^', "^"},
          {'~', "~"},
          {/* any context */},
          {/* any context */},
          {1, SpacingOptions::Undecided},
      },

      // Handle '^' as unary
      {
          DefaultStyle,
          {'^', "^"},
          {verilog_tokentype::TK_DecNumber, "42"},
          {/* any context */},
          {NodeEnum::kUnaryPrefixExpression},
          {0, SpacingOptions::MustAppend},
      },
      {
          DefaultStyle,
          {'^', "^"},
          {verilog_tokentype::SymbolIdentifier, "foo"},
          {/* any context */},
          {NodeEnum::kUnaryPrefixExpression},
          {0, SpacingOptions::MustAppend},
      },
      {
          DefaultStyle,
          {'^', "^"},
          {'(', "("},
          {/* any context */},
          {NodeEnum::kUnaryPrefixExpression},
          {0, SpacingOptions::MustAppend},
      },
      {
          DefaultStyle,
          {'^', "^"},
          {'{', "{"},
          {/* any context */},
          {NodeEnum::kUnaryPrefixExpression},
          {0, SpacingOptions::MustAppend},
      },

      // Test '~' unary token
      {
          DefaultStyle,
          {'~', "~"},
          {'(', "("},
          {/* any context */},
          {NodeEnum::kUnaryPrefixExpression},
          {0, SpacingOptions::MustAppend},
      },
      {
          DefaultStyle,
          {'~', "~"},
          {verilog_tokentype::SymbolIdentifier, "foo"},
          {/* any context */},
          {NodeEnum::kUnaryPrefixExpression},
          {0, SpacingOptions::MustAppend},
      },

      // Test '##' unary (delay) operator
      {
          DefaultStyle,
          {verilog_tokentype::TK_POUNDPOUND, "##"},
          {'(', "("},
          {/* any context */},
          {/* any context */},
          {0, SpacingOptions::MustAppend},
      },
      {
          DefaultStyle,
          {verilog_tokentype::TK_POUNDPOUND, "##"},
          {verilog_tokentype::TK_DecNumber, "10"},
          {/* any context */},
          {/* any context */},
          {0, SpacingOptions::MustAppend},
      },
      {
          DefaultStyle,
          {verilog_tokentype::TK_POUNDPOUND, "##"},
          {verilog_tokentype::SymbolIdentifier, "x_delay"},
          {/* any context */},
          {/* any context */},
          {0, SpacingOptions::MustAppend},
      },
      {
          DefaultStyle,
          {verilog_tokentype::TK_POUNDPOUND, "##"},
          {verilog_tokentype::MacroIdentifier, "`X_DELAY"},
          {/* any context */},
          {/* any context */},
          {0, SpacingOptions::MustAppend},
      },
      {
          DefaultStyle,
          {verilog_tokentype::TK_POUNDPOUND, "##"},
          {verilog_tokentype::TK_LP, "'{"},
          {/* any context */},
          {/* any context */},
          {0, SpacingOptions::MustAppend},
      },
      {
          DefaultStyle,
          {verilog_tokentype::TK_POUNDPOUND, "##"},
          {'[', "["},
          {/* any context */},
          {/* any context */},
          {0, SpacingOptions::MustAppend},
      },
      {
          DefaultStyle,
          {verilog_tokentype::TK_POUNDPOUND, "##"},
          {verilog_tokentype::TK_LBSTARRB, "[*]"},
          {/* any context */},
          {/* any context */},
          {0, SpacingOptions::MustAppend},
      },
      {
          DefaultStyle,
          {verilog_tokentype::TK_POUNDPOUND, "##"},
          {verilog_tokentype::TK_LBPLUSRB, "[+]"},
          {/* any context */},
          {/* any context */},
          {0, SpacingOptions::MustAppend},
      },
      {
          DefaultStyle,
          {verilog_tokentype::SymbolIdentifier, "predicate"},
          {verilog_tokentype::TK_POUNDPOUND, "##"},
          {/* any context */},
          {/* any context */},
          {1, SpacingOptions::Undecided},
      },
      {
          DefaultStyle,
          {')', ")"},
          {verilog_tokentype::TK_POUNDPOUND, "##"},
          {/* any context */},
          {/* any context */},
          {1, SpacingOptions::Undecided},
      },
      {
          DefaultStyle,
          {'(', "("},
          {verilog_tokentype::TK_POUNDPOUND, "##"},
          {/* any context */},
          {/* any context */},
          {0, SpacingOptions::Undecided},
      },
      {
          DefaultStyle,
          {verilog_tokentype::TK_and, "and"},
          {verilog_tokentype::TK_POUNDPOUND, "##"},
          {/* any context */},
          {/* any context */},
          {1, SpacingOptions::Undecided},
      },
      {
          DefaultStyle,
          {verilog_tokentype::TK_or, "or"},
          {verilog_tokentype::TK_POUNDPOUND, "##"},
          {/* any context */},
          {/* any context */},
          {1, SpacingOptions::Undecided},
      },
      {
          DefaultStyle,
          {verilog_tokentype::TK_intersect, "intersect"},
          {verilog_tokentype::TK_POUNDPOUND, "##"},
          {/* any context */},
          {/* any context */},
          {1, SpacingOptions::Undecided},
      },
      {
          DefaultStyle,
          {verilog_tokentype::TK_throughout, "throughout"},
          {verilog_tokentype::TK_POUNDPOUND, "##"},
          {/* any context */},
          {/* any context */},
          {1, SpacingOptions::Undecided},
      },
      {
          DefaultStyle,
          {verilog_tokentype::TK_within, "within"},
          {verilog_tokentype::TK_POUNDPOUND, "##"},
          {/* any context */},
          {/* any context */},
          {1, SpacingOptions::Undecided},
      },

      // Two unary operators
      {
          DefaultStyle,
          {'~', "~"},
          {'~', "~"},
          {/* any context */},
          {NodeEnum::kUnaryPrefixExpression},
          {0, SpacingOptions::MustAppend},
      },

      // Handle '->' as a unary prefix expression.
      {
          DefaultStyle,
          {TK_TRIGGER, "->"},
          {verilog_tokentype::SymbolIdentifier, "a"},
          {/* any context */},             // context
          {/* any context */},             // context
          {0, SpacingOptions::Undecided},  // could be MustAppend though
      },
      {
          DefaultStyle,
          {TK_NONBLOCKING_TRIGGER, "->>"},
          {verilog_tokentype::SymbolIdentifier, "a"},
          {/* any context */},             // context
          {/* any context */},             // context
          {0, SpacingOptions::Undecided},  // could be MustAppend though
      },

      // Handle '->' as a binary operator
      {
          DefaultStyle,
          {TK_LOGICAL_IMPLIES, "->"},
          {verilog_tokentype::SymbolIdentifier, "right"},
          {/* any context */},  // context
          {/* any context */},  // context
          {1, SpacingOptions::Undecided},
      },
      {
          DefaultStyle,
          {verilog_tokentype::SymbolIdentifier, "left"},
          {TK_LOGICAL_IMPLIES, "->"},
          {/* any context */},  // context
          {/* any context */},  // context
          {1, SpacingOptions::Undecided},
      },
      {
          DefaultStyle,
          {TK_CONSTRAINT_IMPLIES, "->"},
          {verilog_tokentype::SymbolIdentifier, "right"},
          {/* any context */},  // context
          {/* any context */},  // context
          {1, SpacingOptions::Undecided},
      },
      {
          DefaultStyle,
          {verilog_tokentype::SymbolIdentifier, "left"},
          {TK_CONSTRAINT_IMPLIES, "->"},
          {/* any context */},  // context
          {/* any context */},  // context
          {1, SpacingOptions::Undecided},
      },

      // Inside dimension ranges, force space preservation if not around ':'
      {
          DefaultStyle,
          {'*', "*"},
          {verilog_tokentype::SymbolIdentifier, "foo"},
          {/* any context */},
          {/* any context */},
          {1, SpacingOptions::Undecided},
      },
      {
          DefaultStyle,
          {verilog_tokentype::SymbolIdentifier, "foo"},
          {'*', "*"},
          {/* any context */},
          {/* any context */},
          {1, SpacingOptions::Undecided},
      },
      {
          DefaultStyle,
          {'*', "*"},
          {verilog_tokentype::SymbolIdentifier, "foo"},
          {/* any context */},
          {NodeEnum::kDimensionRange},
          {1, SpacingOptions::Preserve},
      },
      {
          DefaultStyle,
          {verilog_tokentype::SymbolIdentifier, "foo"},
          {'*', "*"},
          {/* any context */},
          {NodeEnum::kDimensionRange},
          {1, SpacingOptions::Preserve},
      },
      {
          DefaultStyle,
          {':', ":"},
          {verilog_tokentype::SymbolIdentifier, "foo"},
          {/* any context */},
          {NodeEnum::kDimensionRange},
          {0, SpacingOptions::Undecided},
      },
      {
          DefaultStyle,
          {verilog_tokentype::SymbolIdentifier, "foo"},
          {':', ":"},
          {/* any context */},
          {NodeEnum::kDimensionRange},
          {0, SpacingOptions::Undecided},
      },

      // spacing between ranges of multi-dimension arrays
      {
          DefaultStyle,
          {']', "]"},
          {'[', "["},
          {},  // any context
          {},  // any context
          {0, SpacingOptions::Undecided},
      },

      // spacing before first '[' of packed arrays in declarations
      {
          DefaultStyle,
          {verilog_tokentype::TK_logic, "logic"},
          {'[', "["},
          {/* any context */},
          {/* any context */},
          {0, SpacingOptions::Undecided},
      },
      {
          DefaultStyle,
          {verilog_tokentype::SymbolIdentifier, "mytype1"},
          {'[', "["},
          {/* any context */},
          {},  // unspecified context, this covers index expressions
          {0, SpacingOptions::Undecided},
      },
      {
          DefaultStyle,
          {verilog_tokentype::TK_logic, "logic"},
          {'[', "["},
          {/* any context */},
          {NodeEnum::kPackedDimensions},
          {1, SpacingOptions::Undecided},
      },
      {
          DefaultStyle,
          {verilog_tokentype::SymbolIdentifier, "mytype2"},
          {'[', "["},
          {/* any context */},
          {NodeEnum::kPackedDimensions},
          {1, SpacingOptions::Undecided},
      },
      {
          DefaultStyle,
          {verilog_tokentype::SymbolIdentifier, "id1"},
          {'[', "["},
          {/* any context */},
          {NodeEnum::kPackedDimensions, NodeEnum::kExpression},
          {0, SpacingOptions::Undecided},
      },

      // spacing after last ']' of packed arrays in declarations
      {
          DefaultStyle,
          {']', "]"},
          {verilog_tokentype::SymbolIdentifier, "id_a"},
          {/* any context */},
          {/* any context */},
          {1, SpacingOptions::Undecided},
      },
      {
          DefaultStyle,
          {']', "]"},
          {verilog_tokentype::SymbolIdentifier, "id_b"},
          {/* any context */},
          {NodeEnum::kUnqualifiedId},
          {1, SpacingOptions::Undecided},
      },
      {
          DefaultStyle,
          {']', "]"},
          {verilog_tokentype::SymbolIdentifier, "id_c"},
          {/* any context */},
          {NodeEnum::kDataTypeImplicitBasicIdDimensions,
           NodeEnum::kUnqualifiedId},
          {1, SpacingOptions::Undecided},
      },

      // "foo ()" in "module foo();"
      {
          DefaultStyle,
          {verilog_tokentype::SymbolIdentifier, "foo"},
          {'(', "("},
          {/* any context */},
          {/* unspecified context */},
          {0, SpacingOptions::Undecided},
      },
      {
          DefaultStyle,
          {verilog_tokentype::SymbolIdentifier, "foo"},
          {'(', "("},
          {/* any context */},
          {NodeEnum::kModuleHeader},
          {1, SpacingOptions::Undecided},
      },

      // "a(" in "foo bar (.a(b));": instantiation with named ports
      {
          DefaultStyle,
          {verilog_tokentype::SymbolIdentifier, "foo"},
          {'(', "("},
          {/* any context */},
          {NodeEnum::kGateInstance},
          {1, SpacingOptions::Undecided},
      },
      {
          DefaultStyle,
          {verilog_tokentype::SymbolIdentifier, "foo"},
          {'(', "("},
          {/* any context */},
          {NodeEnum::kPrimitiveGateInstance},
          {1, SpacingOptions::Undecided},
      },
      {
          DefaultStyle,
          {verilog_tokentype::SymbolIdentifier, "foo"},
          {'(', "("},
          {/* any context */},
          {NodeEnum::kActualNamedPort},
          {0, SpacingOptions::Undecided},
      },
      {
          DefaultStyle,
          {verilog_tokentype::SymbolIdentifier, "foo"},
          {'(', "("},
          {/* any context */},
          {NodeEnum::kGateInstance, NodeEnum::kActualNamedPort},
          {0, SpacingOptions::Undecided},
      },
      {
          DefaultStyle,
          {verilog_tokentype::SymbolIdentifier, "foo"},
          {'(', "("},
          {/* any context */},
          {NodeEnum::kModuleHeader, NodeEnum::kPort},
          {0, SpacingOptions::Undecided},
      },

      // cases for the heavily overloaded ':'

      // ':' on the right, anything else on the left
      {
          DefaultStyle,
          {verilog_tokentype::SymbolIdentifier, "x"},
          {':', ":"},
          {/* any context */},
          {/* unspecified context */},
          kUnhandledSpacing,
      },
      {
          // a ? b : c (ternary expression)
          DefaultStyle,
          {verilog_tokentype::SymbolIdentifier, "b"},
          {':', ":"},
          {/* any context */},
          {NodeEnum::kTernaryExpression},
          {1, SpacingOptions::Undecided},
      },
      {
          // a ? 111 : c (ternary expression)
          DefaultStyle,
          {verilog_tokentype::TK_DecNumber, "111"},
          {':', ":"},
          {/* any context */},
          {NodeEnum::kTernaryExpression},
          {1, SpacingOptions::Undecided},
      },
      {
          // a ? "1" : c (ternary expression)
          DefaultStyle,
          {verilog_tokentype::TK_StringLiteral, "\"1\""},
          {':', ":"},
          {/* any context */},
          {NodeEnum::kTernaryExpression},
          {1, SpacingOptions::Undecided},
      },
      {
          // a ? (1) : c (ternary expression)
          DefaultStyle,
          {')', ":"},
          {':', ":"},
          {/* any context */},
          {NodeEnum::kTernaryExpression},
          {1, SpacingOptions::Undecided},
      },
      {
          // a ? {b} : {c} (ternary expression)
          DefaultStyle,
          {'}', "}"},
          {':', ":"},
          {/* any context */},
          {NodeEnum::kTernaryExpression},
          {1, SpacingOptions::Undecided},
      },
      {
          // a ? {b} : {c} (ternary expression)
          DefaultStyle,
          {':', ":"},
          {'{', "{"},
          {/* any context */},
          {NodeEnum::kTernaryExpression},
          {1, SpacingOptions::Undecided},
      },

      // ':' on the left, anything else on the right
      {
          DefaultStyle,
          {':', ":"},
          {verilog_tokentype::SymbolIdentifier, "x"},
          {/* any context */},
          {/* unspecified context */},
          {1, SpacingOptions::Undecided},
      },
      {
          // a ? b : c (ternary expression)
          DefaultStyle,
          {':', ":"},
          {verilog_tokentype::SymbolIdentifier, "c"},
          {/* any context */},
          {NodeEnum::kTernaryExpression},
          {1, SpacingOptions::Undecided},
      },
      {
          // a ? b : 7 (ternary expression)
          DefaultStyle,
          {':', ":"},
          {verilog_tokentype::TK_DecNumber, "7"},
          {/* any context */},
          {NodeEnum::kTernaryExpression},
          {1, SpacingOptions::Undecided},
      },
      {
          // a ? b : "7" (ternary expression)
          DefaultStyle,
          {':', ":"},
          {verilog_tokentype::TK_StringLiteral, "\"7\""},
          {/* any context */},
          {NodeEnum::kTernaryExpression},
          {1, SpacingOptions::Undecided},
      },
      {
          // a ? b : (7) (ternary expression)
          DefaultStyle,
          {':', ":"},
          {'(', "("},
          {/* any context */},
          {NodeEnum::kTernaryExpression},
          {1, SpacingOptions::Undecided},
      },

      // ':' in labels
      // ':' before and after keywords:
      {
          // "begin :"
          DefaultStyle,
          {verilog_tokentype::TK_begin, "begin"},
          {':', ":"},
          {/* unspecified context */},
          {/* unspecified context */},
          {1, SpacingOptions::Undecided},
      },
      {
          // ": begin"
          DefaultStyle,
          {':', ":"},
          {verilog_tokentype::TK_begin, "begin"},
          {/* unspecified context */},
          {/* unspecified context */},
          {1, SpacingOptions::Undecided},
      },
      {
          // "fork :"
          DefaultStyle,
          {verilog_tokentype::TK_fork, "fork"},
          {':', ":"},
          {/* unspecified context */},
          {/* unspecified context */},
          {1, SpacingOptions::Undecided},
      },
      {
          // "end :"
          DefaultStyle,
          {verilog_tokentype::TK_end, "end"},
          {':', ":"},
          {/* unspecified context */},
          {/* unspecified context */},
          {1, SpacingOptions::Undecided},
      },
      {
          // "endclass :"
          DefaultStyle,
          {verilog_tokentype::TK_endclass, "endclass"},
          {':', ":"},
          {/* unspecified context */},
          {/* unspecified context */},
          {1, SpacingOptions::Undecided},
      },
      {
          // "endfunction :"
          DefaultStyle,
          {verilog_tokentype::TK_endfunction, "endfunction"},
          {':', ":"},
          {/* unspecified context */},
          {/* unspecified context */},
          {1, SpacingOptions::Undecided},
      },
      {
          // "endtask :"
          DefaultStyle,
          {verilog_tokentype::TK_endtask, "endtask"},
          {':', ":"},
          {/* unspecified context */},
          {/* unspecified context */},
          {1, SpacingOptions::Undecided},
      },
      {
          // "endmodule :"
          DefaultStyle,
          {verilog_tokentype::TK_endmodule, "endmodule"},
          {':', ":"},
          {/* unspecified context */},
          {/* unspecified context */},
          {1, SpacingOptions::Undecided},
      },
      {
          // "endpackage :"
          DefaultStyle,
          {verilog_tokentype::TK_endpackage, "endpackage"},
          {':', ":"},
          {/* unspecified context */},
          {/* unspecified context */},
          {1, SpacingOptions::Undecided},
      },
      {
          // "endinterface :"
          DefaultStyle,
          {verilog_tokentype::TK_endinterface, "endinterface"},
          {':', ":"},
          {/* unspecified context */},
          {/* unspecified context */},
          {1, SpacingOptions::Undecided},
      },
      {
          // "endproperty :"
          DefaultStyle,
          {verilog_tokentype::TK_endproperty, "endproperty"},
          {':', ":"},
          {/* unspecified context */},
          {/* unspecified context */},
          {1, SpacingOptions::Undecided},
      },
      {
          // "endclocking :"
          DefaultStyle,
          {verilog_tokentype::TK_endclocking, "endclocking"},
          {':', ":"},
          {/* unspecified context */},
          {/* unspecified context */},
          {1, SpacingOptions::Undecided},
      },
      // endcase and endgenerate do not get labels

      // ':' before and after label identifiers:
      {
          // "id :"
          DefaultStyle,
          {verilog_tokentype::SymbolIdentifier, "id"},
          {':', ":"},
          {/* unspecified context */},
          {/* unspecified context */},
          kUnhandledSpacing,
      },
      {
          // "id :"
          DefaultStyle,
          {verilog_tokentype::SymbolIdentifier, "id"},
          {':', ":"},
          {/* unspecified context */},
          {NodeEnum::kBlockIdentifier},
          {1, SpacingOptions::Undecided},
      },
      {
          // "id : begin ..."
          DefaultStyle,
          {verilog_tokentype::SymbolIdentifier, "id"},
          {':', ":"},
          {/* unspecified context */},
          {NodeEnum::kLabeledStatement},
          {1, SpacingOptions::Undecided},
      },
      {
          // "id :"
          DefaultStyle,
          {verilog_tokentype::SymbolIdentifier, "id"},
          {':', ":"},
          {/* unspecified context */},
          {NodeEnum::kCaseItem},
          {0, SpacingOptions::Undecided},
      },
      {
          // "id :"
          DefaultStyle,
          {verilog_tokentype::SymbolIdentifier, "id"},
          {':', ":"},
          {/* unspecified context */},
          {NodeEnum::kCaseInsideItem},
          {0, SpacingOptions::Undecided},
      },
      {
          // "id :"
          DefaultStyle,
          {verilog_tokentype::SymbolIdentifier, "id"},
          {':', ":"},
          {/* unspecified context */},
          {NodeEnum::kCasePatternItem},
          {0, SpacingOptions::Undecided},
      },
      {
          // "id :"
          DefaultStyle,
          {verilog_tokentype::SymbolIdentifier, "id"},
          {':', ":"},
          {/* unspecified context */},
          {NodeEnum::kGenerateCaseItem},
          {0, SpacingOptions::Undecided},
      },
      {
          // "id :"
          DefaultStyle,
          {verilog_tokentype::SymbolIdentifier, "id"},
          {':', ":"},
          {/* unspecified context */},
          {NodeEnum::kPropertyCaseItem},
          {0, SpacingOptions::Undecided},
      },
      {
          // "id :"
          DefaultStyle,
          {verilog_tokentype::SymbolIdentifier, "id"},
          {':', ":"},
          {/* unspecified context */},
          {NodeEnum::kRandSequenceCaseItem},
          {0, SpacingOptions::Undecided},
      },
      {
          // ": id"
          DefaultStyle,
          {':', ":"},
          {verilog_tokentype::SymbolIdentifier, "id"},
          {/* unspecified context */},
          {/* unspecified context */},
          {1, SpacingOptions::Undecided},
      },
      {
          // ": id"
          DefaultStyle,
          {':', ":"},
          {verilog_tokentype::SymbolIdentifier, "id"},
          {/* unspecified context */},
          {NodeEnum::kLabel},
          {1, SpacingOptions::Undecided},
      },
      // Shift operators
      {
          // foo = 1 << width;
          DefaultStyle,
          {verilog_tokentype::TK_DecNumber, "1"},
          {verilog_tokentype::TK_LS, "<<"},
          {/* unspecified context */},
          {/* unspecified context */},
          {1, SpacingOptions::Undecided},
      },
      {
          // foo = 1 << width;
          DefaultStyle,
          {verilog_tokentype::TK_LS, "<<"},
          {verilog_tokentype::SymbolIdentifier, "width"},
          {/* unspecified context */},
          {/* unspecified context */},
          {1, SpacingOptions::Undecided},
      },
      {
          // foo = bar << 4;
          DefaultStyle,
          {verilog_tokentype::SymbolIdentifier, "bar"},
          {verilog_tokentype::TK_LS, "<<"},
          {/* unspecified context */},
          {/* unspecified context */},
          {1, SpacingOptions::Undecided},
      },
      {
          // foo = bar << 4;
          DefaultStyle,
          {verilog_tokentype::TK_LS, "<<"},
          {verilog_tokentype::TK_DecNumber, "4"},
          {/* unspecified context */},
          {/* unspecified context */},
          {1, SpacingOptions::Undecided},
      },
      {
          // foo = `VAL << 4;
          DefaultStyle,
          {verilog_tokentype::MacroIdentifier, "`VAL"},
          {verilog_tokentype::TK_LS, "<<"},
          {/* unspecified context */},
          {/* unspecified context */},
          {1, SpacingOptions::Undecided},
      },
      {
          // foo = bar << `SIZE;
          DefaultStyle,
          {verilog_tokentype::TK_LS, "<<"},
          {verilog_tokentype::MacroIdentifier, "`SIZE"},
          {/* unspecified context */},
          {/* unspecified context */},
          {1, SpacingOptions::Undecided},
      },
      {
          // foo = 1 >> width;
          DefaultStyle,
          {verilog_tokentype::TK_DecNumber, "1"},
          {verilog_tokentype::TK_RS, ">>"},
          {/* unspecified context */},
          {/* unspecified context */},
          {1, SpacingOptions::Undecided},
      },
      {
          // foo = 1 >> width;
          DefaultStyle,
          {verilog_tokentype::TK_RS, ">>"},
          {verilog_tokentype::SymbolIdentifier, "width"},
          {/* unspecified context */},
          {/* unspecified context */},
          {1, SpacingOptions::Undecided},
      },
      {
          // foo = bar >> 4;
          DefaultStyle,
          {verilog_tokentype::SymbolIdentifier, "bar"},
          {verilog_tokentype::TK_RS, ">>"},
          {/* unspecified context */},
          {/* unspecified context */},
          {1, SpacingOptions::Undecided},
      },
      {
          // foo = bar >> 4;
          DefaultStyle,
          {verilog_tokentype::TK_RS, ">>"},
          {verilog_tokentype::TK_DecNumber, "4"},
          {/* unspecified context */},
          {/* unspecified context */},
          {1, SpacingOptions::Undecided},
      },
      {
          // foo = `VAL >> 4;
          DefaultStyle,
          {verilog_tokentype::MacroIdentifier, "`VAL"},
          {verilog_tokentype::TK_RS, ">>"},
          {/* unspecified context */},
          {/* unspecified context */},
          {1, SpacingOptions::Undecided},
      },
      {
          // foo = bar >> `SIZE;
          DefaultStyle,
          {verilog_tokentype::TK_RS, ">>"},
          {verilog_tokentype::MacroIdentifier, "`SIZE"},
          {/* unspecified context */},
          {/* unspecified context */},
          {1, SpacingOptions::Undecided},
      },
      // Streaming operators
      {
          // foo = {<<{bar}};
          DefaultStyle,
          {'=', "="},
          {'{', "{"},
          {/* unspecified context */},
          {NodeEnum::kStreamingConcatenation},
          {1, SpacingOptions::Undecided},
      },
      {
          // foo = {<<{bar}};
          DefaultStyle,
          {'{', "{"},
          {verilog_tokentype::TK_LS, "<<"},
          {/* unspecified context */},
          {NodeEnum::kStreamingConcatenation},
          {0, SpacingOptions::Undecided},
      },
      {
          // foo = {<<{bar}};
          DefaultStyle,
          {verilog_tokentype::TK_LS, "<<"},
          {'{', "{"},
          {/* unspecified context */},
          {NodeEnum::kStreamingConcatenation},
          {0, SpacingOptions::Undecided},
      },
      {
          // foo = {<<{bar}};
          DefaultStyle,
          {'{', "{"},
          {verilog_tokentype::SymbolIdentifier, "bar"},
          {/* unspecified context */},
          {NodeEnum::kStreamingConcatenation},
          {0, SpacingOptions::Undecided},
      },
      {
          // foo = {<<{bar}};
          DefaultStyle,
          {verilog_tokentype::SymbolIdentifier, "bar"},
          {'}', "}"},
          {/* unspecified context */},
          {NodeEnum::kStreamingConcatenation},
          {0, SpacingOptions::Undecided},
      },
      {
          // foo = {<<4{bar}};
          DefaultStyle,
          {verilog_tokentype::TK_LS, "<<"},
          {verilog_tokentype::TK_DecNumber, "4"},
          {/* unspecified context */},
          {NodeEnum::kStreamingConcatenation},
          {0, SpacingOptions::Undecided},
      },
      {
          // foo = {<<4{bar}};
          DefaultStyle,
          {verilog_tokentype::TK_DecNumber, "4"},
          {'{', "{"},
          {/* unspecified context */},
          {NodeEnum::kStreamingConcatenation},
          {0, SpacingOptions::Undecided},
      },
      {
          // foo = {<<byte{bar}};
          DefaultStyle,
          {verilog_tokentype::TK_LS, "<<"},
          {verilog_tokentype::TK_byte, "byte"},
          {/* unspecified context */},
          {NodeEnum::kStreamingConcatenation},
          {0, SpacingOptions::Undecided},
      },
      {
          // foo = {<<byte{bar}};
          DefaultStyle,
          {verilog_tokentype::TK_byte, "byte"},
          {'{', "{"},
          {/* unspecified context */},
          {NodeEnum::kStreamingConcatenation},
          {0, SpacingOptions::Undecided},
      },
      {
          // foo = {<<type_t{bar}};
          DefaultStyle,
          {verilog_tokentype::TK_LS, "<<"},
          {verilog_tokentype::SymbolIdentifier, "type_t"},
          {/* unspecified context */},
          {NodeEnum::kStreamingConcatenation},
          {0, SpacingOptions::Undecided},
      },
      {
          // foo = {<<type_t{bar}};
          DefaultStyle,
          {verilog_tokentype::SymbolIdentifier, "type_t"},
          {'{', "{"},
          {/* unspecified context */},
          {NodeEnum::kStreamingConcatenation},
          {0, SpacingOptions::Undecided},
      },
      {
          // foo = {<<`GET_TYPE{bar}};
          DefaultStyle,
          {verilog_tokentype::TK_LS, "<<"},
          {verilog_tokentype::MacroIdentifier, "`GET_TYPE"},
          {/* unspecified context */},
          {NodeEnum::kStreamingConcatenation},
          {0, SpacingOptions::Undecided},
      },
      {
          // foo = {<<`GET_TYPE{bar}};
          DefaultStyle,
          {verilog_tokentype::MacroIdentifier, "`GET_TYPE"},
          {'{', "{"},
          {/* unspecified context */},
          {NodeEnum::kStreamingConcatenation},
          {0, SpacingOptions::Undecided},
      },
      {
          // foo = {>>{bar}};
          DefaultStyle,
          {'=', "="},
          {'{', "{"},
          {/* unspecified context */},
          {NodeEnum::kStreamingConcatenation},
          {1, SpacingOptions::Undecided},
      },
      {
          // foo = {>>{bar}};
          DefaultStyle,
          {'{', "{"},
          {verilog_tokentype::TK_RS, ">>"},
          {/* unspecified context */},
          {NodeEnum::kStreamingConcatenation},
          {0, SpacingOptions::Undecided},
      },
      {
          // foo = {>>{bar}};
          DefaultStyle,
          {verilog_tokentype::TK_RS, ">>"},
          {'{', "{"},
          {/* unspecified context */},
          {NodeEnum::kStreamingConcatenation},
          {0, SpacingOptions::Undecided},
      },
      {
          // foo = {>>{bar}};
          DefaultStyle,
          {'{', "{"},
          {verilog_tokentype::SymbolIdentifier, "bar"},
          {/* unspecified context */},
          {NodeEnum::kStreamingConcatenation},
          {0, SpacingOptions::Undecided},
      },
      {
          // foo = {>>{bar}};
          DefaultStyle,
          {verilog_tokentype::SymbolIdentifier, "bar"},
          {'}', "}"},
          {/* unspecified context */},
          {NodeEnum::kStreamingConcatenation},
          {0, SpacingOptions::Undecided},
      },
      {
          // foo = {>>4{bar}};
          DefaultStyle,
          {verilog_tokentype::TK_RS, ">>"},
          {verilog_tokentype::TK_DecNumber, "4"},
          {/* unspecified context */},
          {NodeEnum::kStreamingConcatenation},
          {0, SpacingOptions::Undecided},
      },
      {
          // foo = {>>4{bar}};
          DefaultStyle,
          {verilog_tokentype::TK_DecNumber, "4"},
          {'{', "{"},
          {/* unspecified context */},
          {NodeEnum::kStreamingConcatenation},
          {0, SpacingOptions::Undecided},
      },
      {
          // foo = {>>byte{bar}};
          DefaultStyle,
          {verilog_tokentype::TK_RS, ">>"},
          {verilog_tokentype::TK_byte, "byte"},
          {/* unspecified context */},
          {NodeEnum::kStreamingConcatenation},
          {0, SpacingOptions::Undecided},
      },
      {
          // foo = {>>byte{bar}};
          DefaultStyle,
          {verilog_tokentype::TK_byte, "byte"},
          {'{', "{"},
          {/* unspecified context */},
          {NodeEnum::kStreamingConcatenation},
          {0, SpacingOptions::Undecided},
      },
      {
          // foo = {>>type_t{bar}};
          DefaultStyle,
          {verilog_tokentype::TK_RS, ">>"},
          {verilog_tokentype::SymbolIdentifier, "type_t"},
          {/* unspecified context */},
          {NodeEnum::kStreamingConcatenation},
          {0, SpacingOptions::Undecided},
      },
      {
          // foo = {>>type_t{bar}};
          DefaultStyle,
          {verilog_tokentype::SymbolIdentifier, "type_t"},
          {'{', "{"},
          {/* unspecified context */},
          {NodeEnum::kStreamingConcatenation},
          {0, SpacingOptions::Undecided},
      },
      {
          // foo = {>>`GET_TYPE{bar}};
          DefaultStyle,
          {verilog_tokentype::TK_RS, ">>"},
          {verilog_tokentype::MacroIdentifier, "`GET_TYPE"},
          {/* unspecified context */},
          {NodeEnum::kStreamingConcatenation},
          {0, SpacingOptions::Undecided},
      },
      {
          // foo = {>>`GET_TYPE{bar}};
          DefaultStyle,
          {verilog_tokentype::MacroIdentifier, "`GET_TYPE"},
          {'{', "{"},
          {/* unspecified context */},
          {NodeEnum::kStreamingConcatenation},
          {0, SpacingOptions::Undecided},
      },
      // ':' in bit slicing and array indexing
      {
          // [1:0]
          DefaultStyle,
          {verilog_tokentype::TK_DecNumber, "1"},
          {':', ":"},
          {/* unspecified context */},
          {NodeEnum::kSelectVariableDimension},
          // no spaces preceding ':' in unit test context
          {0, SpacingOptions::Undecided},
      },
      {
          // [1:0]
          DefaultStyle,
          {':', ":"},
          {verilog_tokentype::TK_DecNumber, "0"},
          {/* unspecified context */},
          {NodeEnum::kSelectVariableDimension},
          // no spaces preceding ':' in unit test context
          {0, SpacingOptions::Undecided},
      },
      {
          // [a:b]
          DefaultStyle,
          {SymbolIdentifier, "a"},
          {':', ":"},
          {/* unspecified context */},
          {NodeEnum::kSelectVariableDimension},
          // no spaces preceding ':' in unit test context
          {0, SpacingOptions::Undecided},
      },
      {
          // [a:b]
          DefaultStyle,
          {':', ":"},
          {SymbolIdentifier, "b"},
          {/* unspecified context */},
          {NodeEnum::kSelectVariableDimension},
          // no spaces preceding ':' in unit test context
          {0, SpacingOptions::Undecided},
      },
      {
          // [1:0]
          DefaultStyle,
          {verilog_tokentype::TK_DecNumber, "1"},
          {':', ":"},
          {/* unspecified context */},
          {NodeEnum::kDimensionRange},
          // no spaces preceding ':' in unit test context
          {0, SpacingOptions::Undecided},
      },
      {
          // [1:0]
          DefaultStyle,
          {':', ":"},
          {verilog_tokentype::TK_DecNumber, "0"},
          {/* unspecified context */},
          {NodeEnum::kDimensionRange},
          // no spaces preceding ':' in unit test context
          {0, SpacingOptions::Undecided},
      },
      {
          // [a:b]
          DefaultStyle,
          {SymbolIdentifier, "a"},
          {':', ":"},
          {/* unspecified context */},
          {NodeEnum::kDimensionRange},
          // no spaces preceding ':' in unit test context
          {0, SpacingOptions::Undecided},
      },
      {
          // [a:b]
          DefaultStyle,
          {':', ":"},
          {SymbolIdentifier, "b"},
          {/* unspecified context */},
          {NodeEnum::kDimensionRange},
          // no spaces preceding ':' in unit test context
          {0, SpacingOptions::Undecided},
      },
      {
          // [1:0]
          DefaultStyle,
          {verilog_tokentype::TK_DecNumber, "1"},
          {':', ":"},
          {/* unspecified context */},
          {NodeEnum::kDimensionSlice},
          // no spaces preceding ':' in unit test context
          {0, SpacingOptions::Undecided},
      },
      {
          // [1:0]
          DefaultStyle,
          {':', ":"},
          {verilog_tokentype::TK_DecNumber, "0"},
          {/* unspecified context */},
          {NodeEnum::kDimensionSlice},
          // no spaces preceding ':' in unit test context
          {0, SpacingOptions::Undecided},
      },
      {
          // [a:b]
          DefaultStyle,
          {SymbolIdentifier, "a"},
          {':', ":"},
          {/* unspecified context */},
          {NodeEnum::kDimensionSlice},
          // no spaces preceding ':' in unit test context
          {0, SpacingOptions::Undecided},
      },
      {
          // [a:b]
          DefaultStyle,
          {':', ":"},
          {SymbolIdentifier, "b"},
          {/* unspecified context */},
          {NodeEnum::kDimensionSlice},
          // no spaces preceding ':' in unit test context
          {0, SpacingOptions::Undecided},
      },
      {
          // [1:0]
          DefaultStyle,
          {verilog_tokentype::TK_DecNumber, "1"},
          {':', ":"},
          {/* any context */},
          {NodeEnum::kCycleDelayRange},
          // no spaces preceding ':' in unit test context
          {0, SpacingOptions::Undecided},
      },
      {
          // [1:0]
          DefaultStyle,
          {':', ":"},
          {verilog_tokentype::TK_DecNumber, "0"},
          {/* any context */},
          {NodeEnum::kCycleDelayRange},
          // no spaces preceding ':' in unit test context
          {0, SpacingOptions::Undecided},
      },
      {
          // [a:b]
          DefaultStyle,
          {SymbolIdentifier, "a"},
          {':', ":"},
          {/* any context */},
          {NodeEnum::kCycleDelayRange},
          // no spaces preceding ':' in unit test context
          {0, SpacingOptions::Undecided},
      },
      {
          // [a:b]
          DefaultStyle,
          {':', ":"},
          {SymbolIdentifier, "b"},
          {/* any context */},
          {NodeEnum::kCycleDelayRange},
          // no spaces preceding ':' in unit test context
          {0, SpacingOptions::Undecided},
      },
      {
          // [1:0]
          DefaultStyle,
          {verilog_tokentype::TK_DecNumber, "1"},
          {':', ":"},
          {/* unspecified context */},
          {NodeEnum::kValueRange},
          // no spaces preceding ':' in unit test context
          {1, SpacingOptions::Undecided},
      },
      {
          // [1:0]
          DefaultStyle,
          {':', ":"},
          {verilog_tokentype::TK_DecNumber, "0"},
          {/* unspecified context */},
          {NodeEnum::kValueRange},
          // no spaces preceding ':' in unit test context
          {1, SpacingOptions::Undecided},
      },
      {
          // [a:b]
          DefaultStyle,
          {SymbolIdentifier, "a"},
          {':', ":"},
          {/* unspecified context */},
          {NodeEnum::kValueRange},
          // no spaces preceding ':' in unit test context
          {1, SpacingOptions::Undecided},
      },
      {
          // [a:b]
          DefaultStyle,
          {':', ":"},
          {SymbolIdentifier, "b"},
          {/* unspecified context */},
          {NodeEnum::kValueRange},
          // no spaces preceding ':' in unit test context
          {1, SpacingOptions::Undecided},
      },

      {
          // "] {" in "typedef logic [N] { ..."
          DefaultStyle,
          {']', "]"},
          {'{', "{"},
          {NodeEnum::kDimensionScalar},
          {/* unspecified context */},
          {1, SpacingOptions::Undecided},
      },
      {
          // "] {" in "typedef logic [M:N] { ..."
          DefaultStyle,
          {']', "]"},
          {'{', "{"},
          {NodeEnum::kDimensionRange},
          {/* unspecified context */},
          {1, SpacingOptions::Undecided},
      },
      {
          // "]{" in other contexts
          DefaultStyle,
          {']', "]"},
          {'{', "{"},
          {/* unspecified context */},
          {/* unspecified context */},
          {0, SpacingOptions::Undecided},
      },

      // name: coverpoint
      {
          DefaultStyle,
          {SymbolIdentifier, "foo_cp"},
          {':', ":"},
          {/* unspecified context */},
          {NodeEnum::kCoverPoint},
          {0, SpacingOptions::Undecided},
      },
      // coverpoint foo {
      {
          DefaultStyle,
          {SymbolIdentifier, "cpaddr"},
          {'{', "{"},
          {/* unspecified context */},
          {NodeEnum::kCoverPoint, NodeEnum::kBraceGroup},
          {1, SpacingOptions::Undecided},
      },

      // x < y (binary operator)
      {
          DefaultStyle,
          {SymbolIdentifier, "id"},
          {'<', "<"},
          {},  // default context
          {},  // default context
          {1, SpacingOptions::Undecided},
      },
      {
          DefaultStyle,
          {TK_DecNumber, "7"},
          {'<', "<"},
          {},  // default context
          {},  // default context
          {1, SpacingOptions::Undecided},
      },
      {
          DefaultStyle,
          {')', ")"},
          {'<', "<"},
          {},  // default context
          {},  // default context
          {1, SpacingOptions::Undecided},
      },
      {
          DefaultStyle,
          {'<', "<"},
          {SymbolIdentifier, "id"},
          {},  // default context
          {},  // default context
          {1, SpacingOptions::Undecided},
      },
      {
          DefaultStyle,
          {'<', "<"},
          {TK_DecNumber, "7"},
          {},  // default context
          {},  // default context
          {1, SpacingOptions::Undecided},
      },
      {
          DefaultStyle,
          {'<', "<"},
          {'(', "("},
          {},  // default context
          {},  // default context
          {1, SpacingOptions::Undecided},
      },

      // x > y (binary operator)
      {
          DefaultStyle,
          {SymbolIdentifier, "id"},
          {'>', ">"},
          {},  // default context
          {},  // default context
          {1, SpacingOptions::Undecided},
      },
      {
          DefaultStyle,
          {TK_DecNumber, "7"},
          {'>', ">"},
          {},  // default context
          {},  // default context
          {1, SpacingOptions::Undecided},
      },
      {
          DefaultStyle,
          {')', ")"},
          {'>', ">"},
          {},  // default context
          {},  // default context
          {1, SpacingOptions::Undecided},
      },
      {
          DefaultStyle,
          {'>', ">"},
          {SymbolIdentifier, "id"},
          {},  // default context
          {},  // default context
          {1, SpacingOptions::Undecided},
      },
      {
          DefaultStyle,
          {'>', ">"},
          {TK_DecNumber, "7"},
          {},  // default context
          {},  // default context
          {1, SpacingOptions::Undecided},
      },
      {
          DefaultStyle,
          {'>', ">"},
          {'(', "("},
          {},  // default context
          {},  // default context
          {1, SpacingOptions::Undecided},
      },

      // '@' on the right
      {
          DefaultStyle,
          {TK_always, "always"},
          {'@', "@"},
          {},  // default context
          {},  // default context
          {1, SpacingOptions::Undecided},
      },
      {
          DefaultStyle,
          {SymbolIdentifier, "cblock"},
          {'@', "@"},
          {},  // default context
          {},  // default context
          {1, SpacingOptions::Undecided},
      },
      // '@' on the left
      {
          DefaultStyle,
          {'@', "@"},
          {'(', "("},
          {},  // default context
          {},  // default context
          {0, SpacingOptions::Undecided},
      },
      {
          DefaultStyle,
          {'@', "@"},
          {'*', "*"},  // not a binary operator in this case
          {},          // default context
          {},          // default context
          {0, SpacingOptions::Undecided},
      },
      {
          DefaultStyle,
          {'@', "@"},
          {SymbolIdentifier, "clock_a"},
          {},  // default context
          {},  // default context
          {0, SpacingOptions::Undecided},
      },

      // '#' on the right
      {
          DefaultStyle,
          {SymbolIdentifier, "id_before_pound"},
          {'#', "#"},
          {},  // default context
          {},  // default context
               // no spaces preceding ':' in unit test context
          {1, SpacingOptions::Undecided},
      },
      {
          DefaultStyle,
          {SymbolIdentifier, "id_before_pound"},
          {'#', "#"},
          {NodeEnum::kUnqualifiedId},
          {},  // default context
          {0, SpacingOptions::Undecided},
      },
      {
          DefaultStyle,
          {SymbolIdentifier, "id_before_pound"},
          {'#', "#"},
          {NodeEnum::kQualifiedId},
          {},  // default context
          {1, SpacingOptions::Undecided},
      },

      // '}' on the left
      {
          DefaultStyle,
          {'}', "}"},
          {SymbolIdentifier, "id_before_open_brace"},
          {},  // default context
          {},  // default context
          {1, SpacingOptions::Undecided},
      },
      {
          DefaultStyle,
          {'}', "}"},
          {',', ","},
          {},  // default context
          {},  // default context
          {0, SpacingOptions::Undecided},
      },
      {
          DefaultStyle,
          {'}', "}"},
          {';', ";"},
          {},  // default context
          {},  // default context
          {0, SpacingOptions::Undecided},
      },
      {
          DefaultStyle,
          {'}', "}"},
          {'}', "}"},
          {},  // default context
          {},  // default context
          {0, SpacingOptions::Undecided},
      },

      // '{' on the right
      {
          DefaultStyle,
          {SymbolIdentifier, "id_before_open_brace"},
          {'{', "{"},
          {},  // default context
          {},  // default context
          {0, SpacingOptions::Undecided},
      },
      {
          DefaultStyle,
          {TK_unique, "unique"},
          {'{', "{"},
          {},  // default context
          {},  // default context
          {1, SpacingOptions::Undecided},
      },
      {
          DefaultStyle,
          {TK_with, "with"},
          {'{', "{"},
          {},  // default context
          {},  // default context
          {1, SpacingOptions::Undecided},
      },
      {
          // constraint c_id {
          DefaultStyle,
          {SymbolIdentifier, "id_before_open_brace"},
          {'{', "{"},
          {},  // default context
          {NodeEnum::kConstraintDeclaration, NodeEnum::kBraceGroup},
          {1, SpacingOptions::Undecided},
      },

      // ';' on the left
      {
          DefaultStyle,
          {';', ";"},
          {SymbolIdentifier, "id_after_semi"},
          {},  // default context
          {},  // default context
          {1, SpacingOptions::Undecided},
      },
      {
          DefaultStyle,
          {SemicolonEndOfAssertionVariableDeclarations, ";"},
          {SymbolIdentifier, "id_after_semi"},
          {},  // default context
          {},  // default context
          {1, SpacingOptions::Undecided},
      },

      // ';' on the right
      {
          DefaultStyle,
          {SymbolIdentifier, "id"},
          {';', ";"},
          {},  // default context
          {},  // default context
          {0, SpacingOptions::Undecided},
      },
      {
          DefaultStyle,
          {SymbolIdentifier, "id"},
          {SemicolonEndOfAssertionVariableDeclarations, ";"},
          {},  // default context
          {},  // default context
          {0, SpacingOptions::Undecided},
      },
      {
          DefaultStyle,
          {')', ")"},
          {';', ";"},
          {},                              // default context
          {},                              // default context
          {0, SpacingOptions::Undecided},  // could be MustAppend too
      },
      {
          DefaultStyle,
          {')', ")"},
          {SemicolonEndOfAssertionVariableDeclarations, ";"},
          {},                              // default context
          {},                              // default context
          {0, SpacingOptions::Undecided},  // could be MustAppend too
      },

      // keyword on right
      {
          DefaultStyle,
          {TK_DecNumber, "1"},
          {TK_begin, "begin"},
          {},  // default context
          {},  // default context
          {1, SpacingOptions::Undecided},
      },
      {
          DefaultStyle,
          {TK_begin, "begin"},
          {TK_begin, "begin"},
          {},  // default context
          {},  // default context
          {1, SpacingOptions::Undecided},
      },
      {
          DefaultStyle,
          {TK_begin, "begin"},
          {TK_end, "end"},
          {},  // default context
          {},  // default context
          {1, SpacingOptions::MustWrap},
      },
      {
          DefaultStyle,
          {TK_end, "end"},
          {TK_begin, "begin"},
          {},  // default context
          {},  // default context
          {1, SpacingOptions::Undecided},
      },
      {
          DefaultStyle,
          {TK_default, "default"},
          {TK_clocking, "clocking"},
          {},  // default context
          {},  // default context
          {1, SpacingOptions::Undecided},
      },
      {
          DefaultStyle,
          {TK_default, "default"},
          {TK_disable, "disable"},
          {},  // default context
          {},  // default context
          {1, SpacingOptions::Undecided},
      },
      {
          DefaultStyle,
          {TK_disable, "disable"},
          {TK_iff, "iff"},
          {},  // default context
          {},  // default context
          {1, SpacingOptions::Undecided},
      },
      {
          DefaultStyle,
          {TK_disable, "disable"},
          {TK_soft, "soft"},
          {},  // default context
          {},  // default context
          {1, SpacingOptions::Undecided},
      },
      {
          DefaultStyle,
          {TK_extern, "extern"},
          {TK_forkjoin, "forkjoin"},
          {},  // default context
          {},  // default context
          {1, SpacingOptions::Undecided},
      },
      {
          DefaultStyle,
          {TK_input, "input"},
          {TK_logic, "logic"},
          {},  // default context
          {},  // default context
          {1, SpacingOptions::Undecided},
      },
      {
          DefaultStyle,
          {TK_var, "var"},
          {TK_logic, "logic"},
          {},  // default context
          {},  // default context
          {1, SpacingOptions::Undecided},
      },
      {
          DefaultStyle,
          {TK_output, "output"},
          {TK_reg, "reg"},
          {},  // default context
          {},  // default context
          {1, SpacingOptions::Undecided},
      },
      {
          DefaultStyle,
          {TK_static, "static"},
          {TK_constraint, "constraint"},
          {},  // default context
          {},  // default context
          {1, SpacingOptions::Undecided},
      },
      {
          DefaultStyle,
          {TK_parameter, "parameter"},
          {TK_type, "type"},
          {},  // default context
          {},  // default context
          {1, SpacingOptions::Undecided},
      },
      {
          DefaultStyle,
          {TK_virtual, "virtual"},
          {TK_interface, "interface"},
          {},  // default context
          {},  // default context
          {1, SpacingOptions::Undecided},
      },
      {
          DefaultStyle,
          {TK_const, "const"},
          {TK_ref, "ref"},
          {},  // default context
          {},  // default context
          {1, SpacingOptions::Undecided},
      },
      {
          DefaultStyle,
          {TK_union, "union"},
          {TK_tagged, "tagged"},
          {},  // default context
          {},  // default context
          {1, SpacingOptions::Undecided},
      },
      {
          DefaultStyle,
          {';', ";"},
          {TK_end, "end"},
          {},  // default context
          {},  // default context
          {1, SpacingOptions::MustWrap},
      },
      {
          DefaultStyle,
          {';', ";"},
          {TK_endfunction, "endfunction"},
          {},  // default context
          {},  // default context
          {1, SpacingOptions::MustWrap},
      },
      {
          DefaultStyle,
          {';', ";"},
          {TK_endtask, "endtask"},
          {},  // default context
          {},  // default context
          {1, SpacingOptions::MustWrap},
      },
      {
          DefaultStyle,
          {';', ";"},
          {TK_endclass, "endclass"},
          {},  // default context
          {},  // default context
          {1, SpacingOptions::MustWrap},
      },
      {
          DefaultStyle,
          {';', ";"},
          {TK_endpackage, "endpackage"},
          {},  // default context
          {},  // default context
          {1, SpacingOptions::MustWrap},
      },
      {
          DefaultStyle,
          {SymbolIdentifier, "nettype_id"},
          {TK_with, "with"},
          {},  // default context
          {},  // default context
          {1, SpacingOptions::Undecided},
      },
      {
          DefaultStyle,
          {SymbolIdentifier, "id"},
          {TK_until, "until"},
          {},  // default context
          {},  // default context
          {1, SpacingOptions::Undecided},
      },
      {
          DefaultStyle,
          {',', ","},
          {TK_highz0, "highz0"},
          {},  // default context
          {},  // default context
          {1, SpacingOptions::Undecided},
      },
      {
          DefaultStyle,
          {',', ","},
          {TK_highz1, "highz1"},
          {},  // default context
          {},  // default context
          {1, SpacingOptions::Undecided},
      },
      // Entries spacing in primitives
      {
          // 1 0 : ? : -;
          DefaultStyle,
          {'1', "1"},
          {'0', "0"},
          {},  // default context
          {NodeEnum::kUdpSequenceEntry},
          {1, SpacingOptions::Undecided},
      },
      {
          // 1 0 : ? : -;
          DefaultStyle,
          {'0', "0"},
          {':', ":"},
          {},  // default context
          {NodeEnum::kUdpSequenceEntry},
          {1, SpacingOptions::Undecided},
      },
      {
          // 1 0 : ? : -;
          DefaultStyle,
          {':', ":"},
          {'?', "?"},
          {},  // default context
          {NodeEnum::kUdpSequenceEntry},
          {1, SpacingOptions::Undecided},
      },
      {
          // 1 0 : ? : -;
          DefaultStyle,
          {'?', "?"},
          {':', ":"},
          {},  // default context
          {NodeEnum::kUdpSequenceEntry},
          {1, SpacingOptions::Undecided},
      },
      {
          // 1 0 : ? : -;
          DefaultStyle,
          {':', ":"},
          {'-', "-"},
          {},  // default context
          {NodeEnum::kUdpSequenceEntry},
          {1, SpacingOptions::Undecided},
      },
      {
          // 1 0 : ? : -;
          DefaultStyle,
          {'-', "-"},
          {';', ";"},
          {},  // default context
          {NodeEnum::kUdpSequenceEntry},
          {0, SpacingOptions::Undecided},
      },
      {
          // 1 0 : -;
          DefaultStyle,
          {'1', "1"},
          {'0', "0"},
          {},  // default context
          {NodeEnum::kUdpCombEntry},
          {1, SpacingOptions::Undecided},
      },
      {
          // 1 0 : -;
          DefaultStyle,
          {'0', "0"},
          {':', ":"},
          {},  // default context
          {NodeEnum::kUdpCombEntry},
          {1, SpacingOptions::Undecided},
      },
      {
          // 1 0 : -;
          DefaultStyle,
          {':', ":"},
          {'-', "-"},
          {},  // default context
          {NodeEnum::kUdpCombEntry},
          {1, SpacingOptions::Undecided},
      },
      {
          // 1 0 : -;
          DefaultStyle,
          {'-', "-"},
          {';', ";"},
          {},  // default context
          {NodeEnum::kUdpCombEntry},
          {0, SpacingOptions::Undecided},
      },

      // time literals
      {
          // #1ps
          DefaultStyle,
          {'#', "#"},
          {verilog_tokentype::TK_TimeLiteral, "1ps"},
          {/* any context */},
          {/* any context */},
          {0, SpacingOptions::MustAppend},
      },
      {
          // #1ps;
          DefaultStyle,
          {verilog_tokentype::TK_TimeLiteral, "1ps"},
          {';', ";"},
          {/* any context */},
          {/* any context */},
          {0, SpacingOptions::MustAppend},
      },
      {
          DefaultStyle,
          {verilog_tokentype::TK_TimeLiteral, "1ps"},
          {verilog_tokentype::SymbolIdentifier, "task_call"},
          {/* any context */},
          {/* any context */},
          {1, SpacingOptions::Undecided},
      },
      {
          DefaultStyle,
          {verilog_tokentype::TK_TimeLiteral, "1ps"},
          {verilog_tokentype::MacroIdentifier, "`MACRO"},
          {/* any context */},
          {/* any context */},
          {1, SpacingOptions::Undecided},
      },
      {
          DefaultStyle,
          {verilog_tokentype::TK_TimeLiteral, "100ps"},
          {verilog_tokentype::MacroCallId, "`MACRO"},
          {/* any context */},
          {/* any context */},
          {1, SpacingOptions::Undecided},
      },
      {
          DefaultStyle,
          {verilog_tokentype::TK_TimeLiteral, "1ps"},
          {'#', "#"},
          {/* any context */},
          {/* any context */},
          {1, SpacingOptions::Undecided},
      },
      {
          DefaultStyle,
          {verilog_tokentype::TK_TimeLiteral, "1ps"},
          {verilog_tokentype::TK_INCR, "++"},
          {/* any context */},
          {/* any context */},
          {1, SpacingOptions::Undecided},
      },
      {
          DefaultStyle,
          {verilog_tokentype::TK_TimeLiteral, "1ps"},
          {verilog_tokentype::TK_DECR, "--"},
          {/* any context */},
          {/* any context */},
          {1, SpacingOptions::Undecided},
      },
      {
          DefaultStyle,
          {verilog_tokentype::TK_TimeLiteral, "1ps"},
          {'@', "@"},
          {/* any context */},
          {/* any context */},
          {1, SpacingOptions::Undecided},
      },
      {
          DefaultStyle,
          {verilog_tokentype::TK_TimeLiteral, "1ps"},
          {verilog_tokentype::TK_begin, "begin"},
          {/* any context */},
          {/* any context */},
          {1, SpacingOptions::Undecided},
      },
      {
          DefaultStyle,
          {verilog_tokentype::TK_TimeLiteral, "1ps"},
          {verilog_tokentype::TK_force, "force"},
          {/* any context */},
          {/* any context */},
          {1, SpacingOptions::Undecided},
      },
      {
          DefaultStyle,
          {verilog_tokentype::TK_TimeLiteral, "1ps"},
          {verilog_tokentype::TK_output, "output"},
          {/* any context */},
          {/* any context */},
          {1, SpacingOptions::Undecided},
      },
  };
  int test_index = 0;
  for (const auto& test_case : kTestCases) {
    VLOG(1) << "test_index[" << test_index << "]:";
    PreFormatToken left(&test_case.left_token);
    PreFormatToken right(&test_case.right_token);
    // Classify token type into major category
    left.format_token_enum =
        GetFormatTokenType(verilog_tokentype(left.TokenEnum()));
    right.format_token_enum =
        GetFormatTokenType(verilog_tokentype(right.TokenEnum()));

    ASSERT_NE(right.format_token_enum, FormatTokenType::eol_comment)
        << "This test does not support cases examining intertoken text. "
           "Move the test case to AnnotateBreakAroundComments instead.";

    VLOG(1) << "left context: " << test_case.left_context;
    VLOG(1) << "right context: " << test_case.right_context;
    AnnotateFormatToken(test_case.style, left, &right, test_case.left_context,
                        test_case.right_context);
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

  InitializedSyntaxTreeContext right_context;
  ExpectedInterTokenInfo expected_annotation;
};

TEST(TokenAnnotatorTest, AnnotateBreakAroundComments) {
  const std::initializer_list<AnnotateBreakAroundCommentsTestCase> kTestCases =
      {
          {// No comments
           DefaultStyle,
           '=',  // left token
           "=",
           "   ",                            // whitespace between
           verilog_tokentype::TK_DecNumber,  // right token
           "0",
           {/* unspecified context */},
           {1, SpacingOptions::Undecided}},
          {// //comment1
           // //comment2
           DefaultStyle,
           verilog_tokentype::TK_EOL_COMMENT,
           "//comment1",
           "\n",
           verilog_tokentype::TK_EOL_COMMENT,
           "//comment2",
           {},
           {2, SpacingOptions::MustWrap}},
          {// 0 // comment
           DefaultStyle,
           verilog_tokentype::TK_DecNumber,
           "0",
           "   ",
           verilog_tokentype::TK_EOL_COMMENT,
           "// comment",
           {/* unspecified context */},
           {2, SpacingOptions::MustAppend}},
          {// 0// comment
           DefaultStyle,
           verilog_tokentype::TK_DecNumber,
           "0",
           "",
           verilog_tokentype::TK_EOL_COMMENT,
           "// comment",
           {/* unspecified context */},
           {2, SpacingOptions::MustAppend}},
          {// 0 \n  // comment
           DefaultStyle,
           verilog_tokentype::TK_DecNumber,
           "0",
           " \n  ",
           verilog_tokentype::TK_EOL_COMMENT,
           "// comment",
           {/* unspecified context */},
           {2, SpacingOptions::Undecided}},
          {// // comment 1 \n  // comment 2
           DefaultStyle,
           verilog_tokentype::TK_EOL_COMMENT,
           "// comment 1",
           " \n  ",
           verilog_tokentype::TK_EOL_COMMENT,
           "// comment 2",
           {/* unspecified context */},
           {2, SpacingOptions::MustWrap}},
          {// /* comment 1 */ \n  // comment 2
           DefaultStyle,
           verilog_tokentype::TK_COMMENT_BLOCK,
           "/* comment 1 */",
           " \n  ",
           verilog_tokentype::TK_EOL_COMMENT,
           "// comment 2",
           {/* unspecified context */},
           {2, SpacingOptions::Undecided}},
          {// /* comment 1 */  // comment 2
           DefaultStyle,
           verilog_tokentype::TK_COMMENT_BLOCK,
           "/* comment 1 */",
           " ",
           verilog_tokentype::TK_EOL_COMMENT,
           "// comment 2",
           {/* unspecified context */},
           {2, SpacingOptions::MustAppend}},
          {// ;  // comment 2
           DefaultStyle,
           ';',
           ";",
           " ",
           verilog_tokentype::TK_EOL_COMMENT,
           "// comment 2",
           {/* unspecified context */},
           {2, SpacingOptions::MustAppend}},
          {// ; \n // comment 2
           DefaultStyle,
           ';',
           ";",
           " \n",
           verilog_tokentype::TK_EOL_COMMENT,
           "// comment 2",
           {/* unspecified context */},
           {2, SpacingOptions::Undecided}},
          {// ,  // comment 2
           DefaultStyle,
           ',',
           ",",
           " ",
           verilog_tokentype::TK_EOL_COMMENT,
           "// comment 2",
           {/* unspecified context */},
           {2, SpacingOptions::MustAppend}},
          {// , \n // comment 2
           DefaultStyle,
           ',',
           ",",
           "\n ",
           verilog_tokentype::TK_EOL_COMMENT,
           "// comment 2",
           {/* unspecified context */},
           {2, SpacingOptions::Undecided}},
          {// begin  // comment 2
           DefaultStyle,
           verilog_tokentype::TK_begin,
           "begin",
           " ",
           verilog_tokentype::TK_EOL_COMMENT,
           "// comment 2",
           {/* unspecified context */},
           {2, SpacingOptions::MustAppend}},
          {// begin \n // comment 2
           DefaultStyle,
           verilog_tokentype::TK_begin,
           "begin",
           "\n",
           verilog_tokentype::TK_EOL_COMMENT,
           "// comment 2",
           {/* unspecified context */},
           {2, SpacingOptions::Undecided}},
          {// else  // comment 2
           DefaultStyle,
           verilog_tokentype::TK_else,
           "else",
           " ",
           verilog_tokentype::TK_EOL_COMMENT,
           "// comment 2",
           {/* unspecified context */},
           {2, SpacingOptions::MustAppend}},
          {// else \n // comment 2
           DefaultStyle,
           verilog_tokentype::TK_else,
           "else",
           " \n  ",
           verilog_tokentype::TK_EOL_COMMENT,
           "// comment 2",
           {/* unspecified context */},
           {2, SpacingOptions::Undecided}},
          {// end  // comment 2
           DefaultStyle,
           verilog_tokentype::TK_end,
           "end",
           " ",
           verilog_tokentype::TK_EOL_COMMENT,
           "// comment 2",
           {/* unspecified context */},
           {2, SpacingOptions::MustAppend}},
          {// end \n // comment 2
           DefaultStyle,
           verilog_tokentype::TK_end,
           "end",
           "  \n ",
           verilog_tokentype::TK_EOL_COMMENT,
           "// comment 2",
           {/* unspecified context */},
           {2, SpacingOptions::Undecided}},
          {// generate  // comment 2
           DefaultStyle,
           verilog_tokentype::TK_generate,
           "generate",
           " ",
           verilog_tokentype::TK_EOL_COMMENT,
           "// comment 2",
           {/* unspecified context */},
           {2, SpacingOptions::MustAppend}},
          {// generate \n // comment 2
           DefaultStyle,
           verilog_tokentype::TK_generate,
           "generate",
           "  \n",
           verilog_tokentype::TK_EOL_COMMENT,
           "// comment 2",
           {/* unspecified context */},
           {2, SpacingOptions::Undecided}},
          {// if  // comment 2
           DefaultStyle,
           verilog_tokentype::TK_if,
           "if",
           " ",
           verilog_tokentype::TK_EOL_COMMENT,
           "// comment 2",
           {/* unspecified context */},
           {2, SpacingOptions::MustAppend}},
          {// if \n\n // comment 2
           DefaultStyle,
           verilog_tokentype::TK_if,
           "if",
           " \n\n ",
           verilog_tokentype::TK_EOL_COMMENT,
           "// comment 2",
           {/* unspecified context */},
           {2, SpacingOptions::Undecided}},
          {
              DefaultStyle,
              verilog_tokentype::MacroCallCloseToEndLine,
              ")",
              " ",
              verilog_tokentype::TK_COMMENT_BLOCK,
              "/*comment*/",
              {/* unspecified context */},
              {2, SpacingOptions::Undecided},  // could be append
          },
          {
              DefaultStyle,
              verilog_tokentype::MacroCallCloseToEndLine,
              ")",
              "\n",
              verilog_tokentype::TK_COMMENT_BLOCK,
              "/*comment*/",
              {/* unspecified context */},
              {2, SpacingOptions::Undecided},
          },
          {
              DefaultStyle,
              verilog_tokentype::MacroCallCloseToEndLine,
              ")",
              " ",
              verilog_tokentype::TK_EOL_COMMENT,
              "//comment",
              {/* unspecified context */},
              {2, SpacingOptions::MustAppend},
          },
          {
              DefaultStyle,
              verilog_tokentype::MacroCallCloseToEndLine,
              ")",
              "\n",
              verilog_tokentype::TK_EOL_COMMENT,
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
              verilog_tokentype::TK_COMMENT_BLOCK,
              "/* comment */",
              {NodeEnum::kUdpCombEntry},
              {2, SpacingOptions::Undecided},
          },
          {
              // 1  /*comment*/ 0 : -;
              DefaultStyle,
              verilog_tokentype::TK_COMMENT_BLOCK,
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
              verilog_tokentype::TK_EOL_COMMENT,
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
              verilog_tokentype::TK_COMMENT_BLOCK,
              "/* comment */",
              {NodeEnum::kUdpSequenceEntry},
              {2, SpacingOptions::Undecided},
          },
          {
              // 1  /*comment*/ 0 : -;
              DefaultStyle,
              verilog_tokentype::TK_COMMENT_BLOCK,
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
              verilog_tokentype::TK_EOL_COMMENT,
              "// comment",
              {NodeEnum::kUdpSequenceEntry},
              {2, SpacingOptions::MustAppend},
          },
          {
              // input  /* comment */ i;
              DefaultStyle,
              verilog_tokentype::TK_input,
              "input",
              "",
              verilog_tokentype::TK_COMMENT_BLOCK,
              "/* comment */",
              {NodeEnum::kUdpPortDeclaration},
              {2, SpacingOptions::Undecided},
          },
          {
              // input  /* comment */ i;
              DefaultStyle,
              verilog_tokentype::TK_COMMENT_BLOCK,
              "/* comment */",
              "",
              verilog_tokentype::SymbolIdentifier,
              "i",
              {NodeEnum::kUdpPortDeclaration},
              {1, SpacingOptions::Undecided},
          },
          {
              // input i  /* comment */;
              DefaultStyle,
              verilog_tokentype::SymbolIdentifier,
              "i",
              "",
              verilog_tokentype::TK_COMMENT_BLOCK,
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
              verilog_tokentype::TK_EOL_COMMENT,
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

    left.format_token_enum =
        GetFormatTokenType(verilog_tokentype(left.TokenEnum()));
    right.format_token_enum =
        GetFormatTokenType(verilog_tokentype(right.TokenEnum()));

    VLOG(1) << "right context: " << test_case.right_context;
    AnnotateFormatToken(test_case.style, left, &right, {},
                        test_case.right_context);
    EXPECT_EQ(test_case.expected_annotation, right.before)
        << "Index: " << test_index << " Context: " << test_case.right_context
        << " with left=" << left.Text() << " and right=" << right.Text();
    ++test_index;
  }
}

}  // namespace
}  // namespace formatter
}  // namespace verilog
