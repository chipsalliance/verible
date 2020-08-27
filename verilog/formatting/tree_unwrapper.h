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

#ifndef VERIBLE_VERILOG_FORMATTING_TREE_UNWRAPPER_H_
#define VERIBLE_VERILOG_FORMATTING_TREE_UNWRAPPER_H_

#include <memory>
#include <vector>

#include "common/formatting/basic_format_style.h"
#include "common/formatting/format_token.h"
#include "common/formatting/token_partition_tree.h"
#include "common/formatting/tree_unwrapper.h"
#include "common/text/concrete_syntax_leaf.h"
#include "common/text/concrete_syntax_tree.h"
#include "common/text/syntax_tree_context.h"
#include "common/text/text_structure.h"
#include "common/text/token_info.h"
#include "common/text/token_stream_view.h"
#include "verilog/formatting/format_style.h"
#include "verilog/parser/verilog_token_enum.h"

namespace verilog {
namespace formatter {

// Data bundle that needs to outlive TreeUnwrapper.
// This data lives outside of TreeUnwrapper so it can be accessible to
// other phases of the formatter.
struct UnwrapperData {
  // TokenStreamView that removes spaces, but preserves comments.
  verible::TokenStreamView tokens_view_no_whitespace;

  // Array of PreFormatTokens that will be partitioned into UnwrappedLines.
  std::vector<verible::PreFormatToken> preformatted_tokens;

  explicit UnwrapperData(const verible::TokenSequence&);
};

// Derived TreeUnwrapper for Verilog Formatting.
// Contains all visitors and logic necessary for creating UnwrappedLines for
// Verilog and SystemVerilog code.
class TreeUnwrapper : public verible::TreeUnwrapper {
 public:
  explicit TreeUnwrapper(const verible::TextStructureView& view,
                         const FormatStyle& style,
                         const preformatted_tokens_type&);

  ~TreeUnwrapper();

  // Deleted standard interfaces:
  TreeUnwrapper() = delete;
  TreeUnwrapper(const TreeUnwrapper&) = delete;
  TreeUnwrapper(TreeUnwrapper&&) = delete;
  TreeUnwrapper& operator=(const TreeUnwrapper&) = delete;
  TreeUnwrapper& operator=(TreeUnwrapper&&) = delete;

  using verible::TreeUnwrapper::Unwrap;

 private:
  typedef std::vector<verible::PreFormatToken> preformatted_tokens_type;

  // Private implementation type for handling tokens between syntax tree leaves.
  class TokenScanner;

  // Collects filtered tokens into the UnwrappedLines from
  // next_unfiltered_token_ up until the TokenInfo referenced by leaf.
  // \postcondition next_unfiltered_token_ points to token corresponding to
  // leaf.
  void CatchUpToCurrentLeaf(const verible::TokenInfo& leaf_token);

  void LookAheadBeyondCurrentLeaf();
  void LookAheadBeyondCurrentNode();
  void InterChildNodeHook(const verible::SyntaxTreeNode&) override;

  void CollectLeadingFilteredTokens() override;

  // Collects filtered tokens into the UnwrappedLines from
  // next_unfiltered_token_ until EOF.
  void CollectTrailingFilteredTokens() override;

  void Visit(const verible::SyntaxTreeLeaf& leaf) override;
  void Visit(const verible::SyntaxTreeNode& node) override;

  void SetIndentationsAndCreatePartitions(const verible::SyntaxTreeNode& node);

  static void ReshapeTokenPartitions(
      const verible::SyntaxTreeNode& node,
      const verible::BasicFormatStyle& style,
      verible::TokenPartitionTree* recent_partition);

  // Visits a node which requires a new UnwrappedLine, followed by
  // traversing all children
  void VisitNewUnwrappedLine(const verible::SyntaxTreeNode& node);

  // Visits a node which requires a new UnwrappedLine, followed by
  // traversing all children. The new UnwrappedLine is *not* indented,
  // which is used for flush-left constructs.
  void VisitNewUnindentedUnwrappedLine(const verible::SyntaxTreeNode& node);

  // Advance next_unfiltered_token_ past any TK_SPACE tokens.
  // This is a stop-gap to help account for the change in
  // the KeepNonWhitespace (private) function, where we filtered out newlines
  // for the sake of PreFormatToken generation.
  // Where we used to traverse more inclusive filtered tokens, we must now
  // traverse unfiltered tokens.
  void EatSpaces();

  // Update token tracking, and possibly start a new partition.
  void UpdateInterLeafScanner(verilog_tokentype);

  // This should only be called directly from CatchUpToCurrentLeaf and
  // LookAheadBeyondCurrentLeaf.
  void AdvanceLastVisitedLeaf();

  // For print debugging.
  verible::TokenWithContext VerboseToken(const verible::TokenInfo&) const;

  // Data members:

  // Formatting style configuration.
  const FormatStyle& style_;

  // State machine for visiting non-syntax-tree tokens between leaves.
  // This determines placement of comments on unwrapped lines.
  // (Private-implementation idiom)
  std::unique_ptr<TokenScanner> inter_leaf_scanner_;

  // For debug printing.
  verible::TokenInfo::Context token_context_;
};

}  // namespace formatter
}  // namespace verilog

#endif  // VERIBLE_VERILOG_FORMATTING_TREE_UNWRAPPER_H_
