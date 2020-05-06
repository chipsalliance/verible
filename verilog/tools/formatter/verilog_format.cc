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
#include "absl/flags/usage.h"
#include "absl/status/status.h"
#include "absl/strings/str_cat.h"
#include "absl/strings/str_join.h"
#include "absl/strings/str_split.h"
#include "absl/strings/string_view.h"
#include "common/util/file_util.h"
#include "common/util/init_command_line.h"
#include "common/util/interval_set.h"
#include "common/util/logging.h"  // for operator<<, LOG, LogMessage, etc
#include "verilog/formatting/format_style.h"
#include "verilog/formatting/formatter.h"

using absl::StatusCode;
using verilog::formatter::ExecutionControl;
using verilog::formatter::FormatStyle;
using verilog::formatter::FormatVerilog;

// Pseudo-singleton, so that repeated flag occurrences accumulate values.
//   --flag x --flag y yields [x, y]
struct LineRanges {
  // need to copy string, cannot just use string_view
  typedef std::vector<std::string> storage_type;
  static storage_type values;
};

LineRanges::storage_type LineRanges::values;  // global initializer

bool AbslParseFlag(absl::string_view flag_arg, LineRanges* /* unused */,
                   std::string* error) {
  auto& values = LineRanges::values;
  // Pre-split strings, so that "--flag v1,v2" and "--flag v1 --flag v2" are
  // equivalent.
  const std::vector<absl::string_view> tokens = absl::StrSplit(flag_arg, ',');
  values.reserve(values.size() + tokens.size());
  for (const auto& token : tokens) {
    // need to copy string, cannot just use string_view
    values.push_back(std::string(token.begin(), token.end()));
  }
  // Range validation done later.
  return true;
}

std::string AbslUnparseFlag(LineRanges /* unused */) {
  const auto& values = LineRanges::values;
  return absl::StrJoin(values.begin(), values.end(), ",",
                       absl::StreamFormatter());
}

// TODO(fangism): Provide -i alias, as it is canonical to many formatters
ABSL_FLAG(bool, inplace, false,
          "If true, overwrite the input file on successful conditions.");
ABSL_FLAG(std::string, stdin_name, "<stdin>",
          "When using '-' to read from stdin, this gives an alternate name for "
          "diagnostic purposes.  Otherwise this is ignored.");
ABSL_FLAG(LineRanges, lines, {},
          "Specific lines to format, 1-based, comma-separated, inclusive N-M "
          "ranges, N is short for N-N.  By default, left unspecified, "
          "all lines are enabled for formatting.  (repeatable, cumulative)");
ABSL_FLAG(bool, failsafe_success, true,
          "If true, always exit with 0 status, even if there were input errors "
          "or internal errors.  In all error conditions, the original text is "
          "always preserved.  This is useful in deploying services where "
          "fail-safe behaviors should be considered a success.");

ABSL_FLAG(int, show_largest_token_partitions, 0,
          "If > 0, print token partitioning and then "
          "exit without formatting output.");
ABSL_FLAG(bool, show_token_partition_tree, false,
          "If true, print diagnostics after token partitioning and then "
          "exit without formatting output.");
ABSL_FLAG(bool, show_inter_token_info, false,
          "If true, along with show_token_partition_tree, include inter-token "
          "information such as spacing and break penalties.");
ABSL_FLAG(bool, show_equally_optimal_wrappings, false,
          "If true, print when multiple optimal solutions are found (stderr), "
          "but continue to operate normally.");
ABSL_FLAG(int, max_search_states, 100000,
          "Limits the number of search states explored during "
          "line wrap optimization.");

// These flags exist in the short term to disable formatting of some regions.
ABSL_FLAG(bool, format_module_port_declarations, false,
          // TODO(b/70310743): format module port declarations in aligned manner
          "If true, format module declarations' list of port declarations, "
          "else leave them unformatted.  This is a short-term workaround.");
ABSL_FLAG(bool, format_module_instantiations, false,
          // TODO(b/152805837): format module instances' ports in aligned manner
          "If true, format module instantiations (data declarations), "
          "else leave them unformatted.  This is a short-term workaround.");

bool formatOneFile(absl::string_view filename,
                   const verilog::formatter::LineNumberSet &lines_to_format) {
  const bool inplace = absl::GetFlag(FLAGS_inplace);
  const bool is_stdin = filename == "-";
  const auto& stdin_name = absl::GetFlag(FLAGS_stdin_name);

  if (inplace && is_stdin) {
    std::cerr << "--inplace is incompatible with stdin.  Ignoring --inplace "
        "and writing to stdout." << std::endl;
  }

  const auto diagnostic_filename = is_stdin ? stdin_name : filename;

  // Read contents into memory first.
  std::string content;
  if (!verible::file::GetContents(filename, &content)) return false;

  // TODO(fangism): When requesting --inplace, verify that file
  // is write-able, and fail-early if it is not.

  // TODO(fangism): support style configuration from flags.
  FormatStyle format_style;

  // Handle special debugging modes.
  ExecutionControl formatter_control;
  {
    // execution control flags
    formatter_control.stream = &std::cout;  // for diagnostics only
    formatter_control.show_largest_token_partitions =
        absl::GetFlag(FLAGS_show_largest_token_partitions);
    formatter_control.show_token_partition_tree =
        absl::GetFlag(FLAGS_show_token_partition_tree);
    formatter_control.show_inter_token_info =
        absl::GetFlag(FLAGS_show_inter_token_info);
    formatter_control.show_equally_optimal_wrappings =
        absl::GetFlag(FLAGS_show_equally_optimal_wrappings);
    formatter_control.max_search_states =
        absl::GetFlag(FLAGS_max_search_states);

    // formatting style flags
    format_style.format_module_port_declarations =
        absl::GetFlag(FLAGS_format_module_port_declarations);
    format_style.format_module_instantiations =
        absl::GetFlag(FLAGS_format_module_instantiations);
  }

  std::ostringstream stream;
  const auto format_status =
      FormatVerilog(content, diagnostic_filename, format_style, stream,
                    lines_to_format, formatter_control);

  const std::string& formatted_output(stream.str());
  if (!format_status.ok()) {
    if (!inplace) {
      // Fall back to printing original content regardless of error condition.
      std::cout << content;
    }
    // Print the error message last so it shows up in user's console.
    std::cerr << format_status.message() << std::endl;
    switch (format_status.code()) {
      case StatusCode::kCancelled:
      case StatusCode::kInvalidArgument:
        break;
      case StatusCode::kDataLoss:
        std::cerr << "Problematic formatter output is:\n"
                  << formatted_output << "<<EOF>>" << std::endl;
        break;
      default:
        std::cerr << "[other error status]" << std::endl;
        break;
    }
    if (absl::GetFlag(FLAGS_failsafe_success)) {
      // original text was preserved, and --inplace modification is skipped.
      return true;
    }
    return false;
  }

  // Safe to write out result, having passed above verification.
  if (inplace && !is_stdin) {
    // Don't write if the output is exactly as the input, so that we don't mess
    // with tools that look for timestamp changes (such as make).
    if (content != formatted_output) {
      if (!verible::file::SetContents(filename, formatted_output)) {
        std::cerr << "Error writing to file: " << filename << std::endl;
        std::cerr << "Printing to stdout instead." << std::endl;
        std::cout << formatted_output;
        return false;
      }
    }
  } else {
    std::cout << formatted_output;
  }

  return true;
}

int main(int argc, char** argv) {
  const auto usage = absl::StrCat("usage: ", argv[0],
                                  " [options] <file>\n"
                                  "To pipe from stdin, use '-' as <file>.");
  const auto file_args = verible::InitCommandLine(usage, &argc, &argv);

  if (file_args.size() == 1) {
    std::cerr << absl::ProgramUsageMessage() << std::endl;
    // TODO(hzeller): how can we append the output of --help here ?
  }

  // Parse LineRanges into a line set, to validate the --lines flag(s)
  verilog::formatter::LineNumberSet lines_to_format;
  if (!verible::ParseInclusiveRanges(
          &lines_to_format, LineRanges::values.begin(),
          LineRanges::values.end(), &std::cerr, '-')) {
    std::cerr << "Error parsing --lines." << std::endl;
    std::cerr << "Got: --lines=" << AbslUnparseFlag(LineRanges()) << std::endl;
    return 1;
  }

  if (!lines_to_format.empty() && file_args.size() > 2) {
    std::cerr << "Note, applying --lines rules to all files" << std::endl;
  }

  bool all_success = true;
  // All positional arguments are file names.  Exclude program name.
  for (const absl::string_view filename :
           verible::make_range(file_args.begin() + 1, file_args.end())) {
    all_success &= formatOneFile(filename, lines_to_format);
  }

  return all_success ? 0 : 1;
}
