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
#include "common/text/tree_utils.h"
#include "common/util/container_iterator_range.h"
#include "common/util/enum_flags.h"
#include "common/util/logging.h"
#include "common/util/value_saver.h"
#include "verilog/CST/declaration.h"
#include "verilog/CST/functions.h"
#include "verilog/CST/macro.h"
#include "verilog/CST/statement.h"
#include "verilog/CST/verilog_nonterminals.h"
#include "verilog/formatting/verilog_token.h"
#include "verilog/parser/verilog_parser.h"
#include "verilog/parser/verilog_token_classifications.h"
#include "verilog/parser/verilog_token_enum.h"

namespace verilog {
namespace formatter {

using ::verible::PartitionPolicyEnum;
using ::verible::PreFormatToken;
using ::verible::SyntaxTreeNode;
using ::verible::TokenInfo;
using ::verible::TokenPartitionTree;
using ::verible::TokenWithContext;
using ::verilog::IsComment;

// Used to filter the TokenStreamView by discarding space-only tokens.
static bool KeepNonWhitespace(const TokenInfo& t) {
  if (t.token_enum() == verible::TK_EOF) return false;  // omit the EOF token
  return !IsWhitespace(verilog_tokentype(t.token_enum()));
}

static bool NodeIsBeginEndBlock(const SyntaxTreeNode& node) {
  return node.MatchesTagAnyOf({NodeEnum::kSeqBlock, NodeEnum::kGenerateBlock});
}

static SyntaxTreeNode& GetBlockEnd(const SyntaxTreeNode& block) {
  CHECK(NodeIsBeginEndBlock(block));
  return verible::SymbolCastToNode(*block.children().back());
}

static const verible::Symbol* GetEndLabel(const SyntaxTreeNode& end_node) {
  return GetSubtreeAsSymbol(end_node, NodeEnum::kEnd, 1);
}

static bool NodeIsConditionalConstruct(const SyntaxTreeNode& node) {
  return node.MatchesTagAnyOf({NodeEnum::kConditionalStatement,
                               NodeEnum::kConditionalGenerateConstruct});
}

static bool NodeIsConditionalOrBlock(const SyntaxTreeNode& node) {
  return NodeIsBeginEndBlock(node) || NodeIsConditionalConstruct(node);
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

static const verible::EnumNameMap<TokenScannerState>
    kTokenScannerStateStringMap = {
        {"kStart", TokenScannerState::kStart},
        {"kHaveNewline", TokenScannerState::kHaveNewline},
        {"kNewPartition", TokenScannerState::kNewPartition},
        {"kEndWithNewline", TokenScannerState::kEndWithNewline},
        {"kEndNoNewline", TokenScannerState::kEndNoNewline},
};

// Conventional stream printer (declared in header providing enum).
std::ostream& operator<<(std::ostream& stream, TokenScannerState p) {
  return kTokenScannerStateStringMap.Unparse(p, stream);
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
      [](const TokenInfo& token) { return token.token_enum() == TK_SPACE; });
  const auto after = NextUnfilteredToken();
  VLOG(4) << __FUNCTION__ << " ate " << std::distance(before, after)
          << " space tokens";
}

TreeUnwrapper::TreeUnwrapper(const verible::TextStructureView& view,
                             const FormatStyle& style,
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
  CHECK(back.text().empty());
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

// These are constructs where it is permissible to fit on one line, but in the
// event that the statement body is split, we need to ensure it is properly
// indented, even if it is a single statement.
// Keep this list in sync below where the same function name appears in comment.
static bool ShouldIndentRelativeToDirectParent(
    const verible::SyntaxTreeContext& context) {
  // TODO(fangism): flip logic to check that item/statement is *not* a direct
  // child of one of the exceptions: sequential/parallel/generate blocks.
  return context.DirectParentIsOneOf({
      //
      NodeEnum::kLoopGenerateConstruct,             //
      NodeEnum::kCaseStatement,                     //
      NodeEnum::kRandCaseStatement,                 //
      NodeEnum::kForLoopStatement,                  //
      NodeEnum::kForeverLoopStatement,              //
      NodeEnum::kRepeatLoopStatement,               //
      NodeEnum::kWhileLoopStatement,                //
      NodeEnum::kDoWhileLoopStatement,              //
      NodeEnum::kForeachLoopStatement,              //
      NodeEnum::kConditionalStatement,              //
      NodeEnum::kIfClause,                          //
      NodeEnum::kGenerateIfClause,                  //
      NodeEnum::kAssertionClause,                   //
      NodeEnum::kAssumeClause,                      //
      NodeEnum::kProceduralTimingControlStatement,  //
      NodeEnum::kCoverStatement,                    //
      NodeEnum::kAssertPropertyClause,              //
      NodeEnum::kAssumePropertyClause,              //
      NodeEnum::kExpectPropertyClause,              //
      NodeEnum::kCoverPropertyStatement,            //
      NodeEnum::kCoverSequenceStatement,            //
      NodeEnum::kWaitStatement,                     //
      NodeEnum::kInitialStatement,                  //
      NodeEnum::kAlwaysStatement,                   //
      NodeEnum::kFinalStatement,                    //
      // Do not further indent under kElseClause and kElseGenerateClause,
      // so that chained else-ifs remain flat.
  });
}

void TreeUnwrapper::UpdateInterLeafScanner(verilog_tokentype token_type) {
  VLOG(4) << __FUNCTION__ << ", token: " << verilog_symbol_name(token_type);
  inter_leaf_scanner_->UpdateState(token_type);
  if (inter_leaf_scanner_->ShouldStartNewPartition()) {
    VLOG(4) << "new partition";
    // interleaf-tokens like comments do not have corresponding syntax tree
    // nodes, so pass nullptr.
    StartNewUnwrappedLine(PartitionPolicyEnum::kFitOnLineElseExpand, nullptr);
  }
  VLOG(4) << "end of " << __FUNCTION__;
}

void TreeUnwrapper::AdvanceLastVisitedLeaf() {
  VLOG(4) << __FUNCTION__;
  EatSpaces();
  const auto& next_token = *NextUnfilteredToken();
  const verilog_tokentype token_enum =
      verilog_tokentype(next_token.token_enum());
  UpdateInterLeafScanner(token_enum);
  AdvanceNextUnfilteredToken();
  VLOG(4) << "end of " << __FUNCTION__;
}

verible::TokenWithContext TreeUnwrapper::VerboseToken(
    const TokenInfo& token) const {
  return TokenWithContext{token, token_context_};
}

void TreeUnwrapper::CatchUpToCurrentLeaf(const TokenInfo& leaf_token) {
  VLOG(4) << __FUNCTION__ << " to " << VerboseToken(leaf_token);
  // "Catch up" NextUnfilteredToken() to the current leaf.
  // Assigns non-whitespace tokens such as comments into UnwrappedLines.
  // Recall that SyntaxTreeLeaf has its own copy of TokenInfo, so we need to
  // compare a unique property of TokenInfo instead of its address.
  while (!NextUnfilteredToken()->isEOF()) {
    EatSpaces();
    // compare const char* addresses:
    if (NextUnfilteredToken()->text().begin() != leaf_token.text().begin()) {
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
    if (IsComment(verilog_tokentype(NextUnfilteredToken()->token_enum()))) {
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
    switch (token_iter->token_enum()) {
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
        verilog_tokentype(next_token.token_enum());
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
void TreeUnwrapper::InterChildNodeHook(const SyntaxTreeNode& node) {
  const auto tag = NodeEnum(node.Tag().tag);
  VLOG(4) << __FUNCTION__ << " node type: " << tag;
  switch (tag) {
    // TODO(fangism): cover all other major lists
    case NodeEnum::kFormalParameterList:
    case NodeEnum::kPortDeclarationList:
    case NodeEnum::kActualParameterByNameList:
    case NodeEnum::kPortActualList:
    // case NodeEnum::kPortList:  // TODO(fangism): for task/function ports
    case NodeEnum::kModuleItemList:
    case NodeEnum::kGenerateItemList:
    case NodeEnum::kClassItems:
    case NodeEnum::kEnumNameList:
    case NodeEnum::kDescriptionList:  // top-level item comments
    case NodeEnum::kStatementList:
    case NodeEnum::kPackageItemList:
    case NodeEnum::kSpecifyItemList:
    case NodeEnum::kBlockItemStatementList:
    case NodeEnum::kFunctionItemList:
    case NodeEnum::kCaseItemList:
    case NodeEnum::kCaseInsideItemList:
    case NodeEnum::kGenerateCaseItemList:
    case NodeEnum::kConstraintExpressionList:
    case NodeEnum::kConstraintBlockItemList:
    case NodeEnum::kDistributionItemList:
      LookAheadBeyondCurrentNode();
      break;
    default: {
      if (Context().DirectParentIs(NodeEnum::kMacroArgList)) {
        StartNewUnwrappedLine(PartitionPolicyEnum::kFitOnLineElseExpand, &node);
      }
      break;
    }
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
  if (IsNewlineOrEOF(verilog_tokentype(NextUnfilteredToken()->token_enum()))) {
    // Filtered tokens may include comments, which do not correspond to syntax
    // tree nodes, so pass nullptr.
    StartNewUnwrappedLine(PartitionPolicyEnum::kFitOnLineElseExpand, nullptr);
  }

  // "Catch up" to EOF.
  // The very last unfiltered token scanned should be the EOF.
  // The byte offset of the EOF token is used as the stop-condition.
  CatchUpToCurrentLeaf(EOFToken());
  VLOG(4) << "end of " << __FUNCTION__;
}

// Visitor to determine which node enum function to call
void TreeUnwrapper::Visit(const SyntaxTreeNode& node) {
  const auto tag = static_cast<NodeEnum>(node.Tag().tag);
  VLOG(3) << __FUNCTION__ << " node: " << tag;

  // This phase is only concerned with creating token partitions (during tree
  // recursive descent) and setting correct indentation values.  It is ok to
  // have excessive partitioning during this phase.
  SetIndentationsAndCreatePartitions(node);

  // This phase is only concerned with reshaping operations on token partitions,
  // such as merging, flattening, hoisting.  Reshaping should only occur on the
  // return path of tree traversal (here).

  auto* partition = CurrentTokenPartition()->PreviousSibling();
  if (partition != nullptr) {
    ReshapeTokenPartitions(node, style_, partition);
  }
}

// CST-descending phase that creates partitions with correct indentation.
void TreeUnwrapper::SetIndentationsAndCreatePartitions(
    const SyntaxTreeNode& node) {
  const auto tag = static_cast<NodeEnum>(node.Tag().tag);
  VLOG(3) << __FUNCTION__ << " node: " << tag;

  // Tips:
  // In addition to the handling based on the node enum,
  // indentation decisions can be based on the following:
  //   * Context() -- lookup upwards
  //   * node.children() substructure -- look downwards
  //     + prefer to use more robust CST functions that have abstracted
  //       away positional and structural details.
  //   * Localize examination of the above to 1 or 2 levels (up/down)
  //     where possible.
  //
  // Do not:
  //   * try to manipulate the partition structures; reshaping is left to a
  //     different phase, ReshapeTokenPartitions().

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
    case NodeEnum::kDescriptionList: {
      // top-level doesn't need to open a new partition level
      // This is because the TreeUnwrapper base class created this partition
      // already.
      // TODO(fangism): make this consistent with style of other cases
      // and call VisitIndentedSection(node, 0, kAlwaysExpand);
      VisitNewUnwrappedLine(node);
      break;
    }

    // Indent only when applying kAppendFittingSubPartitions in parents
    case NodeEnum::kArgumentList:
    case NodeEnum::kIdentifierList: {
      if (Context().DirectParentsAre(
              {NodeEnum::kParenGroup, NodeEnum::kRandomizeFunctionCall}) ||
          Context().DirectParentsAre(
              {NodeEnum::kParenGroup, NodeEnum::kFunctionCall}) ||
          Context().DirectParentsAre(
              {NodeEnum::kParenGroup,
               NodeEnum::kRandomizeMethodCallExtension}) ||
          Context().DirectParentsAre(
              {NodeEnum::kParenGroup, NodeEnum::kSystemTFCall}) ||
          Context().DirectParentsAre(
              {NodeEnum::kParenGroup, NodeEnum::kMethodCallExtension})) {
        // TODO(fangism): Using wrap_spaces because of poor support of
        //     function/system/method/random calls inside trailing assignments,
        //     if headers, ternary operators and so on
        VisitIndentedSection(node, style_.wrap_spaces,
                             PartitionPolicyEnum::kFitOnLineElseExpand);
      } else {
        TraverseChildren(node);
      }
      break;
    }

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
    case NodeEnum::kClassConstructor:
    case NodeEnum::kPackageImportDeclaration:
    // TODO(fangism): case NodeEnum::kDPIExportItem:
    case NodeEnum::kPreprocessorInclude:
    case NodeEnum::kPreprocessorUndef:
    case NodeEnum::kTFPortDeclaration:
    case NodeEnum::kTypeDeclaration:
    case NodeEnum::kNetTypeDeclaration:
    case NodeEnum::kForwardDeclaration:
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
    case NodeEnum::kSpecifyItem:
    case NodeEnum::kForInitialization:
    case NodeEnum::kForCondition:
    case NodeEnum::kForStepList:
    case NodeEnum::kParamByName:
    case NodeEnum::kActualNamedPort:
    case NodeEnum::kActualPositionalPort:
    case NodeEnum::kAssertionVariableDeclaration:
    case NodeEnum::kPortItem:
    case NodeEnum::kMacroFormalArg:
    case NodeEnum::kPropertyDeclaration:
    case NodeEnum::kSequenceDeclaration:
    case NodeEnum::kPort:
    case NodeEnum::kPortDeclaration:
    case NodeEnum::kParamDeclaration:
    case NodeEnum::kClockingDeclaration:
    case NodeEnum::kClockingItem:
    case NodeEnum::kUdpPrimitive:
    case NodeEnum::kGenvarDeclaration:
    case NodeEnum::kConditionalGenerateConstruct:

    case NodeEnum::kMacroGenericItem:
    case NodeEnum::kModuleHeader:
    case NodeEnum::kBindDirective:
    case NodeEnum::kDataDeclaration:
    case NodeEnum::kGateInstantiation:
    case NodeEnum::kLoopHeader:
    case NodeEnum::kCovergroupHeader:
    case NodeEnum::kModportDeclaration:
    case NodeEnum::kInstantiationType:
    case NodeEnum::kRegisterVariable:
    case NodeEnum::kVariableDeclarationAssignment:
    case NodeEnum::kCaseItem:
    case NodeEnum::kDefaultItem:
    case NodeEnum::kCaseInsideItem:
    case NodeEnum::kCasePatternItem:
    case NodeEnum::kGenerateCaseItem:
    case NodeEnum::kGateInstance:
    case NodeEnum::kGenerateIfClause:
    case NodeEnum::kGenerateElseClause:
    case NodeEnum::kGenerateIfHeader:
    case NodeEnum::kIfClause:
    case NodeEnum::kElseClause:
    case NodeEnum::kIfHeader:
    case NodeEnum::kAssertionClause:
    case NodeEnum::kAssumeClause:
    case NodeEnum::kAssertPropertyClause:
    case NodeEnum::kAssumePropertyClause:
    case NodeEnum::kExpectPropertyClause: {
      VisitIndentedSection(node, 0, PartitionPolicyEnum::kFitOnLineElseExpand);
      break;
    }

      // The following cases will always expand into their constituent
      // partitions:
    case NodeEnum::kModuleDeclaration:
    case NodeEnum::kProgramDeclaration:
    case NodeEnum::kPackageDeclaration:
    case NodeEnum::kInterfaceDeclaration:
    case NodeEnum::kFunctionDeclaration:
    case NodeEnum::kTaskDeclaration:
    case NodeEnum::kClassDeclaration:
    case NodeEnum::kClassHeader:
    case NodeEnum::kBegin:
    case NodeEnum::kEnd:
    // case NodeEnum::kFork:  // TODO(fangism): introduce this node enum
    // case NodeEnum::kJoin:  // TODO(fangism): introduce this node enum
    case NodeEnum::kParBlock:
    case NodeEnum::kSeqBlock:
    case NodeEnum::kGenerateBlock: {
      VisitIndentedSection(node, 0, PartitionPolicyEnum::kAlwaysExpand);
      break;
    }

    // The following set of cases are related to flow-control (loops and
    // conditionals) for statements and generate items:
    case NodeEnum::kAssertionBody:
    case NodeEnum::kAssumeBody:
    case NodeEnum::kCoverBody:
    case NodeEnum::kAssertPropertyBody:
    case NodeEnum::kAssumePropertyBody:
    case NodeEnum::kExpectPropertyBody:
    case NodeEnum::kCoverPropertyBody:
    case NodeEnum::kCoverSequenceBody:
    case NodeEnum::kWaitBody:
    case NodeEnum::kGenerateIfBody:
    case NodeEnum::kIfBody: {
      // In the case of if-begin, let the 'begin'-'end' block indent its own
      // section.  Othewise for single statements/items, indent here.
      const auto* subnode =
          verible::CheckOptionalSymbolAsNode(GetSubtreeAsSymbol(node, tag, 0));
      const auto next_indent =
          (subnode != nullptr && NodeIsBeginEndBlock(*subnode))
              ? 0
              : style_.indentation_spaces;
      VisitIndentedSection(node, next_indent,
                           PartitionPolicyEnum::kFitOnLineElseExpand);
      break;
    }
    case NodeEnum::kGenerateElseBody:
    case NodeEnum::kElseBody: {
      // In the case of else-begin, let the 'begin'-'end' block indent its own
      // section, otherwise, indent single statements here.
      // In the case of else-if, suppress further indentation, deferring to
      // the conditional construct.
      const auto& subnode = GetSubtreeAsNode(node, tag, 0);
      const auto next_indent =
          NodeIsConditionalOrBlock(subnode) ? 0 : style_.indentation_spaces;
      VisitIndentedSection(node, next_indent,
                           PartitionPolicyEnum::kFitOnLineElseExpand);
      break;
    }

    // For the following items, start a new unwrapped line only if they are
    // *direct* descendants of list elements.  This effectively suppresses
    // starting a new line when single statements are found to extend other
    // statements, delayed assignments, single-statement if/for loops.
    // search-anchor: STATEMENT_TYPES
    case NodeEnum::kNullItem:       // ;
    case NodeEnum::kNullStatement:  // ;
    case NodeEnum::kStatement:
    case NodeEnum::kLabeledStatement:  // e.g. foo_label : do_something();
    case NodeEnum::kJumpStatement:
    case NodeEnum::kWaitStatement:       // wait(expr) ...
    case NodeEnum::kWaitForkStatement:   // wait fork;
    case NodeEnum::kAssertionStatement:  // assert(expr);
    case NodeEnum::kAssumeStatement:     // assume(expr);
    case NodeEnum::kCoverStatement:      // cover(expr);
    case NodeEnum::kAssertPropertyStatement:
    case NodeEnum::kAssumePropertyStatement:
    case NodeEnum::kExpectPropertyStatement:
    case NodeEnum::kCoverPropertyStatement:
    case NodeEnum::kCoverSequenceStatement:
      // TODO(fangism): case NodeEnum::kRestrictPropertyStatement:
    case NodeEnum::kContinuousAssignmentStatement:  // e.g. assign x=y;
    case NodeEnum::kProceduralContinuousAssignmentStatement:  // e.g. assign
                                                              // x=y;
    case NodeEnum::kProceduralContinuousDeassignmentStatement:
    case NodeEnum::kProceduralContinuousForceStatement:
    case NodeEnum::kProceduralContinuousReleaseStatement:
    case NodeEnum::kNetVariableAssignment:           // e.g. x=y
    case NodeEnum::kBlockingAssignmentStatement:     // id=expr
    case NodeEnum::kNonblockingAssignmentStatement:  // dest <= src;
    case NodeEnum::kAssignModifyStatement:           // id+=expr
    case NodeEnum::kIncrementDecrementExpression:    // --y
    case NodeEnum::kProceduralTimingControlStatement:

      // various flow control constructs
    case NodeEnum::kCaseStatement:
    case NodeEnum::kRandCaseStatement:
    case NodeEnum::kForLoopStatement:
    case NodeEnum::kForeverLoopStatement:
    case NodeEnum::kRepeatLoopStatement:
    case NodeEnum::kWhileLoopStatement:
    case NodeEnum::kDoWhileLoopStatement:
    case NodeEnum::kForeachLoopStatement:
    case NodeEnum::kConditionalStatement:  //
    {
      // Single statements directly inside a flow-control construct
      // should be properly indented one level.
      const int indent = ShouldIndentRelativeToDirectParent(Context())
                             ? style_.indentation_spaces
                             : 0;
      VisitIndentedSection(node, indent,
                           PartitionPolicyEnum::kFitOnLineElseExpand);
      break;
    }

    case NodeEnum::kReferenceCallBase: {
      // TODO(fangism): Create own section only for standalone calls
      if (Context().DirectParentIs(NodeEnum::kStatement)) {
        const auto& subnode = verible::SymbolCastToNode(
            *ABSL_DIE_IF_NULL(node.children().back()));
        if (subnode.MatchesTag(NodeEnum::kRandomizeMethodCallExtension) &&
            subnode.children().back() != nullptr) {
          // TODO(fangism): Handle constriants
          VisitIndentedSection(node, 0, PartitionPolicyEnum::kAlwaysExpand);
        } else if (subnode.MatchesTagAnyOf(
                       {NodeEnum::kMethodCallExtension,
                        NodeEnum::kRandomizeMethodCallExtension,
                        NodeEnum::kFunctionCall})) {
          VisitIndentedSection(
              node, 0, PartitionPolicyEnum::kAppendFittingSubPartitions);
        } else {
          TraverseChildren(node);
        }
      } else {
        TraverseChildren(node);
      }
      break;
    }

    case NodeEnum::kRandomizeFunctionCall: {
      // TODO(fangism): Create own section only for standalone calls
      if (Context().DirectParentIs(NodeEnum::kStatement)) {
        if (node.children().back() != nullptr) {
          // TODO(fangism): Handle constriants
          VisitIndentedSection(node, 0, PartitionPolicyEnum::kAlwaysExpand);
        } else {
          VisitIndentedSection(
              node, 0, PartitionPolicyEnum::kAppendFittingSubPartitions);
        }
      } else {
        TraverseChildren(node);
      }
      break;
    }

    case NodeEnum::kSystemTFCall: {
      // TODO(fangism): Create own section only for standalone calls
      if (Context().DirectParentIs(NodeEnum::kStatement)) {
        VisitIndentedSection(node, 0,
                             PartitionPolicyEnum::kAppendFittingSubPartitions);
      } else {
        TraverseChildren(node);
      }
      break;
    };

    case NodeEnum::kMacroCall: {
      // Single statements directly inside a controlled construct
      // should be properly indented one level.
      const int indent = ShouldIndentRelativeToDirectParent(Context())
                             ? style_.indentation_spaces
                             : 0;
      VisitIndentedSection(node, indent,
                           PartitionPolicyEnum::kAppendFittingSubPartitions);
      break;
    }

      // The following constructs wish to use the partition policy of appending
      // trailing subpartitions greedily as long as they fit, wrapping as
      // needed.
    case NodeEnum::kPreprocessorDefine:
    case NodeEnum::kClassConstructorPrototype:
    case NodeEnum::kTaskHeader:
    case NodeEnum::kFunctionHeader:
    case NodeEnum::kTaskPrototype:
    case NodeEnum::kFunctionPrototype:
    case NodeEnum::kDPIImportItem: {
      VisitIndentedSection(node, 0,
                           PartitionPolicyEnum::kAppendFittingSubPartitions);
      break;
    }

    // For the following constructs, always expand the view to subpartitions.
    // Add a level of indentation.
    case NodeEnum::kPackageImportList:
    case NodeEnum::kPackageItemList:
    case NodeEnum::kInterfaceClassDeclaration:
    case NodeEnum::kCasePatternItemList:
    case NodeEnum::kStructUnionMemberList:
    case NodeEnum::kConstraintBlockItemList:
    case NodeEnum::kConstraintExpressionList:
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
    case NodeEnum::kSpecifyItemList:
    case NodeEnum::kClockingItemList: {
      // Do not further indent preprocessor clauses.
      const int indent = suppress_indentation ? 0 : style_.indentation_spaces;
      VisitIndentedSection(node, indent, PartitionPolicyEnum::kAlwaysExpand);
      break;
    }

      // Add a level of grouping that is treated as wrapping.
    case NodeEnum::kMacroFormalParameterList:
    case NodeEnum::kMacroArgList:
    case NodeEnum::kForSpec:
    case NodeEnum::kModportSimplePortsDeclaration:
    case NodeEnum::kModportTFPortsDeclaration:
    case NodeEnum::kGateInstanceRegisterVariableList:
    case NodeEnum::kVariableDeclarationAssignmentList:
    case NodeEnum::kPortList: {
      // Do not further indent preprocessor clauses.
      const int indent = suppress_indentation ? 0 : style_.wrap_spaces;
      VisitIndentedSection(node, indent,
                           PartitionPolicyEnum::kFitOnLineElseExpand);
      break;
    }
    case NodeEnum::kOpenRangeList: {
      if (Context().DirectParentIs(NodeEnum::kConcatenationExpression)) {
        // Do not further indent preprocessor clauses.
        const int indent = suppress_indentation ? 0 : style_.indentation_spaces;
        VisitIndentedSection(node, indent,
                             PartitionPolicyEnum::kFitOnLineElseExpand);
        break;
      } else {
        // Default handling
        TraverseChildren(node);
        break;
      }
    }

    case NodeEnum::kBlockItemStatementList:
    case NodeEnum::kStatementList:
    case NodeEnum::kFunctionItemList:
    case NodeEnum::kCaseItemList:
    case NodeEnum::kCaseInsideItemList:
    case NodeEnum::kGenerateCaseItemList:
    case NodeEnum::kClassItems:
    case NodeEnum::kModuleItemList:
    case NodeEnum::kGenerateItemList:
    case NodeEnum::kDistributionItemList:
    case NodeEnum::kEnumNameList: {
      const int indent = suppress_indentation ? 0 : style_.indentation_spaces;
      VisitIndentedSection(node, indent,
                           PartitionPolicyEnum::kTabularAlignment);
      break;
    }

      // module instantiations (which look like data declarations) want to
      // expand one parameter/port per line.
    case NodeEnum::kActualParameterByNameList: {
      const int indent =
          suppress_indentation ? 0 : style_.NamedParameterIndentation();
      VisitIndentedSection(node, indent,
                           PartitionPolicyEnum::kTabularAlignment);
      break;
    }
    case NodeEnum::kPortActualList:  // covers named and positional ports
    {
      const int indent =
          suppress_indentation ? 0 : style_.NamedPortIndentation();
      const auto policy = Context().IsInside(NodeEnum::kDataDeclaration)
                              ? PartitionPolicyEnum::kTabularAlignment
                              : PartitionPolicyEnum::kFitOnLineElseExpand;
      VisitIndentedSection(node, indent, policy);
      break;
    }

    case NodeEnum::kPortDeclarationList: {
      if (Context().IsInside(NodeEnum::kPortDeclarationList)) {
        // This "recursion" can occur when there are preprocessing conditional
        // port declaration lists.
        // When this occurs, suppress creating another token partition level.
        // This will essentially flatten all port declarations to the same token
        // partition tree depth, and importantly, be alignable across
        // preprocessing directives as sibling subpartitions.
        // Alternatively, we could have done this flattening during
        // ReshapeTokenPartitions(), but it would've taken more work to
        // re-identify the subpartitions (children) to flatten.
        TraverseChildren(node);
        break;
      }
      // Do not further indent preprocessor clauses.
      const int indent =
          suppress_indentation ? 0 : style_.PortDeclarationsIndentation();
      if (Context().IsInside(NodeEnum::kClassHeader) ||
          // kModuleHeader covers interfaces and programs
          Context().IsInside(NodeEnum::kModuleHeader)) {
        VisitIndentedSection(node, indent,
                             PartitionPolicyEnum::kTabularAlignment);
      } else {
        VisitIndentedSection(node, indent,
                             PartitionPolicyEnum::kFitOnLineElseExpand);
      }
      break;
    }
    case NodeEnum::kFormalParameterList: {
      // Do not further indent preprocessor clauses.
      const int indent =
          suppress_indentation ? 0 : style_.FormalParametersIndentation();
      if (Context().IsInside(NodeEnum::kClassHeader) ||
          // kModuleHeader covers interfaces and programs
          Context().IsInside(NodeEnum::kModuleHeader)) {
        VisitIndentedSection(node, indent,
                             PartitionPolicyEnum::kTabularAlignment);
      } else {
        VisitIndentedSection(node, indent,
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

    // kReference can be found in kIdentifierList
    // and kExpression can be found in kArgumentList & kMacroArgList
    case NodeEnum::kReference:
    case NodeEnum::kExpression: {
      if (Context().DirectParentIsOneOf({NodeEnum::kMacroArgList,
                                         NodeEnum::kArgumentList,
                                         NodeEnum::kIdentifierList})) {
        // original un-lexed macro argument was successfully expanded
        VisitNewUnwrappedLine(node);
      } else if (Context().DirectParentIs(NodeEnum::kOpenRangeList) &&
                 Context().IsInside(NodeEnum::kConcatenationExpression)) {
        VisitIndentedSection(node, 0,
                             PartitionPolicyEnum::kFitOnLineElseExpand);
      } else {
        TraverseChildren(node);
      }
      break;
    }

    default: {
      TraverseChildren(node);
      // TODO(fangism): Eventually replace this case with:
      // VisitIndentedSection(node, 0,
      //     PartitionPolicyEnum::kFitOnLineElseExpand);
    }
  }
}

static bool PartitionStartsWithSemicolon(const TokenPartitionTree& partition) {
  const auto& uwline = partition.RightmostDescendant()->Value();
  return !uwline.IsEmpty() && uwline.TokensRange().front().TokenEnum() == ';';
}

static bool PartitionIsCloseParenSemi(const TokenPartitionTree& partition) {
  const auto ftokens = partition.Value().TokensRange();
  if (ftokens.size() < 2) return false;
  if (ftokens.front().TokenEnum() != ')') return false;
  return ftokens.back().TokenEnum() == ';';
}

static bool PartitionStartsWithCloseParen(const TokenPartitionTree& partition) {
  const auto ftokens = partition.Value().TokensRange();
  if (ftokens.empty()) return false;
  const auto token_enum = ftokens.front().TokenEnum();
  return ((token_enum == ')') || (token_enum == MacroCallCloseToEndLine));
}

static bool PartitionEndsWithOpenParen(const TokenPartitionTree& partition) {
  const auto ftokens = partition.Value().TokensRange();
  if (ftokens.empty()) return false;
  const auto token_enum = ftokens.back().TokenEnum();
  return token_enum == '(';
}

static bool PartitionIsCloseBrace(const TokenPartitionTree& partition) {
  const auto ftokens = partition.Value().TokensRange();
  if (ftokens.size() != 1) return false;
  const auto token_enum = ftokens.front().TokenEnum();
  return token_enum == '}';
}

static bool PartitionEndsWithOpenBrace(const TokenPartitionTree& partition) {
  const auto ftokens = partition.Value().TokensRange();
  if (ftokens.empty()) return false;
  const auto token_enum = ftokens.back().TokenEnum();
  return token_enum == '{';
}

static bool PartitionStartsWithCloseBrace(const TokenPartitionTree& partition) {
  const auto ftokens = partition.Value().TokensRange();
  if (ftokens.empty()) return false;
  const auto token_enum = ftokens.front().TokenEnum();
  return token_enum == '}';
}

static void AttachTrailingSemicolonToPreviousPartition(
    TokenPartitionTree* partition) {
  // Attach the trailing ';' partition to the previous sibling leaf.
  // VisitIndentedSection() finished by starting a new partition,
  // so we need to back-track to the previous sibling partition.

  // In some cases where macros are involved, there may not necessarily
  // be a semicolon where one is grammatically expected.
  // In those cases, do nothing.
  if (PartitionStartsWithSemicolon(*partition)) {
    verible::MergeLeafIntoPreviousLeaf(partition->RightmostDescendant());
    VLOG(4) << "after moving semicolon:\n" << *partition;
  }
}

static void AdjustSubsequentPartitionsIndentation(TokenPartitionTree* partition,
                                                  int indentation) {
  // Adjust indentation of subsequent partitions
  const auto npartitions = partition->Children().size();
  if (npartitions > 1) {
    const auto& first_partition = partition->Children().front();
    const auto& last_partition = partition->Children().back();

    // Do not indent intentionally wrapped partitions, e.g.
    // { (>>[assign foo = {] }
    // { (>>>>[<auto>]
    //   { (>>>>[a ,] }
    //   { (>>>>[b ,] }
    //   { (>>>>[c ,] }
    //   { (>>>>[d] }
    // }
    // { (>>[} ;] }
    if (!(PartitionEndsWithOpenBrace(first_partition) &&
          PartitionStartsWithCloseBrace(last_partition)) &&
        !(PartitionEndsWithOpenParen(first_partition) &&
          PartitionIsCloseParenSemi(last_partition))) {
      for (unsigned int idx = 1; idx < npartitions; ++idx) {
        AdjustIndentationRelative(&partition->Children()[idx], indentation);
      }
    }
  }
}

static void ReshapeIfClause(const SyntaxTreeNode& node,
                            TokenPartitionTree* partition_ptr) {
  auto& partition = *partition_ptr;
  const SyntaxTreeNode* body = GetAnyControlStatementBody(node);
  if (body == nullptr || !NodeIsBeginEndBlock(*body)) {
    VLOG(4) << "if-body was not a begin-end block.";
    // If body is a null statement, attach it to the previous partition.
    AttachTrailingSemicolonToPreviousPartition(partition_ptr);
    return;
  }

  // Then fuse the 'begin' partition with the preceding 'if (...)'
  auto& if_body_partition = partition.Children().back();
  auto& begin_partition = if_body_partition.Children().front();
  verible::MergeLeafIntoPreviousLeaf(&begin_partition);
  partition.Value().SetPartitionPolicy(
      if_body_partition.Value().PartitionPolicy());
  // if seq_block body was empty, that leaves only 'end', so hoist.
  // if if-header was flat, hoist that too.
  partition.FlattenOneChild(if_body_partition.BirthRank());
}

static void ReshapeElseClause(const SyntaxTreeNode& node,
                              TokenPartitionTree* partition_ptr) {
  auto& partition = *partition_ptr;
  const SyntaxTreeNode& else_body_subnode =
      *ABSL_DIE_IF_NULL(GetAnyControlStatementBody(node));
  if (!NodeIsConditionalOrBlock(else_body_subnode)) {
    VLOG(4) << "else-body was neither a begin-end block nor if-conditional.";
    // If body is a null statement, attach it to the previous partition.
    AttachTrailingSemicolonToPreviousPartition(partition_ptr);
    return;
  }

  // Then fuse 'else' and 'begin' partitions together
  // or fuse the 'else' and 'if' (header) partitions together
  auto& else_partition = partition.Children().front();
  verible::MergeLeafIntoNextLeaf(&else_partition);
}

static void HoistOnlyChildPartition(TokenPartitionTree* partition) {
  const auto* origin = partition->Value().Origin();
  if (partition->HoistOnlyChild()) {
    VLOG(4) << "reshape: hoisted, using child partition policy, parent origin";
    // Preserve source origin
    partition->Value().SetOrigin(origin);
  }
}

static void PushEndIntoElsePartition(TokenPartitionTree* partition_ptr) {
  // Then combine 'end' with the following 'else' ...
  // Do not flatten, so that if- and else- clauses can make formatting
  // decisions independently from each other.
  auto& partition = *partition_ptr;
  auto& if_clause_partition = partition.Children().front();
  auto* end_partition = if_clause_partition.RightmostDescendant();
  auto* end_parent = verible::MergeLeafIntoNextLeaf(end_partition);
  // if moving leaf results in any singleton partitions, hoist.
  if (end_parent != nullptr) {
    HoistOnlyChildPartition(end_parent);
  }
}

static void MergeEndElseWithoutLabel(const SyntaxTreeNode& conditional,
                                     TokenPartitionTree* partition_ptr) {
  auto& partition = *partition_ptr;
  // Handle merging of 'else' partition with (possibly)
  // a previous 'end' partition.
  // Do not flatten, so that if- and else- clauses can make formatting
  // decisions independently from each other.
  const auto* if_body_subnode =
      GetAnyControlStatementBody(*GetAnyConditionalIfClause(conditional));
  if (if_body_subnode == nullptr || !NodeIsBeginEndBlock(*if_body_subnode)) {
    VLOG(4) << "if-body was not begin-end block";
    return;
  }
  VLOG(4) << "if body was a begin-end block";
  const auto* end_label = GetEndLabel(GetBlockEnd(*if_body_subnode));
  if (end_label != nullptr) {
    VLOG(4) << "'end' came with label, no merge";
    return;
  }
  VLOG(4) << "No 'end' label, merge 'end' and 'else...' partitions";
  PushEndIntoElsePartition(&partition);
}

static void FlattenElseIfElse(const SyntaxTreeNode& else_clause,
                              TokenPartitionTree* partition_ptr) {
  // Keep chained else-if-else conditionals in a flat structure.
  auto& partition = *partition_ptr;
  const auto& else_body_subnode = *GetAnyControlStatementBody(else_clause);
  if (NodeIsConditionalConstruct(else_body_subnode) &&
      GetAnyConditionalElseClause(else_body_subnode) != nullptr) {
    partition.FlattenOneChild(partition.Children().size() - 1);
  }
}

static void ReshapeConditionalConstruct(const SyntaxTreeNode& conditional,
                                        TokenPartitionTree* partition_ptr) {
  const auto* else_clause = GetAnyConditionalElseClause(conditional);
  if (else_clause == nullptr) {
    VLOG(4) << "there was no else clause";
    return;
  }
  VLOG(4) << "there was an else clause";
  MergeEndElseWithoutLabel(conditional, partition_ptr);
  FlattenElseIfElse(*else_clause, partition_ptr);
}

static void IndentBetweenUVMBeginEndMacros(TokenPartitionTree* partition_ptr,
                                           int indentation_spaces) {
  auto& partition = *partition_ptr;
  // Indent elements between UVM begin/end macro calls
  unsigned int uvm_level = 1;
  std::vector<TokenPartitionTree*> uvm_range;

  // Search backwards for matching _begin.
  for (auto* itr = partition.PreviousSibling(); itr;
       itr = itr->PreviousSibling()) {
    VLOG(4) << "Scanning previous sibling:\n" << *itr;
    const auto macroId =
        itr->LeftmostDescendant()->Value().TokensRange().front().Text();
    VLOG(4) << "macro id: " << macroId;

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
    VLOG(4) << "Found matching uvm-begin/end macros";
    for (auto* itr : uvm_range) {
      // Indent uvm macros inside `uvm.*begin - `uvm.*end
      verible::AdjustIndentationRelative(itr, +indentation_spaces);
    }
  }
}

// This phase is strictly concerned with reshaping token partitions,
// and occurs on the return path of partition tree construction.
void TreeUnwrapper::ReshapeTokenPartitions(
    const SyntaxTreeNode& node, const verible::BasicFormatStyle& style,
    TokenPartitionTree* recent_partition) {
  const auto tag = static_cast<NodeEnum>(node.Tag().tag);
  VLOG(3) << __FUNCTION__ << " node: " << tag;
  auto& partition = *recent_partition;
  VLOG(4) << "before reshaping " << tag << ":\n" << partition;

  // Tips, when making reshaping decisions based on subtrees:
  //   * Manipulate the partition *incrementally* and as close to the
  //     corresponding node enum case that produced it as possible.
  //     In otherwords, minimize the depth of examination needed to
  //     make a reshaping decision.  If there is insufficient information to
  //     make a decision, that would be a good reason to defer to a parent node.
  //   * Prefer to examine the SyntaxTreeNode& node instead of the token
  //     partition tree because the token partition tree may include
  //     EOL-comments as nodes.  Use/create CST functions to abstract away
  //     the precise structural details.
  //   * Altering the indentation and partition policy itself is allowed,
  //     even though they were set during the
  //     SetIndentationsAndCreatePartitions() phase.

  // post-creation token partition adjustments
  switch (tag) {
    // Note: this is also being applied to variable declaration lists,
    // which may want different handling than instantiations.
    case NodeEnum::kDataDeclaration: {
      AttachTrailingSemicolonToPreviousPartition(&partition);
      auto& data_declaration_partition = partition;
      auto& children = data_declaration_partition.Children();
      CHECK(!children.empty());

      // TODO(fangism): fuse qualifiers (if any) with type partition

      // The instances/variable declaration list is always in last position.
      auto& instance_list_partition = children.back();
      if (  // TODO(fangism): get type and qualifiers from declaration node,
            // and check whether type node is implicit, using CST functions.
          instance_list_partition.BirthRank() == 0) {
        VLOG(4) << "No qualifiers and type is implicit";
        // This means there are no qualifiers and type was implicit,
        // so no partition precedes this.
        // Use the declaration (parent) level's indentation.
        verible::AdjustIndentationAbsolute(
            &instance_list_partition,
            data_declaration_partition.Value().IndentationSpaces());
        HoistOnlyChildPartition(&instance_list_partition);
      } else if (GetInstanceListFromDataDeclaration(node).children().size() ==
                 1) {
        VLOG(4) << "Instance list has only one child, singleton.";

        // Undo the indentation of the only instance in the hoisted subtree.
        verible::AdjustIndentationRelative(&instance_list_partition,
                                           -style.wrap_spaces);
        VLOG(4) << "After un-indenting, instance list:\n"
                << instance_list_partition;

        // Reshape type-instance partitions.
        auto* instance_type_partition =
            ABSL_DIE_IF_NULL(children.back().PreviousSibling());
        VLOG(4) << "instance type:\n" << *instance_type_partition;

        if (instance_type_partition == &children.front()) {
          // data_declaration_partition now consists of exactly:
          //   instance_type_partition, instance_list_partition (single
          //   instance)

          // Flatten these (which will invalidate their references).
          std::vector<size_t> offsets;
          data_declaration_partition.FlattenOnlyChildrenWithChildren(&offsets);
          // This position in the flattened result will be merged.
          const size_t fuse_position = offsets.back() - 1;

          // Join the rightmost of instance_type_partition with the leftmost of
          // instance_list_partition.  Keep the type's indentation.
          // This can yield an intermediate partition that contains:
          // ") instance_name (".
          MergeConsecutiveSiblings(&data_declaration_partition, fuse_position);
        } else {
          // There is a qualifier before instance_type_partition.
          data_declaration_partition.FlattenOnlyChildrenWithChildren();
        }
      } else {
        VLOG(4) << "None of the special cases apply.";
      }
      break;
    }

    // For these cases, the leading sub-partition is merged into its next
    // relative.  This is useful for constructs that are repeatedly prefixed
    // with attributes, but otherwise maintain the same subpartition shape.
    case NodeEnum::kForwardDeclaration:
      // partition consists of (example):
      //   [pure virtual]
      //   [task ... ] (task header/prototype)
      // Push the qualifiers down.
    case NodeEnum::kDPIImportItem:
      // partition consists of (example):
      //   [import "DPI-C"]
      //   [function ... ] (task header/prototype)
      // Push the "import..." down.
      {
        verible::MergeLeafIntoNextLeaf(&partition.Children().front());
        break;
      }
    case NodeEnum::kBindDirective: {
      AttachTrailingSemicolonToPreviousPartition(&partition);

      // Take advantage here that preceding data declaration partition
      // was already shaped.
      auto& target_instance_partition = partition;
      auto& children = target_instance_partition.Children();
      // Attach ')' to the instance name
      verible::MergeLeafIntoNextLeaf(children.back().PreviousSibling());

      verible::AdjustIndentationRelative(&children.back(), -style.wrap_spaces);
      break;
    }
    case NodeEnum::kStatement: {
      // This handles cases like macro-calls followed by a semicolon.
      AttachTrailingSemicolonToPreviousPartition(&partition);
      break;
    }
    case NodeEnum::kModuleHeader: {
      // Allow empty ports to appear as "();"
      if (partition.Children().size() >= 2) {
        auto& last = partition.Children().back();
        auto& last_prev = *ABSL_DIE_IF_NULL(last.PreviousSibling());
        if (PartitionStartsWithCloseParen(last) &&
            PartitionEndsWithOpenParen(last_prev)) {
          verible::MergeLeafIntoPreviousLeaf(&last);
        }
      }
      // If there were any parameters or ports at all, expand.
      // TODO(fangism): This should be done by inspecting the CST node,
      // instead of the partition structure.
      if (partition.Children().size() > 2) {
        partition.Value().SetPartitionPolicy(
            PartitionPolicyEnum::kAlwaysExpand);
      }
      break;
    }

    case NodeEnum::kClassHeader: {
      // Allow empty parameters to appear as "#();"
      if (partition.Children().size() >= 2) {
        auto& last = partition.Children().back();
        auto& last_prev = *ABSL_DIE_IF_NULL(last.PreviousSibling());
        if (PartitionStartsWithCloseParen(last) &&
            PartitionEndsWithOpenParen(last_prev)) {
          verible::MergeLeafIntoPreviousLeaf(&last);
        }
      }
      break;
    }

      // The following partitions may end up with a trailing ");" subpartition
      // and want to attach it to the item that preceded it.
    case NodeEnum::kClassConstructorPrototype:
    case NodeEnum::kFunctionHeader:
    case NodeEnum::kFunctionPrototype:
    case NodeEnum::kTaskHeader:
    case NodeEnum::kTaskPrototype: {
      auto& last = *ABSL_DIE_IF_NULL(partition.RightmostDescendant());
      if (PartitionIsCloseParenSemi(last)) {
        verible::MergeLeafIntoPreviousLeaf(&last);
      }
      break;
    }
    case NodeEnum::kReferenceCallBase: {
      const auto& subnode = verible::SymbolCastToNode(*node.children().back());
      if (subnode.MatchesTagAnyOf({NodeEnum::kMethodCallExtension,
                                   NodeEnum::kFunctionCall,
                                   NodeEnum::kRandomizeMethodCallExtension})) {
        if (partition.Value().PartitionPolicy() ==
            PartitionPolicyEnum::kAppendFittingSubPartitions) {
          auto& last = *ABSL_DIE_IF_NULL(partition.RightmostDescendant());
          if (PartitionStartsWithCloseParen(last) ||
              PartitionStartsWithSemicolon(last)) {
            verible::MergeLeafIntoPreviousLeaf(&last);
          }
        }
      }
      break;
    }

    case NodeEnum::kRandomizeFunctionCall:
    case NodeEnum::kSystemTFCall: {
      if (partition.Value().PartitionPolicy() ==
          PartitionPolicyEnum::kAppendFittingSubPartitions) {
        auto& last = *ABSL_DIE_IF_NULL(partition.RightmostDescendant());
        if (PartitionStartsWithCloseParen(last) ||
            PartitionStartsWithSemicolon(last)) {
          verible::MergeLeafIntoPreviousLeaf(&last);
        }
      }
      break;
    }

    case NodeEnum::kGenerateIfHeader:
    case NodeEnum::kIfHeader: {
      // Fix indentation in case of e.g. function calls inside if headers
      // TODO(fangism): This should be done smarter (using CST) or removed
      //     after better handling of function calls inside expressions
      //     e.g. kBinaryExpression, kUnaryPrefixExpression...
      if (partition.Children().size() > 1) {
        auto& if_header_partition = partition.Children()[0];
        const auto original_indentation =
            if_header_partition.Value().IndentationSpaces();
        // Adjust indentation recursively
        verible::AdjustIndentationRelative(&partition, style.wrap_spaces);
        // Restore original indentation in first partition
        partition.Value().SetIndentationSpaces(original_indentation);
        if_header_partition.Value().SetIndentationSpaces(original_indentation);
      }
      break;
    }

      // The following cases handle reshaping around if/else/begin/end.
    case NodeEnum::kAssertionClause:
    case NodeEnum::kAssumeClause:
    case NodeEnum::kCoverStatement:
    case NodeEnum::kWaitStatement:
    case NodeEnum::kAssertPropertyClause:
    case NodeEnum::kAssumePropertyClause:
    case NodeEnum::kExpectPropertyClause:
    case NodeEnum::kCoverPropertyStatement:
    case NodeEnum::kCoverSequenceStatement:
    case NodeEnum::kIfClause:
    case NodeEnum::kGenerateIfClause: {
      ReshapeIfClause(node, &partition);
      break;
    }
    case NodeEnum::kGenerateElseClause:
    case NodeEnum::kElseClause: {
      ReshapeElseClause(node, &partition);
      break;
    }

    case NodeEnum::kAssertionStatement:
      // Contains a kAssertionClause and possibly a kElseClause
    case NodeEnum::kAssumeStatement:
      // Contains a kAssumeClause and possibly a kElseClause
    case NodeEnum::kAssertPropertyStatement:
      // Contains a kAssertPropertyClause and possibly a kElseClause
    case NodeEnum::kAssumePropertyStatement:
      // Contains a kAssumePropertyClause and possibly a kElseClause
    case NodeEnum::kExpectPropertyStatement:
      // Contains a kExpectPropertyClause and possibly a kElseClause
    case NodeEnum::kConditionalStatement:
      // Contains a kIfClause and possibly a kElseClause
    case NodeEnum::kConditionalGenerateConstruct:
      // Contains a kGenerateIfClause and possibly a kGenerateElseClause
      {
        ReshapeConditionalConstruct(node, &partition);
        break;
      }

    case NodeEnum::kPreprocessorDefine: {
      auto& last = *ABSL_DIE_IF_NULL(partition.RightmostDescendant());
      // TODO(fangism): why does test fail without this clause?
      if (PartitionStartsWithCloseParen(last)) {
        verible::MergeLeafIntoPreviousLeaf(&last);
      }
      break;
    }

    case NodeEnum::kMacroCall: {
      // If there are no call args, join the '(' and ')' together.
      if (MacroCallArgsIsEmpty(GetMacroCallArgs(node))) {
        // FIXME HERE: flattening wrong place!  Should merge instead.
        partition.FlattenOnce();
        VLOG(4) << "NODE: kMacroCall (flattened):\n" << partition;
      } else {
        // Merge closing parenthesis into last argument partition
        // Test for ')' and MacroCallCloseToEndLine because macros
        // use its own token 'MacroCallCloseToEndLine'
        auto& last = *ABSL_DIE_IF_NULL(partition.RightmostDescendant());
        if (PartitionStartsWithCloseParen(last) ||
            PartitionIsCloseParenSemi(last)) {
          verible::MergeLeafIntoPreviousLeaf(&last);
        }
      }
      break;
    }
    case NodeEnum::kConstraintDeclaration: {
      // TODO(fangism): kConstraintSet should be handled similarly with {}
      if (partition.Children().size() == 2) {
        auto& last = *ABSL_DIE_IF_NULL(partition.RightmostDescendant());
        if (PartitionIsCloseBrace(last)) {
          verible::MergeLeafIntoPreviousLeaf(&last);
        }
      }
      break;
    }

    case NodeEnum::kConstraintBlockItemList: {
      HoistOnlyChildPartition(&partition);

      // Alwyas expand constraint(s) blocks with braces inside them
      const auto& uwline = partition.Value();
      const auto& ftokens = uwline.TokensRange();
      auto found = std::find_if(ftokens.begin(), ftokens.end(),
                                [](const verible::PreFormatToken& token) {
                                  return token.TokenEnum() == '{';
                                });
      if (found != ftokens.end()) {
        VLOG(4) << "Found brace group, forcing expansion";
        partition.Value().SetPartitionPolicy(
            PartitionPolicyEnum::kAlwaysExpand);
      }
      break;
    }

      // This group of cases is temporary: simplify these during the
      // rewrite/refactor of this function.
      // See search-anchor: STATEMENT_TYPES
    case NodeEnum::kNetVariableAssignment:           // e.g. x=y
    case NodeEnum::kBlockingAssignmentStatement:     // id=expr
    case NodeEnum::kNonblockingAssignmentStatement:  // dest <= src;
    case NodeEnum::kAssignModifyStatement:           // id+=expr
    {
      VLOG(4) << "before moving semicolon:\n" << partition;
      AttachTrailingSemicolonToPreviousPartition(&partition);
      // RHS may have been further partitioned, e.g. a macro call.
      auto& children = partition.Children();
      if (children.size() == 2 && children.front().is_leaf() /* left side */) {
        verible::MergeLeafIntoNextLeaf(&children.front());
        VLOG(4) << "after merge leaf (left-into-right):\n" << partition;
      }
      break;
    }
    case NodeEnum::kProceduralTimingControlStatement: {
      std::vector<size_t> offsets;
      partition.FlattenOnlyChildrenWithChildren(&offsets);
      VLOG(4) << "before moving semicolon:\n" << partition;
      AttachTrailingSemicolonToPreviousPartition(&partition);
      // Check body, for kSeqBlock, merge 'begin' with previous sibling
      if (NodeIsBeginEndBlock(GetProceduralTimingControlStatementBody(node))) {
        verible::MergeConsecutiveSiblings(&partition, offsets[1] - 1);
        VLOG(4) << "after merge siblings:\n" << partition;
      }
      break;
    }
    case NodeEnum::kProceduralContinuousAssignmentStatement:
    case NodeEnum::kProceduralContinuousForceStatement:
    case NodeEnum::kContinuousAssignmentStatement: {  // e.g. assign a=0, b=2;
      // TODO(fangism): group into above similar assignment statement cases?
      //   Cannot easily move now due to multiple-assignments.
      partition.FlattenOnlyChildrenWithChildren();
      VLOG(4) << "after flatten:\n" << partition;
      AttachTrailingSemicolonToPreviousPartition(&partition);
      // Merge the 'assign' keyword with the (first) x=y assignment.
      // TODO(fangism): reshape for multiple assignments.
      verible::MergeConsecutiveSiblings(&partition, 0);
      VLOG(4) << "after merging 'assign':\n" << partition;
      AdjustSubsequentPartitionsIndentation(&partition, style.wrap_spaces);
      VLOG(4) << "after adjusting partitions indentation:\n" << partition;
      break;
    }
    case NodeEnum::kForSpec: {
      // This applies to loop statements and loop generate constructs.
      // There are two 'partitions' with ';'.
      // Merge those with their predecessor sibling partitions.
      auto& children = partition.Children();
      const auto iter1 = std::find_if(children.begin(), children.end(),
                                      PartitionStartsWithSemicolon);
      CHECK(iter1 != children.end());
      const auto iter2 =
          std::find_if(iter1 + 1, children.end(), PartitionStartsWithSemicolon);
      CHECK(iter2 != children.end());
      const int dist1 = std::distance(children.begin(), iter1);
      const int dist2 = std::distance(children.begin(), iter2);
      VLOG(4) << "kForSpec got ';' at child " << dist1 << " and " << dist2;
      // Merge from back-to-front to keep indices valid.
      if (dist2 > 0 && (dist2 - dist1 > 1)) {
        verible::MergeLeafIntoPreviousLeaf(&*iter2);
      }
      if (dist1 > 0) {
        verible::MergeLeafIntoPreviousLeaf(&*iter1);
      }
      break;
    }
    case NodeEnum::kLoopGenerateConstruct:
    case NodeEnum::kForLoopStatement:
    case NodeEnum::kForeverLoopStatement:
    case NodeEnum::kRepeatLoopStatement:
    case NodeEnum::kWhileLoopStatement:
    case NodeEnum::kForeachLoopStatement:
    case NodeEnum::kLabeledStatement:
    case NodeEnum::kCaseItem:
    case NodeEnum::kDefaultItem:
    case NodeEnum::kCaseInsideItem:
    case NodeEnum::kCasePatternItem:
    case NodeEnum::kGenerateCaseItem:
    // case NodeEnum::kAlwaysStatement:  // handled differently below
    // TODO(fangism): always,initial,final should be handled the same way
    case NodeEnum::kInitialStatement:
    case NodeEnum::kFinalStatement: {
      // In these cases, merge the 'begin' partition of the statement block
      // with the preceding keyword or header partition.
      if (NodeIsBeginEndBlock(
              verible::SymbolCastToNode(*node.children().back()))) {
        auto& seq_block_partition = partition.Children().back();
        VLOG(4) << "block partition: " << seq_block_partition;
        auto& begin_partition = *seq_block_partition.LeftmostDescendant();
        VLOG(4) << "begin partition: " << begin_partition;
        CHECK(begin_partition.is_leaf());
        verible::MergeLeafIntoPreviousLeaf(&begin_partition);
        VLOG(4) << "after merging 'begin' to predecessor:\n" << partition;
        // Flatten only the statement block so that the control partition
        // can retain its own partition policy.
        partition.FlattenOneChild(seq_block_partition.BirthRank());
      }
      break;
    }
    case NodeEnum::kDoWhileLoopStatement: {
      if (NodeIsBeginEndBlock(GetDoWhileStatementBody(node))) {
        // between do... and while (...);
        auto& seq_block_partition = partition.Children()[1];

        // merge "do" <- "begin"
        auto& begin_partition = *seq_block_partition.LeftmostDescendant();
        verible::MergeLeafIntoPreviousLeaf(&begin_partition);

        // merge "end" -> "while"
        auto& end_partition = *seq_block_partition.RightmostDescendant();
        verible::MergeLeafIntoNextLeaf(&end_partition);

        // Flatten only the statement block so that the control partition
        // can retain its own partition policy.
        partition.FlattenOneChild(seq_block_partition.BirthRank());
      }
      break;
    }

    case NodeEnum::kAlwaysStatement: {
      if (GetSubtreeAsNode(node, tag, node.children().size() - 1)
              .MatchesTagAnyOf({NodeEnum::kProceduralTimingControlStatement,
                                NodeEnum::kSeqBlock})) {
        // Merge 'always' keyword with next sibling, and adjust subtree indent.
        verible::MergeLeafIntoNextLeaf(&partition.Children().front());
        verible::AdjustIndentationAbsolute(
            &partition.Children().front(),
            partition.Value().IndentationSpaces());
        VLOG(4) << "after merging 'always':\n" << partition;
      }
      break;
    }

    case NodeEnum::kMacroGenericItem: {
      const auto& token = GetMacroGenericItemId(node);
      if (absl::StartsWith(token.text(), "`uvm_") &&
          absl::EndsWith(token.text(), "_end")) {
        VLOG(4) << "Found `uvm_*_end macro call";
        IndentBetweenUVMBeginEndMacros(&partition, style.indentation_spaces);
      }
      break;
    }
    default:
      break;
  }

  // In the majority of cases, automatically hoist singletons.
  // A few node types, however, will wish to delay the hoisting change,
  // so that the parent node can make reshaping decisions based on the
  // child node's original unhoisted form.
  // Exceptions:
  if (!node.MatchesTagAnyOf({
          //
          NodeEnum::kGateInstanceRegisterVariableList  // parent:
                                                       // kDataDeclaration)
      })) {
    HoistOnlyChildPartition(&partition);
  }

  VLOG(4) << "after reshaping " << tag << ":\n" << partition;
  VLOG(3) << "end of " << __FUNCTION__ << " node: " << tag;
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
  CHECK_EQ(NextUnfilteredToken()->text().begin(), leaf.get().text().begin());

  // Start a new partition in the following cases.
  // In most other cases, do nothing.
  if (IsPreprocessorControlFlow(tag)) {
    VLOG(4) << "handling preprocessor control flow token";
    StartNewUnwrappedLine(PartitionPolicyEnum::kFitOnLineElseExpand, &leaf);
    CurrentUnwrappedLine().SetIndentationSpaces(0);
  } else if (IsEndKeyword(tag)) {
    VLOG(4) << "handling end* keyword";
    StartNewUnwrappedLine(PartitionPolicyEnum::kAlwaysExpand, &leaf);
  }

  auto& partition = *ABSL_DIE_IF_NULL(CurrentTokenPartition());
  VLOG(4) << "before adding token " << VerboseToken(leaf.get()) << ":\n"
          << partition;

  // Advances NextUnfilteredToken(), and extends CurrentUnwrappedLine().
  // Should be equivalent to AdvanceNextUnfilteredToken().
  AddTokenToCurrentUnwrappedLine();

  // Look-ahead to any trailing comments that are associated with this leaf,
  // up to a newline.
  LookAheadBeyondCurrentLeaf();

  VLOG(4) << "before reshaping " << VerboseToken(leaf.get()) << ":\n"
          << partition;

  // Post-token-handling token partition adjustments:
  switch (leaf.Tag().tag) {
    case ',': {
      // In many cases (in particular lists), we want to attach delimiters like
      // ',' to the item that preceded it (on its rightmost leaf).
      // This adjustment is necessary for lists of element types that beget
      // their own indented sections.
      //
      // TODO(fangism): See also AttachTrailingSemicolonToPreviousPartition().
      // There should be one consistent way of attaching trailing delimiters.
      // Pick one, even if both work.
      if (current_context_.DirectParentIsOneOf({
              // NodeEnum:xxxx                             // due to element:
              NodeEnum::kMacroArgList,               // MacroArg
              NodeEnum::kFormalParameterList,        // kParamDeclaration
              NodeEnum::kEnumNameList,               // kEnumName
              NodeEnum::kDistributionItemList,       // kDistribution
              NodeEnum::kActualParameterByNameList,  // kParamByName
              NodeEnum::kPortDeclarationList,        // kPort, kPortDeclaration
              NodeEnum::kPortActualList,             // kActualNamedPort,
                                                     // kActualPositionalPort
              NodeEnum::kGateInstanceRegisterVariableList,  // kGateInstance,
                                                            // kRegisterVariable
              NodeEnum::kVariableDeclarationAssignmentList  // due to element:
              // kVariableDeclarationAssignment
          }) ||
          (current_context_.DirectParentIs(NodeEnum::kOpenRangeList) &&
           current_context_.IsInside(NodeEnum::kConcatenationExpression))) {
        MergeLastTwoPartitions();
      } else if (CurrentUnwrappedLine().Size() == 1) {
        // Partition would begin with a comma,
        // instead add this token to previous partition
        MergeLastTwoPartitions();
      }
      break;
    }
    case verilog_tokentype::SemicolonEndOfAssertionVariableDeclarations: {
      // is a ';'
      if (current_context_.DirectParentIs(
              NodeEnum::kAssertionVariableDeclarationList)) {
        MergeLastTwoPartitions();
      }
      break;
    }
    case PP_else: {
      // Do not allow non-comment tokens on the same line as `else
      // (comments were handled above)
      StartNewUnwrappedLine(PartitionPolicyEnum::kFitOnLineElseExpand, &leaf);
      break;
    }
    default:
      break;
  }

  VLOG(3) << "end of " << __FUNCTION__ << " leaf: " << VerboseToken(leaf.get());
}

// Specialized node visitors

void TreeUnwrapper::VisitNewUnwrappedLine(const SyntaxTreeNode& node) {
  StartNewUnwrappedLine(PartitionPolicyEnum::kFitOnLineElseExpand, &node);
  TraverseChildren(node);
}

void TreeUnwrapper::VisitNewUnindentedUnwrappedLine(
    const SyntaxTreeNode& node) {
  StartNewUnwrappedLine(PartitionPolicyEnum::kFitOnLineElseExpand, &node);
  // Force the current line to be unindented without losing track of where
  // the current indentation level is for children.
  CurrentUnwrappedLine().SetIndentationSpaces(0);
  TraverseChildren(node);
}

}  // namespace formatter
}  // namespace verilog
