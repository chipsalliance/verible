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

#include "verible/common/formatting/tree-annotator.h"

#include <iterator>
#include <vector>

#include "verible/common/formatting/format-token.h"
#include "verible/common/text/concrete-syntax-leaf.h"
#include "verible/common/text/symbol.h"
#include "verible/common/text/syntax-tree-context.h"
#include "verible/common/text/token-info.h"
#include "verible/common/text/tree-context-visitor.h"

namespace verible {

namespace {  // implementation detail

// TreeAnnotator traverses a syntax tree and filtered token stream view,
// using the syntax tree to maintain context.
// TODO(fangism): The class bears some semblance to TreeUnwrapper in its
// simultaneous traversal of a token stream and syntax tree, and may be worth
// refactoring as a common pattern.
class TreeAnnotator : public TreeContextVisitor {
 public:
  TreeAnnotator(const Symbol *syntax_tree_root, const TokenInfo &eof_token,
                std::vector<PreFormatToken>::iterator tokens_begin,
                std::vector<PreFormatToken>::iterator tokens_end,
                const ContextTokenAnnotatorFunction &annotator)
      : eof_token_(eof_token),
        syntax_tree_root_(syntax_tree_root),
        token_annotator_(annotator),
        next_filtered_token_(tokens_begin),
        end_filtered_token_(tokens_end) {}

  void Annotate();

 private:                           // methods
  using TreeContextVisitor::Visit;  // for SyntaxTreeNode
  void Visit(const SyntaxTreeLeaf &leaf) final {
    CatchUpToCurrentLeaf(leaf.get());
  }

  void CatchUpToCurrentLeaf(const TokenInfo &leaf_token);

  // TODO(fangism): This exists solely to facilitate CatchUpToCurrentLeaf().
  // Consider using position in text_buffer as a terminator, and eliminating
  // this.
  const TokenInfo &EOFToken() const { return eof_token_; }

 private:  // fields
  // Saved copy of the EOF token from the original token stream.
  // This is used to mark the end of token processing.
  const TokenInfo eof_token_;

  // Syntax tree that will be traversed, and will provide context
  // during leaf visits.
  const Symbol *syntax_tree_root_ = nullptr;

  // Function used to annotate the PreFormatTokens.
  ContextTokenAnnotatorFunction token_annotator_;

  // Pointer to last token that was visited.
  // This gets passed to the first parameter (left token) of the
  // token_annotator_ function.
  std::vector<PreFormatToken>::iterator next_filtered_token_;
  // Pointer to end() of preformatted_tokens.
  const std::vector<PreFormatToken>::iterator end_filtered_token_;

  // Copy of current_context_ that is saved for use as a left-token's context
  // passed into the token_annotator_ function.
  SyntaxTreeContext saved_left_context_;
};

void TreeAnnotator::Annotate() {
  if (next_filtered_token_ == end_filtered_token_) return;

  // Visit the tokens from the beginning of the token stream through
  // the last syntax tree node.
  if (syntax_tree_root_ != nullptr) {
    syntax_tree_root_->Accept(this);
  }
  // Else without a syntax tree, the following code will still annotate
  // over the sequence of format tokens with an empty context, which is
  // suitable for context-insensitive annotations.

  // Visit the tokens between the last syntax tree node and EOF.
  // For example, there could be comments.
  CatchUpToCurrentLeaf(EOFToken());
}

void TreeAnnotator::CatchUpToCurrentLeaf(const TokenInfo &leaf_token) {
  // "Catch up" next_filtered_token_ to the current leaf.
  // Recall that SyntaxTreeLeaf has its own copy of TokenInfo,
  // so we need to compare a unique property instead of address.
  // The very last token (before end_filtered_token) is an EOF token,
  // which doesn't need to be annotated.
  // TODO(fangism): efficiently compute greatest-common-ancestor between
  // left- and right- context.  Ideally this should be memo-ized as the
  // traversal occurs, to avoid any unnecessary (linear) searches.
  while (std::distance(next_filtered_token_, end_filtered_token_) > 1 &&
         // compare const char* addresses:
         next_filtered_token_->token->text().begin() !=
             leaf_token.text().begin()) {
    const auto &left_token = *next_filtered_token_;
    ++next_filtered_token_;
    auto &right_token = *next_filtered_token_;
    token_annotator_(left_token, &right_token, saved_left_context_, Context());
  }
  // next_filtered_token_ now points to leaf_token, now caught up.
  // TODO(fangism): This costs an entire vector/stack-copy for every leaf token.
  // May need to choose a different structure for SyntaxTreeContext.
  saved_left_context_ = Context();
}

}  // namespace

void AnnotateFormatTokensUsingSyntaxContext(
    const Symbol *syntax_tree_root, const TokenInfo &eof_token,
    std::vector<PreFormatToken>::iterator tokens_begin,
    std::vector<PreFormatToken>::iterator tokens_end,
    const ContextTokenAnnotatorFunction &annotator) {
  TreeAnnotator t(syntax_tree_root, eof_token, tokens_begin, tokens_end,
                  annotator);
  t.Annotate();
}

}  // namespace verible
