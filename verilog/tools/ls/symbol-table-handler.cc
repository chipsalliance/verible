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
#include <map>
#include <memory>
#include <optional>
#include <string>
#include <vector>

#include "absl/container/flat_hash_map.h"
#include "absl/flags/flag.h"
#include "absl/strings/str_format.h"
#include "absl/time/clock.h"
#include "absl/time/time.h"
#include "common/lsp/lsp-file-utils.h"
#include "common/strings/line_column_map.h"
#include "common/util/file_util.h"
#include "common/util/iterator_adaptors.h"
#include "common/util/logging.h"
#include "common/util/range.h"
#include "verilog/analysis/verilog_filelist.h"
#include "verilog/tools/ls/lsp-conversion.h"

ABSL_FLAG(std::string, file_list_path, "verible.filelist",
          "Name of the file with Verible FileList for the project");

using verible::lsp::LSPUriToPath;
using verible::lsp::PathToLSPUri;

namespace verilog {

// If vlog(2), output all non-ok messages, with vlog(1) just the first few,
// else: none
static void LogFullIfVLog(const std::vector<absl::Status> &statuses) {
  if (!VLOG_IS_ON(1)) return;

  constexpr int kMaxEmitNoisyMessagesDirectly = 5;
  int report_count = 0;
  absl::flat_hash_map<std::string, int> status_counts;
  for (const auto &s : statuses) {
    if (s.ok()) continue;
    if (++report_count <= kMaxEmitNoisyMessagesDirectly || VLOG_IS_ON(2)) {
      LOG(INFO) << s;
    } else {
      const std::string partial_msg(s.ToString().substr(0, 25));
      ++status_counts[partial_msg];
    }
  }

  if (!status_counts.empty()) {
    LOG(WARNING) << "skipped remaining; switch VLOG(2) on for all "
                 << statuses.size() << " statuses.";
    LOG(INFO) << "Here a summary";
    std::map<int, absl::string_view> sort_by_count;
    for (const auto &stat : status_counts) {
      sort_by_count.emplace(stat.second, stat.first);
    }
    for (const auto &stat : verible::reversed_view(sort_by_count)) {
      LOG(INFO) << absl::StrFormat("%6d x %s...", stat.first, stat.second);
    }
  }
}

std::string FindFileList(absl::string_view current_dir) {
  // search for FileList file up the directory hierarchy
  std::string projectpath;
  if (auto status = verible::file::UpwardFileSearch(
          current_dir, absl::GetFlag(FLAGS_file_list_path), &projectpath);
      !status.ok()) {
    LOG(INFO) << "Could not find " << absl::GetFlag(FLAGS_file_list_path)
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

void SymbolTableHandler::ParseProjectFiles() {
  if (!curr_project_) return;

  // Parse all files separate from SymbolTable::Build() to report parse duration
  VLOG(1) << "Parsing project files...";
  const absl::Time start = absl::Now();
  std::vector<absl::Status> results;
  for (auto &unit : *curr_project_) {
    VerilogSourceFile *const verilog_file = unit.second.get();
    if (verilog_file->is_parsed()) continue;
    results.emplace_back(verilog_file->Parse());
  }
  LogFullIfVLog(results);

  VLOG(1) << "VerilogSourceFile::Parse() for " << results.size()
          << " files: " << (absl::Now() - start);
}

std::vector<absl::Status> SymbolTableHandler::BuildProjectSymbolTable() {
  if (!curr_project_) {
    return {absl::UnavailableError("VerilogProject is not set")};
  }
  ResetSymbolTable();
  ParseProjectFiles();

  std::vector<absl::Status> buildstatus;
  symbol_table_->Build(&buildstatus);
  symbol_table_->Resolve(&buildstatus);
  LogFullIfVLog(buildstatus);

  files_dirty_ = false;
  return buildstatus;
}

bool SymbolTableHandler::LoadProjectFileList(absl::string_view current_dir) {
  VLOG(1) << __FUNCTION__;
  if (!curr_project_) return false;
  if (filelist_path_.empty()) {
    // search for FileList file up the directory hierarchy
    std::string projectpath = FindFileList(current_dir);
    if (projectpath.empty()) {
      filelist_path_ = "";
      last_filelist_update_ = {};
      return false;
    }
    filelist_path_ = projectpath;
  }
  if (!last_filelist_update_) {
    last_filelist_update_ = std::filesystem::last_write_time(filelist_path_);
  } else if (*last_filelist_update_ ==
             std::filesystem::last_write_time(filelist_path_)) {
    // filelist file is unchanged, keeping it
    return true;
  }
  VLOG(1) << "Updating the filelist";
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
  VLOG(1) << "Adding \"" << filelist_dir << "\" to include directories";
  // update include directories in project
  for (const auto &incdir : filelist.preprocessing.include_dirs) {
    VLOG(1) << "Adding include path:  " << incdir;
    curr_project_->AddIncludePath(incdir);
  }
  // Add files from file list to the project
  VLOG(1) << "Resolving " << filelist.file_paths.size() << " files.";
  int actually_opened = 0;
  const absl::Time start = absl::Now();
  for (const auto &file_in_project : filelist.file_paths) {
    const std::string canonicalized =
        std::filesystem::path(file_in_project).lexically_normal().string();
    absl::StatusOr<VerilogSourceFile *> source =
        curr_project_->OpenTranslationUnit(canonicalized);
    if (!source.ok()) source = curr_project_->OpenIncludedFile(canonicalized);
    if (!source.ok()) {
      VLOG(1) << "File included in " << filelist_path_
              << " not found:  " << canonicalized << ":  " << source.status();
      continue;
    }
    ++actually_opened;
  }

  VLOG(1) << "Successfully opened " << actually_opened
          << " files from file-list: " << (absl::Now() - start);
  return true;
}

const SymbolTableNode *ScanSymbolTreeForDefinitionReferenceComponents(
    const ReferenceComponentNode *ref, absl::string_view symbol) {
  if (verible::IsSubRange(symbol, ref->Value().identifier)) {
    return ref->Value().resolved_symbol;
  }
  for (const auto &childref : ref->Children()) {
    const SymbolTableNode *resolved =
        ScanSymbolTreeForDefinitionReferenceComponents(&childref, symbol);
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
  if (context->Key() && verible::IsSubRange(*context->Key(), symbol)) {
    return context;
  }
  for (const auto &sdef : context->Value().supplement_definitions) {
    if (verible::IsSubRange(sdef, symbol)) {
      return context;
    }
  }
  for (const auto &ref : context->Value().local_references_to_bind) {
    if (ref.Empty()) continue;
    const SymbolTableNode *resolved =
        ScanSymbolTreeForDefinitionReferenceComponents(ref.components.get(),
                                                       symbol);
    if (resolved) return resolved;
  }
  for (const auto &child : context->Children()) {
    const SymbolTableNode *res =
        ScanSymbolTreeForDefinition(&child.second, symbol);
    if (res) {
      return res;
    }
  }
  return nullptr;
}

void SymbolTableHandler::Prepare() {
  LoadProjectFileList(curr_project_->TranslationUnitRoot());
  if (files_dirty_) {
    BuildProjectSymbolTable();
  }
}

absl::string_view SymbolTableHandler::GetTokenAtTextDocumentPosition(
    const verible::lsp::TextDocumentPositionParams &params,
    const verilog::BufferTrackerContainer &parsed_buffers) {
  const verilog::BufferTracker *tracker =
      parsed_buffers.FindBufferTrackerOrNull(params.textDocument.uri);
  if (!tracker) {
    VLOG(1) << "Could not find buffer with URI " << params.textDocument.uri;
    return {};
  }
  std::shared_ptr<const ParsedBuffer> parsedbuffer = tracker->current();
  if (!parsedbuffer) {
    VLOG(1) << "Buffer not found among opened buffers:  "
            << params.textDocument.uri;
    return {};
  }
  const verible::LineColumn cursor{params.position.line,
                                   params.position.character};
  const verible::TextStructureView &text = parsedbuffer->parser().Data();

  const verible::TokenInfo cursor_token = text.FindTokenAt(cursor);
  return cursor_token.text();
}

std::optional<verible::lsp::Location>
SymbolTableHandler::GetLocationFromSymbolName(
    absl::string_view symbol_name, const VerilogSourceFile *file_origin) {
  // TODO (glatosinski) add iterating over multiple definitions
  if (!file_origin && curr_project_) {
    file_origin = curr_project_->LookupFileOrigin(symbol_name);
  }
  if (!file_origin) return std::nullopt;

  verible::lsp::Location location;
  location.uri = PathToLSPUri(file_origin->ResolvedPath());
  const verible::TextStructureView *text_view = file_origin->GetTextStructure();
  if (!text_view->ContainsText(symbol_name)) return std::nullopt;
  location.range = RangeFromLineColumn(text_view->GetRangeForText(symbol_name));

  return location;
}

std::vector<verible::lsp::Location> SymbolTableHandler::FindDefinitionLocation(
    const verible::lsp::DefinitionParams &params,
    const verilog::BufferTrackerContainer &parsed_buffers) {
  // TODO add iterating over multiple definitions
  Prepare();
  const std::string filepath = LSPUriToPath(params.textDocument.uri);
  std::string relativepath = curr_project_->GetRelativePathToSource(filepath);
  absl::string_view symbol =
      GetTokenAtTextDocumentPosition(params, parsed_buffers);
  VLOG(1) << "Looking for symbol:  " << symbol;
  VerilogSourceFile *reffile =
      curr_project_->LookupRegisteredFile(relativepath);
  if (!reffile) {
    VLOG(1) << "Unable to lookup " << params.textDocument.uri;
    return {};
  }

  const SymbolTableNode &root = symbol_table_->Root();

  const SymbolTableNode *node = ScanSymbolTreeForDefinition(&root, symbol);
  // Symbol not found
  if (!node) return {};
  std::vector<verible::lsp::Location> locations;
  const std::optional<verible::lsp::Location> location =
      GetLocationFromSymbolName(*node->Key(), node->Value().file_origin);
  if (!location) return {};
  locations.push_back(*location);
  for (const auto &sdef : node->Value().supplement_definitions) {
    const auto loc = GetLocationFromSymbolName(sdef, node->Value().file_origin);
    if (loc) locations.push_back(*loc);
  }
  return locations;
}

const verible::Symbol *SymbolTableHandler::FindDefinitionSymbol(
    absl::string_view symbol) {
  if (files_dirty_) {
    BuildProjectSymbolTable();
  }
  const SymbolTableNode *symbol_table_node =
      ScanSymbolTreeForDefinition(&symbol_table_->Root(), symbol);
  if (symbol_table_node) return symbol_table_node->Value().syntax_origin;
  return nullptr;
}

std::vector<verible::lsp::Location> SymbolTableHandler::FindReferencesLocations(
    const verible::lsp::ReferenceParams &params,
    const verilog::BufferTrackerContainer &parsed_buffers) {
  Prepare();
  const absl::string_view symbol =
      GetTokenAtTextDocumentPosition(params, parsed_buffers);
  const SymbolTableNode &root = symbol_table_->Root();
  const SymbolTableNode *node = ScanSymbolTreeForDefinition(&root, symbol);
  if (!node) {
    return {};
  }
  std::vector<verible::lsp::Location> locations;
  CollectReferences(&root, node, &locations);
  return locations;
}

void SymbolTableHandler::CollectReferencesReferenceComponents(
    const ReferenceComponentNode *ref, const SymbolTableNode *ref_origin,
    const SymbolTableNode *definition_node,
    std::vector<verible::lsp::Location> *references) {
  if (ref->Value().resolved_symbol == definition_node) {
    const auto loc = GetLocationFromSymbolName(ref->Value().identifier,
                                               ref_origin->Value().file_origin);
    if (loc) references->push_back(*loc);
  }
  for (const auto &childref : ref->Children()) {
    CollectReferencesReferenceComponents(&childref, ref_origin, definition_node,
                                         references);
  }
}

void SymbolTableHandler::CollectReferences(
    const SymbolTableNode *context, const SymbolTableNode *definition_node,
    std::vector<verible::lsp::Location> *references) {
  if (!context) return;
  for (const auto &ref : context->Value().local_references_to_bind) {
    if (ref.Empty()) continue;
    CollectReferencesReferenceComponents(ref.components.get(), context,
                                         definition_node, references);
  }
  for (const auto &child : context->Children()) {
    CollectReferences(&child.second, definition_node, references);
  }
}

void SymbolTableHandler::UpdateFileContent(
    absl::string_view path, const verilog::VerilogAnalyzer *parsed) {
  files_dirty_ = true;
  curr_project_->UpdateFileContents(path, parsed);
}

};  // namespace verilog
