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

#include "verilog/analysis/verilog_project.h"

#include <iostream>
#include <string>
#include <vector>

#include "absl/memory/memory.h"
#include "absl/status/status.h"
#include "absl/strings/ascii.h"
#include "absl/strings/match.h"
#include "absl/strings/str_cat.h"
#include "absl/strings/strip.h"
#include "absl/strings/str_join.h"
#include "common/text/text_structure.h"
#include "common/util/file_util.h"
#include "common/util/logging.h"
#include "verilog/analysis/verilog_analyzer.h"

namespace verilog {

VerilogSourceFile::VerilogSourceFile(absl::string_view referenced_path,
                                     const absl::Status& status)
    : referenced_path_(referenced_path), status_(status) {}

absl::Status VerilogSourceFile::Open() {
  // Don't re-open.  analyzed_structure_ should be set/written once only.
  if (state_ != State::kInitialized) return status_;

  // Load file contents.
  std::string content;
  status_ = verible::file::GetContents(ResolvedPath(), &content);
  if (!status_.ok()) return status_;

  // TODO(fangism): std::move or memory-map to avoid a short-term copy.
  analyzed_structure_ =
      absl::make_unique<VerilogAnalyzer>(content, ResolvedPath());
  state_ = State::kOpened;
  // status_ is Ok here.
  return status_;
}

absl::Status VerilogSourceFile::Parse() {
  // Parsed state is cached.
  if (state_ == State::kParsed) return status_;

  // Open file and load contents if not already done.
  status_ = Open();
  if (!status_.ok()) return status_;

  // Lex, parse, populate underlying TextStructureView.
  status_ = analyzed_structure_->Analyze();
  state_ = State::kParsed;
  return status_;
}

const verible::TextStructureView* VerilogSourceFile::GetTextStructure() const {
  if (analyzed_structure_ == nullptr) return nullptr;
  return &analyzed_structure_->Data();
}

std::ostream& operator<<(std::ostream& stream,
                         const VerilogSourceFile& source) {
  stream << "referenced path: " << source.ReferencedPath() << std::endl;
  stream << "resolved path: " << source.ResolvedPath() << std::endl;
  stream << "corpus: " << source.Corpus() << std::endl;
  const auto status = source.Status();
  stream << "status: " << (status.ok() ? "ok" : status.message()) << std::endl;
  const auto* text_structure = source.GetTextStructure();
  stream << "have text structure? "
         << ((text_structure != nullptr) ? "yes" : "no") << std::endl;
  return stream;
}

absl::Status InMemoryVerilogSourceFile::Open() {
  analyzed_structure_ = ABSL_DIE_IF_NULL(
      absl::make_unique<VerilogAnalyzer>(contents_for_open_, ResolvedPath()));
  state_ = State::kOpened;
  status_ = absl::OkStatus();
  return status_;
}

absl::Status ParsedVerilogSourceFile::Open() {
  state_ = State::kOpened;
  status_ = absl::OkStatus();
  return status_;
}

absl::Status ParsedVerilogSourceFile::Parse() {
  state_ = State::kParsed;
  status_ = absl::OkStatus();
  return status_;
}

const verible::TextStructureView* ParsedVerilogSourceFile::GetTextStructure()
    const {
  return text_structure_;
}

absl::StatusOr<VerilogSourceFile*> VerilogProject::OpenFile(
    absl::string_view referenced_filename, absl::string_view resolved_filename,
    absl::string_view corpus) {
  const auto inserted = files_.emplace(
      referenced_filename, absl::make_unique<VerilogSourceFile>(
                               referenced_filename, resolved_filename, corpus));
  CHECK(inserted.second);  // otherwise, would have already returned above
  const auto file_iter = inserted.first;
  VerilogSourceFile& file(*file_iter->second);

  // Read the file's contents.
  const absl::Status status = file.Open();
  if (!status.ok()) return status;

  const absl::string_view contents(file.GetTextStructure()->Contents());

  // Register the file's contents range in string_view_map_.
  string_view_map_.must_emplace(contents);

  // Map the start of the file's contents to its corresponding owner
  // VerilogSourceFile.
  const auto map_inserted =
      buffer_to_analyzer_map_.emplace(contents.begin(), file_iter);
  CHECK(map_inserted.second);

  return &file;
}

absl::StatusOr<VerilogSourceFile*> VerilogProject::OpenTranslationUnit(
    absl::string_view referenced_filename) {
  // Check for a pre-existing entry to avoid duplicate files.
  {
    const auto found = files_.find(referenced_filename);
    if (found != files_.end()) {
      const auto status = found->second->Status();
      if (!status.ok()) return status;
      return found->second.get();
    }
  }

  // Locate the file among the base paths.
  const std::string resolved_filename =
      verible::file::JoinPath(TranslationUnitRoot(), referenced_filename);

  return OpenFile(referenced_filename, resolved_filename, Corpus());
}

absl::Status VerilogProject::IncludeFileNotFoundError(
    absl::string_view referenced_filename) const {
  return absl::NotFoundError(absl::StrCat(
      "Unable to find '", referenced_filename,
      "' among the included paths: ", absl::StrJoin(include_paths_, ", ")));
}

absl::StatusOr<VerilogSourceFile*> VerilogProject::OpenIncludedFile(
    absl::string_view referenced_filename) {
  VLOG(1) << __FUNCTION__ << ", referenced: " << referenced_filename;
  // Check for a pre-existing entry to avoid duplicate files.
  {
    const auto found = files_.find(referenced_filename);
    if (found != files_.end()) {
      const auto status = found->second->Status();
      if (!status.ok()) return status;
      return found->second.get();
    }
  }

  // Locate the file among the base paths.
  for (const auto& include_path : include_paths_) {
    const std::string resolved_filename =
        verible::file::JoinPath(include_path, referenced_filename);
    if (verible::file::FileExists(resolved_filename).ok()) {
      VLOG(2) << "File'" << resolved_filename << "' exists.";
      return OpenFile(referenced_filename, resolved_filename, Corpus());
    }
    VLOG(2) << "Checked for file'" << resolved_filename << "', but not found.";
  }

  // Not found in any path.  Cache this status.
  const auto inserted = files_.emplace(
      referenced_filename,
      absl::make_unique<VerilogSourceFile>(
          referenced_filename, IncludeFileNotFoundError(referenced_filename)));
  CHECK(inserted.second) << "Not-found file should have been recorded as such.";
  return inserted.first->second->Status();
}

void VerilogProject::AddVirtualFile(absl::string_view resolved_filename,
                                    absl::string_view content) {
  std::string referenced_filename(resolved_filename);
  // Make the virtual file relative to the include directory.
  for (absl::string_view include_path : include_paths_) {
    if (absl::StartsWith(resolved_filename, include_path)) {
      referenced_filename =
          absl::StripPrefix(resolved_filename, absl::StrCat(include_path, "/"));
      break;
    }
  }

  const auto inserted = files_.emplace(
      referenced_filename,
      absl::make_unique<InMemoryVerilogSourceFile>(
          referenced_filename, resolved_filename, content, /*corpus=*/""));
  CHECK(inserted.second);
  const auto file_iter = inserted.first;

  // Register the file's contents range in string_view_map_.
  string_view_map_.must_emplace(content);

  // Map the start of the file's contents to its corresponding owner
  // VerilogSourceFile.
  const auto map_inserted =
      buffer_to_analyzer_map_.emplace(content.begin(), file_iter);
  CHECK(map_inserted.second);
}

std::vector<absl::Status> VerilogProject::GetErrorStatuses() const {
  std::vector<absl::Status> statuses;
  for (const auto& file : files_) {
    const auto status = file.second->Status();
    if (!status.ok()) {
      statuses.push_back(status);
    }
  }
  return statuses;
}

const VerilogSourceFile* VerilogProject::LookupFileOrigin(
    absl::string_view content_substring) const {
  // Look for corresponding source text (superstring) buffer start.
  const auto found_superstring = string_view_map_.find(content_substring);
  if (found_superstring == string_view_map_.end()) return nullptr;
  const absl::string_view::const_iterator buffer_start =
      found_superstring->first;

  // Reverse-lookup originating file based on buffer start.
  const auto found_file = buffer_to_analyzer_map_.find(buffer_start);
  if (found_file == buffer_to_analyzer_map_.end()) return nullptr;

  const VerilogSourceFile* file = found_file->second->second.get();
  return file;
}

FileList ParseSourceFileList(absl::string_view file_list_path,
                             const std::string& file_list_content) {
  constexpr absl::string_view kIncludeDirPrefix = "+incdir+";
  FileList file_list_out;
  file_list_out.file_list_path = file_list_path;
  file_list_out.include_dirs.push_back(".");
  std::string file_path;
  std::istringstream stream(file_list_content);
  while (std::getline(stream, file_path)) {
    absl::RemoveExtraAsciiWhitespace(&file_path);
    // Ignore blank lines
    if (file_path.empty()) continue;
    // Ignore "# ..." comments
    if (file_path.front() == '#') continue;
    // Ignore "// ..." comments
    if (absl::StartsWith(file_path, "//")) continue;

    if (absl::StartsWith(file_path, kIncludeDirPrefix)) {
      // Handle includes
      file_list_out.include_dirs.emplace_back(
          absl::StripPrefix(file_path, kIncludeDirPrefix));
    } else {
      // A regular file
      file_list_out.file_paths.push_back(file_path);
    }
  }
  return file_list_out;
}

absl::StatusOr<FileList> ParseSourceFileListFromFile(
    absl::string_view file_list_file) {
  std::string content;
  const auto read_status = verible::file::GetContents(file_list_file, &content);
  if (!read_status.ok()) return read_status;
  return ParseSourceFileList(file_list_file, content);
}

}  // namespace verilog
