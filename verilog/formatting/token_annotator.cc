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

#include <iterator>
#include <vector>

#include "absl/strings/string_view.h"
#include "common/formatting/format_token.h"
#include "common/formatting/tree_annotator.h"
#include "common/strings/range.h"
#include "common/text/syntax_tree_context.h"
#include "common/text/text_structure.h"
#include "common/text/token_info.h"
#include "common/util/iterator_range.h"
#include "common/util/logging.h"
#include "common/util/with_reason.h"
#include "verilog/CST/verilog_nonterminals.h"
#include "verilog/formatting/format_style.h"
#include "verilog/formatting/verilog_token.h"
#include "verilog/parser/verilog_parser.h"
#include "verilog/parser/verilog_token_enum.h"

namespace verilog {
namespace formatter {

using ::verible::PreFormatToken;
using ::verible::SpacingOptions;
using ::verible::SyntaxTreeContext;
using ::verible::WithReason;
using FTT = FormatTokenType;

// Signal that spacing was not explicitly handled in case logic.
// This value must be negative.
static constexpr int kUnhandledSpacesRequired = -1;

static bool IsUnaryPrefixExpressionOperand(const PreFormatToken& left,
                                           const SyntaxTreeContext& context) {
  return IsUnaryOperator(yytokentype(left.TokenEnum())) &&
         context.IsInsideFirst({NodeEnum::kUnaryPrefixExpression},
                               {NodeEnum::kExpression});
}

static bool IsInsideNumericLiteral(const PreFormatToken& left,
                                   const PreFormatToken& right) {
  return (left.format_token_enum == FormatTokenType::numeric_literal &&
          right.format_token_enum == FormatTokenType::numeric_base) ||
         left.format_token_enum == FormatTokenType::numeric_base;
}

// Returns true if keyword can be used like a function/method call.
// Based on various LRM sections mentioning subroutine calls.
static bool IsKeywordCallable(yytokentype e) {
  switch (e) {
    case TK_and:  // array method
    case TK_assert:
    case TK_assume:
    case TK_find:
    case TK_find_index:
    case TK_find_first:
    case TK_find_first_index:
    case TK_find_last:
    case TK_find_last_index:
    case TK_min:
    case TK_max:
    case TK_new:
    case TK_or:  // array method
    case TK_product:
    case TK_randomize:
    case TK_reverse:
    case TK_rsort:
    case TK_shuffle:
    case TK_sort:
    case TK_sum:
    case TK_unique:  // array method
    case TK_wait:    // wait statement
    case TK_xor:     // array method
      // TODO(fangism): Verilog-AMS functions, like sin, cos, ...
      return true;
    default:
      break;
  }
  return false;
}

// The following combinations cannot be merged without a space:
//   number number : would result in one different number
//   number id/kw : would result in a bad identifier (lexer)
//   id/kw number : would result in a (different) identifier
//   id/kw id/kw : would result in a (different) identifier
static bool PairwiseNonmergeable(const PreFormatToken& ftoken) {
  return ftoken.TokenEnum() == TK_DecNumber ||
         ftoken.format_token_enum == FormatTokenType::identifier ||
         ftoken.format_token_enum == FormatTokenType::keyword;
}

static bool InRangeLikeContext(const SyntaxTreeContext& context) {
  return context.IsInsideFirst(
      {NodeEnum::kSelectVariableDimension, NodeEnum::kDimensionRange,
       NodeEnum::kDimensionSlice},
      {});
}

static bool IsAnySemicolon(const PreFormatToken& ftoken) {
  // These are just syntactically disambiguated versions of ';'.
  return ftoken.TokenEnum() == ';' ||
         ftoken.TokenEnum() ==
             yytokentype::SemicolonEndOfAssertionVariableDeclarations;
}

// Returns minimum number of spaces required between left and right token.
// Returning kUnhandledSpacesRequired means the case was not explicitly
// handled, and it is up to the caller to decide what to do when this happens.
static WithReason<int> SpacesRequiredBetween(const PreFormatToken& left,
                                             const PreFormatToken& right,
                                             const SyntaxTreeContext& context) {
  VLOG(3) << "Spacing between " << verilog_symbol_name(left.TokenEnum())
          << " and " << verilog_symbol_name(right.TokenEnum());
  // Higher precedence rules should be handled earlier in this function.

  // Preserve space after escaped identifiers.
  if (left.TokenEnum() == EscapedIdentifier) {
    return {1, "Escaped identifiers must end with whitespace."};
  }

  if (IsComment(FormatTokenType(right.format_token_enum))) {
    return {2, "Style: require 2+ spaces before comments"};
    // TODO(fangism): Take this from FormatStyle.
  }

  if (left.format_token_enum == FormatTokenType::open_group ||
      right.format_token_enum == FormatTokenType::close_group) {
    return {0,
            "Prefer \"(foo)\" over \"( foo )\", \"[x]\" over \"[ x ]\", "
            "and \"{y}\" over \"{ y }\"."};
  }

  // For now, leave everything inside [dimensions] alone.
  if (context.IsInsideFirst(
          {NodeEnum::kDimensionRange, NodeEnum::kDimensionScalar}, {})) {
    // ... except for the spacing before '[' and around ':',
    // which are covered elsewhere.
    if (right.TokenEnum() != '[' && left.TokenEnum() != ':' &&
        right.TokenEnum() != ':') {
      return {kUnhandledSpacesRequired,
              "Leave [expressions] inside scalar and range dimensions alone "
              "(for now)."};
    }
  }

  // Unary operators (context-sensitive)
  if (IsUnaryPrefixExpressionOperand(left, context) &&
      (left.format_token_enum != FormatTokenType::binary_operator ||
       !IsUnaryOperator(static_cast<yytokentype>(right.TokenEnum())))) {
    // TODO: There are _some_ unary operators on the right that could
    // be formatted with 0-space, for example:
    // 'a = & ~b'; could be 'a = &~b;'
    return {0, "Bind unary prefix operator close to its operand."};
  }

  if (left.TokenEnum() == TK_SCOPE_RES) {
    return {0, "Prefer \"::id\" over \":: id\", \"::*\" over \":: *\""};
  }

  // Delimiters, list separators
  if (right.TokenEnum() == ',') return {0, "No space before comma"};
  if (left.TokenEnum() == ',') return {1, "Require space after comma"};

  if (IsAnySemicolon(right)) {
    if (left.TokenEnum() == ':') {
      return {1, "Space between semicolon and colon, (e.g. \"default: ;\")"};
    }
    return {0, "No space before semicolon"};
  }
  if (IsAnySemicolon(left)) {
    return {1, "Require space after semicolon"};
  }

  if (context.IsInsideFirst({NodeEnum::kStreamingConcatenation}, {})) {
    if (left.TokenEnum() == TK_LS || left.TokenEnum() == TK_RS) {
      return {0, "No space around streaming operators"};
    } else if (left.format_token_enum == FormatTokenType::numeric_literal ||
               left.format_token_enum == FormatTokenType::identifier ||
               left.format_token_enum == FormatTokenType::keyword) {
      return {0, "No space around streaming operator slice size"};
    }
  }

  // "@(" vs. "@ (" for event control
  // "@*" vs. "@ *" for event control, '*' is not a binary operator here
  if (left.TokenEnum() == '@') {
    return {0, "No space after \"@\" in most cases."};
  }
  if (right.TokenEnum() == '@') {
    return {1, "Space before \"@\" in most cases."};
  }

  // Do not force space between '^' and '{' operators
  if (context.IsInsideFirst({NodeEnum::kUnaryPrefixExpression}, {})) {
    if (IsUnaryOperator(static_cast<yytokentype>(left.TokenEnum())) &&
        right.TokenEnum() == '{') {
      return {0, "No space between unary and concatenation operators"};
    }
  }

  // Add missing space around either side of all types of assignment operator.
  // "assign foo = bar;"  instead of "assign foo =bar;"
  // Consider assignment operators in the same class as binary operators.
  if (left.format_token_enum == FormatTokenType::binary_operator ||
      right.format_token_enum == FormatTokenType::binary_operator) {
    return {1, "Space around binary and assignment operators"};
  }

  // If the token on either side is an empty string, do not inject any
  // additional spaces.  This can occur with some lexical tokens like
  // yytokentype::PP_define_body.
  if (left.token->text.empty() || right.token->text.empty()) {
    return {0, "No additional space around empty-string tokens."};
  }

  // Remove any extra spaces between numeric literals' width, base and digits.
  // "16'h123, 'h123" instead of "16 'h123", "16'h 123, 'h 123"
  if (IsInsideNumericLiteral(left, right)) {
    return {0, "No space inside based numeric literals"};
  }

  if (context.IsInsideFirst(
          {NodeEnum::kUdpCombEntry, NodeEnum::kUdpSequenceEntry}, {})) {
    // Spacing before ';' is handled above
    return {1, "One space around UDP entries"};
  }

  // TODO(fangism): Never insert trailing spaces before a newline.

  // Hierarchy examples: "a.b", "a::b"
  if (left.format_token_enum == FormatTokenType::hierarchy ||
      right.format_token_enum == FormatTokenType::hierarchy)
    return {0,
            "No space separating hierarchy components "
            "(separated by . or ::)"};
  // TODO(fangism): space between numeric literals and '.'
  // Don't want to accidentally form m.d floating-point values.

  // cast operator, e.g. "void'(...)"
  if (right.TokenEnum() == '\'' || left.TokenEnum() == '\'') {
    return {0, "No space around cast operator '\\''"};
  }

  if (right.TokenEnum() == '(') {
    // "#(" vs. "# (" for parameter formals and arguments
    if (left.TokenEnum() == '#') return {0, "Fuse \"#(\""};

    // ") (" vs. ")(" for between parameter and port formals
    if (left.TokenEnum() == ')') {
      return {1, "Separate \") (\" between parameters and ports"};
    }

    // General handling of ID '(' spacing:
    if (left.format_token_enum == FormatTokenType::identifier ||
        IsKeywordCallable(yytokentype(left.TokenEnum()))) {
      if (context.IsInside(NodeEnum::kActualNamedPort) ||
          context.IsInside(NodeEnum::kPort)) {
        return {0, "Named port: no space between ID and '('"};
      }
      if (context.IsInside(NodeEnum::kGateInstance)) {
        return {1, "Module instance: want space between ID and '('"};
      }
      if (context.IsInside(NodeEnum::kModuleHeader)) {
        return {1,
                "Module/interface declarations: want space between ID and '('"};
      }
      // Default: This case intended to cover function/task/macro calls:
      return {0, "Function/constructor calls: no space before ("};
    }
  }

  if (left.TokenEnum() == '}') {
    // e.g. typedef struct { ... } foo_t;
    return {1, "Space after '}' in most other cases."};
  }
  if (right.TokenEnum() == '{') {
    if (left.format_token_enum == FormatTokenType::keyword) {
      return {1, "Space between keyword and '{'."};
    }
    if (context.DirectParentsAre(
            {NodeEnum::kBraceGroup, NodeEnum::kConstraintDeclaration})) {
      return {1, "Space before '{' when opening a constraint definition body."};
    }
    if (context.DirectParentsAre(
            {NodeEnum::kBraceGroup, NodeEnum::kCoverPoint})) {
      return {1, "Space before '{' when opening a coverpoint body."};
    }
    return {0, "No space before '{' in most other contexts."};
  }

  // Handle padding around packed array dimensions like "type [N] id;"
  if ((left.format_token_enum == FormatTokenType::keyword ||
       left.format_token_enum == FormatTokenType::identifier) &&
      right.TokenEnum() == '[') {
    if (context.IsInsideFirst({NodeEnum::kPackedDimensions},
                              {NodeEnum::kExpression})) {
      // "type [packed...]" (space between type and packed dimensions)
      // avoid touching any expressions inside the packed dimensions
      return {1, "spacing before [packed dimensions] of declarations"};
    }
    // All other contexts, such as "a[i]" indices, no space.
    return {0, "All other cases of \".*[\", no space"};
  }
  if (left.TokenEnum() == ']' &&
      right.format_token_enum == FormatTokenType::identifier) {
    if (context.DirectParentsAre(
            {NodeEnum::kUnqualifiedId,
             NodeEnum::kDataTypeImplicitBasicIdDimensions})) {
      // "[packed...] id" (space between packed dimensions and id)
      return {1, "spacing after [packed dimensions] of declarations"};
    }
    // Not sure if "] id" appears in any other context, so leave it unhandled.
  }

  // Cannot merge tokens that would result in a different token.
  if (PairwiseNonmergeable(left) && PairwiseNonmergeable(right)) {
    return {1, "Cannot pair {number, identifier, keyword} without space."};
  }

  if (right.TokenEnum() == ':') {
    if (left.TokenEnum() == TK_default) {
      return {0, "No space inside \"default:\""};
    }
    if (context.DirectParentIsOneOf(
            {NodeEnum::kCaseItem, NodeEnum::kCaseInsideItem,
             NodeEnum::kCasePatternItem, NodeEnum::kGenerateCaseItem,
             NodeEnum::kPropertyCaseItem, NodeEnum::kRandSequenceCaseItem,
             NodeEnum::kCoverPoint})) {
      return {0, "Case-like items, no space before ':'"};
    }

    // Everything that resembles an end-label should have 1 space
    //   example nodes: kLabel, kEndNew, kFunctionEndLabel
    if (IsEndKeyword(yytokentype(left.TokenEnum()))) {
      return {1, "Want 1 space between end-keyword and ':'"};
    }

    // Spacing between 'begin' and ':' is already covered
    // Spacing between 'fork' and ':' is already covered

    // Everything that resembles a prefix-statement label,
    // and label before 'begin'
    if (context.DirectParentIsOneOf({NodeEnum::kBlockIdentifier,
                                     NodeEnum::kLabeledStatement,
                                     NodeEnum::kGenerateBlock})) {
      return {1, "1 space before ':' in prefix block labels"};
    }

    // kTernaryExpression should have 1 space
    if (context.DirectParentIs(NodeEnum::kTernaryExpression)) {
      return {1, "Ternary ?: expression wants 1 space around ':'"};
    }

    // Spacing in ranges
    if (InRangeLikeContext(context)) {
      int spaces = right.OriginalLeadingSpaces().length();
      if (spaces > 1) {
        spaces = 1;
      }
      return {spaces, "Limit spaces before ':' in bit slice to 0 or 1"};
    }
    if (context.DirectParentIs(NodeEnum::kValueRange)) {
      return {1, "Spaces around ':' in value ranges."};
    }

    // TODO(fangism): Everything that resembles a range (in index, dimensions)
    // should have 1 space.
    //   kValueRange, kCycleRange
    //   kMinTypMax expressions?

    // TODO(fangism): Other unknowns:
    //   'enum_name' in verilog.y
    //   kMemberPattern?
    //   kPatternExpression?
    //   ':' as a polarity operator?
    //   as a UDP combinational entry? UDP sequence entry?
    //   kBindDirective?
    //   kCoverCross? kCoverPoint?
    //   kProduction? (randsequence)

    // For now, if case is not explicitly handled, preserve existing space.
  }
  if (left.TokenEnum() == ':') {
    // Spacing in ranges
    if (InRangeLikeContext(context)) {
      // Take advantage here that the left token was already annotated (above)
      return {left.before.spaces_required,
              "Symmetrize spaces before and after ':' in bit slice"};
    }
    // Most contexts want a space after ':'.
    return {1, "Default to 1 space after ':'"};
  }

  // "if (...)", "for (...) instead of "if(...)", "for(...)",
  // "case ...", "return ..."
  if (left.format_token_enum == FormatTokenType::keyword) {
    // TODO(b/144605476): function-like keywords, however, do not get a space.
    return {1, "Space between flow control keywords and ("};
  }

  if (left.format_token_enum == FormatTokenType::unary_operator)
    return {0, "++i over ++ i"};  // "++i" instead of "++ i"
  if (right.format_token_enum == FormatTokenType::unary_operator)
    return {0, "i++ over i ++"};  // "i++" instead of "i ++"

  // TODO(fangism): handle ranges [ ... : ... ]

  if (left.TokenEnum() == TK_DecNumber &&
      right.TokenEnum() == TK_UnBasedNumber) {
    // e.g. 1'b1, 16'hbabe
    return {0, "No space between numeric width and un-based number"};
  }

  // Brackets in multi-dimensional arrays/indices.
  if (left.TokenEnum() == ']' && right.TokenEnum() == '[') {
    return {0, "No spaces separating multidimensional arrays/indices"};
  }

  if (left.TokenEnum() == '#') {
    return {0, "No spaces after # (delay expressions, parameters)."};
  }
  if (right.TokenEnum() == '#') {
    // This may be controversial or context-dependent, as parameterized
    // classes often appear with method calls like:
    //   type#(params...)::method(...);
    return {1, "Spaces before # in most other contexts."};
  }

  if (right.format_token_enum == FormatTokenType::keyword) {
    return {1, "Space before keywords in most other cases."};
  }

  // e.g. always_ff @(posedge clk) begin ...
  // e.g. case (expr): ...
  if (left.TokenEnum() == ')') {
    switch (right.TokenEnum()) {
      case ':':
        return {0, "No space between ')' and ':'."};
      default:
        break;
    }
    return {1, "Space between ')' and most other tokens"};
  }
  if (left.TokenEnum() == yytokentype::MacroCallCloseToEndLine) {
    if (IsAnySemicolon(right)) {
      return {0, "No space between macro-closing ')' and ';'"};
    }
    // Really only expect comments to follow macro-closing ')'
    return {1, "Space between macro-closing ')' and most other tokens"};
  }
  if (left.TokenEnum() == ']') {
    return {1, "Space between ']' and most other tokens"};
  }

  if (IsPreprocessorKeyword(static_cast<yytokentype>(right.TokenEnum()))) {
    // most of these should start on their own line anyway
    return {1, "Preprocessor keywords should be separated from token on left."};
  }

  if (IsComment(FormatTokenType(left.format_token_enum))) {
    // Nothing should ever be to the right of an EOL comment.
    // But we have to explicitly handle these cases to prevent them from
    // unintentionally preserving spacing after comments.
    return {1, "Handle left=comment to avoid preserving unwanted spaces."};
  }

  // Case was not explicitly handled.
  return {kUnhandledSpacesRequired, "Default: spacing not explicitly handled"};
}

struct SpacePolicy {
  int spaces_required;
  bool force_preserve_spaces;
};

static SpacePolicy SpacesRequiredBetween(const FormatStyle& style,
                                         const PreFormatToken& left,
                                         const PreFormatToken& right,
                                         const SyntaxTreeContext& context) {
  // Default for unhandled cases, 1 space to be conservative.
  constexpr int kUnhandledSpacesDefault = 1;
  const auto spaces = SpacesRequiredBetween(left, right, context);
  VLOG(1) << "spaces: " << spaces.value << ", reason: " << spaces.reason;

  if (spaces.value == kUnhandledSpacesRequired) {
    VLOG(1) << "Unhandled inter-token spacing between "
            << verilog_symbol_name(left.TokenEnum()) << " and "
            << verilog_symbol_name(right.TokenEnum()) << ", defaulting to "
            << kUnhandledSpacesDefault;
    return SpacePolicy{kUnhandledSpacesDefault, true};
  }
  // else spacing was explicitly handled in a case
  return SpacePolicy{spaces.value, false};
}

static WithReason<int> BreakPenaltyBetweenTokens(
    const verible::PreFormatToken& left, const verible::PreFormatToken& right) {
  // Higher precedence rules should be handled earlier in this function.
  if (left.format_token_enum == FormatTokenType::identifier &&
      right.format_token_enum == FormatTokenType::open_group) {
    return {20, "identifier, open-group"};
  }
  // Hierarchy examples: "a.b", "a::b"
  // TODO(fangism): '.' is not always hierarchy, differentiate by context.
  // slightly prefer to break on the left: "a .b" better than "a. b"
  if (left.format_token_enum == FormatTokenType::hierarchy)
    return {50, "hierarchy separator on left"};
  if (right.format_token_enum == FormatTokenType::hierarchy)
    return {45, "hierarchy separator on right"};

  // Prefer to split after commas rather than before them.
  if (right.TokenEnum() == ',') return {10, "avoid breaking before ','"};

  // Prefer to not split directly at an assignment.
  if (left.TokenEnum() == '=') return {2, "right is '='"};

  // Prefer to keep '(' with whatever is on the left.
  // TODO(fangism): ... except when () is used as precedence.
  if (right.format_token_enum == FormatTokenType::open_group)
    return {5, "right is open-group"};

  if (left.TokenEnum() == TK_DecNumber &&
      right.TokenEnum() == TK_UnBasedNumber) {
    // e.g. 1'b1, 16'hbabe
    // doesn't really matter, because we never break here
    return {90, "numeric width, base"};
  }

  // TODO(b/148552963): make this return 0, as relative incremental cost
  // return {0, "no further adjustment (default)"};
  // The default cost of 1 is too low, and is terrible for search convergence.

  constexpr int kUnhandledWrapPenalty = 1;
  return {kUnhandledWrapPenalty, "Unhandled wrap penalty."};
}

// Returns the split penalty for line-breaking before the right token.
static WithReason<int> BreakPenaltyBetween(const verible::PreFormatToken& left,
                                           const verible::PreFormatToken& right,
                                           const SyntaxTreeContext& context) {
  VLOG(3) << "Inter-token penalty between "
          << verilog_symbol_name(left.TokenEnum()) << " and "
          << verilog_symbol_name(right.TokenEnum());

  constexpr int kMinPenalty = 1;  // absolute minimum

  // This factor only looks at left and right tokens:
  const auto inter_token_penalty = BreakPenaltyBetweenTokens(left, right);
  VLOG(3) << "inter-token break penalty: " << inter_token_penalty.value << ", "
          << inter_token_penalty.reason;

  // TODO(b/148552963): impose depth_penalty (not death penalty)
  const int total_penalty = std::max(inter_token_penalty.value, kMinPenalty);

  VLOG(3) << "total break penalty: " << total_penalty;
  return {total_penalty, inter_token_penalty.reason};
}

// Returns decision whether to break, not break, or evaluate both choices.
static WithReason<SpacingOptions> BreakDecisionBetween(
    const FormatStyle& style, const PreFormatToken& left,
    const PreFormatToken& right, const SyntaxTreeContext& context) {
  // For now, leave everything inside [dimensions] alone.
  if (context.IsInsideFirst(
          {NodeEnum::kDimensionRange, NodeEnum::kDimensionScalar}, {})) {
    // ... except for the spacing immediately around '[' and ']',
    // which is covered by other rules.
    if (left.TokenEnum() != '[' && left.TokenEnum() != ']' &&
        right.TokenEnum() != '[' && right.TokenEnum() != ']' &&
        left.TokenEnum() != ':' && right.TokenEnum() != ':') {
      return {SpacingOptions::Preserve,
              "For now, leave spaces inside [] untouched."};
    }
  }

  if (left.TokenEnum() == PP_define) {
    return {SpacingOptions::MustAppend,
            "Keep `define and macro name together."};
  }
  if (right.TokenEnum() == PP_define_body) {
    // TODO(b/141517267): reflow macro definition text with flexible
    // line-continuations.
    return {SpacingOptions::MustAppend,
            "Macro definition body must start on same line (but may be "
            "line-continued)."};
  }

  // Check for mandatory line breaks.
  if (left.format_token_enum == FTT::eol_comment ||
      left.TokenEnum() == PP_define_body  // definition excludes trailing '\n'
  ) {
    return {SpacingOptions::MustWrap, "Token must be newline-terminated"};
  }

  if (right.format_token_enum == FTT::eol_comment) {
    // Check if there are any newlines between these tokens' texts.
    // Caution: when testing this case, must provide valid text between
    // tokens to avoid reading uninitialized memory.
    auto preceding_whitespace = verible::make_string_view_range(
        left.token->text.end(), right.token->text.begin());

    auto pos = preceding_whitespace.find_first_of('\n', 0);
    if (pos == absl::string_view::npos) {
      // There are other tokens on this line
      return {SpacingOptions::MustAppend,
              "EOL comment cannot break from "
              "tokens to the left on its line"};
    }
  }

  // TODO(fangism): check for all token types in verilog.lex that
  // scan to an end-of-line, even if it returns the newline to scanning with
  // yyless().

  // Unary operators (context-sensitive)
  // For now, never separate unary prefix operators from their operands.
  if (IsUnaryPrefixExpressionOperand(left, context)) {
    return {SpacingOptions::MustAppend,
            "Never separate unary prefix operator from its operand"};
  }

  if (IsInsideNumericLiteral(left, right)) {
    return {SpacingOptions::MustAppend,
            "Never separate numeric width, base, and digits"};
  }

  // Preprocessor macro definitions with args: no space between ID and '('.
  if (left.TokenEnum() == PP_Identifier && right.TokenEnum() == '(') {
    return {SpacingOptions::MustAppend, "No space between macro call id and ("};
  }

  // TODO(fangism): No break between `define and PP_Identifier.

  if (IsEndKeyword(yytokentype(right.TokenEnum()))) {
    return {SpacingOptions::MustWrap, "end* keywords should start own lines"};
  }

  if (right.TokenEnum() == TK_else) {
    if (left.TokenEnum() != TK_end)
      return {SpacingOptions::MustWrap,
              "'else' token should start its own line unless preceded by 'end' "
              "without label."};
    else
      return {SpacingOptions::MustAppend,
              "'end'-'else' tokens should be together on one line."};
  }

  if ((left.TokenEnum() == TK_else) && (right.TokenEnum() == TK_begin)) {
    return {SpacingOptions::MustAppend,
            "'else'-'begin' tokens should be together on one line."};
  }

  if ((left.TokenEnum() == ')') && (right.TokenEnum() == TK_begin)) {
    return {SpacingOptions::MustAppend,
            "')'-'begin' tokens should be together on one line."};
  }

  if (left.TokenEnum() == yytokentype::MacroCallCloseToEndLine) {
    if (!IsComment(FormatTokenType(right.format_token_enum)) &&
        !IsAnySemicolon(right)) {
      return {SpacingOptions::MustWrap,
              "Macro-closing ')' should end its own line except for comments "
              "nad ';'."};
    }
  }

  if (left.TokenEnum() == PP_else || left.TokenEnum() == PP_endif) {
    if (IsComment(FormatTokenType(right.format_token_enum))) {
      return {SpacingOptions::Undecided, "Comment may follow `else and `end"};
    }
    return {SpacingOptions::MustWrap,
            "`end and `else should be on their own line except for comments."};
  }

  if (IsPreprocessorKeyword(static_cast<yytokentype>(right.TokenEnum()))) {
    // The tree unwrapper should make sure these start their own partition.
    return {SpacingOptions::MustWrap,
            "Preprocessor directives should start their own line."};
  }

  // By default, leave undecided for penalty minimization.
  return {SpacingOptions::Undecided,
          "Default: leave wrap decision to algorithm"};
}

// Sets pointers that establish substring ranges of (whitespace) text *between*
// non-whitespace tokens.
static void AnnotateOriginalSpacing(
    const char* buffer_start,
    std::vector<verible::PreFormatToken>::iterator tokens_begin,
    std::vector<verible::PreFormatToken>::iterator tokens_end) {
  VLOG(4) << __FUNCTION__;
  CHECK(buffer_start != nullptr);
  for (auto& ftoken : verible::make_range(tokens_begin, tokens_end)) {
    ftoken.before.preserved_space_start = buffer_start;
    VLOG(4) << "original spacing: \""
            << verible::make_string_view_range(buffer_start,
                                               ftoken.Text().begin())
            << "\"";
    buffer_start = ftoken.Text().end();
  }
  // This does not cover the spacing between the last token and EOF.
}

// Extern linkage for sake of direct testing, though not exposed in public
// headers.
// TODO(fangism): could move this to a -internal.h header.
void AnnotateFormatToken(const FormatStyle& style,
                         const PreFormatToken& prev_token,
                         PreFormatToken* curr_token,
                         const SyntaxTreeContext& context) {
  const auto p = SpacesRequiredBetween(style, prev_token, *curr_token, context);
  curr_token->before.spaces_required = p.spaces_required;
  if (p.force_preserve_spaces) {
    // forego all inter-token calculations
    curr_token->before.break_decision = SpacingOptions::Preserve;
  } else {
    // Update the break penalty and if the curr_token is allowed to
    // break before it.
    const auto break_penalty =
        BreakPenaltyBetween(prev_token, *curr_token, context);
    curr_token->before.break_penalty = break_penalty.value;
    const auto breaker =
        BreakDecisionBetween(style, prev_token, *curr_token, context);
    curr_token->before.break_decision = breaker.value;
    VLOG(3) << "line break constraint: " << breaker.reason;
  }
}

void AnnotateFormattingInformation(
    const FormatStyle& style, const verible::TextStructureView& text_structure,
    std::vector<verible::PreFormatToken>::iterator tokens_begin,
    std::vector<verible::PreFormatToken>::iterator tokens_end) {
  // This interface just forwards the relevant information from text_structure.
  AnnotateFormattingInformation(style, text_structure.Contents().begin(),
                                text_structure.SyntaxTree().get(),
                                text_structure.EOFToken(), tokens_begin,
                                tokens_end);
}

void AnnotateFormattingInformation(
    const FormatStyle& style, const char* buffer_start,
    const verible::Symbol* syntax_tree_root,
    const verible::TokenInfo& eof_token,
    std::vector<verible::PreFormatToken>::iterator tokens_begin,
    std::vector<verible::PreFormatToken>::iterator tokens_end) {
  if (tokens_begin == tokens_end) {  // empty range
    return;
  }

  if (buffer_start != nullptr) {
    // For unit testing, tokens' text snippets don't necessarily originate
    // from the same contiguous string buffer, so skip this step.
    AnnotateOriginalSpacing(buffer_start, tokens_begin, tokens_end);
  }

  // Annotate inter-token information using the syntax tree for context.
  AnnotateFormatTokensUsingSyntaxContext(
      syntax_tree_root, eof_token, tokens_begin, tokens_end,
      // lambda: bind the FormatStyle, forwarding all other arguments
      [&style](const PreFormatToken& prev_token, PreFormatToken* curr_token,
               const SyntaxTreeContext& context) {
        AnnotateFormatToken(style, prev_token, curr_token, context);
      });
}

}  // namespace formatter
}  // namespace verilog
