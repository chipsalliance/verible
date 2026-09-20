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

#include "verible/verilog/formatting/token-annotator.h"

#include <algorithm>
#include <initializer_list>
#include <iterator>
#include <ostream>
#include <string_view>
#include <vector>

#include "absl/log/die_if_null.h"
#include "gtest/gtest.h"
#include "verible/common/formatting/format-token.h"
#include "verible/common/formatting/unwrapped-line-test-utils.h"
#include "verible/common/formatting/unwrapped-line.h"
#include "verible/common/text/concrete-syntax-tree.h"
#include "verible/common/text/symbol-ptr.h"
#include "verible/common/text/syntax-tree-context.h"
#include "verible/common/text/token-info-test-util.h"
#include "verible/common/text/token-info.h"
#include "verible/common/util/casts.h"
#include "verible/common/util/iterator-adaptors.h"
#include "verible/common/util/iterator-range.h"
#include "verible/common/util/logging.h"
#include "verible/verilog/CST/verilog-nonterminals.h"
#include "verible/verilog/formatting/format-style.h"
#include "verible/verilog/formatting/verilog-token.h"
#include "verible/verilog/parser/verilog-token-enum.h"

namespace verilog {
namespace formatter {

using ::verible::InterTokenInfo;
using ::verible::PreFormatToken;
using ::verible::SpacingOptions;

// Private function with external linkage from token_annotator.cc.
extern void AnnotateFormatToken(const FormatStyle &style,
                                const PreFormatToken &prev_token,
                                PreFormatToken *curr_token,
                                const verible::SyntaxTreeContext &prev_context,
                                const verible::SyntaxTreeContext &curr_context);

namespace {

// TODO(fangism): Move much of this boilerplate to format_token_test_util.h.

// This test structure is a subset of InterTokenInfo.
// We do not want to compare break penalties, because that would be too
// change-detector-y.
struct ExpectedInterTokenInfo {
  constexpr ExpectedInterTokenInfo(int spaces, const SpacingOptions &bd)
      : spaces_required(spaces), break_decision(bd) {}

  int spaces_required = 0;
  SpacingOptions break_decision = SpacingOptions::kUndecided;

  bool operator==(const InterTokenInfo &before) const {
    return spaces_required == before.spaces_required &&
           break_decision == before.break_decision;
  }

  bool operator!=(const InterTokenInfo &before) const {
    return !(*this == before);
  }
};

std::ostream &operator<<(std::ostream &stream,
                         const ExpectedInterTokenInfo &t) {
  stream << "{\n  spaces_required: " << t.spaces_required
         << "\n  break_decision: " << t.break_decision << "\n}";
  return stream;
}

// Returns false if all ExpectedFormattingCalculations are not equal and outputs
// the first difference.
// type T is any container or range over PreFormatTokens.
template <class T>
bool CorrectExpectedFormatTokens(
    const std::vector<ExpectedInterTokenInfo> &expected, const T &tokens) {
  EXPECT_EQ(expected.size(), tokens.size())
      << "Size of expected calculations and format tokens does not match.";
  if (expected.size() != tokens.size()) {
    return false;
  }

  const auto first_mismatch =
      std::mismatch(expected.cbegin(), expected.cend(), tokens.begin(),
                    [](const ExpectedInterTokenInfo &expected,
                       const PreFormatToken &token) -> bool {
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
std::ostream &operator<<(
    std::ostream &stream,
    const AnnotateFormattingInformationTestCase &test_case) {
  stream << '[';
  for (const auto &token : test_case.input_tokens) {
    stream << ' ' << token.text();
  }
  return stream << " ]";
}

// Pre-populates context stack for testing context-sensitive annotations.
// TODO(fangism): This class is easily made language-agnostic, and could
// move into a _test_util library.
class InitializedSyntaxTreeContext : public verible::SyntaxTreeContext {
 public:
  InitializedSyntaxTreeContext(std::initializer_list<NodeEnum> ancestors) {
    // Build up a "skinny" tree from the bottom-up, much like the parser does.
    std::vector<verible::SyntaxTreeNode *> parents;
    parents.reserve(ancestors.size());
    for (const auto ancestor : verible::reversed_view(ancestors)) {
      if (root_ == nullptr) {
        root_ = verible::MakeTaggedNode(ancestor);
      } else {
        root_ = verible::MakeTaggedNode(ancestor, root_);
      }
      parents.push_back(ABSL_DIE_IF_NULL(
          verible::down_cast<verible::SyntaxTreeNode *>(root_.get())));
    }
    for (const auto *parent : verible::reversed_view(parents)) {
      Push(parent);
    }
  }

 private:
  // Syntax tree synthesized from sequence of node enums.
  verible::SymbolPtr root_;
};

std::ostream &operator<<(std::ostream &stream,
                         const InitializedSyntaxTreeContext &context) {
  stream << "[ ";
  for (const auto *node : verible::make_range(context.begin(), context.end())) {
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
                                                   SpacingOptions::kPreserve};

// This test is going to ensure that given an UnwrappedLine, the format
// tokens are propagated with the correct annotations and spaces_required.
// SpacingOptions::Preserve implies that the particular token pair combination
// was not explicitly handled and just defaulted.
// This test covers cases that are not context-sensitive.
TEST(TokenAnnotatorTest, AnnotateFormattingInfoTest) {
  static const AnnotateFormattingInformationTestCase kTestCases[] = {
      // (empty array of tokens)
      {.style = DefaultStyle,
       .uwline_indentation = 0,
       .expected_calculations = {},
       .input_tokens = {}},

      // //comment1
      // //comment2
      {.style = DefaultStyle,
       .uwline_indentation = 0,
       // ExpectedInterTokenInfo:
       // spaces_required, break_decision
       .expected_calculations = {{0, SpacingOptions::kUndecided},  //
                                 {2, SpacingOptions::kMustWrap}},
       .input_tokens = {{verilog_tokentype::TK_EOL_COMMENT, "//comment1"},
                        {verilog_tokentype::TK_EOL_COMMENT, "//comment2"}}},

      // If there is no newline before comment, it will be appended
      // (  //comment
      {.style = DefaultStyle,
       .uwline_indentation = 0,
       .expected_calculations = {{0, SpacingOptions::kUndecided},  //
                                 {2, SpacingOptions::kMustAppend}},
       .input_tokens = {{'(', "("},
                        {verilog_tokentype::TK_EOL_COMMENT, "//comment"}}},

      // [  //comment
      {.style = DefaultStyle,
       .uwline_indentation = 0,
       .expected_calculations = {{0, SpacingOptions::kUndecided},  //
                                 {2, SpacingOptions::kMustAppend}},
       .input_tokens = {{'[', "["},
                        {verilog_tokentype::TK_EOL_COMMENT, "//comment"}}},

      // {  //comment
      {.style = DefaultStyle,
       .uwline_indentation = 0,
       .expected_calculations = {{0, SpacingOptions::kUndecided},  //
                                 {2, SpacingOptions::kMustAppend}},
       .input_tokens = {{'{', "{"},
                        {verilog_tokentype::TK_EOL_COMMENT, "//comment"}}},

      // ,  //comment
      {.style = DefaultStyle,
       .uwline_indentation = 0,
       .expected_calculations = {{0, SpacingOptions::kUndecided},  //
                                 {2, SpacingOptions::kMustAppend}},
       .input_tokens = {{',', ","},
                        {verilog_tokentype::TK_EOL_COMMENT, "//comment"}}},

      // ;  //comment
      {.style = DefaultStyle,
       .uwline_indentation = 0,
       .expected_calculations = {{0, SpacingOptions::kUndecided},  //
                                 {2, SpacingOptions::kMustAppend}},
       .input_tokens = {{';', ";"},
                        {verilog_tokentype::TK_EOL_COMMENT, "//comment"}}},

      // module foo();
      {.style = DefaultStyle,
       .uwline_indentation = 0,
       .expected_calculations = {{0, SpacingOptions::kUndecided},
                                 {1, SpacingOptions::kUndecided},
                                 {0, SpacingOptions::kUndecided},
                                 {0, SpacingOptions::kUndecided},
                                 {0, SpacingOptions::kUndecided}},
       .input_tokens = {{verilog_tokentype::TK_module, "module"},
                        {verilog_tokentype::SymbolIdentifier, "foo"},
                        {'(', "("},
                        {')', ")"},
                        {';', ";"}}},

      // module foo(a, b);
      {.style = DefaultStyle,
       .uwline_indentation = 0,
       .expected_calculations = {{0, SpacingOptions::kUndecided},
                                 {1, SpacingOptions::kUndecided},
                                 {0, SpacingOptions::kUndecided},
                                 {0, SpacingOptions::kUndecided},  // "a"
                                 {0, SpacingOptions::kUndecided},  // ','
                                 {1, SpacingOptions::kUndecided},  // "b"
                                 {0, SpacingOptions::kUndecided},
                                 {0, SpacingOptions::kUndecided}},
       .input_tokens = {{verilog_tokentype::TK_module, "module"},
                        {verilog_tokentype::SymbolIdentifier, "foo"},
                        {'(', "("},
                        {verilog_tokentype::SymbolIdentifier, "a"},
                        {',', ","},
                        {verilog_tokentype::SymbolIdentifier, "b"},
                        {')', ")"},
                        {';', ";"}}},

      // module with_params #() ();
      {.style = DefaultStyle,
       .uwline_indentation = 0,
       .expected_calculations = {{0, SpacingOptions::kUndecided},
                                 {1,
                                  SpacingOptions::kUndecided},  // with_params
                                 {1, SpacingOptions::kUndecided},   // #
                                 {0, SpacingOptions::kMustAppend},  // (
                                 {0, SpacingOptions::kUndecided},   // )
                                 {1, SpacingOptions::kUndecided},   // (
                                 {0, SpacingOptions::kUndecided},   // )
                                 {0, SpacingOptions::kUndecided}},  // ;
       .input_tokens = {{verilog_tokentype::TK_module, "module"},
                        {verilog_tokentype::SymbolIdentifier, "with_params"},
                        {'#', "#"},
                        {'(', "("},
                        {')', ")"},
                        {'(', "("},
                        {')', ")"},
                        {';', ";"}}},

      // a = b[c];
      {.style = DefaultStyle,
       .uwline_indentation = 0,
       .expected_calculations = {{0, SpacingOptions::kUndecided},
                                 {1, SpacingOptions::kUndecided},
                                 {1, SpacingOptions::kUndecided},
                                 {0, SpacingOptions::kUndecided},
                                 {0, SpacingOptions::kUndecided},
                                 {0, SpacingOptions::kUndecided},
                                 {0, SpacingOptions::kUndecided}},
       .input_tokens = {{verilog_tokentype::SymbolIdentifier, "a"},
                        {'=', "="},
                        {verilog_tokentype::SymbolIdentifier, "b"},
                        {'[', "["},
                        {verilog_tokentype::SymbolIdentifier, "c"},
                        {']', "]"},
                        {';', ";"}}},

      // b[c][d] (multi-dimensional spacing)
      {.style = DefaultStyle,
       .uwline_indentation = 0,
       .expected_calculations = {{0, SpacingOptions::kUndecided},
                                 {0, SpacingOptions::kUndecided},
                                 {0, SpacingOptions::kUndecided},
                                 {0, SpacingOptions::kUndecided},
                                 {0, SpacingOptions::kUndecided},
                                 {0, SpacingOptions::kUndecided},
                                 {0, SpacingOptions::kUndecided}},
       .input_tokens = {{verilog_tokentype::SymbolIdentifier, "b"},
                        {'[', "["},
                        {verilog_tokentype::SymbolIdentifier, "c"},
                        {']', "]"},
                        {'[', "["},
                        {verilog_tokentype::SymbolIdentifier, "d"},
                        {']', "]"}}},

      // always @(posedge clk)
      {.style = DefaultStyle,
       .uwline_indentation = 0,
       .expected_calculations = {{0, SpacingOptions::kUndecided},   // always
                                 {1, SpacingOptions::kUndecided},   // @
                                 {0, SpacingOptions::kUndecided},   // (
                                 {0, SpacingOptions::kUndecided},   // posedge
                                 {1, SpacingOptions::kUndecided},   // clk
                                 {0, SpacingOptions::kUndecided}},  // )
       .input_tokens = {{verilog_tokentype::TK_always, "always"},
                        {'@', "@"},
                        {'(', "("},
                        {verilog_tokentype::TK_posedge, "TK_posedge"},
                        {verilog_tokentype::SymbolIdentifier, "clk"},
                        {')', ")"}}},

      // `WIDTH'(s) (casting operator)
      {.style = DefaultStyle,
       .uwline_indentation = 0,
       .expected_calculations = {{0, SpacingOptions::kUndecided},
                                 {0, SpacingOptions::kUndecided},
                                 {0, SpacingOptions::kUndecided},
                                 {0, SpacingOptions::kUndecided},
                                 {0, SpacingOptions::kUndecided}},
       .input_tokens = {{verilog_tokentype::MacroIdItem, "`WIDTH"},
                        {'\'', "'"},
                        {'(', "("},
                        {verilog_tokentype::SymbolIdentifier, "s"},
                        {')', ")"}}},

      // string'(s) (casting operator)
      {.style = DefaultStyle,
       .uwline_indentation = 0,
       .expected_calculations = {{0, SpacingOptions::kUndecided},
                                 {0, SpacingOptions::kUndecided},
                                 {0, SpacingOptions::kUndecided},
                                 {0, SpacingOptions::kUndecided},
                                 {0, SpacingOptions::kUndecided}},
       .input_tokens = {{verilog_tokentype::TK_string, "string"},
                        {'\'', "'"},
                        {'(', "("},
                        {verilog_tokentype::SymbolIdentifier, "s"},
                        {')', ")"}}},

      // void'(f()) (casting operator)
      {.style = DefaultStyle,
       .uwline_indentation = 0,
       .expected_calculations = {{0, SpacingOptions::kUndecided},
                                 {0, SpacingOptions::kUndecided},
                                 {0, SpacingOptions::kUndecided},
                                 {0, SpacingOptions::kUndecided},
                                 {0, SpacingOptions::kUndecided},
                                 {0, SpacingOptions::kUndecided},
                                 {0, SpacingOptions::kUndecided}},
       .input_tokens = {{verilog_tokentype::TK_void, "void"},
                        {'\'', "'"},
                        {'(', "("},
                        {verilog_tokentype::SymbolIdentifier, "f"},
                        {'(', "("},
                        {')', ")"},
                        {')', ")"}}},

      // 12'{34}
      {.style = DefaultStyle,
       .uwline_indentation = 0,
       .expected_calculations = {{0, SpacingOptions::kUndecided},
                                 {0, SpacingOptions::kUndecided},
                                 {0, SpacingOptions::kUndecided},
                                 {0, SpacingOptions::kUndecided},
                                 {0, SpacingOptions::kUndecided}},
       .input_tokens = {{verilog_tokentype::TK_DecNumber, "12"},
                        {'\'', "'"},
                        {'{', "{"},
                        {verilog_tokentype::TK_DecNumber, "34"},
                        {'}', "}"}}},

      // k()'(s) (casting operator)
      {.style = DefaultStyle,
       .uwline_indentation = 0,
       .expected_calculations = {{0, SpacingOptions::kUndecided},
                                 {0, SpacingOptions::kUndecided},
                                 {0, SpacingOptions::kUndecided},
                                 {0, SpacingOptions::kUndecided},
                                 {0, SpacingOptions::kUndecided},
                                 {0, SpacingOptions::kUndecided},
                                 {0, SpacingOptions::kUndecided}},
       .input_tokens = {{verilog_tokentype::SymbolIdentifier, "k"},
                        {'(', "("},
                        {')', ")"},
                        {'\'', "'"},
                        {'(', "("},
                        {verilog_tokentype::SymbolIdentifier, "s"},
                        {')', ")"}}},

      // #1 $display
      {
          .style = DefaultStyle,
          .uwline_indentation = 0,
          .expected_calculations = {{0, SpacingOptions::kUndecided},
                                    {0, SpacingOptions::kMustAppend},
                                    {1, SpacingOptions::kUndecided}},
          .input_tokens = {{'#', "#"},
                           {verilog_tokentype::TK_DecNumber, "1"},
                           {verilog_tokentype::SystemTFIdentifier, "$display"}},
      },

      // 666 777
      {
          .style = DefaultStyle,
          .uwline_indentation = 0,
          .expected_calculations = {{0, SpacingOptions::kUndecided},
                                    {1, SpacingOptions::kUndecided}},
          .input_tokens = {{verilog_tokentype::TK_DecNumber, "666"},
                           {verilog_tokentype::TK_DecNumber, "777"}},
      },

      // 5678 dance
      {
          .style = DefaultStyle,
          .uwline_indentation = 0,
          .expected_calculations = {{0, SpacingOptions::kUndecided},
                                    {1, SpacingOptions::kUndecided}},
          .input_tokens = {{verilog_tokentype::TK_DecNumber, "5678"},
                           {verilog_tokentype::SymbolIdentifier, "dance"}},
      },

      // id 4321
      {
          .style = DefaultStyle,
          .uwline_indentation = 0,
          .expected_calculations = {{0, SpacingOptions::kUndecided},
                                    {1, SpacingOptions::kUndecided}},
          .input_tokens = {{verilog_tokentype::SymbolIdentifier, "id"},
                           {verilog_tokentype::TK_DecNumber, "4321"}},
      },

      // id1 id2
      {
          .style = DefaultStyle,
          .uwline_indentation = 0,
          .expected_calculations = {{0, SpacingOptions::kUndecided},
                                    {1, SpacingOptions::kUndecided}},
          .input_tokens = {{verilog_tokentype::SymbolIdentifier, "id1"},
                           {verilog_tokentype::SymbolIdentifier, "id2"}},
      },

      // class mate
      {
          .style = DefaultStyle,
          .uwline_indentation = 0,
          .expected_calculations = {{0, SpacingOptions::kUndecided},
                                    {1, SpacingOptions::kUndecided}},
          .input_tokens = {{verilog_tokentype::TK_class, "class"},
                           {verilog_tokentype::SymbolIdentifier, "mate"}},
      },

      // id module
      {
          .style = DefaultStyle,
          .uwline_indentation = 0,
          .expected_calculations = {{0, SpacingOptions::kUndecided},
                                    {1, SpacingOptions::kUndecided}},
          .input_tokens = {{verilog_tokentype::SymbolIdentifier, "lunar"},
                           {verilog_tokentype::TK_module, "module"}},
      },

      // class 1337
      {
          .style = DefaultStyle,
          .uwline_indentation = 0,
          .expected_calculations = {{0, SpacingOptions::kUndecided},
                                    {1, SpacingOptions::kUndecided}},
          .input_tokens = {{verilog_tokentype::TK_class, "class"},
                           {verilog_tokentype::TK_DecNumber, "1337"}},
      },

      // 987 module
      {
          .style = DefaultStyle,
          .uwline_indentation = 0,
          .expected_calculations = {{0, SpacingOptions::kUndecided},
                                    {1, SpacingOptions::kUndecided}},
          .input_tokens = {{verilog_tokentype::TK_DecNumber, "987"},
                           {verilog_tokentype::TK_module, "module"}},
      },

      // a = 16'hf00d;
      {.style = DefaultStyle,
       .uwline_indentation = 0,
       .expected_calculations = {{0, SpacingOptions::kUndecided},
                                 {1, SpacingOptions::kUndecided},
                                 {1, SpacingOptions::kUndecided},
                                 {0, SpacingOptions::kMustAppend},
                                 {0, SpacingOptions::kMustAppend},
                                 {0, SpacingOptions::kUndecided}},
       .input_tokens = {{verilog_tokentype::SymbolIdentifier, "a"},
                        {'=', "="},
                        {verilog_tokentype::TK_DecNumber, "16"},
                        {verilog_tokentype::TK_HexBase, "'h"},
                        {verilog_tokentype::TK_HexDigits, "c0ffee"},
                        {';', ";"}}},

      // a = 8'b1001_0110;
      {.style = DefaultStyle,
       .uwline_indentation = 0,
       .expected_calculations = {{0, SpacingOptions::kUndecided},
                                 {1, SpacingOptions::kUndecided},
                                 {1, SpacingOptions::kUndecided},
                                 {0, SpacingOptions::kMustAppend},
                                 {0, SpacingOptions::kMustAppend},
                                 {0, SpacingOptions::kUndecided}},
       .input_tokens = {{verilog_tokentype::SymbolIdentifier, "a"},
                        {'=', "="},
                        {verilog_tokentype::TK_DecNumber, "8"},
                        {verilog_tokentype::TK_BinBase, "'b"},
                        {verilog_tokentype::TK_BinDigits, "1001_0110"},
                        {';', ";"}}},

      // a = 4'd10;
      {.style = DefaultStyle,
       .uwline_indentation = 0,
       .expected_calculations = {{0, SpacingOptions::kUndecided},
                                 {1, SpacingOptions::kUndecided},
                                 {1, SpacingOptions::kUndecided},
                                 {0, SpacingOptions::kMustAppend},
                                 {0, SpacingOptions::kMustAppend},
                                 {0, SpacingOptions::kUndecided}},
       .input_tokens = {{verilog_tokentype::SymbolIdentifier, "a"},
                        {'=', "="},
                        {verilog_tokentype::TK_DecNumber, "4"},
                        {verilog_tokentype::TK_DecBase, "'d"},
                        {verilog_tokentype::TK_DecDigits, "10"},
                        {';', ";"}}},

      // a = 8'o100;
      {.style = DefaultStyle,
       .uwline_indentation = 0,
       .expected_calculations = {{0, SpacingOptions::kUndecided},
                                 {1, SpacingOptions::kUndecided},
                                 {1, SpacingOptions::kUndecided},
                                 {0, SpacingOptions::kMustAppend},
                                 {0, SpacingOptions::kMustAppend},
                                 {0, SpacingOptions::kUndecided}},
       .input_tokens = {{verilog_tokentype::SymbolIdentifier, "a"},
                        {'=', "="},
                        {verilog_tokentype::TK_DecNumber, "8"},
                        {verilog_tokentype::TK_OctBase, "'o"},
                        {verilog_tokentype::TK_OctDigits, "100"},
                        {';', ";"}}},

      // a = 'hc0ffee;
      {.style = DefaultStyle,
       .uwline_indentation = 0,
       .expected_calculations = {{0, SpacingOptions::kUndecided},
                                 {1, SpacingOptions::kUndecided},
                                 {1, SpacingOptions::kUndecided},
                                 {0, SpacingOptions::kMustAppend},
                                 {0, SpacingOptions::kUndecided}},
       .input_tokens = {{verilog_tokentype::SymbolIdentifier, "a"},
                        {'=', "="},
                        {verilog_tokentype::TK_HexBase, "'h"},
                        {verilog_tokentype::TK_HexDigits, "c0ffee"},
                        {';', ";"}}},

      // a = funk('b0, 'd'8);
      {.style = DefaultStyle,
       .uwline_indentation = 0,
       .expected_calculations = {{0, SpacingOptions::kUndecided},
                                 {1, SpacingOptions::kUndecided},
                                 {1, SpacingOptions::kUndecided},
                                 {0, SpacingOptions::kUndecided},
                                 {0, SpacingOptions::kUndecided},
                                 {0, SpacingOptions::kMustAppend},
                                 {0, SpacingOptions::kUndecided},
                                 {1, SpacingOptions::kUndecided},
                                 {0, SpacingOptions::kMustAppend},
                                 {0, SpacingOptions::kUndecided},
                                 {0, SpacingOptions::kUndecided}},
       .input_tokens = {{verilog_tokentype::SymbolIdentifier, "a"},
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
      {.style = DefaultStyle,
       .uwline_indentation = 0,
       .expected_calculations = {{0, SpacingOptions::kUndecided},
                                 {1, SpacingOptions::kUndecided},
                                 {1, SpacingOptions::kUndecided},
                                 {0, SpacingOptions::kMustAppend},
                                 {1, SpacingOptions::kUndecided},
                                 {1, SpacingOptions::kUndecided},
                                 {0, SpacingOptions::kMustAppend},
                                 {0, SpacingOptions::kUndecided}},
       .input_tokens = {{verilog_tokentype::SymbolIdentifier, "a"},
                        {'=', "="},
                        {verilog_tokentype::TK_BinBase, "'b"},
                        {verilog_tokentype::TK_BinDigits, "0"},
                        {'+', "+"},
                        {verilog_tokentype::TK_DecBase, "'d"},
                        {verilog_tokentype::TK_DecDigits, "9"},
                        {';', ";"}}},

      // a = {3{4'd9, 1'bz}};
      {.style = DefaultStyle,
       .uwline_indentation = 0,
       .expected_calculations = {{0, SpacingOptions::kUndecided},
                                 {1, SpacingOptions::kUndecided},
                                 {1, SpacingOptions::kUndecided},
                                 {0, SpacingOptions::kUndecided},  //  3
                                 {0, SpacingOptions::kUndecided},
                                 {0, SpacingOptions::kUndecided},
                                 {0, SpacingOptions::kMustAppend},
                                 {0, SpacingOptions::kMustAppend},
                                 {0, SpacingOptions::kUndecided},  //  ,
                                 {1, SpacingOptions::kUndecided},
                                 {0, SpacingOptions::kMustAppend},
                                 {0, SpacingOptions::kMustAppend},  //  z
                                 {0, SpacingOptions::kUndecided},
                                 {0, SpacingOptions::kUndecided},
                                 {0, SpacingOptions::kUndecided}},
       .input_tokens = {{verilog_tokentype::SymbolIdentifier, "a"},
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
          .style = DefaultStyle,
          .uwline_indentation = 0,
          .expected_calculations =
              {
                  {0, SpacingOptions::kUndecided},  //  a
                  {1, SpacingOptions::kUndecided},  //  ?
                  {1, SpacingOptions::kUndecided},  //  b
              },
          .input_tokens =
              {
                  {verilog_tokentype::SymbolIdentifier, "a"},
                  {'?', "?"},
                  {verilog_tokentype::SymbolIdentifier, "b"},
              },
      },

      // 1 ? 2 : 3
      {
          .style = DefaultStyle,
          .uwline_indentation = 0,
          .expected_calculations =
              {
                  {0, SpacingOptions::kUndecided},  //  1
                  {1, SpacingOptions::kUndecided},  //  ?
                  {1, SpacingOptions::kUndecided},  //  2
              },
          .input_tokens =
              {
                  {verilog_tokentype::TK_DecNumber, "1"},
                  {'?', "?"},
                  {verilog_tokentype::TK_DecNumber, "2"},
              },
      },

      // "1" ? "2" : "3"
      {
          .style = DefaultStyle,
          .uwline_indentation = 0,
          .expected_calculations =
              {
                  {0, SpacingOptions::kUndecided},  //  "1"
                  {1, SpacingOptions::kUndecided},  //  ?
                  {1, SpacingOptions::kUndecided},  //  "2"
              },
          .input_tokens =
              {
                  {verilog_tokentype::TK_StringLiteral, "1"},
                  {'?', "?"},
                  {verilog_tokentype::TK_StringLiteral, "2"},
              },
      },

      // b ? 8'o100 : '0;
      {.style = DefaultStyle,
       .uwline_indentation = 0,
       .expected_calculations = {{0, SpacingOptions::kUndecided},   //  b
                                 {1, SpacingOptions::kUndecided},   //  ?
                                 {1, SpacingOptions::kUndecided},   //  8
                                 {0, SpacingOptions::kMustAppend},  //  'o
                                 {0, SpacingOptions::kMustAppend},  //  100
                                 kUnhandledSpacing,                 //  :
                                 {1, SpacingOptions::kUndecided},   //  '0
                                 {0, SpacingOptions::kUndecided}},  //  ;
       .input_tokens = {{verilog_tokentype::SymbolIdentifier, "b"},
                        {'?', "?"},
                        {verilog_tokentype::TK_DecNumber, "8"},
                        {verilog_tokentype::TK_OctBase, "'o"},
                        {verilog_tokentype::TK_OctDigits, "100"},
                        {':', ":"},
                        {verilog_tokentype::TK_UnBasedNumber, "'0"},
                        {';', ";"}}},

      // a = (b + c);
      {.style = DefaultStyle,
       .uwline_indentation = 0,
       .expected_calculations = {{0, SpacingOptions::kUndecided},   // a
                                 {1, SpacingOptions::kUndecided},   // =
                                 {1, SpacingOptions::kUndecided},   // (
                                 {0, SpacingOptions::kUndecided},   // b
                                 {1, SpacingOptions::kUndecided},   // +
                                 {1, SpacingOptions::kUndecided},   // c
                                 {0, SpacingOptions::kUndecided},   // )
                                 {0, SpacingOptions::kUndecided}},  // ;
       .input_tokens = {{verilog_tokentype::SymbolIdentifier, "a"},
                        {'=', "="},
                        {'(', "("},
                        {verilog_tokentype::SymbolIdentifier, "b"},
                        {'+', "+"},
                        {verilog_tokentype::SymbolIdentifier, "c"},
                        {')', ")"},
                        {';', ";"}}},

      // function foo(name = "foo");
      {.style = DefaultStyle,
       .uwline_indentation = 0,
       .expected_calculations = {{0, SpacingOptions::kUndecided},   //  function
                                 {1, SpacingOptions::kUndecided},   //  foo
                                 {0, SpacingOptions::kUndecided},   //  (
                                 {0, SpacingOptions::kUndecided},   //  name
                                 {1, SpacingOptions::kUndecided},   //  =
                                 {1, SpacingOptions::kUndecided},   //  "foo"
                                 {0, SpacingOptions::kUndecided},   //  )
                                 {0, SpacingOptions::kUndecided}},  //  ;
       .input_tokens = {{verilog_tokentype::TK_function, "function"},
                        {verilog_tokentype::SymbolIdentifier, "foo"},
                        {'(', "("},
                        {verilog_tokentype::SymbolIdentifier, "name"},
                        {'=', "="},
                        {verilog_tokentype::TK_StringLiteral, "\"foo\""},
                        {')', ")"},
                        {';', ";"}}},

      // `define FOO(name = "bar")
      {.style = DefaultStyle,
       .uwline_indentation = 0,
       .expected_calculations = {{0, SpacingOptions::kUndecided},   //  `define
                                 {1, SpacingOptions::kMustAppend},  //  FOO
                                 {0, SpacingOptions::kUndecided},   //  (
                                 {0, SpacingOptions::kUndecided},   //  name
                                 {1, SpacingOptions::kUndecided},   //  =
                                 {1, SpacingOptions::kUndecided},   //  "bar"
                                 {0, SpacingOptions::kUndecided}},  //  )
       .input_tokens = {{verilog_tokentype::PP_define, "`define"},
                        {verilog_tokentype::SymbolIdentifier, "FOO"},
                        {'(', "("},
                        {verilog_tokentype::SymbolIdentifier, "name"},
                        {'=', "="},
                        {verilog_tokentype::TK_StringLiteral, "\"bar\""},
                        {')', ")"}}},

      // endfunction : funk
      {.style = DefaultStyle,
       .uwline_indentation = 1,
       .expected_calculations = {{0, SpacingOptions::kUndecided},
                                 {1, SpacingOptions::kUndecided},
                                 {1, SpacingOptions::kUndecided}},
       .input_tokens =
           {
               {verilog_tokentype::TK_endfunction, "endfunction"},
               {':', ":"},
               {verilog_tokentype::SymbolIdentifier, "funk"},
           }},

      // case (expr):
      {.style = DefaultStyle,
       .uwline_indentation = 1,
       .expected_calculations =
           {
               {0, SpacingOptions::kUndecided},
               {1, SpacingOptions::kUndecided},
               {0, SpacingOptions::kUndecided},
               {0, SpacingOptions::kUndecided},
               {0, SpacingOptions::kUndecided},
           },
       .input_tokens =
           {
               {verilog_tokentype::TK_case, "case"},
               {'(', "("},
               {verilog_tokentype::SymbolIdentifier, "expr"},
               {')', ")"},
               {':', ":"},
           }},

      // return 0;
      {.style = DefaultStyle,
       .uwline_indentation = 1,
       .expected_calculations =
           {
               {0, SpacingOptions::kUndecided},
               {1, SpacingOptions::kUndecided},
               {0, SpacingOptions::kUndecided},
           },
       .input_tokens =
           {
               {verilog_tokentype::TK_return, "return"},
               {verilog_tokentype::TK_UnBasedNumber, "0"},
               {';', ";"},
           }},

      // funk();
      {.style = DefaultStyle,
       .uwline_indentation = 1,
       .expected_calculations =
           {
               {0, SpacingOptions::kUndecided},
               {0, SpacingOptions::kUndecided},
               {0, SpacingOptions::kUndecided},
               {0, SpacingOptions::kUndecided},
           },
       .input_tokens =
           {
               {verilog_tokentype::SymbolIdentifier, "funk"},
               {'(', "("},
               {')', ")"},
               {';', ";"},
           }},

      // funk(arg);
      {.style = DefaultStyle,
       .uwline_indentation = 1,
       .expected_calculations =
           {
               {0, SpacingOptions::kUndecided},
               {0, SpacingOptions::kUndecided},
               {0, SpacingOptions::kUndecided},
               {0, SpacingOptions::kUndecided},
               {0, SpacingOptions::kUndecided},
           },
       .input_tokens =
           {
               {verilog_tokentype::SymbolIdentifier, "funk"},
               {'(', "("},
               {verilog_tokentype::SymbolIdentifier, "arg"},
               {')', ")"},
               {';', ";"},
           }},

      // funk("arg");
      {.style = DefaultStyle,
       .uwline_indentation = 1,
       .expected_calculations =
           {
               {0, SpacingOptions::kUndecided},
               {0, SpacingOptions::kUndecided},
               {0, SpacingOptions::kUndecided},
               {0, SpacingOptions::kUndecided},
               {0, SpacingOptions::kUndecided},
           },
       .input_tokens =
           {
               {verilog_tokentype::SymbolIdentifier, "funk"},
               {'(', "("},
               {verilog_tokentype::TK_StringLiteral, "\"arg\""},
               {')', ")"},
               {';', ";"},
           }},

      // funk(arg1, arg2);
      {.style = DefaultStyle,
       .uwline_indentation = 1,
       .expected_calculations =
           {
               {0, SpacingOptions::kUndecided},
               {0, SpacingOptions::kUndecided},
               {0, SpacingOptions::kUndecided},
               {0, SpacingOptions::kUndecided},
               {1, SpacingOptions::kUndecided},
               {0, SpacingOptions::kUndecided},
               {0, SpacingOptions::kUndecided},
           },
       .input_tokens =
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
      {.style = DefaultStyle,
       .uwline_indentation = 1,
       .expected_calculations =
           {
               {0, SpacingOptions::kUndecided},
               {1, SpacingOptions::kUndecided},
               {0, SpacingOptions::kUndecided},  // '('
               {0, SpacingOptions::kUndecided},  // '.'
               {0, SpacingOptions::kUndecided},  // "f1"
               {0, SpacingOptions::kUndecided},  // '('
               {0, SpacingOptions::kUndecided},  // "arg1"
               {0, SpacingOptions::kUndecided},  // ')'
               {0, SpacingOptions::kUndecided},  // ','
               {1, SpacingOptions::kUndecided},  // '.'
               {0, SpacingOptions::kUndecided},  // "f1"
               {0, SpacingOptions::kUndecided},  // '('
               {0, SpacingOptions::kUndecided},  // "arg1"
               {0, SpacingOptions::kUndecided},  // ')'
               {0, SpacingOptions::kUndecided},  // ')'
               {0, SpacingOptions::kUndecided},  // ';'
           },
       .input_tokens =
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
      {.style = DefaultStyle,
       .uwline_indentation = 1,
       .expected_calculations =
           {
               {0, SpacingOptions::kUndecided},
               {0, SpacingOptions::kUndecided},
               {0, SpacingOptions::kUndecided},
           },
       .input_tokens =
           {
               {verilog_tokentype::MacroIdentifier, "`ID"},
               {'.', "."},
               {verilog_tokentype::MacroIdentifier, "`ID"},
           }},

      // id.id
      {.style = DefaultStyle,
       .uwline_indentation = 1,
       .expected_calculations =
           {
               {0, SpacingOptions::kUndecided},
               {0, SpacingOptions::kUndecided},
               {0, SpacingOptions::kUndecided},
           },
       .input_tokens =
           {
               {verilog_tokentype::SymbolIdentifier, "id"},
               {'.', "."},
               {verilog_tokentype::SymbolIdentifier, "id"},
           }},

      // super.id
      {.style = DefaultStyle,
       .uwline_indentation = 1,
       .expected_calculations =
           {
               {0, SpacingOptions::kUndecided},
               {0, SpacingOptions::kUndecided},
               {0, SpacingOptions::kUndecided},
           },
       .input_tokens =
           {
               {verilog_tokentype::TK_super, "super"},
               {'.', "."},
               {verilog_tokentype::SymbolIdentifier, "id"},
           }},

      // this.id
      {.style = DefaultStyle,
       .uwline_indentation = 1,
       .expected_calculations =
           {
               {0, SpacingOptions::kUndecided},
               {0, SpacingOptions::kUndecided},
               {0, SpacingOptions::kUndecided},
           },
       .input_tokens =
           {
               {verilog_tokentype::TK_this, "this"},
               {'.', "."},
               {verilog_tokentype::SymbolIdentifier, "id"},
           }},

      // option.id
      {.style = DefaultStyle,
       .uwline_indentation = 1,
       .expected_calculations =
           {
               {0, SpacingOptions::kUndecided},
               {0, SpacingOptions::kUndecided},
               {0, SpacingOptions::kUndecided},
           },
       .input_tokens =
           {
               {verilog_tokentype::TK_option, "option"},
               {'.', "."},
               {verilog_tokentype::SymbolIdentifier, "id"},
           }},

      // `MACRO();
      {.style = DefaultStyle,
       .uwline_indentation = 1,
       .expected_calculations =
           {
               {0, SpacingOptions::kUndecided},
               {0, SpacingOptions::kUndecided},
               {0, SpacingOptions::kUndecided},
               {0, SpacingOptions::kUndecided},
           },
       .input_tokens =
           {
               {verilog_tokentype::MacroCallId, "`MACRO"},
               {'(', "("},
               {verilog_tokentype::MacroCallCloseToEndLine, ")"},
               {';', ";"},
           }},

      // `MACRO(x);
      {.style = DefaultStyle,
       .uwline_indentation = 1,
       .expected_calculations =
           {
               {0, SpacingOptions::kUndecided},
               {0, SpacingOptions::kUndecided},
               {0, SpacingOptions::kUndecided},
               {0, SpacingOptions::kUndecided},
               {0, SpacingOptions::kUndecided},
           },
       .input_tokens =
           {
               {verilog_tokentype::MacroCallId, "`MACRO"},
               {'(', "("},
               {verilog_tokentype::SymbolIdentifier, "x"},
               {verilog_tokentype::MacroCallCloseToEndLine, ")"},
               {';', ";"},
           }},

      // `MACRO(y, x);
      {.style = DefaultStyle,
       .uwline_indentation = 1,
       .expected_calculations =
           {
               {0, SpacingOptions::kUndecided},
               {0, SpacingOptions::kUndecided},
               {0, SpacingOptions::kUndecided},  // "y"
               {0, SpacingOptions::kUndecided},  // ','
               {1, SpacingOptions::kUndecided},  // "x"
               {0, SpacingOptions::kUndecided},
               {0, SpacingOptions::kUndecided},
           },
       .input_tokens =
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
      {.style = DefaultStyle,
       .uwline_indentation = 1,
       .expected_calculations =
           {
               {0, SpacingOptions::kUndecided},   // `define
               {1, SpacingOptions::kMustAppend},  // FOO
               {0, SpacingOptions::kMustAppend},  // "" (empty definition body)
               {0, SpacingOptions::kMustWrap},    // `define
               {1, SpacingOptions::kMustAppend},  // BAR
               {0, SpacingOptions::kMustAppend},  // "" (empty definition body)
           },
       .input_tokens =
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
      {.style = DefaultStyle,
       .uwline_indentation = 1,
       .expected_calculations =
           {
               {0, SpacingOptions::kUndecided},   // `define
               {1, SpacingOptions::kMustAppend},  // FOO
               {1, SpacingOptions::kMustAppend},  // 1
               {1, SpacingOptions::kMustWrap},    // `define
               {1, SpacingOptions::kMustAppend},  // BAR
               {1, SpacingOptions::kMustAppend},  // 2
           },
       .input_tokens =
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
      {.style = DefaultStyle,
       .uwline_indentation = 1,
       .expected_calculations =
           {
               {0, SpacingOptions::kUndecided},   // `define
               {1, SpacingOptions::kMustAppend},  // FOO
               {0, SpacingOptions::kMustAppend},  // (
               {0, SpacingOptions::kUndecided},   // )
               {0, SpacingOptions::kMustAppend},  // "" (empty definition body)

               {0, SpacingOptions::kMustWrap},    // `define
               {1, SpacingOptions::kMustAppend},  // BAR
               {0, SpacingOptions::kMustAppend},  // (
               {0, SpacingOptions::kUndecided},   // x
               {0, SpacingOptions::kUndecided},   // )
               {0, SpacingOptions::kMustAppend},  // "" (empty definition body)

               {0, SpacingOptions::kMustWrap},    // `define
               {1, SpacingOptions::kMustAppend},  // BAZ
               {0, SpacingOptions::kMustAppend},  // (
               {0, SpacingOptions::kUndecided},   // y
               {0, SpacingOptions::kUndecided},   // ,
               {1, SpacingOptions::kUndecided},   // z
               {0, SpacingOptions::kUndecided},   // )
               {0, SpacingOptions::kMustAppend},  // "" (empty definition body)
           },
       .input_tokens =
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
          .style = DefaultStyle,
          .uwline_indentation = 1,
          .expected_calculations =
              {
                  {0, SpacingOptions::kUndecided},   // `define
                  {1, SpacingOptions::kMustAppend},  // ADD
                  {0, SpacingOptions::kMustAppend},  // (
                  {0, SpacingOptions::kUndecided},   // y
                  {0, SpacingOptions::kUndecided},   // ,
                  {1, SpacingOptions::kUndecided},   // z
                  {0, SpacingOptions::kUndecided},   // )
                  {1, SpacingOptions::kMustAppend},  // "y+z"
              },
          .input_tokens =
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
      {.style = DefaultStyle,
       .uwline_indentation = 1,
       .expected_calculations =
           {
               {0, SpacingOptions::kUndecided},  // function
               {1, SpacingOptions::kUndecided},  // new
               {0, SpacingOptions::kUndecided},  // ;
           },
       .input_tokens =
           {
               {verilog_tokentype::TK_function, "function"},
               {verilog_tokentype::TK_new, "new"},
               {';', ";"},
           }},

      // function new();
      {.style = DefaultStyle,
       .uwline_indentation = 1,
       .expected_calculations =
           {
               {0, SpacingOptions::kUndecided},  // function
               {1, SpacingOptions::kUndecided},  // new
               {0, SpacingOptions::kUndecided},  // (
               {0, SpacingOptions::kUndecided},  // )
               {0, SpacingOptions::kUndecided},  // ;
           },
       .input_tokens =
           {
               {verilog_tokentype::TK_function, "function"},
               {verilog_tokentype::TK_new, "new"},
               {'(', "("},
               {')', ")"},
               {';', ";"},
           }},

      // end endfunction endclass (end* keywords)
      {.style = DefaultStyle,
       .uwline_indentation = 1,
       .expected_calculations =
           {
               {0, SpacingOptions::kUndecided},  // end
               {1, SpacingOptions::kMustWrap},   // end
               {1, SpacingOptions::kMustWrap},   // endfunction
               {1, SpacingOptions::kMustWrap},   // endclass
               {1, SpacingOptions::kMustWrap},   // endpackage
           },
       .input_tokens =
           {
               {verilog_tokentype::TK_end, "end"},
               {verilog_tokentype::TK_end, "end"},
               {verilog_tokentype::TK_endfunction, "endfunction"},
               {verilog_tokentype::TK_endclass, "endclass"},
               {verilog_tokentype::TK_endpackage, "endpackage"},
           }},
      {.style = DefaultStyle,
       .uwline_indentation = 1,
       .expected_calculations =
           {
               {0, SpacingOptions::kUndecided},  // end
               {1, SpacingOptions::kMustWrap},   // end
               {1, SpacingOptions::kMustWrap},   // endtask
               {1, SpacingOptions::kMustWrap},   // endmodule
           },
       .input_tokens =
           {
               {verilog_tokentype::TK_end, "end"},
               {verilog_tokentype::TK_end, "end"},
               {verilog_tokentype::TK_endtask, "endtask"},
               {verilog_tokentype::TK_endmodule, "endmodule"},
           }},

      // if (r == t) a.b(c);
      // else d.e(f);
      {.style = DefaultStyle,
       .uwline_indentation = 1,
       .expected_calculations =
           {
               {0, SpacingOptions::kUndecided},  // if
               {1, SpacingOptions::kUndecided},  // (
               {0, SpacingOptions::kUndecided},  // r
               {1, SpacingOptions::kUndecided},  // ==
               {1, SpacingOptions::kUndecided},  // t
               {0, SpacingOptions::kUndecided},  // )
               {1, SpacingOptions::kUndecided},  // a
               {0, SpacingOptions::kUndecided},  // .
               {0, SpacingOptions::kUndecided},  // b
               {0, SpacingOptions::kUndecided},  // (
               {0, SpacingOptions::kUndecided},  // c
               {0, SpacingOptions::kUndecided},  // )
               {0, SpacingOptions::kUndecided},  // ;

               {1, SpacingOptions::kMustWrap},   // else
               {1, SpacingOptions::kUndecided},  // d
               {0, SpacingOptions::kUndecided},  // .
               {0, SpacingOptions::kUndecided},  // e
               {0, SpacingOptions::kUndecided},  // (
               {0, SpacingOptions::kUndecided},  // f
               {0, SpacingOptions::kUndecided},  // )
               {0, SpacingOptions::kUndecided},  // ;
           },
       .input_tokens =
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
      {.style = DefaultStyle,
       .uwline_indentation = 1,
       .expected_calculations =
           {
               {0, SpacingOptions::kUndecided},  // if
               {1, SpacingOptions::kUndecided},  // (
               {0, SpacingOptions::kUndecided},  // r
               {1, SpacingOptions::kUndecided},  // ==
               {1, SpacingOptions::kUndecided},  // t
               {0, SpacingOptions::kUndecided},  // )

               {1, SpacingOptions::kMustAppend},  // begin
               {1, SpacingOptions::kUndecided},   // a
               {0, SpacingOptions::kUndecided},   // .
               {0, SpacingOptions::kUndecided},   // b
               {0, SpacingOptions::kUndecided},   // (
               {0, SpacingOptions::kUndecided},   // c
               {0, SpacingOptions::kUndecided},   // )
               {0, SpacingOptions::kUndecided},   // ;
               {1, SpacingOptions::kMustWrap},    // end

               {1, SpacingOptions::kMustAppend},  // else

               {1, SpacingOptions::kMustAppend},  // begin
               {1, SpacingOptions::kUndecided},   // d
               {0, SpacingOptions::kUndecided},   // .
               {0, SpacingOptions::kUndecided},   // e
               {0, SpacingOptions::kUndecided},   // (
               {0, SpacingOptions::kUndecided},   // f
               {0, SpacingOptions::kUndecided},   // )
               {0, SpacingOptions::kUndecided},   // ;
               {1, SpacingOptions::kMustWrap},    // end
           },
       .input_tokens =
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

      // wait ()
      {.style = DefaultStyle,
       .uwline_indentation = 1,
       .expected_calculations = {{0, SpacingOptions::kUndecided},
                                 {1, SpacingOptions::kUndecided}},
       .input_tokens = {{verilog_tokentype::TK_wait, "wait"}, {'(', "("}}},

      // various built-in function calls
      {.style = DefaultStyle,
       .uwline_indentation = 1,
       .expected_calculations = {{0, SpacingOptions::kUndecided},
                                 {0, SpacingOptions::kUndecided}},
       .input_tokens = {{verilog_tokentype::TK_and, "and"}, {'(', "("}}},
      {.style = DefaultStyle,
       .uwline_indentation = 1,
       .expected_calculations = {{0, SpacingOptions::kUndecided},
                                 {1, SpacingOptions::kUndecided}},
       .input_tokens = {{verilog_tokentype::TK_assert, "assert"}, {'(', "("}}},
      {.style = DefaultStyle,
       .uwline_indentation = 1,
       .expected_calculations = {{0, SpacingOptions::kUndecided},
                                 {1, SpacingOptions::kUndecided}},
       .input_tokens = {{verilog_tokentype::TK_assume, "assume"}, {'(', "("}}},
      {.style = DefaultStyle,
       .uwline_indentation = 1,
       .expected_calculations = {{0, SpacingOptions::kUndecided},
                                 {1, SpacingOptions::kUndecided}},
       .input_tokens = {{verilog_tokentype::TK_cover, "cover"}, {'(', "("}}},
      {.style = DefaultStyle,
       .uwline_indentation = 1,
       .expected_calculations = {{0, SpacingOptions::kUndecided},
                                 {1, SpacingOptions::kUndecided}},
       .input_tokens = {{verilog_tokentype::TK_expect, "expect"}, {'(', "("}}},
      {.style = DefaultStyle,
       .uwline_indentation = 1,
       .expected_calculations = {{0, SpacingOptions::kUndecided},
                                 {1, SpacingOptions::kUndecided}},
       .input_tokens = {{verilog_tokentype::TK_property, "property"},
                        {'(', "("}}},
      {.style = DefaultStyle,
       .uwline_indentation = 1,
       .expected_calculations = {{0, SpacingOptions::kUndecided},
                                 {1, SpacingOptions::kUndecided}},
       .input_tokens = {{verilog_tokentype::TK_sequence, "sequence"},
                        {'(', "("}}},
      {.style = DefaultStyle,
       .uwline_indentation = 1,
       .expected_calculations = {{0, SpacingOptions::kUndecided},
                                 {1, SpacingOptions::kUndecided}},
       .input_tokens = {{verilog_tokentype::TK_final, "final"}, {'(', "("}}},
      {.style = DefaultStyle,
       .uwline_indentation = 1,
       .expected_calculations = {{0, SpacingOptions::kUndecided},
                                 {0, SpacingOptions::kUndecided}},
       .input_tokens = {{verilog_tokentype::TK_find, "find"}, {'(', "("}}},
      {.style = DefaultStyle,
       .uwline_indentation = 1,
       .expected_calculations = {{0, SpacingOptions::kUndecided},
                                 {0, SpacingOptions::kUndecided}},
       .input_tokens = {{verilog_tokentype::TK_find_index, "find_index"},
                        {'(', "("}}},
      {.style = DefaultStyle,
       .uwline_indentation = 1,
       .expected_calculations = {{0, SpacingOptions::kUndecided},
                                 {0, SpacingOptions::kUndecided}},
       .input_tokens = {{verilog_tokentype::TK_find_first, "find_first"},
                        {'(', "("}}},
      {.style = DefaultStyle,
       .uwline_indentation = 1,
       .expected_calculations = {{0, SpacingOptions::kUndecided},
                                 {0, SpacingOptions::kUndecided}},
       .input_tokens = {{verilog_tokentype::TK_find_first_index,
                         "find_first_index"},
                        {'(', "("}}},
      {.style = DefaultStyle,
       .uwline_indentation = 1,
       .expected_calculations = {{0, SpacingOptions::kUndecided},
                                 {0, SpacingOptions::kUndecided}},
       .input_tokens = {{verilog_tokentype::TK_find_last, "find_last"},
                        {'(', "("}}},
      {.style = DefaultStyle,
       .uwline_indentation = 1,
       .expected_calculations = {{0, SpacingOptions::kUndecided},
                                 {0, SpacingOptions::kUndecided}},
       .input_tokens = {{verilog_tokentype::TK_find_last_index,
                         "find_last_index"},
                        {'(', "("}}},
      {.style = DefaultStyle,
       .uwline_indentation = 1,
       .expected_calculations = {{0, SpacingOptions::kUndecided},
                                 {0, SpacingOptions::kUndecided}},
       .input_tokens = {{verilog_tokentype::TK_min, "min"}, {'(', "("}}},
      {.style = DefaultStyle,
       .uwline_indentation = 1,
       .expected_calculations = {{0, SpacingOptions::kUndecided},
                                 {0, SpacingOptions::kUndecided}},
       .input_tokens = {{verilog_tokentype::TK_max, "max"}, {'(', "("}}},
      {.style = DefaultStyle,
       .uwline_indentation = 1,
       .expected_calculations = {{0, SpacingOptions::kUndecided},
                                 {0, SpacingOptions::kUndecided}},
       .input_tokens = {{verilog_tokentype::TK_or, "or"}, {'(', "("}}},
      {.style = DefaultStyle,
       .uwline_indentation = 1,
       .expected_calculations = {{0, SpacingOptions::kUndecided},
                                 {0, SpacingOptions::kUndecided}},
       .input_tokens = {{verilog_tokentype::TK_product, "product"},
                        {'(', "("}}},
      {.style = DefaultStyle,
       .uwline_indentation = 1,
       .expected_calculations = {{0, SpacingOptions::kUndecided},
                                 {0, SpacingOptions::kUndecided}},
       .input_tokens = {{verilog_tokentype::TK_randomize, "randomize"},
                        {'(', "("}}},
      {.style = DefaultStyle,
       .uwline_indentation = 1,
       .expected_calculations = {{0, SpacingOptions::kUndecided},
                                 {0, SpacingOptions::kUndecided}},
       .input_tokens = {{verilog_tokentype::TK_reverse, "reverse"},
                        {'(', "("}}},
      {.style = DefaultStyle,
       .uwline_indentation = 1,
       .expected_calculations = {{0, SpacingOptions::kUndecided},
                                 {0, SpacingOptions::kUndecided}},
       .input_tokens = {{verilog_tokentype::TK_rsort, "rsort"}, {'(', "("}}},
      {.style = DefaultStyle,
       .uwline_indentation = 1,
       .expected_calculations = {{0, SpacingOptions::kUndecided},
                                 {0, SpacingOptions::kUndecided}},
       .input_tokens = {{verilog_tokentype::TK_shuffle, "shuffle"},
                        {'(', "("}}},
      {.style = DefaultStyle,
       .uwline_indentation = 1,
       .expected_calculations = {{0, SpacingOptions::kUndecided},
                                 {0, SpacingOptions::kUndecided}},
       .input_tokens = {{verilog_tokentype::TK_sort, "sort"}, {'(', "("}}},
      {.style = DefaultStyle,
       .uwline_indentation = 1,
       .expected_calculations = {{0, SpacingOptions::kUndecided},
                                 {0, SpacingOptions::kUndecided}},
       .input_tokens = {{verilog_tokentype::TK_sum, "sum"}, {'(', "("}}},
      {.style = DefaultStyle,
       .uwline_indentation = 1,
       .expected_calculations = {{0, SpacingOptions::kUndecided},
                                 {0, SpacingOptions::kUndecided}},
       .input_tokens = {{verilog_tokentype::TK_unique, "unique"}, {'(', "("}}},
      {.style = DefaultStyle,
       .uwline_indentation = 1,
       .expected_calculations = {{0, SpacingOptions::kUndecided},
                                 {0, SpacingOptions::kUndecided}},
       .input_tokens = {{verilog_tokentype::TK_xor, "xor"}, {'(', "("}}},

      // escaped identifier
      // baz.\FOO .bar
      {.style = DefaultStyle,
       .uwline_indentation = 1,
       .expected_calculations =
           {
               {0, SpacingOptions::kUndecided},  // baz
               {0, SpacingOptions::kUndecided},  // .
               {0, SpacingOptions::kUndecided},  // \FOO
               {1, SpacingOptions::kUndecided},  // .
               {0, SpacingOptions::kUndecided},  // bar
           },
       .input_tokens =
           {
               {verilog_tokentype::SymbolIdentifier, "baz"},
               {'.', "."},
               {verilog_tokentype::EscapedIdentifier, "\\FOO"},
               {'.', "."},
               {verilog_tokentype::SymbolIdentifier, "bar"},
           }},

      // escaped identifier inside macro call
      // `BAR(\FOO )
      {.style = DefaultStyle,
       .uwline_indentation = 1,
       .expected_calculations =
           {
               {0, SpacingOptions::kUndecided},  // `BAR
               {0, SpacingOptions::kUndecided},  // (
               {0, SpacingOptions::kUndecided},  // \FOO
               {1, SpacingOptions::kUndecided},  // )
           },
       .input_tokens =
           {
               {verilog_tokentype::MacroCallId, "`BAR"},
               {'(', "("},
               {verilog_tokentype::EscapedIdentifier, "\\FOO"},
               {')', ")"},
           }},

      // import foo_pkg::symbol;
      {.style = DefaultStyle,
       .uwline_indentation = 1,
       .expected_calculations =
           {
               {0, SpacingOptions::kUndecided},  // import
               {1, SpacingOptions::kUndecided},  // foo_pkg
               {0, SpacingOptions::kUndecided},  // ::
               {0, SpacingOptions::kUndecided},  // symbol
               {0, SpacingOptions::kUndecided},  // ;
           },
       .input_tokens =
           {
               {verilog_tokentype::TK_import, "import"},
               {verilog_tokentype::SymbolIdentifier, "foo_pkg"},
               {verilog_tokentype::TK_SCOPE_RES, "::"},
               {verilog_tokentype::SymbolIdentifier, "symbol"},
               {';', ";"},
           }},

      // import foo_pkg::*;
      {.style = DefaultStyle,
       .uwline_indentation = 1,
       .expected_calculations =
           {
               {0, SpacingOptions::kUndecided},  // import
               {1, SpacingOptions::kUndecided},  // foo_pkg
               {0, SpacingOptions::kUndecided},  // ::
               {0, SpacingOptions::kUndecided},  // *
               {0, SpacingOptions::kUndecided},  // ;
           },
       .input_tokens =
           {
               {verilog_tokentype::TK_import, "import"},
               {verilog_tokentype::SymbolIdentifier, "foo_pkg"},
               {verilog_tokentype::TK_SCOPE_RES, "::"},
               {'*', "*"},
               {';', ";"},
           }},

      // #0; (delay, unitless integer)
      {.style = DefaultStyle,
       .uwline_indentation = 1,
       .expected_calculations =
           {
               {0, SpacingOptions::kUndecided},   // #
               {0, SpacingOptions::kMustAppend},  // 0
               {0, SpacingOptions::kUndecided},   // ;
           },
       .input_tokens =
           {
               {'#', "#"},
               {verilog_tokentype::TK_DecNumber, "0"},
               {';', ";"},
           }},

      // #0.5; (delay, real-value)
      {.style = DefaultStyle,
       .uwline_indentation = 1,
       .expected_calculations =
           {
               {0, SpacingOptions::kUndecided},   // #
               {0, SpacingOptions::kMustAppend},  // 0.5
               {0, SpacingOptions::kUndecided},   // ;
           },
       .input_tokens =
           {
               {'#', "#"},
               {verilog_tokentype::TK_RealTime, "0.5"},
               {';', ";"},
           }},

      // #0ns; (delay, time-literal)
      {.style = DefaultStyle,
       .uwline_indentation = 1,
       .expected_calculations =
           {
               {0, SpacingOptions::kUndecided},   // #
               {0, SpacingOptions::kMustAppend},  // 0ns
               {0, SpacingOptions::kMustAppend},  // ;
           },
       .input_tokens =
           {
               {'#', "#"},
               {verilog_tokentype::TK_TimeLiteral, "0ns"},
               {';', ";"},
           }},

      // #1step; (delay, 1step)
      {.style = DefaultStyle,
       .uwline_indentation = 1,
       .expected_calculations =
           {
               {0, SpacingOptions::kUndecided},   // #
               {0, SpacingOptions::kMustAppend},  // 1step
               {0, SpacingOptions::kUndecided},   // ;
           },
       .input_tokens =
           {
               {'#', "#"},
               {verilog_tokentype::TK_1step, "1step"},
               {';', ";"},
           }},

      // default: ;
      {.style = DefaultStyle,
       .uwline_indentation = 0,
       .expected_calculations =
           {
               {0, SpacingOptions::kUndecided},
               {0, SpacingOptions::kUndecided},
               {1, SpacingOptions::kUndecided},
           },
       .input_tokens =
           {
               {verilog_tokentype::TK_default, "default"},
               {':', ":"},
               {';', ";"},
           }},

      // foo = 1 << bar;
      {.style = DefaultStyle,
       .uwline_indentation = 0,
       .expected_calculations =
           {
               {0, SpacingOptions::kUndecided},
               {1, SpacingOptions::kUndecided},
               {1, SpacingOptions::kUndecided},
               {1, SpacingOptions::kUndecided},
               {1, SpacingOptions::kUndecided},
               {0, SpacingOptions::kUndecided},
           },
       .input_tokens =
           {
               {verilog_tokentype::SymbolIdentifier, "foo"},
               {'=', "="},
               {verilog_tokentype::TK_DecNumber, "1"},
               {verilog_tokentype::TK_LS, "<<"},
               {verilog_tokentype::SymbolIdentifier, "bar"},
               {';', ";"},
           }},

      // foo = bar << 1;
      {.style = DefaultStyle,
       .uwline_indentation = 0,
       .expected_calculations =
           {
               {0, SpacingOptions::kUndecided},
               {1, SpacingOptions::kUndecided},
               {1, SpacingOptions::kUndecided},
               {1, SpacingOptions::kUndecided},
               {1, SpacingOptions::kUndecided},
               {0, SpacingOptions::kUndecided},
           },
       .input_tokens =
           {
               {verilog_tokentype::SymbolIdentifier, "foo"},
               {'=', "="},
               {verilog_tokentype::SymbolIdentifier, "bar"},
               {verilog_tokentype::TK_LS, "<<"},
               {verilog_tokentype::TK_DecNumber, "1"},
               {';', ";"},
           }},

      // foo = `BAR << 1;
      {.style = DefaultStyle,
       .uwline_indentation = 0,
       .expected_calculations =
           {
               {0, SpacingOptions::kUndecided},
               {1, SpacingOptions::kUndecided},
               {1, SpacingOptions::kUndecided},
               {1, SpacingOptions::kUndecided},
               {1, SpacingOptions::kUndecided},
               {0, SpacingOptions::kUndecided},
           },
       .input_tokens =
           {
               {verilog_tokentype::SymbolIdentifier, "foo"},
               {'=', "="},
               {verilog_tokentype::MacroIdentifier, "`BAR"},
               {verilog_tokentype::TK_LS, "<<"},
               {verilog_tokentype::TK_DecNumber, "1"},
               {';', ";"},
           }},

      // foo = 1 << `BAR;
      {.style = DefaultStyle,
       .uwline_indentation = 0,
       .expected_calculations =
           {
               {0, SpacingOptions::kUndecided},
               {1, SpacingOptions::kUndecided},
               {1, SpacingOptions::kUndecided},
               {1, SpacingOptions::kUndecided},
               {1, SpacingOptions::kUndecided},
               {0, SpacingOptions::kUndecided},
           },
       .input_tokens =
           {
               {verilog_tokentype::SymbolIdentifier, "foo"},
               {'=', "="},
               {verilog_tokentype::TK_DecNumber, "1"},
               {verilog_tokentype::TK_LS, "<<"},
               {verilog_tokentype::MacroIdentifier, "`BAR"},
               {';', ";"},
           }},

      // foo = 1 >> bar;
      {.style = DefaultStyle,
       .uwline_indentation = 0,
       .expected_calculations =
           {
               {0, SpacingOptions::kUndecided},
               {1, SpacingOptions::kUndecided},
               {1, SpacingOptions::kUndecided},
               {1, SpacingOptions::kUndecided},
               {1, SpacingOptions::kUndecided},
               {0, SpacingOptions::kUndecided},
           },
       .input_tokens =
           {
               {verilog_tokentype::SymbolIdentifier, "foo"},
               {'=', "="},
               {verilog_tokentype::TK_DecNumber, "1"},
               {verilog_tokentype::TK_RS, ">>"},
               {verilog_tokentype::SymbolIdentifier, "bar"},
               {';', ";"},
           }},

      // foo = bar >> 1;
      {.style = DefaultStyle,
       .uwline_indentation = 0,
       .expected_calculations =
           {
               {0, SpacingOptions::kUndecided},
               {1, SpacingOptions::kUndecided},
               {1, SpacingOptions::kUndecided},
               {1, SpacingOptions::kUndecided},
               {1, SpacingOptions::kUndecided},
               {0, SpacingOptions::kUndecided},
           },
       .input_tokens =
           {
               {verilog_tokentype::SymbolIdentifier, "foo"},
               {'=', "="},
               {verilog_tokentype::SymbolIdentifier, "bar"},
               {verilog_tokentype::TK_RS, ">>"},
               {verilog_tokentype::TK_DecNumber, "1"},
               {';', ";"},
           }},

      // foo = `BAR >> 1;
      {.style = DefaultStyle,
       .uwline_indentation = 0,
       .expected_calculations =
           {
               {0, SpacingOptions::kUndecided},
               {1, SpacingOptions::kUndecided},
               {1, SpacingOptions::kUndecided},
               {1, SpacingOptions::kUndecided},
               {1, SpacingOptions::kUndecided},
               {0, SpacingOptions::kUndecided},
           },
       .input_tokens =
           {
               {verilog_tokentype::SymbolIdentifier, "foo"},
               {'=', "="},
               {verilog_tokentype::MacroIdentifier, "`BAR"},
               {verilog_tokentype::TK_RS, ">>"},
               {verilog_tokentype::TK_DecNumber, "1"},
               {';', ";"},
           }},

      // foo = 1 >> `BAR;
      {.style = DefaultStyle,
       .uwline_indentation = 0,
       .expected_calculations =
           {
               {0, SpacingOptions::kUndecided},
               {1, SpacingOptions::kUndecided},
               {1, SpacingOptions::kUndecided},
               {1, SpacingOptions::kUndecided},
               {1, SpacingOptions::kUndecided},
               {0, SpacingOptions::kUndecided},
           },
       .input_tokens =
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
  for (const auto &test_case : kTestCases) {
    verible::UnwrappedLineMemoryHandler handler;
    handler.CreateTokenInfos(test_case.input_tokens);
    verible::UnwrappedLine unwrapped_line(test_case.uwline_indentation,
                                          handler.GetPreFormatTokensBegin());
    handler.AddFormatTokens(&unwrapped_line);
    // The format_token_enums are not yet set by AddFormatTokens.
    for (auto &ftoken : handler.pre_format_tokens_) {
      ftoken.format_token_enum =
          GetFormatTokenType(verilog_tokentype(ftoken.TokenEnum()));
    }

    auto &ftokens_range = handler.pre_format_tokens_;
    // nullptr buffer_start is needed because token text do not belong to the
    // same contiguous string buffer.
    // Pass an empty/fake tree, which will not be used for testing
    // context-insensitive annotation rules.
    // Since we're using the joined string buffer inside handler,
    // we need to pass an EOF token that points to the end of that buffer.
    AnnotateFormattingInformation(test_case.style,
                                  verible::string_view_null_iterator(), nullptr,
                                  handler.EOFToken(), &ftokens_range);
    EXPECT_TRUE(CorrectExpectedFormatTokens(test_case.expected_calculations,
                                            ftokens_range))
        << "mismatch at test case " << test_index << ", tokens " << test_case;
    ++test_index;
  }
}  // NOLINT(readability/fn_size)

// These test cases support the use of syntactic context, but it is not
// required to specify context.
TEST(TokenAnnotatorTest, AnnotateFormattingWithContextTest) {
  static const AnnotateWithContextTestCase kTestCases[] = {
      {
          .style = DefaultStyle,
          .left_token = {'=', "="},
          .right_token = {verilog_tokentype::TK_StringLiteral, "\"hello\""},
          .left_context = {},   // any context
          .right_context = {},  // any context
          .expected_annotation = {1, SpacingOptions::kUndecided},
      },
      {
          .style = DefaultStyle,
          .left_token = {'=', "="},
          .right_token = {verilog_tokentype::TK_EvalStringLiteral,
                          "`\"hello`\""},
          .left_context = {},   // any context
          .right_context = {},  // any context
          .expected_annotation = {1, SpacingOptions::kUndecided},
      },
      // Test cases covering right token as a preprocessor directive:
      {
          .style = DefaultStyle,
          .left_token = {verilog_tokentype::TK_EOL_COMMENT, "//comment1"},
          .right_token = {verilog_tokentype::PP_ifdef, "`ifdef"},
          .left_context = {},   // any context
          .right_context = {},  // any context
          .expected_annotation = {1, SpacingOptions::kMustWrap},
      },
      // Compiler directives (DR_*) should also force a wrap when on the right.
      {
          .style = DefaultStyle,
          .left_token = {verilog_tokentype::SymbolIdentifier, "id"},
          .right_token = {verilog_tokentype::DR_timescale, "`timescale"},
          .left_context = {},   // any context
          .right_context = {},  // any context
          .expected_annotation = {1, SpacingOptions::kMustWrap},
      },
      {
          .style = DefaultStyle,
          .left_token = {';', ";"},
          .right_token = {verilog_tokentype::DR_default_nettype,
                          "`default_nettype"},
          .left_context = {},   // any context
          .right_context = {},  // any context
          .expected_annotation = {1, SpacingOptions::kMustWrap},
      },
      {
          .style = DefaultStyle,
          .left_token = {verilog_tokentype::TK_EOL_COMMENT, "//comment1"},
          .right_token = {verilog_tokentype::DR_resetall, "`resetall"},
          .left_context = {},   // any context
          .right_context = {},  // any context
          .expected_annotation = {1, SpacingOptions::kMustWrap},
      },
      {
          .style = DefaultStyle,
          .left_token = {verilog_tokentype::SymbolIdentifier, "id"},
          .right_token = {verilog_tokentype::PP_ifdef, "`ifdef"},
          .left_context = {},   // any context
          .right_context = {},  // any context
          .expected_annotation = {1, SpacingOptions::kMustWrap},
      },
      {
          .style = DefaultStyle,
          .left_token = {';', ";"},
          .right_token = {verilog_tokentype::PP_ifdef, "`ifdef"},
          .left_context = {},   // any context
          .right_context = {},  // any context
          .expected_annotation = {1, SpacingOptions::kMustWrap},
      },
      {
          .style = DefaultStyle,
          .left_token = {verilog_tokentype::PP_else, "`else"},
          .right_token = {verilog_tokentype::PP_ifdef, "`ifdef"},
          .left_context = {},   // any context
          .right_context = {},  // any context
          .expected_annotation = {1, SpacingOptions::kMustWrap},
      },
      {
          .style = DefaultStyle,
          .left_token = {verilog_tokentype::PP_endif, "`endif"},
          .right_token = {verilog_tokentype::PP_ifdef, "`ifdef"},
          .left_context = {},   // any context
          .right_context = {},  // any context
          .expected_annotation = {1, SpacingOptions::kMustWrap},
      },
      {
          .style = DefaultStyle,
          .left_token = {verilog_tokentype::TK_EOL_COMMENT, "//comment1"},
          .right_token = {verilog_tokentype::PP_ifndef, "`ifndef"},
          .left_context = {},   // any context
          .right_context = {},  // any context
          .expected_annotation = {1, SpacingOptions::kMustWrap},
      },
      {
          .style = DefaultStyle,
          .left_token = {verilog_tokentype::SymbolIdentifier, "id"},
          .right_token = {verilog_tokentype::PP_ifndef, "`ifndef"},
          .left_context = {},   // any context
          .right_context = {},  // any context
          .expected_annotation = {1, SpacingOptions::kMustWrap},
      },
      {
          .style = DefaultStyle,
          .left_token = {';', ";"},
          .right_token = {verilog_tokentype::PP_ifndef, "`ifndef"},
          .left_context = {},   // any context
          .right_context = {},  // any context
          .expected_annotation = {1, SpacingOptions::kMustWrap},
      },
      {
          .style = DefaultStyle,
          .left_token = {SymbolIdentifier, "ID"},
          .right_token = {verilog_tokentype::PP_else, "`else"},
          .left_context = {},   // any context
          .right_context = {},  // any context
          .expected_annotation = {1, SpacingOptions::kMustWrap},
      },
      {
          .style = DefaultStyle,
          .left_token = {';', ";"},
          .right_token = {verilog_tokentype::PP_else, "`else"},
          .left_context = {},   // any context
          .right_context = {},  // any context
          .expected_annotation = {1, SpacingOptions::kMustWrap},
      },
      {
          .style = DefaultStyle,
          .left_token = {verilog_tokentype::PP_endif, "`endif"},
          .right_token = {verilog_tokentype::PP_else, "`else"},
          .left_context = {},   // any context
          .right_context = {},  // any context
          .expected_annotation = {1, SpacingOptions::kMustWrap},
      },
      {
          .style = DefaultStyle,
          .left_token = {verilog_tokentype::PP_include, "`include"},
          .right_token = {TK_StringLiteral, "\"lost/file.svh\""},
          .left_context = {},   // any context
          .right_context = {},  // any context
          .expected_annotation =
              {1, SpacingOptions::kUndecided}, /* or MustAppend? */
      },
      {
          .style = DefaultStyle,
          .left_token = {verilog_tokentype::PP_include, "`include"},
          .right_token = {TK_EvalStringLiteral, "`\"lost/file.svh`\""},
          .left_context = {},   // any context
          .right_context = {},  // any context
          .expected_annotation =
              {1, SpacingOptions::kUndecided}, /* or MustAppend? */
      },
      {
          .style = DefaultStyle,
          .left_token = {TK_StringLiteral, "\"lost/file.svh\""},
          .right_token = {verilog_tokentype::PP_include, "`include"},
          .left_context = {},   // any context
          .right_context = {},  // any context
          .expected_annotation = {1, SpacingOptions::kMustWrap},
      },
      {
          .style = DefaultStyle,
          .left_token = {verilog_tokentype::PP_else, "`else"},
          .right_token = {verilog_tokentype::PP_include, "`include"},
          .left_context = {},   // any context
          .right_context = {},  // any context
          .expected_annotation = {1, SpacingOptions::kMustWrap},
      },
      {
          .style = DefaultStyle,
          .left_token = {';', ";"},
          .right_token = {verilog_tokentype::PP_include, "`include"},
          .left_context = {},   // any context
          .right_context = {},  // any context
          .expected_annotation = {1, SpacingOptions::kMustWrap},
      },
      {
          .style = DefaultStyle,
          .left_token = {SymbolIdentifier, "ID"},
          .right_token = {verilog_tokentype::PP_include, "`include"},
          .left_context = {},   // any context
          .right_context = {},  // any context
          .expected_annotation = {1, SpacingOptions::kMustWrap},
      },
      {
          .style = DefaultStyle,
          .left_token = {TK_StringLiteral, "\"lost/file.svh\""},
          .right_token = {verilog_tokentype::PP_define, "`define"},
          .left_context = {},   // any context
          .right_context = {},  // any context
          .expected_annotation = {1, SpacingOptions::kMustWrap},
      },
      {
          .style = DefaultStyle,
          .left_token = {verilog_tokentype::PP_else, "`else"},
          .right_token = {verilog_tokentype::PP_define, "`define"},
          .left_context = {},   // any context
          .right_context = {},  // any context
          .expected_annotation = {1, SpacingOptions::kMustWrap},
      },
      {
          .style = DefaultStyle,
          .left_token = {';', ";"},
          .right_token = {verilog_tokentype::PP_define, "`define"},
          .left_context = {},   // any context
          .right_context = {},  // any context
          .expected_annotation = {1, SpacingOptions::kMustWrap},
      },
      {
          .style = DefaultStyle,
          .left_token = {SymbolIdentifier, "ID"},
          .right_token = {verilog_tokentype::PP_define, "`define"},
          .left_context = {},   // any context
          .right_context = {},  // any context
          .expected_annotation = {1, SpacingOptions::kMustWrap},
      },
      {
          .style = DefaultStyle,
          .left_token = {verilog_tokentype::PP_define, "`define"},
          .right_token = {SymbolIdentifier, "ID"},
          .left_context = {},   // any context
          .right_context = {},  // any context
          .expected_annotation = {1, SpacingOptions::kMustAppend},
      },
      {
          .style = DefaultStyle,
          .left_token = {TK_StringLiteral, "\"lost/file.svh\""},
          .right_token = {verilog_tokentype::PP_undef, "`undef"},
          .left_context = {},   // any context
          .right_context = {},  // any context
          .expected_annotation = {1, SpacingOptions::kMustWrap},
      },
      {
          .style = DefaultStyle,
          .left_token = {verilog_tokentype::PP_else, "`else"},
          .right_token = {verilog_tokentype::PP_undef, "`undef"},
          .left_context = {},   // any context
          .right_context = {},  // any context
          .expected_annotation = {1, SpacingOptions::kMustWrap},
      },
      {
          .style = DefaultStyle,
          .left_token = {';', ";"},
          .right_token = {verilog_tokentype::PP_undef, "`undef"},
          .left_context = {},   // any context
          .right_context = {},  // any context
          .expected_annotation = {1, SpacingOptions::kMustWrap},
      },
      {
          .style = DefaultStyle,
          .left_token = {SymbolIdentifier, "ID"},
          .right_token = {verilog_tokentype::PP_undef, "`undef"},
          .left_context = {},   // any context
          .right_context = {},  // any context
          .expected_annotation = {1, SpacingOptions::kMustWrap},
      },
      {
          .style = DefaultStyle,
          .left_token = {verilog_tokentype::TK_endfunction, "endfunction"},
          .right_token = {verilog_tokentype::PP_undef, "`undef"},
          .left_context = {},   // any context
          .right_context = {},  // any context
          .expected_annotation = {1, SpacingOptions::kMustWrap},
      },
      {
          .style = DefaultStyle,
          .left_token = {verilog_tokentype::TK_end, "end"},
          .right_token = {verilog_tokentype::PP_undef, "`undef"},
          .left_context = {},   // any context
          .right_context = {},  // any context
          .expected_annotation = {1, SpacingOptions::kMustWrap},
      },
      {
          .style = DefaultStyle,
          .left_token = {verilog_tokentype::MacroCallCloseToEndLine, ")"},
          .right_token = {verilog_tokentype::PP_undef, "`undef"},
          .left_context = {},   // any context
          .right_context = {},  // any context
          .expected_annotation = {1, SpacingOptions::kMustWrap},
      },

      // macro definitions
      {
          .style = DefaultStyle,
          .left_token = {verilog_tokentype::PP_Identifier, "FOO"},
          .right_token = {verilog_tokentype::PP_define_body, ""}, /* empty */
          .left_context = {},                                     // any context
          .right_context = {},                                    // any context
          .expected_annotation = {0, SpacingOptions::kMustAppend},
      },
      {
          .style = DefaultStyle,
          .left_token = {verilog_tokentype::PP_Identifier, "FOO"},
          .right_token = {verilog_tokentype::PP_define_body, "bar"},
          .left_context = {},   // any context
          .right_context = {},  // any context
          .expected_annotation = {1, SpacingOptions::kMustAppend},
      },
      {
          .style = DefaultStyle,
          .left_token = {verilog_tokentype::PP_Identifier, "BAR"},
          .right_token = {verilog_tokentype::PP_define_body, "13"},
          .left_context = {},   // any context
          .right_context = {},  // any context
          .expected_annotation = {1, SpacingOptions::kMustAppend},
      },
      {
          .style = DefaultStyle,
          .left_token = {verilog_tokentype::PP_Identifier, "BAR"},
          .right_token = {verilog_tokentype::PP_define_body, "\\\n  bar"},
          .left_context = {},   // any context
          .right_context = {},  // any context
          .expected_annotation = {1, SpacingOptions::kMustAppend},
      },
      {
          .style = DefaultStyle,
          .left_token = {verilog_tokentype::PP_Identifier, "BAR"},
          .right_token = {verilog_tokentype::PP_define_body,
                          "\\\n  bar \\\n  + foo\n"},
          .left_context = {},   // any context
          .right_context = {},  // any context
          .expected_annotation = {1, SpacingOptions::kPreserve},
      },
      {
          .style = DefaultStyle,
          .left_token = {')', ")"},
          .right_token = {verilog_tokentype::PP_define_body, ""}, /* empty */
          .left_context = {},                                     // any context
          .right_context = {},                                    // any context
          .expected_annotation = {0, SpacingOptions::kMustAppend},
      },
      {
          .style = DefaultStyle,
          .left_token = {')', ")"},
          .right_token = {verilog_tokentype::PP_define_body, "bar"},
          .left_context = {},   // any context
          .right_context = {},  // any context
          .expected_annotation = {1, SpacingOptions::kMustAppend},
      },
      {
          .style = DefaultStyle,
          .left_token = {')', ")"},
          .right_token = {verilog_tokentype::PP_define_body, "13"},
          .left_context = {},   // any context
          .right_context = {},  // any context
          .expected_annotation = {1, SpacingOptions::kMustAppend},
      },
      {
          .style = DefaultStyle,
          .left_token = {')', ")"},
          .right_token = {verilog_tokentype::PP_define_body, "\\\n  bar"},
          .left_context = {},   // any context
          .right_context = {},  // any context
          .expected_annotation = {1, SpacingOptions::kMustAppend},
      },
      {
          // e.g. if (x) { ... } (in constraints)
          .style = DefaultStyle,
          .left_token = {')', ")"},
          .right_token = {'{', "{"},
          .left_context = {},   // any context
          .right_context = {},  // any context
          .expected_annotation = {1, SpacingOptions::kUndecided},
      },

      // right token = MacroCallId or MacroIdentifier
      {
          .style = DefaultStyle,
          .left_token = {SymbolIdentifier, "ID"},
          .right_token = {verilog_tokentype::MacroCallId, "`uvm_foo_macro"},
          .left_context = {},   // any context
          .right_context = {},  // any context
          .expected_annotation = {1, SpacingOptions::kUndecided},
      },
      {
          .style = DefaultStyle,
          .left_token = {SymbolIdentifier, "ID"},
          .right_token = {verilog_tokentype::MacroIdentifier, "`uvm_foo_id"},
          .left_context = {},   // any context
          .right_context = {},  // any context
          .expected_annotation = {1, SpacingOptions::kUndecided},
      },
      {
          .style = DefaultStyle,
          .left_token = {';', ";"},
          .right_token = {verilog_tokentype::MacroCallId, "`uvm_foo_macro"},
          .left_context = {},   // any context
          .right_context = {},  // any context
          .expected_annotation = {1, SpacingOptions::kUndecided},
      },
      {
          .style = DefaultStyle,
          .left_token = {';', ";"},
          .right_token = {verilog_tokentype::MacroIdentifier, "`uvm_foo_id"},
          .left_context = {},   // any context
          .right_context = {},  // any context
          .expected_annotation = {1, SpacingOptions::kUndecided},
      },
      {
          .style = DefaultStyle,
          .left_token = {TK_EOL_COMMENT, "//comment"},
          .right_token = {verilog_tokentype::MacroCallId, "`uvm_foo_macro"},
          .left_context = {},   // any context
          .right_context = {},  // any context
          .expected_annotation = {1, SpacingOptions::kMustWrap},
      },
      {
          .style = DefaultStyle,
          .left_token = {TK_EOL_COMMENT, "//comment"},
          .right_token = {verilog_tokentype::MacroIdentifier, "`uvm_foo_id"},
          .left_context = {},   // any context
          .right_context = {},  // any context
          .expected_annotation = {1, SpacingOptions::kMustWrap},
      },
      {
          .style = DefaultStyle,
          .left_token = {PP_else, "`else"},
          .right_token = {verilog_tokentype::MacroCallId, "`uvm_foo_macro"},
          .left_context = {},   // any context
          .right_context = {},  // any context
          .expected_annotation = {1, SpacingOptions::kMustWrap},
      },
      {
          .style = DefaultStyle,
          .left_token = {PP_else, "`else"},
          .right_token = {verilog_tokentype::MacroIdentifier, "`uvm_foo_id"},
          .left_context = {},   // any context
          .right_context = {},  // any context
          .expected_annotation = {1, SpacingOptions::kMustWrap},
      },
      {
          .style = DefaultStyle,
          .left_token = {PP_endif, "`endif"},
          .right_token = {verilog_tokentype::MacroCallId, "`uvm_foo_macro"},
          .left_context = {},   // any context
          .right_context = {},  // any context
          .expected_annotation = {1, SpacingOptions::kMustWrap},
      },
      {
          .style = DefaultStyle,
          .left_token = {PP_endif, "`endif"},
          .right_token = {verilog_tokentype::MacroIdentifier, "`uvm_foo_id"},
          .left_context = {},   // any context
          .right_context = {},  // any context
          .expected_annotation = {1, SpacingOptions::kMustWrap},
      },
      {
          .style = DefaultStyle,
          .left_token = {verilog_tokentype::MacroCallId, "`uvm_foo_macro"},
          .right_token = {verilog_tokentype::MacroCallId, "`uvm_foo_macro"},
          .left_context = {},   // any context
          .right_context = {},  // any context
          .expected_annotation = {1, SpacingOptions::kUndecided},
      },
      {
          .style = DefaultStyle,
          .left_token = {verilog_tokentype::MacroCallId, "`uvm_foo_macro"},
          .right_token = {verilog_tokentype::MacroIdentifier, "`uvm_foo_id"},
          .left_context = {},   // any context
          .right_context = {},  // any context
          .expected_annotation = {1, SpacingOptions::kUndecided},
      },
      {
          .style = DefaultStyle,
          .left_token = {verilog_tokentype::MacroIdentifier, "`uvm_foo_id"},
          .right_token = {verilog_tokentype::MacroCallId, "`uvm_foo_macro"},
          .left_context = {},   // any context
          .right_context = {},  // any context
          .expected_annotation = {1, SpacingOptions::kUndecided},
      },
      {
          .style = DefaultStyle,
          .left_token = {verilog_tokentype::MacroIdentifier, "`uvm_foo_id"},
          .right_token = {verilog_tokentype::MacroIdentifier, "`uvm_foo_id"},
          .left_context = {},   // any context
          .right_context = {},  // any context
          .expected_annotation = {1, SpacingOptions::kUndecided},
      },
      {
          .style = DefaultStyle,
          .left_token = {')', ")"},
          .right_token = {verilog_tokentype::MacroCallId, "`uvm_foo_macro"},
          .left_context = {},   // any context
          .right_context = {},  // any context
          .expected_annotation = {1, SpacingOptions::kUndecided},
      },
      {
          .style = DefaultStyle,
          .left_token = {')', ")"},
          .right_token = {verilog_tokentype::MacroIdentifier, "`uvm_foo_id"},
          .left_context = {},   // any context
          .right_context = {},  // any context
          .expected_annotation = {1, SpacingOptions::kUndecided},
      },
      {
          .style = DefaultStyle,
          .left_token = {verilog_tokentype::MacroCallCloseToEndLine, ")"},
          .right_token = {verilog_tokentype::MacroCallId, "`uvm_foo_macro"},
          .left_context = {},   // any context
          .right_context = {},  // any context
          .expected_annotation = {1, SpacingOptions::kMustWrap},
      },
      {
          .style = DefaultStyle,
          .left_token = {verilog_tokentype::MacroCallCloseToEndLine, ")"},
          .right_token = {verilog_tokentype::MacroIdentifier, "`uvm_foo_id"},
          .left_context = {},   // any context
          .right_context = {},  // any context
          .expected_annotation = {1, SpacingOptions::kMustWrap},
      },
      {
          .style = DefaultStyle,
          .left_token = {verilog_tokentype::MacroCallCloseToEndLine, ")"},
          .right_token = {';', ";"},
          .left_context = {},   // any context
          .right_context = {},  // any context
          .expected_annotation = {0, SpacingOptions::kUndecided},
      },

      {
          // single-line macro arguments are allowed to move around
          .style = DefaultStyle,
          .left_token = {',', ","},
          .right_token = {verilog_tokentype::MacroArg, "abcde"},
          .left_context = {},   // any context
          .right_context = {},  // any context
          .expected_annotation = {1, SpacingOptions::kUndecided},
      },
      {
          // multi-line macro arguments (unlexed) should start own line
          .style = DefaultStyle,
          .left_token = {',', ","},
          .right_token = {verilog_tokentype::MacroArg, "a;\nb;"},  // multi-line
          .left_context = {},   // any context
          .right_context = {},  // any context
          .expected_annotation = {1, SpacingOptions::kMustWrap},
      },

      // Without context, default is to treat '-' as binary.
      {
          .style = DefaultStyle,
          .left_token = {'-', "-"},  // left token
          .right_token = {verilog_tokentype::TK_DecNumber,
                          "42"},  // right token
          .left_context = {},     // context
          .right_context = {},    // context
          .expected_annotation = {1, SpacingOptions::kUndecided},
      },
      {
          .style = DefaultStyle,
          .left_token = {'-', "-"},
          .right_token = {verilog_tokentype::TK_DecNumber, "42"},
          .left_context = {},  // context
          .right_context = {NodeEnum::kBinaryExpression},
          .expected_annotation = {1, SpacingOptions::kUndecided},
      },

      // Handle '-' as a unary prefix expression.
      {
          .style = DefaultStyle,
          .left_token = {'-', "-"},  // left token
          .right_token = {verilog_tokentype::TK_DecNumber,
                          "42"},                                // right token
          .left_context = {},                                   // context
          .right_context = {NodeEnum::kUnaryPrefixExpression},  // context
          .expected_annotation = {0, SpacingOptions::kMustAppend},
      },
      {
          .style = DefaultStyle,
          .left_token = {'-', "-"},
          .right_token = {verilog_tokentype::SymbolIdentifier, "xyz"},
          .left_context = {},  // context
          .right_context = {NodeEnum::kUnaryPrefixExpression},
          .expected_annotation = {0, SpacingOptions::kMustAppend},
      },
      {
          .style = DefaultStyle,
          .left_token = {'-', "-"},
          .right_token = {'(', "("},
          .left_context = {},  // context
          .right_context = {NodeEnum::kUnaryPrefixExpression},
          .expected_annotation = {0, SpacingOptions::kMustAppend},
      },
      {
          .style = DefaultStyle,
          .left_token = {'-', "-"},
          .right_token = {verilog_tokentype::MacroIdItem, "`FOO"},
          .left_context = {},  // context
          .right_context = {NodeEnum::kUnaryPrefixExpression},
          .expected_annotation = {0, SpacingOptions::kMustAppend},
      },

      // Handle '&' as binary
      {
          .style = DefaultStyle,
          .left_token = {'&', "&"},
          .right_token = {'~', "~"},
          .left_context = {},   // unspecified context
          .right_context = {},  // unspecified context
          .expected_annotation = {1, SpacingOptions::kUndecided},
      },

      // Handle '&' as unary
      {
          .style = DefaultStyle,
          .left_token = {'&', "&"},
          .right_token = {verilog_tokentype::TK_DecNumber, "42"},
          .left_context = {/* any context */},
          .right_context = {NodeEnum::kUnaryPrefixExpression},
          .expected_annotation = {0, SpacingOptions::kMustAppend},
      },
      {
          .style = DefaultStyle,
          .left_token = {'&', "&"},
          .right_token = {verilog_tokentype::SymbolIdentifier, "foo"},
          .left_context = {/* any context */},
          .right_context = {NodeEnum::kUnaryPrefixExpression},
          .expected_annotation = {0, SpacingOptions::kMustAppend},
      },
      {
          .style = DefaultStyle,
          .left_token = {'&', "&"},
          .right_token = {'(', "("},
          .left_context = {/* any context */},
          .right_context = {NodeEnum::kUnaryPrefixExpression},
          .expected_annotation = {0, SpacingOptions::kMustAppend},
      },
      {
          .style = DefaultStyle,
          .left_token = {'&', "&"},
          .right_token = {'{', "{"},
          .left_context = {/* any context */},
          .right_context = {NodeEnum::kUnaryPrefixExpression},
          .expected_annotation = {0, SpacingOptions::kMustAppend},
      },

      // Handle '|' as binary
      {
          .style = DefaultStyle,
          .left_token = {'|', "|"},
          .right_token = {'~', "~"},
          .left_context = {/* any context */},
          .right_context = {/* any context */},
          .expected_annotation = {1, SpacingOptions::kUndecided},
      },

      // Handle '|' as unary
      {
          .style = DefaultStyle,
          .left_token = {'|', "|"},
          .right_token = {verilog_tokentype::TK_DecNumber, "42"},
          .left_context = {/* any context */},
          .right_context = {NodeEnum::kUnaryPrefixExpression},
          .expected_annotation = {0, SpacingOptions::kMustAppend},
      },
      {
          .style = DefaultStyle,
          .left_token = {'|', "|"},
          .right_token = {verilog_tokentype::SymbolIdentifier, "foo"},
          .left_context = {/* any context */},
          .right_context = {NodeEnum::kUnaryPrefixExpression},
          .expected_annotation = {0, SpacingOptions::kMustAppend},
      },
      {
          .style = DefaultStyle,
          .left_token = {'|', "|"},
          .right_token = {'(', "("},
          .left_context = {/* any context */},
          .right_context = {NodeEnum::kUnaryPrefixExpression},
          .expected_annotation = {0, SpacingOptions::kMustAppend},
      },
      {
          .style = DefaultStyle,
          .left_token = {'|', "|"},
          .right_token = {'{', "{"},
          .left_context = {/* any context */},
          .right_context = {NodeEnum::kUnaryPrefixExpression},
          .expected_annotation = {0, SpacingOptions::kMustAppend},
      },

      // Handle '^' as binary
      {
          .style = DefaultStyle,
          .left_token = {'^', "^"},
          .right_token = {'~', "~"},
          .left_context = {/* any context */},
          .right_context = {/* any context */},
          .expected_annotation = {1, SpacingOptions::kUndecided},
      },

      // Handle '^' as unary
      {
          .style = DefaultStyle,
          .left_token = {'^', "^"},
          .right_token = {verilog_tokentype::TK_DecNumber, "42"},
          .left_context = {/* any context */},
          .right_context = {NodeEnum::kUnaryPrefixExpression},
          .expected_annotation = {0, SpacingOptions::kMustAppend},
      },
      {
          .style = DefaultStyle,
          .left_token = {'^', "^"},
          .right_token = {verilog_tokentype::SymbolIdentifier, "foo"},
          .left_context = {/* any context */},
          .right_context = {NodeEnum::kUnaryPrefixExpression},
          .expected_annotation = {0, SpacingOptions::kMustAppend},
      },
      {
          .style = DefaultStyle,
          .left_token = {'^', "^"},
          .right_token = {'(', "("},
          .left_context = {/* any context */},
          .right_context = {NodeEnum::kUnaryPrefixExpression},
          .expected_annotation = {0, SpacingOptions::kMustAppend},
      },
      {
          .style = DefaultStyle,
          .left_token = {'^', "^"},
          .right_token = {'{', "{"},
          .left_context = {/* any context */},
          .right_context = {NodeEnum::kUnaryPrefixExpression},
          .expected_annotation = {0, SpacingOptions::kMustAppend},
      },

      // Test '~' unary token
      {
          .style = DefaultStyle,
          .left_token = {'~', "~"},
          .right_token = {'(', "("},
          .left_context = {/* any context */},
          .right_context = {NodeEnum::kUnaryPrefixExpression},
          .expected_annotation = {0, SpacingOptions::kMustAppend},
      },
      {
          .style = DefaultStyle,
          .left_token = {'~', "~"},
          .right_token = {verilog_tokentype::SymbolIdentifier, "foo"},
          .left_context = {/* any context */},
          .right_context = {NodeEnum::kUnaryPrefixExpression},
          .expected_annotation = {0, SpacingOptions::kMustAppend},
      },

      // Test '##' unary (delay) operator
      {
          .style = DefaultStyle,
          .left_token = {verilog_tokentype::TK_POUNDPOUND, "##"},
          .right_token = {'(', "("},
          .left_context = {/* any context */},
          .right_context = {/* any context */},
          .expected_annotation = {0, SpacingOptions::kMustAppend},
      },
      {
          .style = DefaultStyle,
          .left_token = {verilog_tokentype::TK_POUNDPOUND, "##"},
          .right_token = {verilog_tokentype::TK_DecNumber, "10"},
          .left_context = {/* any context */},
          .right_context = {/* any context */},
          .expected_annotation = {0, SpacingOptions::kMustAppend},
      },
      {
          .style = DefaultStyle,
          .left_token = {verilog_tokentype::TK_POUNDPOUND, "##"},
          .right_token = {verilog_tokentype::SymbolIdentifier, "x_delay"},
          .left_context = {/* any context */},
          .right_context = {/* any context */},
          .expected_annotation = {0, SpacingOptions::kMustAppend},
      },
      {
          .style = DefaultStyle,
          .left_token = {verilog_tokentype::TK_POUNDPOUND, "##"},
          .right_token = {verilog_tokentype::MacroIdentifier, "`X_DELAY"},
          .left_context = {/* any context */},
          .right_context = {/* any context */},
          .expected_annotation = {0, SpacingOptions::kMustAppend},
      },
      {
          .style = DefaultStyle,
          .left_token = {verilog_tokentype::TK_POUNDPOUND, "##"},
          .right_token = {verilog_tokentype::TK_LP, "'{"},
          .left_context = {/* any context */},
          .right_context = {/* any context */},
          .expected_annotation = {0, SpacingOptions::kMustAppend},
      },
      {
          .style = DefaultStyle,
          .left_token = {verilog_tokentype::TK_POUNDPOUND, "##"},
          .right_token = {'[', "["},
          .left_context = {/* any context */},
          .right_context = {/* any context */},
          .expected_annotation = {0, SpacingOptions::kMustAppend},
      },
      {
          .style = DefaultStyle,
          .left_token = {verilog_tokentype::TK_POUNDPOUND, "##"},
          .right_token = {verilog_tokentype::TK_LBSTARRB, "[*]"},
          .left_context = {/* any context */},
          .right_context = {/* any context */},
          .expected_annotation = {0, SpacingOptions::kMustAppend},
      },
      {
          .style = DefaultStyle,
          .left_token = {verilog_tokentype::TK_POUNDPOUND, "##"},
          .right_token = {verilog_tokentype::TK_LBPLUSRB, "[+]"},
          .left_context = {/* any context */},
          .right_context = {/* any context */},
          .expected_annotation = {0, SpacingOptions::kMustAppend},
      },
      {
          .style = DefaultStyle,
          .left_token = {verilog_tokentype::SymbolIdentifier, "predicate"},
          .right_token = {verilog_tokentype::TK_POUNDPOUND, "##"},
          .left_context = {/* any context */},
          .right_context = {/* any context */},
          .expected_annotation = {1, SpacingOptions::kUndecided},
      },
      {
          .style = DefaultStyle,
          .left_token = {')', ")"},
          .right_token = {verilog_tokentype::TK_POUNDPOUND, "##"},
          .left_context = {/* any context */},
          .right_context = {/* any context */},
          .expected_annotation = {1, SpacingOptions::kUndecided},
      },
      {
          .style = DefaultStyle,
          .left_token = {'(', "("},
          .right_token = {verilog_tokentype::TK_POUNDPOUND, "##"},
          .left_context = {/* any context */},
          .right_context = {/* any context */},
          .expected_annotation = {0, SpacingOptions::kUndecided},
      },
      {
          .style = DefaultStyle,
          .left_token = {verilog_tokentype::TK_and, "and"},
          .right_token = {verilog_tokentype::TK_POUNDPOUND, "##"},
          .left_context = {/* any context */},
          .right_context = {/* any context */},
          .expected_annotation = {1, SpacingOptions::kUndecided},
      },
      {
          .style = DefaultStyle,
          .left_token = {verilog_tokentype::TK_or, "or"},
          .right_token = {verilog_tokentype::TK_POUNDPOUND, "##"},
          .left_context = {/* any context */},
          .right_context = {/* any context */},
          .expected_annotation = {1, SpacingOptions::kUndecided},
      },
      {
          .style = DefaultStyle,
          .left_token = {verilog_tokentype::TK_intersect, "intersect"},
          .right_token = {verilog_tokentype::TK_POUNDPOUND, "##"},
          .left_context = {/* any context */},
          .right_context = {/* any context */},
          .expected_annotation = {1, SpacingOptions::kUndecided},
      },
      {
          .style = DefaultStyle,
          .left_token = {verilog_tokentype::TK_throughout, "throughout"},
          .right_token = {verilog_tokentype::TK_POUNDPOUND, "##"},
          .left_context = {/* any context */},
          .right_context = {/* any context */},
          .expected_annotation = {1, SpacingOptions::kUndecided},
      },
      {
          .style = DefaultStyle,
          .left_token = {verilog_tokentype::TK_within, "within"},
          .right_token = {verilog_tokentype::TK_POUNDPOUND, "##"},
          .left_context = {/* any context */},
          .right_context = {/* any context */},
          .expected_annotation = {1, SpacingOptions::kUndecided},
      },

      // Two unary operators
      {
          .style = DefaultStyle,
          .left_token = {'~', "~"},
          .right_token = {'~', "~"},
          .left_context = {/* any context */},
          .right_context = {NodeEnum::kUnaryPrefixExpression},
          .expected_annotation = {0, SpacingOptions::kMustAppend},
      },

      // Postfix "i++"/"j--": operand and operator share the same
      // kIncrementDecrementExpression node, so no space is required.
      {
          .style = DefaultStyle,
          .left_token = {verilog_tokentype::SymbolIdentifier, "i"},
          .right_token = {verilog_tokentype::TK_INCR, "++"},
          .left_context = {NodeEnum::kIncrementDecrementExpression},
          .right_context = {NodeEnum::kIncrementDecrementExpression},
          .expected_annotation = {0, SpacingOptions::kUndecided},
      },
      {
          .style = DefaultStyle,
          .left_token = {verilog_tokentype::SymbolIdentifier, "j"},
          .right_token = {verilog_tokentype::TK_DECR, "--"},
          .left_context = {NodeEnum::kIncrementDecrementExpression},
          .right_context = {NodeEnum::kIncrementDecrementExpression},
          .expected_annotation = {0, SpacingOptions::kUndecided},
      },
      // Prefix "++i"/"--j" used as its own statement/expression: the
      // preceding token (e.g. a ')') is not part of the same expression
      {
          .style = DefaultStyle,
          .left_token = {')', ")"},
          .right_token = {verilog_tokentype::TK_INCR, "++"},
          .left_context = {/* any context */},
          .right_context = {NodeEnum::kIncrementDecrementExpression},
          .expected_annotation = {1, SpacingOptions::kUndecided},
      },
      {
          .style = DefaultStyle,
          .left_token = {')', ")"},
          .right_token = {verilog_tokentype::TK_DECR, "--"},
          .left_context = {/* any context */},
          .right_context = {NodeEnum::kIncrementDecrementExpression},
          .expected_annotation = {1, SpacingOptions::kUndecided},
      },

      // Modport explicit port name, e.g. "input .a(sig)"
      {
          .style = DefaultStyle,
          .left_token = {TK_input, "input"},
          .right_token = {'.', "."},
          .left_context = {/* any context */},
          .right_context = {NodeEnum::kModportSimplePort},
          .expected_annotation = {1, SpacingOptions::kUndecided},
      },
      {
          .style = DefaultStyle,
          .left_token = {TK_output, "output"},
          .right_token = {'.', "."},
          .left_context = {/* any context */},
          .right_context = {NodeEnum::kModportSimplePort},
          .expected_annotation = {1, SpacingOptions::kUndecided},
      },

      // Handle '->' as a unary prefix expression.
      {
          .style = DefaultStyle,
          .left_token = {TK_TRIGGER, "->"},
          .right_token = {verilog_tokentype::SymbolIdentifier, "a"},
          .left_context = {/* any context */},   // context
          .right_context = {/* any context */},  // context
          .expected_annotation =
              {0, SpacingOptions::kUndecided},  // could be MustAppend though
      },
      {
          .style = DefaultStyle,
          .left_token = {TK_NONBLOCKING_TRIGGER, "->>"},
          .right_token = {verilog_tokentype::SymbolIdentifier, "a"},
          .left_context = {/* any context */},   // context
          .right_context = {/* any context */},  // context
          .expected_annotation =
              {0, SpacingOptions::kUndecided},  // could be MustAppend though
      },

      // Handle '->'/'->>' as event trigger statements
      {
          .style = DefaultStyle,
          .left_token = {verilog_tokentype::TK_DecNumber, "1"},
          .right_token = {TK_TRIGGER, "->"},
          .left_context = {/* any context */},   // context
          .right_context = {/* any context */},  // context
          .expected_annotation = {1, SpacingOptions::kUndecided},
      },
      {
          .style = DefaultStyle,
          .left_token = {verilog_tokentype::TK_DecNumber, "1"},
          .right_token = {TK_NONBLOCKING_TRIGGER, "->>"},
          .left_context = {/* any context */},   // context
          .right_context = {/* any context */},  // context
          .expected_annotation = {1, SpacingOptions::kUndecided},
      },

      // Handle '->' as a binary operator
      {
          .style = DefaultStyle,
          .left_token = {TK_LOGICAL_IMPLIES, "->"},
          .right_token = {verilog_tokentype::SymbolIdentifier, "right"},
          .left_context = {/* any context */},   // context
          .right_context = {/* any context */},  // context
          .expected_annotation = {1, SpacingOptions::kUndecided},
      },
      {
          .style = DefaultStyle,
          .left_token = {verilog_tokentype::SymbolIdentifier, "left"},
          .right_token = {TK_LOGICAL_IMPLIES, "->"},
          .left_context = {/* any context */},   // context
          .right_context = {/* any context */},  // context
          .expected_annotation = {1, SpacingOptions::kUndecided},
      },
      {
          .style = DefaultStyle,
          .left_token = {TK_CONSTRAINT_IMPLIES, "->"},
          .right_token = {verilog_tokentype::SymbolIdentifier, "right"},
          .left_context = {/* any context */},   // context
          .right_context = {/* any context */},  // context
          .expected_annotation = {1, SpacingOptions::kUndecided},
      },
      {
          .style = DefaultStyle,
          .left_token = {verilog_tokentype::SymbolIdentifier, "left"},
          .right_token = {TK_CONSTRAINT_IMPLIES, "->"},
          .left_context = {/* any context */},   // context
          .right_context = {/* any context */},  // context
          .expected_annotation = {1, SpacingOptions::kUndecided},
      },

      // Inside dimension ranges, force space preservation if not around ':'
      {
          .style = DefaultStyle,
          .left_token = {'*', "*"},
          .right_token = {verilog_tokentype::SymbolIdentifier, "foo"},
          .left_context = {/* any context */},
          .right_context = {/* any context */},
          .expected_annotation = {1, SpacingOptions::kUndecided},
      },
      {
          .style = DefaultStyle,
          .left_token = {verilog_tokentype::SymbolIdentifier, "foo"},
          .right_token = {'*', "*"},
          .left_context = {/* any context */},
          .right_context = {/* any context */},
          .expected_annotation = {1, SpacingOptions::kUndecided},
      },
      {
          .style = DefaultStyle,
          .left_token = {':', ":"},
          .right_token = {verilog_tokentype::SymbolIdentifier, "foo"},
          .left_context = {/* any context */},
          .right_context = {NodeEnum::kDimensionRange},
          .expected_annotation = {0, SpacingOptions::kUndecided},
      },
      {
          .style = DefaultStyle,
          .left_token = {verilog_tokentype::SymbolIdentifier, "foo"},
          .right_token = {':', ":"},
          .left_context = {/* any context */},
          .right_context = {NodeEnum::kDimensionRange},
          .expected_annotation = {0, SpacingOptions::kUndecided},
      },

      // spacing between ranges of multi-dimension arrays
      {
          .style = DefaultStyle,
          .left_token = {']', "]"},
          .right_token = {'[', "["},
          .left_context = {},   // any context
          .right_context = {},  // any context
          .expected_annotation = {0, SpacingOptions::kUndecided},
      },

      // spacing before first '[' of packed arrays in declarations
      {
          .style = DefaultStyle,
          .left_token = {verilog_tokentype::TK_logic, "logic"},
          .right_token = {'[', "["},
          .left_context = {/* any context */},
          .right_context = {/* any context */},
          .expected_annotation = {0, SpacingOptions::kUndecided},
      },
      {
          .style = DefaultStyle,
          .left_token = {verilog_tokentype::SymbolIdentifier, "mytype1"},
          .right_token = {'[', "["},
          .left_context = {/* any context */},
          .right_context =
              {},  // unspecified context, this covers index expressions
          .expected_annotation = {0, SpacingOptions::kUndecided},
      },
      {
          .style = DefaultStyle,
          .left_token = {verilog_tokentype::TK_logic, "logic"},
          .right_token = {'[', "["},
          .left_context = {/* any context */},
          .right_context = {NodeEnum::kPackedDimensions},
          .expected_annotation = {1, SpacingOptions::kUndecided},
      },
      {
          .style = DefaultStyle,
          .left_token = {verilog_tokentype::SymbolIdentifier, "mytype2"},
          .right_token = {'[', "["},
          .left_context = {/* any context */},
          .right_context = {NodeEnum::kPackedDimensions},
          .expected_annotation = {1, SpacingOptions::kUndecided},
      },
      {
          .style = DefaultStyle,
          .left_token = {verilog_tokentype::SymbolIdentifier, "id1"},
          .right_token = {'[', "["},
          .left_context = {/* any context */},
          .right_context = {NodeEnum::kPackedDimensions, NodeEnum::kExpression},
          .expected_annotation = {0, SpacingOptions::kUndecided},
      },

      // spacing after last ']' of packed arrays in declarations
      {
          .style = DefaultStyle,
          .left_token = {']', "]"},
          .right_token = {verilog_tokentype::SymbolIdentifier, "id_a"},
          .left_context = {/* any context */},
          .right_context = {/* any context */},
          .expected_annotation = {1, SpacingOptions::kUndecided},
      },
      {
          .style = DefaultStyle,
          .left_token = {']', "]"},
          .right_token = {verilog_tokentype::SymbolIdentifier, "id_b"},
          .left_context = {/* any context */},
          .right_context = {NodeEnum::kUnqualifiedId},
          .expected_annotation = {1, SpacingOptions::kUndecided},
      },
      {
          .style = DefaultStyle,
          .left_token = {']', "]"},
          .right_token = {verilog_tokentype::SymbolIdentifier, "id_c"},
          .left_context = {/* any context */},
          .right_context = {NodeEnum::kDataTypeImplicitBasicIdDimensions,
                            NodeEnum::kUnqualifiedId},
          .expected_annotation = {1, SpacingOptions::kUndecided},
      },

      // "foo ()" in "module foo();"
      {
          .style = DefaultStyle,
          .left_token = {verilog_tokentype::SymbolIdentifier, "foo"},
          .right_token = {'(', "("},
          .left_context = {/* any context */},
          .right_context = {/* unspecified context */},
          .expected_annotation = {0, SpacingOptions::kUndecided},
      },
      {
          .style = DefaultStyle,
          .left_token = {verilog_tokentype::SymbolIdentifier, "foo"},
          .right_token = {'(', "("},
          .left_context = {NodeEnum::kModuleHeader},
          .right_context = {/* any context */},
          .expected_annotation = {1, SpacingOptions::kUndecided},
      },

      // "a(" in "foo bar (.a(b));": instantiation with named ports
      {
          .style = DefaultStyle,
          .left_token = {verilog_tokentype::SymbolIdentifier, "foo"},
          .right_token = {'(', "("},
          .left_context = {NodeEnum::kGateInstance},
          .right_context = {NodeEnum::kGateInstance},
          .expected_annotation = {1, SpacingOptions::kUndecided},
      },
      {
          .style = DefaultStyle,
          .left_token = {verilog_tokentype::SymbolIdentifier, "foo"},
          .right_token = {'(', "("},
          .left_context = {/* any context */},
          .right_context = {NodeEnum::kPrimitiveGateInstance},
          .expected_annotation = {1, SpacingOptions::kUndecided},
      },
      {
          .style = DefaultStyle,
          .left_token = {verilog_tokentype::SymbolIdentifier, "foo"},
          .right_token = {'(', "("},
          .left_context = {/* any context */},
          .right_context = {NodeEnum::kActualNamedPort},
          .expected_annotation = {0, SpacingOptions::kUndecided},
      },
      {
          .style = DefaultStyle,
          .left_token = {verilog_tokentype::SymbolIdentifier, "foo"},
          .right_token = {'(', "("},
          .left_context = {/* any context */},
          .right_context = {NodeEnum::kGateInstance,
                            NodeEnum::kActualNamedPort},
          .expected_annotation = {0, SpacingOptions::kUndecided},
      },
      {
          .style = DefaultStyle,
          .left_token = {verilog_tokentype::SymbolIdentifier, "foo"},
          .right_token = {'(', "("},
          .left_context = {/* any context */},
          .right_context = {NodeEnum::kModuleHeader, NodeEnum::kPort},
          .expected_annotation = {0, SpacingOptions::kUndecided},
      },

      // cases for the heavily overloaded ':'

      // ':' on the right, anything else on the left
      {
          .style = DefaultStyle,
          .left_token = {verilog_tokentype::SymbolIdentifier, "x"},
          .right_token = {':', ":"},
          .left_context = {/* any context */},
          .right_context = {/* unspecified context */},
          .expected_annotation = kUnhandledSpacing,
      },
      {
          // a ? b : c (condition expression)
          .style = DefaultStyle,
          .left_token = {verilog_tokentype::SymbolIdentifier, "b"},
          .right_token = {':', ":"},
          .left_context = {/* any context */},
          .right_context = {NodeEnum::kConditionExpression},
          .expected_annotation = {1, SpacingOptions::kUndecided},
      },
      {
          // a ? 111 : c (condition expression)
          .style = DefaultStyle,
          .left_token = {verilog_tokentype::TK_DecNumber, "111"},
          .right_token = {':', ":"},
          .left_context = {/* any context */},
          .right_context = {NodeEnum::kConditionExpression},
          .expected_annotation = {1, SpacingOptions::kUndecided},
      },
      {
          // a ? "1" : c (condition expression)
          .style = DefaultStyle,
          .left_token = {verilog_tokentype::TK_StringLiteral, "\"1\""},
          .right_token = {':', ":"},
          .left_context = {/* any context */},
          .right_context = {NodeEnum::kConditionExpression},
          .expected_annotation = {1, SpacingOptions::kUndecided},
      },
      {
          // a ? (1) : c (condition expression)
          .style = DefaultStyle,
          .left_token = {')', ":"},
          .right_token = {':', ":"},
          .left_context = {/* any context */},
          .right_context = {NodeEnum::kConditionExpression},
          .expected_annotation = {1, SpacingOptions::kUndecided},
      },
      {
          // a ? {b} : {c} (condition expression)
          .style = DefaultStyle,
          .left_token = {'}', "}"},
          .right_token = {':', ":"},
          .left_context = {/* any context */},
          .right_context = {NodeEnum::kConditionExpression},
          .expected_annotation = {1, SpacingOptions::kUndecided},
      },
      {
          // a ? {b} : {c} (condition expression)
          .style = DefaultStyle,
          .left_token = {':', ":"},
          .right_token = {'{', "{"},
          .left_context = {/* any context */},
          .right_context = {NodeEnum::kConditionExpression},
          .expected_annotation = {1, SpacingOptions::kUndecided},
      },

      // ':' on the left, anything else on the right
      {
          .style = DefaultStyle,
          .left_token = {':', ":"},
          .right_token = {verilog_tokentype::SymbolIdentifier, "x"},
          .left_context = {/* any context */},
          .right_context = {/* unspecified context */},
          .expected_annotation = {1, SpacingOptions::kUndecided},
      },
      {
          // a ? b : c (condition expression)
          .style = DefaultStyle,
          .left_token = {':', ":"},
          .right_token = {verilog_tokentype::SymbolIdentifier, "c"},
          .left_context = {/* any context */},
          .right_context = {NodeEnum::kConditionExpression},
          .expected_annotation = {1, SpacingOptions::kUndecided},
      },
      {
          // a ? b : 7 (condition expression)
          .style = DefaultStyle,
          .left_token = {':', ":"},
          .right_token = {verilog_tokentype::TK_DecNumber, "7"},
          .left_context = {/* any context */},
          .right_context = {NodeEnum::kConditionExpression},
          .expected_annotation = {1, SpacingOptions::kUndecided},
      },
      {
          // a ? b : "7" (condition expression)
          .style = DefaultStyle,
          .left_token = {':', ":"},
          .right_token = {verilog_tokentype::TK_StringLiteral, "\"7\""},
          .left_context = {/* any context */},
          .right_context = {NodeEnum::kConditionExpression},
          .expected_annotation = {1, SpacingOptions::kUndecided},
      },
      {
          // a ? b : (7) (condition expression)
          .style = DefaultStyle,
          .left_token = {':', ":"},
          .right_token = {'(', "("},
          .left_context = {/* any context */},
          .right_context = {NodeEnum::kConditionExpression},
          .expected_annotation = {1, SpacingOptions::kUndecided},
      },

      // ':' in labels
      // ':' before and after keywords:
      {
          // "begin :"
          .style = DefaultStyle,
          .left_token = {verilog_tokentype::TK_begin, "begin"},
          .right_token = {':', ":"},
          .left_context = {/* unspecified context */},
          .right_context = {/* unspecified context */},
          .expected_annotation = {1, SpacingOptions::kUndecided},
      },
      {
          // ": begin"
          .style = DefaultStyle,
          .left_token = {':', ":"},
          .right_token = {verilog_tokentype::TK_begin, "begin"},
          .left_context = {/* unspecified context */},
          .right_context = {/* unspecified context */},
          .expected_annotation = {1, SpacingOptions::kUndecided},
      },
      {
          // "fork :"
          .style = DefaultStyle,
          .left_token = {verilog_tokentype::TK_fork, "fork"},
          .right_token = {':', ":"},
          .left_context = {/* unspecified context */},
          .right_context = {/* unspecified context */},
          .expected_annotation = {1, SpacingOptions::kUndecided},
      },
      {
          // "end :"
          .style = DefaultStyle,
          .left_token = {verilog_tokentype::TK_end, "end"},
          .right_token = {':', ":"},
          .left_context = {/* unspecified context */},
          .right_context = {/* unspecified context */},
          .expected_annotation = {1, SpacingOptions::kUndecided},
      },
      {
          // "endclass :"
          .style = DefaultStyle,
          .left_token = {verilog_tokentype::TK_endclass, "endclass"},
          .right_token = {':', ":"},
          .left_context = {/* unspecified context */},
          .right_context = {/* unspecified context */},
          .expected_annotation = {1, SpacingOptions::kUndecided},
      },
      {
          // "endfunction :"
          .style = DefaultStyle,
          .left_token = {verilog_tokentype::TK_endfunction, "endfunction"},
          .right_token = {':', ":"},
          .left_context = {/* unspecified context */},
          .right_context = {/* unspecified context */},
          .expected_annotation = {1, SpacingOptions::kUndecided},
      },
      {
          // "endtask :"
          .style = DefaultStyle,
          .left_token = {verilog_tokentype::TK_endtask, "endtask"},
          .right_token = {':', ":"},
          .left_context = {/* unspecified context */},
          .right_context = {/* unspecified context */},
          .expected_annotation = {1, SpacingOptions::kUndecided},
      },
      {
          // "endmodule :"
          .style = DefaultStyle,
          .left_token = {verilog_tokentype::TK_endmodule, "endmodule"},
          .right_token = {':', ":"},
          .left_context = {/* unspecified context */},
          .right_context = {/* unspecified context */},
          .expected_annotation = {1, SpacingOptions::kUndecided},
      },
      {
          // "endpackage :"
          .style = DefaultStyle,
          .left_token = {verilog_tokentype::TK_endpackage, "endpackage"},
          .right_token = {':', ":"},
          .left_context = {/* unspecified context */},
          .right_context = {/* unspecified context */},
          .expected_annotation = {1, SpacingOptions::kUndecided},
      },
      {
          // "endinterface :"
          .style = DefaultStyle,
          .left_token = {verilog_tokentype::TK_endinterface, "endinterface"},
          .right_token = {':', ":"},
          .left_context = {/* unspecified context */},
          .right_context = {/* unspecified context */},
          .expected_annotation = {1, SpacingOptions::kUndecided},
      },
      {
          // "endproperty :"
          .style = DefaultStyle,
          .left_token = {verilog_tokentype::TK_endproperty, "endproperty"},
          .right_token = {':', ":"},
          .left_context = {/* unspecified context */},
          .right_context = {/* unspecified context */},
          .expected_annotation = {1, SpacingOptions::kUndecided},
      },
      {
          // "endclocking :"
          .style = DefaultStyle,
          .left_token = {verilog_tokentype::TK_endclocking, "endclocking"},
          .right_token = {':', ":"},
          .left_context = {/* unspecified context */},
          .right_context = {/* unspecified context */},
          .expected_annotation = {1, SpacingOptions::kUndecided},
      },
      // endcase and endgenerate do not get labels

      // ':' before and after label identifiers:
      {
          // "id :"
          .style = DefaultStyle,
          .left_token = {verilog_tokentype::SymbolIdentifier, "id"},
          .right_token = {':', ":"},
          .left_context = {/* unspecified context */},
          .right_context = {/* unspecified context */},
          .expected_annotation = kUnhandledSpacing,
      },
      {
          // "id :"
          .style = DefaultStyle,
          .left_token = {verilog_tokentype::SymbolIdentifier, "id"},
          .right_token = {':', ":"},
          .left_context = {/* unspecified context */},
          .right_context = {NodeEnum::kBlockIdentifier},
          .expected_annotation = {1, SpacingOptions::kUndecided},
      },
      {
          // "id : begin ..."
          .style = DefaultStyle,
          .left_token = {verilog_tokentype::SymbolIdentifier, "id"},
          .right_token = {':', ":"},
          .left_context = {/* unspecified context */},
          .right_context = {NodeEnum::kLabeledStatement},
          .expected_annotation = {1, SpacingOptions::kUndecided},
      },
      {
          // "id :"
          .style = DefaultStyle,
          .left_token = {verilog_tokentype::SymbolIdentifier, "id"},
          .right_token = {':', ":"},
          .left_context = {/* unspecified context */},
          .right_context = {NodeEnum::kCaseItem},
          .expected_annotation = {0, SpacingOptions::kUndecided},
      },
      {
          // "id :"
          .style = DefaultStyle,
          .left_token = {verilog_tokentype::SymbolIdentifier, "id"},
          .right_token = {':', ":"},
          .left_context = {/* unspecified context */},
          .right_context = {NodeEnum::kCaseInsideItem},
          .expected_annotation = {0, SpacingOptions::kUndecided},
      },
      {
          // "id :"
          .style = DefaultStyle,
          .left_token = {verilog_tokentype::SymbolIdentifier, "id"},
          .right_token = {':', ":"},
          .left_context = {/* unspecified context */},
          .right_context = {NodeEnum::kCasePatternItem},
          .expected_annotation = {0, SpacingOptions::kUndecided},
      },
      {
          // "id :"
          .style = DefaultStyle,
          .left_token = {verilog_tokentype::SymbolIdentifier, "id"},
          .right_token = {':', ":"},
          .left_context = {/* unspecified context */},
          .right_context = {NodeEnum::kGenerateCaseItem},
          .expected_annotation = {0, SpacingOptions::kUndecided},
      },
      {
          // "id :"
          .style = DefaultStyle,
          .left_token = {verilog_tokentype::SymbolIdentifier, "id"},
          .right_token = {':', ":"},
          .left_context = {/* unspecified context */},
          .right_context = {NodeEnum::kPropertyCaseItem},
          .expected_annotation = {0, SpacingOptions::kUndecided},
      },
      {
          // "id :"
          .style = DefaultStyle,
          .left_token = {verilog_tokentype::SymbolIdentifier, "id"},
          .right_token = {':', ":"},
          .left_context = {/* unspecified context */},
          .right_context = {NodeEnum::kRandSequenceCaseItem},
          .expected_annotation = {0, SpacingOptions::kUndecided},
      },
      {
          // ": id"
          .style = DefaultStyle,
          .left_token = {':', ":"},
          .right_token = {verilog_tokentype::SymbolIdentifier, "id"},
          .left_context = {/* unspecified context */},
          .right_context = {/* unspecified context */},
          .expected_annotation = {1, SpacingOptions::kUndecided},
      },
      {
          // ": id"
          .style = DefaultStyle,
          .left_token = {':', ":"},
          .right_token = {verilog_tokentype::SymbolIdentifier, "id"},
          .left_context = {/* unspecified context */},
          .right_context = {NodeEnum::kLabel},
          .expected_annotation = {1, SpacingOptions::kUndecided},
      },
      // Shift operators
      {
          // foo = 1 << width;
          .style = DefaultStyle,
          .left_token = {verilog_tokentype::TK_DecNumber, "1"},
          .right_token = {verilog_tokentype::TK_LS, "<<"},
          .left_context = {/* unspecified context */},
          .right_context = {/* unspecified context */},
          .expected_annotation = {1, SpacingOptions::kUndecided},
      },
      {
          // foo = 1 << width;
          .style = DefaultStyle,
          .left_token = {verilog_tokentype::TK_LS, "<<"},
          .right_token = {verilog_tokentype::SymbolIdentifier, "width"},
          .left_context = {/* unspecified context */},
          .right_context = {/* unspecified context */},
          .expected_annotation = {1, SpacingOptions::kUndecided},
      },
      {
          // foo = bar << 4;
          .style = DefaultStyle,
          .left_token = {verilog_tokentype::SymbolIdentifier, "bar"},
          .right_token = {verilog_tokentype::TK_LS, "<<"},
          .left_context = {/* unspecified context */},
          .right_context = {/* unspecified context */},
          .expected_annotation = {1, SpacingOptions::kUndecided},
      },
      {
          // foo = bar << 4;
          .style = DefaultStyle,
          .left_token = {verilog_tokentype::TK_LS, "<<"},
          .right_token = {verilog_tokentype::TK_DecNumber, "4"},
          .left_context = {/* unspecified context */},
          .right_context = {/* unspecified context */},
          .expected_annotation = {1, SpacingOptions::kUndecided},
      },
      {
          // foo = `VAL << 4;
          .style = DefaultStyle,
          .left_token = {verilog_tokentype::MacroIdentifier, "`VAL"},
          .right_token = {verilog_tokentype::TK_LS, "<<"},
          .left_context = {/* unspecified context */},
          .right_context = {/* unspecified context */},
          .expected_annotation = {1, SpacingOptions::kUndecided},
      },
      {
          // foo = bar << `SIZE;
          .style = DefaultStyle,
          .left_token = {verilog_tokentype::TK_LS, "<<"},
          .right_token = {verilog_tokentype::MacroIdentifier, "`SIZE"},
          .left_context = {/* unspecified context */},
          .right_context = {/* unspecified context */},
          .expected_annotation = {1, SpacingOptions::kUndecided},
      },
      {
          // foo = 1 >> width;
          .style = DefaultStyle,
          .left_token = {verilog_tokentype::TK_DecNumber, "1"},
          .right_token = {verilog_tokentype::TK_RS, ">>"},
          .left_context = {/* unspecified context */},
          .right_context = {/* unspecified context */},
          .expected_annotation = {1, SpacingOptions::kUndecided},
      },
      {
          // foo = 1 >> width;
          .style = DefaultStyle,
          .left_token = {verilog_tokentype::TK_RS, ">>"},
          .right_token = {verilog_tokentype::SymbolIdentifier, "width"},
          .left_context = {/* unspecified context */},
          .right_context = {/* unspecified context */},
          .expected_annotation = {1, SpacingOptions::kUndecided},
      },
      {
          // foo = bar >> 4;
          .style = DefaultStyle,
          .left_token = {verilog_tokentype::SymbolIdentifier, "bar"},
          .right_token = {verilog_tokentype::TK_RS, ">>"},
          .left_context = {/* unspecified context */},
          .right_context = {/* unspecified context */},
          .expected_annotation = {1, SpacingOptions::kUndecided},
      },
      {
          // foo = bar >> 4;
          .style = DefaultStyle,
          .left_token = {verilog_tokentype::TK_RS, ">>"},
          .right_token = {verilog_tokentype::TK_DecNumber, "4"},
          .left_context = {/* unspecified context */},
          .right_context = {/* unspecified context */},
          .expected_annotation = {1, SpacingOptions::kUndecided},
      },
      {
          // foo = `VAL >> 4;
          .style = DefaultStyle,
          .left_token = {verilog_tokentype::MacroIdentifier, "`VAL"},
          .right_token = {verilog_tokentype::TK_RS, ">>"},
          .left_context = {/* unspecified context */},
          .right_context = {/* unspecified context */},
          .expected_annotation = {1, SpacingOptions::kUndecided},
      },
      {
          // foo = bar >> `SIZE;
          .style = DefaultStyle,
          .left_token = {verilog_tokentype::TK_RS, ">>"},
          .right_token = {verilog_tokentype::MacroIdentifier, "`SIZE"},
          .left_context = {/* unspecified context */},
          .right_context = {/* unspecified context */},
          .expected_annotation = {1, SpacingOptions::kUndecided},
      },
      // Streaming operators
      {
          // foo = {<<{bar}};
          .style = DefaultStyle,
          .left_token = {'=', "="},
          .right_token = {'{', "{"},
          .left_context = {/* unspecified context */},
          .right_context = {NodeEnum::kStreamingConcatenation},
          .expected_annotation = {1, SpacingOptions::kUndecided},
      },
      {
          // foo = {<<{bar}};
          .style = DefaultStyle,
          .left_token = {'{', "{"},
          .right_token = {verilog_tokentype::TK_LS, "<<"},
          .left_context = {/* unspecified context */},
          .right_context = {NodeEnum::kStreamingConcatenation},
          .expected_annotation = {0, SpacingOptions::kUndecided},
      },
      {
          // foo = {<<{bar}};
          .style = DefaultStyle,
          .left_token = {verilog_tokentype::TK_LS, "<<"},
          .right_token = {'{', "{"},
          .left_context = {/* unspecified context */},
          .right_context = {NodeEnum::kStreamingConcatenation},
          .expected_annotation = {0, SpacingOptions::kUndecided},
      },
      {
          // foo = {<<{bar}};
          .style = DefaultStyle,
          .left_token = {'{', "{"},
          .right_token = {verilog_tokentype::SymbolIdentifier, "bar"},
          .left_context = {/* unspecified context */},
          .right_context = {NodeEnum::kStreamingConcatenation},
          .expected_annotation = {0, SpacingOptions::kUndecided},
      },
      {
          // foo = {<<{bar}};
          .style = DefaultStyle,
          .left_token = {verilog_tokentype::SymbolIdentifier, "bar"},
          .right_token = {'}', "}"},
          .left_context = {/* unspecified context */},
          .right_context = {NodeEnum::kStreamingConcatenation},
          .expected_annotation = {0, SpacingOptions::kUndecided},
      },
      {
          // foo = {<<4{bar}};
          .style = DefaultStyle,
          .left_token = {verilog_tokentype::TK_LS, "<<"},
          .right_token = {verilog_tokentype::TK_DecNumber, "4"},
          .left_context = {/* unspecified context */},
          .right_context = {NodeEnum::kStreamingConcatenation},
          .expected_annotation = {0, SpacingOptions::kUndecided},
      },
      {
          // foo = {<<4{bar}};
          .style = DefaultStyle,
          .left_token = {verilog_tokentype::TK_DecNumber, "4"},
          .right_token = {'{', "{"},
          .left_context = {/* unspecified context */},
          .right_context = {NodeEnum::kStreamingConcatenation},
          .expected_annotation = {0, SpacingOptions::kUndecided},
      },
      {
          // foo = {<<byte{bar}};
          .style = DefaultStyle,
          .left_token = {verilog_tokentype::TK_LS, "<<"},
          .right_token = {verilog_tokentype::TK_byte, "byte"},
          .left_context = {/* unspecified context */},
          .right_context = {NodeEnum::kStreamingConcatenation},
          .expected_annotation = {0, SpacingOptions::kUndecided},
      },
      {
          // foo = {<<byte{bar}};
          .style = DefaultStyle,
          .left_token = {verilog_tokentype::TK_byte, "byte"},
          .right_token = {'{', "{"},
          .left_context = {/* unspecified context */},
          .right_context = {NodeEnum::kStreamingConcatenation},
          .expected_annotation = {0, SpacingOptions::kUndecided},
      },
      {
          // foo = {<<type_t{bar}};
          .style = DefaultStyle,
          .left_token = {verilog_tokentype::TK_LS, "<<"},
          .right_token = {verilog_tokentype::SymbolIdentifier, "type_t"},
          .left_context = {/* unspecified context */},
          .right_context = {NodeEnum::kStreamingConcatenation},
          .expected_annotation = {0, SpacingOptions::kUndecided},
      },
      {
          // foo = {<<type_t{bar}};
          .style = DefaultStyle,
          .left_token = {verilog_tokentype::SymbolIdentifier, "type_t"},
          .right_token = {'{', "{"},
          .left_context = {/* unspecified context */},
          .right_context = {NodeEnum::kStreamingConcatenation},
          .expected_annotation = {0, SpacingOptions::kUndecided},
      },
      {
          // foo = {<<`GET_TYPE{bar}};
          .style = DefaultStyle,
          .left_token = {verilog_tokentype::TK_LS, "<<"},
          .right_token = {verilog_tokentype::MacroIdentifier, "`GET_TYPE"},
          .left_context = {/* unspecified context */},
          .right_context = {NodeEnum::kStreamingConcatenation},
          .expected_annotation = {0, SpacingOptions::kUndecided},
      },
      {
          // foo = {<<`GET_TYPE{bar}};
          .style = DefaultStyle,
          .left_token = {verilog_tokentype::MacroIdentifier, "`GET_TYPE"},
          .right_token = {'{', "{"},
          .left_context = {/* unspecified context */},
          .right_context = {NodeEnum::kStreamingConcatenation},
          .expected_annotation = {0, SpacingOptions::kUndecided},
      },
      {
          // foo = {>>{bar}};
          .style = DefaultStyle,
          .left_token = {'=', "="},
          .right_token = {'{', "{"},
          .left_context = {/* unspecified context */},
          .right_context = {NodeEnum::kStreamingConcatenation},
          .expected_annotation = {1, SpacingOptions::kUndecided},
      },
      {
          // foo = {>>{bar}};
          .style = DefaultStyle,
          .left_token = {'{', "{"},
          .right_token = {verilog_tokentype::TK_RS, ">>"},
          .left_context = {/* unspecified context */},
          .right_context = {NodeEnum::kStreamingConcatenation},
          .expected_annotation = {0, SpacingOptions::kUndecided},
      },
      {
          // foo = {>>{bar}};
          .style = DefaultStyle,
          .left_token = {verilog_tokentype::TK_RS, ">>"},
          .right_token = {'{', "{"},
          .left_context = {/* unspecified context */},
          .right_context = {NodeEnum::kStreamingConcatenation},
          .expected_annotation = {0, SpacingOptions::kUndecided},
      },
      {
          // foo = {>>{bar}};
          .style = DefaultStyle,
          .left_token = {'{', "{"},
          .right_token = {verilog_tokentype::SymbolIdentifier, "bar"},
          .left_context = {/* unspecified context */},
          .right_context = {NodeEnum::kStreamingConcatenation},
          .expected_annotation = {0, SpacingOptions::kUndecided},
      },
      {
          // foo = {>>{bar}};
          .style = DefaultStyle,
          .left_token = {verilog_tokentype::SymbolIdentifier, "bar"},
          .right_token = {'}', "}"},
          .left_context = {/* unspecified context */},
          .right_context = {NodeEnum::kStreamingConcatenation},
          .expected_annotation = {0, SpacingOptions::kUndecided},
      },
      {
          // foo = {>>4{bar}};
          .style = DefaultStyle,
          .left_token = {verilog_tokentype::TK_RS, ">>"},
          .right_token = {verilog_tokentype::TK_DecNumber, "4"},
          .left_context = {/* unspecified context */},
          .right_context = {NodeEnum::kStreamingConcatenation},
          .expected_annotation = {0, SpacingOptions::kUndecided},
      },
      {
          // foo = {>>4{bar}};
          .style = DefaultStyle,
          .left_token = {verilog_tokentype::TK_DecNumber, "4"},
          .right_token = {'{', "{"},
          .left_context = {/* unspecified context */},
          .right_context = {NodeEnum::kStreamingConcatenation},
          .expected_annotation = {0, SpacingOptions::kUndecided},
      },
      {
          // foo = {>>byte{bar}};
          .style = DefaultStyle,
          .left_token = {verilog_tokentype::TK_RS, ">>"},
          .right_token = {verilog_tokentype::TK_byte, "byte"},
          .left_context = {/* unspecified context */},
          .right_context = {NodeEnum::kStreamingConcatenation},
          .expected_annotation = {0, SpacingOptions::kUndecided},
      },
      {
          // foo = {>>byte{bar}};
          .style = DefaultStyle,
          .left_token = {verilog_tokentype::TK_byte, "byte"},
          .right_token = {'{', "{"},
          .left_context = {/* unspecified context */},
          .right_context = {NodeEnum::kStreamingConcatenation},
          .expected_annotation = {0, SpacingOptions::kUndecided},
      },
      {
          // foo = {>>type_t{bar}};
          .style = DefaultStyle,
          .left_token = {verilog_tokentype::TK_RS, ">>"},
          .right_token = {verilog_tokentype::SymbolIdentifier, "type_t"},
          .left_context = {/* unspecified context */},
          .right_context = {NodeEnum::kStreamingConcatenation},
          .expected_annotation = {0, SpacingOptions::kUndecided},
      },
      {
          // foo = {>>type_t{bar}};
          .style = DefaultStyle,
          .left_token = {verilog_tokentype::SymbolIdentifier, "type_t"},
          .right_token = {'{', "{"},
          .left_context = {/* unspecified context */},
          .right_context = {NodeEnum::kStreamingConcatenation},
          .expected_annotation = {0, SpacingOptions::kUndecided},
      },
      {
          // foo = {>>`GET_TYPE{bar}};
          .style = DefaultStyle,
          .left_token = {verilog_tokentype::TK_RS, ">>"},
          .right_token = {verilog_tokentype::MacroIdentifier, "`GET_TYPE"},
          .left_context = {/* unspecified context */},
          .right_context = {NodeEnum::kStreamingConcatenation},
          .expected_annotation = {0, SpacingOptions::kUndecided},
      },
      {
          // foo = {>>`GET_TYPE{bar}};
          .style = DefaultStyle,
          .left_token = {verilog_tokentype::MacroIdentifier, "`GET_TYPE"},
          .right_token = {'{', "{"},
          .left_context = {/* unspecified context */},
          .right_context = {NodeEnum::kStreamingConcatenation},
          .expected_annotation = {0, SpacingOptions::kUndecided},
      },
      // ':' in bit slicing and array indexing
      {
          // [1:0]
          .style = DefaultStyle,
          .left_token = {verilog_tokentype::TK_DecNumber, "1"},
          .right_token = {':', ":"},
          .left_context = {/* unspecified context */},
          .right_context = {NodeEnum::kDimensionRange},
          // no spaces preceding ':' in unit test context
          .expected_annotation = {0, SpacingOptions::kUndecided},
      },
      {
          // [1:0]
          .style = DefaultStyle,
          .left_token = {':', ":"},
          .right_token = {verilog_tokentype::TK_DecNumber, "0"},
          .left_context = {/* unspecified context */},
          .right_context = {NodeEnum::kDimensionRange},
          // no spaces preceding ':' in unit test context
          .expected_annotation = {0, SpacingOptions::kUndecided},
      },
      {
          // [a:b]
          .style = DefaultStyle,
          .left_token = {SymbolIdentifier, "a"},
          .right_token = {':', ":"},
          .left_context = {/* unspecified context */},
          .right_context = {NodeEnum::kDimensionRange},
          // no spaces preceding ':' in unit test context
          .expected_annotation = {0, SpacingOptions::kUndecided},
      },
      {
          // [a:b]
          .style = DefaultStyle,
          .left_token = {':', ":"},
          .right_token = {SymbolIdentifier, "b"},
          .left_context = {/* unspecified context */},
          .right_context = {NodeEnum::kDimensionRange},
          // no spaces preceding ':' in unit test context
          .expected_annotation = {0, SpacingOptions::kUndecided},
      },
      {
          // [1:0]
          .style = DefaultStyle,
          .left_token = {verilog_tokentype::TK_DecNumber, "1"},
          .right_token = {':', ":"},
          .left_context = {/* unspecified context */},
          .right_context = {NodeEnum::kDimensionRange},
          // no spaces preceding ':' in unit test context
          .expected_annotation = {0, SpacingOptions::kUndecided},
      },
      {
          // [1:0]
          .style = DefaultStyle,
          .left_token = {':', ":"},
          .right_token = {verilog_tokentype::TK_DecNumber, "0"},
          .left_context = {/* unspecified context */},
          .right_context = {NodeEnum::kDimensionRange},
          // no spaces preceding ':' in unit test context
          .expected_annotation = {0, SpacingOptions::kUndecided},
      },
      {
          // [a:b]
          .style = DefaultStyle,
          .left_token = {SymbolIdentifier, "a"},
          .right_token = {':', ":"},
          .left_context = {/* unspecified context */},
          .right_context = {NodeEnum::kDimensionRange},
          // no spaces preceding ':' in unit test context
          .expected_annotation = {0, SpacingOptions::kUndecided},
      },
      {
          // [a:b]
          .style = DefaultStyle,
          .left_token = {':', ":"},
          .right_token = {SymbolIdentifier, "b"},
          .left_context = {/* unspecified context */},
          .right_context = {NodeEnum::kDimensionRange},
          // no spaces preceding ':' in unit test context
          .expected_annotation = {0, SpacingOptions::kUndecided},
      },
      {
          // [1:0]
          .style = DefaultStyle,
          .left_token = {verilog_tokentype::TK_DecNumber, "1"},
          .right_token = {':', ":"},
          .left_context = {/* unspecified context */},
          .right_context = {NodeEnum::kDimensionSlice},
          // no spaces preceding ':' in unit test context
          .expected_annotation = {0, SpacingOptions::kUndecided},
      },
      {
          // [1:0]
          .style = DefaultStyle,
          .left_token = {':', ":"},
          .right_token = {verilog_tokentype::TK_DecNumber, "0"},
          .left_context = {/* unspecified context */},
          .right_context = {NodeEnum::kDimensionSlice},
          // no spaces preceding ':' in unit test context
          .expected_annotation = {0, SpacingOptions::kUndecided},
      },
      {
          // [a:b]
          .style = DefaultStyle,
          .left_token = {SymbolIdentifier, "a"},
          .right_token = {':', ":"},
          .left_context = {/* unspecified context */},
          .right_context = {NodeEnum::kDimensionSlice},
          // no spaces preceding ':' in unit test context
          .expected_annotation = {0, SpacingOptions::kUndecided},
      },
      {
          // [a:b]
          .style = DefaultStyle,
          .left_token = {':', ":"},
          .right_token = {SymbolIdentifier, "b"},
          .left_context = {/* unspecified context */},
          .right_context = {NodeEnum::kDimensionSlice},
          // no spaces preceding ':' in unit test context
          .expected_annotation = {0, SpacingOptions::kUndecided},
      },
      {
          // [1:0]
          .style = DefaultStyle,
          .left_token = {verilog_tokentype::TK_DecNumber, "1"},
          .right_token = {':', ":"},
          .left_context = {/* any context */},
          .right_context = {NodeEnum::kCycleDelayRange},
          // no spaces preceding ':' in unit test context
          .expected_annotation = {0, SpacingOptions::kUndecided},
      },
      {
          // [1:0]
          .style = DefaultStyle,
          .left_token = {':', ":"},
          .right_token = {verilog_tokentype::TK_DecNumber, "0"},
          .left_context = {/* any context */},
          .right_context = {NodeEnum::kCycleDelayRange},
          // no spaces preceding ':' in unit test context
          .expected_annotation = {0, SpacingOptions::kUndecided},
      },
      {
          // [a:b]
          .style = DefaultStyle,
          .left_token = {SymbolIdentifier, "a"},
          .right_token = {':', ":"},
          .left_context = {/* any context */},
          .right_context = {NodeEnum::kCycleDelayRange},
          // no spaces preceding ':' in unit test context
          .expected_annotation = {0, SpacingOptions::kUndecided},
      },
      {
          // [a:b]
          .style = DefaultStyle,
          .left_token = {':', ":"},
          .right_token = {SymbolIdentifier, "b"},
          .left_context = {/* any context */},
          .right_context = {NodeEnum::kCycleDelayRange},
          // no spaces preceding ':' in unit test context
          .expected_annotation = {0, SpacingOptions::kUndecided},
      },
      {
          // [1:0]
          .style = DefaultStyle,
          .left_token = {verilog_tokentype::TK_DecNumber, "1"},
          .right_token = {':', ":"},
          .left_context = {/* unspecified context */},
          .right_context = {NodeEnum::kValueRange},
          // no spaces preceding ':' in unit test context
          .expected_annotation = {1, SpacingOptions::kUndecided},
      },
      {
          // [1:0]
          .style = DefaultStyle,
          .left_token = {':', ":"},
          .right_token = {verilog_tokentype::TK_DecNumber, "0"},
          .left_context = {/* unspecified context */},
          .right_context = {NodeEnum::kValueRange},
          // no spaces preceding ':' in unit test context
          .expected_annotation = {1, SpacingOptions::kUndecided},
      },
      {
          // [a:b]
          .style = DefaultStyle,
          .left_token = {SymbolIdentifier, "a"},
          .right_token = {':', ":"},
          .left_context = {/* unspecified context */},
          .right_context = {NodeEnum::kValueRange},
          // no spaces preceding ':' in unit test context
          .expected_annotation = {1, SpacingOptions::kUndecided},
      },
      {
          // [a:b]
          .style = DefaultStyle,
          .left_token = {':', ":"},
          .right_token = {SymbolIdentifier, "b"},
          .left_context = {/* unspecified context */},
          .right_context = {NodeEnum::kValueRange},
          // no spaces preceding ':' in unit test context
          .expected_annotation = {1, SpacingOptions::kUndecided},
      },

      {
          // "] {" in "typedef logic [N] { ..."
          // where [N] is a packed dimension
          .style = DefaultStyle,
          .left_token = {']', "]"},
          .right_token = {'{', "{"},
          .left_context = {NodeEnum::kPackedDimensions},
          .right_context = {/* unspecified context */},
          .expected_annotation = {1, SpacingOptions::kUndecided},
      },
      {
          // "] {" in "typedef logic [M:N] { ..."
          // where [M:N] is a packed dimension
          .style = DefaultStyle,
          .left_token = {']', "]"},
          .right_token = {'{', "{"},
          .left_context = {NodeEnum::kPackedDimensions},
          .right_context = {/* unspecified context */},
          .expected_annotation = {1, SpacingOptions::kUndecided},
      },
      {
          // "]{" in other contexts
          .style = DefaultStyle,
          .left_token = {']', "]"},
          .right_token = {'{', "{"},
          .left_context = {/* unspecified context */},
          .right_context = {/* unspecified context */},
          .expected_annotation = {0, SpacingOptions::kUndecided},
      },

      // name: coverpoint
      {
          .style = DefaultStyle,
          .left_token = {SymbolIdentifier, "foo_cp"},
          .right_token = {':', ":"},
          .left_context = {/* unspecified context */},
          .right_context = {NodeEnum::kCoverPoint},
          .expected_annotation = {0, SpacingOptions::kUndecided},
      },
      // coverpoint foo {
      {
          .style = DefaultStyle,
          .left_token = {SymbolIdentifier, "cpaddr"},
          .right_token = {'{', "{"},
          .left_context = {/* unspecified context */},
          .right_context = {NodeEnum::kCoverPoint, NodeEnum::kBraceGroup},
          .expected_annotation = {1, SpacingOptions::kUndecided},
      },
      // enum name TYPEID {
      {
          .style = DefaultStyle,
          .left_token = {SymbolIdentifier, "mytype_t"},
          .right_token = {'{', "{"},
          .left_context = {/* unspecified context */},
          .right_context = {NodeEnum::kEnumType, NodeEnum::kBraceGroup},
          .expected_annotation = {1, SpacingOptions::kUndecided},
      },

      // x < y (binary operator)
      {
          .style = DefaultStyle,
          .left_token = {SymbolIdentifier, "id"},
          .right_token = {'<', "<"},
          .left_context = {},   // default context
          .right_context = {},  // default context
          .expected_annotation = {1, SpacingOptions::kUndecided},
      },
      {
          .style = DefaultStyle,
          .left_token = {TK_DecNumber, "7"},
          .right_token = {'<', "<"},
          .left_context = {},   // default context
          .right_context = {},  // default context
          .expected_annotation = {1, SpacingOptions::kUndecided},
      },
      {
          .style = DefaultStyle,
          .left_token = {')', ")"},
          .right_token = {'<', "<"},
          .left_context = {},   // default context
          .right_context = {},  // default context
          .expected_annotation = {1, SpacingOptions::kUndecided},
      },
      {
          .style = DefaultStyle,
          .left_token = {'<', "<"},
          .right_token = {SymbolIdentifier, "id"},
          .left_context = {},   // default context
          .right_context = {},  // default context
          .expected_annotation = {1, SpacingOptions::kUndecided},
      },
      {
          .style = DefaultStyle,
          .left_token = {'<', "<"},
          .right_token = {TK_DecNumber, "7"},
          .left_context = {},   // default context
          .right_context = {},  // default context
          .expected_annotation = {1, SpacingOptions::kUndecided},
      },
      {
          .style = DefaultStyle,
          .left_token = {'<', "<"},
          .right_token = {'(', "("},
          .left_context = {},   // default context
          .right_context = {},  // default context
          .expected_annotation = {1, SpacingOptions::kUndecided},
      },

      // x > y (binary operator)
      {
          .style = DefaultStyle,
          .left_token = {SymbolIdentifier, "id"},
          .right_token = {'>', ">"},
          .left_context = {},   // default context
          .right_context = {},  // default context
          .expected_annotation = {1, SpacingOptions::kUndecided},
      },
      {
          .style = DefaultStyle,
          .left_token = {TK_DecNumber, "7"},
          .right_token = {'>', ">"},
          .left_context = {},   // default context
          .right_context = {},  // default context
          .expected_annotation = {1, SpacingOptions::kUndecided},
      },
      {
          .style = DefaultStyle,
          .left_token = {')', ")"},
          .right_token = {'>', ">"},
          .left_context = {},   // default context
          .right_context = {},  // default context
          .expected_annotation = {1, SpacingOptions::kUndecided},
      },
      {
          .style = DefaultStyle,
          .left_token = {'>', ">"},
          .right_token = {SymbolIdentifier, "id"},
          .left_context = {},   // default context
          .right_context = {},  // default context
          .expected_annotation = {1, SpacingOptions::kUndecided},
      },
      {
          .style = DefaultStyle,
          .left_token = {'>', ">"},
          .right_token = {TK_DecNumber, "7"},
          .left_context = {},   // default context
          .right_context = {},  // default context
          .expected_annotation = {1, SpacingOptions::kUndecided},
      },
      {
          .style = DefaultStyle,
          .left_token = {'>', ">"},
          .right_token = {'(', "("},
          .left_context = {},   // default context
          .right_context = {},  // default context
          .expected_annotation = {1, SpacingOptions::kUndecided},
      },

      // '@' on the right
      {
          .style = DefaultStyle,
          .left_token = {TK_always, "always"},
          .right_token = {'@', "@"},
          .left_context = {},   // default context
          .right_context = {},  // default context
          .expected_annotation = {1, SpacingOptions::kUndecided},
      },
      {
          .style = DefaultStyle,
          .left_token = {SymbolIdentifier, "cblock"},
          .right_token = {'@', "@"},
          .left_context = {},   // default context
          .right_context = {},  // default context
          .expected_annotation = {1, SpacingOptions::kUndecided},
      },
      // '@' on the left
      {
          .style = DefaultStyle,
          .left_token = {'@', "@"},
          .right_token = {'(', "("},
          .left_context = {},   // default context
          .right_context = {},  // default context
          .expected_annotation = {0, SpacingOptions::kUndecided},
      },
      {
          .style = DefaultStyle,
          .left_token = {'@', "@"},
          .right_token = {'*', "*"},  // not a binary operator in this case
          .left_context = {},         // default context
          .right_context = {},        // default context
          .expected_annotation = {0, SpacingOptions::kUndecided},
      },
      {
          .style = DefaultStyle,
          .left_token = {'@', "@"},
          .right_token = {SymbolIdentifier, "clock_a"},
          .left_context = {},   // default context
          .right_context = {},  // default context
          .expected_annotation = {0, SpacingOptions::kUndecided},
      },

      // '#' on the right
      {
          .style = DefaultStyle,
          .left_token = {SymbolIdentifier, "id_before_pound"},
          .right_token = {'#', "#"},
          .left_context = {},   // default context
          .right_context = {},  // default context
                                // no spaces preceding ':' in unit test context
          .expected_annotation = {1, SpacingOptions::kUndecided},
      },
      {
          .style = DefaultStyle,
          .left_token = {SymbolIdentifier, "id_before_pound"},
          .right_token = {'#', "#"},
          .left_context = {NodeEnum::kUnqualifiedId},
          .right_context = {},  // default context
          .expected_annotation = {0, SpacingOptions::kUndecided},
      },
      {
          .style = DefaultStyle,
          .left_token = {SymbolIdentifier, "id_before_pound"},
          .right_token = {'#', "#"},
          .left_context = {NodeEnum::kQualifiedId},
          .right_context = {},  // default context
          .expected_annotation = {1, SpacingOptions::kUndecided},
      },

      // '}' on the left
      {
          .style = DefaultStyle,
          .left_token = {'}', "}"},
          .right_token = {SymbolIdentifier, "id_before_open_brace"},
          .left_context = {},   // default context
          .right_context = {},  // default context
          .expected_annotation = {1, SpacingOptions::kUndecided},
      },
      {
          .style = DefaultStyle,
          .left_token = {'}', "}"},
          .right_token = {',', ","},
          .left_context = {},   // default context
          .right_context = {},  // default context
          .expected_annotation = {0, SpacingOptions::kUndecided},
      },
      {
          .style = DefaultStyle,
          .left_token = {'}', "}"},
          .right_token = {';', ";"},
          .left_context = {},   // default context
          .right_context = {},  // default context
          .expected_annotation = {0, SpacingOptions::kUndecided},
      },
      {
          .style = DefaultStyle,
          .left_token = {'}', "}"},
          .right_token = {'}', "}"},
          .left_context = {},   // default context
          .right_context = {},  // default context
          .expected_annotation = {0, SpacingOptions::kUndecided},
      },

      // '{' on the right
      {
          .style = DefaultStyle,
          .left_token = {SymbolIdentifier, "id_before_open_brace"},
          .right_token = {'{', "{"},
          .left_context = {},   // default context
          .right_context = {},  // default context
          .expected_annotation = {0, SpacingOptions::kUndecided},
      },
      {
          .style = DefaultStyle,
          .left_token = {TK_unique, "unique"},
          .right_token = {'{', "{"},
          .left_context = {},   // default context
          .right_context = {},  // default context
          .expected_annotation = {1, SpacingOptions::kUndecided},
      },
      {
          .style = DefaultStyle,
          .left_token = {TK_with, "with"},
          .right_token = {'{', "{"},
          .left_context = {},   // default context
          .right_context = {},  // default context
          .expected_annotation = {1, SpacingOptions::kUndecided},
      },
      {
          // constraint c_id {
          .style = DefaultStyle,
          .left_token = {SymbolIdentifier, "id_before_open_brace"},
          .right_token = {'{', "{"},
          .left_context = {},  // default context
          .right_context = {NodeEnum::kConstraintDeclaration,
                            NodeEnum::kBraceGroup},
          .expected_annotation = {1, SpacingOptions::kUndecided},
      },

      // ';' on the left
      {
          .style = DefaultStyle,
          .left_token = {';', ";"},
          .right_token = {SymbolIdentifier, "id_after_semi"},
          .left_context = {},   // default context
          .right_context = {},  // default context
          .expected_annotation = {1, SpacingOptions::kUndecided},
      },
      {
          .style = DefaultStyle,
          .left_token = {SemicolonEndOfAssertionVariableDeclarations, ";"},
          .right_token = {SymbolIdentifier, "id_after_semi"},
          .left_context = {},   // default context
          .right_context = {},  // default context
          .expected_annotation = {1, SpacingOptions::kUndecided},
      },

      // ';' on the right
      {
          .style = DefaultStyle,
          .left_token = {SymbolIdentifier, "id"},
          .right_token = {';', ";"},
          .left_context = {},   // default context
          .right_context = {},  // default context
          .expected_annotation = {0, SpacingOptions::kUndecided},
      },
      {
          .style = DefaultStyle,
          .left_token = {SymbolIdentifier, "id"},
          .right_token = {SemicolonEndOfAssertionVariableDeclarations, ";"},
          .left_context = {},   // default context
          .right_context = {},  // default context
          .expected_annotation = {0, SpacingOptions::kUndecided},
      },
      {
          .style = DefaultStyle,
          .left_token = {')', ")"},
          .right_token = {';', ";"},
          .left_context = {},   // default context
          .right_context = {},  // default context
          .expected_annotation =
              {0, SpacingOptions::kUndecided},  // could be MustAppend too
      },
      {
          .style = DefaultStyle,
          .left_token = {')', ")"},
          .right_token = {SemicolonEndOfAssertionVariableDeclarations, ";"},
          .left_context = {},   // default context
          .right_context = {},  // default context
          .expected_annotation =
              {0, SpacingOptions::kUndecided},  // could be MustAppend too
      },

      // keyword on right
      {
          .style = DefaultStyle,
          .left_token = {TK_DecNumber, "1"},
          .right_token = {TK_begin, "begin"},
          .left_context = {},   // default context
          .right_context = {},  // default context
          .expected_annotation = {1, SpacingOptions::kUndecided},
      },
      {
          .style = DefaultStyle,
          .left_token = {TK_begin, "begin"},
          .right_token = {TK_begin, "begin"},
          .left_context = {},   // default context
          .right_context = {},  // default context
          .expected_annotation = {1, SpacingOptions::kUndecided},
      },
      {
          .style = DefaultStyle,
          .left_token = {TK_begin, "begin"},
          .right_token = {TK_end, "end"},
          .left_context = {},   // default context
          .right_context = {},  // default context
          .expected_annotation = {1, SpacingOptions::kMustWrap},
      },
      {
          .style = DefaultStyle,
          .left_token = {TK_end, "end"},
          .right_token = {TK_begin, "begin"},
          .left_context = {},   // default context
          .right_context = {},  // default context
          .expected_annotation = {1, SpacingOptions::kUndecided},
      },
      {
          .style = DefaultStyle,
          .left_token = {TK_end, "end"},
          .right_token = {TK_else, "else"},
          .left_context = {},   // default context
          .right_context = {},  // default context
          .expected_annotation = {1, SpacingOptions::kMustAppend},
      },
      {
          .style = DefaultStyle,
          .left_token = {'}', "}"},
          .right_token = {TK_else, "else"},
          .left_context = {},   // default context
          .right_context = {},  // default context
          .expected_annotation = {1, SpacingOptions::kMustAppend},
      },
      {
          .style = DefaultStyle,
          .left_token = {';', ";"},
          .right_token = {TK_else, "else"},
          .left_context = {},   // default context
          .right_context = {},  // default context
          .expected_annotation = {1, SpacingOptions::kMustWrap},
      },
      {
          .style = DefaultStyle,
          .left_token = {TK_default, "default"},
          .right_token = {TK_clocking, "clocking"},
          .left_context = {},   // default context
          .right_context = {},  // default context
          .expected_annotation = {1, SpacingOptions::kUndecided},
      },
      {
          .style = DefaultStyle,
          .left_token = {TK_default, "default"},
          .right_token = {TK_disable, "disable"},
          .left_context = {},   // default context
          .right_context = {},  // default context
          .expected_annotation = {1, SpacingOptions::kUndecided},
      },
      {
          .style = DefaultStyle,
          .left_token = {TK_disable, "disable"},
          .right_token = {TK_iff, "iff"},
          .left_context = {},   // default context
          .right_context = {},  // default context
          .expected_annotation = {1, SpacingOptions::kUndecided},
      },
      {
          .style = DefaultStyle,
          .left_token = {TK_disable, "disable"},
          .right_token = {TK_soft, "soft"},
          .left_context = {},   // default context
          .right_context = {},  // default context
          .expected_annotation = {1, SpacingOptions::kUndecided},
      },
      {
          .style = DefaultStyle,
          .left_token = {TK_extern, "extern"},
          .right_token = {TK_forkjoin, "forkjoin"},
          .left_context = {},   // default context
          .right_context = {},  // default context
          .expected_annotation = {1, SpacingOptions::kUndecided},
      },
      {
          .style = DefaultStyle,
          .left_token = {TK_input, "input"},
          .right_token = {TK_logic, "logic"},
          .left_context = {},   // default context
          .right_context = {},  // default context
          .expected_annotation = {1, SpacingOptions::kUndecided},
      },
      {
          .style = DefaultStyle,
          .left_token = {TK_var, "var"},
          .right_token = {TK_logic, "logic"},
          .left_context = {},   // default context
          .right_context = {},  // default context
          .expected_annotation = {1, SpacingOptions::kUndecided},
      },
      {
          .style = DefaultStyle,
          .left_token = {TK_output, "output"},
          .right_token = {TK_reg, "reg"},
          .left_context = {},   // default context
          .right_context = {},  // default context
          .expected_annotation = {1, SpacingOptions::kUndecided},
      },
      {
          .style = DefaultStyle,
          .left_token = {TK_static, "static"},
          .right_token = {TK_constraint, "constraint"},
          .left_context = {},   // default context
          .right_context = {},  // default context
          .expected_annotation = {1, SpacingOptions::kUndecided},
      },
      {
          .style = DefaultStyle,
          .left_token = {TK_parameter, "parameter"},
          .right_token = {TK_type, "type"},
          .left_context = {},   // default context
          .right_context = {},  // default context
          .expected_annotation = {1, SpacingOptions::kUndecided},
      },
      {
          .style = DefaultStyle,
          .left_token = {TK_virtual, "virtual"},
          .right_token = {TK_interface, "interface"},
          .left_context = {},   // default context
          .right_context = {},  // default context
          .expected_annotation = {1, SpacingOptions::kUndecided},
      },
      {
          .style = DefaultStyle,
          .left_token = {TK_const, "const"},
          .right_token = {TK_ref, "ref"},
          .left_context = {},   // default context
          .right_context = {},  // default context
          .expected_annotation = {1, SpacingOptions::kUndecided},
      },
      {
          .style = DefaultStyle,
          .left_token = {TK_union, "union"},
          .right_token = {TK_tagged, "tagged"},
          .left_context = {},   // default context
          .right_context = {},  // default context
          .expected_annotation = {1, SpacingOptions::kUndecided},
      },
      {
          .style = DefaultStyle,
          .left_token = {';', ";"},
          .right_token = {TK_end, "end"},
          .left_context = {},   // default context
          .right_context = {},  // default context
          .expected_annotation = {1, SpacingOptions::kMustWrap},
      },
      {
          .style = DefaultStyle,
          .left_token = {';', ";"},
          .right_token = {TK_endfunction, "endfunction"},
          .left_context = {},   // default context
          .right_context = {},  // default context
          .expected_annotation = {1, SpacingOptions::kMustWrap},
      },
      {
          .style = DefaultStyle,
          .left_token = {';', ";"},
          .right_token = {TK_endtask, "endtask"},
          .left_context = {},   // default context
          .right_context = {},  // default context
          .expected_annotation = {1, SpacingOptions::kMustWrap},
      },
      {
          .style = DefaultStyle,
          .left_token = {';', ";"},
          .right_token = {TK_endclass, "endclass"},
          .left_context = {},   // default context
          .right_context = {},  // default context
          .expected_annotation = {1, SpacingOptions::kMustWrap},
      },
      {
          .style = DefaultStyle,
          .left_token = {';', ";"},
          .right_token = {TK_endpackage, "endpackage"},
          .left_context = {},   // default context
          .right_context = {},  // default context
          .expected_annotation = {1, SpacingOptions::kMustWrap},
      },
      {
          .style = DefaultStyle,
          .left_token = {SymbolIdentifier, "nettype_id"},
          .right_token = {TK_with, "with"},
          .left_context = {},   // default context
          .right_context = {},  // default context
          .expected_annotation = {1, SpacingOptions::kUndecided},
      },
      {
          .style = DefaultStyle,
          .left_token = {SymbolIdentifier, "id"},
          .right_token = {TK_until, "until"},
          .left_context = {},   // default context
          .right_context = {},  // default context
          .expected_annotation = {1, SpacingOptions::kUndecided},
      },
      {
          .style = DefaultStyle,
          .left_token = {',', ","},
          .right_token = {TK_highz0, "highz0"},
          .left_context = {},   // default context
          .right_context = {},  // default context
          .expected_annotation = {1, SpacingOptions::kUndecided},
      },
      {
          .style = DefaultStyle,
          .left_token = {',', ","},
          .right_token = {TK_highz1, "highz1"},
          .left_context = {},   // default context
          .right_context = {},  // default context
          .expected_annotation = {1, SpacingOptions::kUndecided},
      },
      // Entries spacing in primitives
      {
          // 1 0 : ? : -;
          .style = DefaultStyle,
          .left_token = {'1', "1"},
          .right_token = {'0', "0"},
          .left_context = {},  // default context
          .right_context = {NodeEnum::kUdpSequenceEntry},
          .expected_annotation = {1, SpacingOptions::kUndecided},
      },
      {
          // 1 0 : ? : -;
          .style = DefaultStyle,
          .left_token = {'0', "0"},
          .right_token = {':', ":"},
          .left_context = {},  // default context
          .right_context = {NodeEnum::kUdpSequenceEntry},
          .expected_annotation = {1, SpacingOptions::kUndecided},
      },
      {
          // 1 0 : ? : -;
          .style = DefaultStyle,
          .left_token = {':', ":"},
          .right_token = {'?', "?"},
          .left_context = {},  // default context
          .right_context = {NodeEnum::kUdpSequenceEntry},
          .expected_annotation = {1, SpacingOptions::kUndecided},
      },
      {
          // 1 0 : ? : -;
          .style = DefaultStyle,
          .left_token = {'?', "?"},
          .right_token = {':', ":"},
          .left_context = {},  // default context
          .right_context = {NodeEnum::kUdpSequenceEntry},
          .expected_annotation = {1, SpacingOptions::kUndecided},
      },
      {
          // 1 0 : ? : -;
          .style = DefaultStyle,
          .left_token = {':', ":"},
          .right_token = {'-', "-"},
          .left_context = {},  // default context
          .right_context = {NodeEnum::kUdpSequenceEntry},
          .expected_annotation = {1, SpacingOptions::kUndecided},
      },
      {
          // 1 0 : ? : -;
          .style = DefaultStyle,
          .left_token = {'-', "-"},
          .right_token = {';', ";"},
          .left_context = {},  // default context
          .right_context = {NodeEnum::kUdpSequenceEntry},
          .expected_annotation = {0, SpacingOptions::kUndecided},
      },
      {
          // 1 0 : -;
          .style = DefaultStyle,
          .left_token = {'1', "1"},
          .right_token = {'0', "0"},
          .left_context = {},  // default context
          .right_context = {NodeEnum::kUdpCombEntry},
          .expected_annotation = {1, SpacingOptions::kUndecided},
      },
      {
          // 1 0 : -;
          .style = DefaultStyle,
          .left_token = {'0', "0"},
          .right_token = {':', ":"},
          .left_context = {},  // default context
          .right_context = {NodeEnum::kUdpCombEntry},
          .expected_annotation = {1, SpacingOptions::kUndecided},
      },
      {
          // 1 0 : -;
          .style = DefaultStyle,
          .left_token = {':', ":"},
          .right_token = {'-', "-"},
          .left_context = {},  // default context
          .right_context = {NodeEnum::kUdpCombEntry},
          .expected_annotation = {1, SpacingOptions::kUndecided},
      },
      {
          // 1 0 : -;
          .style = DefaultStyle,
          .left_token = {'-', "-"},
          .right_token = {';', ";"},
          .left_context = {},  // default context
          .right_context = {NodeEnum::kUdpCombEntry},
          .expected_annotation = {0, SpacingOptions::kUndecided},
      },

      // time literals
      {
          // #1ps
          .style = DefaultStyle,
          .left_token = {'#', "#"},
          .right_token = {verilog_tokentype::TK_TimeLiteral, "1ps"},
          .left_context = {/* any context */},
          .right_context = {/* any context */},
          .expected_annotation = {0, SpacingOptions::kMustAppend},
      },
      {
          // #1ps;
          .style = DefaultStyle,
          .left_token = {verilog_tokentype::TK_TimeLiteral, "1ps"},
          .right_token = {';', ";"},
          .left_context = {/* any context */},
          .right_context = {/* any context */},
          .expected_annotation = {0, SpacingOptions::kMustAppend},
      },
      {
          .style = DefaultStyle,
          .left_token = {verilog_tokentype::TK_TimeLiteral, "1ps"},
          .right_token = {verilog_tokentype::SymbolIdentifier, "task_call"},
          .left_context = {/* any context */},
          .right_context = {/* any context */},
          .expected_annotation = {1, SpacingOptions::kUndecided},
      },
      {
          .style = DefaultStyle,
          .left_token = {verilog_tokentype::TK_TimeLiteral, "1ps"},
          .right_token = {verilog_tokentype::MacroIdentifier, "`MACRO"},
          .left_context = {/* any context */},
          .right_context = {/* any context */},
          .expected_annotation = {1, SpacingOptions::kUndecided},
      },
      {
          .style = DefaultStyle,
          .left_token = {verilog_tokentype::TK_TimeLiteral, "100ps"},
          .right_token = {verilog_tokentype::MacroCallId, "`MACRO"},
          .left_context = {/* any context */},
          .right_context = {/* any context */},
          .expected_annotation = {1, SpacingOptions::kUndecided},
      },
      {
          .style = DefaultStyle,
          .left_token = {verilog_tokentype::TK_TimeLiteral, "1ps"},
          .right_token = {'#', "#"},
          .left_context = {/* any context */},
          .right_context = {/* any context */},
          .expected_annotation = {1, SpacingOptions::kUndecided},
      },
      {
          .style = DefaultStyle,
          .left_token = {verilog_tokentype::TK_TimeLiteral, "1ps"},
          .right_token = {verilog_tokentype::TK_INCR, "++"},
          .left_context = {/* any context */},
          .right_context = {/* any context */},
          .expected_annotation = {1, SpacingOptions::kUndecided},
      },
      {
          .style = DefaultStyle,
          .left_token = {verilog_tokentype::TK_TimeLiteral, "1ps"},
          .right_token = {verilog_tokentype::TK_DECR, "--"},
          .left_context = {/* any context */},
          .right_context = {/* any context */},
          .expected_annotation = {1, SpacingOptions::kUndecided},
      },
      {
          .style = DefaultStyle,
          .left_token = {verilog_tokentype::TK_TimeLiteral, "1ps"},
          .right_token = {'@', "@"},
          .left_context = {/* any context */},
          .right_context = {/* any context */},
          .expected_annotation = {1, SpacingOptions::kUndecided},
      },
      {
          .style = DefaultStyle,
          .left_token = {verilog_tokentype::TK_TimeLiteral, "1ps"},
          .right_token = {verilog_tokentype::TK_begin, "begin"},
          .left_context = {/* any context */},
          .right_context = {/* any context */},
          .expected_annotation = {1, SpacingOptions::kUndecided},
      },
      {
          .style = DefaultStyle,
          .left_token = {verilog_tokentype::TK_TimeLiteral, "1ps"},
          .right_token = {verilog_tokentype::TK_force, "force"},
          .left_context = {/* any context */},
          .right_context = {/* any context */},
          .expected_annotation = {1, SpacingOptions::kUndecided},
      },
      {
          .style = DefaultStyle,
          .left_token = {verilog_tokentype::TK_TimeLiteral, "1ps"},
          .right_token = {verilog_tokentype::TK_output, "output"},
          .left_context = {/* any context */},
          .right_context = {/* any context */},
          .expected_annotation = {1, SpacingOptions::kUndecided},
      },
      {
          // ... / 1ps
          .style = DefaultStyle,
          .left_token = {'/', "/"},
          .right_token = {verilog_tokentype::TK_TimeLiteral, "1ps"},
          .left_context = {/* any context */},
          .right_context = {/* any context */},
          .expected_annotation = {1, SpacingOptions::kUndecided},
      },
      {
          // 1ps / ...
          .style = DefaultStyle,
          .left_token = {verilog_tokentype::TK_TimeLiteral, "1ps"},
          .right_token = {'/', "/"},
          .left_context = {/* any context */},
          .right_context = {/* any context */},
          .expected_annotation = {1, SpacingOptions::kUndecided},
      },
      {
          .style = DefaultStyle,
          .left_token = {verilog_tokentype::TK_EOL_COMMENT, "//comment"},
          .right_token = {verilog_tokentype::TK_LINE_CONT, "\\"},
          .left_context = {/* any context */},
          .right_context = {/* any context */},
          .expected_annotation = {0, SpacingOptions::kMustAppend},
      },
      {
          .style = DefaultStyle,
          .left_token = {verilog_tokentype::SymbolIdentifier, "id"},
          .right_token = {verilog_tokentype::TK_LINE_CONT, "\\"},
          .left_context = {/* any context */},
          .right_context = {/* any context */},
          .expected_annotation = {0, SpacingOptions::kMustAppend},
      },
      {
          .style = DefaultStyle,
          .left_token = {verilog_tokentype::EscapedIdentifier, "\\id.id[9]"},
          .right_token = {verilog_tokentype::TK_LINE_CONT, "\\"},
          .left_context = {/* any context */},
          .right_context = {/* any context */},
          .expected_annotation = {1, SpacingOptions::kMustAppend},
      },
      {
          .style = DefaultStyle,
          .left_token = {verilog_tokentype::TK_DecNumber, "77"},
          .right_token = {verilog_tokentype::TK_LINE_CONT, "\\"},
          .left_context = {/* any context */},
          .right_context = {/* any context */},
          .expected_annotation = {0, SpacingOptions::kMustAppend},
      },
      {
          .style = DefaultStyle,
          .left_token = {')', ")"},
          .right_token = {verilog_tokentype::TK_LINE_CONT, "\\"},
          .left_context = {/* any context */},
          .right_context = {/* any context */},
          .expected_annotation = {0, SpacingOptions::kMustAppend},
      },
      {
          .style = DefaultStyle,
          .left_token = {'}', "}"},
          .right_token = {verilog_tokentype::TK_LINE_CONT, "\\"},
          .left_context = {/* any context */},
          .right_context = {/* any context */},
          .expected_annotation = {0, SpacingOptions::kMustAppend},
      },
      {
          .style = DefaultStyle,
          .left_token = {']', "]"},
          .right_token = {verilog_tokentype::TK_LINE_CONT, "\\"},
          .left_context = {/* any context */},
          .right_context = {/* any context */},
          .expected_annotation = {0, SpacingOptions::kMustAppend},
      },
      {
          .style = DefaultStyle,
          .left_token = {verilog_tokentype::TK_LINE_CONT, "\\"},
          .right_token = {verilog_tokentype::SymbolIdentifier, "id"},
          .left_context = {/* any context */},
          .right_context = {/* any context */},
          .expected_annotation = {0, SpacingOptions::kMustWrap},
      },
      // Space between return keyword and return value
      {
          .style = DefaultStyle,
          .left_token = {verilog_tokentype::TK_return, "return"},
          .right_token = {'{', "{"},
          .left_context = {/* unspecified context */},
          .right_context = {/* unspecified context */},
          .expected_annotation = {1, SpacingOptions::kUndecided},
      },
      {
          .style = DefaultStyle,
          .left_token = {verilog_tokentype::TK_return, "return"},
          .right_token = {'(', "("},
          .left_context = {/* unspecified context */},
          .right_context = {/* unspecified context */},
          .expected_annotation = {1, SpacingOptions::kUndecided},
      },
      {
          .style = DefaultStyle,
          .left_token = {verilog_tokentype::TK_return, "return"},
          .right_token = {'-', "-"},
          .left_context = {/* unspecified context */},
          .right_context = {/* unspecified context */},
          .expected_annotation = {1, SpacingOptions::kUndecided},
      },
      {
          .style = DefaultStyle,
          .left_token = {verilog_tokentype::TK_return, "return"},
          .right_token = {'!', "!"},
          .left_context = {/* unspecified context */},
          .right_context = {/* unspecified context */},
          .expected_annotation = {1, SpacingOptions::kUndecided},
      },
      {
          .style = DefaultStyle,
          .left_token = {verilog_tokentype::TK_return, "return"},
          .right_token = {'~', "~"},
          .left_context = {/* unspecified context */},
          .right_context = {/* unspecified context */},
          .expected_annotation = {1, SpacingOptions::kUndecided},
      },
      {
          .style = DefaultStyle,
          .left_token = {verilog_tokentype::TK_return, "return"},
          .right_token = {verilog_tokentype::SystemTFIdentifier, "$foo"},
          .left_context = {/* unspecified context */},
          .right_context = {/* unspecified context */},
          .expected_annotation = {1, SpacingOptions::kUndecided},
      },
      // '/' between identifiers is a path separator in macro args (#2352),
      // but remains a binary operator in other contexts.
      {
          .style = DefaultStyle,
          .left_token = {verilog_tokentype::MacroIdentifier, "`PATH"},
          .right_token = {'/', "/"},
          .left_context = {NodeEnum::kMacroArgList, NodeEnum::kMacroCall},
          .right_context = {NodeEnum::kMacroArgList, NodeEnum::kMacroCall},
          .expected_annotation = {0, SpacingOptions::kUndecided},
      },
      {
          .style = DefaultStyle,
          .left_token = {'/', "/"},
          .right_token = {verilog_tokentype::SymbolIdentifier, "src"},
          .left_context = {NodeEnum::kMacroArgList, NodeEnum::kMacroCall},
          .right_context = {NodeEnum::kMacroArgList, NodeEnum::kMacroCall},
          .expected_annotation = {0, SpacingOptions::kUndecided},
      },
      {
          .style = DefaultStyle,
          .left_token = {verilog_tokentype::SymbolIdentifier, "src"},
          .right_token = {'/', "/"},
          .left_context = {NodeEnum::kMacroArgList, NodeEnum::kMacroCall},
          .right_context = {NodeEnum::kMacroArgList, NodeEnum::kMacroCall},
          .expected_annotation = {0, SpacingOptions::kUndecided},
      },
      {
          .style = DefaultStyle,
          .left_token = {verilog_tokentype::SymbolIdentifier, "a"},
          .right_token = {'/', "/"},
          .left_context = {/* expression, not a macro argument */},
          .right_context = {/* expression, not a macro argument */},
          .expected_annotation = {1, SpacingOptions::kUndecided},
      },
      {
          .style = DefaultStyle,
          .left_token = {'/', "/"},
          .right_token = {verilog_tokentype::SymbolIdentifier, "b"},
          .left_context = {/* expression, not a macro argument */},
          .right_context = {/* expression, not a macro argument */},
          .expected_annotation = {1, SpacingOptions::kUndecided},
      },
  };
  int test_index = 0;
  for (const auto &test_case : kTestCases) {
    VLOG(1) << "test_index[" << test_index << "]:";
    PreFormatToken left(&test_case.left_token);
    PreFormatToken right(&test_case.right_token);
    // Classify token type into major category
    left.format_token_enum =
        GetFormatTokenType(verilog_tokentype(left.TokenEnum()));
    right.format_token_enum =
        GetFormatTokenType(verilog_tokentype(right.TokenEnum()));

    ASSERT_TRUE(right.format_token_enum != FormatTokenType::eol_comment &&
                left.format_token_enum != FormatTokenType::comment_block &&
                right.format_token_enum != FormatTokenType::comment_block)
        << "This test does not support cases examining intertoken text. "
           "Move the test case to OriginalSpacingSensitiveTests instead.";

    VLOG(1) << "left context: " << test_case.left_context;
    VLOG(1) << "right context: " << test_case.right_context;
    AnnotateFormatToken(test_case.style, left, &right, test_case.left_context,
                        test_case.right_context);
    EXPECT_EQ(test_case.expected_annotation, right.before)
        << " with left=" << left.Text() << " and right=" << right.Text();
    ++test_index;
  }
}  // NOLINT(readability/fn_size)

struct OriginalSpacingSensitiveTestCase {
  FormatStyle style;

  // TODO(fangism): group this into a TokenInfo.
  int left_token_enum;
  std::string_view left_token_string;

  // This spacing may influence token-annotation behavior.
  std::string_view whitespace_between;

  // TODO(fangism): group this into a TokenInfo.
  int right_token_enum;
  std::string_view right_token_string;

  InitializedSyntaxTreeContext left_context;
  InitializedSyntaxTreeContext right_context;

  ExpectedInterTokenInfo expected_annotation;
};

static const auto CompactIndexSelectionStyle = []() {
  auto style = DefaultStyle;
  style.compact_indexing_and_selections = false;
  return style;
}();

// These tests are allowed to be sensitive to original inter-token spacing.
TEST(TokenAnnotatorTest, OriginalSpacingSensitiveTests) {
  static const OriginalSpacingSensitiveTestCase kTestCases[] = {
      {// No comments
       .style = DefaultStyle,
       .left_token_enum = '=',  // left token
       .left_token_string = "=",
       .whitespace_between = "   ",  // whitespace between
       .right_token_enum = verilog_tokentype::TK_DecNumber,  // right token
       .right_token_string = "0",
       .left_context = {/* unspecified context */},
       .right_context = {/* unspecified context */},
       .expected_annotation = {1, SpacingOptions::kUndecided}},
      {
          .style = DefaultStyle,
          .left_token_enum = TK_COMMENT_BLOCK,
          .left_token_string = "/*comment*/",
          .whitespace_between = "",
          .right_token_enum = verilog_tokentype::MacroCallId,
          .right_token_string = "`uvm_foo_macro",
          .left_context = {},   // any context
          .right_context = {},  // any context
          .expected_annotation = {1, SpacingOptions::kUndecided},
      },
      {
          .style = DefaultStyle,
          .left_token_enum = TK_COMMENT_BLOCK,
          .left_token_string = "/*comment*/",
          .whitespace_between = "",
          .right_token_enum = verilog_tokentype::MacroIdentifier,
          .right_token_string = "`uvm_foo_id",
          .left_context = {},   // any context
          .right_context = {},  // any context
          .expected_annotation = {1, SpacingOptions::kUndecided},
      },
      {
          .style = DefaultStyle,
          .left_token_enum = verilog_tokentype::TK_COMMENT_BLOCK,
          .left_token_string = "/*comment*/",
          .whitespace_between = "",
          .right_token_enum = verilog_tokentype::TK_LINE_CONT,
          .right_token_string = "\\",
          .left_context = {/* any context */},
          .right_context = {/* any context */},
          .expected_annotation = {0, SpacingOptions::kMustAppend},
      },
      {// //comment1
       // //comment2
       .style = DefaultStyle,
       .left_token_enum = verilog_tokentype::TK_EOL_COMMENT,
       .left_token_string = "//comment1",
       .whitespace_between = "\n",
       .right_token_enum = verilog_tokentype::TK_EOL_COMMENT,
       .right_token_string = "//comment2",
       .left_context = {},
       .right_context = {},
       .expected_annotation = {2, SpacingOptions::kMustWrap}},
      {// 0 // comment
       .style = DefaultStyle,
       .left_token_enum = verilog_tokentype::TK_DecNumber,
       .left_token_string = "0",
       .whitespace_between = "   ",
       .right_token_enum = verilog_tokentype::TK_EOL_COMMENT,
       .right_token_string = "// comment",
       .left_context = {/* unspecified context */},
       .right_context = {/* unspecified context */},
       .expected_annotation = {2, SpacingOptions::kMustAppend}},
      {// 0// comment
       .style = DefaultStyle,
       .left_token_enum = verilog_tokentype::TK_DecNumber,
       .left_token_string = "0",
       .whitespace_between = "",
       .right_token_enum = verilog_tokentype::TK_EOL_COMMENT,
       .right_token_string = "// comment",
       .left_context = {/* unspecified context */},
       .right_context = {/* unspecified context */},
       .expected_annotation = {2, SpacingOptions::kMustAppend}},
      {// 0 \n  // comment
       .style = DefaultStyle,
       .left_token_enum = verilog_tokentype::TK_DecNumber,
       .left_token_string = "0",
       .whitespace_between = " \n  ",
       .right_token_enum = verilog_tokentype::TK_EOL_COMMENT,
       .right_token_string = "// comment",
       .left_context = {/* unspecified context */},
       .right_context = {/* unspecified context */},
       .expected_annotation = {2, SpacingOptions::kUndecided}},
      {// // comment 1 \n  // comment 2
       .style = DefaultStyle,
       .left_token_enum = verilog_tokentype::TK_EOL_COMMENT,
       .left_token_string = "// comment 1",
       .whitespace_between = " \n  ",
       .right_token_enum = verilog_tokentype::TK_EOL_COMMENT,
       .right_token_string = "// comment 2",
       .left_context = {/* unspecified context */},
       .right_context = {/* unspecified context */},
       .expected_annotation = {2, SpacingOptions::kMustWrap}},
      {// /* comment 1 */ \n  // comment 2
       .style = DefaultStyle,
       .left_token_enum = verilog_tokentype::TK_COMMENT_BLOCK,
       .left_token_string = "/* comment 1 */",
       .whitespace_between = " \n  ",
       .right_token_enum = verilog_tokentype::TK_EOL_COMMENT,
       .right_token_string = "// comment 2",
       .left_context = {/* unspecified context */},
       .right_context = {/* unspecified context */},
       .expected_annotation = {2, SpacingOptions::kMustWrap}},
      {// /* comment 1 */  // comment 2
       .style = DefaultStyle,
       .left_token_enum = verilog_tokentype::TK_COMMENT_BLOCK,
       .left_token_string = "/* comment 1 */",
       .whitespace_between = " ",
       .right_token_enum = verilog_tokentype::TK_EOL_COMMENT,
       .right_token_string = "// comment 2",
       .left_context = {/* unspecified context */},
       .right_context = {/* unspecified context */},
       .expected_annotation = {2, SpacingOptions::kMustAppend}},
      {// ;  // comment 2
       .style = DefaultStyle,
       .left_token_enum = ';',
       .left_token_string = ";",
       .whitespace_between = " ",
       .right_token_enum = verilog_tokentype::TK_EOL_COMMENT,
       .right_token_string = "// comment 2",
       .left_context = {/* unspecified context */},
       .right_context = {/* unspecified context */},
       .expected_annotation = {2, SpacingOptions::kMustAppend}},
      {// ; \n // comment 2
       .style = DefaultStyle,
       .left_token_enum = ';',
       .left_token_string = ";",
       .whitespace_between = " \n",
       .right_token_enum = verilog_tokentype::TK_EOL_COMMENT,
       .right_token_string = "// comment 2",
       .left_context = {/* unspecified context */},
       .right_context = {/* unspecified context */},
       .expected_annotation = {2, SpacingOptions::kUndecided}},
      {// ,  // comment 2
       .style = DefaultStyle,
       .left_token_enum = ',',
       .left_token_string = ",",
       .whitespace_between = " ",
       .right_token_enum = verilog_tokentype::TK_EOL_COMMENT,
       .right_token_string = "// comment 2",
       .left_context = {/* unspecified context */},
       .right_context = {/* unspecified context */},
       .expected_annotation = {2, SpacingOptions::kMustAppend}},
      {// , \n // comment 2
       .style = DefaultStyle,
       .left_token_enum = ',',
       .left_token_string = ",",
       .whitespace_between = "\n ",
       .right_token_enum = verilog_tokentype::TK_EOL_COMMENT,
       .right_token_string = "// comment 2",
       .left_context = {/* unspecified context */},
       .right_context = {/* unspecified context */},
       .expected_annotation = {2, SpacingOptions::kUndecided}},
      {// begin  // comment 2
       .style = DefaultStyle,
       .left_token_enum = verilog_tokentype::TK_begin,
       .left_token_string = "begin",
       .whitespace_between = " ",
       .right_token_enum = verilog_tokentype::TK_EOL_COMMENT,
       .right_token_string = "// comment 2",
       .left_context = {/* unspecified context */},
       .right_context = {/* unspecified context */},
       .expected_annotation = {2, SpacingOptions::kMustAppend}},
      {// begin \n // comment 2
       .style = DefaultStyle,
       .left_token_enum = verilog_tokentype::TK_begin,
       .left_token_string = "begin",
       .whitespace_between = "\n",
       .right_token_enum = verilog_tokentype::TK_EOL_COMMENT,
       .right_token_string = "// comment 2",
       .left_context = {/* unspecified context */},
       .right_context = {/* unspecified context */},
       .expected_annotation = {2, SpacingOptions::kUndecided}},
      {// else  // comment 2
       .style = DefaultStyle,
       .left_token_enum = verilog_tokentype::TK_else,
       .left_token_string = "else",
       .whitespace_between = " ",
       .right_token_enum = verilog_tokentype::TK_EOL_COMMENT,
       .right_token_string = "// comment 2",
       .left_context = {/* unspecified context */},
       .right_context = {/* unspecified context */},
       .expected_annotation = {2, SpacingOptions::kMustAppend}},
      {// else \n // comment 2
       .style = DefaultStyle,
       .left_token_enum = verilog_tokentype::TK_else,
       .left_token_string = "else",
       .whitespace_between = " \n  ",
       .right_token_enum = verilog_tokentype::TK_EOL_COMMENT,
       .right_token_string = "// comment 2",
       .left_context = {/* unspecified context */},
       .right_context = {/* unspecified context */},
       .expected_annotation = {2, SpacingOptions::kUndecided}},
      {// end  // comment 2
       .style = DefaultStyle,
       .left_token_enum = verilog_tokentype::TK_end,
       .left_token_string = "end",
       .whitespace_between = " ",
       .right_token_enum = verilog_tokentype::TK_EOL_COMMENT,
       .right_token_string = "// comment 2",
       .left_context = {/* unspecified context */},
       .right_context = {/* unspecified context */},
       .expected_annotation = {2, SpacingOptions::kMustAppend}},
      {// end \n // comment 2
       .style = DefaultStyle,
       .left_token_enum = verilog_tokentype::TK_end,
       .left_token_string = "end",
       .whitespace_between = "  \n ",
       .right_token_enum = verilog_tokentype::TK_EOL_COMMENT,
       .right_token_string = "// comment 2",
       .left_context = {/* unspecified context */},
       .right_context = {/* unspecified context */},
       .expected_annotation = {2, SpacingOptions::kUndecided}},
      {// generate  // comment 2
       .style = DefaultStyle,
       .left_token_enum = verilog_tokentype::TK_generate,
       .left_token_string = "generate",
       .whitespace_between = " ",
       .right_token_enum = verilog_tokentype::TK_EOL_COMMENT,
       .right_token_string = "// comment 2",
       .left_context = {/* unspecified context */},
       .right_context = {/* unspecified context */},
       .expected_annotation = {2, SpacingOptions::kMustAppend}},
      {// generate \n // comment 2
       .style = DefaultStyle,
       .left_token_enum = verilog_tokentype::TK_generate,
       .left_token_string = "generate",
       .whitespace_between = "  \n",
       .right_token_enum = verilog_tokentype::TK_EOL_COMMENT,
       .right_token_string = "// comment 2",
       .left_context = {/* unspecified context */},
       .right_context = {/* unspecified context */},
       .expected_annotation = {2, SpacingOptions::kUndecided}},
      {// if  // comment 2
       .style = DefaultStyle,
       .left_token_enum = verilog_tokentype::TK_if,
       .left_token_string = "if",
       .whitespace_between = " ",
       .right_token_enum = verilog_tokentype::TK_EOL_COMMENT,
       .right_token_string = "// comment 2",
       .left_context = {/* unspecified context */},
       .right_context = {/* unspecified context */},
       .expected_annotation = {2, SpacingOptions::kMustAppend}},
      {// if \n\n // comment 2
       .style = DefaultStyle,
       .left_token_enum = verilog_tokentype::TK_if,
       .left_token_string = "if",
       .whitespace_between = " \n\n ",
       .right_token_enum = verilog_tokentype::TK_EOL_COMMENT,
       .right_token_string = "// comment 2",
       .left_context = {/* unspecified context */},
       .right_context = {/* unspecified context */},
       .expected_annotation = {2, SpacingOptions::kUndecided}},
      {
          .style = DefaultStyle,
          .left_token_enum = verilog_tokentype::TK_LINE_CONT,
          .left_token_string = "\\",
          .whitespace_between = "\n",
          .right_token_enum = verilog_tokentype::TK_EOL_COMMENT,
          .right_token_string = "//comment",
          .left_context = {/* any context */},
          .right_context = {/* any context */},
          .expected_annotation = {0, SpacingOptions::kMustWrap},
      },
      {
          .style = DefaultStyle,
          .left_token_enum = verilog_tokentype::TK_LINE_CONT,
          .left_token_string = "\\",
          .whitespace_between = "\n",
          .right_token_enum = verilog_tokentype::TK_COMMENT_BLOCK,
          .right_token_string = "/*comment*/",
          .left_context = {/* any context */},
          .right_context = {/* any context */},
          .expected_annotation = {0, SpacingOptions::kMustWrap},
      },
      {
          .style = DefaultStyle,
          .left_token_enum = verilog_tokentype::MacroCallCloseToEndLine,
          .left_token_string = ")",
          .whitespace_between = " ",
          .right_token_enum = verilog_tokentype::TK_COMMENT_BLOCK,
          .right_token_string = "/*comment*/",
          .left_context = {/* unspecified context */},
          .right_context = {/* unspecified context */},
          .expected_annotation =
              {2, SpacingOptions::kUndecided},  // could be append
      },
      {
          .style = DefaultStyle,
          .left_token_enum = verilog_tokentype::MacroCallCloseToEndLine,
          .left_token_string = ")",
          .whitespace_between = "\n",
          .right_token_enum = verilog_tokentype::TK_COMMENT_BLOCK,
          .right_token_string = "/*comment*/",
          .left_context = {/* unspecified context */},
          .right_context = {/* unspecified context */},
          .expected_annotation = {2, SpacingOptions::kMustWrap},
      },
      {
          .style = DefaultStyle,
          .left_token_enum = verilog_tokentype::MacroCallCloseToEndLine,
          .left_token_string = ")",
          .whitespace_between = " ",
          .right_token_enum = verilog_tokentype::TK_EOL_COMMENT,
          .right_token_string = "//comment",
          .left_context = {/* unspecified context */},
          .right_context = {/* unspecified context */},
          .expected_annotation = {2, SpacingOptions::kMustAppend},
      },
      {
          .style = DefaultStyle,
          .left_token_enum = verilog_tokentype::MacroCallCloseToEndLine,
          .left_token_string = ")",
          .whitespace_between = "\n",
          .right_token_enum = verilog_tokentype::TK_EOL_COMMENT,
          .right_token_string = "//comment",
          .left_context = {/* unspecified context */},
          .right_context = {/* unspecified context */},
          .expected_annotation = {2, SpacingOptions::kUndecided},
      },
      // Comments in UDP entries
      {
          // 1  /*comment*/ 0 : -;
          .style = DefaultStyle,
          .left_token_enum = '1',
          .left_token_string = "1",
          .whitespace_between = "",
          .right_token_enum = verilog_tokentype::TK_COMMENT_BLOCK,
          .right_token_string = "/* comment */",
          .left_context = {NodeEnum::kUdpCombEntry},
          .right_context = {NodeEnum::kUdpCombEntry},
          .expected_annotation = {2, SpacingOptions::kUndecided},
      },
      {
          // 1  /*comment*/ 0 : -;
          .style = DefaultStyle,
          .left_token_enum = verilog_tokentype::TK_COMMENT_BLOCK,
          .left_token_string = "/* comment */",
          .whitespace_between = "",
          .right_token_enum = '0',
          .right_token_string = "0",
          .left_context = {NodeEnum::kUdpCombEntry},
          .right_context = {NodeEnum::kUdpCombEntry},
          .expected_annotation = {1, SpacingOptions::kUndecided},
      },
      {
          // 1 0  // comment\n : -;
          .style = DefaultStyle,
          .left_token_enum = '0',
          .left_token_string = "0",
          .whitespace_between = "",
          .right_token_enum = verilog_tokentype::TK_EOL_COMMENT,
          .right_token_string = "// comment",
          .left_context = {NodeEnum::kUdpCombEntry},
          .right_context = {NodeEnum::kUdpCombEntry},
          .expected_annotation = {2, SpacingOptions::kMustAppend},
      },
      {
          // 1  /*comment*/ 0 : -;
          .style = DefaultStyle,
          .left_token_enum = '1',
          .left_token_string = "1",
          .whitespace_between = "",
          .right_token_enum = verilog_tokentype::TK_COMMENT_BLOCK,
          .right_token_string = "/* comment */",
          .left_context = {NodeEnum::kUdpSequenceEntry},
          .right_context = {NodeEnum::kUdpSequenceEntry},
          .expected_annotation = {2, SpacingOptions::kUndecided},
      },
      {
          // 1  /*comment*/ 0 : -;
          .style = DefaultStyle,
          .left_token_enum = verilog_tokentype::TK_COMMENT_BLOCK,
          .left_token_string = "/* comment */",
          .whitespace_between = "",
          .right_token_enum = '0',
          .right_token_string = "0",
          .left_context = {NodeEnum::kUdpSequenceEntry},
          .right_context = {NodeEnum::kUdpSequenceEntry},
          .expected_annotation = {1, SpacingOptions::kUndecided},
      },
      {
          // 1 0  // comment\n : -;
          .style = DefaultStyle,
          .left_token_enum = '0',
          .left_token_string = "0",
          .whitespace_between = "",
          .right_token_enum = verilog_tokentype::TK_EOL_COMMENT,
          .right_token_string = "// comment",
          .left_context = {NodeEnum::kUdpSequenceEntry},
          .right_context = {NodeEnum::kUdpSequenceEntry},
          .expected_annotation = {2, SpacingOptions::kMustAppend},
      },
      {
          // input  /* comment */ i;
          .style = DefaultStyle,
          .left_token_enum = verilog_tokentype::TK_input,
          .left_token_string = "input",
          .whitespace_between = "",
          .right_token_enum = verilog_tokentype::TK_COMMENT_BLOCK,
          .right_token_string = "/* comment */",
          .left_context = {NodeEnum::kUdpPortDeclaration},
          .right_context = {NodeEnum::kUdpPortDeclaration},
          .expected_annotation = {2, SpacingOptions::kUndecided},
      },
      {
          // input  /* comment */ i;
          .style = DefaultStyle,
          .left_token_enum = verilog_tokentype::TK_COMMENT_BLOCK,
          .left_token_string = "/* comment */",
          .whitespace_between = "",
          .right_token_enum = verilog_tokentype::SymbolIdentifier,
          .right_token_string = "i",
          .left_context = {NodeEnum::kUdpPortDeclaration},
          .right_context = {NodeEnum::kUdpPortDeclaration},
          .expected_annotation = {1, SpacingOptions::kUndecided},
      },
      {
          // input i  /* comment */;
          .style = DefaultStyle,
          .left_token_enum = verilog_tokentype::SymbolIdentifier,
          .left_token_string = "i",
          .whitespace_between = "",
          .right_token_enum = verilog_tokentype::TK_COMMENT_BLOCK,
          .right_token_string = "/* comment */",
          .left_context = {NodeEnum::kUdpPortDeclaration},
          .right_context = {NodeEnum::kUdpPortDeclaration},
          .expected_annotation = {2, SpacingOptions::kUndecided},
      },
      {
          // input i;  // comment\n
          .style = DefaultStyle,
          .left_token_enum = ';',
          .left_token_string = ";",
          .whitespace_between = "",
          .right_token_enum = verilog_tokentype::TK_EOL_COMMENT,
          .right_token_string = "// comment",
          .left_context = {NodeEnum::kUdpPortDeclaration},
          .right_context = {NodeEnum::kUdpPortDeclaration},
          .expected_annotation = {2, SpacingOptions::kMustAppend},
      },

      {
          // [a+b]
          .style = DefaultStyle,
          .left_token_enum = verilog_tokentype::SymbolIdentifier,
          .left_token_string = "a",
          .whitespace_between = "",  // no spaces originally
          .right_token_enum = '+',
          .right_token_string = "+",
          .left_context = {NodeEnum::kDimensionScalar},
          .right_context = {NodeEnum::kDimensionScalar},
          .expected_annotation = {0, SpacingOptions::kUndecided},
      },
      {
          // [a +b]
          .style = DefaultStyle,
          .left_token_enum = verilog_tokentype::SymbolIdentifier,
          .left_token_string = "a",
          .whitespace_between = " ",  // 1 space originally
          .right_token_enum = '+',
          .right_token_string = "+",
          .left_context = {NodeEnum::kDimensionScalar},
          .right_context = {NodeEnum::kDimensionScalar},
          .expected_annotation = {0, SpacingOptions::kUndecided},
      },
      {
          // [a  +b]
          .style = DefaultStyle,
          .left_token_enum = verilog_tokentype::SymbolIdentifier,
          .left_token_string = "a",
          .whitespace_between = "  ",  // 2 spaces originally
          .right_token_enum = '+',
          .right_token_string = "+",
          .left_context = {NodeEnum::kDimensionScalar},
          .right_context = {NodeEnum::kDimensionScalar},
          .expected_annotation = {0, SpacingOptions::kUndecided},  // no spacing
      },
      {
          // [a     :    b]
          .style = DefaultStyle,
          .left_token_enum = verilog_tokentype::SymbolIdentifier,
          .left_token_string = "a",
          .whitespace_between = "     ",
          .right_token_enum = ':',
          .right_token_string = ":",
          .left_context = {NodeEnum::kDimensionRange},
          .right_context = {NodeEnum::kDimensionRange},
          .expected_annotation = {1, SpacingOptions::kUndecided},  // limit to 1
      },
      {
          // [a     :    b]
          .style = DefaultStyle,
          .left_token_enum = ':',
          .left_token_string = ":",
          .whitespace_between = "    ",
          .right_token_enum = verilog_tokentype::SymbolIdentifier,
          .right_token_string = "b",
          .left_context = {NodeEnum::kDimensionRange},
          .right_context = {NodeEnum::kDimensionRange},
          .expected_annotation = {0, SpacingOptions::kUndecided},
      },
      {
          // [a + b]
          .style = CompactIndexSelectionStyle,
          .left_token_enum = verilog_tokentype::SymbolIdentifier,
          .left_token_string = "a",
          .whitespace_between = " ",
          .right_token_enum = '+',
          .right_token_string = "+",
          .left_context = {NodeEnum::kDimensionScalar},
          .right_context = {NodeEnum::kStreamingConcatenation},
          .expected_annotation = {1, SpacingOptions::kUndecided},
      },
      {
          // [a+b]
          .style = CompactIndexSelectionStyle,
          .left_token_enum = verilog_tokentype::SymbolIdentifier,
          .left_token_string = "a",
          .whitespace_between = "",  // no spaces originally
          .right_token_enum = '+',
          .right_token_string = "+",
          .left_context = {NodeEnum::kDimensionScalar},
          .right_context = {NodeEnum::kDimensionScalar},
          .expected_annotation = {0, SpacingOptions::kUndecided},
      },
      {
          // [a +b]
          .style = CompactIndexSelectionStyle,
          .left_token_enum = verilog_tokentype::SymbolIdentifier,
          .left_token_string = "a",
          .whitespace_between = " ",  // 1 space originally
          .right_token_enum = '+',
          .right_token_string = "+",
          .left_context = {NodeEnum::kDimensionScalar},
          .right_context = {NodeEnum::kDimensionScalar},
          .expected_annotation =
              {1, SpacingOptions::kUndecided},  // limit to 1 space
      },
      {
          // [a  +b]
          .style = CompactIndexSelectionStyle,
          .left_token_enum = verilog_tokentype::SymbolIdentifier,
          .left_token_string = "a",
          .whitespace_between = "  ",  // 2 spaces originally
          .right_token_enum = '+',
          .right_token_string = "+",
          .left_context = {NodeEnum::kDimensionScalar},
          .right_context = {NodeEnum::kDimensionScalar},
          .expected_annotation =
              {1, SpacingOptions::kUndecided},  // limit to 1 space
      },
      {
          // [a     :    b]
          .style = CompactIndexSelectionStyle,
          .left_token_enum = verilog_tokentype::SymbolIdentifier,
          .left_token_string = "a",
          .whitespace_between = "     ",
          .right_token_enum = ':',
          .right_token_string = ":",
          .left_context = {NodeEnum::kDimensionRange},
          .right_context = {NodeEnum::kDimensionRange},
          .expected_annotation =
              {1, SpacingOptions::kUndecided},  // limit to 1 space
      },
      {
          // [a     :    b]
          .style = CompactIndexSelectionStyle,
          .left_token_enum = ':',
          .left_token_string = ":",
          .whitespace_between = "    ",
          .right_token_enum = verilog_tokentype::SymbolIdentifier,
          .right_token_string = "b",
          .left_context = {NodeEnum::kDimensionRange},
          .right_context = {NodeEnum::kDimensionRange},
          .expected_annotation = {0, SpacingOptions::kUndecided},
      },
      {
          .style = DefaultStyle,
          .left_token_enum = verilog_tokentype::SymbolIdentifier,
          .left_token_string = "a",
          .whitespace_between = "\n    ",
          .right_token_enum = ':',
          .right_token_string = ":",
          .left_context = {NodeEnum::kDimensionRange},
          .right_context = {NodeEnum::kDimensionRange},
          // 0 spaces as this is an indentation, not spacing
          .expected_annotation = {0, SpacingOptions::kUndecided},
      },
      {
          .style = DefaultStyle,
          .left_token_enum = '*',
          .left_token_string = "*",
          .whitespace_between = "",  // 0 spaces originally
          .right_token_enum = verilog_tokentype::SymbolIdentifier,
          .right_token_string = "foo",
          .left_context = {NodeEnum::kDimensionRange},
          .right_context = {NodeEnum::kDimensionRange},
          .expected_annotation = {0, SpacingOptions::kUndecided},
      },
      {
          .style = DefaultStyle,
          .left_token_enum = verilog_tokentype::SymbolIdentifier,
          .left_token_string = "foo",
          .whitespace_between = "",  // 0 spaces originally
          .right_token_enum = '*',
          .right_token_string = "*",
          .left_context = {NodeEnum::kDimensionRange},
          .right_context = {NodeEnum::kDimensionRange},
          .expected_annotation = {0, SpacingOptions::kUndecided},
      },
      {
          .style = DefaultStyle,
          .left_token_enum = '*',
          .left_token_string = "*",
          .whitespace_between = "",  // 0 spaces originally
          .right_token_enum = verilog_tokentype::SymbolIdentifier,
          .right_token_string = "foo",
          .left_context = {NodeEnum::kDimensionScalar},
          .right_context = {NodeEnum::kDimensionScalar},
          .expected_annotation = {0, SpacingOptions::kUndecided},
      },
      {
          .style = DefaultStyle,
          .left_token_enum = verilog_tokentype::SymbolIdentifier,
          .left_token_string = "foo",
          .whitespace_between = "",  // 0 spaces originally
          .right_token_enum = '*',
          .right_token_string = "*",
          .left_context = {NodeEnum::kDimensionScalar},
          .right_context = {NodeEnum::kDimensionScalar},
          .expected_annotation = {0, SpacingOptions::kUndecided},
      },
      {
          .style = DefaultStyle,
          .left_token_enum = '*',
          .left_token_string = "*",
          .whitespace_between = " ",  // 1 space originally
          .right_token_enum = verilog_tokentype::SymbolIdentifier,
          .right_token_string = "foo",
          .left_context = {NodeEnum::kPackedDimensions},
          .right_context = {NodeEnum::kPackedDimensions},
          .expected_annotation = {1, SpacingOptions::kPreserve},
      },
      {
          .style = DefaultStyle,
          .left_token_enum = verilog_tokentype::SymbolIdentifier,
          .left_token_string = "foo",
          .whitespace_between = " ",  // 1 space originally
          .right_token_enum = '*',
          .right_token_string = "*",
          .left_context = {NodeEnum::kPackedDimensions},
          .right_context = {NodeEnum::kPackedDimensions},
          .expected_annotation = {1, SpacingOptions::kPreserve},
      },
      {
          .style = DefaultStyle,
          .left_token_enum = '*',
          .left_token_string = "*",
          .whitespace_between = " ",  // 1 space originally
          .right_token_enum = verilog_tokentype::SymbolIdentifier,
          .right_token_string = "foo",
          .left_context = {NodeEnum::kUnpackedDimensions},
          .right_context = {NodeEnum::kUnpackedDimensions},
          .expected_annotation = {1, SpacingOptions::kPreserve},
      },
      {
          .style = DefaultStyle,
          .left_token_enum = verilog_tokentype::SymbolIdentifier,
          .left_token_string = "foo",
          .whitespace_between = " ",  // 1 space originally
          .right_token_enum = '*',
          .right_token_string = "*",
          .left_context = {NodeEnum::kUnpackedDimensions},
          .right_context = {NodeEnum::kUnpackedDimensions},
          .expected_annotation = {1, SpacingOptions::kPreserve},
      },
      {
          // [b > 1'h0 ? 1'h0 : c] : space around '>' in ternary inside
          // subscript
          .style = DefaultStyle,
          .left_token_enum = verilog_tokentype::SymbolIdentifier,
          .left_token_string = "b",
          .whitespace_between = " ",  // 1 space originally
          .right_token_enum = '>',
          .right_token_string = ">",
          .left_context = {NodeEnum::kDimensionScalar,
                           NodeEnum::kConditionExpression},
          .right_context = {NodeEnum::kDimensionScalar,
                            NodeEnum::kConditionExpression},
          .expected_annotation = {1, SpacingOptions::kUndecided},
      },
      {
          // [b > 1 ? 1 : c] : space before '?' in ternary inside subscript
          .style = DefaultStyle,
          .left_token_enum = verilog_tokentype::TK_DecNumber,
          .left_token_string = "1",
          .whitespace_between = " ",  // 1 space originally
          .right_token_enum = '?',
          .right_token_string = "?",
          .left_context = {NodeEnum::kDimensionScalar,
                           NodeEnum::kConditionExpression},
          .right_context = {NodeEnum::kDimensionScalar,
                            NodeEnum::kConditionExpression},
          .expected_annotation = {1, SpacingOptions::kUndecided},
      },
  };
  int test_index = 0;
  for (const auto &test_case : kTestCases) {
    VLOG(1) << "test_index[" << test_index << "]:";

    const verible::TokenInfoTestData test_data = {
        {test_case.left_token_enum, test_case.left_token_string},
        test_case.whitespace_between,
        {test_case.right_token_enum, test_case.right_token_string}};

    auto token_vector = test_data.FindImportantTokens();
    ASSERT_EQ(token_vector.size(), 2);

    PreFormatToken left(token_vector.data());
    PreFormatToken right(token_vector.data() + 1);
    // like verible::ConnectPreFormatTokensPreservedSpaceStarts();
    right.before.preserved_space_start = left.Text().end();

    left.format_token_enum =
        GetFormatTokenType(verilog_tokentype(left.TokenEnum()));
    right.format_token_enum =
        GetFormatTokenType(verilog_tokentype(right.TokenEnum()));

    VLOG(1) << "left context: " << test_case.left_context;
    VLOG(1) << "right context: " << test_case.right_context;
    AnnotateFormatToken(test_case.style, left, &right, test_case.left_context,
                        test_case.right_context);
    EXPECT_EQ(test_case.expected_annotation, right.before)
        << "Index: " << test_index                        //
        << " Left context: " << test_case.left_context    //
        << " Right context: " << test_case.right_context  //
        << " with left=" << left.Text()                   //
        << " and right=" << right.Text();
    ++test_index;
  }
}

}  // namespace
}  // namespace formatter
}  // namespace verilog
