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
#include "common/util/file_util.h"
#include "common/util/init_command_line.h"
#include "google/protobuf/io/zero_copy_stream_impl.h"
#include "kythe/cxx/common/indexing/KytheCachingOutput.h"
#include "kythe/proto/storage.pb.h"
#include "verilog/analysis/verilog_analyzer.h"
#include "verilog/tools/kythe/indexing_facts_tree_extractor.h"
#include "verilog/tools/kythe/kythe_facts_extractor.h"

ABSL_FLAG(bool, printextraction, false,
          "Whether or not to print the extracted general indexing facts "
          "from the middle layer)");

ABSL_FLAG(bool, print_kythe_facts_json, false,
          "If true, prints the extracted Kythe facts in JSON format");

ABSL_FLAG(bool, print_kythe_facts_proto, false,
          "If true, prints the extracted Kythe facts in protobuf format");

ABSL_FLAG(
    std::string, file_list_path, "",
    R"(The path to the file list which contains the names of SystermVerilog files.)");

ABSL_FLAG(
    std::string, file_list_root, "",
    R"(The absolute location which we prepend to the files in the file list (where listed files are relative to).)");

// TODO: support repeatable flag
ABSL_FLAG(
    std::vector<std::string>, include_dir_paths, {},
    R"(Comma seperated paths of the directories used to look for included files.
Note: The order of the files here is important.
e.g --include_dir_paths dir1 dir2
if "A.sv" exists in both "dir1" and "dir2" the one in "dir1" is the one we will use.
)");

namespace verilog {
namespace kythe {

using ::kythe::EdgeRef;
using ::kythe::FactRef;
using ::kythe::VNameRef;

// Returns VNameRef based on Verible's VName.
VNameRef ConvertToVnameRef(const VName& vname,
                           std::deque<std::string>* signatures) {
  VNameRef vname_ref;
  signatures->push_back(vname.signature.ToString());
  vname_ref.set_signature(signatures->back());
  vname_ref.set_corpus(vname.corpus);
  vname_ref.set_root(vname.root);
  vname_ref.set_path(vname.path);
  vname_ref.set_language(vname.language);
  return vname_ref;
}

// Prints Kythe facts in proto format to stdout.
void PrintKytheFactsProtoEntries(const IndexingFactNode& file_list_facts_tree) {
  KytheFactsExtractor kythe_extractor(
      GetFileListDirFromRoot(file_list_facts_tree),
      /*previous_files_scopes=*/nullptr);
  auto indexing_data = kythe_extractor.ExtractKytheFacts(file_list_facts_tree);
  google::protobuf::io::FileOutputStream file_output(STDOUT_FILENO);
  file_output.SetCloseOnDelete(true);
  ::kythe::FileOutputStream kythe_output(&file_output);
  kythe_output.set_flush_after_each_entry(true);

  // Keep signatures alive. VNameRef takes a view. Note that we need pointer
  // stability, can't use a vector here!
  std::deque<std::string> signatures;
  for (const Fact& fact : indexing_data.facts) {
    FactRef fact_ref;
    VNameRef source = ConvertToVnameRef(fact.node_vname, &signatures);
    fact_ref.fact_name = fact.fact_name;
    fact_ref.fact_value = fact.fact_value;
    fact_ref.source = &source;
    kythe_output.Emit(fact_ref);
  }
  for (const Edge& edge : indexing_data.edges) {
    EdgeRef edge_ref;
    VNameRef source = ConvertToVnameRef(edge.source_node, &signatures);
    VNameRef target = ConvertToVnameRef(edge.target_node, &signatures);
    edge_ref.edge_kind = edge.edge_name;
    edge_ref.source = &source;
    edge_ref.target = &target;
    kythe_output.Emit(edge_ref);
  }
}

static std::vector<absl::Status> ExtractFiles(
    const std::vector<std::string>& ordered_file_list,
    absl::string_view file_list_dir, absl::string_view file_list_root,
    const std::vector<std::string>& include_dir_paths) {
  std::vector<absl::Status> status;

  const verilog::kythe::IndexingFactNode file_list_facts_tree(
      verilog::kythe::ExtractFiles(ordered_file_list, status, file_list_dir,
                                   file_list_root, include_dir_paths));

  // check for printextraction flag, and print extraction if on
  if (absl::GetFlag(FLAGS_printextraction)) {
    std::cout << file_list_facts_tree << std::endl;
  }

  // check for print_kythe_facts_json flag, and print the facts if on
  if (absl::GetFlag(FLAGS_print_kythe_facts_json)) {
    std::cout << KytheFactsPrinter(file_list_facts_tree) << std::endl;
  }

  if (absl::GetFlag(FLAGS_print_kythe_facts_proto)) {
    PrintKytheFactsProtoEntries(file_list_facts_tree);
  }

  return status;
}

}  // namespace kythe
}  // namespace verilog

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

  // List of the directories for where to look for included files.
  const std::vector<std::string> include_dir_paths =
      absl::GetFlag(FLAGS_include_dir_paths);

  const std::string file_list_path = absl::GetFlag(FLAGS_file_list_path);
  if (file_list_path.empty()) {
    LOG(ERROR) << "No file list path was specified";
    return 1;
  }

  const std::string file_list_root = absl::GetFlag(FLAGS_file_list_root);
  if (file_list_root.empty()) {
    LOG(ERROR) << "No file list root was specified";
    return 1;
  }

  std::string content;
  if (!verible::file::GetContents(file_list_path, &content).ok()) {
    LOG(ERROR) << "Error while reading file list at: " << file_list_path;
    return 1;
  }

  std::vector<std::string> files_names;
  std::string filename;

  std::stringstream stream(content);
  while (stream >> filename) {
    // TODO(minatoma): ignore blank lines and "# ..." comments
    files_names.push_back(filename);
  }

  std::vector<absl::Status> status = verilog::kythe::ExtractFiles(
      files_names, file_list_path, file_list_root, include_dir_paths);
  return 0;
}
