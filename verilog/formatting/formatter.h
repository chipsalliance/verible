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

#ifndef VERIBLE_VERILOG_FORMATTING_FORMATTER_H_
#define VERIBLE_VERILOG_FORMATTING_FORMATTER_H_

#include <iosfwd>
#include <vector>

#include "common/formatting/unwrapped_line.h"
#include "common/text/text_structure.h"
#include "common/util/status.h"
#include "verilog/formatting/comment_controls.h"
#include "verilog/formatting/format_style.h"

namespace verilog {
namespace formatter {

// Takes a TextStructureView and FormatStyle, and formats UnwrappedLines.
class Formatter {
 public:
  // Control over formatter's internal execution phases, mostly for debugging
  // and development.
  struct ExecutionControl {
    // When non-zero, diagnose the largest token partitions, and halt without
    // formatting.
    int show_largest_token_partitions = 0;

    // If true, print the token partition tree, and halt without formatting.
    bool show_token_partition_tree = false;

    // If true, print (stderr) when there are multiple equally optimal wrapping
    // formattings on any token partition, but continue to operate.
    bool show_equally_optimal_wrappings = false;

    // Limit the size of search space for wrapping lines.
    // If this limit is exceeded, error out with a diagnostic message.
    int max_search_states = 10000;

    // Output stream for diagnostic feedback (not formatting output).
    std::ostream* stream = nullptr;

    // Returns *stream or a default stream like std::cout.
    std::ostream& Stream() const;

    bool AnyStop() const {
      return show_largest_token_partitions != 0 || show_token_partition_tree;
    }
  };

  Formatter(const verible::TextStructureView& text_structure,
            const FormatStyle& style)
      : text_structure_(text_structure), style_(style) {}

  // Formats the source code
  verible::util::Status Format(const ExecutionControl&);

  verible::util::Status Format() { return Format(ExecutionControl()); }

  // Outputs all of the FormattedExcerpt lines to stream.
  void Emit(std::ostream& stream) const;

 private:
  absl::string_view TrailingWhiteSpaces() const;

  // protected for testing
 protected:
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

}  // namespace formatter
}  // namespace verilog

#endif  // VERIBLE_VERILOG_FORMATTING_FORMATTER_H_
