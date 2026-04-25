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

// verilog_diff compares the lexical contents of two Verilog source code
// texts.  Inputs only need to be lexically valid, not necessarily syntactically
// valid.  Use '-' to read from stdin.
// Differences are reported to stdout.
// The program exits 0 if no differences are found, else non-zero.
//
// Example usage:
// verilog_diff [options] file1 file2

#include <functional>
#include <iostream>
#include <map>
#include <sstream>  // IWYU pragma: keep  // for ostringstream
#include <string>   // for string, allocator, etc
#include <string_view>

#include "absl/flags/flag.h"
#include "absl/strings/str_cat.h"
#include "verible/common/util/enum-flags.h"
#include "verible/common/util/file-util.h"
#include "verible/common/util/init-command-line.h"
#include "verible/common/util/logging.h"
#include "verible/verilog/analysis/verilog-equivalence.h"

// Enumeration type for selecting
enum class DiffMode {
  // TODO(fangism): kNone: none of the existing presets, let the user compose
  // the filter predicate and comparator independently.
  kFormat,
  kObfuscate,
};

static const verible::EnumNameMap<DiffMode> &DiffModeStringMap() {
  static const verible::EnumNameMap<DiffMode> kDiffModeStringMap({
      {"format", DiffMode::kFormat},
      {"obfuscate", DiffMode::kObfuscate},
  });
  return kDiffModeStringMap;
}

static std::ostream &operator<<(std::ostream &stream, DiffMode p) {
  return DiffModeStringMap().Unparse(p, stream);
}

static bool AbslParseFlag(std::string_view text, DiffMode *mode,
                          std::string *error) {
  return DiffModeStringMap().Parse(text, mode, error, "--mode value");
}

static std::string AbslUnparseFlag(const DiffMode &mode) {
  std::ostringstream stream;
  stream << mode;
  return stream.str();
}

ABSL_FLAG(DiffMode, mode, DiffMode::kFormat,
          R"(Defines difference functions.
  format: ignore whitespaces, compare token texts.
    This is useful for verifying formatter (e.g. verilog_format) output.
  obfuscate: preserve whitespaces, compare token texts' lengths only.
    This is useful for verifying verilog_obfuscate output.
)");

using EquivalenceFunctionType = std::function<verilog::DiffStatus(
    std::string_view, std::string_view, std::ostream *)>;

static const std::map<DiffMode, EquivalenceFunctionType> diff_func_map({
    {DiffMode::kFormat, verilog::FormatEquivalent},
    {DiffMode::kObfuscate, verilog::ObfuscationEquivalent},
});

int main(int argc, char **argv) {
  const auto usage = absl::StrCat("usage: ", argv[0],
                                  " [options] file1 file2\n"
                                  "Use - as a file name to read from stdin.");
  const auto args = verible::InitCommandLine(usage, &argc, &argv);

  enum {
    // inputs differ or there is some lexical error in one of the inputs
    kInputDifferenceErrorCode = 1,

    // error with flags or opening/reading one of the files
    kUserErrorCode = 2,
  };

  if (args.size() != 3) {
    std::cerr << "Program requires 2 positional arguments for input files."
              << std::endl;
    return kUserErrorCode;
  }

  // Open both files.
  const auto content1_or = verible::file::GetContentAsString(args[1]);
  if (!content1_or.ok()) {
    std::cerr << args[1] << ": " << content1_or.status() << std::endl;
    return kUserErrorCode;
  }
  const auto content2_or = verible::file::GetContentAsString(args[2]);
  if (!content2_or.ok()) {
    std::cerr << args[1] << ": " << content1_or.status() << std::endl;
    return kUserErrorCode;
  }

  // Selection diff-ing function.
  const auto diff_mode = absl::GetFlag(FLAGS_mode);
  const auto iter = diff_func_map.find(diff_mode);
  CHECK(iter != diff_func_map.end());
  const auto diff_func = iter->second;

  // Compare.
  std::ostringstream errstream;
  const auto diff_status = diff_func(*content1_or, *content2_or, &errstream);

  // Signal result of comparison.
  switch (diff_status) {
    case verilog::DiffStatus::kEquivalent: {
      std::cout << "Inputs match." << std::endl;
      break;
    }
    case verilog::DiffStatus::kDifferent: {
      std::cout << "Inputs differ.\n" << errstream.str() << std::endl;
      return kInputDifferenceErrorCode;
    }
    case verilog::DiffStatus::kLeftError: {
      std::cout << "Lexical error in first file.\n"
                << errstream.str() << std::endl;
      return kInputDifferenceErrorCode;
    }
    case verilog::DiffStatus::kRightError: {
      std::cout << "Lexical error in second file.\n"
                << errstream.str() << std::endl;
      return kInputDifferenceErrorCode;
    }
  }
  return 0;
}
