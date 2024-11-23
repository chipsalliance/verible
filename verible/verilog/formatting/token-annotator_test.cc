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
#include <vector>

#include "absl/strings/string_view.h"
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
      {DefaultStyle, 0, {}, {}},

      // //comment1
      // //comment2
      {DefaultStyle,
       0,
       // ExpectedInterTokenInfo:
       // spaces_required, break_decision
       {{0, SpacingOptions::kUndecided},  //
        {2, SpacingOptions::kMustWrap}},
       {{verilog_tokentype::TK_EOL_COMMENT, "//comment1"},
        {verilog_tokentype::TK_EOL_COMMENT, "//comment2"}}},

      // If there is no newline before comment, it will be appended
      // (  //comment
      {DefaultStyle,
       0,
       {{0, SpacingOptions::kUndecided},  //
        {2, SpacingOptions::kMustAppend}},
       {{'(', "("}, {verilog_tokentype::TK_EOL_COMMENT, "//comment"}}},

      // [  //comment
      {DefaultStyle,
       0,
       {{0, SpacingOptions::kUndecided},  //
        {2, SpacingOptions::kMustAppend}},
       {{'[', "["}, {verilog_tokentype::TK_EOL_COMMENT, "//comment"}}},

      // {  //comment
      {DefaultStyle,
       0,
       {{0, SpacingOptions::kUndecided},  //
        {2, SpacingOptions::kMustAppend}},
       {{'{', "{"}, {verilog_tokentype::TK_EOL_COMMENT, "//comment"}}},

      // ,  //comment
      {DefaultStyle,
       0,
       {{0, SpacingOptions::kUndecided},  //
        {2, SpacingOptions::kMustAppend}},
       {{',', ","}, {verilog_tokentype::TK_EOL_COMMENT, "//comment"}}},

      // ;  //comment
      {DefaultStyle,
       0,
       {{0, SpacingOptions::kUndecided},  //
        {2, SpacingOptions::kMustAppend}},
       {{';', ";"}, {verilog_tokentype::TK_EOL_COMMENT, "//comment"}}},

      // module foo();
      {DefaultStyle,
       0,
       {{0, SpacingOptions::kUndecided},
        {1, SpacingOptions::kUndecided},
        {0, SpacingOptions::kUndecided},
        {0, SpacingOptions::kUndecided},
        {0, SpacingOptions::kUndecided}},
       {{verilog_tokentype::TK_module, "module"},
        {verilog_tokentype::SymbolIdentifier, "foo"},
        {'(', "("},
        {')', ")"},
        {';', ";"}}},

      // module foo(a, b);
      {DefaultStyle,
       0,
       {{0, SpacingOptions::kUndecided},
        {1, SpacingOptions::kUndecided},
        {0, SpacingOptions::kUndecided},
        {0, SpacingOptions::kUndecided},  // "a"
        {0, SpacingOptions::kUndecided},  // ','
        {1, SpacingOptions::kUndecided},  // "b"
        {0, SpacingOptions::kUndecided},
        {0, SpacingOptions::kUndecided}},
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
       {{0, SpacingOptions::kUndecided},
        {1, SpacingOptions::kUndecided},   // with_params
        {1, SpacingOptions::kUndecided},   // #
        {0, SpacingOptions::kMustAppend},  // (
        {0, SpacingOptions::kUndecided},   // )
        {1, SpacingOptions::kUndecided},   // (
        {0, SpacingOptions::kUndecided},   // )
        {0, SpacingOptions::kUndecided}},  // ;
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
       {{0, SpacingOptions::kUndecided},
        {1, SpacingOptions::kUndecided},
        {1, SpacingOptions::kUndecided},
        {0, SpacingOptions::kUndecided},
        {0, SpacingOptions::kUndecided},
        {0, SpacingOptions::kUndecided},
        {0, SpacingOptions::kUndecided}},
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
       {{0, SpacingOptions::kUndecided},
        {0, SpacingOptions::kUndecided},
        {0, SpacingOptions::kUndecided},
        {0, SpacingOptions::kUndecided},
        {0, SpacingOptions::kUndecided},
        {0, SpacingOptions::kUndecided},
        {0, SpacingOptions::kUndecided}},
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
       {{0, SpacingOptions::kUndecided},   // always
        {1, SpacingOptions::kUndecided},   // @
        {0, SpacingOptions::kUndecided},   // (
        {0, SpacingOptions::kUndecided},   // posedge
        {1, SpacingOptions::kUndecided},   // clk
        {0, SpacingOptions::kUndecided}},  // )
       {{verilog_tokentype::TK_always, "always"},
        {'@', "@"},
        {'(', "("},
        {verilog_tokentype::TK_posedge, "TK_posedge"},
        {verilog_tokentype::SymbolIdentifier, "clk"},
        {')', ")"}}},

      // `WIDTH'(s) (casting operator)
      {DefaultStyle,
       0,
       {{0, SpacingOptions::kUndecided},
        {0, SpacingOptions::kUndecided},
        {0, SpacingOptions::kUndecided},
        {0, SpacingOptions::kUndecided},
        {0, SpacingOptions::kUndecided}},
       {{verilog_tokentype::MacroIdItem, "`WIDTH"},
        {'\'', "'"},
        {'(', "("},
        {verilog_tokentype::SymbolIdentifier, "s"},
        {')', ")"}}},

      // string'(s) (casting operator)
      {DefaultStyle,
       0,
       {{0, SpacingOptions::kUndecided},
        {0, SpacingOptions::kUndecided},
        {0, SpacingOptions::kUndecided},
        {0, SpacingOptions::kUndecided},
        {0, SpacingOptions::kUndecided}},
       {{verilog_tokentype::TK_string, "string"},
        {'\'', "'"},
        {'(', "("},
        {verilog_tokentype::SymbolIdentifier, "s"},
        {')', ")"}}},

      // void'(f()) (casting operator)
      {DefaultStyle,
       0,
       {{0, SpacingOptions::kUndecided},
        {0, SpacingOptions::kUndecided},
        {0, SpacingOptions::kUndecided},
        {0, SpacingOptions::kUndecided},
        {0, SpacingOptions::kUndecided},
        {0, SpacingOptions::kUndecided},
        {0, SpacingOptions::kUndecided}},
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
       {{0, SpacingOptions::kUndecided},
        {0, SpacingOptions::kUndecided},
        {0, SpacingOptions::kUndecided},
        {0, SpacingOptions::kUndecided},
        {0, SpacingOptions::kUndecided}},
       {{verilog_tokentype::TK_DecNumber, "12"},
        {'\'', "'"},
        {'{', "{"},
        {verilog_tokentype::TK_DecNumber, "34"},
        {'}', "}"}}},

      // k()'(s) (casting operator)
      {DefaultStyle,
       0,
       {{0, SpacingOptions::kUndecided},
        {0, SpacingOptions::kUndecided},
        {0, SpacingOptions::kUndecided},
        {0, SpacingOptions::kUndecided},
        {0, SpacingOptions::kUndecided},
        {0, SpacingOptions::kUndecided},
        {0, SpacingOptions::kUndecided}},
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
          {{0, SpacingOptions::kUndecided},
           {0, SpacingOptions::kMustAppend},
           {1, SpacingOptions::kUndecided}},
          {{'#', "#"},
           {verilog_tokentype::TK_DecNumber, "1"},
           {verilog_tokentype::SystemTFIdentifier, "$display"}},
      },

      // 666 777
      {
          DefaultStyle,
          0,
          {{0, SpacingOptions::kUndecided}, {1, SpacingOptions::kUndecided}},
          {{verilog_tokentype::TK_DecNumber, "666"},
           {verilog_tokentype::TK_DecNumber, "777"}},
      },

      // 5678 dance
      {
          DefaultStyle,
          0,
          {{0, SpacingOptions::kUndecided}, {1, SpacingOptions::kUndecided}},
          {{verilog_tokentype::TK_DecNumber, "5678"},
           {verilog_tokentype::SymbolIdentifier, "dance"}},
      },

      // id 4321
      {
          DefaultStyle,
          0,
          {{0, SpacingOptions::kUndecided}, {1, SpacingOptions::kUndecided}},
          {{verilog_tokentype::SymbolIdentifier, "id"},
           {verilog_tokentype::TK_DecNumber, "4321"}},
      },

      // id1 id2
      {
          DefaultStyle,
          0,
          {{0, SpacingOptions::kUndecided}, {1, SpacingOptions::kUndecided}},
          {{verilog_tokentype::SymbolIdentifier, "id1"},
           {verilog_tokentype::SymbolIdentifier, "id2"}},
      },

      // class mate
      {
          DefaultStyle,
          0,
          {{0, SpacingOptions::kUndecided}, {1, SpacingOptions::kUndecided}},
          {{verilog_tokentype::TK_class, "class"},
           {verilog_tokentype::SymbolIdentifier, "mate"}},
      },

      // id module
      {
          DefaultStyle,
          0,
          {{0, SpacingOptions::kUndecided}, {1, SpacingOptions::kUndecided}},
          {{verilog_tokentype::SymbolIdentifier, "lunar"},
           {verilog_tokentype::TK_module, "module"}},
      },

      // class 1337
      {
          DefaultStyle,
          0,
          {{0, SpacingOptions::kUndecided}, {1, SpacingOptions::kUndecided}},
          {{verilog_tokentype::TK_class, "class"},
           {verilog_tokentype::TK_DecNumber, "1337"}},
      },

      // 987 module
      {
          DefaultStyle,
          0,
          {{0, SpacingOptions::kUndecided}, {1, SpacingOptions::kUndecided}},
          {{verilog_tokentype::TK_DecNumber, "987"},
           {verilog_tokentype::TK_module, "module"}},
      },

      // a = 16'hf00d;
      {DefaultStyle,
       0,
       {{0, SpacingOptions::kUndecided},
        {1, SpacingOptions::kUndecided},
        {1, SpacingOptions::kUndecided},
        {0, SpacingOptions::kMustAppend},
        {0, SpacingOptions::kMustAppend},
        {0, SpacingOptions::kUndecided}},
       {{verilog_tokentype::SymbolIdentifier, "a"},
        {'=', "="},
        {verilog_tokentype::TK_DecNumber, "16"},
        {verilog_tokentype::TK_HexBase, "'h"},
        {verilog_tokentype::TK_HexDigits, "c0ffee"},
        {';', ";"}}},

      // a = 8'b1001_0110;
      {DefaultStyle,
       0,
       {{0, SpacingOptions::kUndecided},
        {1, SpacingOptions::kUndecided},
        {1, SpacingOptions::kUndecided},
        {0, SpacingOptions::kMustAppend},
        {0, SpacingOptions::kMustAppend},
        {0, SpacingOptions::kUndecided}},
       {{verilog_tokentype::SymbolIdentifier, "a"},
        {'=', "="},
        {verilog_tokentype::TK_DecNumber, "8"},
        {verilog_tokentype::TK_BinBase, "'b"},
        {verilog_tokentype::TK_BinDigits, "1001_0110"},
        {';', ";"}}},

      // a = 4'd10;
      {DefaultStyle,
       0,
       {{0, SpacingOptions::kUndecided},
        {1, SpacingOptions::kUndecided},
        {1, SpacingOptions::kUndecided},
        {0, SpacingOptions::kMustAppend},
        {0, SpacingOptions::kMustAppend},
        {0, SpacingOptions::kUndecided}},
       {{verilog_tokentype::SymbolIdentifier, "a"},
        {'=', "="},
        {verilog_tokentype::TK_DecNumber, "4"},
        {verilog_tokentype::TK_DecBase, "'d"},
        {verilog_tokentype::TK_DecDigits, "10"},
        {';', ";"}}},

      // a = 8'o100;
      {DefaultStyle,
       0,
       {{0, SpacingOptions::kUndecided},
        {1, SpacingOptions::kUndecided},
        {1, SpacingOptions::kUndecided},
        {0, SpacingOptions::kMustAppend},
        {0, SpacingOptions::kMustAppend},
        {0, SpacingOptions::kUndecided}},
       {{verilog_tokentype::SymbolIdentifier, "a"},
        {'=', "="},
        {verilog_tokentype::TK_DecNumber, "8"},
        {verilog_tokentype::TK_OctBase, "'o"},
        {verilog_tokentype::TK_OctDigits, "100"},
        {';', ";"}}},

      // a = 'hc0ffee;
      {DefaultStyle,
       0,
       {{0, SpacingOptions::kUndecided},
        {1, SpacingOptions::kUndecided},
        {1, SpacingOptions::kUndecided},
        {0, SpacingOptions::kMustAppend},
        {0, SpacingOptions::kUndecided}},
       {{verilog_tokentype::SymbolIdentifier, "a"},
        {'=', "="},
        {verilog_tokentype::TK_HexBase, "'h"},
        {verilog_tokentype::TK_HexDigits, "c0ffee"},
        {';', ";"}}},

      // a = funk('b0, 'd'8);
      {DefaultStyle,
       0,
       {{0, SpacingOptions::kUndecided},
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
       {{0, SpacingOptions::kUndecided},
        {1, SpacingOptions::kUndecided},
        {1, SpacingOptions::kUndecided},
        {0, SpacingOptions::kMustAppend},
        {1, SpacingOptions::kUndecided},
        {1, SpacingOptions::kUndecided},
        {0, SpacingOptions::kMustAppend},
        {0, SpacingOptions::kUndecided}},
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
       {{0, SpacingOptions::kUndecided},
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
              {0, SpacingOptions::kUndecided},  //  a
              {1, SpacingOptions::kUndecided},  //  ?
              {1, SpacingOptions::kUndecided},  //  b
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
              {0, SpacingOptions::kUndecided},  //  1
              {1, SpacingOptions::kUndecided},  //  ?
              {1, SpacingOptions::kUndecided},  //  2
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
              {0, SpacingOptions::kUndecided},  //  "1"
              {1, SpacingOptions::kUndecided},  //  ?
              {1, SpacingOptions::kUndecided},  //  "2"
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
       {{0, SpacingOptions::kUndecided},   //  b
        {1, SpacingOptions::kUndecided},   //  ?
        {1, SpacingOptions::kUndecided},   //  8
        {0, SpacingOptions::kMustAppend},  //  'o
        {0, SpacingOptions::kMustAppend},  //  100
        kUnhandledSpacing,                 //  :
        {1, SpacingOptions::kUndecided},   //  '0
        {0, SpacingOptions::kUndecided}},  //  ;
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
       {{0, SpacingOptions::kUndecided},   // a
        {1, SpacingOptions::kUndecided},   // =
        {1, SpacingOptions::kUndecided},   // (
        {0, SpacingOptions::kUndecided},   // b
        {1, SpacingOptions::kUndecided},   // +
        {1, SpacingOptions::kUndecided},   // c
        {0, SpacingOptions::kUndecided},   // )
        {0, SpacingOptions::kUndecided}},  // ;
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
       {{0, SpacingOptions::kUndecided},   //  function
        {1, SpacingOptions::kUndecided},   //  foo
        {0, SpacingOptions::kUndecided},   //  (
        {0, SpacingOptions::kUndecided},   //  name
        {1, SpacingOptions::kUndecided},   //  =
        {1, SpacingOptions::kUndecided},   //  "foo"
        {0, SpacingOptions::kUndecided},   //  )
        {0, SpacingOptions::kUndecided}},  //  ;
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
       {{0, SpacingOptions::kUndecided},   //  `define
        {1, SpacingOptions::kMustAppend},  //  FOO
        {0, SpacingOptions::kUndecided},   //  (
        {0, SpacingOptions::kUndecided},   //  name
        {1, SpacingOptions::kUndecided},   //  =
        {1, SpacingOptions::kUndecided},   //  "bar"
        {0, SpacingOptions::kUndecided}},  //  )
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
       {{0, SpacingOptions::kUndecided},
        {1, SpacingOptions::kUndecided},
        {1, SpacingOptions::kUndecided}},
       {
           {verilog_tokentype::TK_endfunction, "endfunction"},
           {':', ":"},
           {verilog_tokentype::SymbolIdentifier, "funk"},
       }},

      // case (expr):
      {DefaultStyle,
       1,
       {
           {0, SpacingOptions::kUndecided},
           {1, SpacingOptions::kUndecided},
           {0, SpacingOptions::kUndecided},
           {0, SpacingOptions::kUndecided},
           {0, SpacingOptions::kUndecided},
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
           {0, SpacingOptions::kUndecided},
           {1, SpacingOptions::kUndecided},
           {0, SpacingOptions::kUndecided},
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
           {0, SpacingOptions::kUndecided},
           {0, SpacingOptions::kUndecided},
           {0, SpacingOptions::kUndecided},
           {0, SpacingOptions::kUndecided},
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
           {0, SpacingOptions::kUndecided},
           {0, SpacingOptions::kUndecided},
           {0, SpacingOptions::kUndecided},
           {0, SpacingOptions::kUndecided},
           {0, SpacingOptions::kUndecided},
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
           {0, SpacingOptions::kUndecided},
           {0, SpacingOptions::kUndecided},
           {0, SpacingOptions::kUndecided},
           {0, SpacingOptions::kUndecided},
           {0, SpacingOptions::kUndecided},
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
           {0, SpacingOptions::kUndecided},
           {0, SpacingOptions::kUndecided},
           {0, SpacingOptions::kUndecided},
           {0, SpacingOptions::kUndecided},
           {1, SpacingOptions::kUndecided},
           {0, SpacingOptions::kUndecided},
           {0, SpacingOptions::kUndecided},
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
           {0, SpacingOptions::kUndecided},
           {0, SpacingOptions::kUndecided},
           {0, SpacingOptions::kUndecided},
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
           {0, SpacingOptions::kUndecided},
           {0, SpacingOptions::kUndecided},
           {0, SpacingOptions::kUndecided},
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
           {0, SpacingOptions::kUndecided},
           {0, SpacingOptions::kUndecided},
           {0, SpacingOptions::kUndecided},
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
           {0, SpacingOptions::kUndecided},
           {0, SpacingOptions::kUndecided},
           {0, SpacingOptions::kUndecided},
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
           {0, SpacingOptions::kUndecided},
           {0, SpacingOptions::kUndecided},
           {0, SpacingOptions::kUndecided},
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
           {0, SpacingOptions::kUndecided},
           {0, SpacingOptions::kUndecided},
           {0, SpacingOptions::kUndecided},
           {0, SpacingOptions::kUndecided},
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
           {0, SpacingOptions::kUndecided},
           {0, SpacingOptions::kUndecided},
           {0, SpacingOptions::kUndecided},
           {0, SpacingOptions::kUndecided},
           {0, SpacingOptions::kUndecided},
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
           {0, SpacingOptions::kUndecided},
           {0, SpacingOptions::kUndecided},
           {0, SpacingOptions::kUndecided},  // "y"
           {0, SpacingOptions::kUndecided},  // ','
           {1, SpacingOptions::kUndecided},  // "x"
           {0, SpacingOptions::kUndecided},
           {0, SpacingOptions::kUndecided},
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
           {0, SpacingOptions::kUndecided},   // `define
           {1, SpacingOptions::kMustAppend},  // FOO
           {0, SpacingOptions::kMustAppend},  // "" (empty definition body)
           {0, SpacingOptions::kMustWrap},    // `define
           {1, SpacingOptions::kMustAppend},  // BAR
           {0, SpacingOptions::kMustAppend},  // "" (empty definition body)
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
           {0, SpacingOptions::kUndecided},   // `define
           {1, SpacingOptions::kMustAppend},  // FOO
           {1, SpacingOptions::kMustAppend},  // 1
           {1, SpacingOptions::kMustWrap},    // `define
           {1, SpacingOptions::kMustAppend},  // BAR
           {1, SpacingOptions::kMustAppend},  // 2
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
              {0, SpacingOptions::kUndecided},   // `define
              {1, SpacingOptions::kMustAppend},  // ADD
              {0, SpacingOptions::kMustAppend},  // (
              {0, SpacingOptions::kUndecided},   // y
              {0, SpacingOptions::kUndecided},   // ,
              {1, SpacingOptions::kUndecided},   // z
              {0, SpacingOptions::kUndecided},   // )
              {1, SpacingOptions::kMustAppend},  // "y+z"
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
           {0, SpacingOptions::kUndecided},  // function
           {1, SpacingOptions::kUndecided},  // new
           {0, SpacingOptions::kUndecided},  // ;
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
           {0, SpacingOptions::kUndecided},  // function
           {1, SpacingOptions::kUndecided},  // new
           {0, SpacingOptions::kUndecided},  // (
           {0, SpacingOptions::kUndecided},  // )
           {0, SpacingOptions::kUndecided},  // ;
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
           {0, SpacingOptions::kUndecided},  // end
           {1, SpacingOptions::kMustWrap},   // end
           {1, SpacingOptions::kMustWrap},   // endfunction
           {1, SpacingOptions::kMustWrap},   // endclass
           {1, SpacingOptions::kMustWrap},   // endpackage
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
           {0, SpacingOptions::kUndecided},  // end
           {1, SpacingOptions::kMustWrap},   // end
           {1, SpacingOptions::kMustWrap},   // endtask
           {1, SpacingOptions::kMustWrap},   // endmodule
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
      {DefaultStyle,
       1,
       {{0, SpacingOptions::kUndecided}, {1, SpacingOptions::kUndecided}},
       {{verilog_tokentype::TK_wait, "wait"}, {'(', "("}}},

      // various built-in function calls
      {DefaultStyle,
       1,
       {{0, SpacingOptions::kUndecided}, {0, SpacingOptions::kUndecided}},
       {{verilog_tokentype::TK_and, "and"}, {'(', "("}}},
      {DefaultStyle,
       1,
       {{0, SpacingOptions::kUndecided}, {1, SpacingOptions::kUndecided}},
       {{verilog_tokentype::TK_assert, "assert"}, {'(', "("}}},
      {DefaultStyle,
       1,
       {{0, SpacingOptions::kUndecided}, {1, SpacingOptions::kUndecided}},
       {{verilog_tokentype::TK_assume, "assume"}, {'(', "("}}},
      {DefaultStyle,
       1,
       {{0, SpacingOptions::kUndecided}, {1, SpacingOptions::kUndecided}},
       {{verilog_tokentype::TK_cover, "cover"}, {'(', "("}}},
      {DefaultStyle,
       1,
       {{0, SpacingOptions::kUndecided}, {1, SpacingOptions::kUndecided}},
       {{verilog_tokentype::TK_expect, "expect"}, {'(', "("}}},
      {DefaultStyle,
       1,
       {{0, SpacingOptions::kUndecided}, {1, SpacingOptions::kUndecided}},
       {{verilog_tokentype::TK_property, "property"}, {'(', "("}}},
      {DefaultStyle,
       1,
       {{0, SpacingOptions::kUndecided}, {1, SpacingOptions::kUndecided}},
       {{verilog_tokentype::TK_sequence, "sequence"}, {'(', "("}}},
      {DefaultStyle,
       1,
       {{0, SpacingOptions::kUndecided}, {1, SpacingOptions::kUndecided}},
       {{verilog_tokentype::TK_final, "final"}, {'(', "("}}},
      {DefaultStyle,
       1,
       {{0, SpacingOptions::kUndecided}, {0, SpacingOptions::kUndecided}},
       {{verilog_tokentype::TK_find, "find"}, {'(', "("}}},
      {DefaultStyle,
       1,
       {{0, SpacingOptions::kUndecided}, {0, SpacingOptions::kUndecided}},
       {{verilog_tokentype::TK_find_index, "find_index"}, {'(', "("}}},
      {DefaultStyle,
       1,
       {{0, SpacingOptions::kUndecided}, {0, SpacingOptions::kUndecided}},
       {{verilog_tokentype::TK_find_first, "find_first"}, {'(', "("}}},
      {DefaultStyle,
       1,
       {{0, SpacingOptions::kUndecided}, {0, SpacingOptions::kUndecided}},
       {{verilog_tokentype::TK_find_first_index, "find_first_index"},
        {'(', "("}}},
      {DefaultStyle,
       1,
       {{0, SpacingOptions::kUndecided}, {0, SpacingOptions::kUndecided}},
       {{verilog_tokentype::TK_find_last, "find_last"}, {'(', "("}}},
      {DefaultStyle,
       1,
       {{0, SpacingOptions::kUndecided}, {0, SpacingOptions::kUndecided}},
       {{verilog_tokentype::TK_find_last_index, "find_last_index"},
        {'(', "("}}},
      {DefaultStyle,
       1,
       {{0, SpacingOptions::kUndecided}, {0, SpacingOptions::kUndecided}},
       {{verilog_tokentype::TK_min, "min"}, {'(', "("}}},
      {DefaultStyle,
       1,
       {{0, SpacingOptions::kUndecided}, {0, SpacingOptions::kUndecided}},
       {{verilog_tokentype::TK_max, "max"}, {'(', "("}}},
      {DefaultStyle,
       1,
       {{0, SpacingOptions::kUndecided}, {0, SpacingOptions::kUndecided}},
       {{verilog_tokentype::TK_or, "or"}, {'(', "("}}},
      {DefaultStyle,
       1,
       {{0, SpacingOptions::kUndecided}, {0, SpacingOptions::kUndecided}},
       {{verilog_tokentype::TK_product, "product"}, {'(', "("}}},
      {DefaultStyle,
       1,
       {{0, SpacingOptions::kUndecided}, {0, SpacingOptions::kUndecided}},
       {{verilog_tokentype::TK_randomize, "randomize"}, {'(', "("}}},
      {DefaultStyle,
       1,
       {{0, SpacingOptions::kUndecided}, {0, SpacingOptions::kUndecided}},
       {{verilog_tokentype::TK_reverse, "reverse"}, {'(', "("}}},
      {DefaultStyle,
       1,
       {{0, SpacingOptions::kUndecided}, {0, SpacingOptions::kUndecided}},
       {{verilog_tokentype::TK_rsort, "rsort"}, {'(', "("}}},
      {DefaultStyle,
       1,
       {{0, SpacingOptions::kUndecided}, {0, SpacingOptions::kUndecided}},
       {{verilog_tokentype::TK_shuffle, "shuffle"}, {'(', "("}}},
      {DefaultStyle,
       1,
       {{0, SpacingOptions::kUndecided}, {0, SpacingOptions::kUndecided}},
       {{verilog_tokentype::TK_sort, "sort"}, {'(', "("}}},
      {DefaultStyle,
       1,
       {{0, SpacingOptions::kUndecided}, {0, SpacingOptions::kUndecided}},
       {{verilog_tokentype::TK_sum, "sum"}, {'(', "("}}},
      {DefaultStyle,
       1,
       {{0, SpacingOptions::kUndecided}, {0, SpacingOptions::kUndecided}},
       {{verilog_tokentype::TK_unique, "unique"}, {'(', "("}}},
      {DefaultStyle,
       1,
       {{0, SpacingOptions::kUndecided}, {0, SpacingOptions::kUndecided}},
       {{verilog_tokentype::TK_xor, "xor"}, {'(', "("}}},

      // escaped identifier
      // baz.\FOO .bar
      {DefaultStyle,
       1,
       {
           {0, SpacingOptions::kUndecided},  // baz
           {0, SpacingOptions::kUndecided},  // .
           {0, SpacingOptions::kUndecided},  // \FOO
           {1, SpacingOptions::kUndecided},  // .
           {0, SpacingOptions::kUndecided},  // bar
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
           {0, SpacingOptions::kUndecided},  // `BAR
           {0, SpacingOptions::kUndecided},  // (
           {0, SpacingOptions::kUndecided},  // \FOO
           {1, SpacingOptions::kUndecided},  // )
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
           {0, SpacingOptions::kUndecided},  // import
           {1, SpacingOptions::kUndecided},  // foo_pkg
           {0, SpacingOptions::kUndecided},  // ::
           {0, SpacingOptions::kUndecided},  // symbol
           {0, SpacingOptions::kUndecided},  // ;
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
           {0, SpacingOptions::kUndecided},  // import
           {1, SpacingOptions::kUndecided},  // foo_pkg
           {0, SpacingOptions::kUndecided},  // ::
           {0, SpacingOptions::kUndecided},  // *
           {0, SpacingOptions::kUndecided},  // ;
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
           {0, SpacingOptions::kUndecided},   // #
           {0, SpacingOptions::kMustAppend},  // 0
           {0, SpacingOptions::kUndecided},   // ;
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
           {0, SpacingOptions::kUndecided},   // #
           {0, SpacingOptions::kMustAppend},  // 0.5
           {0, SpacingOptions::kUndecided},   // ;
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
           {0, SpacingOptions::kUndecided},   // #
           {0, SpacingOptions::kMustAppend},  // 0ns
           {0, SpacingOptions::kMustAppend},  // ;
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
           {0, SpacingOptions::kUndecided},   // #
           {0, SpacingOptions::kMustAppend},  // 1step
           {0, SpacingOptions::kUndecided},   // ;
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
           {0, SpacingOptions::kUndecided},
           {0, SpacingOptions::kUndecided},
           {1, SpacingOptions::kUndecided},
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
           {0, SpacingOptions::kUndecided},
           {1, SpacingOptions::kUndecided},
           {1, SpacingOptions::kUndecided},
           {1, SpacingOptions::kUndecided},
           {1, SpacingOptions::kUndecided},
           {0, SpacingOptions::kUndecided},
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
           {0, SpacingOptions::kUndecided},
           {1, SpacingOptions::kUndecided},
           {1, SpacingOptions::kUndecided},
           {1, SpacingOptions::kUndecided},
           {1, SpacingOptions::kUndecided},
           {0, SpacingOptions::kUndecided},
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
           {0, SpacingOptions::kUndecided},
           {1, SpacingOptions::kUndecided},
           {1, SpacingOptions::kUndecided},
           {1, SpacingOptions::kUndecided},
           {1, SpacingOptions::kUndecided},
           {0, SpacingOptions::kUndecided},
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
           {0, SpacingOptions::kUndecided},
           {1, SpacingOptions::kUndecided},
           {1, SpacingOptions::kUndecided},
           {1, SpacingOptions::kUndecided},
           {1, SpacingOptions::kUndecided},
           {0, SpacingOptions::kUndecided},
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
           {0, SpacingOptions::kUndecided},
           {1, SpacingOptions::kUndecided},
           {1, SpacingOptions::kUndecided},
           {1, SpacingOptions::kUndecided},
           {1, SpacingOptions::kUndecided},
           {0, SpacingOptions::kUndecided},
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
           {0, SpacingOptions::kUndecided},
           {1, SpacingOptions::kUndecided},
           {1, SpacingOptions::kUndecided},
           {1, SpacingOptions::kUndecided},
           {1, SpacingOptions::kUndecided},
           {0, SpacingOptions::kUndecided},
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
           {0, SpacingOptions::kUndecided},
           {1, SpacingOptions::kUndecided},
           {1, SpacingOptions::kUndecided},
           {1, SpacingOptions::kUndecided},
           {1, SpacingOptions::kUndecided},
           {0, SpacingOptions::kUndecided},
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
           {0, SpacingOptions::kUndecided},
           {1, SpacingOptions::kUndecided},
           {1, SpacingOptions::kUndecided},
           {1, SpacingOptions::kUndecided},
           {1, SpacingOptions::kUndecided},
           {0, SpacingOptions::kUndecided},
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
    AnnotateFormattingInformation(test_case.style, nullptr, nullptr,
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
          DefaultStyle,
          {'=', "="},
          {verilog_tokentype::TK_StringLiteral, "\"hello\""},
          {},  // any context
          {},  // any context
          {1, SpacingOptions::kUndecided},
      },
      {
          DefaultStyle,
          {'=', "="},
          {verilog_tokentype::TK_EvalStringLiteral, "`\"hello`\""},
          {},  // any context
          {},  // any context
          {1, SpacingOptions::kUndecided},
      },
      // Test cases covering right token as a preprocessor directive:
      {
          DefaultStyle,
          {verilog_tokentype::TK_EOL_COMMENT, "//comment1"},
          {verilog_tokentype::PP_ifdef, "`ifdef"},
          {},  // any context
          {},  // any context
          {1, SpacingOptions::kMustWrap},
      },
      {
          DefaultStyle,
          {verilog_tokentype::SymbolIdentifier, "id"},
          {verilog_tokentype::PP_ifdef, "`ifdef"},
          {},  // any context
          {},  // any context
          {1, SpacingOptions::kMustWrap},
      },
      {
          DefaultStyle,
          {';', ";"},
          {verilog_tokentype::PP_ifdef, "`ifdef"},
          {},  // any context
          {},  // any context
          {1, SpacingOptions::kMustWrap},
      },
      {
          DefaultStyle,
          {verilog_tokentype::PP_else, "`else"},
          {verilog_tokentype::PP_ifdef, "`ifdef"},
          {},  // any context
          {},  // any context
          {1, SpacingOptions::kMustWrap},
      },
      {
          DefaultStyle,
          {verilog_tokentype::PP_endif, "`endif"},
          {verilog_tokentype::PP_ifdef, "`ifdef"},
          {},  // any context
          {},  // any context
          {1, SpacingOptions::kMustWrap},
      },
      {
          DefaultStyle,
          {verilog_tokentype::TK_EOL_COMMENT, "//comment1"},
          {verilog_tokentype::PP_ifndef, "`ifndef"},
          {},  // any context
          {},  // any context
          {1, SpacingOptions::kMustWrap},
      },
      {
          DefaultStyle,
          {verilog_tokentype::SymbolIdentifier, "id"},
          {verilog_tokentype::PP_ifndef, "`ifndef"},
          {},  // any context
          {},  // any context
          {1, SpacingOptions::kMustWrap},
      },
      {
          DefaultStyle,
          {';', ";"},
          {verilog_tokentype::PP_ifndef, "`ifndef"},
          {},  // any context
          {},  // any context
          {1, SpacingOptions::kMustWrap},
      },
      {
          DefaultStyle,
          {SymbolIdentifier, "ID"},
          {verilog_tokentype::PP_else, "`else"},
          {},  // any context
          {},  // any context
          {1, SpacingOptions::kMustWrap},
      },
      {
          DefaultStyle,
          {';', ";"},
          {verilog_tokentype::PP_else, "`else"},
          {},  // any context
          {},  // any context
          {1, SpacingOptions::kMustWrap},
      },
      {
          DefaultStyle,
          {verilog_tokentype::PP_endif, "`endif"},
          {verilog_tokentype::PP_else, "`else"},
          {},  // any context
          {},  // any context
          {1, SpacingOptions::kMustWrap},
      },
      {
          DefaultStyle,
          {verilog_tokentype::PP_include, "`include"},
          {TK_StringLiteral, "\"lost/file.svh\""},
          {},                              // any context
          {},                              // any context
          {1, SpacingOptions::kUndecided}, /* or MustAppend? */
      },
      {
          DefaultStyle,
          {verilog_tokentype::PP_include, "`include"},
          {TK_EvalStringLiteral, "`\"lost/file.svh`\""},
          {},                              // any context
          {},                              // any context
          {1, SpacingOptions::kUndecided}, /* or MustAppend? */
      },
      {
          DefaultStyle,
          {TK_StringLiteral, "\"lost/file.svh\""},
          {verilog_tokentype::PP_include, "`include"},
          {},  // any context
          {},  // any context
          {1, SpacingOptions::kMustWrap},
      },
      {
          DefaultStyle,
          {verilog_tokentype::PP_else, "`else"},
          {verilog_tokentype::PP_include, "`include"},
          {},  // any context
          {},  // any context
          {1, SpacingOptions::kMustWrap},
      },
      {
          DefaultStyle,
          {';', ";"},
          {verilog_tokentype::PP_include, "`include"},
          {},  // any context
          {},  // any context
          {1, SpacingOptions::kMustWrap},
      },
      {
          DefaultStyle,
          {SymbolIdentifier, "ID"},
          {verilog_tokentype::PP_include, "`include"},
          {},  // any context
          {},  // any context
          {1, SpacingOptions::kMustWrap},
      },
      {
          DefaultStyle,
          {TK_StringLiteral, "\"lost/file.svh\""},
          {verilog_tokentype::PP_define, "`define"},
          {},  // any context
          {},  // any context
          {1, SpacingOptions::kMustWrap},
      },
      {
          DefaultStyle,
          {verilog_tokentype::PP_else, "`else"},
          {verilog_tokentype::PP_define, "`define"},
          {},  // any context
          {},  // any context
          {1, SpacingOptions::kMustWrap},
      },
      {
          DefaultStyle,
          {';', ";"},
          {verilog_tokentype::PP_define, "`define"},
          {},  // any context
          {},  // any context
          {1, SpacingOptions::kMustWrap},
      },
      {
          DefaultStyle,
          {SymbolIdentifier, "ID"},
          {verilog_tokentype::PP_define, "`define"},
          {},  // any context
          {},  // any context
          {1, SpacingOptions::kMustWrap},
      },
      {
          DefaultStyle,
          {verilog_tokentype::PP_define, "`define"},
          {SymbolIdentifier, "ID"},
          {},  // any context
          {},  // any context
          {1, SpacingOptions::kMustAppend},
      },
      {
          DefaultStyle,
          {TK_StringLiteral, "\"lost/file.svh\""},
          {verilog_tokentype::PP_undef, "`undef"},
          {},  // any context
          {},  // any context
          {1, SpacingOptions::kMustWrap},
      },
      {
          DefaultStyle,
          {verilog_tokentype::PP_else, "`else"},
          {verilog_tokentype::PP_undef, "`undef"},
          {},  // any context
          {},  // any context
          {1, SpacingOptions::kMustWrap},
      },
      {
          DefaultStyle,
          {';', ";"},
          {verilog_tokentype::PP_undef, "`undef"},
          {},  // any context
          {},  // any context
          {1, SpacingOptions::kMustWrap},
      },
      {
          DefaultStyle,
          {SymbolIdentifier, "ID"},
          {verilog_tokentype::PP_undef, "`undef"},
          {},  // any context
          {},  // any context
          {1, SpacingOptions::kMustWrap},
      },
      {
          DefaultStyle,
          {verilog_tokentype::TK_endfunction, "endfunction"},
          {verilog_tokentype::PP_undef, "`undef"},
          {},  // any context
          {},  // any context
          {1, SpacingOptions::kMustWrap},
      },
      {
          DefaultStyle,
          {verilog_tokentype::TK_end, "end"},
          {verilog_tokentype::PP_undef, "`undef"},
          {},  // any context
          {},  // any context
          {1, SpacingOptions::kMustWrap},
      },
      {
          DefaultStyle,
          {verilog_tokentype::MacroCallCloseToEndLine, ")"},
          {verilog_tokentype::PP_undef, "`undef"},
          {},  // any context
          {},  // any context
          {1, SpacingOptions::kMustWrap},
      },

      // macro definitions
      {
          DefaultStyle,
          {verilog_tokentype::PP_Identifier, "FOO"},
          {verilog_tokentype::PP_define_body, ""}, /* empty */
          {},                                      // any context
          {},                                      // any context
          {0, SpacingOptions::kMustAppend},
      },
      {
          DefaultStyle,
          {verilog_tokentype::PP_Identifier, "FOO"},
          {verilog_tokentype::PP_define_body, "bar"},
          {},  // any context
          {},  // any context
          {1, SpacingOptions::kMustAppend},
      },
      {
          DefaultStyle,
          {verilog_tokentype::PP_Identifier, "BAR"},
          {verilog_tokentype::PP_define_body, "13"},
          {},  // any context
          {},  // any context
          {1, SpacingOptions::kMustAppend},
      },
      {
          DefaultStyle,
          {verilog_tokentype::PP_Identifier, "BAR"},
          {verilog_tokentype::PP_define_body, "\\\n  bar"},
          {},  // any context
          {},  // any context
          {1, SpacingOptions::kMustAppend},
      },
      {
          DefaultStyle,
          {verilog_tokentype::PP_Identifier, "BAR"},
          {verilog_tokentype::PP_define_body, "\\\n  bar \\\n  + foo\n"},
          {},  // any context
          {},  // any context
          {1, SpacingOptions::kPreserve},
      },
      {
          DefaultStyle,
          {')', ")"},
          {verilog_tokentype::PP_define_body, ""}, /* empty */
          {},                                      // any context
          {},                                      // any context
          {0, SpacingOptions::kMustAppend},
      },
      {
          DefaultStyle,
          {')', ")"},
          {verilog_tokentype::PP_define_body, "bar"},
          {},  // any context
          {},  // any context
          {1, SpacingOptions::kMustAppend},
      },
      {
          DefaultStyle,
          {')', ")"},
          {verilog_tokentype::PP_define_body, "13"},
          {},  // any context
          {},  // any context
          {1, SpacingOptions::kMustAppend},
      },
      {
          DefaultStyle,
          {')', ")"},
          {verilog_tokentype::PP_define_body, "\\\n  bar"},
          {},  // any context
          {},  // any context
          {1, SpacingOptions::kMustAppend},
      },
      {
          // e.g. if (x) { ... } (in constraints)
          DefaultStyle,
          {')', ")"},
          {'{', "{"},
          {},  // any context
          {},  // any context
          {1, SpacingOptions::kUndecided},
      },

      // right token = MacroCallId or MacroIdentifier
      {
          DefaultStyle,
          {SymbolIdentifier, "ID"},
          {verilog_tokentype::MacroCallId, "`uvm_foo_macro"},
          {},  // any context
          {},  // any context
          {1, SpacingOptions::kUndecided},
      },
      {
          DefaultStyle,
          {SymbolIdentifier, "ID"},
          {verilog_tokentype::MacroIdentifier, "`uvm_foo_id"},
          {},  // any context
          {},  // any context
          {1, SpacingOptions::kUndecided},
      },
      {
          DefaultStyle,
          {';', ";"},
          {verilog_tokentype::MacroCallId, "`uvm_foo_macro"},
          {},  // any context
          {},  // any context
          {1, SpacingOptions::kUndecided},
      },
      {
          DefaultStyle,
          {';', ";"},
          {verilog_tokentype::MacroIdentifier, "`uvm_foo_id"},
          {},  // any context
          {},  // any context
          {1, SpacingOptions::kUndecided},
      },
      {
          DefaultStyle,
          {TK_EOL_COMMENT, "//comment"},
          {verilog_tokentype::MacroCallId, "`uvm_foo_macro"},
          {},  // any context
          {},  // any context
          {1, SpacingOptions::kMustWrap},
      },
      {
          DefaultStyle,
          {TK_EOL_COMMENT, "//comment"},
          {verilog_tokentype::MacroIdentifier, "`uvm_foo_id"},
          {},  // any context
          {},  // any context
          {1, SpacingOptions::kMustWrap},
      },
      {
          DefaultStyle,
          {PP_else, "`else"},
          {verilog_tokentype::MacroCallId, "`uvm_foo_macro"},
          {},  // any context
          {},  // any context
          {1, SpacingOptions::kMustWrap},
      },
      {
          DefaultStyle,
          {PP_else, "`else"},
          {verilog_tokentype::MacroIdentifier, "`uvm_foo_id"},
          {},  // any context
          {},  // any context
          {1, SpacingOptions::kMustWrap},
      },
      {
          DefaultStyle,
          {PP_endif, "`endif"},
          {verilog_tokentype::MacroCallId, "`uvm_foo_macro"},
          {},  // any context
          {},  // any context
          {1, SpacingOptions::kMustWrap},
      },
      {
          DefaultStyle,
          {PP_endif, "`endif"},
          {verilog_tokentype::MacroIdentifier, "`uvm_foo_id"},
          {},  // any context
          {},  // any context
          {1, SpacingOptions::kMustWrap},
      },
      {
          DefaultStyle,
          {verilog_tokentype::MacroCallId, "`uvm_foo_macro"},
          {verilog_tokentype::MacroCallId, "`uvm_foo_macro"},
          {},  // any context
          {},  // any context
          {1, SpacingOptions::kUndecided},
      },
      {
          DefaultStyle,
          {verilog_tokentype::MacroCallId, "`uvm_foo_macro"},
          {verilog_tokentype::MacroIdentifier, "`uvm_foo_id"},
          {},  // any context
          {},  // any context
          {1, SpacingOptions::kUndecided},
      },
      {
          DefaultStyle,
          {verilog_tokentype::MacroIdentifier, "`uvm_foo_id"},
          {verilog_tokentype::MacroCallId, "`uvm_foo_macro"},
          {},  // any context
          {},  // any context
          {1, SpacingOptions::kUndecided},
      },
      {
          DefaultStyle,
          {verilog_tokentype::MacroIdentifier, "`uvm_foo_id"},
          {verilog_tokentype::MacroIdentifier, "`uvm_foo_id"},
          {},  // any context
          {},  // any context
          {1, SpacingOptions::kUndecided},
      },
      {
          DefaultStyle,
          {')', ")"},
          {verilog_tokentype::MacroCallId, "`uvm_foo_macro"},
          {},  // any context
          {},  // any context
          {1, SpacingOptions::kUndecided},
      },
      {
          DefaultStyle,
          {')', ")"},
          {verilog_tokentype::MacroIdentifier, "`uvm_foo_id"},
          {},  // any context
          {},  // any context
          {1, SpacingOptions::kUndecided},
      },
      {
          DefaultStyle,
          {verilog_tokentype::MacroCallCloseToEndLine, ")"},
          {verilog_tokentype::MacroCallId, "`uvm_foo_macro"},
          {},  // any context
          {},  // any context
          {1, SpacingOptions::kMustWrap},
      },
      {
          DefaultStyle,
          {verilog_tokentype::MacroCallCloseToEndLine, ")"},
          {verilog_tokentype::MacroIdentifier, "`uvm_foo_id"},
          {},  // any context
          {},  // any context
          {1, SpacingOptions::kMustWrap},
      },
      {
          DefaultStyle,
          {verilog_tokentype::MacroCallCloseToEndLine, ")"},
          {';', ";"},
          {},  // any context
          {},  // any context
          {0, SpacingOptions::kUndecided},
      },

      {
          // single-line macro arguments are allowed to move around
          DefaultStyle,
          {',', ","},
          {verilog_tokentype::MacroArg, "abcde"},
          {},  // any context
          {},  // any context
          {1, SpacingOptions::kUndecided},
      },
      {
          // multi-line macro arguments (unlexed) should start own line
          DefaultStyle,
          {',', ","},
          {verilog_tokentype::MacroArg, "a;\nb;"},  // multi-line
          {},                                       // any context
          {},                                       // any context
          {1, SpacingOptions::kMustWrap},
      },

      // Without context, default is to treat '-' as binary.
      {
          DefaultStyle,
          {'-', "-"},                               // left token
          {verilog_tokentype::TK_DecNumber, "42"},  // right token
          {},                                       // context
          {},                                       // context
          {1, SpacingOptions::kUndecided},
      },
      {
          DefaultStyle,
          {'-', "-"},
          {verilog_tokentype::TK_DecNumber, "42"},
          {},  // context
          {NodeEnum::kBinaryExpression},
          {1, SpacingOptions::kUndecided},
      },

      // Handle '-' as a unary prefix expression.
      {
          DefaultStyle,
          {'-', "-"},                               // left token
          {verilog_tokentype::TK_DecNumber, "42"},  // right token
          {},                                       // context
          {NodeEnum::kUnaryPrefixExpression},       // context
          {0, SpacingOptions::kMustAppend},
      },
      {
          DefaultStyle,
          {'-', "-"},
          {verilog_tokentype::SymbolIdentifier, "xyz"},
          {},  // context
          {NodeEnum::kUnaryPrefixExpression},
          {0, SpacingOptions::kMustAppend},
      },
      {
          DefaultStyle,
          {'-', "-"},
          {'(', "("},
          {},  // context
          {NodeEnum::kUnaryPrefixExpression},
          {0, SpacingOptions::kMustAppend},
      },
      {
          DefaultStyle,
          {'-', "-"},
          {verilog_tokentype::MacroIdItem, "`FOO"},
          {},  // context
          {NodeEnum::kUnaryPrefixExpression},
          {0, SpacingOptions::kMustAppend},
      },

      // Handle '&' as binary
      {
          DefaultStyle,
          {'&', "&"},
          {'~', "~"},
          {},  // unspecified context
          {},  // unspecified context
          {1, SpacingOptions::kUndecided},
      },

      // Handle '&' as unary
      {
          DefaultStyle,
          {'&', "&"},
          {verilog_tokentype::TK_DecNumber, "42"},
          {/* any context */},
          {NodeEnum::kUnaryPrefixExpression},
          {0, SpacingOptions::kMustAppend},
      },
      {
          DefaultStyle,
          {'&', "&"},
          {verilog_tokentype::SymbolIdentifier, "foo"},
          {/* any context */},
          {NodeEnum::kUnaryPrefixExpression},
          {0, SpacingOptions::kMustAppend},
      },
      {
          DefaultStyle,
          {'&', "&"},
          {'(', "("},
          {/* any context */},
          {NodeEnum::kUnaryPrefixExpression},
          {0, SpacingOptions::kMustAppend},
      },
      {
          DefaultStyle,
          {'&', "&"},
          {'{', "{"},
          {/* any context */},
          {NodeEnum::kUnaryPrefixExpression},
          {0, SpacingOptions::kMustAppend},
      },

      // Handle '|' as binary
      {
          DefaultStyle,
          {'|', "|"},
          {'~', "~"},
          {/* any context */},
          {/* any context */},
          {1, SpacingOptions::kUndecided},
      },

      // Handle '|' as unary
      {
          DefaultStyle,
          {'|', "|"},
          {verilog_tokentype::TK_DecNumber, "42"},
          {/* any context */},
          {NodeEnum::kUnaryPrefixExpression},
          {0, SpacingOptions::kMustAppend},
      },
      {
          DefaultStyle,
          {'|', "|"},
          {verilog_tokentype::SymbolIdentifier, "foo"},
          {/* any context */},
          {NodeEnum::kUnaryPrefixExpression},
          {0, SpacingOptions::kMustAppend},
      },
      {
          DefaultStyle,
          {'|', "|"},
          {'(', "("},
          {/* any context */},
          {NodeEnum::kUnaryPrefixExpression},
          {0, SpacingOptions::kMustAppend},
      },
      {
          DefaultStyle,
          {'|', "|"},
          {'{', "{"},
          {/* any context */},
          {NodeEnum::kUnaryPrefixExpression},
          {0, SpacingOptions::kMustAppend},
      },

      // Handle '^' as binary
      {
          DefaultStyle,
          {'^', "^"},
          {'~', "~"},
          {/* any context */},
          {/* any context */},
          {1, SpacingOptions::kUndecided},
      },

      // Handle '^' as unary
      {
          DefaultStyle,
          {'^', "^"},
          {verilog_tokentype::TK_DecNumber, "42"},
          {/* any context */},
          {NodeEnum::kUnaryPrefixExpression},
          {0, SpacingOptions::kMustAppend},
      },
      {
          DefaultStyle,
          {'^', "^"},
          {verilog_tokentype::SymbolIdentifier, "foo"},
          {/* any context */},
          {NodeEnum::kUnaryPrefixExpression},
          {0, SpacingOptions::kMustAppend},
      },
      {
          DefaultStyle,
          {'^', "^"},
          {'(', "("},
          {/* any context */},
          {NodeEnum::kUnaryPrefixExpression},
          {0, SpacingOptions::kMustAppend},
      },
      {
          DefaultStyle,
          {'^', "^"},
          {'{', "{"},
          {/* any context */},
          {NodeEnum::kUnaryPrefixExpression},
          {0, SpacingOptions::kMustAppend},
      },

      // Test '~' unary token
      {
          DefaultStyle,
          {'~', "~"},
          {'(', "("},
          {/* any context */},
          {NodeEnum::kUnaryPrefixExpression},
          {0, SpacingOptions::kMustAppend},
      },
      {
          DefaultStyle,
          {'~', "~"},
          {verilog_tokentype::SymbolIdentifier, "foo"},
          {/* any context */},
          {NodeEnum::kUnaryPrefixExpression},
          {0, SpacingOptions::kMustAppend},
      },

      // Test '##' unary (delay) operator
      {
          DefaultStyle,
          {verilog_tokentype::TK_POUNDPOUND, "##"},
          {'(', "("},
          {/* any context */},
          {/* any context */},
          {0, SpacingOptions::kMustAppend},
      },
      {
          DefaultStyle,
          {verilog_tokentype::TK_POUNDPOUND, "##"},
          {verilog_tokentype::TK_DecNumber, "10"},
          {/* any context */},
          {/* any context */},
          {0, SpacingOptions::kMustAppend},
      },
      {
          DefaultStyle,
          {verilog_tokentype::TK_POUNDPOUND, "##"},
          {verilog_tokentype::SymbolIdentifier, "x_delay"},
          {/* any context */},
          {/* any context */},
          {0, SpacingOptions::kMustAppend},
      },
      {
          DefaultStyle,
          {verilog_tokentype::TK_POUNDPOUND, "##"},
          {verilog_tokentype::MacroIdentifier, "`X_DELAY"},
          {/* any context */},
          {/* any context */},
          {0, SpacingOptions::kMustAppend},
      },
      {
          DefaultStyle,
          {verilog_tokentype::TK_POUNDPOUND, "##"},
          {verilog_tokentype::TK_LP, "'{"},
          {/* any context */},
          {/* any context */},
          {0, SpacingOptions::kMustAppend},
      },
      {
          DefaultStyle,
          {verilog_tokentype::TK_POUNDPOUND, "##"},
          {'[', "["},
          {/* any context */},
          {/* any context */},
          {0, SpacingOptions::kMustAppend},
      },
      {
          DefaultStyle,
          {verilog_tokentype::TK_POUNDPOUND, "##"},
          {verilog_tokentype::TK_LBSTARRB, "[*]"},
          {/* any context */},
          {/* any context */},
          {0, SpacingOptions::kMustAppend},
      },
      {
          DefaultStyle,
          {verilog_tokentype::TK_POUNDPOUND, "##"},
          {verilog_tokentype::TK_LBPLUSRB, "[+]"},
          {/* any context */},
          {/* any context */},
          {0, SpacingOptions::kMustAppend},
      },
      {
          DefaultStyle,
          {verilog_tokentype::SymbolIdentifier, "predicate"},
          {verilog_tokentype::TK_POUNDPOUND, "##"},
          {/* any context */},
          {/* any context */},
          {1, SpacingOptions::kUndecided},
      },
      {
          DefaultStyle,
          {')', ")"},
          {verilog_tokentype::TK_POUNDPOUND, "##"},
          {/* any context */},
          {/* any context */},
          {1, SpacingOptions::kUndecided},
      },
      {
          DefaultStyle,
          {'(', "("},
          {verilog_tokentype::TK_POUNDPOUND, "##"},
          {/* any context */},
          {/* any context */},
          {0, SpacingOptions::kUndecided},
      },
      {
          DefaultStyle,
          {verilog_tokentype::TK_and, "and"},
          {verilog_tokentype::TK_POUNDPOUND, "##"},
          {/* any context */},
          {/* any context */},
          {1, SpacingOptions::kUndecided},
      },
      {
          DefaultStyle,
          {verilog_tokentype::TK_or, "or"},
          {verilog_tokentype::TK_POUNDPOUND, "##"},
          {/* any context */},
          {/* any context */},
          {1, SpacingOptions::kUndecided},
      },
      {
          DefaultStyle,
          {verilog_tokentype::TK_intersect, "intersect"},
          {verilog_tokentype::TK_POUNDPOUND, "##"},
          {/* any context */},
          {/* any context */},
          {1, SpacingOptions::kUndecided},
      },
      {
          DefaultStyle,
          {verilog_tokentype::TK_throughout, "throughout"},
          {verilog_tokentype::TK_POUNDPOUND, "##"},
          {/* any context */},
          {/* any context */},
          {1, SpacingOptions::kUndecided},
      },
      {
          DefaultStyle,
          {verilog_tokentype::TK_within, "within"},
          {verilog_tokentype::TK_POUNDPOUND, "##"},
          {/* any context */},
          {/* any context */},
          {1, SpacingOptions::kUndecided},
      },

      // Two unary operators
      {
          DefaultStyle,
          {'~', "~"},
          {'~', "~"},
          {/* any context */},
          {NodeEnum::kUnaryPrefixExpression},
          {0, SpacingOptions::kMustAppend},
      },

      // Handle '->' as a unary prefix expression.
      {
          DefaultStyle,
          {TK_TRIGGER, "->"},
          {verilog_tokentype::SymbolIdentifier, "a"},
          {/* any context */},              // context
          {/* any context */},              // context
          {0, SpacingOptions::kUndecided},  // could be MustAppend though
      },
      {
          DefaultStyle,
          {TK_NONBLOCKING_TRIGGER, "->>"},
          {verilog_tokentype::SymbolIdentifier, "a"},
          {/* any context */},              // context
          {/* any context */},              // context
          {0, SpacingOptions::kUndecided},  // could be MustAppend though
      },

      // Handle '->' as a binary operator
      {
          DefaultStyle,
          {TK_LOGICAL_IMPLIES, "->"},
          {verilog_tokentype::SymbolIdentifier, "right"},
          {/* any context */},  // context
          {/* any context */},  // context
          {1, SpacingOptions::kUndecided},
      },
      {
          DefaultStyle,
          {verilog_tokentype::SymbolIdentifier, "left"},
          {TK_LOGICAL_IMPLIES, "->"},
          {/* any context */},  // context
          {/* any context */},  // context
          {1, SpacingOptions::kUndecided},
      },
      {
          DefaultStyle,
          {TK_CONSTRAINT_IMPLIES, "->"},
          {verilog_tokentype::SymbolIdentifier, "right"},
          {/* any context */},  // context
          {/* any context */},  // context
          {1, SpacingOptions::kUndecided},
      },
      {
          DefaultStyle,
          {verilog_tokentype::SymbolIdentifier, "left"},
          {TK_CONSTRAINT_IMPLIES, "->"},
          {/* any context */},  // context
          {/* any context */},  // context
          {1, SpacingOptions::kUndecided},
      },

      // Inside dimension ranges, force space preservation if not around ':'
      {
          DefaultStyle,
          {'*', "*"},
          {verilog_tokentype::SymbolIdentifier, "foo"},
          {/* any context */},
          {/* any context */},
          {1, SpacingOptions::kUndecided},
      },
      {
          DefaultStyle,
          {verilog_tokentype::SymbolIdentifier, "foo"},
          {'*', "*"},
          {/* any context */},
          {/* any context */},
          {1, SpacingOptions::kUndecided},
      },
      {
          DefaultStyle,
          {':', ":"},
          {verilog_tokentype::SymbolIdentifier, "foo"},
          {/* any context */},
          {NodeEnum::kDimensionRange},
          {0, SpacingOptions::kUndecided},
      },
      {
          DefaultStyle,
          {verilog_tokentype::SymbolIdentifier, "foo"},
          {':', ":"},
          {/* any context */},
          {NodeEnum::kDimensionRange},
          {0, SpacingOptions::kUndecided},
      },

      // spacing between ranges of multi-dimension arrays
      {
          DefaultStyle,
          {']', "]"},
          {'[', "["},
          {},  // any context
          {},  // any context
          {0, SpacingOptions::kUndecided},
      },

      // spacing before first '[' of packed arrays in declarations
      {
          DefaultStyle,
          {verilog_tokentype::TK_logic, "logic"},
          {'[', "["},
          {/* any context */},
          {/* any context */},
          {0, SpacingOptions::kUndecided},
      },
      {
          DefaultStyle,
          {verilog_tokentype::SymbolIdentifier, "mytype1"},
          {'[', "["},
          {/* any context */},
          {},  // unspecified context, this covers index expressions
          {0, SpacingOptions::kUndecided},
      },
      {
          DefaultStyle,
          {verilog_tokentype::TK_logic, "logic"},
          {'[', "["},
          {/* any context */},
          {NodeEnum::kPackedDimensions},
          {1, SpacingOptions::kUndecided},
      },
      {
          DefaultStyle,
          {verilog_tokentype::SymbolIdentifier, "mytype2"},
          {'[', "["},
          {/* any context */},
          {NodeEnum::kPackedDimensions},
          {1, SpacingOptions::kUndecided},
      },
      {
          DefaultStyle,
          {verilog_tokentype::SymbolIdentifier, "id1"},
          {'[', "["},
          {/* any context */},
          {NodeEnum::kPackedDimensions, NodeEnum::kExpression},
          {0, SpacingOptions::kUndecided},
      },

      // spacing after last ']' of packed arrays in declarations
      {
          DefaultStyle,
          {']', "]"},
          {verilog_tokentype::SymbolIdentifier, "id_a"},
          {/* any context */},
          {/* any context */},
          {1, SpacingOptions::kUndecided},
      },
      {
          DefaultStyle,
          {']', "]"},
          {verilog_tokentype::SymbolIdentifier, "id_b"},
          {/* any context */},
          {NodeEnum::kUnqualifiedId},
          {1, SpacingOptions::kUndecided},
      },
      {
          DefaultStyle,
          {']', "]"},
          {verilog_tokentype::SymbolIdentifier, "id_c"},
          {/* any context */},
          {NodeEnum::kDataTypeImplicitBasicIdDimensions,
           NodeEnum::kUnqualifiedId},
          {1, SpacingOptions::kUndecided},
      },

      // "foo ()" in "module foo();"
      {
          DefaultStyle,
          {verilog_tokentype::SymbolIdentifier, "foo"},
          {'(', "("},
          {/* any context */},
          {/* unspecified context */},
          {0, SpacingOptions::kUndecided},
      },
      {
          DefaultStyle,
          {verilog_tokentype::SymbolIdentifier, "foo"},
          {'(', "("},
          {NodeEnum::kModuleHeader},
          {/* any context */},
          {1, SpacingOptions::kUndecided},
      },

      // "a(" in "foo bar (.a(b));": instantiation with named ports
      {
          DefaultStyle,
          {verilog_tokentype::SymbolIdentifier, "foo"},
          {'(', "("},
          {NodeEnum::kGateInstance},
          {NodeEnum::kGateInstance},
          {1, SpacingOptions::kUndecided},
      },
      {
          DefaultStyle,
          {verilog_tokentype::SymbolIdentifier, "foo"},
          {'(', "("},
          {/* any context */},
          {NodeEnum::kPrimitiveGateInstance},
          {1, SpacingOptions::kUndecided},
      },
      {
          DefaultStyle,
          {verilog_tokentype::SymbolIdentifier, "foo"},
          {'(', "("},
          {/* any context */},
          {NodeEnum::kActualNamedPort},
          {0, SpacingOptions::kUndecided},
      },
      {
          DefaultStyle,
          {verilog_tokentype::SymbolIdentifier, "foo"},
          {'(', "("},
          {/* any context */},
          {NodeEnum::kGateInstance, NodeEnum::kActualNamedPort},
          {0, SpacingOptions::kUndecided},
      },
      {
          DefaultStyle,
          {verilog_tokentype::SymbolIdentifier, "foo"},
          {'(', "("},
          {/* any context */},
          {NodeEnum::kModuleHeader, NodeEnum::kPort},
          {0, SpacingOptions::kUndecided},
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
          // a ? b : c (condition expression)
          DefaultStyle,
          {verilog_tokentype::SymbolIdentifier, "b"},
          {':', ":"},
          {/* any context */},
          {NodeEnum::kConditionExpression},
          {1, SpacingOptions::kUndecided},
      },
      {
          // a ? 111 : c (condition expression)
          DefaultStyle,
          {verilog_tokentype::TK_DecNumber, "111"},
          {':', ":"},
          {/* any context */},
          {NodeEnum::kConditionExpression},
          {1, SpacingOptions::kUndecided},
      },
      {
          // a ? "1" : c (condition expression)
          DefaultStyle,
          {verilog_tokentype::TK_StringLiteral, "\"1\""},
          {':', ":"},
          {/* any context */},
          {NodeEnum::kConditionExpression},
          {1, SpacingOptions::kUndecided},
      },
      {
          // a ? (1) : c (condition expression)
          DefaultStyle,
          {')', ":"},
          {':', ":"},
          {/* any context */},
          {NodeEnum::kConditionExpression},
          {1, SpacingOptions::kUndecided},
      },
      {
          // a ? {b} : {c} (condition expression)
          DefaultStyle,
          {'}', "}"},
          {':', ":"},
          {/* any context */},
          {NodeEnum::kConditionExpression},
          {1, SpacingOptions::kUndecided},
      },
      {
          // a ? {b} : {c} (condition expression)
          DefaultStyle,
          {':', ":"},
          {'{', "{"},
          {/* any context */},
          {NodeEnum::kConditionExpression},
          {1, SpacingOptions::kUndecided},
      },

      // ':' on the left, anything else on the right
      {
          DefaultStyle,
          {':', ":"},
          {verilog_tokentype::SymbolIdentifier, "x"},
          {/* any context */},
          {/* unspecified context */},
          {1, SpacingOptions::kUndecided},
      },
      {
          // a ? b : c (condition expression)
          DefaultStyle,
          {':', ":"},
          {verilog_tokentype::SymbolIdentifier, "c"},
          {/* any context */},
          {NodeEnum::kConditionExpression},
          {1, SpacingOptions::kUndecided},
      },
      {
          // a ? b : 7 (condition expression)
          DefaultStyle,
          {':', ":"},
          {verilog_tokentype::TK_DecNumber, "7"},
          {/* any context */},
          {NodeEnum::kConditionExpression},
          {1, SpacingOptions::kUndecided},
      },
      {
          // a ? b : "7" (condition expression)
          DefaultStyle,
          {':', ":"},
          {verilog_tokentype::TK_StringLiteral, "\"7\""},
          {/* any context */},
          {NodeEnum::kConditionExpression},
          {1, SpacingOptions::kUndecided},
      },
      {
          // a ? b : (7) (condition expression)
          DefaultStyle,
          {':', ":"},
          {'(', "("},
          {/* any context */},
          {NodeEnum::kConditionExpression},
          {1, SpacingOptions::kUndecided},
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
          {1, SpacingOptions::kUndecided},
      },
      {
          // ": begin"
          DefaultStyle,
          {':', ":"},
          {verilog_tokentype::TK_begin, "begin"},
          {/* unspecified context */},
          {/* unspecified context */},
          {1, SpacingOptions::kUndecided},
      },
      {
          // "fork :"
          DefaultStyle,
          {verilog_tokentype::TK_fork, "fork"},
          {':', ":"},
          {/* unspecified context */},
          {/* unspecified context */},
          {1, SpacingOptions::kUndecided},
      },
      {
          // "end :"
          DefaultStyle,
          {verilog_tokentype::TK_end, "end"},
          {':', ":"},
          {/* unspecified context */},
          {/* unspecified context */},
          {1, SpacingOptions::kUndecided},
      },
      {
          // "endclass :"
          DefaultStyle,
          {verilog_tokentype::TK_endclass, "endclass"},
          {':', ":"},
          {/* unspecified context */},
          {/* unspecified context */},
          {1, SpacingOptions::kUndecided},
      },
      {
          // "endfunction :"
          DefaultStyle,
          {verilog_tokentype::TK_endfunction, "endfunction"},
          {':', ":"},
          {/* unspecified context */},
          {/* unspecified context */},
          {1, SpacingOptions::kUndecided},
      },
      {
          // "endtask :"
          DefaultStyle,
          {verilog_tokentype::TK_endtask, "endtask"},
          {':', ":"},
          {/* unspecified context */},
          {/* unspecified context */},
          {1, SpacingOptions::kUndecided},
      },
      {
          // "endmodule :"
          DefaultStyle,
          {verilog_tokentype::TK_endmodule, "endmodule"},
          {':', ":"},
          {/* unspecified context */},
          {/* unspecified context */},
          {1, SpacingOptions::kUndecided},
      },
      {
          // "endpackage :"
          DefaultStyle,
          {verilog_tokentype::TK_endpackage, "endpackage"},
          {':', ":"},
          {/* unspecified context */},
          {/* unspecified context */},
          {1, SpacingOptions::kUndecided},
      },
      {
          // "endinterface :"
          DefaultStyle,
          {verilog_tokentype::TK_endinterface, "endinterface"},
          {':', ":"},
          {/* unspecified context */},
          {/* unspecified context */},
          {1, SpacingOptions::kUndecided},
      },
      {
          // "endproperty :"
          DefaultStyle,
          {verilog_tokentype::TK_endproperty, "endproperty"},
          {':', ":"},
          {/* unspecified context */},
          {/* unspecified context */},
          {1, SpacingOptions::kUndecided},
      },
      {
          // "endclocking :"
          DefaultStyle,
          {verilog_tokentype::TK_endclocking, "endclocking"},
          {':', ":"},
          {/* unspecified context */},
          {/* unspecified context */},
          {1, SpacingOptions::kUndecided},
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
          {1, SpacingOptions::kUndecided},
      },
      {
          // "id : begin ..."
          DefaultStyle,
          {verilog_tokentype::SymbolIdentifier, "id"},
          {':', ":"},
          {/* unspecified context */},
          {NodeEnum::kLabeledStatement},
          {1, SpacingOptions::kUndecided},
      },
      {
          // "id :"
          DefaultStyle,
          {verilog_tokentype::SymbolIdentifier, "id"},
          {':', ":"},
          {/* unspecified context */},
          {NodeEnum::kCaseItem},
          {0, SpacingOptions::kUndecided},
      },
      {
          // "id :"
          DefaultStyle,
          {verilog_tokentype::SymbolIdentifier, "id"},
          {':', ":"},
          {/* unspecified context */},
          {NodeEnum::kCaseInsideItem},
          {0, SpacingOptions::kUndecided},
      },
      {
          // "id :"
          DefaultStyle,
          {verilog_tokentype::SymbolIdentifier, "id"},
          {':', ":"},
          {/* unspecified context */},
          {NodeEnum::kCasePatternItem},
          {0, SpacingOptions::kUndecided},
      },
      {
          // "id :"
          DefaultStyle,
          {verilog_tokentype::SymbolIdentifier, "id"},
          {':', ":"},
          {/* unspecified context */},
          {NodeEnum::kGenerateCaseItem},
          {0, SpacingOptions::kUndecided},
      },
      {
          // "id :"
          DefaultStyle,
          {verilog_tokentype::SymbolIdentifier, "id"},
          {':', ":"},
          {/* unspecified context */},
          {NodeEnum::kPropertyCaseItem},
          {0, SpacingOptions::kUndecided},
      },
      {
          // "id :"
          DefaultStyle,
          {verilog_tokentype::SymbolIdentifier, "id"},
          {':', ":"},
          {/* unspecified context */},
          {NodeEnum::kRandSequenceCaseItem},
          {0, SpacingOptions::kUndecided},
      },
      {
          // ": id"
          DefaultStyle,
          {':', ":"},
          {verilog_tokentype::SymbolIdentifier, "id"},
          {/* unspecified context */},
          {/* unspecified context */},
          {1, SpacingOptions::kUndecided},
      },
      {
          // ": id"
          DefaultStyle,
          {':', ":"},
          {verilog_tokentype::SymbolIdentifier, "id"},
          {/* unspecified context */},
          {NodeEnum::kLabel},
          {1, SpacingOptions::kUndecided},
      },
      // Shift operators
      {
          // foo = 1 << width;
          DefaultStyle,
          {verilog_tokentype::TK_DecNumber, "1"},
          {verilog_tokentype::TK_LS, "<<"},
          {/* unspecified context */},
          {/* unspecified context */},
          {1, SpacingOptions::kUndecided},
      },
      {
          // foo = 1 << width;
          DefaultStyle,
          {verilog_tokentype::TK_LS, "<<"},
          {verilog_tokentype::SymbolIdentifier, "width"},
          {/* unspecified context */},
          {/* unspecified context */},
          {1, SpacingOptions::kUndecided},
      },
      {
          // foo = bar << 4;
          DefaultStyle,
          {verilog_tokentype::SymbolIdentifier, "bar"},
          {verilog_tokentype::TK_LS, "<<"},
          {/* unspecified context */},
          {/* unspecified context */},
          {1, SpacingOptions::kUndecided},
      },
      {
          // foo = bar << 4;
          DefaultStyle,
          {verilog_tokentype::TK_LS, "<<"},
          {verilog_tokentype::TK_DecNumber, "4"},
          {/* unspecified context */},
          {/* unspecified context */},
          {1, SpacingOptions::kUndecided},
      },
      {
          // foo = `VAL << 4;
          DefaultStyle,
          {verilog_tokentype::MacroIdentifier, "`VAL"},
          {verilog_tokentype::TK_LS, "<<"},
          {/* unspecified context */},
          {/* unspecified context */},
          {1, SpacingOptions::kUndecided},
      },
      {
          // foo = bar << `SIZE;
          DefaultStyle,
          {verilog_tokentype::TK_LS, "<<"},
          {verilog_tokentype::MacroIdentifier, "`SIZE"},
          {/* unspecified context */},
          {/* unspecified context */},
          {1, SpacingOptions::kUndecided},
      },
      {
          // foo = 1 >> width;
          DefaultStyle,
          {verilog_tokentype::TK_DecNumber, "1"},
          {verilog_tokentype::TK_RS, ">>"},
          {/* unspecified context */},
          {/* unspecified context */},
          {1, SpacingOptions::kUndecided},
      },
      {
          // foo = 1 >> width;
          DefaultStyle,
          {verilog_tokentype::TK_RS, ">>"},
          {verilog_tokentype::SymbolIdentifier, "width"},
          {/* unspecified context */},
          {/* unspecified context */},
          {1, SpacingOptions::kUndecided},
      },
      {
          // foo = bar >> 4;
          DefaultStyle,
          {verilog_tokentype::SymbolIdentifier, "bar"},
          {verilog_tokentype::TK_RS, ">>"},
          {/* unspecified context */},
          {/* unspecified context */},
          {1, SpacingOptions::kUndecided},
      },
      {
          // foo = bar >> 4;
          DefaultStyle,
          {verilog_tokentype::TK_RS, ">>"},
          {verilog_tokentype::TK_DecNumber, "4"},
          {/* unspecified context */},
          {/* unspecified context */},
          {1, SpacingOptions::kUndecided},
      },
      {
          // foo = `VAL >> 4;
          DefaultStyle,
          {verilog_tokentype::MacroIdentifier, "`VAL"},
          {verilog_tokentype::TK_RS, ">>"},
          {/* unspecified context */},
          {/* unspecified context */},
          {1, SpacingOptions::kUndecided},
      },
      {
          // foo = bar >> `SIZE;
          DefaultStyle,
          {verilog_tokentype::TK_RS, ">>"},
          {verilog_tokentype::MacroIdentifier, "`SIZE"},
          {/* unspecified context */},
          {/* unspecified context */},
          {1, SpacingOptions::kUndecided},
      },
      // Streaming operators
      {
          // foo = {<<{bar}};
          DefaultStyle,
          {'=', "="},
          {'{', "{"},
          {/* unspecified context */},
          {NodeEnum::kStreamingConcatenation},
          {1, SpacingOptions::kUndecided},
      },
      {
          // foo = {<<{bar}};
          DefaultStyle,
          {'{', "{"},
          {verilog_tokentype::TK_LS, "<<"},
          {/* unspecified context */},
          {NodeEnum::kStreamingConcatenation},
          {0, SpacingOptions::kUndecided},
      },
      {
          // foo = {<<{bar}};
          DefaultStyle,
          {verilog_tokentype::TK_LS, "<<"},
          {'{', "{"},
          {/* unspecified context */},
          {NodeEnum::kStreamingConcatenation},
          {0, SpacingOptions::kUndecided},
      },
      {
          // foo = {<<{bar}};
          DefaultStyle,
          {'{', "{"},
          {verilog_tokentype::SymbolIdentifier, "bar"},
          {/* unspecified context */},
          {NodeEnum::kStreamingConcatenation},
          {0, SpacingOptions::kUndecided},
      },
      {
          // foo = {<<{bar}};
          DefaultStyle,
          {verilog_tokentype::SymbolIdentifier, "bar"},
          {'}', "}"},
          {/* unspecified context */},
          {NodeEnum::kStreamingConcatenation},
          {0, SpacingOptions::kUndecided},
      },
      {
          // foo = {<<4{bar}};
          DefaultStyle,
          {verilog_tokentype::TK_LS, "<<"},
          {verilog_tokentype::TK_DecNumber, "4"},
          {/* unspecified context */},
          {NodeEnum::kStreamingConcatenation},
          {0, SpacingOptions::kUndecided},
      },
      {
          // foo = {<<4{bar}};
          DefaultStyle,
          {verilog_tokentype::TK_DecNumber, "4"},
          {'{', "{"},
          {/* unspecified context */},
          {NodeEnum::kStreamingConcatenation},
          {0, SpacingOptions::kUndecided},
      },
      {
          // foo = {<<byte{bar}};
          DefaultStyle,
          {verilog_tokentype::TK_LS, "<<"},
          {verilog_tokentype::TK_byte, "byte"},
          {/* unspecified context */},
          {NodeEnum::kStreamingConcatenation},
          {0, SpacingOptions::kUndecided},
      },
      {
          // foo = {<<byte{bar}};
          DefaultStyle,
          {verilog_tokentype::TK_byte, "byte"},
          {'{', "{"},
          {/* unspecified context */},
          {NodeEnum::kStreamingConcatenation},
          {0, SpacingOptions::kUndecided},
      },
      {
          // foo = {<<type_t{bar}};
          DefaultStyle,
          {verilog_tokentype::TK_LS, "<<"},
          {verilog_tokentype::SymbolIdentifier, "type_t"},
          {/* unspecified context */},
          {NodeEnum::kStreamingConcatenation},
          {0, SpacingOptions::kUndecided},
      },
      {
          // foo = {<<type_t{bar}};
          DefaultStyle,
          {verilog_tokentype::SymbolIdentifier, "type_t"},
          {'{', "{"},
          {/* unspecified context */},
          {NodeEnum::kStreamingConcatenation},
          {0, SpacingOptions::kUndecided},
      },
      {
          // foo = {<<`GET_TYPE{bar}};
          DefaultStyle,
          {verilog_tokentype::TK_LS, "<<"},
          {verilog_tokentype::MacroIdentifier, "`GET_TYPE"},
          {/* unspecified context */},
          {NodeEnum::kStreamingConcatenation},
          {0, SpacingOptions::kUndecided},
      },
      {
          // foo = {<<`GET_TYPE{bar}};
          DefaultStyle,
          {verilog_tokentype::MacroIdentifier, "`GET_TYPE"},
          {'{', "{"},
          {/* unspecified context */},
          {NodeEnum::kStreamingConcatenation},
          {0, SpacingOptions::kUndecided},
      },
      {
          // foo = {>>{bar}};
          DefaultStyle,
          {'=', "="},
          {'{', "{"},
          {/* unspecified context */},
          {NodeEnum::kStreamingConcatenation},
          {1, SpacingOptions::kUndecided},
      },
      {
          // foo = {>>{bar}};
          DefaultStyle,
          {'{', "{"},
          {verilog_tokentype::TK_RS, ">>"},
          {/* unspecified context */},
          {NodeEnum::kStreamingConcatenation},
          {0, SpacingOptions::kUndecided},
      },
      {
          // foo = {>>{bar}};
          DefaultStyle,
          {verilog_tokentype::TK_RS, ">>"},
          {'{', "{"},
          {/* unspecified context */},
          {NodeEnum::kStreamingConcatenation},
          {0, SpacingOptions::kUndecided},
      },
      {
          // foo = {>>{bar}};
          DefaultStyle,
          {'{', "{"},
          {verilog_tokentype::SymbolIdentifier, "bar"},
          {/* unspecified context */},
          {NodeEnum::kStreamingConcatenation},
          {0, SpacingOptions::kUndecided},
      },
      {
          // foo = {>>{bar}};
          DefaultStyle,
          {verilog_tokentype::SymbolIdentifier, "bar"},
          {'}', "}"},
          {/* unspecified context */},
          {NodeEnum::kStreamingConcatenation},
          {0, SpacingOptions::kUndecided},
      },
      {
          // foo = {>>4{bar}};
          DefaultStyle,
          {verilog_tokentype::TK_RS, ">>"},
          {verilog_tokentype::TK_DecNumber, "4"},
          {/* unspecified context */},
          {NodeEnum::kStreamingConcatenation},
          {0, SpacingOptions::kUndecided},
      },
      {
          // foo = {>>4{bar}};
          DefaultStyle,
          {verilog_tokentype::TK_DecNumber, "4"},
          {'{', "{"},
          {/* unspecified context */},
          {NodeEnum::kStreamingConcatenation},
          {0, SpacingOptions::kUndecided},
      },
      {
          // foo = {>>byte{bar}};
          DefaultStyle,
          {verilog_tokentype::TK_RS, ">>"},
          {verilog_tokentype::TK_byte, "byte"},
          {/* unspecified context */},
          {NodeEnum::kStreamingConcatenation},
          {0, SpacingOptions::kUndecided},
      },
      {
          // foo = {>>byte{bar}};
          DefaultStyle,
          {verilog_tokentype::TK_byte, "byte"},
          {'{', "{"},
          {/* unspecified context */},
          {NodeEnum::kStreamingConcatenation},
          {0, SpacingOptions::kUndecided},
      },
      {
          // foo = {>>type_t{bar}};
          DefaultStyle,
          {verilog_tokentype::TK_RS, ">>"},
          {verilog_tokentype::SymbolIdentifier, "type_t"},
          {/* unspecified context */},
          {NodeEnum::kStreamingConcatenation},
          {0, SpacingOptions::kUndecided},
      },
      {
          // foo = {>>type_t{bar}};
          DefaultStyle,
          {verilog_tokentype::SymbolIdentifier, "type_t"},
          {'{', "{"},
          {/* unspecified context */},
          {NodeEnum::kStreamingConcatenation},
          {0, SpacingOptions::kUndecided},
      },
      {
          // foo = {>>`GET_TYPE{bar}};
          DefaultStyle,
          {verilog_tokentype::TK_RS, ">>"},
          {verilog_tokentype::MacroIdentifier, "`GET_TYPE"},
          {/* unspecified context */},
          {NodeEnum::kStreamingConcatenation},
          {0, SpacingOptions::kUndecided},
      },
      {
          // foo = {>>`GET_TYPE{bar}};
          DefaultStyle,
          {verilog_tokentype::MacroIdentifier, "`GET_TYPE"},
          {'{', "{"},
          {/* unspecified context */},
          {NodeEnum::kStreamingConcatenation},
          {0, SpacingOptions::kUndecided},
      },
      // ':' in bit slicing and array indexing
      {
          // [1:0]
          DefaultStyle,
          {verilog_tokentype::TK_DecNumber, "1"},
          {':', ":"},
          {/* unspecified context */},
          {NodeEnum::kDimensionRange},
          // no spaces preceding ':' in unit test context
          {0, SpacingOptions::kUndecided},
      },
      {
          // [1:0]
          DefaultStyle,
          {':', ":"},
          {verilog_tokentype::TK_DecNumber, "0"},
          {/* unspecified context */},
          {NodeEnum::kDimensionRange},
          // no spaces preceding ':' in unit test context
          {0, SpacingOptions::kUndecided},
      },
      {
          // [a:b]
          DefaultStyle,
          {SymbolIdentifier, "a"},
          {':', ":"},
          {/* unspecified context */},
          {NodeEnum::kDimensionRange},
          // no spaces preceding ':' in unit test context
          {0, SpacingOptions::kUndecided},
      },
      {
          // [a:b]
          DefaultStyle,
          {':', ":"},
          {SymbolIdentifier, "b"},
          {/* unspecified context */},
          {NodeEnum::kDimensionRange},
          // no spaces preceding ':' in unit test context
          {0, SpacingOptions::kUndecided},
      },
      {
          // [1:0]
          DefaultStyle,
          {verilog_tokentype::TK_DecNumber, "1"},
          {':', ":"},
          {/* unspecified context */},
          {NodeEnum::kDimensionRange},
          // no spaces preceding ':' in unit test context
          {0, SpacingOptions::kUndecided},
      },
      {
          // [1:0]
          DefaultStyle,
          {':', ":"},
          {verilog_tokentype::TK_DecNumber, "0"},
          {/* unspecified context */},
          {NodeEnum::kDimensionRange},
          // no spaces preceding ':' in unit test context
          {0, SpacingOptions::kUndecided},
      },
      {
          // [a:b]
          DefaultStyle,
          {SymbolIdentifier, "a"},
          {':', ":"},
          {/* unspecified context */},
          {NodeEnum::kDimensionRange},
          // no spaces preceding ':' in unit test context
          {0, SpacingOptions::kUndecided},
      },
      {
          // [a:b]
          DefaultStyle,
          {':', ":"},
          {SymbolIdentifier, "b"},
          {/* unspecified context */},
          {NodeEnum::kDimensionRange},
          // no spaces preceding ':' in unit test context
          {0, SpacingOptions::kUndecided},
      },
      {
          // [1:0]
          DefaultStyle,
          {verilog_tokentype::TK_DecNumber, "1"},
          {':', ":"},
          {/* unspecified context */},
          {NodeEnum::kDimensionSlice},
          // no spaces preceding ':' in unit test context
          {0, SpacingOptions::kUndecided},
      },
      {
          // [1:0]
          DefaultStyle,
          {':', ":"},
          {verilog_tokentype::TK_DecNumber, "0"},
          {/* unspecified context */},
          {NodeEnum::kDimensionSlice},
          // no spaces preceding ':' in unit test context
          {0, SpacingOptions::kUndecided},
      },
      {
          // [a:b]
          DefaultStyle,
          {SymbolIdentifier, "a"},
          {':', ":"},
          {/* unspecified context */},
          {NodeEnum::kDimensionSlice},
          // no spaces preceding ':' in unit test context
          {0, SpacingOptions::kUndecided},
      },
      {
          // [a:b]
          DefaultStyle,
          {':', ":"},
          {SymbolIdentifier, "b"},
          {/* unspecified context */},
          {NodeEnum::kDimensionSlice},
          // no spaces preceding ':' in unit test context
          {0, SpacingOptions::kUndecided},
      },
      {
          // [1:0]
          DefaultStyle,
          {verilog_tokentype::TK_DecNumber, "1"},
          {':', ":"},
          {/* any context */},
          {NodeEnum::kCycleDelayRange},
          // no spaces preceding ':' in unit test context
          {0, SpacingOptions::kUndecided},
      },
      {
          // [1:0]
          DefaultStyle,
          {':', ":"},
          {verilog_tokentype::TK_DecNumber, "0"},
          {/* any context */},
          {NodeEnum::kCycleDelayRange},
          // no spaces preceding ':' in unit test context
          {0, SpacingOptions::kUndecided},
      },
      {
          // [a:b]
          DefaultStyle,
          {SymbolIdentifier, "a"},
          {':', ":"},
          {/* any context */},
          {NodeEnum::kCycleDelayRange},
          // no spaces preceding ':' in unit test context
          {0, SpacingOptions::kUndecided},
      },
      {
          // [a:b]
          DefaultStyle,
          {':', ":"},
          {SymbolIdentifier, "b"},
          {/* any context */},
          {NodeEnum::kCycleDelayRange},
          // no spaces preceding ':' in unit test context
          {0, SpacingOptions::kUndecided},
      },
      {
          // [1:0]
          DefaultStyle,
          {verilog_tokentype::TK_DecNumber, "1"},
          {':', ":"},
          {/* unspecified context */},
          {NodeEnum::kValueRange},
          // no spaces preceding ':' in unit test context
          {1, SpacingOptions::kUndecided},
      },
      {
          // [1:0]
          DefaultStyle,
          {':', ":"},
          {verilog_tokentype::TK_DecNumber, "0"},
          {/* unspecified context */},
          {NodeEnum::kValueRange},
          // no spaces preceding ':' in unit test context
          {1, SpacingOptions::kUndecided},
      },
      {
          // [a:b]
          DefaultStyle,
          {SymbolIdentifier, "a"},
          {':', ":"},
          {/* unspecified context */},
          {NodeEnum::kValueRange},
          // no spaces preceding ':' in unit test context
          {1, SpacingOptions::kUndecided},
      },
      {
          // [a:b]
          DefaultStyle,
          {':', ":"},
          {SymbolIdentifier, "b"},
          {/* unspecified context */},
          {NodeEnum::kValueRange},
          // no spaces preceding ':' in unit test context
          {1, SpacingOptions::kUndecided},
      },

      {
          // "] {" in "typedef logic [N] { ..."
          // where [N] is a packed dimension
          DefaultStyle,
          {']', "]"},
          {'{', "{"},
          {NodeEnum::kPackedDimensions},
          {/* unspecified context */},
          {1, SpacingOptions::kUndecided},
      },
      {
          // "] {" in "typedef logic [M:N] { ..."
          // where [M:N] is a packed dimension
          DefaultStyle,
          {']', "]"},
          {'{', "{"},
          {NodeEnum::kPackedDimensions},
          {/* unspecified context */},
          {1, SpacingOptions::kUndecided},
      },
      {
          // "]{" in other contexts
          DefaultStyle,
          {']', "]"},
          {'{', "{"},
          {/* unspecified context */},
          {/* unspecified context */},
          {0, SpacingOptions::kUndecided},
      },

      // name: coverpoint
      {
          DefaultStyle,
          {SymbolIdentifier, "foo_cp"},
          {':', ":"},
          {/* unspecified context */},
          {NodeEnum::kCoverPoint},
          {0, SpacingOptions::kUndecided},
      },
      // coverpoint foo {
      {
          DefaultStyle,
          {SymbolIdentifier, "cpaddr"},
          {'{', "{"},
          {/* unspecified context */},
          {NodeEnum::kCoverPoint, NodeEnum::kBraceGroup},
          {1, SpacingOptions::kUndecided},
      },
      // enum name TYPEID {
      {
          DefaultStyle,
          {SymbolIdentifier, "mytype_t"},
          {'{', "{"},
          {/* unspecified context */},
          {NodeEnum::kEnumType, NodeEnum::kBraceGroup},
          {1, SpacingOptions::kUndecided},
      },

      // x < y (binary operator)
      {
          DefaultStyle,
          {SymbolIdentifier, "id"},
          {'<', "<"},
          {},  // default context
          {},  // default context
          {1, SpacingOptions::kUndecided},
      },
      {
          DefaultStyle,
          {TK_DecNumber, "7"},
          {'<', "<"},
          {},  // default context
          {},  // default context
          {1, SpacingOptions::kUndecided},
      },
      {
          DefaultStyle,
          {')', ")"},
          {'<', "<"},
          {},  // default context
          {},  // default context
          {1, SpacingOptions::kUndecided},
      },
      {
          DefaultStyle,
          {'<', "<"},
          {SymbolIdentifier, "id"},
          {},  // default context
          {},  // default context
          {1, SpacingOptions::kUndecided},
      },
      {
          DefaultStyle,
          {'<', "<"},
          {TK_DecNumber, "7"},
          {},  // default context
          {},  // default context
          {1, SpacingOptions::kUndecided},
      },
      {
          DefaultStyle,
          {'<', "<"},
          {'(', "("},
          {},  // default context
          {},  // default context
          {1, SpacingOptions::kUndecided},
      },

      // x > y (binary operator)
      {
          DefaultStyle,
          {SymbolIdentifier, "id"},
          {'>', ">"},
          {},  // default context
          {},  // default context
          {1, SpacingOptions::kUndecided},
      },
      {
          DefaultStyle,
          {TK_DecNumber, "7"},
          {'>', ">"},
          {},  // default context
          {},  // default context
          {1, SpacingOptions::kUndecided},
      },
      {
          DefaultStyle,
          {')', ")"},
          {'>', ">"},
          {},  // default context
          {},  // default context
          {1, SpacingOptions::kUndecided},
      },
      {
          DefaultStyle,
          {'>', ">"},
          {SymbolIdentifier, "id"},
          {},  // default context
          {},  // default context
          {1, SpacingOptions::kUndecided},
      },
      {
          DefaultStyle,
          {'>', ">"},
          {TK_DecNumber, "7"},
          {},  // default context
          {},  // default context
          {1, SpacingOptions::kUndecided},
      },
      {
          DefaultStyle,
          {'>', ">"},
          {'(', "("},
          {},  // default context
          {},  // default context
          {1, SpacingOptions::kUndecided},
      },

      // '@' on the right
      {
          DefaultStyle,
          {TK_always, "always"},
          {'@', "@"},
          {},  // default context
          {},  // default context
          {1, SpacingOptions::kUndecided},
      },
      {
          DefaultStyle,
          {SymbolIdentifier, "cblock"},
          {'@', "@"},
          {},  // default context
          {},  // default context
          {1, SpacingOptions::kUndecided},
      },
      // '@' on the left
      {
          DefaultStyle,
          {'@', "@"},
          {'(', "("},
          {},  // default context
          {},  // default context
          {0, SpacingOptions::kUndecided},
      },
      {
          DefaultStyle,
          {'@', "@"},
          {'*', "*"},  // not a binary operator in this case
          {},          // default context
          {},          // default context
          {0, SpacingOptions::kUndecided},
      },
      {
          DefaultStyle,
          {'@', "@"},
          {SymbolIdentifier, "clock_a"},
          {},  // default context
          {},  // default context
          {0, SpacingOptions::kUndecided},
      },

      // '#' on the right
      {
          DefaultStyle,
          {SymbolIdentifier, "id_before_pound"},
          {'#', "#"},
          {},  // default context
          {},  // default context
               // no spaces preceding ':' in unit test context
          {1, SpacingOptions::kUndecided},
      },
      {
          DefaultStyle,
          {SymbolIdentifier, "id_before_pound"},
          {'#', "#"},
          {NodeEnum::kUnqualifiedId},
          {},  // default context
          {0, SpacingOptions::kUndecided},
      },
      {
          DefaultStyle,
          {SymbolIdentifier, "id_before_pound"},
          {'#', "#"},
          {NodeEnum::kQualifiedId},
          {},  // default context
          {1, SpacingOptions::kUndecided},
      },

      // '}' on the left
      {
          DefaultStyle,
          {'}', "}"},
          {SymbolIdentifier, "id_before_open_brace"},
          {},  // default context
          {},  // default context
          {1, SpacingOptions::kUndecided},
      },
      {
          DefaultStyle,
          {'}', "}"},
          {',', ","},
          {},  // default context
          {},  // default context
          {0, SpacingOptions::kUndecided},
      },
      {
          DefaultStyle,
          {'}', "}"},
          {';', ";"},
          {},  // default context
          {},  // default context
          {0, SpacingOptions::kUndecided},
      },
      {
          DefaultStyle,
          {'}', "}"},
          {'}', "}"},
          {},  // default context
          {},  // default context
          {0, SpacingOptions::kUndecided},
      },

      // '{' on the right
      {
          DefaultStyle,
          {SymbolIdentifier, "id_before_open_brace"},
          {'{', "{"},
          {},  // default context
          {},  // default context
          {0, SpacingOptions::kUndecided},
      },
      {
          DefaultStyle,
          {TK_unique, "unique"},
          {'{', "{"},
          {},  // default context
          {},  // default context
          {1, SpacingOptions::kUndecided},
      },
      {
          DefaultStyle,
          {TK_with, "with"},
          {'{', "{"},
          {},  // default context
          {},  // default context
          {1, SpacingOptions::kUndecided},
      },
      {
          // constraint c_id {
          DefaultStyle,
          {SymbolIdentifier, "id_before_open_brace"},
          {'{', "{"},
          {},  // default context
          {NodeEnum::kConstraintDeclaration, NodeEnum::kBraceGroup},
          {1, SpacingOptions::kUndecided},
      },

      // ';' on the left
      {
          DefaultStyle,
          {';', ";"},
          {SymbolIdentifier, "id_after_semi"},
          {},  // default context
          {},  // default context
          {1, SpacingOptions::kUndecided},
      },
      {
          DefaultStyle,
          {SemicolonEndOfAssertionVariableDeclarations, ";"},
          {SymbolIdentifier, "id_after_semi"},
          {},  // default context
          {},  // default context
          {1, SpacingOptions::kUndecided},
      },

      // ';' on the right
      {
          DefaultStyle,
          {SymbolIdentifier, "id"},
          {';', ";"},
          {},  // default context
          {},  // default context
          {0, SpacingOptions::kUndecided},
      },
      {
          DefaultStyle,
          {SymbolIdentifier, "id"},
          {SemicolonEndOfAssertionVariableDeclarations, ";"},
          {},  // default context
          {},  // default context
          {0, SpacingOptions::kUndecided},
      },
      {
          DefaultStyle,
          {')', ")"},
          {';', ";"},
          {},                               // default context
          {},                               // default context
          {0, SpacingOptions::kUndecided},  // could be MustAppend too
      },
      {
          DefaultStyle,
          {')', ")"},
          {SemicolonEndOfAssertionVariableDeclarations, ";"},
          {},                               // default context
          {},                               // default context
          {0, SpacingOptions::kUndecided},  // could be MustAppend too
      },

      // keyword on right
      {
          DefaultStyle,
          {TK_DecNumber, "1"},
          {TK_begin, "begin"},
          {},  // default context
          {},  // default context
          {1, SpacingOptions::kUndecided},
      },
      {
          DefaultStyle,
          {TK_begin, "begin"},
          {TK_begin, "begin"},
          {},  // default context
          {},  // default context
          {1, SpacingOptions::kUndecided},
      },
      {
          DefaultStyle,
          {TK_begin, "begin"},
          {TK_end, "end"},
          {},  // default context
          {},  // default context
          {1, SpacingOptions::kMustWrap},
      },
      {
          DefaultStyle,
          {TK_end, "end"},
          {TK_begin, "begin"},
          {},  // default context
          {},  // default context
          {1, SpacingOptions::kUndecided},
      },
      {
          DefaultStyle,
          {TK_end, "end"},
          {TK_else, "else"},
          {},  // default context
          {},  // default context
          {1, SpacingOptions::kMustAppend},
      },
      {
          DefaultStyle,
          {'}', "}"},
          {TK_else, "else"},
          {},  // default context
          {},  // default context
          {1, SpacingOptions::kMustAppend},
      },
      {
          DefaultStyle,
          {';', ";"},
          {TK_else, "else"},
          {},  // default context
          {},  // default context
          {1, SpacingOptions::kMustWrap},
      },
      {
          DefaultStyle,
          {TK_default, "default"},
          {TK_clocking, "clocking"},
          {},  // default context
          {},  // default context
          {1, SpacingOptions::kUndecided},
      },
      {
          DefaultStyle,
          {TK_default, "default"},
          {TK_disable, "disable"},
          {},  // default context
          {},  // default context
          {1, SpacingOptions::kUndecided},
      },
      {
          DefaultStyle,
          {TK_disable, "disable"},
          {TK_iff, "iff"},
          {},  // default context
          {},  // default context
          {1, SpacingOptions::kUndecided},
      },
      {
          DefaultStyle,
          {TK_disable, "disable"},
          {TK_soft, "soft"},
          {},  // default context
          {},  // default context
          {1, SpacingOptions::kUndecided},
      },
      {
          DefaultStyle,
          {TK_extern, "extern"},
          {TK_forkjoin, "forkjoin"},
          {},  // default context
          {},  // default context
          {1, SpacingOptions::kUndecided},
      },
      {
          DefaultStyle,
          {TK_input, "input"},
          {TK_logic, "logic"},
          {},  // default context
          {},  // default context
          {1, SpacingOptions::kUndecided},
      },
      {
          DefaultStyle,
          {TK_var, "var"},
          {TK_logic, "logic"},
          {},  // default context
          {},  // default context
          {1, SpacingOptions::kUndecided},
      },
      {
          DefaultStyle,
          {TK_output, "output"},
          {TK_reg, "reg"},
          {},  // default context
          {},  // default context
          {1, SpacingOptions::kUndecided},
      },
      {
          DefaultStyle,
          {TK_static, "static"},
          {TK_constraint, "constraint"},
          {},  // default context
          {},  // default context
          {1, SpacingOptions::kUndecided},
      },
      {
          DefaultStyle,
          {TK_parameter, "parameter"},
          {TK_type, "type"},
          {},  // default context
          {},  // default context
          {1, SpacingOptions::kUndecided},
      },
      {
          DefaultStyle,
          {TK_virtual, "virtual"},
          {TK_interface, "interface"},
          {},  // default context
          {},  // default context
          {1, SpacingOptions::kUndecided},
      },
      {
          DefaultStyle,
          {TK_const, "const"},
          {TK_ref, "ref"},
          {},  // default context
          {},  // default context
          {1, SpacingOptions::kUndecided},
      },
      {
          DefaultStyle,
          {TK_union, "union"},
          {TK_tagged, "tagged"},
          {},  // default context
          {},  // default context
          {1, SpacingOptions::kUndecided},
      },
      {
          DefaultStyle,
          {';', ";"},
          {TK_end, "end"},
          {},  // default context
          {},  // default context
          {1, SpacingOptions::kMustWrap},
      },
      {
          DefaultStyle,
          {';', ";"},
          {TK_endfunction, "endfunction"},
          {},  // default context
          {},  // default context
          {1, SpacingOptions::kMustWrap},
      },
      {
          DefaultStyle,
          {';', ";"},
          {TK_endtask, "endtask"},
          {},  // default context
          {},  // default context
          {1, SpacingOptions::kMustWrap},
      },
      {
          DefaultStyle,
          {';', ";"},
          {TK_endclass, "endclass"},
          {},  // default context
          {},  // default context
          {1, SpacingOptions::kMustWrap},
      },
      {
          DefaultStyle,
          {';', ";"},
          {TK_endpackage, "endpackage"},
          {},  // default context
          {},  // default context
          {1, SpacingOptions::kMustWrap},
      },
      {
          DefaultStyle,
          {SymbolIdentifier, "nettype_id"},
          {TK_with, "with"},
          {},  // default context
          {},  // default context
          {1, SpacingOptions::kUndecided},
      },
      {
          DefaultStyle,
          {SymbolIdentifier, "id"},
          {TK_until, "until"},
          {},  // default context
          {},  // default context
          {1, SpacingOptions::kUndecided},
      },
      {
          DefaultStyle,
          {',', ","},
          {TK_highz0, "highz0"},
          {},  // default context
          {},  // default context
          {1, SpacingOptions::kUndecided},
      },
      {
          DefaultStyle,
          {',', ","},
          {TK_highz1, "highz1"},
          {},  // default context
          {},  // default context
          {1, SpacingOptions::kUndecided},
      },
      // Entries spacing in primitives
      {
          // 1 0 : ? : -;
          DefaultStyle,
          {'1', "1"},
          {'0', "0"},
          {},  // default context
          {NodeEnum::kUdpSequenceEntry},
          {1, SpacingOptions::kUndecided},
      },
      {
          // 1 0 : ? : -;
          DefaultStyle,
          {'0', "0"},
          {':', ":"},
          {},  // default context
          {NodeEnum::kUdpSequenceEntry},
          {1, SpacingOptions::kUndecided},
      },
      {
          // 1 0 : ? : -;
          DefaultStyle,
          {':', ":"},
          {'?', "?"},
          {},  // default context
          {NodeEnum::kUdpSequenceEntry},
          {1, SpacingOptions::kUndecided},
      },
      {
          // 1 0 : ? : -;
          DefaultStyle,
          {'?', "?"},
          {':', ":"},
          {},  // default context
          {NodeEnum::kUdpSequenceEntry},
          {1, SpacingOptions::kUndecided},
      },
      {
          // 1 0 : ? : -;
          DefaultStyle,
          {':', ":"},
          {'-', "-"},
          {},  // default context
          {NodeEnum::kUdpSequenceEntry},
          {1, SpacingOptions::kUndecided},
      },
      {
          // 1 0 : ? : -;
          DefaultStyle,
          {'-', "-"},
          {';', ";"},
          {},  // default context
          {NodeEnum::kUdpSequenceEntry},
          {0, SpacingOptions::kUndecided},
      },
      {
          // 1 0 : -;
          DefaultStyle,
          {'1', "1"},
          {'0', "0"},
          {},  // default context
          {NodeEnum::kUdpCombEntry},
          {1, SpacingOptions::kUndecided},
      },
      {
          // 1 0 : -;
          DefaultStyle,
          {'0', "0"},
          {':', ":"},
          {},  // default context
          {NodeEnum::kUdpCombEntry},
          {1, SpacingOptions::kUndecided},
      },
      {
          // 1 0 : -;
          DefaultStyle,
          {':', ":"},
          {'-', "-"},
          {},  // default context
          {NodeEnum::kUdpCombEntry},
          {1, SpacingOptions::kUndecided},
      },
      {
          // 1 0 : -;
          DefaultStyle,
          {'-', "-"},
          {';', ";"},
          {},  // default context
          {NodeEnum::kUdpCombEntry},
          {0, SpacingOptions::kUndecided},
      },

      // time literals
      {
          // #1ps
          DefaultStyle,
          {'#', "#"},
          {verilog_tokentype::TK_TimeLiteral, "1ps"},
          {/* any context */},
          {/* any context */},
          {0, SpacingOptions::kMustAppend},
      },
      {
          // #1ps;
          DefaultStyle,
          {verilog_tokentype::TK_TimeLiteral, "1ps"},
          {';', ";"},
          {/* any context */},
          {/* any context */},
          {0, SpacingOptions::kMustAppend},
      },
      {
          DefaultStyle,
          {verilog_tokentype::TK_TimeLiteral, "1ps"},
          {verilog_tokentype::SymbolIdentifier, "task_call"},
          {/* any context */},
          {/* any context */},
          {1, SpacingOptions::kUndecided},
      },
      {
          DefaultStyle,
          {verilog_tokentype::TK_TimeLiteral, "1ps"},
          {verilog_tokentype::MacroIdentifier, "`MACRO"},
          {/* any context */},
          {/* any context */},
          {1, SpacingOptions::kUndecided},
      },
      {
          DefaultStyle,
          {verilog_tokentype::TK_TimeLiteral, "100ps"},
          {verilog_tokentype::MacroCallId, "`MACRO"},
          {/* any context */},
          {/* any context */},
          {1, SpacingOptions::kUndecided},
      },
      {
          DefaultStyle,
          {verilog_tokentype::TK_TimeLiteral, "1ps"},
          {'#', "#"},
          {/* any context */},
          {/* any context */},
          {1, SpacingOptions::kUndecided},
      },
      {
          DefaultStyle,
          {verilog_tokentype::TK_TimeLiteral, "1ps"},
          {verilog_tokentype::TK_INCR, "++"},
          {/* any context */},
          {/* any context */},
          {1, SpacingOptions::kUndecided},
      },
      {
          DefaultStyle,
          {verilog_tokentype::TK_TimeLiteral, "1ps"},
          {verilog_tokentype::TK_DECR, "--"},
          {/* any context */},
          {/* any context */},
          {1, SpacingOptions::kUndecided},
      },
      {
          DefaultStyle,
          {verilog_tokentype::TK_TimeLiteral, "1ps"},
          {'@', "@"},
          {/* any context */},
          {/* any context */},
          {1, SpacingOptions::kUndecided},
      },
      {
          DefaultStyle,
          {verilog_tokentype::TK_TimeLiteral, "1ps"},
          {verilog_tokentype::TK_begin, "begin"},
          {/* any context */},
          {/* any context */},
          {1, SpacingOptions::kUndecided},
      },
      {
          DefaultStyle,
          {verilog_tokentype::TK_TimeLiteral, "1ps"},
          {verilog_tokentype::TK_force, "force"},
          {/* any context */},
          {/* any context */},
          {1, SpacingOptions::kUndecided},
      },
      {
          DefaultStyle,
          {verilog_tokentype::TK_TimeLiteral, "1ps"},
          {verilog_tokentype::TK_output, "output"},
          {/* any context */},
          {/* any context */},
          {1, SpacingOptions::kUndecided},
      },
      {
          // ... / 1ps
          DefaultStyle,
          {'/', "/"},
          {verilog_tokentype::TK_TimeLiteral, "1ps"},
          {/* any context */},
          {/* any context */},
          {1, SpacingOptions::kUndecided},
      },
      {
          // 1ps / ...
          DefaultStyle,
          {verilog_tokentype::TK_TimeLiteral, "1ps"},
          {'/', "/"},
          {/* any context */},
          {/* any context */},
          {1, SpacingOptions::kUndecided},
      },
      {
          DefaultStyle,
          {verilog_tokentype::TK_EOL_COMMENT, "//comment"},
          {verilog_tokentype::TK_LINE_CONT, "\\"},
          {/* any context */},
          {/* any context */},
          {0, SpacingOptions::kMustAppend},
      },
      {
          DefaultStyle,
          {verilog_tokentype::SymbolIdentifier, "id"},
          {verilog_tokentype::TK_LINE_CONT, "\\"},
          {/* any context */},
          {/* any context */},
          {0, SpacingOptions::kMustAppend},
      },
      {
          DefaultStyle,
          {verilog_tokentype::EscapedIdentifier, "\\id.id[9]"},
          {verilog_tokentype::TK_LINE_CONT, "\\"},
          {/* any context */},
          {/* any context */},
          {1, SpacingOptions::kMustAppend},
      },
      {
          DefaultStyle,
          {verilog_tokentype::TK_DecNumber, "77"},
          {verilog_tokentype::TK_LINE_CONT, "\\"},
          {/* any context */},
          {/* any context */},
          {0, SpacingOptions::kMustAppend},
      },
      {
          DefaultStyle,
          {')', ")"},
          {verilog_tokentype::TK_LINE_CONT, "\\"},
          {/* any context */},
          {/* any context */},
          {0, SpacingOptions::kMustAppend},
      },
      {
          DefaultStyle,
          {'}', "}"},
          {verilog_tokentype::TK_LINE_CONT, "\\"},
          {/* any context */},
          {/* any context */},
          {0, SpacingOptions::kMustAppend},
      },
      {
          DefaultStyle,
          {']', "]"},
          {verilog_tokentype::TK_LINE_CONT, "\\"},
          {/* any context */},
          {/* any context */},
          {0, SpacingOptions::kMustAppend},
      },
      {
          DefaultStyle,
          {verilog_tokentype::TK_LINE_CONT, "\\"},
          {verilog_tokentype::SymbolIdentifier, "id"},
          {/* any context */},
          {/* any context */},
          {0, SpacingOptions::kMustWrap},
      },
      // Space between return keyword and return value
      {
          DefaultStyle,
          {verilog_tokentype::TK_return, "return"},
          {'{', "{"},
          {/* unspecified context */},
          {/* unspecified context */},
          {1, SpacingOptions::kUndecided},
      },
      {
          DefaultStyle,
          {verilog_tokentype::TK_return, "return"},
          {'(', "("},
          {/* unspecified context */},
          {/* unspecified context */},
          {1, SpacingOptions::kUndecided},
      },
      {
          DefaultStyle,
          {verilog_tokentype::TK_return, "return"},
          {'-', "-"},
          {/* unspecified context */},
          {/* unspecified context */},
          {1, SpacingOptions::kUndecided},
      },
      {
          DefaultStyle,
          {verilog_tokentype::TK_return, "return"},
          {'!', "!"},
          {/* unspecified context */},
          {/* unspecified context */},
          {1, SpacingOptions::kUndecided},
      },
      {
          DefaultStyle,
          {verilog_tokentype::TK_return, "return"},
          {'~', "~"},
          {/* unspecified context */},
          {/* unspecified context */},
          {1, SpacingOptions::kUndecided},
      },
      {
          DefaultStyle,
          {verilog_tokentype::TK_return, "return"},
          {verilog_tokentype::SystemTFIdentifier, "$foo"},
          {/* unspecified context */},
          {/* unspecified context */},
          {1, SpacingOptions::kUndecided},
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
  absl::string_view left_token_string;

  // This spacing may influence token-annotation behavior.
  absl::string_view whitespace_between;

  // TODO(fangism): group this into a TokenInfo.
  int right_token_enum;
  absl::string_view right_token_string;

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
       DefaultStyle,
       '=',  // left token
       "=",
       "   ",                            // whitespace between
       verilog_tokentype::TK_DecNumber,  // right token
       "0",
       {/* unspecified context */},
       {/* unspecified context */},
       {1, SpacingOptions::kUndecided}},
      {
          DefaultStyle,
          TK_COMMENT_BLOCK,
          "/*comment*/",
          "",
          verilog_tokentype::MacroCallId,
          "`uvm_foo_macro",
          {},  // any context
          {},  // any context
          {1, SpacingOptions::kUndecided},
      },
      {
          DefaultStyle,
          TK_COMMENT_BLOCK,
          "/*comment*/",
          "",
          verilog_tokentype::MacroIdentifier,
          "`uvm_foo_id",
          {},  // any context
          {},  // any context
          {1, SpacingOptions::kUndecided},
      },
      {
          DefaultStyle,
          verilog_tokentype::TK_COMMENT_BLOCK,
          "/*comment*/",
          "",
          verilog_tokentype::TK_LINE_CONT,
          "\\",
          {/* any context */},
          {/* any context */},
          {0, SpacingOptions::kMustAppend},
      },
      {// //comment1
       // //comment2
       DefaultStyle,
       verilog_tokentype::TK_EOL_COMMENT,
       "//comment1",
       "\n",
       verilog_tokentype::TK_EOL_COMMENT,
       "//comment2",
       {},
       {},
       {2, SpacingOptions::kMustWrap}},
      {// 0 // comment
       DefaultStyle,
       verilog_tokentype::TK_DecNumber,
       "0",
       "   ",
       verilog_tokentype::TK_EOL_COMMENT,
       "// comment",
       {/* unspecified context */},
       {/* unspecified context */},
       {2, SpacingOptions::kMustAppend}},
      {// 0// comment
       DefaultStyle,
       verilog_tokentype::TK_DecNumber,
       "0",
       "",
       verilog_tokentype::TK_EOL_COMMENT,
       "// comment",
       {/* unspecified context */},
       {/* unspecified context */},
       {2, SpacingOptions::kMustAppend}},
      {// 0 \n  // comment
       DefaultStyle,
       verilog_tokentype::TK_DecNumber,
       "0",
       " \n  ",
       verilog_tokentype::TK_EOL_COMMENT,
       "// comment",
       {/* unspecified context */},
       {/* unspecified context */},
       {2, SpacingOptions::kUndecided}},
      {// // comment 1 \n  // comment 2
       DefaultStyle,
       verilog_tokentype::TK_EOL_COMMENT,
       "// comment 1",
       " \n  ",
       verilog_tokentype::TK_EOL_COMMENT,
       "// comment 2",
       {/* unspecified context */},
       {/* unspecified context */},
       {2, SpacingOptions::kMustWrap}},
      {// /* comment 1 */ \n  // comment 2
       DefaultStyle,
       verilog_tokentype::TK_COMMENT_BLOCK,
       "/* comment 1 */",
       " \n  ",
       verilog_tokentype::TK_EOL_COMMENT,
       "// comment 2",
       {/* unspecified context */},
       {/* unspecified context */},
       {2, SpacingOptions::kMustWrap}},
      {// /* comment 1 */  // comment 2
       DefaultStyle,
       verilog_tokentype::TK_COMMENT_BLOCK,
       "/* comment 1 */",
       " ",
       verilog_tokentype::TK_EOL_COMMENT,
       "// comment 2",
       {/* unspecified context */},
       {/* unspecified context */},
       {2, SpacingOptions::kMustAppend}},
      {// ;  // comment 2
       DefaultStyle,
       ';',
       ";",
       " ",
       verilog_tokentype::TK_EOL_COMMENT,
       "// comment 2",
       {/* unspecified context */},
       {/* unspecified context */},
       {2, SpacingOptions::kMustAppend}},
      {// ; \n // comment 2
       DefaultStyle,
       ';',
       ";",
       " \n",
       verilog_tokentype::TK_EOL_COMMENT,
       "// comment 2",
       {/* unspecified context */},
       {/* unspecified context */},
       {2, SpacingOptions::kUndecided}},
      {// ,  // comment 2
       DefaultStyle,
       ',',
       ",",
       " ",
       verilog_tokentype::TK_EOL_COMMENT,
       "// comment 2",
       {/* unspecified context */},
       {/* unspecified context */},
       {2, SpacingOptions::kMustAppend}},
      {// , \n // comment 2
       DefaultStyle,
       ',',
       ",",
       "\n ",
       verilog_tokentype::TK_EOL_COMMENT,
       "// comment 2",
       {/* unspecified context */},
       {/* unspecified context */},
       {2, SpacingOptions::kUndecided}},
      {// begin  // comment 2
       DefaultStyle,
       verilog_tokentype::TK_begin,
       "begin",
       " ",
       verilog_tokentype::TK_EOL_COMMENT,
       "// comment 2",
       {/* unspecified context */},
       {/* unspecified context */},
       {2, SpacingOptions::kMustAppend}},
      {// begin \n // comment 2
       DefaultStyle,
       verilog_tokentype::TK_begin,
       "begin",
       "\n",
       verilog_tokentype::TK_EOL_COMMENT,
       "// comment 2",
       {/* unspecified context */},
       {/* unspecified context */},
       {2, SpacingOptions::kUndecided}},
      {// else  // comment 2
       DefaultStyle,
       verilog_tokentype::TK_else,
       "else",
       " ",
       verilog_tokentype::TK_EOL_COMMENT,
       "// comment 2",
       {/* unspecified context */},
       {/* unspecified context */},
       {2, SpacingOptions::kMustAppend}},
      {// else \n // comment 2
       DefaultStyle,
       verilog_tokentype::TK_else,
       "else",
       " \n  ",
       verilog_tokentype::TK_EOL_COMMENT,
       "// comment 2",
       {/* unspecified context */},
       {/* unspecified context */},
       {2, SpacingOptions::kUndecided}},
      {// end  // comment 2
       DefaultStyle,
       verilog_tokentype::TK_end,
       "end",
       " ",
       verilog_tokentype::TK_EOL_COMMENT,
       "// comment 2",
       {/* unspecified context */},
       {/* unspecified context */},
       {2, SpacingOptions::kMustAppend}},
      {// end \n // comment 2
       DefaultStyle,
       verilog_tokentype::TK_end,
       "end",
       "  \n ",
       verilog_tokentype::TK_EOL_COMMENT,
       "// comment 2",
       {/* unspecified context */},
       {/* unspecified context */},
       {2, SpacingOptions::kUndecided}},
      {// generate  // comment 2
       DefaultStyle,
       verilog_tokentype::TK_generate,
       "generate",
       " ",
       verilog_tokentype::TK_EOL_COMMENT,
       "// comment 2",
       {/* unspecified context */},
       {/* unspecified context */},
       {2, SpacingOptions::kMustAppend}},
      {// generate \n // comment 2
       DefaultStyle,
       verilog_tokentype::TK_generate,
       "generate",
       "  \n",
       verilog_tokentype::TK_EOL_COMMENT,
       "// comment 2",
       {/* unspecified context */},
       {/* unspecified context */},
       {2, SpacingOptions::kUndecided}},
      {// if  // comment 2
       DefaultStyle,
       verilog_tokentype::TK_if,
       "if",
       " ",
       verilog_tokentype::TK_EOL_COMMENT,
       "// comment 2",
       {/* unspecified context */},
       {/* unspecified context */},
       {2, SpacingOptions::kMustAppend}},
      {// if \n\n // comment 2
       DefaultStyle,
       verilog_tokentype::TK_if,
       "if",
       " \n\n ",
       verilog_tokentype::TK_EOL_COMMENT,
       "// comment 2",
       {/* unspecified context */},
       {/* unspecified context */},
       {2, SpacingOptions::kUndecided}},
      {
          DefaultStyle,
          verilog_tokentype::TK_LINE_CONT,
          "\\",
          "\n",
          verilog_tokentype::TK_EOL_COMMENT,
          "//comment",
          {/* any context */},
          {/* any context */},
          {0, SpacingOptions::kMustWrap},
      },
      {
          DefaultStyle,
          verilog_tokentype::TK_LINE_CONT,
          "\\",
          "\n",
          verilog_tokentype::TK_COMMENT_BLOCK,
          "/*comment*/",
          {/* any context */},
          {/* any context */},
          {0, SpacingOptions::kMustWrap},
      },
      {
          DefaultStyle,
          verilog_tokentype::MacroCallCloseToEndLine,
          ")",
          " ",
          verilog_tokentype::TK_COMMENT_BLOCK,
          "/*comment*/",
          {/* unspecified context */},
          {/* unspecified context */},
          {2, SpacingOptions::kUndecided},  // could be append
      },
      {
          DefaultStyle,
          verilog_tokentype::MacroCallCloseToEndLine,
          ")",
          "\n",
          verilog_tokentype::TK_COMMENT_BLOCK,
          "/*comment*/",
          {/* unspecified context */},
          {/* unspecified context */},
          {2, SpacingOptions::kMustWrap},
      },
      {
          DefaultStyle,
          verilog_tokentype::MacroCallCloseToEndLine,
          ")",
          " ",
          verilog_tokentype::TK_EOL_COMMENT,
          "//comment",
          {/* unspecified context */},
          {/* unspecified context */},
          {2, SpacingOptions::kMustAppend},
      },
      {
          DefaultStyle,
          verilog_tokentype::MacroCallCloseToEndLine,
          ")",
          "\n",
          verilog_tokentype::TK_EOL_COMMENT,
          "//comment",
          {/* unspecified context */},
          {/* unspecified context */},
          {2, SpacingOptions::kUndecided},
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
          {NodeEnum::kUdpCombEntry},
          {2, SpacingOptions::kUndecided},
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
          {NodeEnum::kUdpCombEntry},
          {1, SpacingOptions::kUndecided},
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
          {NodeEnum::kUdpCombEntry},
          {2, SpacingOptions::kMustAppend},
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
          {NodeEnum::kUdpSequenceEntry},
          {2, SpacingOptions::kUndecided},
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
          {NodeEnum::kUdpSequenceEntry},
          {1, SpacingOptions::kUndecided},
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
          {NodeEnum::kUdpSequenceEntry},
          {2, SpacingOptions::kMustAppend},
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
          {NodeEnum::kUdpPortDeclaration},
          {2, SpacingOptions::kUndecided},
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
          {NodeEnum::kUdpPortDeclaration},
          {1, SpacingOptions::kUndecided},
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
          {NodeEnum::kUdpPortDeclaration},
          {2, SpacingOptions::kUndecided},
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
          {NodeEnum::kUdpPortDeclaration},
          {2, SpacingOptions::kMustAppend},
      },

      {
          // [a+b]
          DefaultStyle,
          verilog_tokentype::SymbolIdentifier,
          "a",
          "",  // no spaces originally
          '+',
          "+",
          {NodeEnum::kDimensionScalar},
          {NodeEnum::kDimensionScalar},
          {0, SpacingOptions::kUndecided},
      },
      {
          // [a +b]
          DefaultStyle,
          verilog_tokentype::SymbolIdentifier,
          "a",
          " ",  // 1 space originally
          '+',
          "+",
          {NodeEnum::kDimensionScalar},
          {NodeEnum::kDimensionScalar},
          {0, SpacingOptions::kUndecided},
      },
      {
          // [a  +b]
          DefaultStyle,
          verilog_tokentype::SymbolIdentifier,
          "a",
          "  ",  // 2 spaces originally
          '+',
          "+",
          {NodeEnum::kDimensionScalar},
          {NodeEnum::kDimensionScalar},
          {0, SpacingOptions::kUndecided},  // no spacing
      },
      {
          // [a     :    b]
          DefaultStyle,
          verilog_tokentype::SymbolIdentifier,
          "a",
          "     ",
          ':',
          ":",
          {NodeEnum::kDimensionRange},
          {NodeEnum::kDimensionRange},
          {1, SpacingOptions::kUndecided},  // limit to 1
      },
      {
          // [a     :    b]
          DefaultStyle,
          ':',
          ":",
          "    ",
          verilog_tokentype::SymbolIdentifier,
          "b",
          {NodeEnum::kDimensionRange},
          {NodeEnum::kDimensionRange},
          {0, SpacingOptions::kUndecided},
      },
      {
          // [a + b]
          CompactIndexSelectionStyle,
          verilog_tokentype::SymbolIdentifier,
          "a",
          " ",
          '+',
          "+",
          {NodeEnum::kDimensionScalar},
          {NodeEnum::kStreamingConcatenation},
          {1, SpacingOptions::kUndecided},
      },
      {
          // [a+b]
          CompactIndexSelectionStyle,
          verilog_tokentype::SymbolIdentifier,
          "a",
          "",  // no spaces originally
          '+',
          "+",
          {NodeEnum::kDimensionScalar},
          {NodeEnum::kDimensionScalar},
          {0, SpacingOptions::kUndecided},
      },
      {
          // [a +b]
          CompactIndexSelectionStyle,
          verilog_tokentype::SymbolIdentifier,
          "a",
          " ",  // 1 space originally
          '+',
          "+",
          {NodeEnum::kDimensionScalar},
          {NodeEnum::kDimensionScalar},
          {1, SpacingOptions::kUndecided},  // limit to 1 space
      },
      {
          // [a  +b]
          CompactIndexSelectionStyle,
          verilog_tokentype::SymbolIdentifier,
          "a",
          "  ",  // 2 spaces originally
          '+',
          "+",
          {NodeEnum::kDimensionScalar},
          {NodeEnum::kDimensionScalar},
          {1, SpacingOptions::kUndecided},  // limit to 1 space
      },
      {
          // [a     :    b]
          CompactIndexSelectionStyle,
          verilog_tokentype::SymbolIdentifier,
          "a",
          "     ",
          ':',
          ":",
          {NodeEnum::kDimensionRange},
          {NodeEnum::kDimensionRange},
          {1, SpacingOptions::kUndecided},  // limit to 1 space
      },
      {
          // [a     :    b]
          CompactIndexSelectionStyle,
          ':',
          ":",
          "    ",
          verilog_tokentype::SymbolIdentifier,
          "b",
          {NodeEnum::kDimensionRange},
          {NodeEnum::kDimensionRange},
          {0, SpacingOptions::kUndecided},
      },
      {
          DefaultStyle,
          verilog_tokentype::SymbolIdentifier,
          "a",
          "\n    ",
          ':',
          ":",
          {NodeEnum::kDimensionRange},
          {NodeEnum::kDimensionRange},
          // 0 spaces as this is an indentation, not spacing
          {0, SpacingOptions::kUndecided},
      },
      {
          DefaultStyle,
          '*',
          "*",
          "",  // 0 spaces originally
          verilog_tokentype::SymbolIdentifier,
          "foo",
          {NodeEnum::kDimensionRange},
          {NodeEnum::kDimensionRange},
          {0, SpacingOptions::kUndecided},
      },
      {
          DefaultStyle,
          verilog_tokentype::SymbolIdentifier,
          "foo",
          "",  // 0 spaces originally
          '*',
          "*",
          {NodeEnum::kDimensionRange},
          {NodeEnum::kDimensionRange},
          {0, SpacingOptions::kUndecided},
      },
      {
          DefaultStyle,
          '*',
          "*",
          "",  // 0 spaces originally
          verilog_tokentype::SymbolIdentifier,
          "foo",
          {NodeEnum::kDimensionScalar},
          {NodeEnum::kDimensionScalar},
          {0, SpacingOptions::kUndecided},
      },
      {
          DefaultStyle,
          verilog_tokentype::SymbolIdentifier,
          "foo",
          "",  // 0 spaces originally
          '*',
          "*",
          {NodeEnum::kDimensionScalar},
          {NodeEnum::kDimensionScalar},
          {0, SpacingOptions::kUndecided},
      },
      {
          DefaultStyle,
          '*',
          "*",
          " ",  // 1 space originally
          verilog_tokentype::SymbolIdentifier,
          "foo",
          {NodeEnum::kPackedDimensions},
          {NodeEnum::kPackedDimensions},
          {1, SpacingOptions::kPreserve},
      },
      {
          DefaultStyle,
          verilog_tokentype::SymbolIdentifier,
          "foo",
          " ",  // 1 space originally
          '*',
          "*",
          {NodeEnum::kPackedDimensions},
          {NodeEnum::kPackedDimensions},
          {1, SpacingOptions::kPreserve},
      },
      {
          DefaultStyle,
          '*',
          "*",
          " ",  // 1 space originally
          verilog_tokentype::SymbolIdentifier,
          "foo",
          {NodeEnum::kUnpackedDimensions},
          {NodeEnum::kUnpackedDimensions},
          {1, SpacingOptions::kPreserve},
      },
      {
          DefaultStyle,
          verilog_tokentype::SymbolIdentifier,
          "foo",
          " ",  // 1 space originally
          '*',
          "*",
          {NodeEnum::kUnpackedDimensions},
          {NodeEnum::kUnpackedDimensions},
          {1, SpacingOptions::kPreserve},
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
