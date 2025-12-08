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

#include <string>
#include <string_view>
#include <vector>

#include "absl/flags/flag.h"
#include "absl/status/statusor.h"
#include "absl/strings/str_cat.h"
#include "third_party/proto/kythe/analysis.pb.h"
#include "verible/common/util/file-util.h"
#include "verible/common/util/init-command-line.h"
#include "verible/common/util/logging.h"
#include "verible/verilog/analysis/verilog-filelist.h"
#include "verible/verilog/tools/kythe/kzip-creator.h"

ABSL_FLAG(std::string, filelist_path, "",
          "The path to the file list which contains the names of SystemVerilog "
          "files. The files should be ordered by definition dependencies.");

ABSL_FLAG(std::string, code_revision, "",
          "Version control revision at which this code was taken.");

ABSL_FLAG(std::string, corpus, "",
          "Corpus (e.g., the project) to which this code belongs.");

ABSL_FLAG(std::string, output_path, "", "Path where to write the kzip.");

ABSL_RETIRED_FLAG(
    std::string, filelist_root, ".",
    "The absolute location which we prepend to the files in the file "
    "list (where listed files are relative to).");

int main(int argc, char **argv) {
  const auto usage = absl::StrCat("usage: ", argv[0],
                                  " [options] --filelist_path FILE "
                                  R"(
Produces Kythe KZip from the given SystemVerilog source files.

Input: A file which lists paths to the SystemVerilog top-level translation
       unit files (one per line; the path is relative to the location of the
       file list).
Output: Produces Kythe KZip (https://kythe.io/docs/kythe-kzip.html).
)");
  const auto args = verible::InitCommandLine(usage, &argc, &argv);

  const std::string filelist_path = absl::GetFlag(FLAGS_filelist_path);
  if (filelist_path.empty()) {
    LOG(ERROR) << "No --filelist_path was specified";
    return 1;
  }
  const std::string output_path = absl::GetFlag(FLAGS_output_path);
  if (output_path.empty()) {
    LOG(ERROR) << "No --output_path was specified";
    return 1;
  }

  // Load file list.
  const absl::StatusOr<std::string> filelist_content_or =
      verible::file::GetContentAsString(filelist_path);
  CHECK(filelist_content_or.ok())
      << "Failed to open the file list at " << filelist_path;
  verilog::FileList filelist;
  if (auto status = verilog::AppendFileListFromContent(
          filelist_path, *filelist_content_or, &filelist);
      !status.ok()) {
    LOG(ERROR) << "Filelist parse error " << status;
    return 1;
  }
  // Normalize the file list
  std::string_view filelist_root = verible::file::Dirname(filelist_path);
  for (std::string &file_path : filelist.file_paths) {
    file_path = verible::file::JoinPath(filelist_root, file_path);
  }
  for (std::string &include : filelist.preprocessing.include_dirs) {
    include = verible::file::JoinPath(filelist_root, include);
  }

  kythe::proto::IndexedCompilation compilation;
  const std::string code_revision = absl::GetFlag(FLAGS_code_revision);
  if (!code_revision.empty()) {
    compilation.mutable_index()->add_revisions(code_revision);
  }

  auto *unit = compilation.mutable_unit();
  *unit->mutable_v_name()->mutable_corpus() = absl::GetFlag(FLAGS_corpus);
  *unit->mutable_v_name()->mutable_language() = "verilog";
  *unit->add_argument() = "--f=filelist";

  // Construct Verible project
  const std::vector<std::string> &file_paths(filelist.file_paths);

  verilog::kythe::KzipCreator kzip(output_path);
  const std::string filelist_digest =
      kzip.AddSourceFile("filelist", filelist.ToString());
  auto *filelist_input = unit->add_required_input();
  *filelist_input->mutable_info()->mutable_path() = "filelist";
  *filelist_input->mutable_info()->mutable_digest() = filelist_digest;
  for (const std::string &file_path : file_paths) {
    auto content_or = verible::file::GetContentAsString(file_path);
    if (!content_or.ok()) {
      LOG(ERROR) << "Failed to open " << file_path
                 << ". Error: " << content_or.status();
      continue;
    }
    const std::string digest = kzip.AddSourceFile(file_path, *content_or);
    auto *file_input = unit->add_required_input();
    *file_input->mutable_info()->mutable_path() = file_path;
    *file_input->mutable_info()->mutable_digest() = digest;
    *file_input->mutable_v_name()->mutable_path() = file_path;
  }
  CHECK(kzip.AddCompilationUnit(compilation).ok());

  return 0;
}
