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

#include <iostream>
#include <sstream>  // IWYU pragma: keep  // for ostringstream
#include <string>   // for string, allocator, etc
#include <string_view>
#include <vector>

#include "absl/flags/flag.h"
#include "absl/flags/usage.h"
#include "absl/status/status.h"
#include "absl/status/statusor.h"
#include "absl/strings/str_cat.h"
#include "absl/strings/str_join.h"
#include "absl/strings/str_split.h"
#include "verible/common/strings/position.h"
#include "verible/common/util/file-util.h"
#include "verible/common/util/init-command-line.h"
#include "verible/common/util/interval-set.h"
#include "verible/common/util/iterator-range.h"
#include "verible/verilog/formatting/format-style-init.h"
#include "verible/verilog/formatting/format-style.h"
#include "verible/verilog/formatting/formatter.h"

using absl::StatusCode;
using verible::LineNumberSet;
using verilog::formatter::ExecutionControl;
using verilog::formatter::FormatStyle;
using verilog::formatter::FormatVerilog;

// Pseudo-singleton, so that repeated flag occurrences accumulate values.
//   --flag x --flag y yields [x, y]
struct LineRanges {
  // need to copy string, cannot just use string_view
  using storage_type = std::vector<std::string>;
  static storage_type values;
};

LineRanges::storage_type LineRanges::values;  // global initializer

static bool AbslParseFlag(std::string_view flag_arg, LineRanges * /* unused */,
                          std::string *error) {
  auto &values = LineRanges::values;
  // Pre-split strings, so that "--flag v1,v2" and "--flag v1 --flag v2" are
  // equivalent.
  const std::vector<std::string_view> tokens = absl::StrSplit(flag_arg, ',');
  values.reserve(values.size() + tokens.size());
  for (const std::string_view &token : tokens) {
    // need to copy string, cannot just use string_view
    values.emplace_back(token.begin(), token.end());
  }
  // Range validation done later.
  return true;
}

static std::string AbslUnparseFlag(LineRanges /* unused */) {
  const auto &values = LineRanges::values;
  return absl::StrJoin(values.begin(), values.end(), ",",
                       absl::StreamFormatter());
}

// TODO(fangism): Provide -i alias, as it is canonical to many formatters
ABSL_FLAG(bool, inplace, false,
          "If true, overwrite the input file on successful conditions.");
ABSL_FLAG(
    bool, verify, false,
    "If true, only checks if formatting would be done. Return code 0 means "
    "no files would change. Return code 1 means some files would "
    "be reformatted.");
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
ABSL_FLAG(bool, verify_convergence, true,
          "If true, and not incrementally formatting with --lines, "
          "verify that re-formatting the formatted output yields "
          "no further changes, i.e. formatting is convergent.");

ABSL_FLAG(bool, verbose, false, "Be more verbose.");

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

static std::ostream &FileMsg(std::string_view filename) {
  std::cerr << filename << ": ";
  return std::cerr;
}

// TODO: Refactor and simplify
static bool formatOneFile(std::string_view filename,
                          const LineNumberSet &lines_to_format,
                          bool *any_changes) {
  const bool inplace = absl::GetFlag(FLAGS_inplace);
  const bool check_changes_only = absl::GetFlag(FLAGS_verify);
  const bool is_stdin = filename == "-";
  const auto &stdin_name = absl::GetFlag(FLAGS_stdin_name);

  if (inplace && is_stdin) {
    FileMsg(filename)
        << "--inplace is incompatible with stdin.  Ignoring --inplace "
        << "and writing to stdout." << std::endl;
  }

  const auto diagnostic_filename = is_stdin ? stdin_name : filename;

  // Read contents into memory first.
  const absl::StatusOr<std::string> content_or =
      verible::file::GetContentAsString(filename);
  if (!content_or.ok()) {
    // Not using FileMsg(): file status already has filename attached.
    std::cerr << content_or.status().message() << std::endl;
    return false;
  }

  // TODO(fangism): When requesting --inplace, verify that file
  // is write-able, and fail-early if it is not.

  FormatStyle format_style;
  verilog::formatter::InitializeFromFlags(&format_style);

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
    formatter_control.verify_convergence =
        absl::GetFlag(FLAGS_verify_convergence);
  }

  std::ostringstream stream;
  const auto format_status =
      FormatVerilog(*content_or, diagnostic_filename, format_style, stream,
                    lines_to_format, formatter_control);

  const std::string &formatted_output(stream.str());
  if (!format_status.ok()) {
    if (!inplace) {
      // Fall back to printing original content regardless of error condition.
      std::cout << *content_or;
    }
    switch (format_status.code()) {
      case StatusCode::kCancelled:
      case StatusCode::kInvalidArgument:
        FileMsg(filename) << format_status.message() << std::endl;
        break;
      case StatusCode::kDataLoss:
        FileMsg(filename) << format_status.message()
                          << "; problematic formatter output is\n"
                          << formatted_output << "<<EOF>>" << std::endl;
        break;
      default:
        FileMsg(filename) << format_status.message() << "[other error status]"
                          << std::endl;
        break;
    }

    return absl::GetFlag(FLAGS_failsafe_success);
  }

  // Check if the output is the same as the input.
  const bool file_changed = (*content_or != formatted_output);
  *any_changes |= file_changed;

  // Don't output or write if --check is set.
  if (check_changes_only) {
    if (file_changed) {
      FileMsg(filename) << "Needs formatting." << std::endl;
    } else if (absl::GetFlag(FLAGS_verbose)) {
      FileMsg(filename) << "Already formatted, no change." << std::endl;
    }
  } else {
    // Safe to write out result, having passed above verification.
    if (inplace && !is_stdin) {
      // Don't write if the output is exactly as the input, so that we don't
      // mess with tools that look for timestamp changes (such as make).
      if (file_changed) {
        if (auto status =
                verible::file::SetContents(filename, formatted_output);
            !status.ok()) {
          FileMsg(filename) << "error writing result " << status << std::endl;
          return false;
        }
      } else if (absl::GetFlag(FLAGS_verbose)) {
        FileMsg(filename) << "Already formatted, no change." << std::endl;
      }
    } else {
      std::cout << formatted_output;
    }
  }

  return true;
}

int main(int argc, char **argv) {
  const auto usage = absl::StrCat("usage: ", argv[0],
                                  " [options] <file> [<file...>]\n"
                                  "To pipe from stdin, use '-' as <file>.");
  const auto file_args = verible::InitCommandLine(usage, &argc, &argv);

  if (file_args.size() == 1) {
    std::cerr << absl::ProgramUsageMessage() << std::endl;
    // TODO(hzeller): how can we append the output of --help here ?
    return 1;
  }

  // Parse LineRanges into a line set, to validate the --lines flag(s)
  LineNumberSet lines_to_format;
  if (!verible::ParseInclusiveRanges(
          &lines_to_format, LineRanges::values.begin(),
          LineRanges::values.end(), &std::cerr, '-')) {
    std::cerr << "Error parsing --lines." << std::endl;
    std::cerr << "Got: --lines=" << AbslUnparseFlag(LineRanges()) << std::endl;
    return 1;
  }

  // Some sanity checks if multiple files are given.
  if (file_args.size() > 2) {
    if (!lines_to_format.empty()) {
      std::cerr << "--lines only works for single files." << std::endl;
      return 1;
    }

    if (!absl::GetFlag(FLAGS_inplace)) {
      // Dumping all to stdout doesn't really make sense.
      std::cerr << "--inplace required for multiple files." << std::endl;
      return 1;
    }
  }

  bool all_success = true;
  bool any_changes = false;
  // All positional arguments are file names.  Exclude program name.
  for (const std::string_view filename :
       verible::make_range(file_args.begin() + 1, file_args.end())) {
    all_success &= formatOneFile(filename, lines_to_format, &any_changes);
  }

  int ret_val = 0;
  if (absl::GetFlag(FLAGS_verify)) {
    ret_val = any_changes;
  } else {
    ret_val = all_success ? 0 : 1;
  }

  return ret_val;
}
