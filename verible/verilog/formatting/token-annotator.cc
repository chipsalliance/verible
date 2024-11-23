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
#include <iterator>
#include <utility>
#include <vector>

#include "absl/strings/string_view.h"
#include "verible/common/formatting/format-token.h"
#include "verible/common/formatting/tree-annotator.h"
#include "verible/common/strings/range.h"
#include "verible/common/text/symbol.h"
#include "verible/common/text/syntax-tree-context.h"
#include "verible/common/text/text-structure.h"
#include "verible/common/text/token-info.h"
#include "verible/common/util/logging.h"
#include "verible/common/util/with-reason.h"
#include "verible/verilog/CST/verilog-nonterminals.h"
#include "verible/verilog/formatting/format-style.h"
#include "verible/verilog/formatting/verilog-token.h"
#include "verible/verilog/parser/verilog-parser.h"
#include "verible/verilog/parser/verilog-token-classifications.h"
#include "verible/verilog/parser/verilog-token-enum.h"

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

static bool IsUnaryPrefixExpressionOperand(const PreFormatToken &left,
                                           const SyntaxTreeContext &context) {
  return (IsUnaryOperator(verilog_tokentype(left.TokenEnum())) &&
          context.IsInsideFirst({NodeEnum::kUnaryPrefixExpression},
                                {NodeEnum::kExpression})) ||
         // Treat '##' like a unary prefix operator.
         left.TokenEnum() == verilog_tokentype::TK_POUNDPOUND;
}

static bool IsInsideNumericLiteral(const PreFormatToken &left,
                                   const PreFormatToken &right) {
  return (left.format_token_enum == FormatTokenType::numeric_literal &&
          right.format_token_enum == FormatTokenType::numeric_base) ||
         left.format_token_enum == FormatTokenType::numeric_base;
}

// Returns true if keyword can be used like a function/method call.
// Based on various LRM sections mentioning subroutine calls.
static bool IsKeywordCallable(verilog_tokentype e) {
  switch (e) {
    case TK_and:  // array method
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
static bool PairwiseNonmergeable(const PreFormatToken &ftoken) {
  return ftoken.TokenEnum() == TK_DecNumber ||
         ftoken.format_token_enum == FormatTokenType::identifier ||
         ftoken.format_token_enum == FormatTokenType::keyword;
}

static bool InDeclaredDimensions(const SyntaxTreeContext &context) {
  return context.IsInsideFirst(
      {NodeEnum::kPackedDimensions, NodeEnum::kUnpackedDimensions}, {});
}

static bool InRangeLikeContext(const SyntaxTreeContext &context) {
  return context.IsInsideFirst(
      {NodeEnum::kDimensionScalar, NodeEnum::kDimensionRange,
       NodeEnum::kDimensionSlice, NodeEnum::kCycleDelayRange},
      {});
}

static bool IsAnySemicolon(const PreFormatToken &ftoken) {
  // These are just syntactically disambiguated versions of ';'.
  return ftoken.TokenEnum() == ';' ||
         ftoken.TokenEnum() ==
             verilog_tokentype::SemicolonEndOfAssertionVariableDeclarations;
}

// Returns minimum number of spaces required between left and right token.
// Returning kUnhandledSpacesRequired means the case was not explicitly
// handled, and it is up to the caller to decide what to do when this happens.
static WithReason<int> SpacesRequiredBetween(
    const PreFormatToken &left, const PreFormatToken &right,
    const SyntaxTreeContext &left_context,
    const SyntaxTreeContext &right_context, const FormatStyle &style) {
  VLOG(3) << "Spacing between " << verilog_symbol_name(left.TokenEnum())
          << " and " << verilog_symbol_name(right.TokenEnum());
  // Higher precedence rules should be handled earlier in this function.

  // Preserve space after escaped identifiers.
  if (left.TokenEnum() == EscapedIdentifier) {
    return {1, "Escaped identifiers must end with whitespace."};
  }

  if (right.TokenEnum() == verilog_tokentype::TK_LINE_CONT) {
    return {0, "Add no spaces before \\ line continuation."};
  }
  if (left.TokenEnum() == verilog_tokentype::TK_LINE_CONT) {
    return {0, "Add no spaces after \\ line continuation."};
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

  // Unary operators (context-sensitive)
  if (IsUnaryPrefixExpressionOperand(left, right_context) &&
      (left.format_token_enum != FormatTokenType::binary_operator ||
       !IsUnaryOperator(static_cast<verilog_tokentype>(right.TokenEnum())))) {
    // TODO: There are _some_ unary operators on the right that could
    // be formatted with 0-space, for example:
    // 'a = & ~b'; could be 'a = &~b;'
    return {0, "Bind unary prefix operator close to its operand."};
  }

  if (left.TokenEnum() == TK_SCOPE_RES) {
    return {0, R"(Prefer "::id" over ":: id", \"::*" over ":: *")"};
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

  if (left.TokenEnum() == TK_return) {
    return {1, "Space between return keyword and return value"};
  }

  if (right_context.IsInsideFirst({NodeEnum::kStreamingConcatenation}, {}) &&
      style.compact_indexing_and_selections) {
    if (left.TokenEnum() == TK_LS || left.TokenEnum() == TK_RS) {
      return {0, "No space around streaming operators"};
    }
    if (left.format_token_enum == FormatTokenType::numeric_literal ||
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
  if (right_context.IsInsideFirst({NodeEnum::kUnaryPrefixExpression}, {})) {
    if (IsUnaryOperator(static_cast<verilog_tokentype>(left.TokenEnum())) &&
        right.TokenEnum() == '{') {
      return {0, "No space between unary and concatenation operators"};
    }
  }

  // Add missing space around either side of all types of assignment operator.
  // "assign foo = bar;"  instead of "assign foo =bar;"
  // Consider assignment operators in the same class as binary operators.
  if (left.format_token_enum == FormatTokenType::binary_operator ||
      right.format_token_enum == FormatTokenType::binary_operator) {
    // Inside [], allows 0 or 1 spaces, and symmetrize.
    // TODO(fangism): make this behavior configurable
    if (right.format_token_enum == FormatTokenType::binary_operator &&
        InRangeLikeContext(right_context)) {
      if (style.compact_indexing_and_selections &&
          !InDeclaredDimensions(right_context)) {
        return {0,
                "Compact binary expressions inside indexing / bit selection "
                "operator []"};
      }

      int spaces = right.OriginalLeadingSpaces().length();
      if (spaces > 1) {
        spaces = 1;
      }
      return {spaces, "Limit <= 1 space before binary operator inside []."};
    }
    if (left.format_token_enum == FormatTokenType::binary_operator &&
        InRangeLikeContext(left_context)) {
      return {left.before.spaces_required,
              "Symmetrize spaces before and after binary operator inside []."};
    }
    return {1, "Space around binary and assignment operators"};
  }

  // If the token on either side is an empty string, do not inject any
  // additional spaces.  This can occur with some lexical tokens like
  // verilog_tokentype::PP_define_body.
  if (left.token->text().empty() || right.token->text().empty()) {
    return {0, "No additional space around empty-string tokens."};
  }

  // Remove any extra spaces between numeric literals' width, base and digits.
  // "16'h123, 'h123" instead of "16 'h123", "16'h 123, 'h 123"
  if (IsInsideNumericLiteral(left, right)) {
    return {0, "No space inside based numeric literals"};
  }

  if (right_context.IsInsideFirst(
          {NodeEnum::kUdpCombEntry, NodeEnum::kUdpSequenceEntry}, {})) {
    // Spacing before ';' is handled above
    return {1, "One space around UDP entries"};
  }

  // TODO(fangism): Never insert trailing spaces before a newline.

  // Hierarchy examples: "a.b", "a::b"
  if (left.format_token_enum == FormatTokenType::hierarchy ||
      right.format_token_enum == FormatTokenType::hierarchy) {
    return {0,
            "No space separating hierarchy components "
            "(separated by . or ::)"};
  }
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
        IsKeywordCallable(verilog_tokentype(left.TokenEnum()))) {
      // TODO(fangism): This logic should use .DirectParentIs() to minimize risk
      // of unintended reach.
      if (right_context.IsInside(NodeEnum::kActualNamedPort) ||
          right_context.IsInside(NodeEnum::kPort)) {
        return {0, "Named port: no space between ID and '('"};
      }
      if (right_context.IsInside(NodeEnum::kPrimitiveGateInstance)) {
        return {1, "Primitive instance: want space between ID and '('"};
      }
      if (left_context.DirectParentIs(NodeEnum::kGateInstance) &&
          right_context.IsInside(NodeEnum::kGateInstance)) {
        return {1, "Module declarations: want space between ID and '('"};
      }
      if (left_context.DirectParentIs(NodeEnum::kModuleHeader)) {
        return {1,
                "Module/interface declarations: want space between ID and '('"};
      }
      // Default: This case intended to cover function/task/macro calls:
      return {0, "Function/constructor calls: no space before ("};
    }
  }

  if (left.TokenEnum() == ':') {
    // Spacing in ranges
    if (InRangeLikeContext(right_context)) {
      // Take advantage here that the left token was already annotated (above)
      return {left.before.spaces_required,
              "Symmetrize spaces before and after ':' in bit slice"};
    }
    // Most contexts want a space after ':'.
    return {1, "Default to 1 space after ':'"};
  }

  if (left.TokenEnum() == '}') {
    // e.g. typedef struct { ... } foo_t;
    return {1, "Space after '}' in most other cases."};
  }
  if (right.TokenEnum() == '{') {
    if (left.format_token_enum == FormatTokenType::keyword) {
      return {1, "Space between keyword and '{'."};
    }
    if (right_context.DirectParentsAre(
            {NodeEnum::kBraceGroup, NodeEnum::kConstraintDeclaration})) {
      return {1, "Space before '{' when opening a constraint definition body."};
    }
    if (right_context.DirectParentsAre(
            {NodeEnum::kBraceGroup, NodeEnum::kCoverPoint})) {
      return {1, "Space before '{' when opening a coverpoint body."};
    }
    if (right_context.DirectParentsAre(
            {NodeEnum::kBraceGroup, NodeEnum::kEnumType})) {
      return {1, "Space before '{' when opening an enum type."};
    }
    if (left.TokenEnum() == ')') {
      return {1, "Space betwen ')' and '{', e.g. conditional constraint."};
    }
    if (left.TokenEnum() == ']' && InDeclaredDimensions(left_context)) {
      return {1, "Space between declared array type and '{' (e.g. in typedef)"};
    }
    return {0, "No space before '{' in most other contexts."};
  }

  // Handle padding around packed array dimensions like "type [N] id;"
  if ((left.format_token_enum == FormatTokenType::keyword ||
       left.format_token_enum == FormatTokenType::identifier) &&
      right.TokenEnum() == '[') {
    if (right_context.IsInsideFirst({NodeEnum::kPackedDimensions},
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
    if (right_context.DirectParentsAre(
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
    if (right_context.DirectParentIsOneOf(
            {NodeEnum::kCaseItem, NodeEnum::kCaseInsideItem,
             NodeEnum::kCasePatternItem, NodeEnum::kGenerateCaseItem,
             NodeEnum::kPropertyCaseItem, NodeEnum::kRandSequenceCaseItem,
             NodeEnum::kCoverPoint})) {
      return {0, "Case-like items, no space before ':'"};
    }

    // Everything that resembles an end-label should have 1 space
    //   example nodes: kLabel, kEndNew, kFunctionEndLabel
    if (IsEndKeyword(verilog_tokentype(left.TokenEnum()))) {
      return {1, "Want 1 space between end-keyword and ':'"};
    }

    // Spacing between 'begin' and ':' is already covered
    // Spacing between 'fork' and ':' is already covered

    // Everything that resembles a prefix-statement label,
    // and label before 'begin'
    if (right_context.DirectParentIsOneOf({NodeEnum::kBlockIdentifier,
                                           NodeEnum::kLabeledStatement,
                                           NodeEnum::kGenerateBlock})) {
      return {1, "1 space before ':' in prefix block labels"};
    }

    // kConditionExpression should have 1 space
    if (right_context.DirectParentIs(NodeEnum::kConditionExpression)) {
      return {1, "condition ?: expression wants 1 space around ':'"};
    }

    // Spacing in ranges
    if (InRangeLikeContext(right_context)) {
      int spaces = right.OriginalLeadingSpaces().length();
      if (spaces > 1) {
        // ExcessSpaces returns 0 if there was a newline - prevents
        // counting indentation as spaces
        spaces = right.ExcessSpaces() ? 1 : 0;
      }
      return {spaces, "Limit spaces before ':' in bit slice to 0 or 1"};
    }
    if (right_context.DirectParentIs(NodeEnum::kValueRange)) {
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

  // "if (...)", "for (...) instead of "if(...)", "for(...)",
  // "case ...", "return ..."
  if (left.format_token_enum == FormatTokenType::keyword) {
    // TODO(b/144605476): function-like keywords, however, do not get a space.
    return {1, "Space between flow control keywords and ("};
  }

  if (left.TokenEnum() == verilog_tokentype::TK_TimeLiteral) {
    if (right.TokenEnum() == ';') {
      return {0, "No space between time literal and ';'."};
    }
    return {1, "Space after time literals in most other cases."};
  }

  if (right.TokenEnum() == TK_POUNDPOUND) {
    return {1, "Space before ## (delay) operator"};
  }
  if (left.format_token_enum == FormatTokenType::unary_operator) {
    return {0, "++i over ++ i"};  // "++i" instead of "++ i"
  }
  if (right.format_token_enum == FormatTokenType::unary_operator) {
    return {0, "i++ over i ++"};  // "i++" instead of "i ++"
  }

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
    if (left_context.DirectParentIs(NodeEnum::kUnqualifiedId) &&
        !left_context.IsInsideFirst(
            {NodeEnum::kInstantiationType, NodeEnum::kBindTargetInstance,
             NodeEnum::kExtendsList, NodeEnum::kBraceGroup},
            {})) {
      return {0, "No space before # when direct parent is kUnqualifiedId."};
    }
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
  if (left.TokenEnum() == verilog_tokentype::MacroCallCloseToEndLine) {
    if (IsAnySemicolon(right)) {
      return {0, "No space between macro-closing ')' and ';'"};
    }
    // Really only expect comments to follow macro-closing ')'
    return {1, "Space between macro-closing ')' and most other tokens"};
  }
  if (left.TokenEnum() == ']') {
    return {1, "Space between ']' and most other tokens"};
  }

  if (IsPreprocessorKeyword(
          static_cast<verilog_tokentype>(right.TokenEnum()))) {
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

static SpacePolicy SpacesRequiredBetween(
    const FormatStyle &style, const PreFormatToken &left,
    const PreFormatToken &right, const SyntaxTreeContext &left_context,
    const SyntaxTreeContext &right_context) {
  // Default for unhandled cases, 1 space to be conservative.
  constexpr int kUnhandledSpacesDefault = 1;
  const auto spaces =
      SpacesRequiredBetween(left, right, left_context, right_context, style);
  VLOG(2) << "spaces: " << spaces.value << ", reason: " << spaces.reason;

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

// Context-independent break penalty factor.
static WithReason<int> BreakPenaltyBetweenTokens(
    const verible::PreFormatToken &left, const verible::PreFormatToken &right) {
  // Higher precedence rules should be handled earlier in this function.
  if (left.format_token_enum == FormatTokenType::identifier &&
      right.format_token_enum == FormatTokenType::open_group) {
    return {20, "identifier, open-group"};
  }
  // Hierarchy examples: "a.b", "a::b"
  // TODO(fangism): '.' is not always hierarchy, differentiate by context.
  // slightly prefer to break on the left: "a .b" better than "a. b"
  if (left.format_token_enum == FormatTokenType::hierarchy) {
    return {50, "hierarchy separator on left"};
  }
  if (right.format_token_enum == FormatTokenType::hierarchy) {
    return {45, "hierarchy separator on right"};
  }

  // Prefer to split after commas rather than before them.
  if (right.TokenEnum() == ',') return {10, "avoid breaking before ','"};
  if (right.TokenEnum() == ';') return {10, "avoid breaking before ';'"};

  if (left.TokenEnum() == ',') return {-5, "encourage breaking after ','"};
  if (left.TokenEnum() == ';') return {-5, "encourage breaking after ';'"};

  // Prefer to split after an assignment operator, rather than before.
  // TODO(fangism): use context to cover all assignment-like cases
  if (right.TokenEnum() == '=') return {8, "right is '='"};

  if ((left.format_token_enum != FormatTokenType::binary_operator ||
       left.TokenEnum() == '=') &&
      right.format_token_enum == FormatTokenType::open_group) {
    // Prefer to keep '(' with a token on the left, as long as it is not binary
    // operator other than '='
    // TODO(fangism): ... except when () is used as precedence.
    return {5, "right is open-group"};
  }
  // Prefer to keep ')' with whatever is on the left.
  if (right.format_token_enum == FormatTokenType::close_group ||
      right.TokenEnum() == verilog_tokentype::MacroCallCloseToEndLine) {
    return {10, "right is close-group"};
  }

  if (left.TokenEnum() == TK_DecNumber &&
      right.TokenEnum() == TK_UnBasedNumber) {
    // e.g. 1'b1, 16'hbabe
    // doesn't really matter, because we never break here
    return {90, "numeric width, base"};
  }

  return {0, "no further adjustment (default)"};
}

static int CommonAncestors(const SyntaxTreeContext &left,
                           const SyntaxTreeContext &right) {
  // TODO(fangism): re-check of common ancestry is slow (linear-time),
  // and could be avoided by memoizing the point of common ancestry between
  // leaves *during* the traversal.
  const auto *shorter = &left;
  const auto *longer = &right;
  // For C++11 compatibility, we use the 3-iterator form of std::mismatch().
  if (shorter->size() > longer->size()) std::swap(shorter, longer);
  const auto first_mismatches =
      std::mismatch(shorter->begin(), shorter->end(), longer->begin());
  const int short_common =
      std::distance(shorter->begin(), first_mismatches.first);
  const int long_common =
      std::distance(longer->begin(), first_mismatches.second);
  CHECK_GE(short_common, 0);
  CHECK_EQ(short_common, long_common);
  return short_common;
}

// Token-independent break penalty factor.
static int ContextBasedPenalty(const SyntaxTreeContext &left_context,
                               const SyntaxTreeContext &right_context) {
  // This factor takes into account syntax tree depth, favoring keeping
  // elements deeper in the tree closer together.
  // The current simple model gives equal weight to every element in the
  // context stack.
  // TODO(fangism): custom weights by syntax tree node type.
  constexpr int kDepthScaleFactor = 2;
  const int num_common = CommonAncestors(left_context, right_context);
  const int penalty = num_common * kDepthScaleFactor;
  return penalty;
}

static WithReason<int> TokensWithContextBreakPenalty(
    const verible::PreFormatToken &left, const verible::PreFormatToken &right,
    const SyntaxTreeContext &left_context,
    const SyntaxTreeContext &right_context) {
  const verilog_tokentype left_type =
      static_cast<verilog_tokentype>(left.TokenEnum());
  const verilog_tokentype right_type =
      static_cast<verilog_tokentype>(right.TokenEnum());
  if (right_context.DirectParentIs(NodeEnum::kConditionExpression) &&
      IsTernaryOperator(right_type)) {
    return {10, "Prefer to split after ternary operators (+10 on left)."};
  }
  if (left_context.DirectParentIs(NodeEnum::kConditionExpression) &&
      IsTernaryOperator(left_type)) {
    return {-5, "Prefer to split after ternary operators (-5 on right)."};
  }
  if (right_context.DirectParentIs(NodeEnum::kBinaryExpression) &&
      right.format_token_enum == FormatTokenType::binary_operator) {
    // This value should be kept small so that binding affinity still honors
    // operator precedence which is currently reflected in syntax tree depth.
    return {8, "Prefer to split after binary operators (+8 on left)."};
  }
  if (left_context.DirectParentIs(NodeEnum::kBinaryExpression) &&
      left.format_token_enum == FormatTokenType::binary_operator) {
    return {-5, "Prefer to split after binary operators (-5 on right)."};
  }
  return {0, "No adjustment."};
}

// Returns the split penalty for line-breaking before the right token.
static WithReason<int> BreakPenaltyBetween(
    const verible::PreFormatToken &left, const verible::PreFormatToken &right,
    const SyntaxTreeContext &left_context,
    const SyntaxTreeContext &right_context) {
  VLOG(3) << "Inter-token penalty between "
          << verilog_symbol_name(left.TokenEnum()) << " and "
          << verilog_symbol_name(right.TokenEnum());

  const int depth_penalty = ContextBasedPenalty(left_context, right_context);
  VLOG(3) << "context break penalty: " << depth_penalty;

  // This factor only looks at left and right tokens:
  const auto inter_token_penalty = BreakPenaltyBetweenTokens(left, right);
  VLOG(3) << "inter-token break penalty: " << inter_token_penalty.value << ", "
          << inter_token_penalty.reason;

  const auto token_with_context_penalty =
      TokensWithContextBreakPenalty(left, right, left_context, right_context);
  VLOG(3) << "token+context break penalty: " << token_with_context_penalty.value
          << ", " << token_with_context_penalty.reason;

  constexpr int kMinPenalty = 1;   // absolute minimum
  constexpr int kPenaltyBias = 5;  // baseline penalty value
  const int total_penalty =
      std::max(kPenaltyBias + depth_penalty + inter_token_penalty.value +
                   token_with_context_penalty.value,
               kMinPenalty);

  VLOG(3) << "total break penalty: " << total_penalty;
  return {total_penalty, inter_token_penalty.reason};
}

// Returns decision whether to break, not break, or evaluate both choices.
static WithReason<SpacingOptions> BreakDecisionBetween(
    const FormatStyle &style, const PreFormatToken &left,
    const PreFormatToken &right, const SyntaxTreeContext &left_context,
    const SyntaxTreeContext &right_context) {
  // For now, leave everything inside [dimensions] alone.
  if (InDeclaredDimensions(right_context)) {
    // ... except for the spacing immediately around '[' and ']',
    // which is covered by other rules.
    if (left.TokenEnum() != '[' && left.TokenEnum() != ']' &&
        right.TokenEnum() != '[' && right.TokenEnum() != ']' &&
        left.TokenEnum() != ':' && right.TokenEnum() != ':') {
      return {SpacingOptions::kPreserve,
              "For now, leave spaces inside [] untouched."};
    }
  }

  if (right.TokenEnum() == verilog_tokentype::TK_LINE_CONT) {
    return {SpacingOptions::kMustAppend,
            "Keep \\ line continuation attached to its left neighbor."};
  }

  if (left.TokenEnum() == verilog_tokentype::TK_LINE_CONT) {
    return {SpacingOptions::kMustWrap,
            "Keep \\ line continuation is always followed by \\n."};
  }

  if (left.TokenEnum() == PP_define) {
    return {SpacingOptions::kMustAppend,
            "Keep `define and macro name together."};
  }
  if (right.TokenEnum() == PP_define_body) {
    // TODO(b/141517267): reflow macro definition text with flexible
    // line-continuations.
    const absl::string_view text = right.Text();
    if (std::count(text.begin(), text.end(), '\n') >= 2) {
      return {SpacingOptions::kPreserve,
              "Preserve spacing before a multi-line macro definition body."};
    }
    return {SpacingOptions::kMustAppend,
            "Macro definition body must start on same line (but may be "
            "line-continued)."};
  }

  // Check for mandatory line breaks.
  if (left.format_token_enum == FTT::eol_comment ||
      left.TokenEnum() == PP_define_body  // definition excludes trailing '\n'
  ) {
    return {SpacingOptions::kMustWrap, "Token must be newline-terminated"};
  }

  if (right.format_token_enum == FTT::eol_comment) {
    // Check if there are any newlines between these tokens' texts.
    // Caution: when testing this case, must provide valid text between
    // tokens to avoid reading uninitialized memory.
    auto preceding_whitespace = verible::make_string_view_range(
        left.token->text().end(), right.token->text().begin());

    auto pos = preceding_whitespace.find_first_of('\n', 0);
    if (pos == absl::string_view::npos) {
      // There are other tokens on this line
      return {SpacingOptions::kMustAppend,
              "EOL comment cannot break from "
              "tokens to the left on its line"};
    }
  }

  if (left.format_token_enum == FTT::comment_block ||
      right.format_token_enum == FTT::comment_block) {
    auto preceding_whitespace = verible::make_string_view_range(
        left.token->text().end(), right.token->text().begin());

    auto pos = preceding_whitespace.find_first_of('\n', 0);
    if (pos != absl::string_view::npos) {
      // TODO(mglb): Preserve would be more suitable, but it doesn't work
      // correctly yet.
      // Add support for "Preserve" in Layout Optimizer.
      // Correctly split partitions before tokens with "Preserve" decision in
      // Tree Unwrapper.
      return {SpacingOptions::kMustWrap,
              "Force-preserve line break around block comment"};
    }
  }

  // TODO(fangism): check for all token types in verilog.lex that
  // scan to an end-of-line, even if it returns the newline to scanning with
  // yyless().

  // Unary operators (context-sensitive)
  // For now, never separate unary prefix operators from their operands.
  if (IsUnaryPrefixExpressionOperand(left, right_context)) {
    return {SpacingOptions::kMustAppend,
            "Never separate unary prefix operator from its operand"};
  }

  if (IsInsideNumericLiteral(left, right)) {
    return {SpacingOptions::kMustAppend,
            "Never separate numeric width, base, and digits"};
  }

  // Preprocessor macro definitions with args: no space between ID and '('.
  if (left.TokenEnum() == PP_Identifier && right.TokenEnum() == '(') {
    return {SpacingOptions::kMustAppend,
            "No space between macro call id and ("};
  }

  // TODO(fangism): No break between `define and PP_Identifier.

  if (IsEndKeyword(verilog_tokentype(right.TokenEnum()))) {
    return {SpacingOptions::kMustWrap, "end* keywords should start own lines"};
  }

  if (right.TokenEnum() == TK_else) {
    // TODO(fangism): feels like this should be the responsibility of
    // tree_unwrapper, handled by kElseClause, kGenerateElseClause, etc.
    if (left.TokenEnum() == TK_end && !style.wrap_end_else_clauses) {
      return {SpacingOptions::kMustAppend,
              "'end'-'else' and should be together on one line."};
    }
    if (left.TokenEnum() == TK_end && style.wrap_end_else_clauses) {
      return {SpacingOptions::kMustWrap, "'end'-'else' Should be split."};
    }
    if (left.TokenEnum() == '}') {
      return {SpacingOptions::kMustAppend,
              "'}'-'else' and should be together on one line."};
    }
    // TODO(fangism): Some styles prefer to start with 'else' on its own line.
    return {SpacingOptions::kMustWrap, "'else' starts its own line."};
  }

  if ((left.TokenEnum() == TK_else) && (right.TokenEnum() == TK_begin)) {
    return {SpacingOptions::kMustAppend,
            "'else'-'begin' tokens should be together on one line."};
  }

  if ((left.TokenEnum() == ')') && (right.TokenEnum() == TK_begin)) {
    return {SpacingOptions::kMustAppend,
            "')'-'begin' tokens should be together on one line."};
  }

  if (left.TokenEnum() == verilog_tokentype::MacroCallCloseToEndLine) {
    if (!IsComment(FormatTokenType(right.format_token_enum)) &&
        !IsAnySemicolon(right) && !InRangeLikeContext(left_context)) {
      return {SpacingOptions::kMustWrap,
              "Macro-closing ')' should end its own line except for comments "
              "nad ';'."};
    }
  }

  if (left.TokenEnum() == PP_else || left.TokenEnum() == PP_endif) {
    if (IsComment(FormatTokenType(right.format_token_enum))) {
      return {SpacingOptions::kUndecided, "Comment may follow `else and `end"};
    }
    return {SpacingOptions::kMustWrap,
            "`end and `else should be on their own line except for comments."};
  }

  if (IsPreprocessorKeyword(
          static_cast<verilog_tokentype>(right.TokenEnum()))) {
    // The tree unwrapper should make sure these start their own partition.
    return {SpacingOptions::kMustWrap,
            "Preprocessor directives should start their own line."};
  }

  if (left.TokenEnum() == '#') {
    return {SpacingOptions::kMustAppend,
            "Never separate # from whatever follows (delay expressions)."};
  }
  if (left.TokenEnum() == verilog_tokentype::TK_TimeLiteral) {
    if (right.TokenEnum() == ';') {
      return {SpacingOptions::kMustAppend,
              "Keep delay statements together, like \"#1ps;\"."};
    }
  }

  if (left.TokenEnum() == ',' &&
      right.TokenEnum() == verilog_tokentype::MacroArg) {
    const absl::string_view text(right.Text());
    if (std::find(text.begin(), text.end(), '\n') != text.end()) {
      return {SpacingOptions::kMustWrap,
              "Multi-line unlexed macro arguments start on their own line."};
    }
  }

  // By default, leave undecided for penalty minimization.
  return {SpacingOptions::kUndecided,
          "Default: leave wrap decision to algorithm"};
}

// Extern linkage for sake of direct testing, though not exposed in public
// headers.
// TODO(fangism): could move this to a -internal.h header.
void AnnotateFormatToken(const FormatStyle &style,
                         const PreFormatToken &prev_token,
                         PreFormatToken *curr_token,
                         const SyntaxTreeContext &prev_context,
                         const SyntaxTreeContext &curr_context) {
  const auto p = SpacesRequiredBetween(style, prev_token, *curr_token,
                                       prev_context, curr_context);
  curr_token->before.spaces_required = p.spaces_required;
  if (p.force_preserve_spaces) {
    // forego all inter-token calculations
    curr_token->before.break_decision = SpacingOptions::kPreserve;
  } else {
    // Update the break penalty and if the curr_token is allowed to
    // break before it.
    const auto break_penalty = BreakPenaltyBetween(prev_token, *curr_token,
                                                   prev_context, curr_context);
    curr_token->before.break_penalty = break_penalty.value;
    const auto breaker = BreakDecisionBetween(style, prev_token, *curr_token,
                                              prev_context, curr_context);
    curr_token->before.break_decision = breaker.value;
    VLOG(3) << "line break constraint: " << breaker.value << ": "
            << breaker.reason;
  }
}

void AnnotateFormattingInformation(
    const FormatStyle &style, const verible::TextStructureView &text_structure,
    std::vector<verible::PreFormatToken> *format_tokens) {
  // This interface just forwards the relevant information from text_structure.
  AnnotateFormattingInformation(style, text_structure.Contents().begin(),
                                text_structure.SyntaxTree().get(),
                                text_structure.EOFToken(), format_tokens);
}

void AnnotateFormattingInformation(
    const FormatStyle &style, const char *buffer_start,
    const verible::Symbol *syntax_tree_root,
    const verible::TokenInfo &eof_token,
    std::vector<verible::PreFormatToken> *format_tokens) {
  if (format_tokens->empty()) {
    return;
  }

  if (buffer_start != nullptr) {
    // For unit testing, tokens' text snippets don't necessarily originate
    // from the same contiguous string buffer, so skip this step.
    ConnectPreFormatTokensPreservedSpaceStarts(buffer_start, format_tokens);
  }

  // Annotate inter-token information using the syntax tree for context.
  AnnotateFormatTokensUsingSyntaxContext(
      syntax_tree_root, eof_token, format_tokens->begin(), format_tokens->end(),
      // lambda: bind the FormatStyle, forwarding all other arguments
      [&style](const PreFormatToken &prev_token, PreFormatToken *curr_token,
               const SyntaxTreeContext &prev_context,
               const SyntaxTreeContext &current_context) {
        AnnotateFormatToken(style, prev_token, curr_token, prev_context,
                            current_context);
      });
}

}  // namespace formatter
}  // namespace verilog
