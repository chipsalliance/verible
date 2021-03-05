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
#include "common/formatting/format_token.h"
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
#include "common/util/process.h"
#include "common/util/range.h"
#include "common/util/spacer.h"
#include "common/util/vector_tree.h"
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

namespace verilog {
namespace formatter {
using absl::Status;
using absl::StatusCode;

using verible::ByteOffsetSet;
using verible::ExpandableTreeView;
using verible::LineNumberSet;
using verible::MutableFormatTokenRange;
using verible::PartitionPolicyEnum;
using verible::TokenPartitionTree;
using verible::TreeViewNodeInfo;
using verible::UnwrappedLine;
using verible::VectorTree;

typedef VectorTree<TreeViewNodeInfo<UnwrappedLine>> partition_node_type;

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
  void Emit(std::ostream& stream) const;

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
  const auto reanalyzer =
      VerilogAnalyzer::AnalyzeAutomaticMode(formatted_output, filename);
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

Status FormatVerilog(absl::string_view text, absl::string_view filename,
                     const FormatStyle& style, std::ostream& formatted_stream,
                     const LineNumberSet& lines,
                     const ExecutionControl& control) {
  const auto analyzer = VerilogAnalyzer::AnalyzeAutomaticMode(text, filename);
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

  const verible::TextStructureView& text_structure = analyzer->Data();
  Formatter fmt(text_structure, style);
  fmt.SelectLines(lines);

  // Format code.
  const Status format_status = fmt.Format(control);
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

  // Render formatted text to a temporary buffer, so that it can be verified.
  std::ostringstream output_buffer;
  fmt.Emit(output_buffer);
  const std::string& formatted_text(output_buffer.str());

  // Commit verified formatted text to the output stream.
  formatted_stream << formatted_text;

  // For now, unconditionally verify.
  const Status verify_status =
      VerifyFormatting(text_structure, formatted_text, filename);
  if (!verify_status.ok()) {
    return verify_status;
  }

  // When formatting whole-file (no --lines are specified), ensure that
  // the formatting transformation is convergent after one iteration.
  //   format(format(text)) == format(text)
  if (control.verify_convergence) {
    std::ostringstream reformat_stream;
    const auto reformat_status = ReformatVerilog(
        text, formatted_text, filename, style, reformat_stream, lines, control);
    if (!reformat_status.ok()) {
      return reformat_status;
    }
    const std::string& reformatted_text(reformat_stream.str());
    return verible::ReformatMustMatch(text, lines, formatted_text,
                                      reformatted_text);
  }
  return format_status;
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
  if (node->is_leaf()) {
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
    // Always view tabular aligned partitions in expanded form.
    case PartitionPolicyEnum::kTabularAlignment:
    case PartitionPolicyEnum::kAlwaysExpand: {
      if (children.size() > 1) {
        node_view.Expand();
      }
      break;
    }
    case PartitionPolicyEnum::kSuccessfullyAligned:
      VLOG(3) << "Aligned fits, un-expanding.";
      node_view.Unexpand();
      break;
    // Try to fit kAppendFittingSubPartitions partition into single line.
    // If it doesn't fit expand to grouped nodes.
    case PartitionPolicyEnum::kAppendFittingSubPartitions:
    case PartitionPolicyEnum::kFitOnLineElseExpand: {
      // !style.try_wrap_long_lines was already handled above
      if (verible::FitsOnLine(uwline, style).fits) {
        VLOG(3) << "Fits, un-expanding.";
        node_view.Unexpand();
      } else {
        VLOG(3) << "Does not fit, expanding.";
        node_view.Expand();
      }
    }
  }
}

// Produce a worklist of independently formattable UnwrappedLines.
static std::vector<UnwrappedLine> MakeUnwrappedLinesWorklist(
    const TokenPartitionTree& format_tokens_partitions,
    std::vector<verible::PreFormatToken>* preformatted_tokens,
    absl::string_view full_text, const ByteOffsetSet& disabled_ranges,
    const FormatStyle& style) {
  // Initialize a tree view that treats partitions as fully-expanded.
  ExpandableTreeView<UnwrappedLine> format_tokens_partition_view(
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
             << line_column_map(
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
          verible::ReshapeFittingSubpartitions(&node, style_);
          break;
        case PartitionPolicyEnum::kTabularAlignment:
          // TODO(b/145170750): Adjust inter-token spacing to achieve alignment,
          // but leave partitioning intact.
          // This relies on inter-token spacing having already been annotated.
          TabularAlignTokenPartitions(&node,
                                      &unwrapper_data.preformatted_tokens,
                                      full_text, disabled_ranges_, style_);
          break;
        default:
          break;
      }
    });
  }

  // Produce sequence of independently operable UnwrappedLines.
  const auto unwrapped_lines = MakeUnwrappedLinesWorklist(
      *format_tokens_partitions, &unwrapper_data.preformatted_tokens, full_text,
      disabled_ranges_, style_);

  // For each UnwrappedLine: minimize total penalty of wrap/break decisions.
  // TODO(fangism): This could be parallelized if results are written
  // to their own 'slots'.
  std::vector<const UnwrappedLine*> partially_formatted_lines;
  formatted_lines_.reserve(unwrapped_lines.size());
  for (const auto& uwline : unwrapped_lines) {
    // TODO(fangism): Use different formatting strategies depending on
    // uwline.PartitionPolicy().
    if (uwline.PartitionPolicy() == PartitionPolicyEnum::kSuccessfullyAligned) {
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

void Formatter::Emit(std::ostream& stream) const {
  const absl::string_view full_text(text_structure_.Contents());
  int position = 0;  // tracks with the position in the original full_text
  for (const auto& line : formatted_lines_) {
    // TODO(fangism): The handling of preserved spaces before tokens is messy:
    // some of it is handled here, some of it is inside FormattedToken.
    const auto front_offset = line.Tokens().front().token->left(full_text);
    const absl::string_view leading_whitespace(
        full_text.substr(position, front_offset - position));
    FormatWhitespaceWithDisabledByteRanges(full_text, leading_whitespace,
                                           disabled_ranges_, stream);
    // When front of first token is format-disabled, the previous call will
    // already cover the space up to the front token, in which case,
    // the left-indentation for this line should be suppressed to avoid
    // being printed twice.
    line.FormattedText(stream, !disabled_ranges_.Contains(front_offset));
    position = line.Tokens().back().token->right(full_text);
  }
  // Handle trailing spaces after last token.
  const absl::string_view trailing_whitespace(full_text.substr(position));
  FormatWhitespaceWithDisabledByteRanges(full_text, trailing_whitespace,
                                         disabled_ranges_, stream);
}

}  // namespace formatter
}  // namespace verilog
