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

#ifndef VERIBLE_COMMON_FORMATTING_FORMAT_TOKEN_H_
#define VERIBLE_COMMON_FORMATTING_FORMAT_TOKEN_H_

#include <cstddef>
#include <iosfwd>
#include <string>
#include <vector>

#include "absl/strings/string_view.h"
#include "verible/common/strings/position.h"
#include "verible/common/text/concrete-syntax-leaf.h"
#include "verible/common/text/token-info.h"
#include "verible/common/util/container-iterator-range.h"

namespace verible {

// Enumeration for options for formatting spaces between tokens.
// This controls what to explore (if not pre-determined).
// Related enum: SpacingDecision
enum class SpacingOptions {
  kUndecided,      // unconstrained, not yet decided, to be optimized (default)
  kMustAppend,     // cannot break here
  kMustWrap,       // must break here
  kAppendAligned,  // when appending, allow for left-padding spaces
  kPreserve,       // do not optimize, use original spacing
};

std::ostream &operator<<(std::ostream &, SpacingOptions);

// Tri-state value that encodes how this token affects group balancing
// for line-wrapping purposes.
enum class GroupBalancing {
  kNone,  // This token does not involve any grouping.
  kOpen,  // This token marks the beginning of a balanced group.
  kClose  // This token marks the closing of a balanced group.
          // TODO(fangism): Reset?  (separator)
};

std::ostream &operator<<(std::ostream &, GroupBalancing);

// InterTokenInfo defines parameters that are important to formatting
// decisions related to adjacent tokens.
// This is used during wrapping exploration and optimization.
// See also InterTokenDecision for decision-bound information.
struct InterTokenInfo {
  // The number of spaces that should be inserted before this token.
  // This can be nonzero when a style guide dictates a mimimum spacing
  // between certain tokens.  This can be used to indent the first token
  // on a formatted line.
  // This *must* be nonzero when removing a space between tokens would
  // result in changing the lexical stream (incorrect).
  int spaces_required = 0;

  // The penalty for line-breaking before this token.
  // This value is used during optimization.
  int break_penalty = 0;

  // Encodes spacing exploration options.
  SpacingOptions break_decision = SpacingOptions::kUndecided;

  // Point to previous position in string buffer before series of white space
  // tokens, for the sake of preserving space.
  // Together with the current token, they can form a string_view representing
  // pre-existing space from the original buffer.
  const char *preserved_space_start = nullptr;

  InterTokenInfo() = default;
  InterTokenInfo(const InterTokenInfo &) = default;

  // Comparison is only really used for testing.
  bool operator==(const InterTokenInfo &r) const {
    return spaces_required == r.spaces_required &&
           break_penalty == r.break_penalty &&
           break_decision == r.break_decision &&
           preserved_space_start == r.preserved_space_start;
  }

  bool operator!=(const InterTokenInfo &r) const { return !((*this) == r); }

  // For debug printing.
  std::ostream &CompactNotation(std::ostream &) const;
};

// Human-readable form, for debugging.
std::ostream &operator<<(std::ostream &, const InterTokenInfo &);

// PreFormatToken is a wrapper for TokenInfo objects. It contains an original
// pointer to a TokenInfo object, as well as additional information for
// formatting purposes.  It is first used to markup an UnwrappedLine with
// inter-token annotations.
struct PreFormatToken {
  // The token this PreFormatToken holds. TokenInfo must outlive this object.
  const TokenInfo *token = nullptr;

  // The enum for this PreFormatToken, an abstraction from the TokenInfo enum
  // for decision making. The values are intended to come from language-specific
  // enumerations.
  int format_token_enum = -1;

  // Formatting parameters that apply between the previous token and this one.
  // This is used in annotating tokens before spacing/wrapping optimization.
  InterTokenInfo before;

  // This marks how this token is involved in group balancing for line-wrapping.
  GroupBalancing balancing = GroupBalancing::kNone;

  PreFormatToken() = default;

  explicit PreFormatToken(const TokenInfo *t) : token(t) {}

  explicit PreFormatToken(const verible::SyntaxTreeLeaf &leaf)
      : PreFormatToken(&leaf.get()) {}

  // The length of characters in the PreFormatToken
  int Length() const { return token->text().length(); }

  // Returns the text of the TokenInfo token held by this PreFormatToken
  absl::string_view Text() const { return token->text(); }

  // Returns the enum of the TokenInfo token held by this PreFormatToken
  int TokenEnum() const { return token->token_enum(); }

  // Reconstructs the original spacing that preceded this token.
  absl::string_view OriginalLeadingSpaces() const;

  // Returns OriginalLeadingSpaces().length() - before.spaces_required.
  // If there is no leading spaces text, return 0.
  // If the original leading text contains any newlines, return 0.
  int ExcessSpaces() const;

  // Returns the number of leading spaces that this format token would occupy
  // when rendered, based on the formatting decision and before.required_spaces.
  size_t LeadingSpacesLength() const;

  // Returns a human-readable string representation of the FormatToken.
  // This is only intended for debugging.
  std::string ToString() const;
};

std::ostream &operator<<(std::ostream &stream, const PreFormatToken &token);

// Sets pointers that establish substring ranges of (whitespace) text *between*
// non-whitespace tokens.  This allows for reconstruction and analysis of
// inter-token (space) text.
// Note that this does not cover the space between the last token and EOF.
void ConnectPreFormatTokensPreservedSpaceStarts(
    const char *buffer_start,
    std::vector<verible::PreFormatToken> *format_tokens);

// Marks formatting-disabled ranges of tokens so that their original spacing is
// preserved.  'ftokens' is the array of PreFormatTokens to potentially mark.
// 'disabled_byte_ranges' is a set of formatting-disabled intervals.
// 'base_text' is the string_view of the whole text being formatted, and serves
// as the base reference for 'disabled_byte_ranges' offsets.
void PreserveSpacesOnDisabledTokenRanges(
    std::vector<PreFormatToken> *ftokens,
    const ByteOffsetSet &disabled_byte_ranges, absl::string_view base_text);

using FormatTokenRange =
    container_iterator_range<std::vector<PreFormatToken>::const_iterator>;
using MutableFormatTokenRange =
    container_iterator_range<std::vector<PreFormatToken>::iterator>;

// Enumeration for the final decision about spacing between tokens.
// Related enum: SpacingConstraint.
// These values are also used during line wrap searching and optimization.
// Notably and intentionally, there is no undecided or default.
enum class SpacingDecision {
  kPreserve,  // keep original inter-token spacing
  kAppend,    // add onto current line, with appropriate amount of spacing
  kWrap,      // wrap onto new line, with appropriate amount of indentation
  kAlign,     // like Append, but force left-padding of spaces, even at the
              // front of line.
};

std::ostream &operator<<(std::ostream &stream, SpacingDecision);

// Set of bound parameters for formatting around this token.
// The fields here are related to InterTokenInfo.
struct InterTokenDecision {
  // Number of spaces to insert, used when SpacingDecision is Append.
  int spaces = 0;

  // Choice of space formatting before this token.
  SpacingDecision action = SpacingDecision::kPreserve;

  // When preserving spaces before this token, start from this offset.
  const char *preserved_space_start = nullptr;

  InterTokenDecision() = default;

  explicit InterTokenDecision(const InterTokenInfo &);
};

// FormattedToken represents re-formatted text, whose spacing/line-break
// decisions have been bound.  The information in this struct can be derived
// entirely from a PreFormatToken.
struct FormattedToken {
  FormattedToken() = default;

  // Don't care what spacing decision is at this time, it will be populated
  // when reconstructing formatting decisions from StateNode.
  explicit FormattedToken(const PreFormatToken &ftoken)
      : token(ftoken.token), before(ftoken.before) {}

  // Reconstructs the original spacing that preceded this token.
  absl::string_view OriginalLeadingSpaces() const;

  // Print out formatted result after formatting decision optimization.
  std::ostream &FormattedText(std::ostream &) const;

  // The token this PreFormatToken holds. TokenInfo must outlive this object.
  const TokenInfo *token = nullptr;

  // Decision about what spaces to apply before printing this token.
  InterTokenDecision before;
};

std::ostream &operator<<(std::ostream &stream, const FormattedToken &token);

}  // namespace verible

#endif  // VERIBLE_COMMON_FORMATTING_FORMAT_TOKEN_H_
