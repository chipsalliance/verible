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

#include <algorithm>
#include <functional>
#include <iostream>
#include <iterator>
#include <map>
#include <vector>

#include "common/formatting/align.h"
#include "common/formatting/format_token.h"
#include "common/formatting/token_partition_tree.h"
#include "common/formatting/unwrapped_line.h"
#include "common/text/concrete_syntax_leaf.h"
#include "common/text/concrete_syntax_tree.h"
#include "common/text/symbol.h"
#include "common/text/token_info.h"
#include "common/text/tree_utils.h"
#include "common/util/casts.h"
#include "common/util/logging.h"
#include "common/util/value_saver.h"
#include "verilog/CST/context_functions.h"
#include "verilog/CST/declaration.h"
#include "verilog/CST/verilog_nonterminals.h"
#include "verilog/parser/verilog_token_classifications.h"
#include "verilog/parser/verilog_token_enum.h"

namespace verilog {
namespace formatter {

using verible::AlignablePartitionGroup;
using verible::AlignedPartitionClassification;
using verible::AlignmentCellScannerGenerator;
using verible::AlignmentColumnProperties;
using verible::AlignmentGroupAction;
using verible::ByteOffsetSet;
using verible::ColumnSchemaScanner;
using verible::down_cast;
using verible::ExtractAlignmentGroupsFunction;
using verible::FormatTokenRange;
using verible::MutableFormatTokenRange;
using verible::PreFormatToken;
using verible::Symbol;
using verible::SyntaxTreeLeaf;
using verible::SyntaxTreeNode;
using verible::SyntaxTreePath;
using verible::TokenPartitionRange;
using verible::TokenPartitionTree;
using verible::TreeContextPathVisitor;
using verible::TreePathFormatter;
using verible::ValueSaver;

static const AlignmentColumnProperties FlushLeft(true);
static const AlignmentColumnProperties FlushRight(false);

template <class T>
static bool TokensAreAllComments(const T& tokens) {
  return std::all_of(
      tokens.begin(), tokens.end(), [](const typename T::value_type& token) {
        return IsComment(verilog_tokentype(token.token->token_enum()));
      });
}

template <class T>
static bool TokensHaveParenthesis(const T& tokens) {
  return std::any_of(tokens.begin(), tokens.end(),
                     [](const typename T::value_type& token) {
                       return token.TokenEnum() == '(';
                     });
}

static bool IgnoreWithinPortDeclarationPartitionGroup(
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

static bool IgnoreCommentsAndPreprocessingDirectives(
    const TokenPartitionTree& partition) {
  const auto& uwline = partition.Value();
  const auto token_range = uwline.TokensRange();
  CHECK(!token_range.empty());
  // ignore lines containing only comments
  if (TokensAreAllComments(token_range)) return true;

  // ignore partitions belonging to preprocessing directives
  if (IsPreprocessorKeyword(verilog_tokentype(token_range.front().TokenEnum())))
    return true;

  return false;
}

static bool IgnoreWithinActualNamedParameterPartitionGroup(
    const TokenPartitionTree& partition) {
  if (IgnoreCommentsAndPreprocessingDirectives(partition)) return true;

  // ignore everything that isn't passing a parameter by name
  const auto& uwline = partition.Value();
  return !verible::SymbolCastToNode(*uwline.Origin())
              .MatchesTag(NodeEnum::kParamByName);
}

static bool IgnoreWithinActualNamedPortPartitionGroup(
    const TokenPartitionTree& partition) {
  if (IgnoreCommentsAndPreprocessingDirectives(partition)) return true;

  const auto& uwline = partition.Value();
  const auto token_range = uwline.TokensRange();

  // ignore wildcard connections .*
  if (verilog_tokentype(token_range.front().TokenEnum()) ==
      verilog_tokentype::TK_DOTSTAR) {
    return true;
  }

  // ignore implicit connections .aaa
  if (verible::SymbolCastToNode(*uwline.Origin())
          .MatchesTag(NodeEnum::kActualNamedPort) &&
      !TokensHaveParenthesis(token_range)) {
    return true;
  }

  // ignore positional port connections
  if (verible::SymbolCastToNode(*uwline.Origin())
          .MatchesTag(NodeEnum::kActualPositionalPort)) {
    return true;
  }

  return false;
}

static bool TokenForcesLineBreak(const PreFormatToken& ftoken) {
  switch (ftoken.TokenEnum()) {
    case verilog_tokentype::TK_begin:
    case verilog_tokentype::TK_fork:
      return true;
  }
  return false;
}

static bool IgnoreMultilineCaseStatements(const TokenPartitionTree& partition) {
  if (IgnoreCommentsAndPreprocessingDirectives(partition)) return true;

  const auto& uwline = partition.Value();
  const auto token_range = uwline.TokensRange();

  // Scan for any tokens that would force a line break.
  if (std::any_of(token_range.begin(), token_range.end(),
                  &TokenForcesLineBreak)) {
    return true;
  }

  return false;
}

// This class marks up token-subranges in named parameter assignments for
// alignment. e.g. ".parameter_name(value_expression)"
class ActualNamedParameterColumnSchemaScanner : public ColumnSchemaScanner {
 public:
  ActualNamedParameterColumnSchemaScanner() = default;

  void Visit(const SyntaxTreeNode& node) override {
    auto tag = NodeEnum(node.Tag().tag);
    VLOG(2) << __FUNCTION__ << ", node: " << tag << " at "
            << TreePathFormatter(Path());
    switch (tag) {
      case NodeEnum::kParamByName: {
        // Always start first column right away
        ReserveNewColumn(node, FlushLeft);
        break;
      }
      case NodeEnum::kParenGroup:
        // Second column starts at the open parenthesis.
        if (Context().DirectParentIs(NodeEnum::kParamByName)) {
          ReserveNewColumn(node, FlushLeft);
        }
        break;
      default:
        break;
    }
    TreeContextPathVisitor::Visit(node);
    VLOG(2) << __FUNCTION__ << ", leaving node: " << tag;
  }
};

// This class marks up token-subranges in named port connections for alignment.
// e.g. ".port_name(net_name)"
class ActualNamedPortColumnSchemaScanner : public ColumnSchemaScanner {
 public:
  ActualNamedPortColumnSchemaScanner() = default;

  void Visit(const SyntaxTreeNode& node) override {
    auto tag = NodeEnum(node.Tag().tag);
    VLOG(2) << __FUNCTION__ << ", node: " << tag << " at "
            << TreePathFormatter(Path());
    switch (tag) {
      case NodeEnum::kActualNamedPort: {
        // Always start first column right away
        ReserveNewColumn(node, FlushLeft);
        break;
      }
      case NodeEnum::kParenGroup:
        // Second column starts at the open parenthesis.
        if (Context().DirectParentIs(NodeEnum::kActualNamedPort)) {
          ReserveNewColumn(node, FlushLeft);
        }
        break;
      default:
        break;
    }
    TreeContextPathVisitor::Visit(node);
    VLOG(2) << __FUNCTION__ << ", leaving node: " << tag;
  }
};

// This class marks up token-subranges in port declarations for alignment.
// e.g. "input wire clk,"
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
        if (verilog::analysis::ContextIsInsideDeclarationDimensions(
                Context())) {
          // FlushLeft vs. Right doesn't matter, this is a single character.
          ReserveNewColumn(leaf, FlushLeft);
          new_column_after_open_bracket_ = true;
        }
        break;
      }
      case ']': {
        if (verilog::analysis::ContextIsInsideDeclarationDimensions(
                Context())) {
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

 private:
  // Set this to force the next syntax tree node/leaf to start a new column.
  // This is useful for aligning after punctation marks.
  bool new_column_after_open_bracket_ = false;
};

static bool IsAlignableDeclaration(const SyntaxTreeNode& node) {
  // A data/net/variable declaration is alignable if:
  // * it is not a module instance
  // * it declares exactly one identifier
  switch (static_cast<NodeEnum>(node.Tag().tag)) {
    case NodeEnum::kDataDeclaration: {
      const SyntaxTreeNode& instances(GetInstanceListFromDataDeclaration(node));
      if (FindAllRegisterVariables(instances).size() > 1) return false;
      if (!FindAllGateInstances(instances).empty()) return false;
      return true;
    }
    case NodeEnum::kNetDeclaration: {
      if (FindAllNetVariables(node).size() > 1) return false;
      return true;
    }
    default:
      return false;
  }
}

// These enums classify alignable groups of token partitions by their syntax
// structure, which then map to different alignment handler routines.
// These need not have a 1:1 correspondence to verilog::NodeEnum syntax tree
// enums, a single value here could apply to a group of syntax tree node types.
enum class AlignableSyntaxSubtype {
  kDontCare = 0,
  // named actual parameters
  kNamedActualParameters,
  // named actual ports
  kNamedActualPorts,
  // parameter declarations
  kParameterDeclaration,
  // port declarations
  kPortDeclaration,
  // net/variable declarations
  kDataDeclaration,
  // class member variable declarations
  kClassMemberVariables,
  // case-like items
  kCaseLikeItems,
  // continuous assignment statements
  kContinuousAssignment,
};

static AlignedPartitionClassification AlignClassify(
    AlignmentGroupAction match,
    AlignableSyntaxSubtype subtype = AlignableSyntaxSubtype::kDontCare) {
  if (match == AlignmentGroupAction::kMatch) {
    CHECK(subtype != AlignableSyntaxSubtype::kDontCare);
  }
  return {match, static_cast<int>(subtype)};
}

static std::vector<verible::TaggedTokenPartitionRange>
GetConsecutiveModuleItemGroups(const TokenPartitionRange& partitions) {
  VLOG(2) << __FUNCTION__;
  return GetPartitionAlignmentSubranges(
      partitions,  //
      [](const TokenPartitionTree& partition)
          -> AlignedPartitionClassification {
        const Symbol* origin = partition.Value().Origin();
        if (origin == nullptr) return {AlignmentGroupAction::kIgnore};
        const verible::SymbolTag symbol_tag = origin->Tag();
        if (symbol_tag.kind != verible::SymbolKind::kNode)
          return AlignClassify(AlignmentGroupAction::kIgnore);
        const SyntaxTreeNode& node = verible::SymbolCastToNode(*origin);
        // Align net/variable declarations.
        if (IsAlignableDeclaration(node)) {
          return AlignClassify(AlignmentGroupAction::kMatch,
                               AlignableSyntaxSubtype::kDataDeclaration);
        }
        // Align continuous assignment, like "assign foo = bar;"
        if (node.MatchesTag(NodeEnum::kContinuousAssignmentStatement)) {
          return AlignClassify(AlignmentGroupAction::kMatch,
                               AlignableSyntaxSubtype::kContinuousAssignment);
        }
        return AlignClassify(AlignmentGroupAction::kNoMatch);
      });
}

static std::vector<verible::TaggedTokenPartitionRange>
GetConsecutiveClassItemGroups(const TokenPartitionRange& partitions) {
  VLOG(2) << __FUNCTION__;
  return GetPartitionAlignmentSubranges(
      partitions,  //
      [](const TokenPartitionTree& partition)
          -> AlignedPartitionClassification {
        const Symbol* origin = partition.Value().Origin();
        if (origin == nullptr) return {AlignmentGroupAction::kIgnore};
        const verible::SymbolTag symbol_tag = origin->Tag();
        if (symbol_tag.kind != verible::SymbolKind::kNode)
          return {AlignmentGroupAction::kIgnore};
        const SyntaxTreeNode& node = verible::SymbolCastToNode(*origin);
        return AlignClassify(IsAlignableDeclaration(node)
                                 ? AlignmentGroupAction::kMatch
                                 : AlignmentGroupAction::kNoMatch,
                             AlignableSyntaxSubtype::kClassMemberVariables);
      });
}

// This class marks up token-subranges in data declarations for alignment.
// e.g. "foo_pkg::bar_t [3:0] some_values;"
// Much of the implementation of this scanner was based on
// PortDeclarationColumnSchemaScanner.
// Differences:
//   * here, there are no port directions to worry about.
//   * need to handle both kDataDeclaration and kNetDeclaration.
// TODO(fangism): refactor out common logic
class DataDeclarationColumnSchemaScanner : public ColumnSchemaScanner {
 public:
  DataDeclarationColumnSchemaScanner() = default;

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
      case NodeEnum::kDataDeclaration:
      case NodeEnum::kNetDeclaration: {
        // Don't wait for the type node, just start the first column right away.
        ReserveNewColumn(node, FlushLeft);
        break;
      }
      case NodeEnum::kPackedDimensions: {
        // Kludge: kPackedDimensions can appear in paths:
        //   [1,0,0,2] in kDataDeclaration
        //   [1,0,1] in kNetDeclaration
        // but we want them to line up in the same column.  Make it so.
        if (current_path_ == SyntaxTreePath{1, 0, 0, 2}) {
          SyntaxTreePath new_path{1, 0, 1};
          const ValueSaver<SyntaxTreePath> path_saver(&current_path_, new_path);
          TreeContextPathVisitor::Visit(node);
          return;
        }
        break;
      }
      case NodeEnum::kDimensionRange:
      case NodeEnum::kDimensionScalar:
      case NodeEnum::kDimensionSlice:
      case NodeEnum::kDimensionAssociativeType: {
        // all of these cases cover packed and unpacked dimensions
        ReserveNewColumn(node, FlushLeft);
        break;
      }
      case NodeEnum::kRegisterVariable: {
        // at path [1,1,0] in kDataDeclaration
        // contains the declared id
        ReserveNewColumn(node, FlushLeft);
        break;
      }
      case NodeEnum::kNetVariable: {
        // at path [2,0] in kNetDeclaration
        // contains the declared id
        // make this fit with kRegisterVariable
        if (current_path_ == SyntaxTreePath{2, 0}) {
          SyntaxTreePath new_path{1, 1, 0};
          const ValueSaver<SyntaxTreePath> path_saver(&current_path_, new_path);
          ReserveNewColumn(node, FlushLeft);
          TreeContextPathVisitor::Visit(node);
          return;
        }
        break;
      }
      case NodeEnum::kExpression:
        // optional: Early termination of tree traversal.
        // This also helps reduce noise during debugging of this visitor.
        return;
      // case NodeEnum::kConstRef: possible in CST, but should be
      // syntactically illegal in module ports context.
      default:
        break;
    }
    TreeContextPathVisitor::Visit(node);
    VLOG(2) << "end of " << __FUNCTION__ << ", node: " << tag;
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

// This class marks up token-subranges in class member variable (data
// declarations) for alignment. e.g. "const int [3:0] member_name;" For now,
// re-use the same column scanner as data/variable/net declarations.
class ClassPropertyColumnSchemaScanner : public ColumnSchemaScanner {
 public:
  ClassPropertyColumnSchemaScanner() = default;

  void Visit(const SyntaxTreeNode& node) override {
    auto tag = NodeEnum(node.Tag().tag);
    VLOG(2) << __FUNCTION__ << ", node: " << tag << " at "
            << TreePathFormatter(Path());
    switch (tag) {
      case NodeEnum::kDataDeclaration:
      case NodeEnum::kVariableDeclarationAssignment: {
        // Don't wait for the type node, just start the first column right away.
        ReserveNewColumn(node, FlushLeft);
        break;
      }
      default:
        break;
    }
    TreeContextPathVisitor::Visit(node);
    VLOG(2) << "end of " << __FUNCTION__ << ", node: " << tag;
  }

  void Visit(const SyntaxTreeLeaf& leaf) override {
    VLOG(2) << __FUNCTION__ << ", leaf: " << leaf.get() << " at "
            << TreePathFormatter(Path());
    const int tag = leaf.get().token_enum();
    switch (tag) {
      case '=':
        ReserveNewColumn(leaf, FlushLeft);
        break;
      default:
        break;
    }
    VLOG(2) << __FUNCTION__ << ", leaving leaf: " << leaf.get();
  }
};

// This class marks up token-subranges in formal parameter declarations for
// alignment.
// e.g. "localparam int Width = 5;"
class ParameterDeclarationColumnSchemaScanner : public ColumnSchemaScanner {
 public:
  ParameterDeclarationColumnSchemaScanner() = default;

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
      case NodeEnum::kTypeInfo: {
        SyntaxTreePath new_path{1};
        const ValueSaver<SyntaxTreePath> path_saver(&current_path_, new_path);
        ReserveNewColumn(node, FlushLeft);
        break;
      }

      case NodeEnum::kTrailingAssign: {
        ReserveNewColumn(node, FlushLeft);
        break;
      }

      case NodeEnum::kUnqualifiedId: {
        if (Context().DirectParentIs(NodeEnum::kParamType)) {
          ReserveNewColumn(node, FlushLeft);
        }
        break;
      }
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
      // Align keywords 'parameter', 'localparam' and 'type' under the same
      // column.
      case verilog_tokentype::TK_parameter:
      case verilog_tokentype::TK_localparam: {
        ReserveNewColumn(leaf, FlushLeft);
        break;
      }

      case verilog_tokentype::TK_type: {
        if (Context().DirectParentIs(NodeEnum::kParamDeclaration)) {
          ReserveNewColumn(leaf, FlushLeft);
        }
        break;
      }

      // Sometimes the parameter indentifier which is of token SymbolIdentifier
      // can appear at different paths depending on the parameter type. Make
      // them aligned so they fall under the same column.
      case verilog_tokentype::SymbolIdentifier: {
        if (current_path_ == SyntaxTreePath{2, 0}) {
          SyntaxTreePath new_path{1, 2};
          const ValueSaver<SyntaxTreePath> path_saver(&current_path_, new_path);
          ReserveNewColumn(leaf, FlushLeft);
          return;
        }

        if (Context().DirectParentIs(NodeEnum::kParamType))
          ReserveNewColumn(leaf, FlushLeft);
        break;
      }

      // '=' is another column where things should be aligned. But type
      // declarations and localparam cause '=' to appear under two different
      // paths in CST. Align them.
      case '=': {
        if (current_path_ == SyntaxTreePath{2, 1}) {
          SyntaxTreePath new_path{2};
          const ValueSaver<SyntaxTreePath> path_saver(&current_path_, new_path);
          ReserveNewColumn(leaf, FlushLeft);
        }
        break;
      }

      // Align packed and unpacked dimenssions
      case '[': {
        if (verilog::analysis::ContextIsInsideDeclarationDimensions(
                Context()) &&
            !Context().IsInside(NodeEnum::kActualParameterList)) {
          // FlushLeft vs. Right doesn't matter, this is a single character.
          ReserveNewColumn(leaf, FlushLeft);
          new_column_after_open_bracket_ = true;
        }
        break;
      }
      case ']': {
        if (verilog::analysis::ContextIsInsideDeclarationDimensions(
                Context()) &&
            !Context().IsInside(NodeEnum::kActualParameterList)) {
          // FlushLeft vs. Right doesn't matter, this is a single character.
          ReserveNewColumn(leaf, FlushLeft);
        }
        break;
      }

      default:
        break;
    }
    VLOG(2) << __FUNCTION__ << ", leaving leaf: " << leaf.get();
  }

 private:
  bool new_column_after_open_bracket_ = false;
};

// This class marks up token-subranges in case items for alignment.
// e.g. "value1, value2: x = f(y);"
// This is suitable for a variety of case-like items: statements, generate
// items.
class CaseItemColumnSchemaScanner : public ColumnSchemaScanner {
 public:
  CaseItemColumnSchemaScanner() = default;

  bool ParentContextIsCaseItem() const {
    return Context().DirectParentIsOneOf(
        {NodeEnum::kCaseItem, NodeEnum::kCaseInsideItem,
         NodeEnum::kGenerateCaseItem, NodeEnum::kDefaultItem});
  }

  void Visit(const SyntaxTreeNode& node) override {
    auto tag = NodeEnum(node.Tag().tag);
    VLOG(2) << __FUNCTION__ << ", node: " << tag << " at "
            << TreePathFormatter(Path());

    if (previous_token_was_case_colon_) {
      if (ParentContextIsCaseItem()) {
        ReserveNewColumn(node, FlushLeft);
        previous_token_was_case_colon_ = false;
      }
    } else {
      switch (tag) {
        case NodeEnum::kCaseItem:
        case NodeEnum::kCaseInsideItem:
        case NodeEnum::kGenerateCaseItem:
        case NodeEnum::kDefaultItem: {
          // Start a new column right away.
          ReserveNewColumn(node, FlushLeft);
          break;
        }
        default:
          break;
      }
    }

    // recursive visitation
    TreeContextPathVisitor::Visit(node);
    VLOG(2) << __FUNCTION__ << ", leaving node: " << tag;
  }

  void Visit(const SyntaxTreeLeaf& leaf) override {
    VLOG(2) << __FUNCTION__ << ", leaf: " << leaf.get() << " at "
            << TreePathFormatter(Path());
    const int tag = leaf.get().token_enum();
    switch (tag) {
      case ':':
        if (ParentContextIsCaseItem()) {
          // mark the next node as the start of a new column
          previous_token_was_case_colon_ = true;
        }
        break;
      default:
        break;
    }
    VLOG(2) << __FUNCTION__ << ", leaving leaf: " << leaf.get();
  }

 private:
  bool previous_token_was_case_colon_ = false;
};

// This class marks up token-subranges in assignment statements for alignment.
// e.g. "assign foo = bar;"
class ContinuousAssignmentColumnSchemaScanner : public ColumnSchemaScanner {
 public:
  ContinuousAssignmentColumnSchemaScanner() = default;

  void Visit(const SyntaxTreeNode& node) override {
    auto tag = NodeEnum(node.Tag().tag);
    VLOG(2) << __FUNCTION__ << ", node: " << tag << " at "
            << TreePathFormatter(Path());

    switch (tag) {
      case NodeEnum::kContinuousAssignmentStatement: {
        // Start a new column right away.
        ReserveNewColumn(node, FlushLeft);
        break;
      }
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
    const int tag = leaf.get().token_enum();
    switch (tag) {
      case '=':  // align at '='
        if (Context().DirectParentIs(NodeEnum::kNetVariableAssignment)) {
          ReserveNewColumn(leaf, FlushLeft);
          break;
        }
        break;
      default:
        break;
    }
    VLOG(2) << __FUNCTION__ << ", leaving leaf: " << leaf.get();
  }
};

static std::function<
    std::vector<verible::TaggedTokenPartitionRange>(const TokenPartitionRange&)>
PartitionBetweenBlankLines(AlignableSyntaxSubtype subtype) {
  return [subtype](const TokenPartitionRange& range) {
    return verible::GetSubpartitionsBetweenBlankLinesSingleTag(
        range, static_cast<int>(subtype));
  };
}

// Each alignment group subtype maps to a set of functions.
struct AlignmentGroupHandlers {
  verible::AlignmentCellScannerFunction column_scanner;
  std::function<verible::AlignmentPolicy(const FormatStyle& vstyle)>
      policy_func;
};

// Convert a pointer-to-member to a function/lambda that accesses that member.
// Returns the referenced member by value.
// TODO(fangism): move this to an STL-style util/functional library
template <typename MemberType, typename StructType>
std::function<MemberType(const StructType&)> function_from_pointer_to_member(
    MemberType StructType::*member) {
  return [member](const StructType& obj) { return obj.*member; };
}

using AlignmentHandlerMapType =
    std::map<AlignableSyntaxSubtype, AlignmentGroupHandlers>;

// Global registry of all known alignment handlers for Verilog.
// This organization lets the same handlers be re-used in multiple syntactic
// contexts, e.g. data declarations can be module items and generate items and
// block statement items.
static const AlignmentHandlerMapType& AlignmentHandlerLibrary() {
  static const auto* handler_map = new AlignmentHandlerMapType{
      {AlignableSyntaxSubtype::kDataDeclaration,
       {AlignmentCellScannerGenerator<DataDeclarationColumnSchemaScanner>(),
        function_from_pointer_to_member(
            &FormatStyle::module_net_variable_alignment)}},
      {AlignableSyntaxSubtype::kNamedActualParameters,
       {AlignmentCellScannerGenerator<
            ActualNamedParameterColumnSchemaScanner>(),
        function_from_pointer_to_member(
            &FormatStyle::named_parameter_alignment)}},
      {AlignableSyntaxSubtype::kNamedActualPorts,
       {AlignmentCellScannerGenerator<ActualNamedPortColumnSchemaScanner>(),
        function_from_pointer_to_member(&FormatStyle::named_port_alignment)}},
      {AlignableSyntaxSubtype::kParameterDeclaration,
       {AlignmentCellScannerGenerator<
            ParameterDeclarationColumnSchemaScanner>(),
        function_from_pointer_to_member(
            &FormatStyle::formal_parameters_alignment)}},
      {AlignableSyntaxSubtype::kPortDeclaration,
       {AlignmentCellScannerGenerator<PortDeclarationColumnSchemaScanner>(),
        function_from_pointer_to_member(
            &FormatStyle::port_declarations_alignment)}},
      {AlignableSyntaxSubtype::kClassMemberVariables,
       {AlignmentCellScannerGenerator<ClassPropertyColumnSchemaScanner>(),
        function_from_pointer_to_member(
            &FormatStyle::class_member_variable_alignment)}},
      {AlignableSyntaxSubtype::kCaseLikeItems,
       {AlignmentCellScannerGenerator<CaseItemColumnSchemaScanner>(),
        function_from_pointer_to_member(&FormatStyle::case_items_alignment)}},
      {AlignableSyntaxSubtype::kContinuousAssignment,
       {AlignmentCellScannerGenerator<
            ContinuousAssignmentColumnSchemaScanner>(),
        function_from_pointer_to_member(
            &FormatStyle::assignment_statement_alignment)}},
  };
  return *handler_map;
}

static verible::AlignmentCellScannerFunction AlignmentColumnScannerSelector(
    int subtype) {
  static const auto& handler_map = AlignmentHandlerLibrary();
  const auto iter = handler_map.find(AlignableSyntaxSubtype(subtype));
  CHECK(iter != handler_map.end()) << "subtype: " << subtype;
  return iter->second.column_scanner;
}

static std::function<verible::AlignmentPolicy(int)> AlignmentPolicySelector(
    const FormatStyle& vstyle) {
  static const auto& handler_map = AlignmentHandlerLibrary();
  return [&vstyle](int subtype) {
    const auto iter = handler_map.find(AlignableSyntaxSubtype(subtype));
    CHECK(iter != handler_map.end()) << "subtype: " << subtype;
    return iter->second.policy_func(vstyle);
  };
}

using AlignSyntaxGroupsFunction =
    std::function<std::vector<AlignablePartitionGroup>(
        const TokenPartitionRange& range, const FormatStyle& style)>;

static std::vector<AlignablePartitionGroup> AlignPortDeclarations(
    const TokenPartitionRange& full_range, const FormatStyle& vstyle) {
  return verible::ExtractAlignmentGroupsAdapter(
      PartitionBetweenBlankLines(AlignableSyntaxSubtype::kPortDeclaration),
      &IgnoreWithinPortDeclarationPartitionGroup,
      &AlignmentColumnScannerSelector,
      AlignmentPolicySelector(vstyle))(full_range);
}

static std::vector<AlignablePartitionGroup> AlignActualNamedParameters(
    const TokenPartitionRange& full_range, const FormatStyle& vstyle) {
  return verible::ExtractAlignmentGroupsAdapter(
      PartitionBetweenBlankLines(
          AlignableSyntaxSubtype::kNamedActualParameters),
      &IgnoreWithinActualNamedParameterPartitionGroup,
      &AlignmentColumnScannerSelector,
      AlignmentPolicySelector(vstyle))(full_range);
}

static std::vector<AlignablePartitionGroup> AlignActualNamedPorts(
    const TokenPartitionRange& full_range, const FormatStyle& vstyle) {
  return verible::ExtractAlignmentGroupsAdapter(
      PartitionBetweenBlankLines(AlignableSyntaxSubtype::kNamedActualPorts),
      &IgnoreWithinActualNamedPortPartitionGroup,
      &AlignmentColumnScannerSelector,
      AlignmentPolicySelector(vstyle))(full_range);
}

static std::vector<AlignablePartitionGroup> AlignModuleItems(
    const TokenPartitionRange& full_range, const FormatStyle& vstyle) {
  // Currently, this only handles data/net/variable declarations.
  // TODO(b/161814377): align continuous assignments
  return verible::ExtractAlignmentGroupsAdapter(
      &GetConsecutiveModuleItemGroups,
      &IgnoreCommentsAndPreprocessingDirectives,
      &AlignmentColumnScannerSelector,
      AlignmentPolicySelector(vstyle))(full_range);
}

static std::vector<AlignablePartitionGroup> AlignClassItems(
    const TokenPartitionRange& full_range, const FormatStyle& vstyle) {
  // TODO(fangism): align other class items besides member variables.
  return verible::ExtractAlignmentGroupsAdapter(
      &GetConsecutiveClassItemGroups, &IgnoreCommentsAndPreprocessingDirectives,
      &AlignmentColumnScannerSelector,
      AlignmentPolicySelector(vstyle))(full_range);
}

static std::vector<AlignablePartitionGroup> AlignCaseItems(
    const TokenPartitionRange& full_range, const FormatStyle& vstyle) {
  return verible::ExtractAlignmentGroupsAdapter(
      PartitionBetweenBlankLines(AlignableSyntaxSubtype::kCaseLikeItems),
      &IgnoreMultilineCaseStatements, &AlignmentColumnScannerSelector,
      AlignmentPolicySelector(vstyle))(full_range);
}

static std::vector<AlignablePartitionGroup> AlignParameterDeclarations(
    const TokenPartitionRange& full_range, const FormatStyle& vstyle) {
  return verible::ExtractAlignmentGroupsAdapter(
      PartitionBetweenBlankLines(AlignableSyntaxSubtype::kParameterDeclaration),
      &IgnoreWithinPortDeclarationPartitionGroup,
      &AlignmentColumnScannerSelector,
      AlignmentPolicySelector(vstyle))(full_range);
}

void TabularAlignTokenPartitions(TokenPartitionTree* partition_ptr,
                                 std::vector<PreFormatToken>* ftokens,
                                 absl::string_view full_text,
                                 const ByteOffsetSet& disabled_byte_ranges,
                                 const FormatStyle& style) {
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

  static const auto* const kAlignHandlers =
      new std::map<NodeEnum, AlignSyntaxGroupsFunction>{
          {NodeEnum::kPortDeclarationList, &AlignPortDeclarations},
          {NodeEnum::kActualParameterByNameList, &AlignActualNamedParameters},
          {NodeEnum::kPortActualList, &AlignActualNamedPorts},
          // TODO(fangism): make the following apply to generate-item lists too
          {NodeEnum::kModuleItemList, &AlignModuleItems},
          {NodeEnum::kFormalParameterList, &AlignParameterDeclarations},
          {NodeEnum::kClassItems, &AlignClassItems},
          // various case-like constructs:
          {NodeEnum::kCaseItemList, &AlignCaseItems},
          {NodeEnum::kCaseInsideItemList, &AlignCaseItems},
          {NodeEnum::kGenerateCaseItemList, &AlignCaseItems},
      };
  const auto handler_iter = kAlignHandlers->find(NodeEnum(node->Tag().tag));
  if (handler_iter == kAlignHandlers->end()) return;
  verible::TabularAlignTokens(
      partition_ptr,
      [&style, handler_iter](const TokenPartitionRange& range) {
        // Return a closure bound to the current formatting style.
        return handler_iter->second(range, style);
      },
      ftokens, full_text, disabled_byte_ranges, style.column_limit);
  VLOG(1) << "end of " << __FUNCTION__;
}

}  // namespace formatter
}  // namespace verilog
