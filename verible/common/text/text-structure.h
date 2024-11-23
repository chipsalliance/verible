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
#include <vector>

#include "absl/status/status.h"
#include "absl/strings/string_view.h"
#include "verible/common/strings/line-column-map.h"
#include "verible/common/strings/mem-block.h"
#include "verible/common/text/concrete-syntax-tree.h"
#include "verible/common/text/symbol.h"
#include "verible/common/text/token-info.h"
#include "verible/common/text/token-stream-view.h"
#include "verible/common/text/tree-utils.h"

namespace verilog {
class VerilogPreprocess;
}  // namespace verilog

namespace verible {

class TextStructure;

// TextStructureView contains sequences of tokens and a tree, but all
// string_views in this structure rely on string memory owned elsewhere.
//
// TODO(hzeller): This is a kitchen sink and should be split into multiple
// aspects; tokens, concrete syntax tree or line number mapping are different
// aspects not needed everywhere.
class TextStructureView {
 public:
  // Deferred in-place expansion of the syntax tree.
  // TODO(b/136014603): Replace with expandable token stream view abstraction.
  struct DeferredExpansion {
    // Position in the syntax tree to expand (leaf or node).
    SymbolPtr *expansion_point;

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
  TextStructureView(const TextStructureView &) = delete;
  TextStructureView &operator=(const TextStructureView &) = delete;

  absl::string_view Contents() const { return contents_; }

  const std::vector<absl::string_view> &Lines() const {
    return lazy_lines_info_.Get(contents_).lines;
  }

  const ConcreteSyntaxTree &SyntaxTree() const { return syntax_tree_; }

  ConcreteSyntaxTree &MutableSyntaxTree() { return syntax_tree_; }

  const TokenSequence &TokenStream() const { return tokens_; }

  TokenSequence &MutableTokenStream() { return tokens_; }

  const TokenStreamView &GetTokenStreamView() const { return tokens_view_; }

  TokenStreamView &MutableTokenStreamView() { return tokens_view_; }

  // Creates a stream of modifiable iterators to the filtered tokens.
  // Uses tokens_view_ to create the iterators.
  TokenStreamReferenceView MakeTokenStreamReferenceView();

  const LineColumnMap &GetLineColumnMap() const {
    return *lazy_lines_info_.Get(contents_).line_column_map;
  }

  // Given a byte offset, return the line/column
  // TODO(hzeller): this should probably be int64_t or a DocumentOffset typedef
  LineColumn GetLineColAtOffset(int bytes_offset) const {
    return GetLineColumnMap().GetLineColAtOffset(contents_, bytes_offset);
  }

  // Convenience function: Given the token, return the range it covers.
  LineColumnRange GetRangeForToken(const TokenInfo &token) const;

  // Convenience function: Given a text snippet, that needs to be a substring
  // of Contents(), return the range it covers.
  LineColumnRange GetRangeForText(absl::string_view text) const;

  // checks if a given text belongs to the TextStructure
  bool ContainsText(absl::string_view text) const;

  const std::vector<TokenSequence::const_iterator> &GetLineTokenMap() const;

  // Given line/column, find token that is available there. If this is out of
  // range, returns EOF.
  TokenInfo FindTokenAt(const LineColumn &pos) const;

  // Create the EOF token given the contents.
  TokenInfo EOFToken() const;

  // Trigger line token map re-calculation on next request.
  void CalculateFirstTokensPerLine() { lazy_line_token_map_.clear(); }

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
  void FilterTokens(const TokenFilterPredicate &);

  // Apply the same transformation to the token sequence, and the tokens
  // that were copied into the syntax tree.
  void MutateTokens(const LeafMutator &mutator);

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
  void ExpandSubtrees(NodeExpansionMap *expansions);

  // All of this class's consistency checks combined.
  absl::Status InternalConsistencyCheck() const;

 protected:
  // This is the text that is spanned by the token sequence and syntax tree.
  // This is required for calculating byte offsets to substrings contained
  // within this structure.  Pass this (via Contents()) to TokenInfo::left() and
  // TokenInfo::right() to calculate byte offsets, useful for diagnostics.
  absl::string_view contents_;

  // TODO(hzeller): These lazily generated elements are good candidates
  // for breaking out into their own abstraction.
  struct LinesInfo {
    bool valid = false;

    // Line-by-line view of contents_.
    std::vector<absl::string_view> lines;

    // Map to translate byte-offsets to line and column for diagnostics.
    std::unique_ptr<LineColumnMap> line_column_map;

    const LinesInfo &Get(absl::string_view contents);
  };
  // Mutable as we fill it lazily on request; conceptually the data is const.
  mutable LinesInfo lazy_lines_info_;

  // Tokens that constitute the original file (contents_).
  // This should always be terminated with a sentinel EOF token.
  TokenSequence tokens_;

  // Possibly modified view of the tokens_ token sequence.
  TokenStreamView tokens_view_;

  // Index of token iterators that mark the beginnings of each line.
  // Lazily calculated on request.
  mutable std::vector<TokenSequence::const_iterator> lazy_line_token_map_;

  // Tree representation of file contents.
  ConcreteSyntaxTree syntax_tree_;

  void TrimSyntaxTree(int first_token_offset, int last_token_offset);

  void TrimTokensToSubstring(int left_offset, int right_offset);

  void TrimContents(int left_offset, int length);

  void ConsumeDeferredExpansion(
      TokenSequence::const_iterator *next_token_iter,
      TokenStreamView::const_iterator *next_token_view_iter,
      DeferredExpansion *expansion, TokenSequence *combined_tokens,
      std::vector<int> *token_view_indices, const char *offset);

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

// TextStructure holds the text and the results of lexing, parsing, and other
// analysis in the corresponding TextStructureView.
//
// This class is not providing much benefit as ownership of memory and the
// parse result are only slightly related; but combining them here makes it
// harder to actually handle memory ownership and views independently. For
// instance the FileAnalyzer should keep track of the file content itself and
// then choose to generate a view (or multiple) on top of that (e.g for
// fallback parsing). Similar for VerilogSourceFile.
// The language server already has an in-memory representation which is
// unnecessarily copied into a TextStructure just to do the rest of the
// analysis, etc.. Long story short: it is beneficial to separate ownership and
// views.
//
// So: this class is eventually to be removed. For now, make all the
// constructors private and add explicitly mention all uses as friend classes,
// so a future refactoring is easier.
// (If, in the meantime, more TextStructure use is needed in other classes,
// not to worry: just add them here as friend class. This is merely
// documentation of use currently).
class TextStructure {
 private:
  friend class FileAnalyzer;
  friend class TextStructureTokenized;
  friend class TextStructureViewPublicTest_ExpandSubtreesOneLeaf_Test;
  friend class TextStructureViewPublicTest_ExpandSubtreesMultipleLeaves_Test;
  friend class verilog::VerilogPreprocess;  // NOLINT

  explicit TextStructure(std::shared_ptr<MemBlock> contents);

  // Convenience constructor in case our input is a string.
  explicit TextStructure(absl::string_view contents);

 public:
  TextStructure(const TextStructure &) = delete;
  TextStructure &operator=(const TextStructure &) = delete;
  TextStructure(TextStructure &&) = delete;
  TextStructure &operator=(TextStructure &&) = delete;

  // DeferredExpansion::subanalysis requires this destructor to be virtual.
  virtual ~TextStructure();

  const TextStructureView &Data() const { return data_; }

  TextStructureView &MutableData() { return data_; }

  const ConcreteSyntaxTree &SyntaxTree() const { return data_.SyntaxTree(); }

  // Verify that string_views are inside memory owned by owned_contents_.
  absl::Status StringViewConsistencyCheck() const;

  // Verify that internal data structures have valid ranges.
  absl::Status InternalConsistencyCheck() const;

 protected:
  // The content of this memblock is referenced in the TextStructureView.
  // The data itself might be shared between multiple entitites
  // (using a heavy shared_ptr might very well intermediate while refactoring
  // the details. https://github.com/chipsalliance/verible/issues/1502 )
  std::shared_ptr<MemBlock> contents_;

  // The data_ object's string_views are owned by owned_contents_.
  TextStructureView data_;
};

}  // namespace verible

#endif  // VERIBLE_COMMON_TEXT_TEXT_STRUCTURE_H_
