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

#include <deque>
#include <fstream>
#include <iostream>
#include <string>

#include "absl/flags/flag.h"
#include "absl/status/status.h"
#include "absl/strings/str_cat.h"
#include "absl/strings/string_view.h"
#include "absl/strings/substitute.h"
#include "common/util/bijective_map.h"
#include "common/util/enum_flags.h"
#include "common/util/file_util.h"
#include "common/util/init_command_line.h"
#include "common/util/tree_operations.h"
#include "verilog/analysis/verilog_analyzer.h"
#include "verilog/analysis/verilog_project.h"
#include "verilog/tools/kythe/indexing_facts_tree_extractor.h"
#include "verilog/tools/kythe/kythe_facts_extractor.h"
#include "verilog/tools/kythe/kythe_proto_output.h"

#ifndef _WIN32
#include <unistd.h>  // for STDOUT_FILENO
#else
#include <stdio.h>
#define STDOUT_FILENO _fileno(stdout)
#endif

// for --print_kythe_facts flag
enum class PrintMode {
  kJSON,
  kJSONDebug,
  kProto,
  kNone,
};

static const verible::EnumNameMap<PrintMode>& PrintModeStringMap() {
  static const verible::EnumNameMap<PrintMode> kPrintModeStringMap({
      {"json", PrintMode::kJSON},
      {"json_debug", PrintMode::kJSONDebug},
      {"proto", PrintMode::kProto},
      {"none", PrintMode::kNone},
  });
  return kPrintModeStringMap;
}

static std::ostream& operator<<(std::ostream& stream, PrintMode mode) {
  return PrintModeStringMap().Unparse(mode, stream);
}

static bool AbslParseFlag(absl::string_view text, PrintMode* mode,
                          std::string* error) {
  return PrintModeStringMap().Parse(text, mode, error,
                                    "--print_kythe_facts value");
}

static std::string AbslUnparseFlag(const PrintMode& mode) {
  std::ostringstream stream;
  stream << mode;
  return stream.str();
}

ABSL_FLAG(bool, printextraction, false,
          "Whether or not to print the extracted general indexing facts tree "
          "from the middle layer)");

ABSL_FLAG(PrintMode, print_kythe_facts, PrintMode::kJSON,
          "Determines how to print Kythe indexing facts.  Options:\n"
          "  json: Outputs Kythe facts in JSON format (one per line)\n"
          "  json_debug: Outputs Kythe facts in JSON format (without encoding, "
          "all in one JSON object)\n"
          "  proto: Outputs Kythe facts in proto format\n"
          "  none: Just collect facts, don't output them (for debugging)\n"
          "Default: json\n");

ABSL_FLAG(
    std::string, file_list_path, "",
    R"(The path to the file list which contains the names of SystemVerilog files.
    The files should be ordered by definition dependencies.)");

ABSL_FLAG(
    std::string, file_list_root, ".",
    R"(The absolute location which we prepend to the files in the file list (where listed files are relative to).)");

// TODO: support repeatable flag
ABSL_FLAG(
    std::vector<std::string>, include_dir_paths, {},
    R"(Comma separated paths of the directories used to look for included files.
Note: The order of the files here is important.
File search will stop at the the first found among the listed directories.
e.g --include_dir_paths directory1,directory2
if "A.sv" exists in both "directory1" and "directory2" the one in "directory1" is the one we will use.
)");

namespace verilog {
namespace kythe {

// Prints Kythe facts in proto format to stdout.
static void PrintKytheFactsProtoEntries(
    const IndexingFactNode& file_list_facts_tree, const VerilogProject& project,
    int fd) {
  KytheProtoOutput proto_output(fd);
  StreamKytheFactsEntries(&proto_output, file_list_facts_tree, project);
}

// Just collect the facts, but don't print anything. Mostly useful for
// debugging error checking or performance.
static void KytheFactsNullPrinter(const IndexingFactNode& file_list_facts_tree,
                                  const VerilogProject& project) {
  class NullPrinter final : public KytheOutput {
   public:
    void Emit(const Fact& fact) final {}
    void Emit(const Edge& edge) final {}
  } printer;
  StreamKytheFactsEntries(&printer, file_list_facts_tree, project);
}

static std::vector<absl::Status> ExtractTranslationUnits(
    absl::string_view file_list_path, VerilogProject* project,
    const std::vector<std::string>& file_names) {
  const verilog::kythe::IndexingFactNode file_list_facts_tree(
      verilog::kythe::ExtractFiles(file_list_path, project, file_names));

  // check for printextraction flag, and print extraction if on
  if (absl::GetFlag(FLAGS_printextraction)) {
    // Don't use std::cout unless KytheFactsPrinter uses another stream.
    LOG(INFO) << file_list_facts_tree << std::endl;
  }

  // check how to output kythe facts.
  switch (absl::GetFlag(FLAGS_print_kythe_facts)) {
    case PrintMode::kJSON:
      std::cout << KytheFactsPrinter(file_list_facts_tree, *project)
                << std::endl;
      break;
    case PrintMode::kJSONDebug:
      std::cout << KytheFactsPrinter(file_list_facts_tree, *project,
                                     /*debug=*/true)
                << std::endl;
      break;
    case PrintMode::kProto:
      PrintKytheFactsProtoEntries(file_list_facts_tree, *project,
                                  STDOUT_FILENO);
      break;
    case PrintMode::kNone:
      KytheFactsNullPrinter(file_list_facts_tree, *project);
      break;
  }

  return project->GetErrorStatuses();
}

}  // namespace kythe
}  // namespace verilog

int main(int argc, char** argv) {
  const auto usage =
      absl::StrCat("usage: ", argv[0], " [options] --file_list_path FILE\n", R"(
Extracts kythe indexing facts from the given SystemVerilog source files.

Input: A file which lists paths to the SystemVerilog top-level translation
       unit files (one per line; the path is relative to the location of the
       file list).
Output: Produces Indexing Facts for kythe (http://kythe.io).
)");
  const auto args = verible::InitCommandLine(usage, &argc, &argv);

  const std::string file_list_path = absl::GetFlag(FLAGS_file_list_path);
  if (file_list_path.empty()) {
    LOG(ERROR) << "No file list path was specified";
    return 1;
  }
  const std::string file_list_root = absl::GetFlag(FLAGS_file_list_root);

  // Load file list.
  const auto file_list_or(verilog::ParseSourceFileListFromFile(file_list_path));
  if (!file_list_or.ok()) {
    LOG(ERROR) << "Error while reading file list: "
               << file_list_or.status().message();
    return 1;
  }
  const std::vector<std::string>& file_paths(file_list_or->file_paths);

  // List of the directories for where to look for included files.
  std::vector<std::string> include_dir_paths =
      absl::GetFlag(FLAGS_include_dir_paths);
  // Merge the include dirs from the file list.
  include_dir_paths.insert(include_dir_paths.end(),
                           file_list_or->include_dirs.begin(),
                           file_list_or->include_dirs.end());
  verilog::VerilogProject project(file_list_root, include_dir_paths);

  const std::vector<absl::Status> errors(
      verilog::kythe::ExtractTranslationUnits(file_list_path, &project,
                                              file_paths));
  if (!errors.empty()) {
    LOG(ERROR) << "Encountered some issues while indexing files (could result "
                  "in missing indexing data):"
               << std::endl;
    for (const auto& error : errors) {
      LOG(ERROR) << error.message();
    }
    // TODO(ikr): option to cause any errors to exit non-zero, like
    // (bool) --index_files_fatal.  This can signal to user/caller that
    // something went wrong, and surface errors.
  }
  return 0;
}
