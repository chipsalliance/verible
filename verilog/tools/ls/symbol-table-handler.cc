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

void SymbolTableHandler::SetProject(
    const std::shared_ptr<VerilogProject> &project) {
  currproject = project;
  ResetSymbolTable();
  LoadProjectFileList(currproject->TranslationUnitRoot());
}

void SymbolTableHandler::ResetSymbolTable() {
  checkedfiles.clear();
  symboltable = std::make_unique<SymbolTable>(currproject.get());
}

void SymbolTableHandler::BuildSymbolTableFor(const VerilogSourceFile &file) {
  auto result = BuildSymbolTable(file, symboltable.get(), currproject.get());
}

void SymbolTableHandler::BuildProjectSymbolTable() {
  ResetSymbolTable();
  if (!currproject) {
    return;
  }
  LOG(INFO) << "Parsing project files...";
  std::vector<absl::Status> buildstatus;
  symboltable->Build(&buildstatus);
  for (const auto &diagnostic : buildstatus) {
    LOG(WARNING) << diagnostic.message();
  }
  std::vector<absl::Status> resolvestatus;
  symboltable->Resolve(&resolvestatus);
  for (const auto &diagnostic : resolvestatus) {
    LOG(WARNING) << diagnostic.message();
  }
  files_dirty_ = false;
}

void SymbolTableHandler::LoadProjectFileList(absl::string_view current_dir) {
  LOG(INFO) << __FUNCTION__;
  if (!currproject) return;
  // search for FileList file up the directory hierarchy
  FileList filelist;
  std::string projectpath;
  if (auto status = verible::file::UpwardFileSearch(
          current_dir, absl::GetFlag(FLAGS_file_list_path), &projectpath);
      !status.ok()) {
    LOG(WARNING) << "Could not find " << absl::GetFlag(FLAGS_file_list_path)
                 << " file in the project:  " << status;
    return;
  }
  LOG(INFO) << "Found file list under " << projectpath;
  // fill the FileList object
  if (absl::Status status = AppendFileListFromFile(projectpath, &filelist);
      !status.ok()) {
    // if failed to parse
    LOG(WARNING) << "Failed to parse file list in " << projectpath << ":  "
                 << status;
    return;
  }
  // update include directories in project
  for (const auto &incdir : filelist.preprocessing.include_dirs) {
    LOG(INFO) << "Adding include path:  " << incdir;
    currproject->AddIncludePath(incdir);
  }
  // add files from file list to the project
  for (auto &incfile : filelist.file_paths) {
    auto incsource = currproject->OpenIncludedFile(incfile);
    if (!incsource.ok()) {
      LOG(WARNING) << "File included in " << projectpath
                   << " not found:  " << incfile << ":  " << incsource.status();
      continue;
    }
    LOG(INFO) << "Creating symbol table for:  " << incfile;
    BuildSymbolTableFor(*incsource.value());
  }
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

std::vector<verible::lsp::Location> SymbolTableHandler::FindDefinition(
    const verible::lsp::DefinitionParams &params,
    const verilog::BufferTrackerContainer &parsed_buffers) {
  const absl::Time finddefinition_start = absl::Now();
  if (files_dirty_) {
    BuildProjectSymbolTable();
  }
  absl::string_view filepath = LSPUriToPath(params.textDocument.uri);
  if (filepath.empty()) {
    LOG(ERROR) << "Could not convert URI " << params.textDocument.uri
               << " to filesystem path." << std::endl;
    LOG(INFO) << "textDocument/definition processing time:  "
              << absl::ToInt64Milliseconds(absl::Now() - finddefinition_start)
              << "ms";
    return {};
  }
  std::string relativepath = currproject->GetRelativePathToSource(filepath);
  const verilog::ParsedBuffer *parsedbuffer =
      parsed_buffers.FindBufferTrackerOrNull(params.textDocument.uri)
          ->current();
  if (!parsedbuffer) {
    LOG(ERROR) << "Buffer not found among opened buffers:  "
               << params.textDocument.uri;
    LOG(INFO) << "textDocument/definition processing time:  "
              << absl::ToInt64Milliseconds(absl::Now() - finddefinition_start)
              << "ms";
    return {};
  }
  const verible::LineColumn cursor{params.position.line,
                                   params.position.character};
  const verible::TextStructureView &text = parsedbuffer->parser().Data();

  const verible::TokenInfo cursor_token = text.FindTokenAt(cursor);
  auto symbol = cursor_token.text();
  auto reffile = currproject->LookupRegisteredFile(relativepath);
  if (!reffile) {
    LOG(ERROR) << "Unable to lookup " << params.textDocument.uri;
    LOG(INFO) << "textDocument/definition processing time:  "
              << absl::ToInt64Milliseconds(absl::Now() - finddefinition_start)
              << "ms";
    return {};
  }

  auto &root = symboltable->Root();

  auto node = ScanSymbolTreeForDefinition(&root, symbol);
  if (!node) {
    LOG(INFO) << "Symbol " << symbol << " not found in symbol table:  " << node;
    LOG(INFO) << "textDocument/definition processing time:  "
              << absl::ToInt64Milliseconds(absl::Now() - finddefinition_start)
              << "ms";
    return {};
  }
  // TODO add iterating over multiple definitions?
  verible::lsp::Location location;
  const verilog::SymbolInfo &symbolinfo = node->Value();
  if (!symbolinfo.file_origin) {
    LOG(ERROR) << "Origin file not available";
    LOG(INFO) << "textDocument/definition processing time:  "
              << absl::ToInt64Milliseconds(absl::Now() - finddefinition_start)
              << "ms";
    return {};
  }
  location.uri = PathToLSPUri(symbolinfo.file_origin->ResolvedPath());
  auto *textstructure = symbolinfo.file_origin->GetTextStructure();
  if (!textstructure) {
    LOG(ERROR) << "Origin file's text structure is not parsed";
    LOG(INFO) << "textDocument/definition processing time:  "
              << absl::ToInt64Milliseconds(absl::Now() - finddefinition_start)
              << "ms";
    return {};
  }
  verible::LineColumnRange symbollocation =
      textstructure->GetRangeForText(*node->Key());
  location.range.start = {.line = symbollocation.start.line,
                          .character = symbollocation.start.column};
  location.range.end = {.line = symbollocation.end.line,
                        .character = symbollocation.end.column};
  LOG(INFO) << "textDocument/definition processing time:  "
            << absl::ToInt64Milliseconds(absl::Now() - finddefinition_start)
            << "ms";
  return {location};
}

absl::Status SymbolTableHandler::UpdateFileContent(
    absl::string_view path, const verible::TextStructureView *content) {
  files_dirty_ = true;
  return currproject->UpdateFileContents(path, content);
}

};  // namespace verilog
