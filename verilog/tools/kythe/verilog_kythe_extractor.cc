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

static int ExtractFiles(std::vector<std::string> ordered_file_list,
                        absl::string_view file_list_dir) {
  int exit_status = 0;

  const verilog::kythe::IndexingFactNode file_list_facts_tree(
      verilog::kythe::ExtractFiles(ordered_file_list, exit_status,
                                      file_list_dir));

  // check for printextraction flag, and print extraction if on
  if (absl::GetFlag(FLAGS_printextraction)) {
    std::cout << file_list_facts_tree << std::endl;
  }
  LOG(INFO) << '\n' << file_list_facts_tree;

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

// update usage
int main(int argc, char** argv) {
  const auto usage =
      absl::StrCat("usage: ", argv[0], " [options] <file> [<file>...]\n",
                   R"(
verilog_kythe_extractor is a simple command-line utility
to extract kythe indexing facts from the given file.

Expected Input: verilog file.
Expected output: Produces Indexing Facts for kythe.

Example usage:
verible-verilog-kythe-extractor files...)");

  const auto args = verible::InitCommandLine(usage, &argc, &argv);

  std::vector<verilog::kythe::IndexingFactNode> indexing_facts_trees;

  std::string content;
  if (!verible::file::GetContents(args[1], &content).ok()) {
    LOG(INFO) << "Erro while reading file: " << args[1];
    return 1;
  };

  std::vector<std::string> files_names;
  std::string filename;

  std::stringstream stream(content);
  while (stream >> filename) {
    files_names.push_back(filename);
  }

  int exit_status = ExtractFiles(files_names, verible::file::Direname(args[1]));
  return exit_status;
}
