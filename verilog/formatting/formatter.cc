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
#include <cstdlib>
#include <functional>
#include <iostream>
#include <iterator>
#include <memory>
#include <sstream>
#include <string>
#include <vector>

#include "absl/algorithm/container.h"
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
#include "verilog/analysis/flow_tree.h"
#include "verilog/analysis/verilog_analyzer.h"
#include "verilog/analysis/verilog_equivalence.h"
#include "verilog/formatting/align.h"
#include "verilog/formatting/comment_controls.h"
#include "verilog/formatting/format_style.h"
#include "verilog/formatting/token_annotator.h"
#include "verilog/formatting/tree_unwrapper.h"
#include "verilog/parser/verilog_lexer.h"
#include "verilog/parser/verilog_token_classifications.h"
#include "verilog/parser/verilog_token_enum.h"

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

using partition_node_type = VectorTree<TreeViewNodeInfo<TokenPartitionTree>>;

using PreprocessConfigVariants = std::vector<VerilogPreprocess::Config>;

// Takes a TextStructureView and FormatStyle, and formats UnwrappedLines.
class Formatter {
 public:
  Formatter(const absl::string_view text, const FormatStyle& style,
            const PreprocessConfigVariants& preproc_config_variants_ = {{}})
      : text_(text),
        preproc_config_variants_(preproc_config_variants_),
        style_(style) {}

  // Formats the source code
  Status Format(const ExecutionControl&, const verible::LineNumberSet&);

  Status Format() { return Format(ExecutionControl(), {}); }

  // Outputs all of the FormattedExcerpt lines to stream.
  // If "include_disabled" is false, does not contain the disabled ranges.
  void Emit(bool include_disabled, std::ostream& stream) const;

 private:
  // The original text to format
  absl::string_view text_;

  // Preprocessor configuration variants to use for formatting.
  std::vector<VerilogPreprocess::Config> preproc_config_variants_;

  // The style configuration for the formatter
  FormatStyle style_;

  // Contains structural information about the code to format, such as
  // TokenSequence from lexing, and ConcreteSyntaxTree from parsing
  std::vector<std::unique_ptr<verible::TextStructure>> text_structures_;

  // Ranges of text where formatter is disabled (by comment directives).
  ByteOffsetSet disabled_ranges_;

  // Set of formatted lines, populated by calling Format().
  std::vector<verible::FormattedExcerpt> formatted_lines_;
};

// TODO(b/148482625): make this public/re-usable for general content comparison.
Status VerifyFormatting(
    absl::string_view text, absl::string_view formatted_output,
    absl::string_view filename,
    const PreprocessConfigVariants& preproc_config_variants = {{}}) {
  // Verify that the formatted output creates the same lexical
  // stream (filtered) as the original.  If any tokens were lost, fall back to
  // printing the original source unformatted.
  // Note: We cannot just Tokenize() and compare because Analyze()
  // performs additional transformations like expanding MacroArgs to
  // expression subtrees.
  for (const VerilogPreprocess::Config& config : preproc_config_variants) {
    {
      const auto reanalyzer = VerilogAnalyzer::AnalyzeAutomaticMode(
          formatted_output, filename, config);
      const auto relex_status = ABSL_DIE_IF_NULL(reanalyzer)->LexStatus();
      const auto reparse_status = reanalyzer->ParseStatus();

      if (!relex_status.ok() || !reparse_status.ok()) {
        const auto& token_errors = reanalyzer->TokenErrorMessages();
        // Only print the first error.
        if (!token_errors.empty()) {
          return absl::DataLossError(
              absl::StrCat("Error lexing/parsing formatted output.  "
                           "Please file a bug.\nFirst error: ",
                           token_errors.front()));
        }
      }
    }

    {
      // Filter out only whitespaces and compare.
      // First difference will be printed to cerr for debugging.
      std::ostringstream errstream;
      // See analysis/verilog_equivalence.cc implementation.
      if (verilog::FormatEquivalent(text, formatted_output, &errstream) !=
          DiffStatus::kEquivalent) {
        return absl::DataLossError(absl::StrCat(
            "Formatted output is lexically different from the input.    "
            "Please file a bug.  Details:\n",
            errstream.str()));
      }
    }
  }

  return absl::OkStatus();
}

static Status ReformatVerilogIncrementally(absl::string_view original_text,
                                           absl::string_view formatted_text,
                                           absl::string_view filename,
                                           const FormatStyle& style,
                                           std::ostream& reformat_stream,
                                           const ExecutionControl& control,
                                           FormatMethod preprocess) {
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
                       formatted_lines, control, preprocess);
}

static Status ReformatVerilog(
    absl::string_view original_text, absl::string_view formatted_text,
    absl::string_view filename, const FormatStyle& style,
    std::ostream& reformat_stream, const LineNumberSet& lines,
    const ExecutionControl& control, FormatMethod preprocess) {
  // Disable reformat check to terminate recursion.
  ExecutionControl convergence_control(control);
  convergence_control.verify_convergence = false;

  if (lines.empty()) {
    // format whole file
    return FormatVerilog(formatted_text, filename, style, reformat_stream,
                         lines, convergence_control, preprocess);
  }
  // reformat incrementally
  return ReformatVerilogIncrementally(original_text, formatted_text, filename,
                                      style, reformat_stream,
                                      convergence_control, preprocess);
}

static absl::StatusOr<std::unique_ptr<VerilogAnalyzer>> ParseWithStatus(
    absl::string_view text, absl::string_view filename,
    const verilog::VerilogPreprocess::Config& preprocess_config = {}) {
  std::unique_ptr<VerilogAnalyzer> analyzer =
      VerilogAnalyzer::AnalyzeAutomaticMode(text, filename, preprocess_config);
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

absl::Status FormatVerilog(
    const absl::string_view text, absl::string_view filename,
    const FormatStyle& style, std::string* formatted_text,
    const verible::LineNumberSet& lines, const ExecutionControl& control,
    const std::vector<VerilogPreprocess::Config>& preproc_config_variants) {
  Formatter fmt(text, style, preproc_config_variants);

  // Format code.
  Status format_status = fmt.Format(control, lines);
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
  if (Status verify_status = VerifyFormatting(text, *formatted_text, filename,
                                              preproc_config_variants);
      !verify_status.ok()) {
    return verify_status;
  }

  return format_status;
}

// Returns the minimum set of filtering preprocess configs needed to generate
// token stream variants that cover the entire source.
absl::StatusOr<PreprocessConfigVariants> GetMinCoverPreprocessConfigs(
    VerilogAnalyzer* analyzer) {
  if (Status tokenize_status = analyzer->Tokenize(); !tokenize_status.ok()) {
    return tokenize_status;
  }
  FlowTree control_flow_tree(analyzer->Data().TokenStream());
  auto define_variants = control_flow_tree.MinCoverDefineVariants();
  if (!define_variants.ok()) return define_variants.status();

  std::vector<VerilogPreprocess::Config> config_variants;
  config_variants.reserve(define_variants->size());
  for (const FlowTree::DefineSet& defines : *define_variants) {
    VerilogPreprocess::Config config_variant{.filter_branches = true};
    for (auto define : defines) {
      config_variant.macro_definitions.emplace(define, std::nullopt);
    }
    config_variants.push_back(config_variant);
  }
  return config_variants;
}

Status FormatVerilog(absl::string_view text, absl::string_view filename,
                     const FormatStyle& style, std::ostream& formatted_stream,
                     const LineNumberSet& lines,
                     const ExecutionControl& control, FormatMethod preprocess) {
  // Prepare preprocess config variants
  std::vector<VerilogPreprocess::Config> preproc_config_variants;
  // TODO: Make VerilogAnalyzer movable and make this an std::optional
  std::unique_ptr<VerilogAnalyzer> preprocess_variants_analyzer;
  // Keep this analyzer alive, as the defines in preprocess_config_variants
  // refer to its text contents
  if (preprocess == FormatMethod::kPreprocessVariants) {
    preprocess_variants_analyzer =
        std::make_unique<VerilogAnalyzer>(text, filename);
    auto variants_or =
        GetMinCoverPreprocessConfigs(&*preprocess_variants_analyzer);
    if (!variants_or.ok()) return variants_or.status();
    preproc_config_variants = std::move(*variants_or);
  } else {
    preproc_config_variants.push_back(VerilogPreprocess::Config());
  }

  std::string formatted_text;
  Status format_status = FormatVerilog(text, filename, style, &formatted_text,
                                       lines, control, preproc_config_variants);

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
                            reformat_stream, lines, control, preprocess);
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

  Formatter fmt(structure.Contents(), style);

  // Format code.
  Status format_status = fmt.Format(control, {line_range});
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
    if (formatted_column_ == kInvalidColumn) {
      formatted_column_ = CalculateEolCommentColumn(previous_line);
    }
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
      case verible::SpacingDecision::kPreserve: {
        if (token.before.preserved_space_start != nullptr) {
          *column += token.OriginalLeadingSpaces().length();
        } else {
          *column += token.before.spaces;
        }
        break;
      }
      case verible::SpacingDecision::kWrap:
        *column = 0;
        ABSL_FALLTHROUGH_INTENDED;
      case verible::SpacingDecision::kAlign:
      case verible::SpacingDecision::kAppend:
        *column += token.before.spaces;
        break;
    }
  }

  static int CalculateEolCommentColumn(const verible::FormattedExcerpt& line) {
    int column = 0;
    const auto& front = line.Tokens().front();

    if (front.before.action != verible::SpacingDecision::kPreserve) {
      column += line.IndentationSpaces();
    }
    if (front.before.action == verible::SpacingDecision::kAlign) {
      column += front.before.spaces;
    }
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

// Merges the second partition tree into the first one. Whenever there is a
// discrepancy, it chooses the more granular partition.
// This function assumes that the two trees point to the same token stream.
void MergePartitionTrees(verible::TokenPartitionTree* target,
                         const verible::TokenPartitionTree& source) {
  const auto tokens_begin = [](const TokenPartitionTree& node) {
    return node.Value().TokensRange().begin();
  };
  const auto tokens_end = [](const TokenPartitionTree& node) {
    return node.Value().TokensRange().end();
  };
  auto target_it = verible::VectorTreePreOrderIterator(target);
  const auto target_end =
      ++verible::VectorTreePreOrderIterator(&RightmostDescendant(*target));
  auto source_it = verible::VectorTreePreOrderIterator(&source);
  const auto source_end = ++verible::VectorTreePreOrderIterator(
      &verible::RightmostDescendant(source));
  while (target_it != target_end && source_it != source_end) {
    if (tokens_begin(*target_it) == tokens_begin(*source_it) &&
        target_it->Children().empty()) {
      // The first tokens of source and target are aligned. This is the only
      // case that modifies target.
      if (tokens_end(*target_it) == tokens_end(*source_it)) {
        VLOG(1) << "Exact match of partitions: " << *target_it;
        // Assign if the target node has no origin, or if the source node has
        // children (which means it is more granular).
        if (!target_it->Value().Origin() || !source_it->Children().empty()) {
          VLOG(1) << "Assigning to target: " << *source_it;
          *target_it = *source_it;
        } else {
          VLOG(1) << "Skipping";
        }
        ++target_it;
        ++source_it;
      } else if (tokens_end(*target_it) < tokens_end(*source_it) &&
                 !source_it->Children().empty()) {
        // Source node has children, so it should be assigned to target.
        // However, its token range ends after the target node's. We have to cut
        // the tokens that exceed the target node.
        VLOG(1) << "Replacing: " << *target_it;
        VLOG(1) << "With: " << *source_it;
        TokenPartitionTree source_copy = *source_it;
        const auto end = ++verible::VectorTreePreOrderIterator(
            &RightmostDescendant(source_copy));
        // Remove surplus nodes from the copied partition tree.
        for (auto erase_it = verible::VectorTreePreOrderIterator(&source_copy);
             erase_it != end && tokens_end(*erase_it) <= tokens_end(*source_it);
             ++erase_it) {
          // Only erase after we move past target_it
          if (tokens_begin(*erase_it) < tokens_end(*target_it)) continue;
          TokenPartitionTree* parent = erase_it->Parent();
          size_t i = &*erase_it - &parent->Children().front();
          parent->Children().erase(parent->Children().begin() + i,
                                   parent->Children().end());
          // erase_it is invalid, move back by one
          erase_it = i > 0 ? verible::VectorTreePreOrderIterator(
                                 &parent->Children()[i - 1])
                           : verible::VectorTreePreOrderIterator(parent);
        }
        // Now we can copy the cut down tree to the target tree
        *target_it = source_copy;
        VLOG(1) << "Result: " << *target_it;
        // Move source_it after target_it
        while (source_it != source_end &&
               tokens_begin(*source_it) < tokens_end(*target_it)) {
          ++source_it;
        }
      } else if (tokens_end(*target_it) > tokens_end(*source_it)) {
        // Target node is longer than source node. To preserve information, we
        // split the target node into two. The first part is the same length as
        // the source node, and replaced by it.
        VLOG(1) << "Splitting partition: " << *target_it;
        VLOG(1) << "First part will be replaced by: " << *source_it;
        // Construct the second part
        TokenPartitionTree second_part = *target_it;
        second_part.Value().SetTokensRange(
            source_it->Value().TokensRange().end(),
            target_it->Value().TokensRange().end());
        // Replace the first part
        *target_it = *source_it;
        // Insert the second part after the first part
        TokenPartitionTree* parent = target_it->Parent();
        size_t i = &*target_it - &parent->Children().front();
        parent->Children().insert(parent->Children().begin() + i + 1,
                                  second_part);
        target_it =
            verible::VectorTreePreOrderIterator(&parent->Children()[i + 1]);
      } else {
        // tokens_end(*target_it) < tokens_end(*source_it)
        VLOG(1) << "Target partition more granular than source partition; "
                   "moving source forward";
        ++source_it;
      }
    } else if (tokens_begin(*source_it) < tokens_begin(*target_it)) {
      // Catch up source_it to target_it
      ++source_it;
    } else if (tokens_begin(*target_it) < tokens_begin(*source_it)) {
      // Catch up target_it to source_it
      ++target_it;
    } else {
      // tokens_begin(*source_it) == tokens_begin(*target_it)
      // But *target_it has children, so we cannot replace it. Iterate forward.
      ++source_it;
      ++target_it;
    }
  }
}

Status Formatter::Format(const ExecutionControl& control,
                         const verible::LineNumberSet& lines) {
  struct FormatVariant {
    FormatVariant(const FormatVariant&) = delete;
    FormatVariant(FormatVariant&&) = default;

    const verible::TextStructureView& text_structure;
    std::unique_ptr<UnwrapperData> unwrapper_data;
    TreeUnwrapper tree_unwrapper;

    FormatVariant(const verible::TextStructureView& text_struct,
                  const FormatStyle& style)
        : text_structure(text_struct),
          unwrapper_data{
              std::make_unique<UnwrapperData>(text_structure.TokenStream())},
          tree_unwrapper{text_structure, style,
                         unwrapper_data->preformatted_tokens} {}
  };

  std::vector<FormatVariant> format_variants;
  format_variants.reserve(preproc_config_variants_.size());
  size_t color = 0;
  for (const VerilogPreprocess::Config& config : preproc_config_variants_) {
    color++;

    const auto analyzer = ParseWithStatus(text_, "", config);
    if (!analyzer.ok()) return analyzer.status();
    auto text_structure = (*analyzer)->ReleaseTextStructure();

    if (!text_structures_.empty()) {
      text_structure->RebaseTokens(*text_structures_.front());
    }

    text_structure->MutableData().ColorStreamViewTokens(color);
    format_variants.push_back({text_structure->Data(), style_});

    text_structures_.push_back(std::move(text_structure));
  }
  disabled_ranges_ = EnabledLinesToDisabledByteRanges(
      lines, text_structures_.front()->Data().GetLineColumnMap());
  // Update text, as it may have been modified by the analyzer.
  text_ = text_structures_.front()->Data().Contents();

  // Partition input token stream into hierarchical set of UnwrappedLines.

  // TODO(fangism): The following block could be parallelized because
  // full-partitioning does not depend on format annotations.
  for (FormatVariant& format_variant : format_variants) {
    // Annotate inter-token information between all adjacent PreFormatTokens.
    // This must be done before any decisions about ExpandableTreeView
    // can be made because they depend on minimum-spacing, and must-break.
    AnnotateFormattingInformation(
        style_, format_variant.text_structure,
        &format_variant.unwrapper_data->preformatted_tokens);

    // Determine ranges of disabling the formatter, based on comment controls.
    disabled_ranges_.Union(DisableFormattingRanges(
        text_, format_variant.text_structure.TokenStream()));

    // Find disabled formatting ranges for specific syntax tree node types.
    // These are typically temporary workarounds for sections that users
    // habitually prefer to format themselves.
    if (const auto& root = format_variant.text_structure.SyntaxTree()) {
      DisableSyntaxBasedRanges(&disabled_ranges_, *root, style_, text_);
    }

    // Disable formatting ranges.
    verible::PreserveSpacesOnDisabledTokenRanges(
        &format_variant.unwrapper_data->preformatted_tokens, disabled_ranges_,
        text_);

    // Partition PreFormatTokens into candidate unwrapped lines.
    format_variant.tree_unwrapper.Unwrap();
  }

  // Merge partition trees from all format variants into the first one.
  for (auto it = format_variants.begin() + 1; it != format_variants.end();
       ++it) {
    // Rebase the the source partitions to the target tree tokens.
    // This is necessary for MergePartitionTrees to work.
    auto target =
        format_variants.front().tree_unwrapper.CurrentTokenPartition();
    auto source = it->tree_unwrapper.CurrentTokenPartition();
    auto old_base = source->Value().TokensRange().begin();
    auto new_base = target->Value().TokensRange().begin();
    verible::ApplyPostOrder(*source, [old_base,
                                      new_base](TokenPartitionTree& node) {
      if (node.Children().empty()) {
        auto begin =
            new_base +
            std::distance(old_base, node.Value().TokensRange().begin());
        auto end = new_base +
                   std::distance(old_base, node.Value().TokensRange().end());
        node.Value().SetTokensRange(begin, end);
      } else {
        node.Value().SetTokensRange(
            node.Children().front().Value().TokensRange().begin(),
            node.Children().back().Value().TokensRange().end());
      }
    });
    MergePartitionTrees(target, *source);
  }

  const verible::TextStructureView& text_structure =
      format_variants.front().text_structure;
  TreeUnwrapper& tree_unwrapper = format_variants.front().tree_unwrapper;
  verible::TokenPartitionTree* format_tokens_partitions =
      tree_unwrapper.UnwrappedLines();

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
                             text_structure.GetLineColumnMap(), text_);
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
          // TODO(b/145170750): Adjust inter-token spacing to achieve
          // alignment, but leave partitioning intact. This relies on
          // inter-token spacing having already been annotated.
          TabularAlignTokenPartitions(style_, text_, disabled_ranges_, &node);
          break;
        default:
          break;
      }
    });
  }

  verible::TokenPartitionTree* const root =
      tree_unwrapper.CurrentTokenPartition();
  const auto unwrapper_data = std::move(format_variants.front().unwrapper_data);

  // Apply token spacing from partitions to tokens. This is permanent, so it
  // must be done after all reshaping is done.
  {
    auto node_iter = VectorTreeLeavesIterator(&LeftmostDescendant(*root));
    const auto end = ++VectorTreeLeavesIterator(&RightmostDescendant(*root));

    // Iterate over leaves. kAlreadyFormatted partitions are either leaves
    // themselves or parents of leaf partitions with kInline policy.
    for (; node_iter != end; ++node_iter) {
      const auto partition_policy = node_iter->Value().PartitionPolicy();

      if (partition_policy == PartitionPolicyEnum::kAlreadyFormatted) {
        verible::ApplyAlreadyFormattedPartitionPropertiesToTokens(
            &(*node_iter), &unwrapper_data->preformatted_tokens);
      } else if (partition_policy == PartitionPolicyEnum::kInline) {
        auto* parent = node_iter->Parent();
        CHECK_NOTNULL(parent);
        CHECK_EQ(parent->Value().PartitionPolicy(),
                 PartitionPolicyEnum::kAlreadyFormatted);
        // This removes the node pointed to by node_iter (and all other
        // siblings)
        verible::ApplyAlreadyFormattedPartitionPropertiesToTokens(
            parent, &unwrapper_data->preformatted_tokens);
        // Move to the parent which is now a leaf
        node_iter = verible::VectorTreeLeavesIterator(parent);
      }
    }
  }

  // Produce sequence of independently operable UnwrappedLines.
  const auto unwrapped_lines = MakeUnwrappedLinesWorklist(
      style_, text_, disabled_ranges_, *format_tokens_partitions,
      &unwrapper_data->preformatted_tokens);

  // For each UnwrappedLine: minimize total penalty of wrap/break decisions.
  // TODO(fangism): This could be parallelized if results are written
  // to their own 'slots'.
  std::vector<const UnwrappedLine*> partially_formatted_lines;
  formatted_lines_.reserve(unwrapped_lines.size());
  ContinuationCommentAligner continuation_comment_aligner(
      text_structure.GetLineColumnMap(), text_);
  for (const auto& uwline : unwrapped_lines) {
    // TODO(fangism): Use different formatting strategies depending on
    // uwline.PartitionPolicy().
    if (continuation_comment_aligner.HandleLine(uwline, &formatted_lines_)) {
    } else if (uwline.PartitionPolicy() ==
               PartitionPolicyEnum::kAlreadyFormatted) {
      // For partitions that were successfully aligned, do not search
      // line-wrapping, but instead accept the adjusted padded spacing.
      formatted_lines_.emplace_back(uwline);
    } else if (IsPreprocessorControlFlow(verilog_tokentype(
                   uwline.TokensRange().begin()->TokenEnum()))) {
      // Remove indentation before preprocessing control flow.
      UnwrappedLine formatted_line = uwline;
      formatted_line.SetIndentationSpaces(0);
      formatted_lines_.emplace_back(formatted_line);
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
  std::function<bool(const verible::TokenInfo&)> include_token_p;
  if (include_disabled) {
    include_token_p = [](const verible::TokenInfo&) { return true; };
  } else {
    include_token_p = [this](const verible::TokenInfo& tok) {
      return !disabled_ranges_.Contains(tok.left(text_));
    };
  }

  int position = 0;  // tracks with the position in the original full_text
  for (const verible::FormattedExcerpt& line : formatted_lines_) {
    // TODO(fangism): The handling of preserved spaces before tokens is messy:
    // some of it is handled here, some of it is inside FormattedToken.
    // TODO(mglb): Test empty line handling when this method becomes testable.
    const auto front_offset = line.Tokens().empty()
                                  ? position
                                  : line.Tokens().front().token->left(text_);
    const absl::string_view leading_whitespace(
        text_.substr(position, front_offset - position));
    FormatWhitespaceWithDisabledByteRanges(
        text_, leading_whitespace, disabled_ranges_, include_disabled, stream);

    // When front of first token is format-disabled, the previous call will
    // already cover the space up to the front token, in which case,
    // the left-indentation for this line should be suppressed to avoid
    // being printed twice.
    if (!line.Tokens().empty()) {
      line.FormattedText(stream, !disabled_ranges_.Contains(front_offset),
                         include_token_p);
      position = line.Tokens().back().token->right(text_);
    }
  }

  // Handle trailing spaces after last token.
  const absl::string_view trailing_whitespace(text_.substr(position));
  FormatWhitespaceWithDisabledByteRanges(
      text_, trailing_whitespace, disabled_ranges_, include_disabled, stream);
}

}  // namespace formatter
}  // namespace verilog
