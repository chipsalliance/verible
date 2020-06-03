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

#include "verilog/formatting/align.h"

#include <functional>
#include <iostream>
#include <iterator>
#include <map>
#include <vector>

#include "common/formatting/align.h"
#include "common/formatting/format_token.h"
#include "common/formatting/unwrapped_line.h"
#include "common/text/concrete_syntax_leaf.h"
#include "common/text/concrete_syntax_tree.h"
#include "common/text/token_info.h"
#include "common/text/tree_utils.h"
#include "common/util/casts.h"
#include "common/util/logging.h"
#include "common/util/value_saver.h"
#include "verilog/CST/verilog_nonterminals.h"
#include "verilog/parser/verilog_token_classifications.h"
#include "verilog/parser/verilog_token_enum.h"

namespace verilog {
namespace formatter {

using verible::AlignmentCellScannerGenerator;
using verible::AlignmentColumnProperties;
using verible::ByteOffsetSet;
using verible::ColumnSchemaScanner;
using verible::down_cast;
using verible::FormatTokenRange;
using verible::MutableFormatTokenRange;
using verible::PreFormatToken;
using verible::Symbol;
using verible::SyntaxTreeLeaf;
using verible::SyntaxTreeNode;
using verible::SyntaxTreePath;
using verible::TokenPartitionTree;
using verible::TreeContextPathVisitor;
using verible::TreePathFormatter;
using verible::ValueSaver;

static const AlignmentColumnProperties FlushLeft(true);
static const AlignmentColumnProperties FlushRight(false);

template <class T>
static bool TokensAreAllComments(const T& tokens) {
  return std::find_if(
             tokens.begin(), tokens.end(),
             [](const typename T::value_type& token) {
               return !IsComment(verilog_tokentype(token.token->token_enum()));
             }) == tokens.end();
}

static bool IgnorePortDeclarationPartition(
    const TokenPartitionTree& partition) {
  const auto& uwline = partition.Value();
  const auto token_range = uwline.TokensRange();
  CHECK(!token_range.empty());
  // ignore lines containing only comments
  if (TokensAreAllComments(token_range)) return true;

  // ignore partitions belonging to preprocessing directives
  if (IsPreprocessorKeyword(verilog_tokentype(token_range.front().TokenEnum())))
    return true;

  // Ignore .x or .x(x) port declarations.
  // These can appear in a list_of_port_or_port_declarations.
  if (verible::SymbolCastToNode(*uwline.Origin()).MatchesTag(NodeEnum::kPort)) {
    return true;
  }
  return false;
}

class PortDeclarationColumnSchemaScanner : public ColumnSchemaScanner {
 public:
  PortDeclarationColumnSchemaScanner() = default;

  void Visit(const SyntaxTreeNode& node) override {
    auto tag = NodeEnum(node.Tag().tag);
    VLOG(2) << __FUNCTION__ << ", node: " << tag << " at "
            << TreePathFormatter(Path());
    if (new_column_after_open_bracket_) {
      ReserveNewColumn(node, FlushRight);
      new_column_after_open_bracket_ = false;
      TreeContextPathVisitor::Visit(node);
      return;
    }
    switch (tag) {
      case NodeEnum::kPackedDimensions: {
        // Kludge: kPackedDimensions can appear in paths [2,1] and [2,0,2],
        // but we want them to line up in the same column.  Make it so.
        if (current_path_ == SyntaxTreePath{2, 1}) {
          SyntaxTreePath new_path{2, 0, 2};
          const ValueSaver<SyntaxTreePath> path_saver(&current_path_, new_path);
          // TODO(fangism): a swap-based saver would be more efficient
          // for vectors.
          TreeContextPathVisitor::Visit(node);
          return;
        }
        break;
      }
      case NodeEnum::kDataType:
        // appears in path [2,0]
      case NodeEnum::kDimensionRange:
      case NodeEnum::kDimensionScalar:
      case NodeEnum::kDimensionSlice:
      case NodeEnum::kDimensionAssociativeType:
        // all of these cases cover packed and unpacked
        ReserveNewColumn(node, FlushLeft);
        break;
      case NodeEnum::kUnqualifiedId:
        if (Context().DirectParentIs(NodeEnum::kPortDeclaration)) {
          ReserveNewColumn(node, FlushLeft);
        }
        break;
      case NodeEnum::kExpression:
        // optional: Early termination of tree traversal.
        // This also helps reduce noise during debugging of this visitor.
        return;
        // case NodeEnum::kConstRef: possible in CST, but should be
        // syntactically illegal in module ports context.
      default:
        break;
    }
    // recursive visitation
    TreeContextPathVisitor::Visit(node);
    VLOG(2) << __FUNCTION__ << ", leaving node: " << tag;
  }

  void Visit(const SyntaxTreeLeaf& leaf) override {
    VLOG(2) << __FUNCTION__ << ", leaf: " << leaf.get() << " at "
            << TreePathFormatter(Path());
    if (new_column_after_open_bracket_) {
      ReserveNewColumn(leaf, FlushRight);
      new_column_after_open_bracket_ = false;
      return;
    }
    const int tag = leaf.get().token_enum();
    switch (tag) {
      // port directions
      case verilog_tokentype::TK_inout:
      case verilog_tokentype::TK_input:
      case verilog_tokentype::TK_output:
      case verilog_tokentype::TK_ref: {
        ReserveNewColumn(leaf, FlushLeft);
        break;
      }

        // net types
      case verilog_tokentype::TK_wire:
      case verilog_tokentype::TK_tri:
      case verilog_tokentype::TK_tri1:
      case verilog_tokentype::TK_supply0:
      case verilog_tokentype::TK_wand:
      case verilog_tokentype::TK_triand:
      case verilog_tokentype::TK_tri0:
      case verilog_tokentype::TK_supply1:
      case verilog_tokentype::TK_wor:
      case verilog_tokentype::TK_trior:
      case verilog_tokentype::TK_wone:
      case verilog_tokentype::TK_uwire: {
        // Effectively merge/re-map this into the next node slot,
        // which is kDataType of kPortDeclaration.
        // This works-around a quirk in the CST construction where net_types
        // like 'wire' appear positionally before kDataType variable types
        // like 'reg'.
        ReserveNewColumn(leaf, FlushLeft, verible::NextSiblingPath(Path()));
        break;
      }
      // For now, treat [...] as a single column per dimension.
      case '[': {
        if (ContextAtDeclarationDimensions()) {
          // FlushLeft vs. Right doesn't matter, this is a single character.
          ReserveNewColumn(leaf, FlushLeft);
          new_column_after_open_bracket_ = true;
        }
        break;
      }
      case ']': {
        if (ContextAtDeclarationDimensions()) {
          // FlushLeft vs. Right doesn't matter, this is a single character.
          ReserveNewColumn(leaf, FlushLeft);
        }
        break;
      }
      // TODO(b/70310743): Treat "[...:...]" as 5 columns.
      // Treat "[...]" (scalar) as 3 columns.
      // TODO(b/70310743): Treat the ... as a multi-column cell w.r.t.
      // the 5-column range format.
      default:
        break;
    }
    VLOG(2) << __FUNCTION__ << ", leaving leaf: " << leaf.get();
  }

 protected:
  bool ContextAtDeclarationDimensions() const {
    // Alternatively, could check that grandparent is
    // kDeclarationDimensions.
    return current_context_.DirectParentIsOneOf(
        {NodeEnum::kDimensionRange, NodeEnum::kDimensionScalar,
         NodeEnum::kDimensionSlice, NodeEnum::kDimensionAssociativeType});
  }

 private:
  // Set this to force the next syntax tree node/leaf to start a new column.
  // This is useful for aligning after punctation marks.
  bool new_column_after_open_bracket_ = false;
};

void TabularAlignTokenPartitions(TokenPartitionTree* partition_ptr,
                                 std::vector<PreFormatToken>* ftokens,
                                 absl::string_view full_text,
                                 const ByteOffsetSet& disabled_byte_ranges,
                                 int column_limit) {
  VLOG(1) << __FUNCTION__;
  auto& partition = *partition_ptr;
  auto& uwline = partition.Value();
  const auto* origin = uwline.Origin();
  VLOG(1) << "origin is nullptr? " << (origin == nullptr);
  if (origin == nullptr) return;
  const auto* node = down_cast<const SyntaxTreeNode*>(origin);
  VLOG(1) << "origin is node? " << (node != nullptr);
  if (node == nullptr) return;
  // Dispatch aligning function based on syntax tree node type.
  auto ftoken_base = ftokens->begin();

  static const auto* kAlignHandlers =
      new std::map<NodeEnum, verible::AlignmentCellScannerFunction>{
          {NodeEnum::kPortDeclarationList,
           AlignmentCellScannerGenerator<PortDeclarationColumnSchemaScanner>()},
      };
  const auto handler_iter = kAlignHandlers->find(NodeEnum(node->Tag().tag));
  if (handler_iter == kAlignHandlers->end()) return;
  verible::TabularAlignTokens(partition_ptr, handler_iter->second,
                              &IgnorePortDeclarationPartition, ftoken_base,
                              full_text, disabled_byte_ranges, column_limit);
  VLOG(1) << "end of " << __FUNCTION__;
}

}  // namespace formatter
}  // namespace verilog
