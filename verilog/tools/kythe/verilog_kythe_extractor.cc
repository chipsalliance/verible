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

#include <iostream>

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

int ExtractOneFile(absl::string_view content, absl::string_view filename) {
  int exit_status = 0;
  bool parse_ok = false;

  const verilog::kythe::IndexingFactNode& facts_tree =
      verilog::kythe::ExtractOneFile(content, filename, exit_status, parse_ok);

  // check for printextraction flag, and print extraction if on
  if (absl::GetFlag(FLAGS_printextraction)) {
    std::cout << std::endl
              << (!parse_ok ? " (incomplete due to syntax errors): " : "")
              << std::endl;

    std::cout << facts_tree << '\n';
  }
  LOG(INFO) << '\n' << facts_tree;

  // check for printkythefacts flag, and print the facts if on
  if (absl::GetFlag(FLAGS_printkythefacts)) {
    std::cout << std::endl
              << (!parse_ok ? " (incomplete due to syntax errors): " : "")
              << std::endl;

    std::cout << verilog::kythe::KytheFactsPrinter(facts_tree) << '\n';
  }

  return exit_status;
}

// verilog_kythe_extractor is a simple command-line utility to extract kythe
// indexing facts from the given file.
//
// Expected Input: verilog file.
// Expected output: Produces Indexing Facts for kythe.
//
// Example usage:
// verilog_kythe_extractor files...
int main(int argc, char** argv) {
  const auto usage =
      absl::StrCat("usage: ", argv[0], " [options] <file> [<file>...]\n",
                   " verilog_kythe_extractor is a simple command-line utility "
                   "to extract kythe indexing facts from the given file.\n"
                   "\n"
                   " Expected Input: verilog file.\n"
                   " Expected output: Produces Indexing Facts for kythe.\n"
                   "\n"
                   " Example usage:\n"
                   " verible-verilog_kythe_extractor files...");

  const auto args = verible::InitCommandLine(usage, &argc, &argv);

  int exit_status = 0;
  // All positional arguments are file names.  Exclude program name.
  for (const auto filename :
       verible::make_range(args.begin() + 1, args.end())) {
    std::string content;
    if (!verible::file::GetContents(filename, &content).ok()) {
      exit_status = 1;
      continue;
    }

    int file_status = ExtractOneFile(content, filename);
    exit_status = std::max(exit_status, file_status);
  }
  return exit_status;
}
