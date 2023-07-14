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

#include "absl/status/status.h"
#include "common/strings/position.h"
#include "common/text/text_structure.h"
#include "verilog/analysis/flow_tree.h"
#include "verilog/formatting/format_style.h"
#include "verilog/preprocessor/verilog_preprocess.h"

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

  // If true, show_token_partition_tree will also include inter-token
  // information, such as spacings and break penalties.
  bool show_inter_token_info = false;

  // If true, print (stderr) when there are multiple equally optimal wrapping
  // formattings on any token partition, but continue to operate.
  bool show_equally_optimal_wrappings = false;

  // Limit the size of search space for wrapping lines.
  // If this limit is exceeded, error out with a diagnostic message.
  int max_search_states = 10000;

  // If true, and not running in incremental format mode with lines specified,
  // format the formatted output one more time to compare and check for
  // convergence: format(format(text)) == format(text).
  bool verify_convergence = true;

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

// Used to select which formatting method to use by FormatVerilog.
enum class FormatMethod {
  kSinglePass,          // The default single-pass formatting
                        // method
  kPreprocessVariants,  // The '--preprocess_variants' method
};

using PreprocessConfigVariants = std::vector<VerilogPreprocess::Config>;

// Formats Verilog/SystemVerilog source code.
// 'lines' controls which lines have formattting explicitly enabled.
// If this is empty, interpret as all lines enabled for formatting.
// Does verification of the resulting format (re-parse and compare) and
// convergence test (if enabled in "control")
absl::Status FormatVerilog(absl::string_view text, absl::string_view filename,
                           const FormatStyle& style,
                           std::ostream& formatted_stream,
                           const verible::LineNumberSet& lines = {},
                           const ExecutionControl& control = {},
                           FormatMethod preprocess = FormatMethod::kSinglePass);
// Ditto, but with TextStructureView as input and std::string as output.
// This does verification of the resulting format, but _no_ convergence test.
absl::Status FormatVerilog(
    absl::string_view text, absl::string_view filename,
    const FormatStyle& style, std::string* formatted_text,
    const verible::LineNumberSet& lines = {},
    const ExecutionControl& control = {},
    const PreprocessConfigVariants& preproc_config_variants = {{}});

// Format only lines in line_range interval [min, max) in "full_content"
// using "style". Emits _only_ the formatted code in the range
// to "formatted_stream".
// Used for interactive formatting, e.g. in editors and does not do convergence
// tests.
absl::Status FormatVerilogRange(absl::string_view full_content,
                                absl::string_view filename,
                                const FormatStyle& style,
                                std::string* formatted_text,
                                const verible::Interval<int>& line_range,
                                const ExecutionControl& control = {});
// Ditto, with TextStructureView as input.
absl::Status FormatVerilogRange(const verible::TextStructureView& structure,
                                const FormatStyle& style,
                                std::string* formatted_text,
                                const verible::Interval<int>& line_range,
                                const ExecutionControl& control = {});

}  // namespace formatter
}  // namespace verilog

#endif  // VERIBLE_VERILOG_FORMATTING_FORMATTER_H_
