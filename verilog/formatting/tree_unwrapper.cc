// Copyright 2017-2019 The Verible Authors.
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

#include "verilog/formatting/tree_unwrapper.h"

#include <algorithm>
#include <iterator>
#include <vector>

#include "common/formatting/basic_format_style.h"
#include "common/formatting/format_token.h"
#include "common/formatting/token_partition_tree.h"
#include "common/formatting/tree_unwrapper.h"
#include "common/formatting/unwrapped_line.h"
#include "common/text/concrete_syntax_leaf.h"
#include "common/text/concrete_syntax_tree.h"
#include "common/text/constants.h"
#include "common/text/symbol.h"
#include "common/text/syntax_tree_context.h"
#include "common/text/text_structure.h"
#include "common/text/token_info.h"
#include "common/text/token_stream_view.h"
#include "common/util/container_iterator_range.h"
#include "common/util/logging.h"
#include "common/util/value_saver.h"
#include "verilog/CST/verilog_nonterminals.h"
#include "verilog/formatting/token_scanner.h"
#include "verilog/formatting/verilog_token.h"
#include "verilog/parser/verilog_parser.h"
#include "verilog/parser/verilog_token_enum.h"

namespace verilog {
namespace formatter {

using ::verible::PartitionPolicyEnum;
using ::verible::PreFormatToken;
using ::verible::TokenInfo;

// Used to filter the TokenStreamView by discarding space-only tokens.
static bool KeepNonWhitespace(const TokenInfo& t) {
  switch (t.token_enum) {
    case yytokentype::TK_NEWLINE:  // fall-through
    case yytokentype::TK_SPACE:
    case verible::TK_EOF:  // omit the EOF token
      return false;
    default:
      break;
  }
  return true;
}

// Creates a PreFormatToken given a TokenInfo and returns it
static PreFormatToken CreateFormatToken(const TokenInfo& token) {
  PreFormatToken format_token(&token);
  format_token.format_token_enum =
      GetFormatTokenType(yytokentype(format_token.TokenEnum()));
  switch (format_token.format_token_enum) {
    case FormatTokenType::open_group:
      format_token.balancing = verible::GroupBalancing::Open;
      break;
    case FormatTokenType::close_group:
      format_token.balancing = verible::GroupBalancing::Close;
      break;
    default:
      format_token.balancing = verible::GroupBalancing::None;
      break;
  }
  return format_token;
}

UnwrapperData::UnwrapperData(const verible::TokenSequence& tokens) {
  // Create a TokenStreamView that removes spaces, but preserves comments.
  {
    verible::InitTokenStreamView(tokens, &tokens_view_no_whitespace);
    verible::FilterTokenStreamViewInPlace(KeepNonWhitespace,
                                          &tokens_view_no_whitespace);
  }

  // Create an array of PreFormatTokens.
  {
    preformatted_tokens.reserve(tokens_view_no_whitespace.size());
    std::transform(tokens_view_no_whitespace.begin(),
                   tokens_view_no_whitespace.end(),
                   std::back_inserter(preformatted_tokens),
                   [=](verible::TokenSequence::const_iterator iter) {
                     return CreateFormatToken(*iter);
                   });
  }
}

void TreeUnwrapper::EatSpaces() {
  SkipUnfilteredTokens(
      [](const TokenInfo& token) { return token.token_enum == TK_SPACE; });
}

TreeUnwrapper::TreeUnwrapper(const verible::TextStructureView& view,
                             const verible::BasicFormatStyle& style,
                             const preformatted_tokens_type& ftokens)
    : verible::TreeUnwrapper(view, ftokens), style_(style) {}

static bool IsPreprocessorClause(NodeEnum e) {
  switch (e) {
    case NodeEnum::kPreprocessorIfdefClause:
    case NodeEnum::kPreprocessorIfndefClause:
    case NodeEnum::kPreprocessorElseClause:
    case NodeEnum::kPreprocessorElsifClause:
      return true;
    default:
      return false;
  }
}

void TreeUnwrapper::AdvanceLastVisitedLeaf(bool new_unwrapped_lines_allowed) {
  const auto& next_token = *NextUnfilteredToken();
  const yytokentype token_enum = yytokentype(next_token.token_enum);
  inter_leaf_scanner_.UpdateState(token_enum);

  if (new_unwrapped_lines_allowed) {
    if (inter_leaf_scanner_.EndState()) {
      CurrentUnwrappedLine().only_comments_ = true;
      StartNewUnwrappedLine();
    } else if (inter_leaf_scanner_.RepeatNewlineState()) {
      StartNewUnwrappedLine();
    }
  }
  AdvanceNextUnfilteredToken();
}

void TreeUnwrapper::CatchUpToCurrentLeaf(const verible::TokenInfo& leaf_token) {
  // "Catch up" NextUnfilteredToken() to the current leaf.
  // Assigns non-whitespace tokens such as comments into UnwrappedLines.
  // Recall that SyntaxTreeLeaf has its own copy of TokenInfo,
  // so we need to compare a unique property instead of address.
  const bool can_create_unwrapped_lines = CurrentUnwrappedLine().IsEmpty();
  while (!NextUnfilteredToken()->isEOF() &&
         // compare const char* addresses:
         NextUnfilteredToken()->text.begin() != leaf_token.text.begin()) {
    AdvanceLastVisitedLeaf(can_create_unwrapped_lines);
  }
}

void TreeUnwrapper::LookAheadBeyondCurrentLeaf() {
  // Re-initialize internal token-scanning state machine.
  inter_leaf_scanner_.Reset();

  // If current unwrapped line already contains a tree leaf token,
  // then only scan up to the next newline without creating new unwrapped lines.
  const bool can_create_unwrapped_lines = CurrentUnwrappedLine().IsEmpty();

  // Consume trailing comments after the last leaf token.
  // Ignore spaces but stop at newline.
  // Advance until hitting a newline or catching up to the current leaf node,
  // whichever comes first.
  //
  // TODO(fangism): This might not adequately handle a block of //-style
  // comments that span multiple lines.  e.g.
  //
  //   ... some token;   // start of long comment ...
  //                     // continuation of long comment
  //   next tokens...
  //
  while (!NextUnfilteredToken()->isEOF()) {
    EatSpaces();
    if (IsComment(yytokentype(NextUnfilteredToken()->token_enum))) {
      AdvanceLastVisitedLeaf(can_create_unwrapped_lines);
    } else {  // including newline
      // Don't advance NextUnfilteredToken() in this case.
      break;
    }
  }
}

void TreeUnwrapper::CollectTrailingFilteredTokens() {
  // This should emulate ::Visit(leaf == EOF token).

  // A newline means there are no comments to add to this UnwrappedLine
  if (NextUnfilteredToken()->token_enum == yytokentype::TK_NEWLINE) {
    StartNewUnwrappedLine();
  }

  // "Catch up" to EOF.
  // The very last unfiltered token scanned should be the EOF.
  CatchUpToCurrentLeaf(EOFToken());
}

// Visitor to determine which node enum function to call
void TreeUnwrapper::Visit(const verible::SyntaxTreeNode& node) {
  const auto tag = static_cast<NodeEnum>(node.Tag().tag);
  VLOG(3) << __FUNCTION__ << " node: " << tag;

  // Suppress additional indentation when at the syntax tree root and
  // when direct parent is `ifdef/`ifndef/`else/`elsif clause.
  const bool suppress_indentation =
      Context().empty() ||
      IsPreprocessorClause(NodeEnum(Context().top().Tag().tag));

  // Traversal control section.
  switch (tag) {
    // The following constructs are flushed-left, not indented:
    case NodeEnum::kPreprocessorIfdefClause:
    case NodeEnum::kPreprocessorIfndefClause:
    case NodeEnum::kPreprocessorElseClause:
    case NodeEnum::kPreprocessorElsifClause: {
      VisitNewUnindentedUnwrappedLine(node);
      break;
    }
    // The following constructs start a new UnwrappedLine indented to the
    // current level.
    case NodeEnum::kGenerateRegion:
    case NodeEnum::kConditionalGenerateConstruct:
    case NodeEnum::kLoopGenerateConstruct:
    case NodeEnum::kCaseStatement:
    case NodeEnum::kClassDeclaration:
    case NodeEnum::kClassConstructor:
    case NodeEnum::kPackageImportDeclaration:
    case NodeEnum::kPreprocessorInclude:
    case NodeEnum::kPreprocessorDefine:
    case NodeEnum::kPreprocessorUndef:
    case NodeEnum::kModuleDeclaration:
    case NodeEnum::kPackageDeclaration:
    case NodeEnum::kInterfaceDeclaration:
    case NodeEnum::kFunctionDeclaration:
    case NodeEnum::kTaskDeclaration:
    case NodeEnum::kTFPortDeclaration:
    case NodeEnum::kTypeDeclaration:
    case NodeEnum::kConstraintDeclaration:
    case NodeEnum::kConstraintExpression:
    case NodeEnum::kCovergroupDeclaration:
    case NodeEnum::kCoverageOption:
    case NodeEnum::kCoverageBin:
    case NodeEnum::kCoverPoint:
    case NodeEnum::kCoverCross:
    case NodeEnum::kBinsSelection:
    case NodeEnum::kDistributionItem:
    case NodeEnum::kCaseItem:
    case NodeEnum::kGenerateCaseItem:
    case NodeEnum::kDefaultItem:
    case NodeEnum::kStructUnionMember:
    case NodeEnum::kEnumName:
    case NodeEnum::kNetDeclaration:
    case NodeEnum::kModulePortDeclaration:
    case NodeEnum::kAlwaysStatement:
    case NodeEnum::kInitialStatement:
    case NodeEnum::kFinalStatement:
    case NodeEnum::kForInitialization:
    case NodeEnum::kForCondition:
    case NodeEnum::kForStepList:
    case NodeEnum::kRegisterVariable:
    case NodeEnum::kGateInstance:
    case NodeEnum::kActualNamedPort:
    case NodeEnum::kActualPositionalPort:
    case NodeEnum::kAssertionVariableDeclaration:
    case NodeEnum::kPortItem:
    case NodeEnum::kPropertyDeclaration:
    case NodeEnum::kSequenceDeclaration:
    case NodeEnum::kPortDeclaration:
    case NodeEnum::kParamDeclaration:
    case NodeEnum::kForwardDeclaration: {
      VisitNewUnwrappedLine(node);
      break;
    }

      // For the following items, start a new unwrapped line only if they are
      // *direct* descendants of list elements.  This effectively suppresses
      // starting a new line when single statements are found to extend other
      // statements, delayed assignments, single-statement if/for loops.
    case NodeEnum::kMacroCall:
    case NodeEnum::kForLoopStatement:
    case NodeEnum::kForeverLoopStatement:
    case NodeEnum::kRepeatLoopStatement:
    case NodeEnum::kWhileLoopStatement:
    case NodeEnum::kDoWhileLoopStatement:
    case NodeEnum::kForeachLoopStatement:
    case NodeEnum::kConditionalStatement:
    case NodeEnum::kStatement:
    case NodeEnum::kLabeledStatement:  // e.g. foo_label : do_something();
    case NodeEnum::kJumpStatement:
    case NodeEnum::kContinuousAssign:               // e.g. assign a=0, b=2;
    case NodeEnum::kContinuousAssignmentStatement:  // e.g. x=y
    case NodeEnum::kBlockingAssignmentStatement:    // id=expr
    case NodeEnum::kNonblockingAssignmentStatement:  // dest <= src;
    case NodeEnum::kAssignmentStatement:            // id=expr
    case NodeEnum::kProceduralTimingControlStatement: {
      if (Context().DirectParentIsOneOf(
              {NodeEnum::kStatementList, NodeEnum::kModuleItemList,
               NodeEnum::kClassItems, NodeEnum::kBlockItemStatementList})) {
        VisitNewUnwrappedLine(node);
      } else {
        // Otherwise extend previous token partition.
        TraverseChildren(node);
      }
      break;
    }

    // Add a level of grouping without indentation.
    // This should include most declaration headers.
    case NodeEnum::kFunctionHeader:
    case NodeEnum::kClassConstructorPrototype:
    case NodeEnum::kTaskHeader:
    case NodeEnum::kBindDirective:
    case NodeEnum::kDataDeclaration:
    case NodeEnum::kLoopHeader:
    case NodeEnum::kClassHeader:
    case NodeEnum::kModuleHeader: {
      VisitIndentedSection(node, 0, PartitionPolicyEnum::kFitOnLineElseExpand);
      break;
    }

    // Add an additional level of indentation.
    case NodeEnum::kClassItems:
    case NodeEnum::kModuleItemList:
    case NodeEnum::kPackageItemList:
    case NodeEnum::kInterfaceClassDeclaration:
    case NodeEnum::kGenerateItemList:
    case NodeEnum::kCaseItemList:
    case NodeEnum::kGenerateCaseItemList:
    case NodeEnum::kStructUnionMemberList:
    case NodeEnum::kEnumNameList:
    case NodeEnum::kConstraintBlockItemList:
    case NodeEnum::kConstraintExpressionList:
    case NodeEnum::kDistributionItemList:
    case NodeEnum::kBlockItemStatementList:
    case NodeEnum::kFunctionItemList:
    case NodeEnum::kAssertionVariableDeclarationList:
    // The final sequence_expr of a sequence_declaration is same indentation
    // level as the kAssertionVariableDeclarationList that precedes it.
    case NodeEnum::kSequenceDeclarationFinalExpr:
    case NodeEnum::kCoverageSpecOptionList:
    case NodeEnum::kBinOptionList:
    case NodeEnum::kCrossBodyItemList:
    case NodeEnum::kStatementList: {
      if (suppress_indentation) {
        // Do not further indent preprocessor clauses.
        // Maintain same level as before.
        TraverseChildren(node);
      } else {
        VisitIndentedSection(node, style_.indentation_spaces,
                             PartitionPolicyEnum::kAlwaysExpand);
      }
      break;
    }

      // Add a level of grouping that is treated as wrapping.
    case NodeEnum::kForSpec:
    case NodeEnum::kGateInstanceRegisterVariableList:
    case NodeEnum::kPortActualList:
    case NodeEnum::kFormalParameterList:
    case NodeEnum::kPortList:
    case NodeEnum::kPortDeclarationList: {
      if (suppress_indentation) {
        // Do not further indent preprocessor clauses.
        // Maintain same level as before.
        TraverseChildren(node);
      } else {
        VisitIndentedSection(node, style_.wrap_spaces,
                             PartitionPolicyEnum::kFitOnLineElseExpand);
      }
      break;
    }

      // Special cases:

    case NodeEnum::kPropertySpec: {
      if (Context().IsInside(NodeEnum::kPropertyDeclaration)) {
        // indent the same level as kAssertionVariableDeclarationList
        // which (optionally) appears before the final property_spec.
        VisitIndentedSection(node, style_.indentation_spaces,
                             PartitionPolicyEnum::kAlwaysExpand);
      } else {
        TraverseChildren(node);
      }
      break;
    }

    default: {
      TraverseChildren(node);
    }
  }

  // post-traversal token partition adjustments
  switch (tag) {
    case NodeEnum::kDataDeclaration:
    case NodeEnum::kBindDirective: {
      // Attach the trailing ';' partition to the previous sibling leaf.
      // VisitIndentedSection() finished by starting a new partition,
      // so we need to back-track to the previous sibling partition.
      auto* recent_partition = CurrentTokenPartition()->PreviousSibling();
      if (recent_partition != nullptr) {
        verible::MoveLastLeafIntoPreviousSibling(recent_partition);
      }
      break;
    }
    case NodeEnum::kBegin: {  // may contain label
      // If begin is part of a conditional or for-loop block, attach it to the
      // end of the previous header, right after the close-paren.
      // e.g. "for (...; ...; ...) begin : label"
      if ((Context().DirectParentsAre(
              {NodeEnum::kSeqBlock, NodeEnum::kForLoopStatement})) ||
          (Context().DirectParentsAre(
              {NodeEnum::kGenerateBlock, NodeEnum::kLoopGenerateConstruct}))) {
        // TODO(b/142684459): same for if-statement case, and other situations
        // where we want 'begin' to continue a previous partition.
        verible::MoveLastLeafIntoPreviousSibling(
            CurrentTokenPartition()->Root());
      }
      // close-out current token partition?
      break;
    }
    default:
      break;
  }

  VLOG(3) << "end of " << __FUNCTION__ << " node: " << tag;
}

// Visitor to determine which leaf enum function to call
void TreeUnwrapper::Visit(const verible::SyntaxTreeLeaf& leaf) {
  const yytokentype tag = yytokentype(leaf.Tag().tag);
  const auto symbol_name = verilog_symbol_name(tag);  // only needed for debug
  VLOG(3) << __FUNCTION__ << " leaf: " << symbol_name;

  switch (tag) {
    case yytokentype::PP_endif: {
      StartNewUnwrappedLine();
      CurrentUnwrappedLine().SetIndentationSpaces(0);
      break;
    }
    // TODO(fangism): restructure CST so that optional end labels can be
    // handled as being part of a single node.
    case yytokentype::TK_end:
    case yytokentype::TK_endcase:
    case yytokentype::TK_endgroup:
    case yytokentype::TK_endpackage:
    case yytokentype::TK_endgenerate:
    case yytokentype::TK_endinterface:
    case yytokentype::TK_endfunction:
    case yytokentype::TK_endtask:
    case yytokentype::TK_endproperty:
    case yytokentype::TK_endclass:
    case yytokentype::TK_endmodule: {
      StartNewUnwrappedLine();
      break;
    }
    default:
      // In most other cases, do nothing.
      break;
  }

  // Catch up on any non-whitespace tokens that fall between the syntax tree
  // leaves, such as comments and attributes, stopping at the current leaf.
  CatchUpToCurrentLeaf(leaf.get());

  // Sanity check that NextUnfilteredToken() is aligned to the current leaf.
  CHECK_EQ(NextUnfilteredToken()->text.begin(), leaf.get().text.begin());

  // Advances NextUnfilteredToken(), and extends CurrentUnwrappedLine().
  // Should be equivalent to AdvanceNextUnfilteredToken().
  AddTokenToCurrentUnwrappedLine();

  // Look-ahead to any trailing comments that are associated with this leaf,
  // up to a newline.
  LookAheadBeyondCurrentLeaf();

  VLOG(3) << "end of " << __FUNCTION__ << " leaf: " << symbol_name;
}

// Specialized node visitors

void TreeUnwrapper::VisitNewUnwrappedLine(const verible::SyntaxTreeNode& node) {
  StartNewUnwrappedLine();
  TraverseChildren(node);
}

void TreeUnwrapper::VisitNewUnindentedUnwrappedLine(
    const verible::SyntaxTreeNode& node) {
  StartNewUnwrappedLine();
  // Force the current line to be unindented without losing track of where
  // the current indentation level is for children.
  CurrentUnwrappedLine().SetIndentationSpaces(0);
  TraverseChildren(node);
}

}  // namespace formatter
}  // namespace verilog
