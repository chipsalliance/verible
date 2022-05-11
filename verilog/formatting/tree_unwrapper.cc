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
#include <initializer_list>
#include <iterator>
#include <ostream>
#include <type_traits>
#include <utility>
#include <vector>

#include "absl/base/macros.h"
#include "absl/container/inlined_vector.h"
#include "absl/strings/match.h"
#include "common/formatting/basic_format_style.h"
#include "common/formatting/format_token.h"
#include "common/formatting/token_partition_tree.h"
#include "common/formatting/tree_unwrapper.h"
#include "common/formatting/unwrapped_line.h"
#include "common/strings/range.h"
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
#include "common/util/tree_operations.h"
#include "common/util/value_saver.h"
#include "common/util/vector_tree_iterators.h"
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

using ::verible::iterator_range;
using ::verible::NodeTag;
using ::verible::PartitionPolicyEnum;
using ::verible::PreFormatToken;
using ::verible::SpacingOptions;
using ::verible::SymbolTag;
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

static const verible::EnumNameMap<TokenScannerState>&
TokenScannerStateStrings() {
  static const verible::EnumNameMap<TokenScannerState>
      kTokenScannerStateStringMap({
          {"kStart", TokenScannerState::kStart},
          {"kHaveNewline", TokenScannerState::kHaveNewline},
          {"kNewPartition", TokenScannerState::kNewPartition},
          {"kEndWithNewline", TokenScannerState::kEndWithNewline},
          {"kEndNoNewline", TokenScannerState::kEndNoNewline},
      });
  return kTokenScannerStateStringMap;
}

// Conventional stream printer (declared in header providing enum).
std::ostream& operator<<(std::ostream& stream, TokenScannerState p) {
  return TokenScannerStateStrings().Unparse(p, stream);
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

static void VerilogOriginPrinter(
    std::ostream& stream, const verible::TokenInfo::Context& token_context,
    const verible::Symbol* symbol) {
  CHECK_NOTNULL(symbol);

  if (symbol->Kind() == verible::SymbolKind::kNode) {
    stream << NodeEnum(symbol->Tag().tag);
  } else {
    stream << "#" << verilog_symbol_name(verilog_tokentype(symbol->Tag().tag));
  }

  const auto* left = verible::GetLeftmostLeaf(*symbol);
  const auto* right = verible::GetRightmostLeaf(*symbol);
  if (left && right) {
    const int start = left->get().left(token_context.base);
    const int end = right->get().right(token_context.base);
    stream << "@" << start << "-" << end;
  }
  stream << " ";
  verible::UnwrappedLine::DefaultOriginPrinter(stream, symbol);
}

verible::TokenPartitionTreePrinter TreeUnwrapper::VerilogPartitionPrinter(
    const verible::TokenPartitionTree& partition) const {
  return verible::TokenPartitionTreePrinter(
      partition, false,
      [this](std::ostream& stream, const verible::Symbol* symbol) {
        VerilogOriginPrinter(stream, this->token_context_, symbol);
      });
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
    case NodeEnum::kStructUnionMemberList:
      LookAheadBeyondCurrentNode();
      break;
    case NodeEnum::kArgumentList:
    case NodeEnum::kMacroArgList:
      // Put each argument, separator, and comment in a new unwrapped line
      StartNewUnwrappedLine(PartitionPolicyEnum::kFitOnLineElseExpand, &node);
      // Catch comments after last argument
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

enum class ContextHint {
  kInsideStandaloneMacroCall,
};

static const verible::EnumNameMap<ContextHint>& ContextHintStrings() {
  static const verible::EnumNameMap<ContextHint> kContextHintStringMap({
      {"kInsideStandaloneMacroCall", ContextHint::kInsideStandaloneMacroCall},
  });
  return kContextHintStringMap;
}

std::ostream& operator<<(std::ostream& stream, ContextHint p) {
  return ContextHintStrings().Unparse(p, stream);
}

std::ostream& operator<<(std::ostream& stream,
                         const std::vector<ContextHint>& f) {
  return stream << verible::SequenceFormatter(f);
}

// Visitor to determine which node enum function to call
void TreeUnwrapper::Visit(const SyntaxTreeNode& node) {
  const auto tag = static_cast<NodeEnum>(node.Tag().tag);
  VLOG(3) << __FUNCTION__ << " node: " << tag;

  const auto context_hints_size = context_hints_.size();

  // This phase is only concerned with creating token partitions (during tree
  // recursive descent) and setting correct indentation values.  It is ok to
  // have excessive partitioning during this phase.
  SetIndentationsAndCreatePartitions(node);

  // This phase is only concerned with reshaping operations on token partitions,
  // such as merging, flattening, hoisting.  Reshaping should only occur on the
  // return path of tree traversal (here).

  auto* partition = PreviousSibling(*CurrentTokenPartition());
  if (partition != nullptr) {
    ReshapeTokenPartitions(node, style_, partition);
  }

  CHECK_GE(context_hints_.size(), context_hints_size);
  // Remove hints from traversed context
  context_hints_.erase(context_hints_.begin() + context_hints_size,
                       context_hints_.end());
}

// CST-descending phase that creates partitions with correct indentation.
// TODO(mglb): This function is > 600 lines and should probably be split
//             up into multiple.
void TreeUnwrapper::SetIndentationsAndCreatePartitions(
    const SyntaxTreeNode& node) {
  const auto tag = static_cast<NodeEnum>(node.Tag().tag);
  VLOG(3) << __FUNCTION__ << " node: " << tag;
  VLOG(4) << "Context hints: " << ContextHints();
  if (!Context().empty()) {
    DVLOG(4) << "Context: "
             << absl::StrJoin(Context().begin(), Context().end(), " / ",
                              [](std::string* out, const SyntaxTreeNode* n) {
                                const auto tag =
                                    static_cast<NodeEnum>(n->Tag().tag);
                                absl::StrAppend(out, NodeEnumToString(tag));
                              });
  }

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
              {NodeEnum::kParenGroup, NodeEnum::kFunctionCall}) &&
          Context().IsInside(NodeEnum::kAlwaysStatement) &&
          Context().IsInside(NodeEnum::kNetVariableAssignment)) {
        if (any_of(node.children().begin(), node.children().end(),
                   [](const verible::SymbolPtr& p) {
                     return p->Tag().tag ==
                            static_cast<int>(NodeEnum::kParamByName);
                   })) {
          // Indentation of named arguments of functions
          VisitIndentedSection(node, style_.indentation_spaces,
                               PartitionPolicyEnum::kTabularAlignment);
        } else {
          VisitIndentedSection(node, style_.indentation_spaces,
                               PartitionPolicyEnum::kFitOnLineElseExpand);
        }
      } else if (HasContextHint(ContextHint::kInsideStandaloneMacroCall) &&
                 Context().DirectParentsAre(
                     {NodeEnum::kParenGroup, NodeEnum::kSystemTFCall})) {
        VisitIndentedSection(node, style_.wrap_spaces,
                             PartitionPolicyEnum::kWrap);
      } else if (Context().DirectParentsAre(
                     {NodeEnum::kParenGroup,
                      NodeEnum::kRandomizeFunctionCall}) ||
                 (Context().DirectParentsAre(
                     {NodeEnum::kParenGroup, NodeEnum::kFunctionCall})) ||
                 (Context().DirectParentsAre(
                     {NodeEnum::kParenGroup, NodeEnum::kClassNew})) ||
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
      const auto* subnode = GetSubtreeAsNode(node, tag, 0);
      const auto next_indent = subnode && NodeIsConditionalOrBlock(*subnode)
                                   ? 0
                                   : style_.indentation_spaces;
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
      const auto is_nested_call = [&] {
        return Context().DirectParentsAre(
                   {NodeEnum::kExpression, NodeEnum::kArgumentList}) ||
               Context().DirectParentsAre(
                   {NodeEnum::kExpression, NodeEnum::kMacroArgList});
      };

      const auto is_standalone_call = [&] {
        return Context().DirectParentsAre(
                   {NodeEnum::kStatement, NodeEnum::kStatementList}) ||
               Context().DirectParentsAre(
                   {NodeEnum::kStatement, NodeEnum::kBlockItemStatementList}) ||
               Context().DirectParentIs(NodeEnum::kModuleItemList);
      };

      // TODO(mglb): Format non-standalone calls with layout optimizer.
      // This requires using layout optimizer for structures wrapping the
      // calls.

      if (is_nested_call() &&
          HasContextHint(ContextHint::kInsideStandaloneMacroCall)) {
        VisitIndentedSection(node, 0, PartitionPolicyEnum::kStack);
      } else if (is_standalone_call()) {
        PushContextHint(ContextHint::kInsideStandaloneMacroCall);
        VisitIndentedSection(node, 0, PartitionPolicyEnum::kStack);
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

      const auto is_nested_call = [&] {
        return Context().DirectParentsAre(
                   {NodeEnum::kExpression, NodeEnum::kArgumentList}) ||
               Context().DirectParentsAre(
                   {NodeEnum::kExpression, NodeEnum::kMacroArgList});
      };
      const auto is_standalone_call = [&] {
        return ((Context().DirectParentIs(NodeEnum::kModuleItemList) ||
                 Context().DirectParentIs(NodeEnum::kClassItems) ||
                 Context().DirectParentIs(NodeEnum::kDescriptionList) ||
                 Context().DirectParentsAre({
                     NodeEnum::kStatementList,
                     NodeEnum::kTaskDeclaration,
                 }) ||
                 Context().DirectParentsAre({
                     NodeEnum::kLocalRoot,
                     NodeEnum::kReference,
                     NodeEnum::kReferenceCallBase,
                 }) ||
                 Context().DirectParentsAre({
                     NodeEnum::kBlockItemStatementList,
                     NodeEnum::kSeqBlock,
                     NodeEnum::kIfBody,
                     NodeEnum::kIfClause,
                 })) &&
                !(Context().IsInside(NodeEnum::kNetVariableAssignment)  //
                  ));
      };

      // TODO(mglb): Format non-standalone calls with layout optimizer.
      // This would require using layout optimizer for structures wrapping the
      // calls.

      if (is_nested_call() &&
          HasContextHint(ContextHint::kInsideStandaloneMacroCall)) {
        VLOG(4) << "kMacroCall: nested";
        VisitIndentedSection(node, indent, PartitionPolicyEnum::kStack);
      } else if (is_standalone_call()) {
        VLOG(4) << "kMacroCall: standalone";
        PushContextHint(ContextHint::kInsideStandaloneMacroCall);
        VisitIndentedSection(node, indent, PartitionPolicyEnum::kStack);
      } else {
        VisitIndentedSection(node, indent,
                             PartitionPolicyEnum::kAppendFittingSubPartitions);
      }

      break;
    }

    case NodeEnum::kParenGroup: {
      const bool is_directly_inside_macro_call = Context().DirectParentIsOneOf({
          NodeEnum::kMacroCall,
          NodeEnum::kSystemTFCall,
      });

      if (HasContextHint(ContextHint::kInsideStandaloneMacroCall) &&
          is_directly_inside_macro_call) {
        const bool is_nested_call = Context().IsInsideFirst(
            {
                NodeEnum::kArgumentList,
                NodeEnum::kMacroArgList,
                NodeEnum::kParenGroup,
            },
            {
                NodeEnum::kStatementList,
                NodeEnum::kStatement,
            });

        if (is_nested_call) {
          VisitIndentedSection(node, 0, PartitionPolicyEnum::kWrap);
        } else {
          VisitIndentedSection(node, style_.wrap_spaces,
                               PartitionPolicyEnum::kWrap);
        }
      } else {
        TraverseChildren(node);
      }
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
    case NodeEnum::kFunctionPrototype: {
      if (Context().IsInside(NodeEnum::kDPIExportItem) ||
          Context().IsInside(NodeEnum::kDPIImportItem)) {
        VisitIndentedSection(node, 0,
                             PartitionPolicyEnum::kFitOnLineElseExpand);
      } else {
        VisitIndentedSection(node, 0,
                             PartitionPolicyEnum::kAppendFittingSubPartitions);
      }
      break;
    }

    case NodeEnum::kDPIExportItem:
    case NodeEnum::kDPIImportItem: {
      VisitIndentedSection(node, 0, PartitionPolicyEnum::kAlwaysExpand);
      break;
    }

    // For the following constructs, always expand the view to subpartitions.
    // Add a level of indentation.
    case NodeEnum::kPackageImportList:
    case NodeEnum::kPackageItemList:
    case NodeEnum::kInterfaceClassDeclaration:
    case NodeEnum::kCasePatternItemList:
    case NodeEnum::kConstraintBlockItemList:
    case NodeEnum::kConstraintExpressionList:
    case NodeEnum::kAssertionVariableDeclarationList:
    // The final sequence_expr of a sequence_declaration is same indentation
    // level as the kAssertionVariableDeclarationList that precedes it.
    case NodeEnum::kSequenceDeclarationFinalExpr:
    case NodeEnum::kCoverageSpecOptionList:
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

    case NodeEnum::kBinOptionList: {
      PartitionPolicyEnum shouldExpand =
          style_.expand_coverpoints ? PartitionPolicyEnum::kAlwaysExpand
                                    : PartitionPolicyEnum::kFitOnLineElseExpand;
      // Do not further indent preprocessor clauses.
      const int indent = suppress_indentation ? 0 : style_.indentation_spaces;
      VisitIndentedSection(node, indent, shouldExpand);
      break;
    }

    case NodeEnum::kCoverPoint: {
      if (style_.expand_coverpoints) {
        VisitIndentedSection(node, 0, PartitionPolicyEnum::kAlwaysExpand);
      } else
        VisitIndentedSection(node, 0,
                             PartitionPolicyEnum::kFitOnLineElseExpand);
      break;
    }

    case NodeEnum::kMacroArgList: {
      if (node.children().front() == nullptr ||
          node.children().front()->Tag().tag == verible::kUntagged) {
        // Empty arguments list.
        TraverseChildren(node);
        break;
      }
      if (HasContextHint(ContextHint::kInsideStandaloneMacroCall) &&
          Context().DirectParentsAre(
              {NodeEnum::kParenGroup, NodeEnum::kMacroCall})) {
        VisitIndentedSection(node, style_.wrap_spaces,
                             PartitionPolicyEnum::kWrap);
        break;
      }
    }
      [[fallthrough]];
    // Add a level of grouping that is treated as wrapping.
    case NodeEnum::kMacroFormalParameterList:
    case NodeEnum::kForSpec:
    case NodeEnum::kModportSimplePortsDeclaration:
    case NodeEnum::kModportTFPortsDeclaration:
    case NodeEnum::kGateInstanceRegisterVariableList:
    case NodeEnum::kVariableDeclarationAssignmentList: {
      // Do not further indent preprocessor clauses.
      const int indent = suppress_indentation ? 0 : style_.wrap_spaces;
      VisitIndentedSection(node, indent,
                           PartitionPolicyEnum::kFitOnLineElseExpand);
      break;
    }
    case NodeEnum::kOpenRangeList: {
      if (Context().DirectParentsAre(
              {NodeEnum::kConcatenationExpression, NodeEnum::kExpression}) &&
          !Context().IsInside(NodeEnum::kCoverageBin) &&
          !Context().IsInside(NodeEnum::kConditionExpression) &&
          (!Context().IsInside(NodeEnum::kBinaryExpression) ||
           Context().IsInside(NodeEnum::kPropertyImplicationList))) {
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

    case NodeEnum::kPortList: {
      if (Context().IsInside(NodeEnum::kDPIExportItem) ||
          Context().IsInside(NodeEnum::kDPIImportItem)) {
        const int indent = suppress_indentation ? 0 : style_.indentation_spaces;
        VisitIndentedSection(node, indent,
                             PartitionPolicyEnum::kTabularAlignment);
      } else {
        const int indent = suppress_indentation ? 0 : style_.wrap_spaces;
        VisitIndentedSection(node, indent,
                             PartitionPolicyEnum::kFitOnLineElseExpand);
      }
      break;
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
    case NodeEnum::kEnumNameList:
    case NodeEnum::kStructUnionMemberList: {
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
      const auto policy = Context().IsInside(NodeEnum::kDataDeclaration) ||
                                  Context().IsInside(NodeEnum::kBindDirective)
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
      } else if (Context().DirectParentsAre({NodeEnum::kOpenRangeList,
                                             NodeEnum::kConcatenationExpression,
                                             NodeEnum::kExpression}) &&
                 !Context().IsInside(NodeEnum::kCoverageBin) &&
                 !Context().IsInside(NodeEnum::kConditionExpression) &&
                 (!Context().IsInside(NodeEnum::kBinaryExpression) ||
                  Context().IsInside(NodeEnum::kPropertyImplicationList))) {
        VisitIndentedSection(node, 0,
                             PartitionPolicyEnum::kFitOnLineElseExpand);
      } else if (Context().IsInside(NodeEnum::kAssignmentPattern)) {
        VisitIndentedSection(node, 0,
                             PartitionPolicyEnum::kFitOnLineElseExpand);
      } else {
        TraverseChildren(node);
      }
      break;
    }

    case NodeEnum::kExpressionList:
    case NodeEnum::kUntagged: {
      if (Context().DirectParentIs(NodeEnum::kAssignmentPattern)) {
        VisitIndentedSection(node, style_.wrap_spaces,
                             PartitionPolicyEnum::kFitOnLineElseExpand);
      } else {
        TraverseChildren(node);
      }
      break;
    }

    case NodeEnum::kAssignmentPattern:
    case NodeEnum::kPatternExpression: {
      VisitIndentedSection(node, 0, PartitionPolicyEnum::kFitOnLineElseExpand);
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
  const auto& uwline = partition.Value();
  if (uwline.IsEmpty()) return false;
  const auto first_token = uwline.TokensRange().front().TokenEnum();
  return (first_token == ';' ||
          first_token ==
              verilog_tokentype::SemicolonEndOfAssertionVariableDeclarations);
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

// Returns true if the partition is forced to start in a new line.
static bool PartitionIsForcedIntoNewLine(const TokenPartitionTree& partition) {
  const auto policy = partition.Value().PartitionPolicy();
  if (policy == PartitionPolicyEnum::kAlreadyFormatted) return true;
  if (policy == PartitionPolicyEnum::kInline) return false;
  if (!is_leaf(partition)) {
    auto* first_leaf = &LeftmostDescendant(partition);
    const auto leaf_policy = first_leaf->Value().PartitionPolicy();
    if (leaf_policy == PartitionPolicyEnum::kAlreadyFormatted ||
        leaf_policy == PartitionPolicyEnum::kInline)
      return true;
  }

  const auto ftokens = partition.Value().TokensRange();
  if (ftokens.empty()) return false;
  return ftokens.front().before.break_decision == SpacingOptions::MustWrap;
}

// Joins partition containing only a ',' (and optionally comments) with
// a partition preceding or following it.
static void AttachSeparatorToPreviousOrNextPartition(
    TokenPartitionTree* partition) {
  CHECK_NOTNULL(partition);
  VLOG(5) << __FUNCTION__ << ": subpartition:\n" << *partition;

  if (!is_leaf(*partition)) {
    VLOG(5) << "  skip: not a leaf.";
    return;
  }

  // Find a separator and make sure this is the only non-comment token
  const verible::PreFormatToken* separator = nullptr;
  for (const auto& token : partition->Value().TokensRange()) {
    switch (token.TokenEnum()) {
      case verilog_tokentype::TK_COMMENT_BLOCK:
      case verilog_tokentype::TK_EOL_COMMENT:
      case verilog_tokentype::TK_ATTRIBUTE:
        break;
      case ',':
      case ':':
        if (separator == nullptr) {
          separator = &token;
          break;
        }
        [[fallthrough]];
      default:
        VLOG(5) << "  skip: contains tokens other than separator and comments.";
        return;
    }
  }
  if (separator == nullptr) {
    VLOG(5) << "  skip: separator token not found.";
    return;
  }

  // Merge with previous partition if both partitions are in the same line in
  // original text
  const auto* previous_partition = PreviousLeaf(*partition);
  if (previous_partition != nullptr) {
    if (!previous_partition->Value().TokensRange().empty()) {
      const auto& previous_token =
          previous_partition->Value().TokensRange().back();
      absl::string_view original_text_between = verible::make_string_view_range(
          previous_token.Text().end(), separator->Text().begin());
      if (!absl::StrContains(original_text_between, '\n')) {
        VLOG(5) << "  merge into previous partition.";
        verible::MergeLeafIntoPreviousLeaf(partition);
        return;
      }
    }
  }

  // Merge with next partition if both partitions are in the same line in
  // original text
  const auto* next_partition = NextLeaf(*partition);
  if (next_partition != nullptr) {
    if (!next_partition->Value().TokensRange().empty()) {
      const auto& next_token = next_partition->Value().TokensRange().front();
      absl::string_view original_text_between = verible::make_string_view_range(
          separator->Text().end(), next_token.Text().begin());
      if (!absl::StrContains(original_text_between, '\n')) {
        VLOG(5) << "  merge into next partition.";
        verible::MergeLeafIntoNextLeaf(partition);
        return;
      }
    }
  }

  // If there are no comments (separator is the only token)...
  if (partition->Value().TokensRange().size() == 1) {
    // Try merging with previous partition
    if (!PartitionIsForcedIntoNewLine(*partition)) {
      VLOG(5) << "  merge into previous partition.";
      verible::MergeLeafIntoPreviousLeaf(partition);
      return;
    }

    // Try merging with next partition
    if (next_partition != nullptr &&
        !PartitionIsForcedIntoNewLine(*next_partition)) {
      VLOG(5) << "  merge into next partition.";
      verible::MergeLeafIntoNextLeaf(partition);
      return;
    }
  }

  // Leave the separator in its own line and clear its Origin() to not confuse
  // tabular alignment.
  VLOG(5) << "  keep in separate line, remove origin.";
  partition->Value().SetOrigin(nullptr);
}

void AttachSeparatorsToListElementPartitions(TokenPartitionTree* partition) {
  CHECK_NOTNULL(partition);
  // Skip the first partition, it can't contain just a separator.
  for (int i = 1; i < static_cast<int>(partition->Children().size()); ++i) {
    auto& subpartition = partition->Children()[i];
    // This can change children count
    AttachSeparatorToPreviousOrNextPartition(&subpartition);
  }
}

static void AttachTrailingSemicolonToPreviousPartition(
    TokenPartitionTree* partition) {
  // TODO(mglb): Replace this function with
  // AttachSeparatorToPreviousOrNextPartition().

  // Attach the trailing ';' partition to the previous sibling leaf.
  // VisitIndentedSection() finished by starting a new partition,
  // so we need to back-track to the previous sibling partition.

  // In some cases where macros are involved, there may not necessarily
  // be a semicolon where one is grammatically expected.
  // In those cases, do nothing.
  auto* semicolon_partition = &RightmostDescendant(*partition);
  if (PartitionStartsWithSemicolon(*semicolon_partition)) {
    // When the semicolon is forced to wrap (e.g. when previous partition ends
    // with EOL comment), wrap previous partition with a group and append the
    // semicolon partition to it.
    if (PartitionIsForcedIntoNewLine(*semicolon_partition)) {
      auto* group = verible::GroupLeafWithPreviousLeaf(semicolon_partition);
      // Update invalidated pointer
      semicolon_partition = &RightmostDescendant(*group);
      group->Value().SetPartitionPolicy(
          verible::PartitionPolicyEnum::kAlwaysExpand);
      // Set indentation of partition with semicolon to match indentation of the
      // partition it has been grouped with
      semicolon_partition->Value().SetIndentationSpaces(
          group->Value().IndentationSpaces());
    } else {
      verible::MergeLeafIntoPreviousLeaf(semicolon_partition);
    }
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

// Merges first found subpartition ending with "=" or ":" with "'{" or "{"
// from the subpartition following it.
static void AttachOpeningBraceToDeclarationsAssignmentOperator(
    TokenPartitionTree* partition) {
  const int children_count = partition->Children().size();
  if (children_count <= 1) return;

  for (int i = 0; i < children_count - 1; ++i) {
    auto& current = RightmostDescendant(partition->Children()[i]);
    const auto tokens = current.Value().TokensRange();
    if (tokens.empty()) continue;

    auto rbegin = std::make_reverse_iterator(tokens.end());
    auto rend = std::make_reverse_iterator(tokens.begin());
    auto last_non_comment_it =
        std::find_if_not(rbegin, rend, [](const PreFormatToken& token) {
          // Lines with EOL Comment are non-mergable, so don't skip it here.
          return token.TokenEnum() == verilog_tokentype::TK_COMMENT_BLOCK;
        });
    if (last_non_comment_it == rend ||
        !(last_non_comment_it->TokenEnum() == '=' ||
          last_non_comment_it->TokenEnum() == ':'))
      continue;

    const auto& next = partition->Children()[i + 1];
    if (next.Value().IsEmpty()) continue;

    const auto& next_token = next.Value().TokensRange().front();
    if (!(next_token.TokenEnum() == verilog_tokentype::TK_LP ||
          next_token.TokenEnum() == '{'))
      continue;

    if (!PartitionIsForcedIntoNewLine(next)) {
      verible::MergeLeafIntoNextLeaf(&current);
      // There shouldn't be more matching partitions
      return;
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
  FlattenOneChild(partition, verible::BirthRank(if_body_partition));
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
  auto& children = partition.Children();
  auto else_partition_iter = std::find_if(
      children.begin(), children.end(), [](const TokenPartitionTree& n) {
        const auto* origin = n.Value().Origin();
        return origin &&
               origin->Tag() == verible::LeafTag(verilog_tokentype::TK_else);
      });
  if (else_partition_iter == children.end()) return;

  auto& else_partition = *else_partition_iter;
  auto* next_leaf = NextLeaf(else_partition);
  if (!next_leaf || PartitionIsForcedIntoNewLine(*next_leaf)) return;

  const auto* next_origin = next_leaf->Value().Origin();
  if (!next_origin ||
      !(next_origin->Tag() == verible::NodeTag(NodeEnum::kBegin) ||
        next_origin->Tag() == verible::LeafTag(verilog_tokentype::TK_begin) ||
        next_origin->Tag() == verible::NodeTag(NodeEnum::kIfHeader) ||
        next_origin->Tag() == verible::NodeTag(NodeEnum::kGenerateIfHeader) ||
        next_origin->Tag() == verible::LeafTag(verilog_tokentype::TK_if)))
    return;

  verible::MergeLeafIntoNextLeaf(&else_partition);
}

static void HoistOnlyChildPartition(TokenPartitionTree* partition) {
  // kWrap uses relative child indentation as a hanging
  // if (partition->Value().PartitionPolicy() == PartitionPolicyEnum::kWrap) {
  // TODO(mglb): implement or remove.
  // }
  const auto* origin = partition->Value().Origin();
  if (HoistOnlyChild(*partition)) {
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
  auto* end_partition = &RightmostDescendant(if_clause_partition);
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
    FlattenOneChild(partition, partition.Children().size() - 1);
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

static bool TokenIsComment(const PreFormatToken& t) {
  return IsComment(verilog_tokentype(t.TokenEnum()));
}

static void SetCommentLinePartitionAsAlreadyFormatted(
    TokenPartitionTree* partition) {
  for (auto& child : partition->Children()) {
    if (!is_leaf(child)) continue;
    const auto tokens = child.Value().TokensRange();
    if (std::all_of(tokens.begin(), tokens.end(), TokenIsComment)) {
      child.Value().SetPartitionPolicy(PartitionPolicyEnum::kAlreadyFormatted);
    }
  }
}

using TokenPartitionPredicate = std::function<bool(const TokenPartitionTree&)>;

// TokenPartitionPredicate returning true for partitions whose Origin's tag is
// equal to any tag from `tags` list.
struct OriginTagIs {
  OriginTagIs(std::initializer_list<SymbolTag> tags) : tags(tags) {}

  std::initializer_list<SymbolTag> tags;

  bool operator()(const TokenPartitionTree& partition) const {
    if (!partition.Value().Origin()) return false;
    for (const auto& tag : tags) {
      if (partition.Value().Origin()->Tag() == tag) return true;
    }
    return false;
  }
};

// TokenPartitionPredicate returning true for partitions containing token with a
// type equal to any value from `token_types` list.
struct ContainsToken {
  ContainsToken(std::initializer_list<int> token_types)
      : token_types(token_types) {}

  std::initializer_list<int> token_types;

  bool operator()(const TokenPartitionTree& partition) const {
    for (const auto& token : partition.Value().TokensRange()) {
      for (const auto token_type : token_types) {
        if (token.TokenEnum() == token_type) return true;
      }
    }
    return false;
  }
};

// Finds direct child of the `parent` node satisfying the `predicate`. Returns
// nullptr when child has not been found.
static const TokenPartitionTree* FindDirectChild(
    const TokenPartitionTree* parent, TokenPartitionPredicate predicate) {
  if (!parent) return nullptr;
  auto iter = std::find_if(parent->Children().begin(), parent->Children().end(),
                           std::move(predicate));
  if (iter == parent->Children().end()) return nullptr;
  return &(*iter);
}

// Finds direct child of the `parent` node satisfying the `predicate`. Returns
// nullptr when child has not been found.
static TokenPartitionTree* FindDirectChild(TokenPartitionTree* parent,
                                           TokenPartitionPredicate predicate) {
  const TokenPartitionTree* const_parent = parent;
  return const_cast<TokenPartitionTree*>(
      FindDirectChild(const_parent, std::move(predicate)));
}

static bool LineBreaksInsidePartitionBeforeChild(
    const TokenPartitionTree& parent, const TokenPartitionTree& child) {
  for (auto& node : parent.Children()) {
    if (PartitionIsForcedIntoNewLine(node)) return true;
    if (&node == &child) break;
  }
  return false;
}

class MacroCallReshaper {
 public:
  explicit MacroCallReshaper(const FormatStyle& style,
                             TokenPartitionTree* main_node)
      : style_(style),
        main_node_(main_node),
        is_nested_(IsNested(*main_node)) {}

  void Reshape() {
    const auto* const main_node_origin = main_node_->Value().Origin();

    if (!FindInitialPartitions()) {
      // Something went wrong, leave partitions intact. Log message has been
      // printed.
      return;
    }

    if (semicolon_) {
      if (semicolon_ == NextSibling(*paren_group_)) {
        if (!PartitionIsForcedIntoNewLine(*semicolon_)) {
          // Merge with ')'
          verible::MergeLeafIntoPreviousLeaf(semicolon_);
          semicolon_ = nullptr;
        }
      }
    }

    if (ReshapeEmptyParenGroup()) {
      auto* identifier_with_paren_group = main_node_;
      if (!IsLastChild(*paren_group_)) {
        // There is a semicolon preceded by comment line(s).
        identifier_with_paren_group =
            verible::GroupLeafWithPreviousLeaf(paren_group_);
        identifier_with_paren_group->Value().SetPartitionPolicy(
            PartitionPolicyEnum::kJuxtapositionOrIndentedStack);
        main_node_->Value().SetPartitionPolicy(PartitionPolicyEnum::kStack);
      }

      if (main_node_origin &&
          main_node_origin->Tag() == NodeTag(NodeEnum::kMacroCall)) {
        // '(' must follow macro identifier in the same line
        CHECK(!PartitionIsForcedIntoNewLine(*paren_group_));
        identifier_with_paren_group->Children().clear();
        identifier_with_paren_group->Value().SetPartitionPolicy(
            PartitionPolicyEnum::kAlreadyFormatted);
      }

      return;
    }

    if ((!argument_list_ || is_leaf(*argument_list_)) &&
        (BirthRank(*r_paren_) - BirthRank(*l_paren_) > 1)) {
      // Group partitions between parentheses (comments and stuff)
      CreateArgumentList();
    }

    if (argument_list_ &&
        (is_nested_ || !PartitionIsForcedIntoNewLine(*r_paren_))) {
      // Format like a part of argument list.
      MoveRightParenToArgumentList();
    }

    if (argument_list_ && style_.try_wrap_long_lines) {
      for (auto& arg : argument_list_->Children()) {
        const auto tokens = arg.Value().TokensRange();
        // Hack: in order to avoid wrapping just before comment, do not enable
        // wrapping on partitions containing comments.
        if (is_leaf(arg) &&
            arg.Value().PartitionPolicy() !=
                PartitionPolicyEnum::kAlreadyFormatted &&
            std::none_of(tokens.begin(), tokens.end(),
                         [](const PreFormatToken& t) {
                           return IsComment(verilog_tokentype(t.TokenEnum()));
                         }))
          arg.Value().SetPartitionPolicy(PartitionPolicyEnum::kWrap);
      }
    }

    if (main_node_origin &&
        main_node_origin->Tag() == NodeTag(NodeEnum::kMacroCall)) {
      // '(' must follow macro identifier in the same line
      MergeIdentifierAndLeftParen();
    } else if (!LineBreaksInsidePartitionBeforeChild(*paren_group_,
                                                     *l_paren_)) {
      GroupIdentifierCommentsAndLeftParen();
    } else if (l_paren_ != &paren_group_->Children().front()) {
      GroupCommentsAndLeftParen();
    }

    if (argument_list_) {
      HoistOnlyChildPartition(argument_list_);
      if (paren_group_->Children().size() == 3) {
        // Children: '(', argument_list, ')'
        GroupLeftParenAndArgumentList();
      }

      if (!PartitionIsForcedIntoNewLine(paren_group_->Children().back())) {
        paren_group_->Value().SetPartitionPolicy(
            PartitionPolicyEnum::kJuxtapositionOrIndentedStack);
      } else {
        paren_group_->Value().SetPartitionPolicy(PartitionPolicyEnum::kStack);
      }
    }
  }

 private:
  static bool IsLeftParenPartition(const TokenPartitionTree& node) {
    const auto tokens = node.Value().TokensRange();
    bool found = false;
    auto token = tokens.begin();
    for (; token != tokens.end(); ++token) {
      if (token->TokenEnum() == '(') {
        found = true;
        ++token;
        break;
      }
      if (token->TokenEnum() == verilog_tokentype::TK_COMMENT_BLOCK) continue;
      return false;
    }
    for (; token != tokens.end(); ++token) {
      if (IsComment(verilog_tokentype(token->TokenEnum()))) continue;
      if (found && token->TokenEnum() == ')') continue;
      return false;
    }
    return found;
  }

  // Report a bug/untested situation, but don't abort.
  // As macro to be agnostic to logging implementation changes
  // (hopefully ABSL will provide logging soon, to reduce glog dependency)
#define LOG_BUG(log_sink, reason)                             \
  log_sink << "formatting of macro call failed: " << (reason) \
           << "\n*** Please file a bug. ***";

  bool FindInitialPartitions() {
    // Note: identifier can contain tokens for macro name + EOL comment
    identifier_ = FindDirectChild(
        main_node_, ContainsToken{verilog_tokentype::SystemTFIdentifier,
                                  verilog_tokentype::MacroCallId});
    if (!identifier_) {
      LOG_BUG(LOG(ERROR), "identifier not found.");
      return false;
    }

    paren_group_ = FindDirectChild(main_node_,
                                   OriginTagIs{NodeTag(NodeEnum::kParenGroup)});
    if (!paren_group_) {
      LOG_BUG(LOG(ERROR), "paren_group not found.");
      return false;
    }

    // Make sure there's nothing between identifier and paren group partitions.
    // Optional comment partitions are expected to be inside paren group.
    if (NextSibling(*identifier_) != paren_group_) {
      LOG_BUG(LOG(ERROR),
              "Unexpected partitions between identifier and paren_group.");
      return false;
    }

    if (!IsLastChild(*paren_group_)) {
      semicolon_ = &main_node_->Children().back();
      if (!ContainsToken{';'}(*semicolon_)) {
        LOG_BUG(LOG(ERROR), "Unexpected partition(s) after the call.");
        LOG(ERROR) << "\n" << *main_node_;
        return false;
      }
    }

    argument_list_ = FindDirectChild(
        paren_group_, OriginTagIs{NodeTag(NodeEnum::kArgumentList),
                                  NodeTag(NodeEnum::kMacroArgList)});

    if (is_leaf(*paren_group_)) {
      l_paren_ = paren_group_;
      r_paren_ = paren_group_;
    } else {
      l_paren_ = FindDirectChild(paren_group_, IsLeftParenPartition);
      if (!l_paren_) {
        LOG_BUG(LOG(ERROR), "'(' not found.");
        return false;
      }
      r_paren_ = &paren_group_->Children().back();
      if (!ContainsToken{
              ')', verilog_tokentype::MacroCallCloseToEndLine}(*r_paren_)) {
        LOG_BUG(LOG(ERROR), "')' not found.");
        return false;
      }
    }
    return true;
  }

#undef LOG_BUG

  bool ReshapeEmptyParenGroup() {
    if (paren_group_->Children().size() == 2 && l_paren_ != r_paren_ &&
        l_paren_->Value().TokensRange().end() ==
            r_paren_->Value().TokensRange().begin() &&
        !PartitionIsForcedIntoNewLine(*r_paren_)) {
      VLOG(6) << "Flatten paren group.";
      paren_group_->Children().clear();
    }
    if (is_leaf(*paren_group_)) {
      if (!PartitionIsForcedIntoNewLine(*paren_group_)) {
        main_node_->Value().SetPartitionPolicy(
            PartitionPolicyEnum::kJuxtapositionOrIndentedStack);
      }
      VLOG(6) << "Empty paren group.";
      return true;
    }

    return false;
  }

  void CreateArgumentList() {
    const int arguments_indentation =
        paren_group_->Value().IndentationSpaces() + style_.wrap_spaces;
    auto group = TokenPartitionTree(verible::UnwrappedLine(
        arguments_indentation, l_paren_->Value().TokensRange().end(),
        PartitionPolicyEnum::kWrap));
    group.Value().SpanUpToToken(r_paren_->Value().TokensRange().begin());

    const auto paren_group_begin = paren_group_->Children().begin();
    const auto args_begin = paren_group_begin + BirthRank(*l_paren_) + 1;
    const auto args_end = paren_group_begin + BirthRank(*r_paren_);
    // Move partitions into the group.
    group.Children().assign(std::make_move_iterator(args_begin),
                            std::make_move_iterator(args_end));
    for (auto& node : group.Children()) {
      verible::AdjustIndentationAbsolute(&node, arguments_indentation);
    }
    // Remove leftover entries of all grouped partitions except the first.
    paren_group_->Children().erase(args_begin + 1, args_end);

    // Move the group into first grouped partition's place.
    *args_begin = std::move(group);

    argument_list_ = &*args_begin;
    r_paren_ = NextSibling(*argument_list_);
  }

  void MoveRightParenToArgumentList() {
    {
      r_paren_->Value().SetIndentationSpaces(
          argument_list_->Value().IndentationSpaces());
      const auto r_paren_iter =
          paren_group_->Children().begin() + BirthRank(*r_paren_);
      argument_list_->Value().SpanUpToToken(
          r_paren_->Value().TokensRange().end());
      argument_list_->Children().push_back(std::move(*r_paren_));
      r_paren_ = &argument_list_->Children().back();
      paren_group_->Children().erase(r_paren_iter);
    }
    if (!PartitionIsForcedIntoNewLine(*r_paren_)) {
      // We want to avoid wrapping just before `)`. It would be best to use
      // token's break_penalty in Layout Optimizer, but that's a task for
      // future.
      // So far, use hacky heuristic: merge short partitions (things
      // like `);`) and group longer ones (anything with comments).
      const auto r_paren_tokens = r_paren_->Value().TokensRange();
      if (is_leaf(*r_paren_) &&
          std::none_of(r_paren_tokens.begin(), r_paren_tokens.end(),
                       [](const PreFormatToken& t) {
                         return IsComment(verilog_tokentype(t.TokenEnum()));
                       })) {
        // Merge
        verible::MergeLeafIntoPreviousLeaf(r_paren_);
        r_paren_ = &argument_list_->Children().back();
      } else {
        // Group
        auto& last_argument = *PreviousSibling(*r_paren_);
        auto group = TokenPartitionTree(
            verible::UnwrappedLine(last_argument.Value().IndentationSpaces(),
                                   last_argument.Value().TokensRange().begin(),
                                   PartitionPolicyEnum::kJuxtaposition));
        group.Value().SpanUpToToken(r_paren_->Value().TokensRange().end());
        group.Children().reserve(2);
        group.Children().push_back(std::move(last_argument));
        group.Children().push_back(std::move(*r_paren_));
        argument_list_->Children().erase(argument_list_->Children().end() - 1);
        argument_list_->Children().back() = std::move(group);
        r_paren_ = &argument_list_->Children().back();
      }
    }
  }

  void GroupIdentifierCommentsAndLeftParen() {
    const bool r_paren_is_in_arg_list = (r_paren_->Parent() == argument_list_);
    auto group = TokenPartitionTree(
        verible::UnwrappedLine(main_node_->Value().IndentationSpaces(),
                               identifier_->Value().TokensRange().begin(),
                               PartitionPolicyEnum::kJuxtaposition));
    group.Value().SpanUpToToken(l_paren_->Value().TokensRange().end());

    verible::AdjustIndentationAbsolute(paren_group_,
                                       main_node_->Value().IndentationSpaces());

    paren_group_->Value().SpanBackToToken(group.Value().TokensRange().begin());

    const auto old_identifier_iter =
        identifier_->Parent()->Children().begin() + BirthRank(*identifier_);
    const auto l_paren_index = BirthRank(*l_paren_);
    auto nodes_to_group = verible::iterator_range(
        paren_group_->Children().begin(),
        paren_group_->Children().begin() + l_paren_index + 1);

    const auto nodes_count = l_paren_index + 1 + 1;  // + 1 for identifier
    group.Children().reserve(nodes_count);
    // Move partitions into the group.
    group.Children().push_back(std::move(*identifier_));
    group.Children().insert(group.Children().end(),
                            std::make_move_iterator(nodes_to_group.begin()),
                            std::make_move_iterator(nodes_to_group.end()));
    // Remove leftover entries of all grouped partitions except:
    // * First paren_group's child: will be reused as a group node.
    // * The identifier: removing it will invalidate iterators; done later.
    paren_group_->Children().erase(nodes_to_group.begin() + 1,
                                   nodes_to_group.end());

    // Move the group into first grouped partition's place.
    *nodes_to_group.begin() = std::move(group);

    // Remove identifier
    main_node_->Children().erase(old_identifier_iter);

    HoistOnlyChildPartition(main_node_);

    paren_group_ = main_node_;
    identifier_ = nullptr;
    l_paren_ = &paren_group_->Children().front();
    if (argument_list_) {
      argument_list_ = NextSibling(*l_paren_);
      r_paren_ = r_paren_is_in_arg_list ? &argument_list_->Children().back()
                                        : NextSibling(*argument_list_);
    } else {
      r_paren_ = &paren_group_->Children().back();
    }
  }

  void MergeIdentifierAndLeftParen() {
    const bool r_paren_is_in_arg_list = (r_paren_->Parent() == argument_list_);
    CHECK_EQ(NextLeaf(*identifier_), l_paren_);
    CHECK(!PartitionIsForcedIntoNewLine(*l_paren_));
    verible::AdjustIndentationAbsolute(paren_group_,
                                       main_node_->Value().IndentationSpaces());
    verible::MergeLeafIntoNextLeaf(identifier_);
    HoistOnlyChildPartition(main_node_);

    paren_group_ = main_node_;
    identifier_ = nullptr;
    l_paren_ = &paren_group_->Children().front();
    if (argument_list_) {
      argument_list_ = NextSibling(*l_paren_);
      r_paren_ = r_paren_is_in_arg_list ? &argument_list_->Children().back()
                                        : NextSibling(*argument_list_);
    } else {
      r_paren_ = &paren_group_->Children().back();
    }
  }

  void GroupCommentsAndLeftParen() {
    // Group comments + '('
    auto group = TokenPartitionTree(
        verible::UnwrappedLine(paren_group_->Value().IndentationSpaces(),
                               paren_group_->Value().TokensRange().begin(),
                               PartitionPolicyEnum::kWrap));
    group.Value().SpanUpToToken(l_paren_->Value().TokensRange().end());

    const auto l_paren_index = BirthRank(*l_paren_);
    const auto paren_group_begin = paren_group_->Children().begin();
    auto nodes_to_group = verible::iterator_range(
        paren_group_begin, paren_group_begin + l_paren_index + 1);

    // Move partitions into the group.
    group.Children().assign(std::make_move_iterator(nodes_to_group.begin()),
                            std::make_move_iterator(nodes_to_group.end()));

    // Remove leftover entries of all grouped partitions except:
    // * First paren_group's child: will be reused as a group node.
    paren_group_->Children().erase(nodes_to_group.begin() + 1,
                                   nodes_to_group.end());

    // Move the group into first grouped partition's place.
    *nodes_to_group.begin() = std::move(group);

    l_paren_ = &(*nodes_to_group.begin());

    if (argument_list_) {
      argument_list_ = NextSibling(*l_paren_);
      r_paren_ = is_nested_ ? &argument_list_->Children().back()
                            : NextSibling(*argument_list_);
    } else {
      r_paren_ = &paren_group_->Children().back();
    }
  }

  void GroupLeftParenAndArgumentList() {
    auto old_argument_list_iter =
        paren_group_->Children().begin() + BirthRank(*argument_list_);

    const auto group_policy =
        PartitionIsForcedIntoNewLine(*argument_list_)
            ? PartitionPolicyEnum::kStack
            : PartitionPolicyEnum::kJuxtapositionOrIndentedStack;

    auto group = TokenPartitionTree(verible::UnwrappedLine(
        l_paren_->Value().IndentationSpaces(),
        l_paren_->Value().TokensRange().begin(), group_policy));
    group.Value().SpanUpToToken(argument_list_->Value().TokensRange().end());
    AdoptSubtree(group, std::move(*l_paren_), std::move(*argument_list_));

    *l_paren_ = std::move(group);
    paren_group_->Children().erase(old_argument_list_iter);

    l_paren_ = nullptr;
    argument_list_ = nullptr;
  }

  const FormatStyle& style_;
  TokenPartitionTree* const main_node_;

  // Subnodes of main_node_. Initialized by FindInitialPartitions().
  // Tree modifications invalidate pointers, so keeping pointers here instead
  // of passing them to each method allows for more readable code.
  TokenPartitionTree* identifier_ = nullptr;
  TokenPartitionTree* paren_group_ = nullptr;
  TokenPartitionTree* argument_list_ = nullptr;
  TokenPartitionTree* l_paren_ = nullptr;
  TokenPartitionTree* r_paren_ = nullptr;
  TokenPartitionTree* semicolon_ = nullptr;

  static bool IsNested(const TokenPartitionTree& macro_call_node) {
    auto* paren_group = FindDirectChild(
        &macro_call_node, OriginTagIs{NodeTag(NodeEnum::kParenGroup)});
    if (!paren_group) return false;
    return paren_group->Value().IndentationSpaces() ==
           macro_call_node.Value().IndentationSpaces();
  }

  const bool is_nested_;
};

static void IndentBetweenUVMBeginEndMacros(TokenPartitionTree* partition_ptr,
                                           int indentation_spaces) {
  auto& partition = *partition_ptr;
  // Indent elements between UVM begin/end macro calls
  unsigned int uvm_level = 1;
  std::vector<TokenPartitionTree*> uvm_range;

  // Search backwards for matching _begin.
  for (auto* itr = PreviousSibling(partition); itr;
       itr = PreviousSibling(*itr)) {
    VLOG(4) << "Scanning previous sibling:\n" << *itr;
    const auto macroId =
        LeftmostDescendant(*itr).Value().TokensRange().front().Text();
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

static const SyntaxTreeNode* GetAssignedExpressionFromDataDeclaration(
    const verible::Symbol& data_declaration) {
  const auto* instance_list =
      GetInstanceListFromDataDeclaration(data_declaration);
  if (!instance_list || instance_list->children().empty()) return nullptr;

  const auto& variable_or_gate = *instance_list->children().front();
  if (variable_or_gate.Tag().kind != verible::SymbolKind::kNode) return nullptr;

  const verible::Symbol* trailing_assign;
  if (variable_or_gate.Tag() == verible::NodeTag(NodeEnum::kRegisterVariable)) {
    trailing_assign =
        GetTrailingExpressionFromRegisterVariable(variable_or_gate);
  } else if (variable_or_gate.Tag() ==
             verible::NodeTag(NodeEnum::kVariableDeclarationAssignment)) {
    trailing_assign =
        GetTrailingExpressionFromVariableDeclarationAssign(variable_or_gate);
  } else {
    return nullptr;
  }
  if (!trailing_assign) return nullptr;

  const verible::Symbol* expression = verible::GetSubtreeAsSymbol(
      *trailing_assign, NodeEnum::kTrailingAssign, 1);
  if (!expression ||
      expression->Tag() != verible::NodeTag(NodeEnum::kExpression))
    return nullptr;

  return &verible::SymbolCastToNode(*expression);
}

// This phase is strictly concerned with reshaping token partitions,
// and occurs on the return path of partition tree construction.
void TreeUnwrapper::ReshapeTokenPartitions(
    const SyntaxTreeNode& node, const FormatStyle& style,
    TokenPartitionTree* recent_partition) {
  const auto tag = static_cast<NodeEnum>(node.Tag().tag);
  VLOG(3) << __FUNCTION__ << " node: " << tag;
  auto& partition = *recent_partition;
  VLOG(4) << "before reshaping " << tag << ":\n"
          << VerilogPartitionPrinter(partition);

  // Few node types that need to skip hoisting at the end of this function set
  // this flag to true.
  bool skip_singleton_hoisting = false;

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
          verible::BirthRank(instance_list_partition) == 0) {
        VLOG(4) << "No qualifiers and type is implicit";
        // This means there are no qualifiers and type was implicit,
        // so no partition precedes this.
        // Use the declaration (parent) level's indentation.
        verible::AdjustIndentationAbsolute(
            &instance_list_partition,
            data_declaration_partition.Value().IndentationSpaces());
        HoistOnlyChildPartition(&instance_list_partition);
      } else if (GetInstanceListFromDataDeclaration(node)->children().size() ==
                 1) {
        VLOG(4) << "Instance list has only one child, singleton.";

        // Undo the indentation of the only instance in the hoisted subtree.
        verible::AdjustIndentationRelative(&instance_list_partition,
                                           -style.wrap_spaces);
        VLOG(4) << "After un-indenting, instance list:\n"
                << instance_list_partition;

        // Reshape type-instance partitions.
        auto* instance_type_partition =
            ABSL_DIE_IF_NULL(PreviousSibling(children.back()));
        VLOG(4) << "instance type:\n" << *instance_type_partition;

        if (instance_type_partition == &children.front()) {
          // data_declaration_partition now consists of exactly:
          //   instance_type_partition, instance_list_partition (single
          //   instance)

          // Flatten these (which will invalidate their references).
          std::vector<size_t> offsets;
          FlattenOnlyChildrenWithChildren(data_declaration_partition, &offsets);
          // This position in the flattened result will be merged.
          const size_t fuse_position = offsets.back() - 1;

          // Join the rightmost of instance_type_partition with the leftmost of
          // instance_list_partition.  Keep the type's indentation.
          // This can yield an intermediate partition that contains:
          // ") instance_name (".
          MergeConsecutiveSiblings(&data_declaration_partition, fuse_position);
        } else {
          // There is a qualifier before instance_type_partition.
          FlattenOnlyChildrenWithChildren(data_declaration_partition);
        }
      } else {
        VLOG(4) << "None of the special cases apply.";
      }

      // Handle variable declaration with an assignment

      const auto* assigned_expr =
          GetAssignedExpressionFromDataDeclaration(node);
      if (!assigned_expr || assigned_expr->children().empty()) break;

      AttachOpeningBraceToDeclarationsAssignmentOperator(&partition);

      // Special handling for assignment of a string concatenation

      const auto* concat_expr_symbol = assigned_expr->children().front().get();
      if (concat_expr_symbol->Tag() !=
          verible::NodeTag(NodeEnum::kConcatenationExpression))
        break;

      const auto* open_range_list_symbol =
          verible::SymbolCastToNode(*concat_expr_symbol).children()[1].get();
      if (open_range_list_symbol->Tag() !=
          verible::NodeTag(NodeEnum::kOpenRangeList))
        break;

      const auto& open_range_list =
          verible::SymbolCastToNode(*open_range_list_symbol);

      if (std::all_of(
              open_range_list.children().cbegin(),
              open_range_list.children().cend(),
              [](const verible::SymbolPtr& sym) {
                if (sym->Tag() == verible::NodeTag(NodeEnum::kExpression)) {
                  const auto& expr = verible::SymbolCastToNode(*sym.get());
                  return !expr.children().empty() &&
                         expr.children().front()->Tag() ==
                             verible::LeafTag(
                                 verilog_tokentype::TK_StringLiteral);
                }
                return (sym->Kind() == verible::SymbolKind::kLeaf);
              })) {
        auto& assigned_value_partition =
            data_declaration_partition.Children()[1];
        partition.Value().SetPartitionPolicy(
            PartitionPolicyEnum::kAppendFittingSubPartitions);
        assigned_value_partition.Value().SetPartitionPolicy(
            PartitionPolicyEnum::kAlwaysExpand);
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
    case NodeEnum::kDPIExportItem:
    case NodeEnum::kDPIImportItem:
      // partition consists of (example):
      //   [import "DPI-C"]
      //   [function ... ] (task header/prototype)
      // Push the "import..." down.
      {
        verible::MergeLeafIntoNextLeaf(&partition.Children().front());
        AttachTrailingSemicolonToPreviousPartition(&partition);
        break;
      }
    case NodeEnum::kBindDirective: {
      AttachTrailingSemicolonToPreviousPartition(&partition);

      // Take advantage here that preceding data declaration partition
      // was already shaped.
      auto& target_instance_partition = partition;
      auto& children = target_instance_partition.Children();
      // Attach ')' to the instance name
      verible::MergeLeafIntoNextLeaf(PreviousSibling(children.back()));

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
        auto& last_prev = *ABSL_DIE_IF_NULL(PreviousSibling(last));
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
        auto& last_prev = *ABSL_DIE_IF_NULL(PreviousSibling(last));
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
      auto& last = RightmostDescendant(partition);
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
          auto& last = RightmostDescendant(partition);
          if (PartitionStartsWithCloseParen(last) ||
              PartitionStartsWithSemicolon(last)) {
            verible::MergeLeafIntoPreviousLeaf(&last);
          }
        }
      }
      break;
    }

    case NodeEnum::kMacroCall: {
      const auto policy = partition.Value().PartitionPolicy();
      if (policy == PartitionPolicyEnum::kStack) {
        MacroCallReshaper(style, &partition).Reshape();
        break;
      }
      // If there are no call args, join the '(' and ')' together.
      if (MacroCallArgsIsEmpty(*GetMacroCallArgs(node))) {
        // FIXME (mglb): Do more checks: EOL comments can be inside.
        FlattenOnce(partition);
        VLOG(4) << "NODE: kMacroCall (flattened):\n" << partition;
      } else {
        // Merge closing parenthesis into last argument partition
        // Test for ')' and MacroCallCloseToEndLine because macros
        // use its own token 'MacroCallCloseToEndLine'
        auto& last = RightmostDescendant(partition);
        if (PartitionStartsWithCloseParen(last) ||
            PartitionIsCloseParenSemi(last)) {
          auto& token = last.Value().TokensRange().front();
          if (token.before.break_decision ==
              verible::SpacingOptions::MustWrap) {
            auto* group = verible::GroupLeafWithPreviousLeaf(&last);
            group->Value().SetPartitionPolicy(PartitionPolicyEnum::kStack);
          } else {
            verible::MergeLeafIntoPreviousLeaf(&last);
          }
        }
      }
      break;
    }

    case NodeEnum::kRandomizeFunctionCall:
    case NodeEnum::kSystemTFCall: {
      // Function/system calls don't always have it's own section.
      // So before we do any adjuments we check that.
      // We're checking for partition policy because those two policies
      // force own partition section.
      const auto policy = partition.Value().PartitionPolicy();
      if (policy == PartitionPolicyEnum::kAppendFittingSubPartitions) {
        auto& last = RightmostDescendant(partition);
        if (PartitionStartsWithCloseParen(last) ||
            PartitionStartsWithSemicolon(last)) {
          auto& token = last.Value().TokensRange().front();
          if (token.before.break_decision ==
              verible::SpacingOptions::MustWrap) {
            auto* group = verible::GroupLeafWithPreviousLeaf(&last);
            group->Value().SetPartitionPolicy(PartitionPolicyEnum::kStack);
          } else {
            verible::MergeLeafIntoPreviousLeaf(&last);
          }
        }
      } else if (policy == PartitionPolicyEnum::kStack) {
        MacroCallReshaper(style, &partition).Reshape();
      }
      break;
    }

    case NodeEnum::kAssertionVariableDeclarationList: {
      AttachTrailingSemicolonToPreviousPartition(&partition);
      break;
    }

    case NodeEnum::kArgumentList: {
      SetCommentLinePartitionAsAlreadyFormatted(&partition);
      AttachSeparatorsToListElementPartitions(&partition);
      break;
    }

    case NodeEnum::kMacroArgList: {
      SetCommentLinePartitionAsAlreadyFormatted(&partition);
      AttachSeparatorsToListElementPartitions(&partition);
      break;
    }

    case NodeEnum::kParenGroup: {
      SetCommentLinePartitionAsAlreadyFormatted(&partition);
      break;
    }

    case NodeEnum::kGenerateIfHeader:
    case NodeEnum::kIfHeader: {
      // Fix indentation in case of e.g. function calls inside if headers
      // TODO(fangism): This should be done smarter (using CST) or removed
      //     after better handling of function calls inside expressions
      //     e.g. kBinaryExpression, kUnaryPrefixExpression...
      if (partition.Children().size() > 1) {
        auto if_header_partition_iter = std::find_if(
            partition.Children().begin(), partition.Children().end(),
            [](const TokenPartitionTree& n) {
              const auto* origin = n.Value().Origin();
              return origin && origin->Tag() ==
                                   verible::LeafTag(verilog_tokentype::TK_if);
            });
        if (if_header_partition_iter == partition.Children().end()) break;

        // Adjust indentation of all partitions following if header recursively
        for (auto& child : make_range(if_header_partition_iter + 1,
                                      partition.Children().end())) {
          verible::AdjustIndentationRelative(&child, style.wrap_spaces);
        }
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
      auto& last = RightmostDescendant(partition);
      // TODO(fangism): why does test fail without this clause?
      if (PartitionStartsWithCloseParen(last)) {
        verible::MergeLeafIntoPreviousLeaf(&last);
      }
      break;
    }

    case NodeEnum::kConstraintDeclaration: {
      // TODO(fangism): kConstraintSet should be handled similarly with {}
      if (partition.Children().size() == 2) {
        auto& last = RightmostDescendant(partition);
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
      if (children.size() == 2 &&
          verible::is_leaf(children.front()) /* left side */) {
        verible::MergeLeafIntoNextLeaf(&children.front());
        VLOG(4) << "after merge leaf (left-into-right):\n" << partition;
      }
      break;
    }
    case NodeEnum::kProceduralTimingControlStatement: {
      std::vector<size_t> offsets;
      FlattenOnlyChildrenWithChildren(partition, &offsets);
      VLOG(4) << "before moving semicolon:\n" << partition;
      AttachTrailingSemicolonToPreviousPartition(&partition);
      // Check body, for kSeqBlock, merge 'begin' with previous sibling
      if (const auto* tbody = GetProceduralTimingControlStatementBody(node);
          tbody != nullptr && NodeIsBeginEndBlock(*tbody)) {
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
      FlattenOnlyChildrenWithChildren(partition);
      VLOG(4) << "after flatten:\n" << partition;
      AttachTrailingSemicolonToPreviousPartition(&partition);
      AttachSeparatorsToListElementPartitions(&partition);
      // Merge the 'assign' keyword with the (first) x=y assignment.
      // TODO(fangism): reshape for multiple assignments.
      verible::MergeLeafIntoNextLeaf(&partition.Children().front());
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
      // Check if 'initial' is separated from 'begin' by EOL_COMMENT. If so,
      // adjust indentation and do not merge leaf into previous leaf.
      const verible::UnwrappedLine& unwrapped_line = partition.Value();
      verible::FormatTokenRange tokenrange = unwrapped_line.TokensRange();
      auto token_tmp = tokenrange.begin();
      if (token_tmp->TokenEnum() == TK_initial &&
          (++token_tmp)->TokenEnum() == TK_EOL_COMMENT &&
          (++token_tmp)->TokenEnum() == TK_begin) {
        AdjustIndentationRelative(
            &partition.Children()[(partition.Children().size() - 1)],
            style.indentation_spaces);
        break;
      }

      // In these cases, merge the 'begin' partition of the statement block
      // with the preceding keyword or header partition.
      if (NodeIsBeginEndBlock(
              verible::SymbolCastToNode(*node.children().back()))) {
        auto& seq_block_partition = partition.Children().back();
        VLOG(4) << "block partition: " << seq_block_partition;
        auto& begin_partition = LeftmostDescendant(seq_block_partition);
        VLOG(4) << "begin partition: " << begin_partition;
        CHECK(is_leaf(begin_partition));
        verible::MergeLeafIntoPreviousLeaf(&begin_partition);
        VLOG(4) << "after merging 'begin' to predecessor:\n" << partition;
        // Flatten only the statement block so that the control partition
        // can retain its own partition policy.
        FlattenOneChild(partition, verible::BirthRank(seq_block_partition));
      }
      break;
    }
    case NodeEnum::kDoWhileLoopStatement: {
      if (const auto* dw = GetDoWhileStatementBody(node);
          dw != nullptr && NodeIsBeginEndBlock(*dw)) {
        // between do... and while (...);
        auto& seq_block_partition = partition.Children()[1];

        // merge "do" <- "begin"
        auto& begin_partition = LeftmostDescendant(seq_block_partition);
        verible::MergeLeafIntoPreviousLeaf(&begin_partition);

        // merge "end" -> "while"
        auto& end_partition = RightmostDescendant(seq_block_partition);
        verible::MergeLeafIntoNextLeaf(&end_partition);

        // Flatten only the statement block so that the control partition
        // can retain its own partition policy.
        FlattenOneChild(partition, verible::BirthRank(seq_block_partition));
      }
      break;
    }

    case NodeEnum::kAlwaysStatement: {
      if (GetSubtreeAsNode(node, tag, node.children().size() - 1)
              ->MatchesTagAnyOf({NodeEnum::kProceduralTimingControlStatement,
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
      const auto* token = GetMacroGenericItemId(node);
      if (token && absl::StartsWith(token->text(), "`uvm_") &&
          absl::EndsWith(token->text(), "_end")) {
        VLOG(4) << "Found `uvm_*_end macro call";
        IndentBetweenUVMBeginEndMacros(&partition, style.indentation_spaces);
      }
      break;
    }

    case NodeEnum::kGateInstanceRegisterVariableList:
      // parent: kDataDeclaration
      skip_singleton_hoisting = true;
      [[fallthrough]];
    case NodeEnum::kActualParameterByNameList:
    case NodeEnum::kDistributionItemList:
    case NodeEnum::kEnumNameList:
    case NodeEnum::kFormalParameterList:
    case NodeEnum::kOpenRangeList:
    case NodeEnum::kPortActualList:
    case NodeEnum::kPortDeclarationList:
    case NodeEnum::kPortList:
    case NodeEnum::kVariableDeclarationAssignmentList:
    case NodeEnum::kMacroFormalParameterList: {
      AttachSeparatorsToListElementPartitions(&partition);
      break;
    }

    case NodeEnum::kModportDeclaration: {
      AttachSeparatorsToListElementPartitions(&partition);
      break;
    }

    case NodeEnum::kExpressionList:
    case NodeEnum::kUntagged: {
      AttachSeparatorsToListElementPartitions(&partition);
      skip_singleton_hoisting = true;
      break;
    }

    case NodeEnum::kParamDeclaration: {
      AttachTrailingSemicolonToPreviousPartition(&partition);
      AttachOpeningBraceToDeclarationsAssignmentOperator(&partition);
      break;
    }

    case NodeEnum::kAssignmentPattern: {
      FlattenOnlyChildrenWithChildren(partition);
      break;
    }

    case NodeEnum::kPatternExpression: {
      if (partition.Children().size() >= 3) {
        auto& colon = partition.Children()[1];
        AttachSeparatorToPreviousOrNextPartition(&colon);
      }
      FlattenOnlyChildrenWithChildren(partition);
      AttachOpeningBraceToDeclarationsAssignmentOperator(&partition);
      break;
    }

    default:
      break;
  }

  // In the majority of cases, automatically hoist singletons.
  // A few node types, however, will wish to delay the hoisting change,
  // so that the parent node can make reshaping decisions based on the
  // child node's original unhoisted form.
  if (!skip_singleton_hoisting) {
    HoistOnlyChildPartition(&partition);
  }

  VLOG(4) << "after reshaping " << tag << ":\n"
          << VerilogPartitionPrinter(partition);
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
          << VerilogPartitionPrinter(partition);

  // Advances NextUnfilteredToken(), and extends CurrentUnwrappedLine().
  // Should be equivalent to AdvanceNextUnfilteredToken().
  AddTokenToCurrentUnwrappedLine();

  // Make sure that newly extended unwrapped line has origin symbol
  // FIXME: Pretty sure that this should be handled in an other way,
  //    e.g. in common/formatting/tree_unwrapper.cc similarly
  //    to StartNewUnwrappedLine()
  if (CurrentUnwrappedLine().Origin() == nullptr &&
      CurrentUnwrappedLine().TokensRange().size() == 1) {
    CurrentUnwrappedLine().SetOrigin(&leaf);
  }

  // Look-ahead to any trailing comments that are associated with this leaf,
  // up to a newline.
  LookAheadBeyondCurrentLeaf();

  VLOG(4) << "before reshaping " << VerboseToken(leaf.get()) << ":\n"
          << VerilogPartitionPrinter(partition);

  // Post-token-handling token partition adjustments:
  switch (leaf.Tag().tag) {
    case PP_else: {
      // Do not allow non-comment tokens on the same line as `else
      // (comments were handled above)
      StartNewUnwrappedLine(PartitionPolicyEnum::kFitOnLineElseExpand, &leaf);
      break;
    }
    default:
      break;
  }

  VLOG(4) << "after reshaping " << VerboseToken(leaf.get()) << ":\n"
          << VerilogPartitionPrinter(partition);
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
