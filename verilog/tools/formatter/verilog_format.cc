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

// verilog_format is a command-line utility to format verilog source code
// for a given file.
//
// Example usage:
// verilog_format original-file > new-file
//
// Exit code:
//   0: stdout output can be used to replace original file
//   nonzero: stdout output (if any) should be discarded

#include <fstream>
#include <iostream>
#include <memory>
#include <sstream>  // IWYU pragma: keep  // for ostringstream
#include <string>   // for string, allocator, etc
#include <vector>

#include "absl/flags/flag.h"
#include "absl/strings/str_cat.h"
#include "absl/strings/string_view.h"
#include "common/util/file_util.h"
#include "common/util/init_command_line.h"
#include "common/util/logging.h"  // for operator<<, LOG, LogMessage, etc
#include "common/util/status.h"
#include "verilog/formatting/format_style.h"
#include "verilog/formatting/formatter.h"

using verible::util::StatusCode;
using verilog::formatter::ExecutionControl;
using verilog::formatter::FormatStyle;
using verilog::formatter::FormatVerilog;
using verilog::formatter::PreserveSpaces;

// TODO(fangism): Provide -i alias, as it is canonical to many formatters
ABSL_FLAG(bool, inplace, false,
          "If true, overwrite the input file on successful conditions.");
ABSL_FLAG(std::string, stdin_name, "<stdin>",
          "When using '-' to read from stdin, this gives an alternate name for "
          "diagnostic purposes.  Otherwise this is ignored.");

ABSL_FLAG(int, show_largest_token_partitions, 0,
          "If > 0, print token partitioning and then "
          "exit without formatting output.");
ABSL_FLAG(bool, show_token_partition_tree, false,
          "If true, print diagnostics after token partitioning and then "
          "exit without formatting output.");
ABSL_FLAG(bool, show_equally_optimal_wrappings, false,
          "If true, print when multiple optimal solutions are found (stderr), "
          "but continue to operate normally.");
ABSL_FLAG(int, max_search_states, 100000,
          "Limits the number of search states explored during "
          "line wrap optimization.");

ABSL_FLAG(
    PreserveSpaces, preserve_vspaces, PreserveSpaces::UnhandledCasesOnly,
    R"(Mode that controls how original inter-line (vertical) spacing is used.
  none: disregard all original spacing
  all: keep original vertical spacing (newlines only, no spaces/tabs)
  unhandled: same as 'all' (for now).)");

int main(int argc, char** argv) {
  const auto usage = absl::StrCat("usage: ", argv[0],
                                  " [options] <file>\n"
                                  "To pipe from stdin, use '-' as <file>.");
  const auto file_args = verible::InitCommandLine(usage, &argc, &argv);

  // Currently accepts only one file positional argument.
  // TODO(fangism): Support multiple file names.
  QCHECK_GT(file_args.size(), 1)
      << "Missing required positional argument (filename).";
  const absl::string_view filename = file_args[1];

  const bool inplace = FLAGS_inplace.Get();
  const bool is_stdin = filename == "-";
  const auto& stdin_name = FLAGS_stdin_name.Get();

  if (inplace && is_stdin) {
    std::cerr << "--inplace is incompatible with stdin.  Ignoring --inplace "
                 "and writing to stdout."
              << std::endl;
  }
  absl::string_view diagnostic_filename = filename;
  if (is_stdin) {
    diagnostic_filename = stdin_name;
  }

  // Read contents into memory first.
  std::string content;
  if (!verible::file::GetContents(filename, &content)) return 1;

  // TODO(fangism): When requesting --inplace, verify that file
  // is write-able, and fail-early if it is not.

  // Handle special debugging modes.
  ExecutionControl formatter_control;
  {
    formatter_control.stream = &std::cout;  // for diagnostics only
    formatter_control.show_largest_token_partitions =
        FLAGS_show_largest_token_partitions.Get();
    formatter_control.show_token_partition_tree =
        FLAGS_show_token_partition_tree.Get();
    formatter_control.show_equally_optimal_wrappings =
        FLAGS_show_equally_optimal_wrappings.Get();
    formatter_control.max_search_states = FLAGS_max_search_states.Get();
  }

  FormatStyle format_style;
  { format_style.preserve_vertical_spaces = FLAGS_preserve_vspaces.Get(); }

  std::ostringstream stream;
  const auto format_status = FormatVerilog(
      content, diagnostic_filename, format_style, stream, formatter_control);

  const std::string& formatted_output(stream.str());
  if (!format_status.ok()) {
    std::cerr << format_status.message();
    if (format_status.code() != StatusCode::kCancelled) {
      // Don't bother printing original code
      return 1;
    }
    // Do not write back to file, leave original untouched.
    // Print original code to stdout (in case user is redirecting output
    // to a file, possibly the original), and rejected output to stderr.
    std::cerr << "Problematic formatter output is:\n"
              << formatted_output << "<<EOF>>" << std::endl;
    std::cout << content;
    return 1;
  }

  // Safe to write out result, having passed above verification.
  int exit_code = 0;
  std::ostream* output_stream = &std::cout;
  std::ofstream inplace_file;
  if (inplace && !is_stdin) {
    inplace_file.open(std::string(filename));
    if (inplace_file.good()) {
      output_stream = &inplace_file;
    } else {
      std::cerr << "Error writing to file: " << filename << std::endl;
      std::cerr << "Printing to stdout instead." << std::endl;
      exit_code = 1;
    }
  }
  *output_stream << formatted_output;

  return exit_code;
}
