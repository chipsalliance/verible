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
#include <set>
#include <sstream>  // IWYU pragma: keep  // for ostringstream
#include <string>   // for string, allocator, etc
#include <utility>

#include "absl/flags/flag.h"
#include "absl/strings/str_cat.h"
#include "absl/strings/string_view.h"
#include "common/strings/obfuscator.h"
#include "common/util/file_util.h"
#include "common/util/init_command_line.h"
#include "verilog/analysis/extractors.h"
#include "verilog/analysis/verilog_analyzer.h"
#include "verilog/transform/obfuscate.h"

using verible::IdentifierObfuscator;

ABSL_FLAG(                      //
    std::string, load_map, "",  //
    "If provided, pre-load an existing translation dictionary (written by "
    "--save_map).  This is useful for applying pre-existing transforms.");
ABSL_FLAG(                      //
    std::string, save_map, "",  //
    "If provided, save the translation to a dictionary for reuse in a "
    "future obfuscation with --load_map.");
ABSL_FLAG(                //
    bool, decode, false,  //
    "If true, when used with --load_map, apply the translation dictionary in "
    "reverse to de-obfuscate the source code, and do not obfuscate any unseen "
    "identifiers.  There is no need to --save_map with this option, because "
    "no new substitutions are established.");
ABSL_FLAG(                            //
    bool, preserve_interface, false,  //
    "If true, module name, port names and parameter names will be preserved.  "
    "The translation map saved with --save_map will have identity mappings for "
    "these identifiers.  When used with --load_map, the mapping explicitly "
    "specified in the map file will have higher priority than this option.");

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

  // Set mode to encode or decode.
  const bool decode = absl::GetFlag(FLAGS_decode);
  subst.set_decode_mode(decode);

  const auto& load_map_file = absl::GetFlag(FLAGS_load_map);
  const auto& save_map_file = absl::GetFlag(FLAGS_save_map);
  if (!load_map_file.empty()) {
    std::string load_map_content;
    absl::Status status =
        verible::file::GetContents(load_map_file, &load_map_content);
    if (!status.ok()) {
      std::cerr << "Error reading --load_map file " << load_map_file << ": "
                << status << std::endl;
      return 1;
    }
    status = subst.load(load_map_content);
    if (!status.ok()) {
      std::cerr << "Error parsing --load_map file: " << load_map_file << '\n'
                << status.message() << std::endl;
      return 1;
    }
  } else if (decode) {
    std::cerr << "--load_map is required with --decode." << std::endl;
    return 1;
  }

  // Read from stdin.
  std::string content;
  if (!verible::file::GetContents("-", &content).ok()) {
    return 1;
  }

  // Preserve interface names (e.g. module name, port names).
  // TODO: an inner module's interface in a nested module will be preserved.
  // but this may not be required.
  const bool preserve_interface = absl::GetFlag(FLAGS_preserve_interface);
  if (preserve_interface) {
    std::set<std::string> preserved;
    const auto status =
        verilog::analysis::CollectInterfaceNames(content, &preserved);
    if (!status.ok()) {
      std::cerr << status.message();
      return 1;
    }
    for (auto const& preserved_name : preserved)
      subst.encode(preserved_name, preserved_name);
  }

  // Encode/obfuscate.  Also verifies decode-ability.
  std::ostringstream output;  // result buffer
  const auto status = verilog::ObfuscateVerilogCode(content, &output, &subst);
  if (!status.ok()) {
    std::cerr << status.message();
    return 1;
  }

  if (!decode && !save_map_file.empty()) {
    if (!verible::file::SetContents(save_map_file, subst.save()).ok()) {
      std::cerr << "Error writing --save_map file: " << save_map_file
                << std::endl;
      return 1;
    }
  }

  // Print obfuscated code.
  std::cout << output.str();
  return 0;
}
