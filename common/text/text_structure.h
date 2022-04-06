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

// TextStructure is a class responsible for managing the structural
// information of a block of text: tokenized view, syntax tree.
// It retains a shared pointer to the backing memory referenced by its
// string_views, and is suitable for breaking out substring analyses.
//
// See test_structure_test_utils.h for utilities for constructing fake
// (valid) TextStructures without a lexer or parser.
//
// TODO(fangism): object serialization/deserialization for TextStructure or
// TextStructureView.  Could also be related to protocol-buffer-ization.

#ifndef VERIBLE_COMMON_TEXT_TEXT_STRUCTURE_H_
#define VERIBLE_COMMON_TEXT_TEXT_STRUCTURE_H_

#include <cstddef>
#include <map>
#include <memory>
#include <string>
#include <vector>

#include "absl/status/status.h"
#include "absl/strings/string_view.h"
#include "common/strings/line_column_map.h"
#include "common/text/concrete_syntax_tree.h"
#include "common/text/symbol.h"
#include "common/text/token_stream_view.h"
#include "common/text/tree_utils.h"

namespace verible {

class TextStructure;

// TextStructureView contains sequences of tokens and a tree, but all
// string_views in this structure rely on string memory owned elsewhere,
// for example in TextStructure.
class TextStructureView {
 public:
  // Deferred in-place expansion of the syntax tree.
  // TODO(b/136014603): Replace with expandable token stream view abstraction.
  struct DeferredExpansion {
    // Position in the syntax tree to expand (leaf or node).
    std::unique_ptr<Symbol>* expansion_point;

    // Analysis of the substring that corresponds to the expansion_point.
    std::unique_ptr<TextStructure> subanalysis;
  };

  // NodeExpansionMap is a map of offsets to substring analysis results
  // that are to be expanded.  The rationale is that it is more efficient to
  // collect expansions and process them in bulk rather than as each
  // expansion is encountered.
  using NodeExpansionMap = std::map<int, DeferredExpansion>;

  explicit TextStructureView(absl::string_view contents);

  ~TextStructureView();

  // Do not copy/assign.  This contains pointers/iterators to internals.
  TextStructureView(const TextStructureView&) = delete;
  TextStructureView& operator=(const TextStructureView&) = delete;

  absl::string_view Contents() const { return contents_; }

  const std::vector<absl::string_view>& Lines() const {
    return lazy_lines_info_.Get(contents_).lines;
  }

  const ConcreteSyntaxTree& SyntaxTree() const { return syntax_tree_; }

  ConcreteSyntaxTree& MutableSyntaxTree() { return syntax_tree_; }

  const TokenSequence& TokenStream() const { return tokens_; }

  TokenSequence& MutableTokenStream() { return tokens_; }

  const TokenStreamView& GetTokenStreamView() const { return tokens_view_; }

  TokenStreamView& MutableTokenStreamView() { return tokens_view_; }

  // Creates a stream of modifiable iterators to the filtered tokens.
  // Uses tokens_view_ to create the iterators.
  TokenStreamReferenceView MakeTokenStreamReferenceView();

  const LineColumnMap& GetLineColumnMap() const {
    return *lazy_lines_info_.Get(contents_).line_column_map;
  }

  // Given a byte offset, return the line/column
  LineColumn GetLineColAtOffset(int bytes_offset) const {
    return GetLineColumnMap().GetLineColAtOffset(contents_, bytes_offset);
  }

  // Convenience function: Given the token, return the range it covers.
  LineColumnRange GetRangeForToken(const TokenInfo& token) const;

  // Convenience function: Given a text snippet, that needs to be a substring
  // of Contents(), return the range it covers.
  LineColumnRange GetRangeForText(absl::string_view text) const;

  const std::vector<TokenSequence::const_iterator>& GetLineTokenMap() const {
    return line_token_map_;
  }

  // Given line/column, find token that is available there. If this is out of
  // range, returns EOF.
  TokenInfo FindTokenAt(const LineColumn& pos) const;

  // Create the EOF token given the contents.
  TokenInfo EOFToken() const;

  // Compute the token stream iterators that start each line.
  // This populates the line_token_map_ array with token iterators.
  void CalculateFirstTokensPerLine();

  // Returns iterator range of tokens that span the given file offsets.
  // The second iterator points 1-past-the-end of the range.
  TokenRange TokenRangeSpanningOffsets(size_t lower, size_t upper) const;

  // Returns an iterator range of tokens that start on the given line number.
  // The lineno index is 0-based.  The last token spanned by the returned
  // range is the newline token that terminates the line.
  // Precondition: CalculateFirstTokensPerLine() has already been called.
  TokenRange TokenRangeOnLine(size_t lineno) const;

  // Filter out tokens from token stream view before parsing.
  // Can be called successively with different predicates.
  void FilterTokens(const TokenFilterPredicate&);

  // Apply the same transformation to the token sequence, and the tokens
  // that were copied into the syntax tree.
  void MutateTokens(const LeafMutator& mutator);

  // Update tokens to point their text into new (superstring) owner.
  // This is done to prepare for transfer of ownership of syntax_tree_
  // to a new owner.
  void RebaseTokensToSuperstring(absl::string_view superstring,
                                 absl::string_view src_base, int offset);

  // Narrows the view of text, tokens, and syntax tree to the node that starts
  // at left_offset.  The resulting state looks as if only a snippet of
  // text were parsed as a particular construct of the larger grammar.
  // The contents will be pared down to a substring, and irrelevant tokens will
  // be pruned from the token sequence and syntax tree.
  void FocusOnSubtreeSpanningSubstring(int left_offset, int length);

  // ExpandSubtrees performs bulk substitution of syntax tree leaves to
  // subtrees that result from other analyses.  Memory ownership of the
  // analysis results passed through the expansions is transferred (consumed)
  // by this function.
  void ExpandSubtrees(NodeExpansionMap* expansions);

  // All of this class's consistency checks combined.
  absl::Status InternalConsistencyCheck() const;

 protected:
  // This is the text that is spanned by the token sequence and syntax tree.
  // This is required for calculating byte offsets to substrings contained
  // within this structure.  Pass this (via Contents()) to TokenInfo::left() and
  // TokenInfo::right() to calculate byte offsets, useful for diagnostics.
  absl::string_view contents_;

  struct LinesInfo {
    bool valid = false;

    // Line-by-line view of contents_.
    std::vector<absl::string_view> lines;

    // Map to translate byte-offsets to line and column for diagnostics.
    std::unique_ptr<LineColumnMap> line_column_map;

    const LinesInfo& Get(absl::string_view contents);
  };
  // Mutable as we fill it lazily on request; conceptually the data is const.
  mutable LinesInfo lazy_lines_info_;

  // Tokens that constitute the original file (contents_).
  // This should always be terminated with a sentinel EOF token.
  TokenSequence tokens_;

  // Possibly modified view of the tokens_ token sequence.
  TokenStreamView tokens_view_;

  // Index of token iterators that mark the beginnings of each line.
  std::vector<TokenSequence::const_iterator> line_token_map_;

  // Tree representation of file contents.
  ConcreteSyntaxTree syntax_tree_;

  void TrimSyntaxTree(int first_token_offset, int last_token_offset);

  void TrimTokensToSubstring(int left_offset, int right_offset);

  void TrimContents(int left_offset, int length);

  void ConsumeDeferredExpansion(
      TokenSequence::const_iterator* next_token_iter,
      TokenStreamView::const_iterator* next_token_view_iter,
      DeferredExpansion* expansion, TokenSequence* combined_tokens,
      std::vector<int>* token_view_indices, const char* offset);

  // Resets all fields. Only needed in tests.
  void Clear();

  // Verify that internal iterators point to locations owned by this object,
  // and that all string_views in the tokens_view_ are substring views of the
  // contents_ string view.
  absl::Status FastTokenRangeConsistencyCheck() const;

  // Verify that line-based view of contents_ is consistent with the
  // contents_ text itself.
  absl::Status FastLineRangeConsistencyCheck() const;

  // Verify that the string views in the syntax tree are contained within
  // the contents_ string view.
  absl::Status SyntaxTreeConsistencyCheck() const;
};

// TextStructure holds the results of lexing and parsing.
// This contains rather than inherits from TextStructureView because
// the same owned memory can be used for multiple analysis views.
class TextStructure {
 public:
  explicit TextStructure(absl::string_view contents);

  TextStructure(const TextStructure&) = delete;
  TextStructure& operator=(const TextStructure&) = delete;
  TextStructure(TextStructure&&) = delete;
  TextStructure& operator=(TextStructure&&) = delete;

  // DeferredExpansion::subanalysis requires this destructor to be virtual.
  virtual ~TextStructure();

  const TextStructureView& Data() const { return data_; }

  TextStructureView& MutableData() { return data_; }

  const ConcreteSyntaxTree& SyntaxTree() const { return data_.SyntaxTree(); }

  // Verify that string_views are inside memory owned by owned_contents_.
  absl::Status StringViewConsistencyCheck() const;

  // Verify that internal data structures have valid ranges.
  absl::Status InternalConsistencyCheck() const;

 protected:
  // This string owns the memory referenced by all substring string_views
  // in this object.
  // TODO(hzeller): avoid local copy and just require that the string-view
  // we are called with outlives this object?
  const std::string owned_contents_;

  // The data_ object's string_views are owned by owned_contents_.
  TextStructureView data_;
};

}  // namespace verible

#endif  // VERIBLE_COMMON_TEXT_TEXT_STRUCTURE_H_
