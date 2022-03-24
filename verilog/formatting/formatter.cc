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

#include "verilog/formatting/formatter.h"

#include <algorithm>
#include <cstddef>
#include <iostream>
#include <iterator>
#include <vector>

#include "absl/status/status.h"
#include "absl/status/statusor.h"
#include "common/formatting/format_token.h"
#include "common/formatting/layout_optimizer.h"
#include "common/formatting/line_wrap_searcher.h"
#include "common/formatting/token_partition_tree.h"
#include "common/formatting/unwrapped_line.h"
#include "common/formatting/verification.h"
#include "common/strings/diff.h"
#include "common/strings/line_column_map.h"
#include "common/strings/position.h"
#include "common/strings/range.h"
#include "common/text/text_structure.h"
#include "common/text/token_info.h"
#include "common/text/tree_utils.h"
#include "common/util/expandable_tree_view.h"
#include "common/util/interval.h"
#include "common/util/iterator_range.h"
#include "common/util/logging.h"
#include "common/util/range.h"
#include "common/util/spacer.h"
#include "common/util/tree_operations.h"
#include "common/util/vector_tree.h"
#include "common/util/vector_tree_iterators.h"
#include "verilog/CST/declaration.h"
#include "verilog/CST/module.h"
#include "verilog/analysis/verilog_analyzer.h"
#include "verilog/analysis/verilog_equivalence.h"
#include "verilog/formatting/align.h"
#include "verilog/formatting/comment_controls.h"
#include "verilog/formatting/format_style.h"
#include "verilog/formatting/token_annotator.h"
#include "verilog/formatting/tree_unwrapper.h"
#include "verilog/parser/verilog_token_enum.h"
#include "verilog/preprocessor/verilog_preprocess.h"

namespace verilog {
namespace formatter {
using absl::Status;
using absl::StatusCode;

using verible::ByteOffsetSet;
using verible::ExpandableTreeView;
using verible::LineNumberSet;
using verible::PartitionPolicyEnum;
using verible::TokenPartitionTree;
using verible::TreeViewNodeInfo;
using verible::UnwrappedLine;
using verible::VectorTree;
using verible::VectorTreeLeavesIterator;

typedef VectorTree<TreeViewNodeInfo<TokenPartitionTree>> partition_node_type;

// Takes a TextStructureView and FormatStyle, and formats UnwrappedLines.
class Formatter {
 public:
  Formatter(const verible::TextStructureView& text_structure,
            const FormatStyle& style)
      : text_structure_(text_structure), style_(style) {}

  // Formats the source code
  Status Format(const ExecutionControl&);

  Status Format() { return Format(ExecutionControl()); }

  void SelectLines(const LineNumberSet& lines);

  // Outputs all of the FormattedExcerpt lines to stream.
  // If "include_disabled" is false, does not contain the disabled ranges.
  void Emit(bool include_disabled, std::ostream& stream) const;

 private:
  // Contains structural information about the code to format, such as
  // TokenSequence from lexing, and ConcreteSyntaxTree from parsing
  const verible::TextStructureView& text_structure_;

  // The style configuration for the formatter
  FormatStyle style_;

  // Ranges of text where formatter is disabled (by comment directives).
  ByteOffsetSet disabled_ranges_;

  // Set of formatted lines, populated by calling Format().
  std::vector<verible::FormattedExcerpt> formatted_lines_;
};

// TODO(b/148482625): make this public/re-usable for general content comparison.
Status VerifyFormatting(const verible::TextStructureView& text_structure,
                        absl::string_view formatted_output,
                        absl::string_view filename) {
  // Verify that the formatted output creates the same lexical
  // stream (filtered) as the original.  If any tokens were lost, fall back to
  // printing the original source unformatted.
  // Note: We cannot just Tokenize() and compare because Analyze()
  // performs additional transformations like expanding MacroArgs to
  // expression subtrees.
  const auto reanalyzer = VerilogAnalyzer::AnalyzeAutomaticMode(
      formatted_output, filename, verilog::VerilogPreprocess::Config());
  const auto relex_status = ABSL_DIE_IF_NULL(reanalyzer)->LexStatus();
  const auto reparse_status = reanalyzer->ParseStatus();

  if (!relex_status.ok() || !reparse_status.ok()) {
    const auto& token_errors = reanalyzer->TokenErrorMessages();
    // Only print the first error.
    if (!token_errors.empty()) {
      return absl::DataLossError(
          absl::StrCat("Error lex/parsing-ing formatted output.  "
                       "Please file a bug.\nFirst error: ",
                       token_errors.front()));
    }
  }

  {
    // Filter out only whitespaces and compare.
    // First difference will be printed to cerr for debugging.
    std::ostringstream errstream;
    // Note: text_structure.TokenStream() and reanalyzer->Data().TokenStream()
    // contain already lexed tokens, so this comparison check is repeating the
    // work done by the lexers.
    // Should performance be a concern, we could pass in those tokens to
    // avoid lexing twice, but for now, using plain strings as an interface
    // to comparator functions is simpler and more intuitive.
    // See analysis/verilog_equivalence.cc implementation.
    if (verilog::FormatEquivalent(text_structure.Contents(), formatted_output,
                                  &errstream) != DiffStatus::kEquivalent) {
      return absl::DataLossError(absl::StrCat(
          "Formatted output is lexically different from the input.    "
          "Please file a bug.  Details:\n",
          errstream.str()));
    }
  }

  return absl::OkStatus();
}

static Status ReformatVerilogIncrementally(absl::string_view original_text,
                                           absl::string_view formatted_text,
                                           absl::string_view filename,
                                           const FormatStyle& style,
                                           std::ostream& reformat_stream,
                                           const ExecutionControl& control) {
  // Differences from the first formatting.
  const verible::LineDiffs formatting_diffs(original_text, formatted_text);
  // Added lines will be re-applied to incremental re-formatting.
  LineNumberSet formatted_lines(
      verible::DiffEditsToAddedLineNumbers(formatting_diffs.edits));
  // Even if no line were changed by formatting, need to make sure that
  // reformatting does not accidentally reformat the whole file by
  // adding an out-of-range lines interval.  This effectively disables
  // re-formatting on the whole file unless line ranges are specified.
  formatted_lines.Add(formatting_diffs.after_lines.size() + 1);
  VLOG(1) << "formatted changed lines: " << formatted_lines;
  return FormatVerilog(formatted_text, filename, style, reformat_stream,
                       formatted_lines, control);
}

static Status ReformatVerilog(absl::string_view original_text,
                              absl::string_view formatted_text,
                              absl::string_view filename,
                              const FormatStyle& style,
                              std::ostream& reformat_stream,
                              const LineNumberSet& lines,
                              const ExecutionControl& control) {
  // Disable reformat check to terminate recursion.
  ExecutionControl convergence_control(control);
  convergence_control.verify_convergence = false;

  if (lines.empty()) {
    // format whole file
    return FormatVerilog(formatted_text, filename, style, reformat_stream,
                         lines, convergence_control);
  } else {
    // reformat incrementally
    return ReformatVerilogIncrementally(original_text, formatted_text, filename,
                                        style, reformat_stream,
                                        convergence_control);
  }
}

static absl::StatusOr<std::unique_ptr<VerilogAnalyzer>> ParseWithStatus(
    absl::string_view text, absl::string_view filename) {
  std::unique_ptr<VerilogAnalyzer> analyzer =
      VerilogAnalyzer::AnalyzeAutomaticMode(
          text, filename, verilog::VerilogPreprocess::Config());
  {
    // Lex and parse code.  Exit on failure.
    const auto lex_status = ABSL_DIE_IF_NULL(analyzer)->LexStatus();
    const auto parse_status = analyzer->ParseStatus();
    if (!lex_status.ok() || !parse_status.ok()) {
      std::ostringstream errstream;
      constexpr bool with_diagnostic_context = false;
      const std::vector<std::string> syntax_error_messages(
          analyzer->LinterTokenErrorMessages(with_diagnostic_context));
      for (const auto& message : syntax_error_messages) {
        errstream << message << std::endl;
      }
      // Don't bother printing original code
      return absl::InvalidArgumentError(errstream.str());
    }
  }
  return analyzer;
}

absl::Status FormatVerilog(const verible::TextStructureView& text_structure,
                           absl::string_view filename, const FormatStyle& style,
                           std::string* formatted_text,
                           const verible::LineNumberSet& lines,
                           const ExecutionControl& control) {
  Formatter fmt(text_structure, style);
  fmt.SelectLines(lines);

  // Format code.
  Status format_status = fmt.Format(control);
  if (!format_status.ok()) {
    if (format_status.code() != StatusCode::kResourceExhausted) {
      // Some more fatal error, halt immediately.
      return format_status;
    }
    // Else allow remainder of this function to execute, and print partially
    // formatted code, but force a non-zero exit status in the end.
  }

  // In any diagnostic mode, proceed no further.
  if (control.AnyStop()) {
    return absl::CancelledError("Halting for diagnostic operation.");
  }

  // Render formatted text to the output buffer.
  std::ostringstream output_buffer;
  fmt.Emit(true, output_buffer);
  *formatted_text = output_buffer.str();

  // For now, unconditionally verify.
  if (Status verify_status =
          VerifyFormatting(text_structure, *formatted_text, filename);
      !verify_status.ok()) {
    return verify_status;
  }

  return format_status;
}

Status FormatVerilog(absl::string_view text, absl::string_view filename,
                     const FormatStyle& style, std::ostream& formatted_stream,
                     const LineNumberSet& lines,
                     const ExecutionControl& control) {
  const auto analyzer = ParseWithStatus(text, filename);
  if (!analyzer.ok()) return analyzer.status();

  const verible::TextStructureView& text_structure = analyzer->get()->Data();
  std::string formatted_text;
  Status format_status = FormatVerilog(text_structure, filename, style,
                                       &formatted_text, lines, control);
  // Commit formatted text to the output stream independent of status.
  formatted_stream << formatted_text;
  if (!format_status.ok()) return format_status;

  // When formatting whole-file (no --lines are specified), ensure that
  // the formatting transformation is convergent after one iteration.
  //   format(format(text)) == format(text)
  if (control.verify_convergence) {
    std::ostringstream reformat_stream;
    if (auto reformat_status =
            ReformatVerilog(text, formatted_text, filename, style,
                            reformat_stream, lines, control);
        !reformat_status.ok()) {
      return reformat_status;
    }
    const std::string& reformatted_text(reformat_stream.str());
    return verible::ReformatMustMatch(text, lines, formatted_text,
                                      reformatted_text);
  }
  return format_status;
}

absl::Status FormatVerilogRange(const verible::TextStructureView& structure,
                                const FormatStyle& style,
                                std::string* formatted_text,
                                const verible::Interval<int>& line_range,
                                const ExecutionControl& control) {
  if (line_range.empty()) {
    return absl::OkStatus();
  }

  Formatter fmt(structure, style);
  fmt.SelectLines({line_range});

  // Format code.
  Status format_status = fmt.Format(control);
  if (!format_status.ok()) return format_status;

  // In any diagnostic mode, proceed no further.
  if (control.AnyStop()) {
    return absl::CancelledError("Halting for diagnostic operation.");
  }

  std::ostringstream output_buffer;
  fmt.Emit(false, output_buffer);
  *formatted_text = output_buffer.str();

  // The range-format can output a spurious newline in the beginning (#1150).
  // Whitespace handling needs some rework in the formatter, and it is not
  // trivial in the current state to fix at the source.
  // However, we can easily see the effects and mitigate it here: if we see a
  // newline at the beginning of the formatted range, but no newline at the
  // beginning of the original range, erase it in the formatted output.
  // TODO(hzeller): This can go when whitespace handling is revisited.
  //                (Emit(), FormatWhitespaceWithDisabledByteRanges())
  const auto& text_lines = structure.Lines();
  const char unformatted_begin = *text_lines[line_range.min - 1].begin();
  if (!formatted_text->empty() && (*formatted_text)[0] == '\n' &&
      unformatted_begin != '\n') {
    formatted_text->erase(0, 1);  // possibly expensive.
  }

  // We don't have verification tests here as we only have a subset of code
  // so it would be more tricky. Since this output is used in interactive
  // settings such as editors, this is less of an issue.

  return absl::OkStatus();
}

absl::Status FormatVerilogRange(absl::string_view full_content,
                                absl::string_view filename,
                                const FormatStyle& style,
                                std::string* formatted_text,
                                const verible::Interval<int>& line_range,
                                const ExecutionControl& control) {
  const auto analyzer = ParseWithStatus(full_content, filename);
  if (!analyzer.ok()) return analyzer.status();
  return FormatVerilogRange(analyzer->get()->Data(), style, formatted_text,
                            line_range, control);
}

static verible::Interval<int> DisableByteOffsetRange(
    absl::string_view substring, absl::string_view superstring) {
  CHECK(!substring.empty());
  auto range = verible::SubstringOffsets(substring, superstring);
  // +1 so that formatting can still occur on the space before the start
  // of the disabled range, for example allowing for indentation adjustments.
  return {range.first + 1, range.second};
}

// Decided at each node in UnwrappedLine partition tree whether or not
// it should be expanded or unexpanded.
static void DeterminePartitionExpansion(
    partition_node_type* node,
    std::vector<verible::PreFormatToken>* preformatted_tokens,
    absl::string_view full_text, const ByteOffsetSet& disabled_ranges,
    const FormatStyle& style) {
  auto& node_view = node->Value();
  const UnwrappedLine& uwline = node_view.Value();
  VLOG(3) << "unwrapped line: " << uwline;
  const verible::FormatTokenRange ftoken_range(uwline.TokensRange());
  const auto partition_policy = uwline.PartitionPolicy();

  const auto PreserveSpaces = [&ftoken_range, &full_text,
                               preformatted_tokens]() {
    const ByteOffsetSet new_disable_range{{DisableByteOffsetRange(
        verible::make_string_view_range(ftoken_range.front().Text().begin(),
                                        ftoken_range.back().Text().end()),
        full_text)}};
    verible::PreserveSpacesOnDisabledTokenRanges(preformatted_tokens,
                                                 new_disable_range, full_text);
  };

  // Expand or not, depending on partition policy and other conditions.

  // If this is a leaf partition, there is nothing to expand.
  if (is_leaf(*node)) {
    VLOG(3) << "No children to expand.";
    node_view.Unexpand();
    if (partition_policy == PartitionPolicyEnum::kFitOnLineElseExpand &&
        !style.try_wrap_long_lines &&
        !verible::FitsOnLine(uwline, style).fits) {
      // give-up early and preserve original spacing
      VLOG(3) << "Does not fit (leaf), preserving.";
      PreserveSpaces();
    }
    return;
  }

  // If any children are expanded, then this node must be expanded,
  // regardless of the UnwrappedLine's chosen policy.
  // Thus, this function must be executed with a post-order traversal.
  const auto& children = node->Children();
  if (std::any_of(children.begin(), children.end(),
                  [](const partition_node_type& child) {
                    return child.Value().IsExpanded();
                  })) {
    VLOG(3) << "Child forces parent to expand.";
    node_view.Expand();
    return;
  }

  {
    // If any part of the range is formatting-disabled, expand this partition so
    // that whitespace between subpartitions can be handled accordingly in
    // Formatter::Emit().
    const verible::FormatTokenRange range(uwline.TokensRange());
    const verible::IntervalSet<int> partition_byte_range{
        {range.front().token->left(full_text),
         range.back().token->right(full_text)}};
    // (Same as IntervalSet::Complement without temporary copy.)
    verible::IntervalSet<int> diff(partition_byte_range);
    diff.Difference(disabled_ranges);
    if (partition_byte_range != diff) {
      // Then some sub-interval was removed.
      VLOG(3) << "Partition @bytes " << partition_byte_range
              << " is partially format-disabled, so expand.";
      node_view.Expand();
      return;
    }
  }

  VLOG(3) << "partition policy: " << partition_policy;
  switch (partition_policy) {
    case PartitionPolicyEnum::kUninitialized: {
      LOG(FATAL) << "Got an uninitialized partition policy at: " << uwline;
      break;
    }
    case PartitionPolicyEnum::kAlwaysExpand: {
      if (children.size() > 1) {
        node_view.Expand();
      }
      break;
    }
    case PartitionPolicyEnum::kTabularAlignment: {
      if (uwline.Origin()->Tag().tag ==
          static_cast<int>(NodeEnum::kArgumentList)) {
        // Check whether the whole function call fits on one line. If possible,
        // unexpand and fit into one line. Otherwise expand argument list with
        // tabular alignment.
        auto& node_view_parent = node->Parent()->Value();
        const UnwrappedLine& uwline_parent = node_view_parent.Value();
        if (verible::FitsOnLine(uwline_parent, style).fits) {
          node_view.Unexpand();
          break;
        }
      }

      if (children.size() > 1) {
        node_view.Expand();
      }
      break;
    }

    case PartitionPolicyEnum::kAlreadyFormatted: {
      // All partitions with this policy should be leafs at this point, which
      // are handled above. Just in case - unexpand.
      node_view.Unexpand();
      break;
    }

    case PartitionPolicyEnum::kInline: {
      // All partitions with this policy should be already applied and removed
      // by calls to ApplyAlreadyFormattedPartitionPropertiesToTokens on their
      // parent partitions.
      LOG(FATAL) << "Unreachable. " << partition_policy;
      break;
    }

    // Try to fit kAppendFittingSubPartitions partition into single line.
    // If it doesn't fit expand to grouped nodes.
    case PartitionPolicyEnum::kAppendFittingSubPartitions: {
      // !style.try_wrap_long_lines was already handled above
      if (verible::FitsOnLine(uwline, style).fits) {
        VLOG(3) << "Fits, un-expanding.";
        node_view.Unexpand();
      } else {
        VLOG(3) << "Does not fit, expanding.";
        node_view.Expand();
      }
      break;
    }

    case PartitionPolicyEnum::kFitOnLineElseExpand: {
      if (uwline.Origin() &&
          (uwline.Origin()->Tag().tag ==
               static_cast<int>(NodeEnum::kNetVariableAssignment) ||
           uwline.Origin()->Tag().tag ==
               static_cast<int>(NodeEnum::kBlockItemStatementList) ||
           uwline.Origin()->Tag().tag ==
               static_cast<int>(NodeEnum::kBlockingAssignmentStatement))) {
        // Align unnamed parameters in function call. Example:
        // always_comb begin
        //   value = function_name(8'hA, signal,
        //                         signal_1234);
        // end
        const auto& children_tmp = node->Children();
        auto look_for_arglist = [](const partition_node_type& child) {
          const auto& node_view_child = child.Value();
          const UnwrappedLine& uwline_child = node_view_child.Value();
          return (uwline_child.Origin() &&
                  uwline_child.Origin()->Kind() == verible::SymbolKind::kNode &&
                  verible::SymbolCastToNode(*uwline_child.Origin())
                      .MatchesTag(NodeEnum::kArgumentList));
        };

        // Check if kNetVariableAssignment or kBlockItemStatementList contains
        // kArgumentList node
        if (std::any_of(children_tmp.begin(), children_tmp.end(),
                        look_for_arglist)) {
          node_view.Unexpand();
          break;
        }
      }

      if (verible::FitsOnLine(uwline, style).fits) {
        VLOG(3) << "Fits, un-expanding.";
        node_view.Unexpand();
      } else {
        VLOG(3) << "Does not fit, expanding.";
        node_view.Expand();
      }
      break;
    }

    case PartitionPolicyEnum::kJuxtapositionOrIndentedStack:
    case PartitionPolicyEnum::kJuxtaposition:
    case PartitionPolicyEnum::kStack:
    case PartitionPolicyEnum::kWrap: {
      // The policies are handled (and replaced) in Layout Optimizer.
      LOG(FATAL) << "Unreachable. " << partition_policy;
      break;
    }
  }
}

// Produce a worklist of independently formattable UnwrappedLines.
static std::vector<UnwrappedLine> MakeUnwrappedLinesWorklist(
    const FormatStyle& style, absl::string_view full_text,
    const ByteOffsetSet& disabled_ranges,
    const TokenPartitionTree& format_tokens_partitions,
    std::vector<verible::PreFormatToken>* preformatted_tokens) {
  // Initialize a tree view that treats partitions as fully-expanded.
  ExpandableTreeView<TokenPartitionTree> format_tokens_partition_view(
      format_tokens_partitions);

  // For unwrapped lines that fit, don't bother expanding their partitions.
  // Post-order traversal: if a child doesn't 'fit' and needs to be expanded,
  // so must all of its parents (and transitively, ancestors).
  format_tokens_partition_view.ApplyPostOrder(
      [&full_text, &disabled_ranges, &style,
       preformatted_tokens](partition_node_type& node) {
        DeterminePartitionExpansion(&node, preformatted_tokens, full_text,
                                    disabled_ranges, style);
      });

  // Remove trailing blank lines.
  std::vector<UnwrappedLine> unwrapped_lines(
      format_tokens_partition_view.begin(), format_tokens_partition_view.end());
  while (!unwrapped_lines.empty() && unwrapped_lines.back().IsEmpty()) {
    unwrapped_lines.pop_back();
  }
  return unwrapped_lines;
}

static void PrintLargestPartitions(
    std::ostream& stream, const TokenPartitionTree& token_partitions,
    size_t max_partitions, const verible::LineColumnMap& line_column_map,
    absl::string_view base_text) {
  stream << "Showing the " << max_partitions
         << " largest (leaf) token partitions:" << std::endl;
  const auto ranked_partitions =
      FindLargestPartitions(token_partitions, max_partitions);
  const verible::Spacer hline(80, '=');
  for (const auto& partition : ranked_partitions) {
    stream << hline << "\n[" << partition->Size() << " tokens";
    if (!partition->IsEmpty()) {
      stream << ", starting at line:col "
             << line_column_map.GetLineColAtOffset(
                    base_text,
                    partition->TokensRange().front().token->left(base_text));
    }
    stream << "]: " << *partition << std::endl;
  }
  stream << hline << std::endl;
}

std::ostream& ExecutionControl::Stream() const {
  return (stream != nullptr) ? *stream : std::cout;
}

void Formatter::SelectLines(const LineNumberSet& lines) {
  disabled_ranges_ = EnabledLinesToDisabledByteRanges(
      lines, text_structure_.GetLineColumnMap());
}

// Given control flags and syntax tree, selectively disable some ranges
// of text from formatting.  This provides an easy way to preserve spacing on
// selected syntax subtrees to reduce formatter harm while allowing
// development to progress.
static void DisableSyntaxBasedRanges(ByteOffsetSet* disabled_ranges,
                                     const verible::Symbol& root,
                                     const FormatStyle& style,
                                     absl::string_view full_text) {
  /**
  // Basic template:
  if (!style.controlling_flag) {
    for (const auto& match : FindAllSyntaxTreeNodeTypes(root)) {
      // Refine search into specific subtrees, if applicable.
      // Convert the spanning string_views into byte offset ranges to disable.
      const auto inst_text = verible::StringSpanOfSymbol(*match.match);
      VLOG(4) << "disabled: " << inst_text;
      disabled_ranges->Add(DisableByteOffsetRange(inst_text, full_text));
    }
  }
  **/
}

// Keeps multi-line EOL comments aligned to the same column.
//
// When a line containing nothing else than a single EOL comment follows a line
// containing any tokens and an EOL comment, starting columns (from original,
// unformatted source code) of both comments are compared. If the columns differ
// no more than kMaxColumnDifference, the comment in the comment-only line is
// considered to be a continuation comment. The same check is performed on all
// following comment-only lines. The process of continuation detection ends when
// currently handled line has any tokens other than EOL comment, or when the
// comment's starting column differs too much from the first comment's column.
// All continuation comments are placed in the same column as their starting
// comment's column in formatted output.
class ContinuationCommentAligner {
  // Maximum accepted difference between continuation and starting comments'
  // starting columns
  static constexpr int kMaxColumnDifference = 1;

 public:
  ContinuationCommentAligner(const verible::LineColumnMap& line_column_map,
                             const absl::string_view base_text)
      : line_column_map_(line_column_map), base_text_(base_text) {}

  // Takes the next line that has to be formatted and a vector of already
  // formatted lines.
  //
  // Continuation comment lines are formatted and appended to
  // already_formatted_lines. In case of all other lines neither the line nor
  // already_formatted_lines are modified.
  // Return value informs whether the line has been formatted and added
  // to already_formatted_lines.
  bool HandleLine(
      const UnwrappedLine& uwline,
      std::vector<verible::FormattedExcerpt>* already_formatted_lines) {
    VLOG(4) << __FUNCTION__ << ": " << uwline;

    if (already_formatted_lines->empty()) {
      VLOG(4) << "Not a continuation comment line: first line";
      return false;
    }

    if (uwline.Size() != 1 || uwline.TokensRange().back().TokenEnum() !=
                                  verilog_tokentype::TK_EOL_COMMENT) {
      VLOG(4) << "Not a continuation comment line: "
              << "does not consist of a single EOL comment.";
      formatted_column_ = kInvalidColumn;
      original_column_ = kInvalidColumn;
      return false;
    }

    const auto& previous_line = already_formatted_lines->back();
    VLOG(4) << __FUNCTION__ << ": previous line: " << previous_line;
    if (original_column_ == kInvalidColumn) {
      if (previous_line.Tokens().size() <= 1) {
        VLOG(4) << "Not a continuation comment line: "
                << "too few tokens in previous line.";
        return false;
      }
      const auto* previous_comment = previous_line.Tokens().back().token;
      if (previous_comment->token_enum() != verilog_tokentype::TK_EOL_COMMENT) {
        VLOG(4) << "Not a continuation comment line: "
                << "no EOL comment in previous line.";
        return false;
      }
      original_column_ = GetTokenColumn(previous_comment);
    }

    const auto* comment = uwline.TokensRange().back().token;
    const int comment_column = GetTokenColumn(comment);

    VLOG(4) << "Original column: " << original_column_ << " vs. "
            << comment_column;

    if (std::abs(original_column_ - comment_column) > kMaxColumnDifference) {
      VLOG(4) << "Not a continuation comment line: "
              << "starting column difference is too big";
      original_column_ = kInvalidColumn;
      return false;
    }

    VLOG(4) << "Continuation comment line - finalizing formatting";
    if (formatted_column_ == kInvalidColumn)
      formatted_column_ = CalculateEolCommentColumn(previous_line);

    UnwrappedLine aligned_uwline(uwline);
    aligned_uwline.SetIndentationSpaces(formatted_column_);
    already_formatted_lines->emplace_back(aligned_uwline);

    return true;
  }

 private:
  int GetTokenColumn(const verible::TokenInfo* token) {
    CHECK_NOTNULL(token);
    const int column =
        line_column_map_.GetLineColAtOffset(base_text_, token->left(base_text_))
            .column;
    CHECK_GE(column, 0);
    return column;
  }

  static void AdjustColumnUsingTokenSpacing(
      const verible::FormattedToken& token, int* column) {
    switch (token.before.action) {
      case verible::SpacingDecision::Preserve: {
        if (token.before.preserved_space_start != nullptr)
          *column += token.OriginalLeadingSpaces().length();
        else
          *column += token.before.spaces;
        break;
      }
      case verible::SpacingDecision::Wrap:
        *column = 0;
        ABSL_FALLTHROUGH_INTENDED;
      case verible::SpacingDecision::Align:
      case verible::SpacingDecision::Append:
        *column += token.before.spaces;
        break;
    }
  }

  static int CalculateEolCommentColumn(const verible::FormattedExcerpt& line) {
    int column = 0;
    const auto& front = line.Tokens().front();

    if (front.before.action != verible::SpacingDecision::Preserve)
      column += line.IndentationSpaces();
    if (front.before.action == verible::SpacingDecision::Align)
      column += front.before.spaces;
    column += front.token->text().length();

    for (const auto& ftoken : verible::make_range(line.Tokens().begin() + 1,
                                                  line.Tokens().end() - 1)) {
      AdjustColumnUsingTokenSpacing(ftoken, &column);
      column += ftoken.token->text().length();
    }
    AdjustColumnUsingTokenSpacing(line.Tokens().back(), &column);

    CHECK_GE(column, 0);
    return column;
  }

  const verible::LineColumnMap& line_column_map_;
  const absl::string_view base_text_;

  // Used when the most recenly handled line can't have a continuation comment.
  static constexpr int kInvalidColumn = -1;

  // Starting column of current comment group in original source code.
  int original_column_ = kInvalidColumn;
  // Starting column of current comment group in formatted source code.
  int formatted_column_ = kInvalidColumn;
};

Status Formatter::Format(const ExecutionControl& control) {
  const absl::string_view full_text(text_structure_.Contents());
  const auto& token_stream(text_structure_.TokenStream());

  // Initialize auxiliary data needed for TreeUnwrapper.
  UnwrapperData unwrapper_data(token_stream);

  // Partition input token stream into hierarchical set of UnwrappedLines.
  TreeUnwrapper tree_unwrapper(text_structure_, style_,
                               unwrapper_data.preformatted_tokens);

  const TokenPartitionTree* format_tokens_partitions = nullptr;
  // TODO(fangism): The following block could be parallelized because
  // full-partitioning does not depend on format annotations.
  {
    // Annotate inter-token information between all adjacent PreFormatTokens.
    // This must be done before any decisions about ExpandableTreeView
    // can be made because they depend on minimum-spacing, and must-break.
    AnnotateFormattingInformation(style_, text_structure_,
                                  &unwrapper_data.preformatted_tokens);

    // Determine ranges of disabling the formatter, based on comment controls.
    disabled_ranges_.Union(DisableFormattingRanges(full_text, token_stream));

    // Find disabled formatting ranges for specific syntax tree node types.
    // These are typically temporary workarounds for sections that users
    // habitually prefer to format themselves.
    if (const auto& root = text_structure_.SyntaxTree()) {
      DisableSyntaxBasedRanges(&disabled_ranges_, *root, style_, full_text);
    }

    // Disable formatting ranges.
    verible::PreserveSpacesOnDisabledTokenRanges(
        &unwrapper_data.preformatted_tokens, disabled_ranges_, full_text);

    // Partition PreFormatTokens into candidate unwrapped lines.
    format_tokens_partitions = tree_unwrapper.Unwrap();
  }

  {
    // For debugging only: identify largest leaf partitions, and stop.
    if (control.show_token_partition_tree) {
      control.Stream() << "Full token partition tree:\n"
                       << verible::TokenPartitionTreePrinter(
                              *format_tokens_partitions,
                              control.show_inter_token_info)
                       << std::endl;
    }
    if (control.show_largest_token_partitions != 0) {
      PrintLargestPartitions(control.Stream(), *format_tokens_partitions,
                             control.show_largest_token_partitions,
                             text_structure_.GetLineColumnMap(), full_text);
    }
    if (control.AnyStop()) {
      return absl::OkStatus();
    }
  }

  {  // In this pass, perform additional modifications to the partitions and
     // spacings.
    tree_unwrapper.ApplyPreOrder([&](TokenPartitionTree& node) {
      const auto& uwline = node.Value();
      const auto partition_policy = uwline.PartitionPolicy();

      switch (partition_policy) {
        case PartitionPolicyEnum::kAppendFittingSubPartitions:
          // Reshape partition tree with kAppendFittingSubPartitions policy
          verible::ReshapeFittingSubpartitions(style_, &node);
          break;
        case PartitionPolicyEnum::kJuxtaposition:
        case PartitionPolicyEnum::kStack:
        case PartitionPolicyEnum::kWrap:
        case PartitionPolicyEnum::kJuxtapositionOrIndentedStack:
          verible::OptimizeTokenPartitionTree(style_, &node);
          break;
        case PartitionPolicyEnum::kTabularAlignment:
          // TODO(b/145170750): Adjust inter-token spacing to achieve alignment,
          // but leave partitioning intact.
          // This relies on inter-token spacing having already been annotated.
          TabularAlignTokenPartitions(style_, full_text, disabled_ranges_,
                                      &node);
          break;
        default:
          break;
      }
    });
  }

  // Apply token spacing from partitions to tokens. This is permanent, so it
  // must be done after all reshaping is done.
  {
    auto* root = tree_unwrapper.CurrentTokenPartition();
    auto node_iter = VectorTreeLeavesIterator(&LeftmostDescendant(*root));
    const auto end = ++VectorTreeLeavesIterator(&RightmostDescendant(*root));

    // Iterate over leaves. kAlreadyFormatted partitions are either leaves
    // themselves or parents of leaf partitions with kInline policy.
    for (; node_iter != end; ++node_iter) {
      const auto partition_policy = node_iter->Value().PartitionPolicy();

      if (partition_policy == PartitionPolicyEnum::kAlreadyFormatted) {
        verible::ApplyAlreadyFormattedPartitionPropertiesToTokens(
            &(*node_iter), &unwrapper_data.preformatted_tokens);
      } else if (partition_policy == PartitionPolicyEnum::kInline) {
        auto* parent = node_iter->Parent();
        CHECK_NOTNULL(parent);
        CHECK_EQ(parent->Value().PartitionPolicy(),
                 PartitionPolicyEnum::kAlreadyFormatted);
        // This removes the node pointed to by node_iter (and all other
        // siblings)
        verible::ApplyAlreadyFormattedPartitionPropertiesToTokens(
            parent, &unwrapper_data.preformatted_tokens);
        // Move to the parent which is now a leaf
        node_iter = verible::VectorTreeLeavesIterator(parent);
      }
    }
  }

  // Produce sequence of independently operable UnwrappedLines.
  const auto unwrapped_lines = MakeUnwrappedLinesWorklist(
      style_, full_text, disabled_ranges_, *format_tokens_partitions,
      &unwrapper_data.preformatted_tokens);

  // For each UnwrappedLine: minimize total penalty of wrap/break decisions.
  // TODO(fangism): This could be parallelized if results are written
  // to their own 'slots'.
  std::vector<const UnwrappedLine*> partially_formatted_lines;
  formatted_lines_.reserve(unwrapped_lines.size());
  ContinuationCommentAligner continuation_comment_aligner(
      text_structure_.GetLineColumnMap(), text_structure_.Contents());
  for (const auto& uwline : unwrapped_lines) {
    // TODO(fangism): Use different formatting strategies depending on
    // uwline.PartitionPolicy().
    if (continuation_comment_aligner.HandleLine(uwline, &formatted_lines_)) {
    } else if (uwline.PartitionPolicy() ==
               PartitionPolicyEnum::kAlreadyFormatted) {
      // For partitions that were successfully aligned, do not search
      // line-wrapping, but instead accept the adjusted padded spacing.
      formatted_lines_.emplace_back(uwline);
    } else {
      // In other case, default to searching for optimal line wrapping.
      const auto optimal_solutions =
          verible::SearchLineWraps(uwline, style_, control.max_search_states);
      if (control.show_equally_optimal_wrappings &&
          optimal_solutions.size() > 1) {
        verible::DisplayEquallyOptimalWrappings(control.Stream(), uwline,
                                                optimal_solutions);
      }
      // Arbitrarily choose the first solution, if there are multiple.
      formatted_lines_.push_back(optimal_solutions.front());
      if (!formatted_lines_.back().CompletedFormatting()) {
        // Copy over any lines that did not finish wrap searching.
        partially_formatted_lines.push_back(&uwline);
      }
    }
  }

  // Report any unwrapped lines that failed to complete wrap searching.
  if (!partially_formatted_lines.empty()) {
    std::ostringstream err_stream;
    err_stream << "*** Some token partitions failed to complete within the "
                  "search limit:"
               << std::endl;
    for (const auto* line : partially_formatted_lines) {
      err_stream << *line << std::endl;
    }
    err_stream << "*** end of partially formatted partition list" << std::endl;
    // Treat search state limit like a limited resource.
    return absl::ResourceExhaustedError(err_stream.str());
  }

  return absl::OkStatus();
}

void Formatter::Emit(bool include_disabled, std::ostream& stream) const {
  const absl::string_view full_text(text_structure_.Contents());
  std::function<bool(const verible::TokenInfo&)> include_token_p;
  if (include_disabled) {
    include_token_p = [](const verible::TokenInfo&) { return true; };
  } else {
    include_token_p = [this, &full_text](const verible::TokenInfo& tok) {
      return !disabled_ranges_.Contains(tok.left(full_text));
    };
  }

  int position = 0;  // tracks with the position in the original full_text
  for (const verible::FormattedExcerpt& line : formatted_lines_) {
    // TODO(fangism): The handling of preserved spaces before tokens is messy:
    // some of it is handled here, some of it is inside FormattedToken.
    // TODO(mglb): Test empty line handling when this method becomes testable.
    const auto front_offset =
        line.Tokens().empty() ? position
                              : line.Tokens().front().token->left(full_text);
    const absl::string_view leading_whitespace(
        full_text.substr(position, front_offset - position));
    FormatWhitespaceWithDisabledByteRanges(full_text, leading_whitespace,
                                           disabled_ranges_, include_disabled,
                                           stream);

    // When front of first token is format-disabled, the previous call will
    // already cover the space up to the front token, in which case,
    // the left-indentation for this line should be suppressed to avoid
    // being printed twice.
    if (!line.Tokens().empty()) {
      line.FormattedText(stream, !disabled_ranges_.Contains(front_offset),
                         include_token_p);
      position = line.Tokens().back().token->right(full_text);
    }
  }

  // Handle trailing spaces after last token.
  const absl::string_view trailing_whitespace(full_text.substr(position));
  FormatWhitespaceWithDisabledByteRanges(full_text, trailing_whitespace,
                                         disabled_ranges_, include_disabled,
                                         stream);
}

}  // namespace formatter
}  // namespace verilog
