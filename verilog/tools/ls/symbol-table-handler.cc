// Copyright 2021 The Verible Authors.
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
//

#include "verilog/tools/ls/symbol-table-handler.h"

#include <filesystem>

#include "absl/flags/flag.h"
#include "common/strings/line_column_map.h"
#include "common/util/file_util.h"
#include "verilog/analysis/verilog_filelist.h"

ABSL_FLAG(std::string, file_list_path, "verible.filelist",
          "Name of the file with Verible FileList for the project");

namespace verilog {

static constexpr absl::string_view kFileSchemePrefix = "file://";

absl::string_view LSPUriToPath(absl::string_view uri) {
  if (!absl::StartsWith(uri, kFileSchemePrefix)) return "";
  return uri.substr(kFileSchemePrefix.size());
}

std::string PathToLSPUri(absl::string_view path) {
  std::filesystem::path p(path.begin(), path.end());
  return absl::StrCat(kFileSchemePrefix, std::filesystem::absolute(p).string());
}

std::string FindFileList(absl::string_view current_dir) {
  // search for FileList file up the directory hierarchy
  std::string projectpath;
  if (auto status = verible::file::UpwardFileSearch(
          current_dir, absl::GetFlag(FLAGS_file_list_path), &projectpath);
      !status.ok()) {
    LOG(WARNING) << "Could not find " << absl::GetFlag(FLAGS_file_list_path)
                 << " file in the project root (" << current_dir
                 << "):  " << status;
    return "";
  }
  LOG(INFO) << "Found file list under " << projectpath;
  return projectpath;
}

void SymbolTableHandler::SetProject(
    const std::shared_ptr<VerilogProject> &project) {
  curr_project_ = project;
  ResetSymbolTable();
  if (curr_project_) LoadProjectFileList(curr_project_->TranslationUnitRoot());
}

void SymbolTableHandler::ResetSymbolTable() {
  symbol_table_ = std::make_unique<SymbolTable>(curr_project_.get());
}

std::vector<absl::Status> SymbolTableHandler::BuildSymbolTableFor(
    const VerilogSourceFile &file) {
  return BuildSymbolTable(file, symbol_table_.get(), curr_project_.get());
}

std::vector<absl::Status> SymbolTableHandler::BuildProjectSymbolTable() {
  if (!curr_project_) {
    return {absl::UnavailableError("VerilogProject is not set")};
  }
  ResetSymbolTable();
  LOG(INFO) << "Parsing project files...";
  std::vector<absl::Status> buildstatus;
  symbol_table_->Build(&buildstatus);
  for (const auto &diagnostic : buildstatus) {
    LOG(WARNING) << diagnostic.message();
  }
  std::vector<absl::Status> resolvestatus;
  symbol_table_->Resolve(&resolvestatus);
  for (const auto &diagnostic : resolvestatus) {
    LOG(WARNING) << diagnostic.message();
  }
  buildstatus.insert(buildstatus.end(), resolvestatus.begin(),
                     resolvestatus.end());
  files_dirty_ = false;
  return buildstatus;
}

bool SymbolTableHandler::LoadProjectFileList(absl::string_view current_dir) {
  LOG(INFO) << __FUNCTION__;
  if (!curr_project_) return false;
  if (filelist_path_.empty()) {
    // search for FileList file up the directory hierarchy
    std::string projectpath = FindFileList(current_dir);
    if (projectpath.empty()) {
      filelist_path_ = "";
      last_filelist_update_ = {};
      return false;
    }
    LOG(INFO) << "Found file list under " << projectpath;
    filelist_path_ = projectpath;
  }
  if (!last_filelist_update_) {
    last_filelist_update_ = std::filesystem::last_write_time(filelist_path_);
  } else if (*last_filelist_update_ ==
             std::filesystem::last_write_time(filelist_path_)) {
    // filelist file is unchanged, keeping it
    return true;
  }
  LOG(INFO) << "Updating the filelist";
  // fill the FileList object
  FileList filelist;
  if (absl::Status status = AppendFileListFromFile(filelist_path_, &filelist);
      !status.ok()) {
    // if failed to parse
    LOG(WARNING) << "Failed to parse file list in " << filelist_path_ << ":  "
                 << status;
    filelist_path_ = "";
    last_filelist_update_ = {};
    return false;
  }
  // add directory containing filelist to includes
  // TODO (glatosinski): should we do this?
  const absl::string_view filelist_dir = verible::file::Dirname(filelist_path_);
  curr_project_->AddIncludePath(filelist_dir);
  LOG(INFO) << "Adding \"" << filelist_dir << "\" to include directories";
  // update include directories in project
  for (const auto &incdir : filelist.preprocessing.include_dirs) {
    LOG(INFO) << "Adding include path:  " << incdir;
    curr_project_->AddIncludePath(incdir);
  }
  // add files from file list to the project
  for (const auto &file_in_project : filelist.file_paths) {
    const std::string canonicalized =
        std::filesystem::path(file_in_project).lexically_normal().string();
    auto source = curr_project_->OpenTranslationUnit(canonicalized);
    if (!source.ok()) source = curr_project_->OpenIncludedFile(canonicalized);
    if (!source.ok()) {
      LOG(WARNING) << "File included in " << filelist_path_
                   << " not found:  " << canonicalized << ":  "
                   << source.status();
      continue;
    }
    LOG(INFO) << "Creating symbol table for:  " << canonicalized;
    BuildSymbolTableFor(*source.value());
  }
  return true;
}

bool IsStringViewContained(absl::string_view origin, absl::string_view substr) {
  const int from = std::distance(origin.begin(), substr.begin());
  const int to = std::distance(origin.begin(), substr.end());
  if (from < 0) return false;
  if (to > static_cast<int>(origin.length())) return false;
  return true;
}

const SymbolTableNode *ScanReferenceComponents(
    const ReferenceComponentNode *ref, absl::string_view symbol) {
  if (IsStringViewContained(ref->Value().identifier, symbol))
    return ref->Value().resolved_symbol;
  for (const auto &childref : ref->Children()) {
    const SymbolTableNode *resolved =
        ScanReferenceComponents(&childref, symbol);
    if (resolved) return resolved;
  }
  return nullptr;
}

const SymbolTableNode *SymbolTableHandler::ScanSymbolTreeForDefinition(
    const SymbolTableNode *context, absl::string_view symbol) {
  if (!context) {
    return nullptr;
  }
  // TODO (glatosinski): reduce searched scope by utilizing information from
  // syntax tree?
  for (const auto &ref : context->Value().local_references_to_bind) {
    if (ref.Empty()) continue;
    const SymbolTableNode *resolved =
        ScanReferenceComponents(ref.components.get(), symbol);
    if (resolved) return resolved;
  }
  for (const auto &child : context->Children()) {
    auto res = ScanSymbolTreeForDefinition(&child.second, symbol);
    if (res) {
      return res;
    }
  }
  return nullptr;
}

std::vector<verible::lsp::Location> SymbolTableHandler::FindDefinitionLocation(
    const verible::lsp::DefinitionParams &params,
    const verilog::BufferTrackerContainer &parsed_buffers) {
  const absl::Time finddefinition_start = absl::Now();
  LoadProjectFileList(curr_project_->TranslationUnitRoot());
  if (files_dirty_) {
    std::vector<absl::Status> diagnostics = BuildProjectSymbolTable();
    bool success = true;
    for (const auto &diagnostic : diagnostics) {
      if (!diagnostic.ok())
        LOG(WARNING) << "Error on textDocument/definition:  " << diagnostic;
      success &= diagnostic.ok();
    }
    if (!success) {
      LOG(WARNING) << "There were errors during symbol table building";
    }
  }
  absl::string_view filepath = LSPUriToPath(params.textDocument.uri);
  if (filepath.empty()) {
    LOG(ERROR) << "Could not convert URI " << params.textDocument.uri
               << " to filesystem path." << std::endl;
    LOG(INFO) << "textDocument/definition processing time:  "
              << (absl::Now() - finddefinition_start);
    return {};
  }
  std::string relativepath = curr_project_->GetRelativePathToSource(filepath);
  const verilog::BufferTracker *tracker =
      parsed_buffers.FindBufferTrackerOrNull(params.textDocument.uri);
  if (!tracker) {
    LOG(ERROR) << "Could not find buffer with URI " << params.textDocument.uri;
    LOG(INFO) << "textDocument/definition processing time:  "
              << (absl::Now() - finddefinition_start);
    return {};
  }
  const verilog::ParsedBuffer *parsedbuffer = tracker->current();
  if (!parsedbuffer) {
    LOG(ERROR) << "Buffer not found among opened buffers:  "
               << params.textDocument.uri;
    LOG(INFO) << "textDocument/definition processing time:  "
              << (absl::Now() - finddefinition_start);
    return {};
  }
  const verible::LineColumn cursor{params.position.line,
                                   params.position.character};
  const verible::TextStructureView &text = parsedbuffer->parser().Data();

  const verible::TokenInfo cursor_token = text.FindTokenAt(cursor);
  auto symbol = cursor_token.text();
  LOG(INFO) << "Looking for symbol:  " << symbol;
  auto reffile = curr_project_->LookupRegisteredFile(relativepath);
  if (!reffile) {
    LOG(ERROR) << "Unable to lookup " << params.textDocument.uri;
    LOG(INFO) << "textDocument/definition processing time:  "
              << (absl::Now() - finddefinition_start);
    return {};
  }

  auto &root = symbol_table_->Root();

  auto node = ScanSymbolTreeForDefinition(&root, symbol);
  if (!node) {
    LOG(INFO) << "Symbol " << symbol << " not found in symbol table:  " << node;
    LOG(INFO) << "textDocument/definition processing time:  "
              << (absl::Now() - finddefinition_start);
    return {};
  }
  // TODO add iterating over multiple definitions?
  verible::lsp::Location location;
  const verilog::SymbolInfo &symbolinfo = node->Value();
  if (!symbolinfo.file_origin) {
    LOG(ERROR) << "Origin file not available";
    LOG(INFO) << "textDocument/definition processing time:  "
              << (absl::Now() - finddefinition_start);
    return {};
  }
  location.uri = PathToLSPUri(symbolinfo.file_origin->ResolvedPath());
  auto *textstructure = symbolinfo.file_origin->GetTextStructure();
  if (!textstructure) {
    LOG(ERROR) << "Origin file's text structure is not parsed";
    LOG(INFO) << "textDocument/definition processing time:  "
              << (absl::Now() - finddefinition_start);
    return {};
  }
  verible::LineColumnRange symbollocation =
      textstructure->GetRangeForText(*node->Key());
  location.range.start = {.line = symbollocation.start.line,
                          .character = symbollocation.start.column};
  location.range.end = {.line = symbollocation.end.line,
                        .character = symbollocation.end.column};
  LOG(INFO) << "textDocument/definition processing time:  "
            << (absl::Now() - finddefinition_start);
  return {location};
}

const verible::Symbol *SymbolTableHandler::FindDefinitionSymbol(
    absl::string_view symbol) {
  if (files_dirty_) {
    BuildProjectSymbolTable();
  }
  auto symbol_table_node =
      ScanSymbolTreeForDefinition(&symbol_table_->Root(), symbol);
  if (symbol_table_node) return symbol_table_node->Value().syntax_origin;
  return nullptr;
}

void SymbolTableHandler::UpdateFileContent(
    absl::string_view path, const verible::TextStructureView *content) {
  files_dirty_ = true;
  curr_project_->UpdateFileContents(path, content);
}

};  // namespace verilog
