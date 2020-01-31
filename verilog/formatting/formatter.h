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

#include "common/util/status.h"
#include "verilog/formatting/comment_controls.h"
#include "verilog/formatting/format_style.h"

namespace verilog {
namespace formatter {

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
  // This is useful for seeing diagnostics without waiting for a Status
  // to be returned.
  std::ostream* stream = nullptr;

  // Returns *stream or a default stream like std::cout.
  std::ostream& Stream() const;

  bool AnyStop() const {
    return show_largest_token_partitions != 0 || show_token_partition_tree;
  }
};

// Formats Verilog/SystemVerilog source code.
// 'lines' controls which lines have formattting explicitly enabled.
// If this is empty, interpret as all lines enabled for formatting.
verible::util::Status FormatVerilog(absl::string_view text,
                                    absl::string_view filename,
                                    const FormatStyle& style,
                                    std::ostream& formatted_stream,
                                    const LineNumberSet& lines = {},
                                    const ExecutionControl& control = {});

}  // namespace formatter
}  // namespace verilog

#endif  // VERIBLE_VERILOG_FORMATTING_FORMATTER_H_
