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

#ifndef VERIBLE_COMMON_FORMATTING_UNWRAPPED_LINE_H_
#define VERIBLE_COMMON_FORMATTING_UNWRAPPED_LINE_H_

#include <iosfwd>
#include <string>
#include <vector>

#include "common/formatting/format_token.h"
#include "common/text/symbol.h"

namespace verible {

// Enumeration of partitioning choices at each node in the UnwrappedLine
// token range partition tree.
// TODO(fangism): It is forseeable that each language's formatter may have a
// different set of policies, this this might eventually have to move into
// language-specific implementation code.
enum class PartitionPolicyEnum {
  // Denotes that no partition policy has been set.
  kUninitialized,

  // This partition exists solely just for grouping purposes.
  // Always view subpartitions of node tagged with this, rather than whole range
  // spanned by the subpartitions.
  kAlwaysExpand,

  // Collapse into one line if it doesn't exceed column limit.
  kFitOnLineElseExpand,

  // There's no kNeverExpand, because one would just not create the partition in
  // the first place.

  // This is where future formatting configuration policies could go:
  // e.g. kOneItemPerLine, kCompactItems

  // With this policy, coordinate the spacing of subpartitions like auto-sized
  // columns that use space-padding to achieve vertical alignment.
  kTabularAlignment,

  // Signal that this unwrapped line (a direct child of a partition marked with
  // kTabularAlignment) has been successfully aligned with spacing padded.
  // In this case, do NOT bother to call SearchLineWraps or perform any other
  // spacing/wrapping optimization.
  // Reserved for setting only from align.cc.
  kSuccessfullyAligned,

  // Treats subpartitions as units, and appends them to the same line as
  // long as they fit, else wrap them aligned to the position of the first
  // element It uses first subpartition length to compute indentation spaces or
  // FormatStyle.wrap_spaces when wrapping.
  kAppendFittingSubPartitions,
};

std::ostream& operator<<(std::ostream&, PartitionPolicyEnum);

// An UnwrappedLine represents a partition of the input token stream that
// is an independent unit of work for other phases of formatting, such as
// line wrap optimization. It consists of a lightweight const_iterator range
// that can be easily grown without any copy-overhead.
class UnwrappedLine {
  typedef FormatTokenRange::const_iterator token_iterator;

 public:
  enum {
    kIndentationMarker = '>'  // for readable debug printing
  };

  // Parameter d is the indentation level, and iterator b points to the first
  // PreFormatToken spanned by this range, which is initially empty.
  UnwrappedLine(int d, token_iterator b,
                PartitionPolicyEnum p = PartitionPolicyEnum::kUninitialized)
      : indentation_spaces_(d), tokens_(b, b), partition_policy_(p) {}

  // Allow default construction for use in resize-able containers.
  UnwrappedLine() = default;
  UnwrappedLine(const UnwrappedLine&) = default;
  UnwrappedLine(UnwrappedLine&&) = default;
  UnwrappedLine& operator=(const UnwrappedLine&) = default;
  UnwrappedLine& operator=(UnwrappedLine&&) = default;

  // Extends PreFormatToken range spanned by this UnwrappedLine by one token at
  // the back.
  void SpanNextToken() { tokens_.extend_back(); }

  // Extends PreFormatToken range spanned by this UnwrappedLine by one token at
  // the front.
  void SpanPrevToken() { tokens_.extend_front(); }

  // Extends PreFormatToken range's lower bound to the given token (inclusive).
  void SpanBackToToken(token_iterator iter) { tokens_.set_begin(iter); }

  // Extends PreFormatToken range's upper bound up to the given token
  // (exclusive).
  void SpanUpToToken(token_iterator iter) { tokens_.set_end(iter); }

  int IndentationSpaces() const { return indentation_spaces_; }

  void SetIndentationSpaces(int spaces);

  PartitionPolicyEnum PartitionPolicy() const { return partition_policy_; }

  void SetPartitionPolicy(PartitionPolicyEnum policy) {
    partition_policy_ = policy;
  }

  const Symbol* Origin() const { return origin_; }
  void SetOrigin(const Symbol* origin) { origin_ = origin; }

  // Returns the range of PreFormatTokens spanned by this UnwrappedLine.
  // Note that this is a *copy*, and not a reference to the underlying range.
  FormatTokenRange TokensRange() const { return tokens_; }

  // Returns the number of tokens in this UnwrappedLine
  size_t Size() const { return tokens_.size(); }

  // Returns true if the UnwrappedLine is empty, if it has no tokens
  bool IsEmpty() const { return tokens_.empty(); }

  // Currently for debugging, prints the UnwrappedLine as Code
  std::ostream* AsCode(std::ostream*, bool verbose = false) const;

 private:
  // Data members:

  // indentation_spaces_ is the number of spaces to indent from the left.
  int indentation_spaces_;

  // The range of sequential PreFormatTokens spanned by this UnwrappedLine.
  // These represent the tokens that will be formatted independently.
  // The memory for these must be owned elsewhere.
  FormatTokenRange tokens_;

  // This determines under what conditions this UnwrappedLine should be
  // further partitioned for formatting.
  PartitionPolicyEnum partition_policy_ = PartitionPolicyEnum::kUninitialized;

  // Hint about the origin of this partition, e.g. a particular syntax
  // tree node/leaf.
  const Symbol* origin_ = nullptr;
};

std::ostream& operator<<(std::ostream&, const UnwrappedLine&);

// FormattedExcerpt is the result of formatting a slice of code represented
// as an UnwrappedLine.  In this representation, wrapping and spacing decisions
// are considered bound.
// TODO(fangism): move this class to its own file.
class FormattedExcerpt {
 public:
  FormattedExcerpt() = default;

  explicit FormattedExcerpt(const UnwrappedLine&);

  // Returns the number of spaces to indent.
  int IndentationSpaces() const { return indentation_spaces_; }

  const std::vector<FormattedToken>& Tokens() const { return tokens_; }

  // Note: The mutable variant is only intended for use in StateNode.
  std::vector<FormattedToken>& MutableTokens() { return tokens_; }

  // Prints formatted text.  If indent is true, include the spacing
  // that is to the left of the first token.
  std::ostream& FormattedText(std::ostream&, bool indent) const;

  // Returns formatted code as a string.
  std::string Render() const;

  // Signal to the consumer that the analysis use to construct this
  // formatted excerpt did not run to completion, and that the result
  // may be unoptimal.
  void MarkIncomplete() { completed_formatting_ = false; }

  // Returns true if this result represents optimal formatting.
  bool CompletedFormatting() const { return completed_formatting_; }

 private:
  // Number of spaces to indent this line from the left.
  int indentation_spaces_ = 0;

  // Sequence of formatted tokens.
  std::vector<FormattedToken> tokens_;

  // If true, this result can be interpreted as formatting-optimal.
  // if false, this is the result of incomplete optimization.
  bool completed_formatting_ = true;
};

std::ostream& operator<<(std::ostream&, const FormattedExcerpt&);

}  // namespace verible

#endif  // VERIBLE_COMMON_FORMATTING_UNWRAPPED_LINE_H_
