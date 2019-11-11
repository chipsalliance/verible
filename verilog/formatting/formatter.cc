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

#include "verilog/formatting/formatter.h"

#include <algorithm>
#include <cstddef>
#include <iostream>
#include <iterator>
#include <vector>

#include "common/formatting/format_token.h"
#include "common/formatting/line_wrap_searcher.h"
#include "common/formatting/token_partition_tree.h"
#include "common/formatting/unwrapped_line.h"
#include "common/strings/range.h"
#include "common/text/line_column_map.h"
#include "common/text/text_structure.h"
#include "common/text/token_info.h"
#include "common/util/expandable_tree_view.h"
#include "common/util/iterator_range.h"
#include "common/util/logging.h"
#include "common/util/spacer.h"
#include "common/util/status.h"
#include "common/util/vector_tree.h"
#include "verilog/formatting/format_style.h"
#include "verilog/formatting/token_annotator.h"
#include "verilog/formatting/tree_unwrapper.h"

namespace verilog {
namespace formatter {

using verible::ExpandableTreeView;
using verible::PartitionPolicyEnum;
using verible::TokenPartitionTree;
using verible::TreeViewNodeInfo;
using verible::UnwrappedLine;
using verible::VectorTree;

typedef VectorTree<TreeViewNodeInfo<UnwrappedLine>> partition_node_type;

// Decided at each node in UnwrappedLine partition tree whether or not
// it should be expanded or unexpanded.
static void DeterminePartitionExpansion(partition_node_type* node,
                                        const FormatStyle& style) {
  auto& node_view = node->Value();
  const auto& children = node->Children();

  // If this is a leaf partition, there is nothing to expand.
  if (children.empty()) {
    VLOG(3) << "No children to expand.";
    node_view.Unexpand();
    return;
  }

  // If any children are expanded, then this node must be expanded,
  // regardless of the UnwrappedLine's chosen policy.
  // Thus, this function must be executed with a post-order traversal.
  const auto iter = std::find_if(children.begin(), children.end(),
                                 [](const partition_node_type& child) {
                                   return child.Value().IsExpanded();
                                 });
  if (iter != children.end()) {
    VLOG(3) << "Child forces parent to expand.";
    node_view.Expand();
    return;
  }

  // Expand or not, depending on partition policy and other conditions.
  const auto& uwline = node_view.Value();
  const auto partition_policy = uwline.PartitionPolicy();
  VLOG(3) << "partition policy: " << partition_policy;
  switch (partition_policy) {
    case PartitionPolicyEnum::kAlwaysExpand: {
      node_view.Expand();
      break;
    }
    case PartitionPolicyEnum::kFitOnLineElseExpand: {
      if (verible::FitsOnLine(uwline, style)) {
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
    const FormatStyle& style) {
  // Initialize a tree view that treats partitions as fully-expanded.
  ExpandableTreeView<UnwrappedLine> format_tokens_partition_view(
      format_tokens_partitions);

  // For unwrapped lines that fit, don't bother expanding their partitions.
  // Post-order traversal: if a child doesn't 'fit' and needs to be expanded,
  // so must all of its parents (and transitively, ancestors).
  format_tokens_partition_view.ApplyPostOrder(
      [&style](partition_node_type& node) {
        DeterminePartitionExpansion(&node, style);
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

std::ostream& Formatter::ExecutionControl::Stream() const {
  return (stream != nullptr) ? *stream : std::cout;
}

verible::util::Status Formatter::Format(const ExecutionControl& control) {
  // Initialize auxiliary data needed for TreeUnwrapper.
  UnwrapperData unwrapper_data(text_structure_.TokenStream());

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
                                  unwrapper_data.preformatted_tokens.begin(),
                                  unwrapper_data.preformatted_tokens.end());

    // Partition PreFormatTokens into candidate unwrapped lines.
    format_tokens_partitions = tree_unwrapper.Unwrap();
  }

  {
    // For debugging only: identify largest leaf partitions, and stop.
    if (control.show_token_partition_tree) {
      control.Stream() << "Full token partition tree:\n"
                       << verible::TokenPartitionTreePrinter(
                              *format_tokens_partitions)
                       << std::endl;
    }
    if (control.show_largest_token_partitions != 0) {
      PrintLargestPartitions(control.Stream(), *format_tokens_partitions,
                             control.show_largest_token_partitions,
                             text_structure_.GetLineColumnMap(),
                             text_structure_.Contents());
    }
    if (control.AnyStop()) {
      return verible::util::OkStatus();
    }
  }

  // Produce sequence of independently operable UnwrappedLines.
  const auto unwrapped_lines =
      MakeUnwrappedLinesWorklist(*format_tokens_partitions, style_);

  // For each UnwrappedLine: minimize total penalty of wrap/break decisions.
  // TODO(fangism): This could be parallelized if results are written
  // to their own 'slots'.
  std::vector<const UnwrappedLine*> partially_formatted_lines;
  formatted_lines_.reserve(unwrapped_lines.size());
  for (const auto& uwline : unwrapped_lines) {
    // TODO(fangism): Use different formatting strategies depending on
    // uwline.PartitionPolicy().
    formatted_lines_.push_back(
        verible::SearchLineWraps(uwline, style_, control.max_search_states));
    if (!formatted_lines_.back().CompletedFormatting()) {
      // Copy over any lines that did not finish wrap searching.
      partially_formatted_lines.push_back(&uwline);
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
    return verible::util::ResourceExhaustedError(err_stream.str());
  }

  return verible::util::OkStatus();
}

// Returns text between last token and EOF.
absl::string_view Formatter::TrailingWhiteSpaces() const {
  const absl::string_view full_text(text_structure_.Contents());
  if (formatted_lines_.empty()) {
    // Text contains only whitespace tokens.
    return full_text;
  } else {
    // Preserve vertical spaces between last token and EOF.
    const auto& last_line = formatted_lines_.back().Tokens();
    const auto* end_of_buffer = full_text.end();
    if (last_line.empty()) {
      return absl::string_view(end_of_buffer, 0);
    } else {
      const auto* last_printed_offset = last_line.back().token->text.end();
      return verible::make_string_view_range(last_printed_offset,
                                             end_of_buffer);
    }
  }
}

void Formatter::Emit(std::ostream& stream) const {
  if (style_.preserve_horizontal_spaces == PreserveSpaces::All) {
    // Need to track pre-existing spaces between token partitions.
    for (const auto& line : formatted_lines_) {
      line.FormatLinePreserveLeadingSpace(stream);
    }
    // Handle trailing spaces after last token.
    stream << TrailingWhiteSpaces();
  } else {  // (horizontal) PreserveSpaces::None or UnhandledCasesOnly
    switch (style_.preserve_vertical_spaces) {
      case PreserveSpaces::None:
        for (const auto& line : formatted_lines_) {
          stream << line << '\n';
        }
        break;
      case PreserveSpaces::All:
      case PreserveSpaces::UnhandledCasesOnly:
        bool is_first_line = true;
        for (const auto& line : formatted_lines_) {
          line.FormatLinePreserveLeadingNewlines(stream, is_first_line);
          is_first_line = false;
        }
        // Handle trailing spaces after last token.
        const size_t newline_count =
            verible::FormattedExcerpt::PreservedNewlinesCount(
                TrailingWhiteSpaces(), is_first_line);
        stream << verible::Spacer(newline_count, '\n');
        break;
    }
    // TODO(fangism): This currently doesn't adequately handle anything betweeen
    // PreserveSpace::None and ::All, needs a clean policy for
    // PreserveSpace::UnhandledCasesOnly.
  }
}

}  // namespace formatter
}  // namespace verilog
