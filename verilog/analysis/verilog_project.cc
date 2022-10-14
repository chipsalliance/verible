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
#include <memory>
#include <string>
#include <vector>

#include "absl/memory/memory.h"
#include "absl/status/status.h"
#include "absl/strings/ascii.h"
#include "absl/strings/str_cat.h"
#include "absl/strings/str_join.h"
#include "absl/strings/string_view.h"
#include "absl/time/time.h"
#include "absl/types/optional.h"
#include "common/text/text_structure.h"
#include "common/util/file_util.h"
#include "common/util/logging.h"

namespace verilog {

// All files we process with the verilog project, essentially applications that
// build a symbol table (project-tool, kythe-indexer) only benefit from
// processing the same sequence of tokens a synthesis tool sees.
/* static constexpr verilog::VerilogPreprocess::Config kPreprocessConfig{ */
/*     .filter_branches = true, */
/* }; */

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

  memory_ = std::make_unique<verible::StringMemBlock>(content);

  // TODO(fangism): std::move or memory-map to avoid a short-term copy.
  /* analyzed_structure_ = std::make_unique<VerilogAnalyzer>( */
  /*     content, ResolvedPath(), kPreprocessConfig); */
  state_ = State::kOpened;
  // status_ is Ok here.
  return status_;
}

/* absl::Status VerilogSourceFile::Parse() { */
/*   // Parsed state is cached. */
/*   if (state_ == State::kParsed) return status_; */

/*   // Open file and load contents if not already done. */
/*   status_ = Open(); */
/*   if (!status_.ok()) return status_; */

/*   // Lex, parse, populate underlying TextStructureView. */
/*   const absl::Time analyze_start = absl::Now(); */
/*   status_ = analyzed_structure_->Analyze(); */
/*   LOG(INFO) << "Analyzed " << ResolvedPath() << " in " */
/*             << absl::ToInt64Milliseconds(absl::Now() - analyze_start) <<
 * "ms"; */
/*   state_ = State::kParsed; */
/*   return status_; */
/* } */

absl::string_view VerilogSourceFile::GetStringView() const {
  return memory_->AsStringView();
}

/* const verible::TextStructureView* VerilogSourceFile::GetTextStructure() const
 * { */
/*   if (analyzed_structure_ == nullptr) return nullptr; */
/*   return &analyzed_structure_->Data(); */
/* } */

/* std::vector<std::string> VerilogSourceFile::ErrorMessages() const { */
/*   std::vector<std::string> result; */
/*   if (!analyzed_structure_) return result; */
/*   result = analyzed_structure_->LinterTokenErrorMessages(false); */
/*   return result; */
/* } */

std::ostream& operator<<(std::ostream& stream,
                         const VerilogSourceFile& source) {
  stream << "referenced path: " << source.ReferencedPath() << std::endl;
  stream << "resolved path: " << source.ResolvedPath() << std::endl;
  stream << "corpus: " << source.Corpus() << std::endl;
  const auto status = source.Status();
  stream << "status: " << (status.ok() ? "ok" : status.message()) << std::endl;
  /* const auto* text_structure = source.GetTextStructure(); */
  /* stream << "have text structure? " */
  /*        << ((text_structure != nullptr) ? "yes" : "no") << std::endl; */
  return stream;
}

absl::Status InMemoryVerilogSourceFile::Open() {
  memory_ = std::make_unique<verible::StringMemBlock>(contents_for_open_);
  state_ = State::kOpened;
  status_ = absl::OkStatus();
  return status_;
}

/* absl::Status ParsedVerilogSourceFile::Open() { */
/*   state_ = State::kOpened; */
/*   status_ = absl::OkStatus(); */
/*   return status_; */
/* } */

/* absl::Status ParsedVerilogSourceFile::Parse() { */
/*   state_ = State::kParsed; */
/*   status_ = absl::OkStatus(); */
/*   return status_; */
/* } */

/* const verible::TextStructureView* ParsedVerilogSourceFile::GetTextStructure()
 */
/*     const { */
/*   return text_structure_; */
/* } */

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

  // NOTE: string view maps don't support removal operation. The following block
  // is valid only if files won't be removed from the project.
  if (populate_string_maps_) {
    /* const absl::string_view contents(file.GetTextStructure()->Contents()); */
    const absl::string_view contents = file.memory_->AsStringView();

    // Register the file's contents range in string_view_map_.
    string_view_map_.must_emplace(contents);

    // Map the start of the file's contents to its corresponding owner
    // VerilogSourceFile.
    const auto map_inserted =
        buffer_to_analyzer_map_.emplace(contents.begin(), file_iter);
    CHECK(map_inserted.second);
  }

  return &file;
}

bool VerilogProject::RemoveRegisteredFile(
    absl::string_view referenced_filename) {
  CHECK(!populate_string_maps_)
      << "Removing of files added to string maps is not supported! Disable "
         "populating string maps.";
  if (files_.erase(std::string(referenced_filename)) == 1) {
    LOG(INFO) << "Removed " << referenced_filename << " from the project.";
    return true;
  }
  for (const auto& include_path : include_paths_) {
    const std::string resolved_filename =
        verible::file::JoinPath(include_path, referenced_filename);
    if (files_.erase(resolved_filename) == 1) {
      LOG(INFO) << "Removed " << resolved_filename << " from the project.";
      return true;
    }
  }
  return false;
}

VerilogSourceFile* VerilogProject::LookupRegisteredFileInternal(
    absl::string_view referenced_filename) const {
  const auto opened_file = FindOpenedFile(referenced_filename);
  if (opened_file) {
    if (!opened_file->ok()) {
      return nullptr;
    }
    return opened_file->value();
  }

  // Check if this is already opened include file
  for (const auto& include_path : include_paths_) {
    const std::string resolved_filename =
        verible::file::JoinPath(include_path, referenced_filename);
    const auto opened_file = FindOpenedFile(resolved_filename);
    if (opened_file) {
      return opened_file->ok() ? opened_file->value() : nullptr;
    }
  }
  return nullptr;
}

absl::optional<absl::StatusOr<VerilogSourceFile*>>
VerilogProject::FindOpenedFile(absl::string_view filename) const {
  const auto found = files_.find(filename);
  if (found != files_.end()) {
    const auto status = found->second->Status();
    if (!status.ok()) return status;
    return found->second.get();
  }
  return absl::nullopt;
}

absl::StatusOr<VerilogSourceFile*> VerilogProject::OpenTranslationUnit(
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
  return absl::NotFoundError(absl::StrCat(
      "Unable to find '", referenced_filename,
      "' among the included paths: ", absl::StrJoin(include_paths_, ", ")));
}

absl::StatusOr<VerilogSourceFile*> VerilogProject::OpenIncludedFile(
    absl::string_view referenced_filename) {
  VLOG(1) << __FUNCTION__ << ", referenced: " << referenced_filename;
  // Check for a pre-existing entry to avoid duplicate files.
  {
    const auto opened_file = FindOpenedFile(referenced_filename);
    if (opened_file) {
      return *opened_file;
    }
  }

  // Check if this is already opened include file
  for (const auto& include_path : include_paths_) {
    const std::string resolved_filename =
        verible::file::JoinPath(include_path, referenced_filename);
    const auto opened_file = FindOpenedFile(resolved_filename);
    if (opened_file) {
      return *opened_file;
    }
  }

  // Locate the file among the base paths.
  for (const auto& include_path : include_paths_) {
    const std::string resolved_filename =
        verible::file::JoinPath(include_path, referenced_filename);
    if (verible::file::FileExists(resolved_filename).ok()) {
      VLOG(2) << "File '" << resolved_filename << "' exists. Resolved from '"
              << referenced_filename << "'";
      return OpenFile(referenced_filename, resolved_filename, Corpus());
    }
    VLOG(2) << "Checked for file'" << resolved_filename
            << "', but not found. Resolved from '" << referenced_filename
            << "'";
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
  const auto inserted = files_.emplace(
      resolved_filename, absl::make_unique<InMemoryVerilogSourceFile>(
                             resolved_filename, content, /*corpus=*/""));
  CHECK(inserted.second);
}

const VerilogSourceFile* VerilogProject::LookupFileOrigin(
    absl::string_view content_substring) const {
  CHECK(populate_string_maps_)
      << "Populating string maps must be enabled for LookupFileOrigin!";
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

}  // namespace verilog
