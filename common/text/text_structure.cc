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

// Implementation of TextStructure methods.

#include "common/text/text_structure.h"

#include <algorithm>
#include <cstddef>
#include <iterator>
#include <memory>
#include <sstream>  // IWYU pragma: keep  // for ostringstream
#include <string>
#include <utility>
#include <vector>

#include "absl/status/status.h"
#include "absl/strings/str_cat.h"
#include "absl/strings/str_split.h"
#include "absl/strings/string_view.h"
#include "common/strings/line_column_map.h"
#include "common/text/concrete_syntax_leaf.h"
#include "common/text/concrete_syntax_tree.h"
#include "common/text/symbol.h"
#include "common/text/token_info.h"
#include "common/text/token_stream_view.h"
#include "common/text/tree_utils.h"
#include "common/util/iterator_range.h"
#include "common/util/logging.h"
#include "common/util/range.h"
#include "common/util/status_macros.h"

namespace verible {

TextStructureView::TextStructureView(absl::string_view contents)
    : contents_(contents) {
  // more than sufficient memory as number-of-tokens <= bytes-in-file,
  // push_back() should never re-alloc because size <= initial capacity.
  tokens_.reserve(contents.length());
  const absl::Status status = InternalConsistencyCheck();
  CHECK(status.ok())
      << "Failed internal iterator/string_view consistency check in ctor:\n  "
      << status.message();
}

TextStructureView::~TextStructureView() {
  const absl::Status status = InternalConsistencyCheck();
  CHECK(status.ok())
      << "Failed internal iterator/string_view consistency check in dtor:\n  "
      << status.message();
}

void TextStructureView::Clear() {
  syntax_tree_ = nullptr;
  lazy_lines_info_.valid = false;
  line_token_map_.clear();
  tokens_view_.clear();
  tokens_.clear();
  contents_ = contents_.substr(0, 0);  // clear
}

static bool TokenLocationLess(const TokenInfo& token, const char* offset) {
  return token.text().begin() < offset;
}

// Makes an iterator-writable copy of items_view without using const_cast.
template <class V>
std::vector<typename V::iterator> _CopyWriteableIterators(
    V& items, const std::vector<typename V::const_iterator>& items_view) {
  // precondition: items_view's iterators all point into items array.
  // postcondition: results's iterators point to the same items as items_view.
  std::vector<typename V::iterator> result;
  result.reserve(items_view.size());
  typename V::iterator iter(items.begin());
  const typename V::const_iterator const_iter(items.begin());
  for (auto view_iter : items_view) {
    result.push_back(iter + std::distance(const_iter, view_iter));
  }
  return result;
}

TokenStreamReferenceView TextStructureView::MakeTokenStreamReferenceView() {
  return _CopyWriteableIterators(tokens_, tokens_view_);
}

void TextStructureView::CalculateFirstTokensPerLine() {
  line_token_map_.clear();
  auto token_iter = tokens_.cbegin();
  const auto& offset_map = GetLineColumnMap().GetBeginningOfLineOffsets();
  for (const auto offset : offset_map) {
    // TODO(fangism): linear search might be as competitive as binary search
    token_iter =
        std::lower_bound(token_iter, tokens_.cend(),
                         Contents().begin() + offset, &TokenLocationLess);
    line_token_map_.push_back(token_iter);
  }
  // Add an end() iterator so map has N+1 entries.
  // When there is no newline on the last line, this value will be different
  // from the previous iterator's index.
  line_token_map_.push_back(tokens_.cend());
}

TokenRange TextStructureView::TokenRangeSpanningOffsets(size_t lower,
                                                        size_t upper) const {
  const auto text_base = Contents().begin();
  // Range is found with two binary searches.
  // TODO(fangism): Is there a way to make this work with a single call
  // to std::equal_range()?
  const auto left = std::lower_bound(tokens_.cbegin(), tokens_.cend(),
                                     text_base + lower, &TokenLocationLess);
  const auto right = std::lower_bound(left, tokens_.cend(), text_base + upper,
                                      &TokenLocationLess);
  return make_range(left, right);
}

LineColumnRange TextStructureView::GetRangeForToken(
    const TokenInfo& token) const {
  if (token.isEOF()) {
    // In particular some unit tests pass in an artificial EOF token, not a
    // EOF token generated from this view. So handle this directly.
    const LineColumn eofPos = GetLineColAtOffset(Contents().length());
    return {eofPos, eofPos};
  }
  // TODO(hzeller): This should simply be GetRangeForText(token.text()),
  // but the more thorough error checking in GetRangeForText()
  // exposes a token overrun in verilog_analyzer_test.cc
  // Defer to fix in separate change.
  return {GetLineColAtOffset(token.left(Contents())),
          GetLineColAtOffset(token.right(Contents()))};
}

LineColumnRange TextStructureView::GetRangeForText(
    absl::string_view text) const {
  const int from = std::distance(Contents().begin(), text.begin());
  const int to = std::distance(Contents().begin(), text.end());
  CHECK_GE(from, 0) << '"' << text << '"';
  CHECK_LE(to, Contents().length()) << '"' << text << '"';
  return {GetLineColAtOffset(from), GetLineColAtOffset(to)};
}

TokenRange TextStructureView::TokenRangeOnLine(size_t lineno) const {
  if (lineno + 1 < line_token_map_.size()) {
    return make_range(line_token_map_[lineno], line_token_map_[lineno + 1]);
  } else {
    return make_range(tokens_.cend(), tokens_.cend());
  }
}

TokenInfo TextStructureView::FindTokenAt(const LineColumn& pos) const {
  if (pos.line < 0 || pos.column < 0) return EOFToken();
  // Maybe do binary search here if we have a huge amount tokens per line.
  for (const TokenInfo& token : TokenRangeOnLine(pos.line)) {
    if (GetRangeForToken(token).PositionInRange(pos)) return token;
  }
  return EOFToken();
}

TokenInfo TextStructureView::EOFToken() const {
  return TokenInfo::EOFToken(Contents());
}

// Removes tokens from the TokenStreamView that do not satisfy the keep
// predicate.
void TextStructureView::FilterTokens(const TokenFilterPredicate& keep) {
  FilterTokenStreamViewInPlace(keep, &tokens_view_);
}

static void TerminateTokenStream(TokenSequence* tokens) {
  if (tokens->empty()) return;
  if (tokens->back().isEOF()) return;
  // push_back might cause re-alloc.
  tokens->push_back(TokenInfo::EOFToken(tokens->back().text()));
}

void TextStructureView::FocusOnSubtreeSpanningSubstring(int left_offset,
                                                        int length) {
  VLOG(2) << __FUNCTION__ << " at " << left_offset << " +" << length;
  const int right_offset = left_offset + length;
  TrimSyntaxTree(left_offset, right_offset);
  TrimTokensToSubstring(left_offset, right_offset);
  TrimContents(left_offset, length);
  lazy_lines_info_.valid = false;
  CalculateFirstTokensPerLine();
  const absl::Status status = InternalConsistencyCheck();
  CHECK(status.ok())
      << "Failed internal iterator/string_view consistency check:\n  "
      << status.message();
  VLOG(2) << "end of " << __FUNCTION__;
}

// Replace the syntax_tree_ field with the largest subtree wholly contained
// inside the offset bounds.
// Discards nodes outside of the referenced subtree.
void TextStructureView::TrimSyntaxTree(int first_token_offset,
                                       int last_token_offset) {
  const absl::string_view text_range(Contents().substr(
      first_token_offset, last_token_offset - first_token_offset));
  verible::TrimSyntaxTree(&syntax_tree_, text_range);
}

// Reduces the set of tokens to that spanned by [left_offset, right_offset).
// The resulting token stream is terminated with an EOF token, whose range
// reflects the right_offset.
void TextStructureView::TrimTokensToSubstring(int left_offset,
                                              int right_offset) {
  VLOG(2) << __FUNCTION__ << " [" << left_offset << ',' << right_offset << ')';
  // Find first token that starts at or after the offset.  (binary_search)
  // Find first token that starts beyond the syntax tree.  (binary_search)
  const auto view_trim_range =
      TokenRangeSpanningOffsets(left_offset, right_offset);
  CHECK(tokens_.begin() <= view_trim_range.begin());
  CHECK(view_trim_range.begin() <= view_trim_range.end());
  CHECK(view_trim_range.end() <= tokens_.end());

  // Find the view iterators that fall within this range.
  const auto iter_trim_begin = std::lower_bound(
      tokens_view_.begin(), tokens_view_.end(), view_trim_range.begin());
  const auto iter_trim_end = std::lower_bound(
      iter_trim_begin, tokens_view_.end(), view_trim_range.end());

  // Copy subset of tokens to new token sequence.
  TokenSequence trimmed_stream(view_trim_range.begin(), view_trim_range.end());

  // If the last token straddles the end-of-range, (possibly due to lexical
  // error), then trim its tail, bounded by right_offset.
  if (!trimmed_stream.empty()) {
    const absl::string_view substr(
        contents_.substr(left_offset, right_offset - left_offset));
    TokenInfo& last(trimmed_stream.back());
    const int overhang = std::distance(substr.end(), last.text().end());
    if (!IsSubRange(last.text(), substr)) {
      VLOG(2) << "last token overhangs end by " << overhang << ": " << last;
      absl::string_view trimmed_tail_token(last.text());
      trimmed_tail_token.remove_suffix(overhang);
      last.set_text(trimmed_tail_token);
      // TODO(fangism): Should the token enum be set to some error value,
      // if it is not already an error value?
    }
  }

  TerminateTokenStream(&trimmed_stream);  // Append EOF token.

  // Recalculate iterators for new token stream view, pointing into new
  // token sequence.
  const int index_difference =
      std::distance(tokens_.cbegin(), view_trim_range.begin());
  TokenStreamView trimmed_view;
  trimmed_view.reserve(std::distance(iter_trim_begin, iter_trim_end));
  for (auto token_iterator : make_range(iter_trim_begin, iter_trim_end)) {
    const int old_index = std::distance(tokens_.cbegin(), token_iterator);
    const int new_index = old_index - index_difference;
    trimmed_view.push_back(trimmed_stream.begin() + new_index);
  }

  // Swap new-old arrays, which will cause old arrays to be deleted.
  tokens_view_.swap(trimmed_view);
  tokens_.swap(trimmed_stream);
}

void TextStructureView::TrimContents(int left_offset, int length) {
  contents_ = contents_.substr(left_offset, length);
}

const TextStructureView::LinesInfo& TextStructureView::LinesInfo::Get(
    absl::string_view contents) {
  if (valid) return *this;

  lines = absl::StrSplit(contents, '\n');
  line_column_map.reset(new LineColumnMap(contents));
  valid = true;

  return *this;
}

void TextStructureView::RebaseTokensToSuperstring(absl::string_view superstring,
                                                  absl::string_view src_base,
                                                  int offset) {
  MutateTokens([&](TokenInfo* token) {
    const int delta = token->left(src_base);
    // Superstring must point to separate memory space.
    token->RebaseStringView(superstring.begin() + offset + delta);
  });
  // Assigning superstring for the sake of maintaining range invariants.
  contents_ = superstring;
  lazy_lines_info_.valid = false;
}

void TextStructureView::MutateTokens(const LeafMutator& mutator) {
  for (auto& token : tokens_) {
    mutator(&token);
  }
  // No need to touch tokens_view_, all transformations are in-place.

  if (syntax_tree_ != nullptr) {
    // The tokens at the leaves of the tree are their own copies, and thus
    // need to re-apply the same transformation.
    MutateLeaves(&syntax_tree_, mutator);
  }
}

// Find the last non-EOF token.  Usually searches at most 2 tokens.
static const TokenInfo* FindLastNonEOFToken(const TokenSequence& tokens) {
  const auto iter =
      std::find_if(tokens.rbegin(), tokens.rend(),
                   [](const TokenInfo& token) { return !token.isEOF(); });
  return iter != tokens.rend() ? &*iter : nullptr;
}

absl::Status TextStructureView::FastTokenRangeConsistencyCheck() const {
  VLOG(2) << __FUNCTION__;
  // Check the ranges of the first and last element of the critical arrays.
  // A more thorough full-check would scan every single token.
  const auto lower_bound = contents_.begin();
  const auto upper_bound = contents_.end();
  if (!tokens_.empty()) {
    // Check that extremities of first and last token lie inside contents_.
    const TokenInfo& first = tokens_.front();
    if (!first.isEOF() && lower_bound > first.text().cbegin()) {
      return absl::InternalError(absl::StrCat(
          "Token offset points before beginning of string contents.  delta=",
          std::distance(first.text().cbegin(), lower_bound)));
    }
    const TokenInfo* last = FindLastNonEOFToken(tokens_);
    if (last != nullptr && last->text().cend() > upper_bound) {
      return absl::InternalError(absl::StrCat(
          "Token offset points past end of string contents.  delta=",
          std::distance(upper_bound, last->text().cend())));
    }
    if (!tokens_view_.empty()) {
      // Check that TokenSequence iterators point into tokens_.
      if (tokens_.begin() > tokens_view_.front()) {
        return absl::InternalError(
            "First token iterator points before beginning of array.");
      }
      if (tokens_view_.front() >= tokens_.end()) {
        return absl::InternalError(
            "First token iterator points past end of array.");
      }
      if (tokens_.begin() > tokens_view_.back()) {
        return absl::InternalError(
            "Last token iterator points before beginning of array.");
      }
      if (tokens_view_.back() >= tokens_.end()) {
        return absl::InternalError(
            "Last token iterator points past end of array.");
      }
    }
    if (!line_token_map_.empty()) {
      if (line_token_map_.front() != tokens_.begin()) {
        return absl::InternalError(
            "Per-line token iterator map does not start with the beginning of "
            "the token sequence.");
      }
      if (line_token_map_.back() != tokens_.end()) {
        return absl::InternalError(
            "Per-line token iterator map does not end with to the end of the "
            "token sequence.");
      }
    }
  }
  return absl::OkStatus();
}

absl::Status TextStructureView::FastLineRangeConsistencyCheck() const {
  VLOG(2) << __FUNCTION__;
  const auto& lines = Lines();
  if (!lines.empty()) {
    if (lines.front().cbegin() != contents_.cbegin()) {
      return absl::InternalError(
          "First line does not match beginning of text.");
    }
    if (lines.back().cend() != contents_.cend()) {
      return absl::InternalError("Last line does not match end of text.");
    }
  }
  return absl::OkStatus();
}

absl::Status TextStructureView::SyntaxTreeConsistencyCheck() const {
  VLOG(2) << __FUNCTION__;
  // Check that first and last token in syntax_tree_ point to text
  // inside contents_.
  const char* const lower_bound = contents_.data();
  const char* const upper_bound = lower_bound + contents_.length();
  if (syntax_tree_ != nullptr) {
    const SyntaxTreeLeaf* left = GetLeftmostLeaf(*syntax_tree_);
    if (!left) return absl::OkStatus();
    const SyntaxTreeLeaf* right = GetRightmostLeaf(*syntax_tree_);
    if (lower_bound > left->get().text().cbegin()) {
      return absl::InternalError(
          "Left-most tree leaf points before beginning of contents.");
    }
    if (right->get().text().cend() > upper_bound) {
      return absl::InternalError(
          "Right-most tree leaf points past end of contents.");
    }
  }
  return absl::OkStatus();
}

absl::Status TextStructureView::InternalConsistencyCheck() const {
  RETURN_IF_ERROR(FastLineRangeConsistencyCheck());
  RETURN_IF_ERROR(FastTokenRangeConsistencyCheck());
  return SyntaxTreeConsistencyCheck();
}

// TokenRange can be a container reference or iterator range.
// TokenViewRange can be a container reference or iterator range.
template <typename TokenRange, typename TokenViewRange>
static void CopyTokensAndView(TokenSequence* destination,
                              std::vector<int>* view_indices,
                              const TokenRange& token_source,
                              const TokenViewRange& view_source) {
  // Translate token_view's iterators into array indices, adjusting for the
  // number of pre-existing tokens.
  for (const auto& token_iter : view_source) {
    view_indices->push_back(destination->size() +
                            std::distance(token_source.begin(), token_iter));
  }
  // Copy tokens up to this expansion point.
  for (const auto& token : token_source) {
    destination->push_back(token);
  }
}

// Incrementally copies a slice of tokens and expands a single subtree.
// This advances the next_token_iter and next_token_view_iter iterators.
// The subtree from the expansion is transferred into this objects's syntax
// tree.  Indices into the final token stream view are collected in
// token_view_indices.  Offset is the location of each expansion point.
void TextStructureView::ConsumeDeferredExpansion(
    TokenSequence::const_iterator* next_token_iter,
    TokenStreamView::const_iterator* next_token_view_iter,
    DeferredExpansion* expansion, TokenSequence* combined_tokens,
    std::vector<int>* token_view_indices, const char* offset) {
  auto token_iter = *next_token_iter;
  auto token_view_iter = *next_token_view_iter;
  // Find the position up to each expansion point.
  *next_token_iter =
      std::lower_bound(token_iter, tokens_.cend(), offset,
                       [](const TokenInfo& token, const char* target) {
                         return std::distance(target, token.text().begin()) < 0;
                       });
  CHECK(*next_token_iter != tokens_.cend());
  *next_token_view_iter = std::lower_bound(
      token_view_iter, tokens_view_.cend(), offset,
      [](TokenStreamView::const_reference token_ref, const char* target) {
        return std::distance(target, token_ref->text().begin()) < 0;
      });
  CHECK(*next_token_view_iter != tokens_view_.cend());

  // Copy tokens and partial view into output.
  CopyTokensAndView(combined_tokens, token_view_indices,
                    make_range(token_iter, *next_token_iter),
                    make_range(token_view_iter, *next_token_view_iter));

  // Adjust locations of tokens in the expanded tree by pointing them
  // into the original text (contents_).
  std::unique_ptr<TextStructure>& subanalysis = expansion->subanalysis;
  TextStructureView& sub_data = ABSL_DIE_IF_NULL(subanalysis)->MutableData();
  const absl::string_view sub_data_text(sub_data.Contents());
  CHECK(!IsSubRange(sub_data_text, contents_));
  CHECK_EQ(sub_data_text, absl::string_view(offset, sub_data_text.length()));
  CHECK_GE(offset, contents_.begin());
  sub_data.RebaseTokensToSuperstring(contents_, sub_data_text,
                                     std::distance(contents_.begin(), offset));

  // Translate token_view's iterators into array indices.
  if (!sub_data.tokens_.empty() && sub_data.tokens_.back().isEOF()) {
    // Remove auxiliary data's end-token sentinel before copying.
    // Don't want to splice it into result.
    sub_data.tokens_.pop_back();
  }
  CopyTokensAndView(combined_tokens, token_view_indices, sub_data.tokens_,
                    sub_data.tokens_view_);

  // Transfer ownership of transformed syntax tree to this object's tree.
  *expansion->expansion_point = std::move(sub_data.MutableSyntaxTree());
  subanalysis->MutableData().Clear();

  // Advance one past expansion point to skip over expanded token.
  ++*next_token_iter;
  ++*next_token_view_iter;
}

TextStructure::TextStructure(absl::string_view contents)
    : owned_contents_(contents), data_(owned_contents_) {
  // Internal string_view must point to memory owned by owned_contents_.
  const absl::Status status = InternalConsistencyCheck();
  CHECK(status.ok()) << status.message() << " (in ctor)";
}

TextStructure::~TextStructure() {
  const absl::Status status = StringViewConsistencyCheck();
  CHECK(status.ok()) << status.message() << " (in dtor)";
}

void TextStructureView::ExpandSubtrees(NodeExpansionMap* expansions) {
  TokenSequence combined_tokens;
  // Gather indices and reconstruct iterators after there are no more
  // reallocations due to growing combined_tokens.
  std::vector<int> combined_token_view_indices;
  auto token_iter = tokens_.cbegin();
  auto token_view_iter = tokens_view_.cbegin();
  for (auto& expansion_entry : *expansions) {
    const auto offset = Contents().begin() + expansion_entry.first;
    ConsumeDeferredExpansion(&token_iter, &token_view_iter,
                             &expansion_entry.second, &combined_tokens,
                             &combined_token_view_indices, offset);
  }

  // Copy the remaining tokens beyond the last expansion point.
  CopyTokensAndView(&combined_tokens, &combined_token_view_indices,
                    make_range(token_iter, tokens_.cend()),
                    make_range(token_view_iter, tokens_view_.cend()));

  // Commit the newly expanded sequence of tokens.
  tokens_.swap(combined_tokens);

  // Reconstruct view iterators from indices into the new sequence.
  tokens_view_.clear();
  tokens_view_.reserve(combined_token_view_indices.size());
  for (const auto index : combined_token_view_indices) {
    tokens_view_.push_back(tokens_.cbegin() + index);
  }

  // Recalculate line-by-line token ranges.
  // TODO(fangism): Should be possible to update line_token_map_ incrementally
  // as well.
  CalculateFirstTokensPerLine();
}

absl::Status TextStructure::StringViewConsistencyCheck() const {
  const absl::string_view contents = data_.Contents();
  if (!contents.empty() &&
      !IsSubRange(contents, absl::string_view(owned_contents_))) {
    return absl::InternalError(
        "string_view contents_ is not a substring of owned_contents_, "
        "contents_ might reference deallocated memory!");
  }
  return absl::OkStatus();
}

absl::Status TextStructure::InternalConsistencyCheck() const {
  RETURN_IF_ERROR(StringViewConsistencyCheck());
  return data_.InternalConsistencyCheck();
}

}  // namespace verible
