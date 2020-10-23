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

#include <fstream>
#include <iostream>
#include <string>

#include "absl/flags/flag.h"
#include "absl/status/status.h"
#include "absl/strings/str_cat.h"
#include "absl/strings/string_view.h"
#include "absl/strings/substitute.h"
#include "common/util/file_util.h"
#include "common/util/init_command_line.h"
#include "verilog/analysis/verilog_analyzer.h"
#include "verilog/tools/kythe/indexing_facts_tree_extractor.h"
#include "verilog/tools/kythe/kythe_facts_extractor.h"

ABSL_FLAG(bool, printextraction, false,
          "Whether or not to print the extracted general indexing facts "
          "from the middle layer)");

ABSL_FLAG(bool, printkythefacts, false,
          "Whether or not to print the extracted kythe facts");

ABSL_FLAG(std::string, output_path, "",
          "File path where to write the extracted Kythe facts in JSON format.");

ABSL_FLAG(std::string, include_dir_paths, "",
          "Paths of the directorires used to look for included files.");

static int ExtractFiles(const std::vector<std::string>& ordered_file_list,
                        absl::string_view file_list_dir,
                        const std::vector<std::string>& include_dir_paths) {
  int exit_status = 0;

  const verilog::kythe::IndexingFactNode file_list_facts_tree(
      verilog::kythe::ExtractFiles(ordered_file_list, exit_status,
                                   file_list_dir, include_dir_paths));

  // check for printextraction flag, and print extraction if on
  if (absl::GetFlag(FLAGS_printextraction)) {
    std::cout << file_list_facts_tree << std::endl;
  }

  // check for printkythefacts flag, and print the facts if on
  if (absl::GetFlag(FLAGS_printkythefacts)) {
    std::cout << verilog::kythe::KytheFactsPrinter(file_list_facts_tree)
              << std::endl;
  }

  const std::string output_path = absl::GetFlag(FLAGS_output_path);
  if (!output_path.empty()) {
    std::ofstream f(output_path.c_str());
    if (!f.good()) {
      LOG(FATAL) << "Can't write to " << output_path;
    }
    f << verilog::kythe::KytheFactsPrinter(file_list_facts_tree) << std::endl;
  }

  return exit_status;
}

int main(int argc, char** argv) {
  const auto usage =
      absl::StrCat("usage: ", argv[0], " [options] file_list_path\n", R"(
Extracts kythe indexing facts from the given SystemVerilog source files.

Input: A file which lists paths to the SystemVerilog top-level translation
       unit files (one per line; the path is relative to the location of the
       file list).
Output: Produces Indexing Facts for kythe (http://kythe.io).
)");
  const auto args = verible::InitCommandLine(usage, &argc, &argv);

  std::vector<std::string> include_dir_paths;
  const std::string include_dir_string = absl::GetFlag(FLAGS_include_dir_paths);
  if (!include_dir_string.empty()) {
    const std::vector<absl::string_view> paths =
        absl::StrSplit(include_dir_string.c_str(), ',');

    for (absl::string_view path : paths) {
      include_dir_paths.push_back(std::string(path));
    }
  }

  std::string content;
  if (!verible::file::GetContents(args[1], &content).ok()) {
    LOG(ERROR) << "Error while reading file: " << args[1];
    return 1;
  }

  std::vector<std::string> files_names;
  std::string filename;

  std::stringstream stream(content);
  while (stream >> filename) {
    // TODO(minatoma): ignore blank lines and "# ..." comments
    files_names.push_back(filename);
  }

  int exit_status = ExtractFiles(files_names, verible::file::Dirname(args[1]),
                                 include_dir_paths);
  return exit_status;
}
