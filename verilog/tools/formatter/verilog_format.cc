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
#include "common/text/text_structure.h"
#include "common/util/file_util.h"
#include "common/util/init_command_line.h"
#include "common/util/logging.h"  // for operator<<, LOG, LogMessage, etc
#include "common/util/status.h"
#include "verilog/analysis/verilog_analyzer.h"
#include "verilog/formatting/format_style.h"
#include "verilog/formatting/formatter.h"

using verible::util::StatusCode;
using verilog::VerilogAnalyzer;
using verilog::formatter::FormatStyle;
using verilog::formatter::Formatter;
using verilog::formatter::PreserveSpaces;

// TODO(fangism): Provide -i alias, as it is canonical to many formatters
ABSL_FLAG(bool, inplace, false,
          "If true, overwrite the input file on successful conditions.");

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
    PreserveSpaces, preserve_hspaces, PreserveSpaces::UnhandledCasesOnly,
    R"(Mode that controls how original inter-token (horizontal) spacing is used.
  none: disregard all original spacing
  all: only use original spacing (does no formatting)
  unhandled: fall-back to original spacing in unhandled cases.)");
ABSL_FLAG(
    PreserveSpaces, preserve_vspaces, PreserveSpaces::UnhandledCasesOnly,
    R"(Mode that controls how original inter-line (vertical) spacing is used.
This only takes any effect when preserve_hspaces != all.
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

  if (inplace && is_stdin) {
    std::cerr << "--inplace is incompatible with stdin.  Ignoring --inplace "
                 "and writing to stdout."
              << std::endl;
  }

  // Read contents into memory first.
  std::string content;
  if (!verible::file::GetContents(filename, &content)) return 1;

  // TODO(fangism): When requesting --inplace, verify that file
  // is write-able, and fail-early if it is not.

  // Handle special debugging modes.
  Formatter::ExecutionControl formatter_control;
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

  const auto analyzer =
      VerilogAnalyzer::AnalyzeAutomaticMode(content, filename);
  {
    // Lex and parse code.  Exit on failure.
    const auto lex_status = ABSL_DIE_IF_NULL(analyzer)->LexStatus();
    const auto parse_status = analyzer->ParseStatus();
    if (!lex_status.ok() || !parse_status.ok()) {
      const std::vector<std::string> syntax_error_messages(
          analyzer->LinterTokenErrorMessages());
      for (const auto& message : syntax_error_messages) {
        std::cerr << message << std::endl;
      }
      // Don't bother printing original code
      return 1;
    }
  }

  int exit_code = 0;
  const verible::TextStructureView& text_structure = analyzer->Data();
  std::ostringstream stream;
  {
    // TODO(fangism) support style customization
    FormatStyle format_style;
    {
      format_style.preserve_horizontal_spaces = FLAGS_preserve_hspaces.Get();
      format_style.preserve_vertical_spaces = FLAGS_preserve_vspaces.Get();
    }
    Formatter formatter(text_structure, format_style);

    // Format code.
    verible::util::Status format_status = formatter.Format(formatter_control);
    if (!format_status.ok()) {
      if (format_status.code() == StatusCode::kResourceExhausted) {
        // Allow remainder of this function to execute, and print partially
        // formatted code, but force a non-zero exit status.
        std::cerr << format_status.message();
        exit_code = 1;
      }
    }

    // In any diagnostic mode, proceed no further.
    if (formatter_control.AnyStop()) {
      std::cout << "Halting for diagnostic operation." << std::endl;
      return 0;
    }

    // Render formatted result.
    formatter.Emit(stream);
  }
  const std::string& formatted_output(stream.str());

  {
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

    {
      const auto& original_tokens = text_structure.TokenStream();
      const auto& formatted_tokens = reanalyzer->Data().TokenStream();
      // Filter out only whitespaces and compare.
      // First difference will be printed to cerr for debugging.
      if (!verilog::LexicallyEquivalent(original_tokens, formatted_tokens,
                                        &std::cerr)) {
        std::cerr << "Formatted output is lexically different from the input.  "
                  << "Please file a bug." << std::endl;
        exit_code = 1;
      }
    }

    if (!relex_status.ok() || !reparse_status.ok()) {
      std::cerr << "Error lex/parsing-ing formatted output.  "
                << "Please file a bug." << std::endl;
      const auto& token_errors = reanalyzer->TokenErrorMessages();
      // Only print the first error.
      if (!token_errors.empty()) {
        std::cerr << "First error: " << token_errors.front() << std::endl;
      }
      exit_code = 1;
    }

    // TODO(b/138868051): Verify output stability/convergence.
  }

  if (exit_code != 0) {
    // Do not write back to file, leave original untouched.
    // Print original code to stdout (in case user is redirecting output
    // to a file, possibly the original), and rejected output to stderr.
    std::cerr << "Problematic formatter output is:\n"
              << formatted_output << "<<EOF>>" << std::endl;
    std::cout << content;
  } else {
    // Safe to write out result, having passed above verification.
    std::ostream* output_stream = &std::cout;
    std::ofstream inplace_file;
    if (inplace && !is_stdin) {
      inplace_file.open(filename.data());
      if (inplace_file.good()) {
        output_stream = &inplace_file;
      }
    }
    *output_stream << formatted_output;
  }

  return exit_code;
}
