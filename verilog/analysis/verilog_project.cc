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

#include <filesystem>
#include <iostream>
#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "absl/status/status.h"
#include "absl/status/statusor.h"
#include "absl/strings/str_cat.h"
#include "absl/strings/string_view.h"
#include "absl/time/clock.h"
#include "absl/time/time.h"
#include "absl/types/optional.h"
#include "common/strings/mem_block.h"
#include "common/text/text_structure.h"
#include "common/util/file_util.h"
#include "common/util/logging.h"
#include "verilog/analysis/verilog_analyzer.h"

namespace verilog {

// All files we process with the verilog project, essentially applications that
// build a symbol table (project-tool, kythe-indexer) only benefit from
// processing the same sequence of tokens a synthesis tool sees.
static constexpr verilog::VerilogPreprocess::Config kPreprocessConfig{
    .filter_branches = true,
};

VerilogSourceFile::VerilogSourceFile(absl::string_view referenced_path,
                                     const absl::Status &status)
    : referenced_path_(referenced_path), status_(status) {}

absl::Status VerilogSourceFile::Open() {
  // Don't re-open.  analyzed_structure_ should be set/written once only.
  if (processing_state_ != ProcessingState::kInitialized) return status_;

  // Load file contents.
  auto content_status = verible::file::GetContentAsMemBlock(ResolvedPath());
  status_ = content_status.status();
  if (!status_.ok()) return status_;

  content_ = std::move(*content_status);
  processing_state_ = ProcessingState::kOpened;

  return status_;  // status_ is Ok here.
}

absl::string_view VerilogSourceFile::GetContent() const {
  return content_ ? content_->AsStringView() : "";
}

absl::Status VerilogSourceFile::Parse() {
  // Parsed state is cached.
  if (processing_state_ == ProcessingState::kParsed) return status_;

  // Open file and load contents if not already done.
  status_ = Open();
  if (!status_.ok()) return status_;

  // Lex, parse, populate underlying TextStructureView.
  analyzed_structure_ = std::make_unique<VerilogAnalyzer>(
      content_, ResolvedPath(), kPreprocessConfig);

  const absl::Time start = absl::Now();
  status_ = analyzed_structure_->Analyze();
  const absl::Duration analyze_time = absl::Now() - start;
  if (analyze_time > absl::Milliseconds(500)) {
    LOG(WARNING) << "Slow Parse " << ResolvedPath() << " took " << analyze_time;
  } else {
    VLOG(2) << "Parse " << ResolvedPath() << " in " << analyze_time;
  }

  processing_state_ = ProcessingState::kParsed;
  return status_;
}

const verible::TextStructureView *VerilogSourceFile::GetTextStructure() const {
  if (analyzed_structure_ == nullptr) return nullptr;
  return &analyzed_structure_->Data();
}

std::vector<std::string> VerilogSourceFile::ErrorMessages() const {
  std::vector<std::string> result;
  if (!analyzed_structure_) return result;
  result = analyzed_structure_->LinterTokenErrorMessages(false);
  return result;
}

std::ostream &operator<<(std::ostream &stream,
                         const VerilogSourceFile &source) {
  stream << "referenced path: " << source.ReferencedPath() << std::endl;
  stream << "resolved path: " << source.ResolvedPath() << std::endl;
  stream << "corpus: " << source.Corpus() << std::endl;
  const absl::Status status = source.Status();
  stream << "status: " << (status.ok() ? "ok" : status.message()) << std::endl;
  const auto content = source.GetContent();
  stream << "have content? " << (!content.empty() ? "yes" : "no") << std::endl;
  const auto *text_structure = source.GetTextStructure();
  stream << "have text structure? " << (text_structure ? "yes" : "no")
         << std::endl;
  return stream;
}

void VerilogProject::ContentToFileIndex::Register(
    const VerilogSourceFile *file) {
  CHECK(file);
  const absl::string_view content = file->GetContent();
  string_view_map_.must_emplace(content);
  const auto map_inserted =
      buffer_to_analyzer_map_.emplace(content.begin(), file);
  CHECK(map_inserted.second);
}

void VerilogProject::ContentToFileIndex::Unregister(
    const VerilogSourceFile *file) {
  CHECK(file);
  const absl::string_view content = file->GetContent();
  auto full_content_found = string_view_map_.find(content);
  if (full_content_found != string_view_map_.end()) {
    string_view_map_.erase(full_content_found);
  }
  buffer_to_analyzer_map_.erase(content.begin());
}

const VerilogSourceFile *VerilogProject::ContentToFileIndex::Lookup(
    absl::string_view content_substring) const {
  // Look for corresponding source text (superstring) buffer start.
  const auto found_superstring = string_view_map_.find(content_substring);
  if (found_superstring == string_view_map_.end()) return nullptr;
  const absl::string_view::const_iterator buffer_start =
      found_superstring->first;

  // Reverse-lookup originating file based on buffer start.
  const auto found_file = buffer_to_analyzer_map_.find(buffer_start);
  if (found_file == buffer_to_analyzer_map_.end()) return nullptr;

  return found_file->second;
}

absl::StatusOr<VerilogSourceFile *> VerilogProject::OpenFile(
    absl::string_view referenced_filename, absl::string_view resolved_filename,
    absl::string_view corpus) {
  const auto inserted = files_.emplace(
      referenced_filename, std::make_unique<VerilogSourceFile>(
                               referenced_filename, resolved_filename, corpus));
  CHECK(inserted.second);  // otherwise, would have already returned above
  const auto file_iter = inserted.first;
  VerilogSourceFile &file(*file_iter->second);

  // Read the file's contents.
  if (absl::Status status = file.Open(); !status.ok()) {
    return status;
  }

  if (content_index_) content_index_->Register(&file);

  return &file;
}

bool VerilogProject::RemoveByName(const std::string &filename) {
  NameToFileMap::const_iterator found = files_.find(filename);
  if (found == files_.end()) return false;
  if (content_index_) content_index_->Unregister(found->second.get());
  files_.erase(found);
  return true;
}

// TODO: explain better in the header what happens with includes.
bool VerilogProject::RemoveRegisteredFile(
    absl::string_view referenced_filename) {
  if (RemoveByName(std::string(referenced_filename))) {
    LOG(INFO) << "Removed " << referenced_filename << " from the project.";
    return true;
  }
  for (const auto &include_path : include_paths_) {
    const std::string resolved_filename =
        verible::file::JoinPath(include_path, referenced_filename);
    if (RemoveByName(resolved_filename)) {
      LOG(INFO) << "Removed " << resolved_filename << " from the project.";
      return true;
    }
  }
  return false;
}

std::string VerilogProject::GetRelativePathToSource(
    const absl::string_view absolute_filepath) {
  // TODO add check if the absolute_filepath is out of the VerilogProject
  std::filesystem::path absolutepath{absolute_filepath.begin(),
                                     absolute_filepath.end()};
  auto root = TranslationUnitRoot();
  std::filesystem::path projectpath{root.begin(), root.end()};
  auto relative = std::filesystem::relative(absolutepath, projectpath);
  return relative.string();
}

void VerilogProject::UpdateFileContents(
    absl::string_view path, const verilog::VerilogAnalyzer *parsed) {
  constexpr absl::string_view kCorpus = "";
  const std::string projectpath = GetRelativePathToSource(path);

  // If we get a non-null parsed file, use that, otherwise fall back to
  // file based loading.
  std::unique_ptr<VerilogSourceFile> source_file;
  bool do_register_content = true;
  if (parsed) {
    source_file = std::make_unique<ParsedVerilogSourceFile>(projectpath, path,
                                                            *parsed, kCorpus);
  } else {
    source_file =
        std::make_unique<VerilogSourceFile>(projectpath, path, kCorpus);
    do_register_content = source_file->Open().ok();
  }

  if (content_index_ && do_register_content) {
    content_index_->Register(source_file.get());
  }

  auto previously_existing = files_.find(projectpath);
  if (previously_existing == files_.end()) {
    files_.emplace(projectpath, std::move(source_file));
  } else {
    if (content_index_) {
      content_index_->Unregister(previously_existing->second.get());
    }
    previously_existing->second = std::move(source_file);
  }
}

VerilogSourceFile *VerilogProject::LookupRegisteredFileInternal(
    absl::string_view referenced_filename) const {
  const auto opened_file = FindOpenedFile(referenced_filename);
  if (opened_file) {
    if (!opened_file->ok()) {
      return nullptr;
    }
    return opened_file->value();
  }

  // Check if this is already opened include file
  for (const auto &include_path : include_paths_) {
    const std::string resolved_filename =
        verible::file::JoinPath(include_path, referenced_filename);
    const auto opened_file = FindOpenedFile(resolved_filename);
    if (opened_file) {
      return opened_file->ok() ? opened_file->value() : nullptr;
    }
  }
  return nullptr;
}

absl::optional<absl::StatusOr<VerilogSourceFile *>>
VerilogProject::FindOpenedFile(absl::string_view filename) const {
  const auto found = files_.find(filename);
  if (found != files_.end()) {
    if (absl::Status status = found->second->Status(); !status.ok()) {
      return status;
    }
    return found->second.get();
  }
  return absl::nullopt;
}

absl::StatusOr<VerilogSourceFile *> VerilogProject::OpenTranslationUnit(
    absl::string_view referenced_filename) {
  // Check for a pre-existing entry to avoid duplicate files.
  {
    const auto opened_file = FindOpenedFile(referenced_filename);
    if (opened_file) {
      return *opened_file;
    }
  }

  // Locate the file among the base paths.
  const std::string resolved_filename =
      verible::file::JoinPath(TranslationUnitRoot(), referenced_filename);
  // Check if this is already opened file
  {
    const auto opened_file = FindOpenedFile(resolved_filename);
    if (opened_file) {
      return *opened_file;
    }
  }

  return OpenFile(referenced_filename, resolved_filename, Corpus());
}

absl::Status VerilogProject::IncludeFileNotFoundError(
    absl::string_view referenced_filename) const {
  return absl::NotFoundError(
      absl::StrCat("'", referenced_filename, "' not in any of the ",
                   include_paths_.size(), " include paths"));
}

absl::StatusOr<VerilogSourceFile *> VerilogProject::OpenIncludedFile(
    absl::string_view referenced_filename) {
  VLOG(2) << __FUNCTION__ << ", referenced: " << referenced_filename;
  // Check for a pre-existing entry to avoid duplicate files.
  {
    const auto opened_file = FindOpenedFile(referenced_filename);
    if (opened_file) {
      return *opened_file;
    }
  }

  // Check if this is already opened include file
  for (const auto &include_path : include_paths_) {
    const std::string resolved_filename =
        verible::file::JoinPath(include_path, referenced_filename);
    const auto opened_file = FindOpenedFile(resolved_filename);
    if (opened_file) {
      return *opened_file;
    }
  }

  // Locate the file among the base paths.
  for (const auto &include_path : include_paths_) {
    const std::string resolved =
        verible::file::JoinPath(include_path, referenced_filename);
    if (verible::file::FileExists(resolved).ok()) {
      VLOG(2) << referenced_filename << " in incdir '" << resolved << "'";
      return OpenFile(referenced_filename, resolved, Corpus());
    }
    VLOG(2) << referenced_filename << " not in incdir '" << resolved << "'";
  }

  VLOG(1) << __FUNCTION__ << "': '" << referenced_filename << "' not found";
  // Not found in any path.  Cache this status.
  const auto inserted = files_.emplace(
      referenced_filename,
      std::make_unique<VerilogSourceFile>(
          referenced_filename, IncludeFileNotFoundError(referenced_filename)));
  CHECK(inserted.second) << "Not-found file should have been recorded as such.";
  return inserted.first->second->Status();
}

void VerilogProject::AddVirtualFile(absl::string_view resolved_filename,
                                    absl::string_view content) {
  const auto inserted = files_.emplace(
      resolved_filename,
      std::make_unique<InMemoryVerilogSourceFile>(
          resolved_filename, std::make_shared<verible::StringMemBlock>(content),
          /*corpus=*/""));
  if (!inserted.second) {
    const bool same_content = inserted.first->second->GetContent() == content;
    LOG(WARNING) << "The virtual file " << resolved_filename
                 << " is already registered with the project (with "
                 << (same_content ? "same" : "different") << " content)";
  }
}

const VerilogSourceFile *VerilogProject::LookupFileOrigin(
    absl::string_view content_substring) const {
  CHECK(content_index_) << "LookupFileOrigin() not enabled in constructor";
  return content_index_->Lookup(content_substring);
}

}  // namespace verilog
