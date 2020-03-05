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

#include "verilog/formatting/tree_unwrapper.h"

#include <algorithm>
#include <iterator>
#include <vector>

#include "absl/base/macros.h"
#include "absl/strings/match.h"
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
#include "common/util/enum_flags.h"
#include "common/util/logging.h"
#include "common/util/value_saver.h"
#include "verilog/CST/macro.h"
#include "verilog/CST/verilog_nonterminals.h"
#include "verilog/formatting/verilog_token.h"
#include "verilog/parser/verilog_parser.h"
#include "verilog/parser/verilog_token_classifications.h"
#include "verilog/parser/verilog_token_enum.h"

namespace verilog {
namespace formatter {

using ::verible::PartitionPolicyEnum;
using ::verible::PreFormatToken;
using ::verible::TokenInfo;
using ::verible::TokenWithContext;
using ::verible::UnwrappedLine;
using ::verilog::IsComment;

// Used to filter the TokenStreamView by discarding space-only tokens.
static bool KeepNonWhitespace(const TokenInfo& t) {
  if (t.token_enum == verible::TK_EOF) return false;  // omit the EOF token
  return !IsWhitespace(verilog_tokentype(t.token_enum));
}

// Creates a PreFormatToken given a TokenInfo and returns it
static PreFormatToken CreateFormatToken(const TokenInfo& token) {
  PreFormatToken format_token(&token);
  format_token.format_token_enum =
      GetFormatTokenType(verilog_tokentype(format_token.TokenEnum()));
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

// Represents a state of scanning tokens between syntax tree leaves.
enum TokenScannerState {
  // Initial state: immediately after a leaf token
  kStart,

  // While encountering any string of consecutive newlines
  kHaveNewline,

  // Transition from newline to non-newline
  kNewPartition,

  // Reached next leaf token, stop.  Preserve existing newline.
  kEndWithNewline,

  // Reached next leaf token, stop.
  kEndNoNewline,
};

static const std::initializer_list<
    std::pair<const absl::string_view, TokenScannerState>>
    kTokenScannerStateStringMap = {
        {"kStart", TokenScannerState::kStart},
        {"kHaveNewline", TokenScannerState::kHaveNewline},
        {"kNewPartition", TokenScannerState::kNewPartition},
        {"kEndWithNewline", TokenScannerState::kEndWithNewline},
        {"kEndNoNewline", TokenScannerState::kEndNoNewline},
};

// Conventional stream printer (declared in header providing enum).
std::ostream& operator<<(std::ostream& stream, TokenScannerState p) {
  static const auto* flag_map =
      verible::MakeEnumToStringMap(kTokenScannerStateStringMap);
  return stream << flag_map->find(p)->second;
}

// This finite state machine class is used to determine the placement of
// non-whitespace and non-syntax-tree-node tokens such as comments in
// UnwrappedLines.  The input to this FSM is a Verilog-specific token enum.
// This class is an internal implementation detail of the TreeUnwrapper class.
// TODO(fangism): handle attributes.
class TreeUnwrapper::TokenScanner {
 public:
  TokenScanner() = default;

  // Deleted standard interfaces:
  TokenScanner(const TokenScanner&) = delete;
  TokenScanner(TokenScanner&&) = delete;
  TokenScanner& operator=(const TokenScanner&) = delete;
  TokenScanner& operator=(TokenScanner&&) = delete;

  // Re-initializes state.
  void Reset() {
    current_state_ = State::kStart;
    seen_any_nonspace_ = false;
  }

  // Calls TransitionState using current_state_ and the tranition token_Type
  void UpdateState(verilog_tokentype token_type) {
    current_state_ = TransitionState(current_state_, token_type);
    seen_any_nonspace_ |= IsComment(token_type);
  }

  // Returns true if this is a state that should start a new token partition.
  bool ShouldStartNewPartition() const {
    return current_state_ == State::kNewPartition ||
           (current_state_ == State::kEndWithNewline && seen_any_nonspace_);
  }

 protected:
  typedef TokenScannerState State;

  // The current state of the TokenScanner.
  State current_state_ = kStart;
  bool seen_any_nonspace_ = false;

  // Transitions the TokenScanner given a TokenState and a verilog_tokentype
  // transition
  static State TransitionState(const State& old_state,
                               verilog_tokentype token_type);
};

static bool IsNewlineOrEOF(verilog_tokentype token_type) {
  return token_type == verilog_tokentype::TK_NEWLINE ||
         token_type == verible::TK_EOF;
}

/*
 * TransitionState computes the next state in the state machine given
 * a current TokenScanner::State and token_type transition.
 * This state machine is only expected to handle tokens that can appear
 * between syntax tree leaves (whitespace, comments, attributes).
 *
 * Transitions are expected to take place across 3 phases of inter-leaf
 * scanning:
 *   TreeUnwrapper::LookAheadBeyondCurrentLeaf()
 *   TreeUnwrapper::LookAheadBeyondCurrentNode()
 *   TreeUnwrapper::CatchUpToCurrentLeaf()
 */
TokenScannerState TreeUnwrapper::TokenScanner::TransitionState(
    const State& old_state, verilog_tokentype token_type) {
  VLOG(4) << "state transition on: " << old_state
          << ", token: " << verilog_symbol_name(token_type);
  State new_state = old_state;
  switch (old_state) {
    case kStart: {
      if (IsNewlineOrEOF(token_type)) {
        new_state = kHaveNewline;
      } else if (IsComment(token_type)) {
        new_state = kStart;  // same state
      } else {
        new_state = kEndNoNewline;
      }
      break;
    }
    case kHaveNewline: {
      if (IsNewlineOrEOF(token_type)) {
        new_state = kHaveNewline;  // same state
      } else if (IsComment(token_type)) {
        new_state = kNewPartition;
      } else {
        new_state = kEndWithNewline;
      }
      break;
    }
    case kNewPartition: {
      if (IsNewlineOrEOF(token_type)) {
        new_state = kHaveNewline;
      } else if (IsComment(token_type)) {
        new_state = kStart;
      } else {
        new_state = kEndNoNewline;
      }
      break;
    }
    case kEndWithNewline:
    case kEndNoNewline: {
      // Terminal states.  Must Reset() next.
      break;
    }
    default:
      break;
  }
  VLOG(4) << "new state: " << new_state;
  return new_state;
}

void TreeUnwrapper::EatSpaces() {
  // TODO(fangism): consider a NextNonSpaceToken() method that combines
  // NextUnfilteredToken with EatSpaces.
  const auto before = NextUnfilteredToken();
  SkipUnfilteredTokens(
      [](const TokenInfo& token) { return token.token_enum == TK_SPACE; });
  const auto after = NextUnfilteredToken();
  VLOG(4) << __FUNCTION__ << " ate " << std::distance(before, after)
          << " space tokens";
}

TreeUnwrapper::TreeUnwrapper(const verible::TextStructureView& view,
                             const verible::BasicFormatStyle& style,
                             const preformatted_tokens_type& ftokens)
    : verible::TreeUnwrapper(view, ftokens),
      style_(style),
      inter_leaf_scanner_(new TokenScanner),
      token_context_(FullText(), [](std::ostream& stream, int e) {
        stream << verilog_symbol_name(e);
      }) {
  // Verify that unfiltered token stream is properly EOF terminated,
  // so that stream scanners (inter_leaf_scanner_) know when to stop.
  const auto& tokens = view.TokenStream();
  CHECK(!tokens.empty());
  const auto& back(tokens.back());
  CHECK(back.isEOF());
  CHECK(back.text.empty());
}

TreeUnwrapper::~TreeUnwrapper() {}

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

static bool IsTopLevelListItem(const verible::SyntaxTreeContext& context) {
  return context.DirectParentIsOneOf(
      {NodeEnum::kStatementList, NodeEnum::kModuleItemList,
       NodeEnum::kGenerateItemList, NodeEnum::kClassItems,
       NodeEnum::kBlockItemStatementList, NodeEnum::kDescriptionList,
       NodeEnum::kPackageItemList});
}

// These are constructs where it is permissible to fit on one line, but in the
// event that the statement body is split, we need to ensure it is properly
// indented, even if it is a single statement.
// Keep this list in sync below where the same function name appears in comment.
static bool DirectParentIsFlowControlConstruct(
    const verible::SyntaxTreeContext& context) {
  return context.DirectParentIsOneOf({
      // LINT.IfChange(flow_control_parents)
      NodeEnum::kCaseStatement,         //
      NodeEnum::kRandCaseStatement,     //
      NodeEnum::kForLoopStatement,      //
      NodeEnum::kForeverLoopStatement,  //
      NodeEnum::kRepeatLoopStatement,   //
      NodeEnum::kWhileLoopStatement,    //
      NodeEnum::kDoWhileLoopStatement,  //
      NodeEnum::kForeachLoopStatement,  //
      NodeEnum::kConditionalStatement,
      // LINT.ThenChange(:flow_control_cases)
  });
}

void TreeUnwrapper::UpdateInterLeafScanner(verilog_tokentype token_type) {
  VLOG(4) << __FUNCTION__ << ", token: " << verilog_symbol_name(token_type);
  inter_leaf_scanner_->UpdateState(token_type);
  if (inter_leaf_scanner_->ShouldStartNewPartition()) {
    VLOG(4) << "new partition";
    StartNewUnwrappedLine();
  }
  VLOG(4) << "end of " << __FUNCTION__;
}

void TreeUnwrapper::AdvanceLastVisitedLeaf() {
  VLOG(4) << __FUNCTION__;
  EatSpaces();
  const auto& next_token = *NextUnfilteredToken();
  const verilog_tokentype token_enum = verilog_tokentype(next_token.token_enum);
  UpdateInterLeafScanner(token_enum);
  AdvanceNextUnfilteredToken();
  VLOG(4) << "end of " << __FUNCTION__;
}

verible::TokenWithContext TreeUnwrapper::VerboseToken(
    const verible::TokenInfo& token) const {
  return TokenWithContext{token, token_context_};
}

void TreeUnwrapper::CatchUpToCurrentLeaf(const verible::TokenInfo& leaf_token) {
  VLOG(4) << __FUNCTION__ << " to " << VerboseToken(leaf_token);
  // "Catch up" NextUnfilteredToken() to the current leaf.
  // Assigns non-whitespace tokens such as comments into UnwrappedLines.
  // Recall that SyntaxTreeLeaf has its own copy of TokenInfo, so we need to
  // compare a unique property of TokenInfo instead of its address.
  while (!NextUnfilteredToken()->isEOF()) {
    EatSpaces();
    // compare const char* addresses:
    if (NextUnfilteredToken()->text.begin() != leaf_token.text.begin()) {
      VLOG(4) << "token: " << VerboseToken(*NextUnfilteredToken());
      AdvanceLastVisitedLeaf();
    } else {
      break;
    }
  }
  VLOG(4) << "end of " << __FUNCTION__;
}

// Scan forward from the current leaf token, until some implementation-defined
// stopping condition.  This collects comments up until the first newline,
// and appends them to the most recent partition.
void TreeUnwrapper::LookAheadBeyondCurrentLeaf() {
  VLOG(4) << __FUNCTION__;
  // Re-initialize internal token-scanning state machine.
  inter_leaf_scanner_->Reset();

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
    VLOG(4) << "lookahead token: " << VerboseToken(*NextUnfilteredToken());
    if (IsComment(verilog_tokentype(NextUnfilteredToken()->token_enum))) {
      // TODO(fangism): or IsAttribute().  Basically, any token that is not
      // in the syntax tree and not a space.
      AdvanceLastVisitedLeaf();
    } else {  // including newline
      // Don't advance NextUnfilteredToken() in this case.
      VLOG(4) << "no advance";
      break;
    }
  }
  VLOG(4) << "end of " << __FUNCTION__;
}

// Scan forward up to a syntax tree leaf token, but return (possibly) the
// position of the last newline *before* that leaf token.
// This allows prefix comments and attributes to stick with their
// intended token that immediately follows.
static verible::TokenSequence::const_iterator StopAtLastNewlineBeforeTreeLeaf(
    const verible::TokenSequence::const_iterator token_begin,
    const TokenInfo::Context& context) {
  VLOG(4) << __FUNCTION__;
  auto token_iter = token_begin;
  auto last_newline = token_begin;
  bool have_last_newline = false;

  // Find next syntax tree token or EOF.
  bool break_while = false;
  while (!token_iter->isEOF() && !break_while) {
    VLOG(4) << "scan: " << TokenWithContext{*token_iter, context};
    switch (token_iter->token_enum) {
      // TODO(b/144653479): this token-case logic is redundant with other
      // places; plumb that through to here instead of replicating it.
      case TK_NEWLINE:
        have_last_newline = true;
        last_newline = token_iter;
        ABSL_FALLTHROUGH_INTENDED;
      case TK_SPACE:
      case TK_EOL_COMMENT:
      case TK_COMMENT_BLOCK:
      case TK_ATTRIBUTE:
        ++token_iter;
        break;
      default:
        break_while = true;
        break;
    }
  }

  const auto result = have_last_newline ? last_newline : token_iter;
  VLOG(4) << "end of " << __FUNCTION__ << ", advanced "
          << std::distance(token_begin, result) << " tokens";
  return result;
}

// Scan forward for comments between leaf tokens, and append them to a partition
// with the correct amount of indentation.
void TreeUnwrapper::LookAheadBeyondCurrentNode() {
  VLOG(4) << __FUNCTION__;
  // Scan until token is reached, or the last newline before token is reached.
  const auto token_begin = NextUnfilteredToken();
  const auto token_end =
      StopAtLastNewlineBeforeTreeLeaf(token_begin, token_context_);
  VLOG(4) << "stop before: " << VerboseToken(*token_end);
  while (NextUnfilteredToken() != token_end) {
    // Almost like AdvanceLastVisitedLeaf(), except suppress the last
    // advancement.
    EatSpaces();
    const auto next_token_iter = NextUnfilteredToken();
    const auto& next_token = *next_token_iter;
    VLOG(4) << "token: " << VerboseToken(next_token);
    const verilog_tokentype token_enum =
        verilog_tokentype(next_token.token_enum);
    UpdateInterLeafScanner(token_enum);
    if (next_token_iter != token_end) {
      AdvanceNextUnfilteredToken();
    } else {
      break;
    }
  }
  VLOG(4) << "end of " << __FUNCTION__;
}

// This hook is called between the children nodes of the handled node types.
// This is what allows partitions containing only comments to be properly
// indented to the same level that non-comment sub-partitions would.
void TreeUnwrapper::InterChildNodeHook(const verible::SyntaxTreeNode& node) {
  const auto tag = NodeEnum(node.Tag().tag);
  VLOG(4) << __FUNCTION__ << " node type: " << tag;
  switch (tag) {
    // TODO(fangism): cover all other major lists
    case NodeEnum::kPortDeclarationList:
    // case NodeEnum::kPortList:  // TODO(fangism): for task/function ports
    case NodeEnum::kModuleItemList:
    case NodeEnum::kGenerateItemList:
    case NodeEnum::kClassItems:
    case NodeEnum::kDescriptionList:  // top-level item comments
    case NodeEnum::kStatementList:
    case NodeEnum::kPackageItemList:
    case NodeEnum::kBlockItemStatementList:
      LookAheadBeyondCurrentNode();
      break;
    default:
      break;
  }
  VLOG(4) << "end of " << __FUNCTION__ << " node type: " << tag;
}

void TreeUnwrapper::CollectLeadingFilteredTokens() {
  VLOG(4) << __FUNCTION__;
  // inter_leaf_scanner_ is already initialized to kStart state
  LookAheadBeyondCurrentNode();
  VLOG(4) << "end of " << __FUNCTION__;
}

void TreeUnwrapper::CollectTrailingFilteredTokens() {
  VLOG(4) << __FUNCTION__;
  // This should emulate ::Visit(leaf == EOF token).

  // A newline means there are no comments to add to this UnwrappedLine
  // TODO(fangism): fold this logic into CatchUpToCurrentLeaf()
  if (IsNewlineOrEOF(verilog_tokentype(NextUnfilteredToken()->token_enum))) {
    StartNewUnwrappedLine();
  }

  // "Catch up" to EOF.
  // The very last unfiltered token scanned should be the EOF.
  // The byte offset of the EOF token is used as the stop-condition.
  CatchUpToCurrentLeaf(EOFToken());
  VLOG(4) << "end of " << __FUNCTION__;
}

// Visitor to determine which node enum function to call
void TreeUnwrapper::Visit(const verible::SyntaxTreeNode& node) {
  const auto tag = static_cast<NodeEnum>(node.Tag().tag);
  VLOG(3) << __FUNCTION__ << " node: " << tag;

  // This phase is only concerned with creating token partitions (during tree
  // recursive descent) and setting correct indentation values.  It is ok to
  // have excessive partitioning during this phase.
  SetIndentationsAndCreatePartitions(node);

  // This phase is only concerned with reshaping operations on token partitions,
  // such as merging, flattening, hoisting.  Reshaping should only occur on the
  // return path of tree traversal (here).
  ReshapeTokenPartitions(node);
}

// CST-descending phase that creates partitions with correct indentation.
void TreeUnwrapper::SetIndentationsAndCreatePartitions(
    const verible::SyntaxTreeNode& node) {
  const auto tag = static_cast<NodeEnum>(node.Tag().tag);
  VLOG(3) << __FUNCTION__ << " node: " << tag;

  // Suppress additional indentation when:
  // - at the syntax tree root
  // - direct parent is `ifdef/`ifndef/`else/`elsif clause
  // - at the first level of macro argument expansion
  const bool suppress_indentation =
      Context().empty() ||
      IsPreprocessorClause(NodeEnum(Context().top().Tag().tag)) ||
      Context().DirectParentIs(NodeEnum::kMacroArgList);

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
    case NodeEnum::kCaseGenerateConstruct:
    case NodeEnum::kLoopGenerateConstruct:
    case NodeEnum::kClassDeclaration:
    case NodeEnum::kClassConstructor:
    case NodeEnum::kPackageImportDeclaration:
    case NodeEnum::kDPIImportItem:
    // TODO(fangism): case NodeEnum::kDPIExportItem:
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
    case NodeEnum::kStructUnionMember:
    case NodeEnum::kEnumName:
    case NodeEnum::kNetDeclaration:
    case NodeEnum::kModulePortDeclaration:
    case NodeEnum::kAlwaysStatement:
    case NodeEnum::kInitialStatement:
    case NodeEnum::kFinalStatement:
    case NodeEnum::kDisableStatement:
    case NodeEnum::kForInitialization:
    case NodeEnum::kForCondition:
    case NodeEnum::kForStepList:
    case NodeEnum::kParBlock:
    case NodeEnum::kParamByName:
    case NodeEnum::kActualNamedPort:
    case NodeEnum::kActualPositionalPort:
    case NodeEnum::kAssertionVariableDeclaration:
    case NodeEnum::kPort:
    case NodeEnum::kPortItem:
    case NodeEnum::kPropertyDeclaration:
    case NodeEnum::kSequenceDeclaration:
    case NodeEnum::kPortDeclaration:
    case NodeEnum::kParamDeclaration:
    case NodeEnum::kClockingDeclaration:
    case NodeEnum::kClockingItem:
    case NodeEnum::kGenvarDeclaration:
    case NodeEnum::kDescriptionList:
    case NodeEnum::kForwardDeclaration: {
      VisitNewUnwrappedLine(node);
      break;
    }
    case NodeEnum::kConditionalGenerateConstruct: {
      if (IsTopLevelListItem(Context())) {
        // Create a level of grouping without additional indentation.
        VisitIndentedSection(node, 0,
                             PartitionPolicyEnum::kFitOnLineElseExpand);
      } else {
        TraverseChildren(node);
      }
      break;
    }
    case NodeEnum::kSeqBlock:
      if (Context().DirectParentsAre(
              {NodeEnum::kBlockItemStatementList, NodeEnum::kParBlock})) {
        // begin inside fork
        VisitNewUnwrappedLine(node);
      } else {
        TraverseChildren(node);
      }
      break;

    // For the following items, start a new partition group (no additional
    // indentation) only if they are *direct* descendants of list elements.
    // This effectively suppresses starting a new partition when single
    // statements are found to extend other statements, delayed assignments,
    // single-statement if/for loops.
    // Keep this group of cases in sync with (earlier in this file):
    // DirectParentIsFlowControlConstruct()
    // LINT.IfChange(flow_control_cases)
    case NodeEnum::kCaseStatement:
    case NodeEnum::kRandCaseStatement:
    case NodeEnum::kForLoopStatement:
    case NodeEnum::kForeverLoopStatement:
    case NodeEnum::kRepeatLoopStatement:
    case NodeEnum::kWhileLoopStatement:
    case NodeEnum::kDoWhileLoopStatement:
    case NodeEnum::kForeachLoopStatement:
    case NodeEnum::kConditionalStatement:
      // LINT.ThenChange(:flow_control_parents)
      {
        if (IsTopLevelListItem(Context())) {
          // Create a level of grouping without additional indentation.
          VisitIndentedSection(node, 0,
                               PartitionPolicyEnum::kFitOnLineElseExpand);
        } else {
          TraverseChildren(node);
        }
        break;
      }

    case NodeEnum::kIfClause:
    case NodeEnum::kIfBody: {
      const verible::SyntaxTreeNode* subnode;

      if (tag == NodeEnum::kIfClause)
        subnode = &GetSubtreeAsNode(node, NodeEnum::kIfClause, 1);
      else
        subnode = &node;

      if (GetSubtreeNodeEnum(*subnode, NodeEnum::kIfBody, 0) ==
          NodeEnum::kSeqBlock) {
        // Extend current token partition with following begin keyword,
        // kBlockItemStatementList would create indented section anyway
        TraverseChildren(node);
      } else {
        // Otherwise create indented section by ourselves.
        // It's for single-statement branches
        if (tag == NodeEnum::kIfBody) {
          // indent if-branch
          VisitIndentedSection(node, style_.indentation_spaces,
                               PartitionPolicyEnum::kFitOnLineElseExpand);
        } else {
          VisitIndentedSection(node, 0,
                               PartitionPolicyEnum::kFitOnLineElseExpand);
        }
      }
      break;
    }

    case NodeEnum::kElseClause:
    case NodeEnum::kElseBody: {
      const verible::SyntaxTreeNode* subnode;

      if (tag == NodeEnum::kElseClause)
        subnode = &GetSubtreeAsNode(node, NodeEnum::kElseClause, 1);
      else
        subnode = &node;

      const auto subtag = GetSubtreeNodeEnum(*subnode, NodeEnum::kElseBody, 0);
      switch (subtag) {
        case NodeEnum::kSeqBlock:
        case NodeEnum::kConditionalStatement:
          if (std::prev(CurrentFormatTokenIterator())->TokenEnum() ==
              verilog_tokentype::SymbolIdentifier) {
            // Start new token partition if current token is label identifier
            VisitNewUnwrappedLine(node);
          } else {
            // Extend current token partition by begin keyword and if keyword
            // Plus keep else-if-else... structure flat
            TraverseChildren(node);
          }
          break;

        default:
          // Otherwise create indented section by ourselves.
          // It's for single-statement branches
          if (tag == NodeEnum::kElseBody) {
            // indent else-branch statement
            VisitIndentedSection(node, style_.indentation_spaces,
                                 PartitionPolicyEnum::kFitOnLineElseExpand);
          } else {
            VisitIndentedSection(node, 0,
                                 PartitionPolicyEnum::kFitOnLineElseExpand);
          }
          break;
      }
      break;
    }
    case NodeEnum::kGenerateIfClause:
    case NodeEnum::kGenerateIfBody: {
      const verible::SyntaxTreeNode* subnode;

      if (tag == NodeEnum::kGenerateIfClause)
        subnode = &GetSubtreeAsNode(node, NodeEnum::kGenerateIfClause, 1);
      else
        subnode = &node;

      if (GetSubtreeNodeEnum(*subnode, NodeEnum::kGenerateIfBody, 0) ==
          NodeEnum::kGenerateBlock) {
        // Extend current token partition with following begin keyword,
        // kBlockItemStatementList would create indented section anyway
        TraverseChildren(node);
      } else {
        // Otherwise create indented section by ourselves.
        // It's for single-statement branches
        if (tag == NodeEnum::kGenerateIfBody) {
          // indent if-branch
          VisitIndentedSection(node, style_.indentation_spaces,
                               PartitionPolicyEnum::kFitOnLineElseExpand);
        } else {
          VisitIndentedSection(node, 0,
                               PartitionPolicyEnum::kFitOnLineElseExpand);
        }
      }
      break;
    }

    case NodeEnum::kGenerateElseClause:
    case NodeEnum::kGenerateElseBody: {
      const verible::SyntaxTreeNode* subnode;

      if (tag == NodeEnum::kGenerateElseClause)
        subnode = &GetSubtreeAsNode(node, NodeEnum::kGenerateElseClause, 1);
      else
        subnode = &node;

      const auto subtag =
          GetSubtreeNodeEnum(*subnode, NodeEnum::kGenerateElseBody, 0);
      switch (subtag) {
        case NodeEnum::kGenerateBlock:
        case NodeEnum::kConditionalGenerateConstruct:
          if (std::prev(CurrentFormatTokenIterator())->TokenEnum() ==
              verilog_tokentype::SymbolIdentifier) {
            // Start new token partition if current token is label identifier
            VisitNewUnwrappedLine(node);
          } else {
            // Extend current token partition by begin keyword and if keyword
            // Plus keep else-if-else... structure flat
            TraverseChildren(node);
          }
          break;

        default:
          // Otherwise create indented section by ourselves.
          // It's for single-statement branches
          if (tag == NodeEnum::kGenerateElseBody) {
            // indent else-branch statement
            VisitIndentedSection(node, style_.indentation_spaces,
                                 PartitionPolicyEnum::kFitOnLineElseExpand);
          } else {
            VisitIndentedSection(node, 0,
                                 PartitionPolicyEnum::kFitOnLineElseExpand);
          }
          break;
      }
      break;
    }

    // For the following items, start a new unwrapped line only if they are
    // *direct* descendants of list elements.  This effectively suppresses
    // starting a new line when single statements are found to extend other
    // statements, delayed assignments, single-statement if/for loops.
    case NodeEnum::kStatement:
    case NodeEnum::kLabeledStatement:  // e.g. foo_label : do_something();
    case NodeEnum::kJumpStatement:
    case NodeEnum::kWaitStatement:                   // wait(expr) ...
    case NodeEnum::kAssertionStatement:              // assert(expr);
    case NodeEnum::kContinuousAssign:                // e.g. assign a=0, b=2;
    case NodeEnum::kContinuousAssignmentStatement:   // e.g. x=y
    case NodeEnum::kBlockingAssignmentStatement:     // id=expr
    case NodeEnum::kNonblockingAssignmentStatement:  // dest <= src;
    case NodeEnum::kAssignmentStatement:             // id=expr
    case NodeEnum::kAssignModifyStatement:           // id+=expr
    case NodeEnum::kProceduralTimingControlStatement: {
      if (IsTopLevelListItem(Context())) {
        VisitIndentedSection(node, 0,
                             PartitionPolicyEnum::kFitOnLineElseExpand);
      } else if (DirectParentIsFlowControlConstruct(Context())) {
        // This is a single statement directly inside a flow-control construct,
        // and thus should be properly indented one level.
        VisitIndentedSection(node, style_.indentation_spaces,
                             PartitionPolicyEnum::kFitOnLineElseExpand);
      } else {
        // Otherwise extend previous token partition.
        TraverseChildren(node);
      }
      break;
    }
    case NodeEnum::kMacroArgList: {
      // Indentation at wrap level until proper partition policy implemented
      VisitIndentedSection(node, style_.wrap_spaces,
                           PartitionPolicyEnum::kFitOnLineElseExpand);
      break;
    }

    case NodeEnum::kMacroCall: {
      if (IsTopLevelListItem(Context())) {
        VisitIndentedSection(node, 0,
                             PartitionPolicyEnum::kFitOnLineElseExpand);
      } else if (DirectParentIsFlowControlConstruct(Context())) {
        // This is a single statement directly inside a flow-control construct,
        // and thus should be properly indented one level.
        VisitIndentedSection(node, style_.indentation_spaces,
                             PartitionPolicyEnum::kFitOnLineElseExpand);
      } else if (absl::StartsWith(GetMacroCallId(node).text, "`uvm_")) {
        // For each `uvm macro start a new unwrapped line
        VisitNewUnwrappedLine(node);
      } else {
        // Otherwise extend previous token partition.
        TraverseChildren(node);
      }
      break;
    }

    // Add a level of grouping without indentation.
    // This should include most declaration headers.
    case NodeEnum::kBindDirective:
    case NodeEnum::kDataDeclaration:
    case NodeEnum::kLoopHeader:
    case NodeEnum::kClassHeader:
    case NodeEnum::kCovergroupHeader:
    case NodeEnum::kModportDeclaration:
    case NodeEnum::kModuleHeader:
    case NodeEnum::kInstantiationType:
    case NodeEnum::kRegisterVariable:
    case NodeEnum::kVariableDeclarationAssignment:
    case NodeEnum::kCaseItem:
    case NodeEnum::kDefaultItem:
    case NodeEnum::kCaseInsideItem:
    case NodeEnum::kCasePatternItem:
    case NodeEnum::kGenerateCaseItem:
    case NodeEnum::kGateInstance: {
      VisitIndentedSection(node, 0, PartitionPolicyEnum::kFitOnLineElseExpand);
      break;
    }

    case NodeEnum::kClassConstructorPrototype:
    case NodeEnum::kTaskHeader:
    case NodeEnum::kFunctionHeader: {
      VisitIndentedSection(node, 0,
                           PartitionPolicyEnum::kAppendFittingSubPartitions);
      break;
    }

    // Add an additional level of indentation.
    case NodeEnum::kClassItems:
    case NodeEnum::kModuleItemList:
    case NodeEnum::kPackageItemList:
    case NodeEnum::kInterfaceClassDeclaration:
    case NodeEnum::kGenerateItemList:
    case NodeEnum::kCaseItemList:
    case NodeEnum::kCaseInsideItemList:
    case NodeEnum::kCasePatternItemList:
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
    case NodeEnum::kUdpBody:
    case NodeEnum::kUdpPortDeclaration:
    case NodeEnum::kUdpSequenceEntry:
    case NodeEnum::kUdpCombEntry:
    case NodeEnum::kStatementList:
    case NodeEnum::kClockingItemList: {
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
    case NodeEnum::kModportSimplePortsDeclaration:
    case NodeEnum::kModportTFPortsDeclaration:
    case NodeEnum::kGateInstanceRegisterVariableList:
    case NodeEnum::kVariableDeclarationAssignmentList:
    case NodeEnum::kPortActualList:  // TODO(b/146083526): one port per line
    case NodeEnum::kActualParameterByNameList:
    case NodeEnum::kPortList: {
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

    case NodeEnum::kPortDeclarationList:
    case NodeEnum::kFormalParameterList: {
      if (suppress_indentation) {
        // Do not further indent preprocessor clauses.
        // Maintain same level as before.
        TraverseChildren(node);
      } else if (Context().IsInside(NodeEnum::kClassHeader)) {
        VisitIndentedSection(node, style_.wrap_spaces,
                             PartitionPolicyEnum::kAlwaysExpand);
      } else if (Context().IsInside(NodeEnum::kInterfaceDeclaration) ||
                 Context().IsInside(NodeEnum::kModuleDeclaration)) {
        VisitIndentedSection(node, style_.wrap_spaces,
                             PartitionPolicyEnum::kAlwaysExpand);
      } else {
        VisitIndentedSection(node, style_.wrap_spaces,
                             PartitionPolicyEnum::kFitOnLineElseExpand);
      }
      break;
    }

      // Since NodeEnum::kActualParameterPositionalList consists of a
      // comma-separated list of expressions, we let those default to appending
      // to current token partition, and let the line-wrap-optimizer handle the
      // enclosing construct, such as a parameterized type like "foo #(1, 2)".

      // Special cases:

    case NodeEnum::kPropertySpec: {
      if (Context().IsInside(NodeEnum::kPropertyDeclaration)) {
        // indent the same level as kAssertionVariableDeclarationList
        // which (optionally) appears before the final property_spec.
        VisitIndentedSection(node, style_.indentation_spaces,
                             PartitionPolicyEnum::kAlwaysExpand);
      } else if (Context().DirectParentIs(NodeEnum::kMacroArgList)) {
        VisitIndentedSection(node, 0,
                             PartitionPolicyEnum::kFitOnLineElseExpand);
      } else {
        TraverseChildren(node);
      }
      break;
    }

    case NodeEnum::kExpression: {
      if (Context().DirectParentIs(NodeEnum::kMacroArgList)) {
        VisitNewUnwrappedLine(node);
      } else {
        TraverseChildren(node);
      }
      break;
    }

    default: {
      TraverseChildren(node);
    }
  }
}

// This phase is strictly concerned with reshaping token partitions,
// and occurs on the return path of partition tree construction.
void TreeUnwrapper::ReshapeTokenPartitions(
    const verible::SyntaxTreeNode& node) {
  const auto tag = static_cast<NodeEnum>(node.Tag().tag);
  VLOG(3) << __FUNCTION__ << " node: " << tag;
  // post-traversal token partition adjustments
  switch (tag) {
    // Note: this is also being applied to variable declaration lists,
    // which may want different handling than instantiations.
    case NodeEnum::kDataDeclaration: {
      AttachTrailingSemicolonToPreviousPartition();
      auto& data_declaration_partition =
          *ABSL_DIE_IF_NULL(CurrentTokenPartition()->PreviousSibling());
      auto& children = data_declaration_partition.Children();
      CHECK(!children.empty());

      // The instances/variable declaration list is always in last position.
      auto& instance_list_partition = children.back();
      if (instance_list_partition.BirthRank() == 0) {
        // This means there are no qualifiers and type was implicit,
        // so no partition precedes this.
        instance_list_partition.HoistOnlyChild();
      } else if (instance_list_partition.Children().size() == 1) {
        // Flatten partition tree by one level.
        instance_list_partition.HoistOnlyChild();

        // Undo the indentation of the only instance in the hoisted subtree.
        instance_list_partition.ApplyPreOrder([&](UnwrappedLine& uwline) {
          // If there were any unindented lines like preprocessing directives,
          // leave those unindented (without going negative).
          const int unindentation =
              std::max(uwline.IndentationSpaces() - style_.wrap_spaces, 0);
          uwline.SetIndentationSpaces(unindentation);
        });

        // Reshape type-instance partitions.

        const auto& instance_type_partition = children.back().PreviousSibling();

        if (instance_type_partition == &children.front()) {
          // data_declaration_partition now consists of exactly:
          //   instance_type_partition, instance_list_partition (single
          //   instance)

          // Compute end-of-type position before flattening (and invalidating
          // references).
          const size_t fuse_position =
              ABSL_DIE_IF_NULL(instance_type_partition)->Children().size() - 1;
          // Flatten these (which will invalidate their references).
          data_declaration_partition.FlattenOnce();

          // Join the rightmost of instance_type_partition with the leftmost of
          // instance_list_partition.  Keep the type's indentation.
          // This can yield an intermediate partition that contains:
          // ") instance_name (".
          data_declaration_partition.MergeConsecutiveSiblings(
              fuse_position, [](UnwrappedLine* left_uwline,
                                const UnwrappedLine& right_uwline) {
                CHECK(left_uwline->TokensRange().end() ==
                      right_uwline.TokensRange().begin());
                left_uwline->SpanUpToToken(right_uwline.TokensRange().end());
              });
        } else {
          // There is a qualifier before instance_type_partition, so we cannot
          // just flatten it. Manually flatten subpartitions.
          ABSL_DIE_IF_NULL(instance_type_partition)->HoistOnlyChild();
          instance_list_partition.HoistOnlyChild();
        }
      }
      break;
    }
    case NodeEnum::kClassConstructorPrototype:
    case NodeEnum::kTaskHeader:
    case NodeEnum::kFunctionHeader:
    case NodeEnum::kBindDirective: {
      AttachTrailingSemicolonToPreviousPartition();
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

        // Similar to verible::MoveLastLeafIntoPreviousSibling(), but
        // ensures that the CurrentTokenPartition() is updated to not reference
        // a potentially invalid removed partition.
        MergeLastTwoPartitions();
      }
      // else close-out current token partition?
      break;
    }
    case NodeEnum::kConditionalStatement: {
      CurrentTokenPartition()->Parent()->ApplyPreOrder(
          [](verible::VectorTree<UnwrappedLine>& node) {
            const auto& range = node.Value().TokensRange();
            if ((range.back().TokenEnum() == verilog_tokentype::TK_else) &&
                (range.end()->TokenEnum() == verilog_tokentype::TK_if)) {
              // Extend [if ( )] partition of [else] token
              node.NextLeaf()->Value().SpanPrevToken();
              // Update partition parent token range
              node.NextLeaf()->Parent()->Value().SpanPrevToken();
              // Delete unneeded [else] partition
              node.Parent()->Children().erase(
                  node.Parent()->Children().begin() + node.BirthRank());
            }
          });
      break;
    }
    case NodeEnum::kMacroCall: {
      if (IsTopLevelListItem(Context())) {
        auto* prev = CurrentTokenPartition()->PreviousSibling();
        if (prev != nullptr) prev->HoistOnlyChild();
      }
      break;
    }
    // In the following cases, forcibly close out the current partition.
    case NodeEnum::kPreprocessorDefine:
      StartNewUnwrappedLine();
      break;
    default:
      break;
  }
  VLOG(3) << "end of " << __FUNCTION__ << " node: " << tag;
}

void TreeUnwrapper::AttachTrailingSemicolonToPreviousPartition() {
  // Attach the trailing ';' partition to the previous sibling leaf.
  // VisitIndentedSection() finished by starting a new partition,
  // so we need to back-track to the previous sibling partition.
  auto* recent_partition = CurrentTokenPartition()->PreviousSibling();
  if (recent_partition != nullptr) {
    verible::MoveLastLeafIntoPreviousSibling(recent_partition);
  }
}

// Visitor to determine which leaf enum function to call
void TreeUnwrapper::Visit(const verible::SyntaxTreeLeaf& leaf) {
  VLOG(3) << __FUNCTION__ << " leaf: " << VerboseToken(leaf.get());
  const verilog_tokentype tag = verilog_tokentype(leaf.Tag().tag);

  // Catch up on any non-whitespace tokens that fall between the syntax tree
  // leaves, such as comments and attributes, stopping at the current leaf.
  CatchUpToCurrentLeaf(leaf.get());
  VLOG(4) << "Visit leaf: after CatchUp";

  // Possibly start new partition (without advancing token iterator).
  UpdateInterLeafScanner(tag);

  // Sanity check that NextUnfilteredToken() is aligned to the current leaf.
  CHECK_EQ(NextUnfilteredToken()->text.begin(), leaf.get().text.begin());

  // Start a new partition in the following cases.
  // In most other cases, do nothing.
  if (IsPreprocessorControlFlow(tag)) {
    StartNewUnwrappedLine();
    CurrentUnwrappedLine().SetIndentationSpaces(0);
  } else if (IsEndKeyword(tag)) {
    StartNewUnwrappedLine();
  } else if ((static_cast<int>(tag) == verilog_tokentype::MacroIdItem) &&
             absl::StartsWith(leaf.get().text, "`uvm")) {
    // For each `uvm macro start a new unwrapped line
    StartNewUnwrappedLine();
  }

  // Advances NextUnfilteredToken(), and extends CurrentUnwrappedLine().
  // Should be equivalent to AdvanceNextUnfilteredToken().
  AddTokenToCurrentUnwrappedLine();

  // Look-ahead to any trailing comments that are associated with this leaf,
  // up to a newline.
  LookAheadBeyondCurrentLeaf();

  // Post-token-handling token partition adjustments:
  switch (leaf.Tag().tag) {
    case verilog_tokentype::MacroIdItem: {
      if (absl::StartsWith(leaf.get().text, "`uvm_") &&
          absl::EndsWith(leaf.get().text, "_end")) {
        // Indent elements between UVM begin/end macro calls
        unsigned int uvm_level = 1;
        std::vector<verible::TokenPartitionTree*> uvm_range;

        // Search backwards for matching _begin.
        for (auto* itr = CurrentTokenPartition()->PreviousSibling(); itr;
             itr = itr->PreviousSibling()) {
          const auto macroId = itr->Value().TokensRange().begin()->Text();

          // Indent only uvm macros
          if (!absl::StartsWith(macroId, "`uvm_")) {
            continue;
          }

          // Count uvm indentation level
          if (absl::EndsWith(macroId, "_end")) {
            uvm_level++;
          } else if (absl::EndsWith(macroId, "_begin")) {
            uvm_level--;
          }

          // Break before indenting matching _begin macro
          if (uvm_level == 0) {
            break;
          }

          uvm_range.push_back(itr);
        }

        // Found matching _begin-_end macros
        if ((uvm_level == 0) && !uvm_range.empty()) {
          for (auto* itr : uvm_range) {
            // Indent uvm macros inside `uvm.*begin - `uvm.*end
            itr->ApplyPreOrder([&](verible::UnwrappedLine& line) {
              line.SetIndentationSpaces(line.IndentationSpaces() +
                                        style_.indentation_spaces);
            });
          }
        }
      }
      break;
    }
    case ',': {
      // In many cases (in particular lists), we want to attach delimiters like
      // ',' to the item that preceded it (on its rightmost leaf).
      // This adjustment is necessary for lists of element types that beget
      // their own indented sections.
      if (current_context_.DirectParentIsOneOf({
              // NodeEnum:xxxx                             // due to element:
              NodeEnum::kGateInstanceRegisterVariableList,  // kGateInstance,
                                                            // kRegisterVariable
              NodeEnum::kVariableDeclarationAssignmentList  // due to element:
              // kVariableDeclarationAssignment
          })) {
        MergeLastTwoPartitions();
      } else if (CurrentUnwrappedLine().Size() == 1) {
        // Partition would begin with a comma,
        // instead add this token to previous partition
        MergeLastTwoPartitions();
      }
      break;
    }
    case PP_else: {
      // Do not allow non-comment tokens on the same line as `else
      // (comments were handled above)
      StartNewUnwrappedLine();
      break;
    }
    default:
      break;
  }

  VLOG(3) << "end of " << __FUNCTION__ << " leaf: " << VerboseToken(leaf.get());
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
