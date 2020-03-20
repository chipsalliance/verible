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

// verilog_obfuscate mangles verilog code by changing identifiers.
// All whitespace and identifier lengths are preserved.
// Output is written to stdout.
//
// Example usage:
// verilog_obfuscate [options] < file > output
// cat files... | verilog_obfuscate [options] > output

#include <iostream>
#include <sstream>  // IWYU pragma: keep  // for ostringstream
#include <string>   // for string, allocator, etc
#include <utility>

#include "absl/flags/flag.h"
#include "absl/strings/str_cat.h"
#include "absl/strings/string_view.h"
#include "common/strings/obfuscator.h"
#include "common/util/file_util.h"
#include "common/util/init_command_line.h"
#include "verilog/analysis/verilog_analyzer.h"
#include "verilog/analysis/verilog_equivalence.h"
#include "verilog/transform/obfuscate.h"

using verible::IdentifierObfuscator;

static void InternalError(std::ostream& stream, absl::string_view summary,
                          absl::string_view detail, absl::string_view output) {
  stream << "Internal error: " << summary << ":\n" << detail << std::endl;
  stream << "Output would have been:\n" << output;
  stream << "*** Please file a bug. ***" << std::endl;
}

int main(int argc, char** argv) {
  const auto usage = absl::StrCat("usage: ", argv[0],
                                  " [options] < original > output\n"
                                  R"(
verilog_obfuscate mangles Verilog code by changing identifiers.
All whitespaces and identifier lengths are preserved.
Output is written to stdout.
)");
  const auto args = verible::InitCommandLine(usage, &argc, &argv);

  IdentifierObfuscator subst;  // initially empty identifier map

  // TODO(fangism): load pre-existing dictionary, and apply it.

  // TODO(fangism): decode by loading dictionary as a reverse map

  // Read from stdin.
  std::string content;
  if (!verible::file::GetContents("-", &content)) {
    return 1;
  }

  std::ostringstream output;  // result buffer
  verilog::ObfuscateVerilogCode(content, &output, &subst);

  // Verify that obfuscated output is lexically equivalent to original.
  std::ostringstream errstream;
  const auto diff_status =
      verilog::ObfuscationEquivalent(content, output.str(), &errstream);
  int exit_code = 0;
  switch (diff_status) {
    case verilog::DiffStatus::kEquivalent:
      break;
    case verilog::DiffStatus::kDifferent:
      InternalError(std::cerr, "output is not equivalent", errstream.str(),
                    output.str());
      return 1;
    case verilog::DiffStatus::kLeftError:
      std::cerr << "Input contains lexical errors:\n"
                << errstream.str() << std::endl;
      return 1;
    case verilog::DiffStatus::kRightError:
      InternalError(std::cerr, "output contains lexical errors",
                    errstream.str(), output.str());
      return 1;
  }

  // TODO(fangism): save obfuscation mapping, so that it may be re-used, or even
  // reverse-applied for decoding.  Verify reversibility with a reverse map.

  // Print obfuscated code.
  std::cout << output.str();

  return exit_code;
}
