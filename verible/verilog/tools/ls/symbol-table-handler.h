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

#ifndef VERILOG_TOOLS_LS_SYMBOL_TABLE_HANDLER_H
#define VERILOG_TOOLS_LS_SYMBOL_TABLE_HANDLER_H

#include <filesystem>
#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

#include "absl/status/status.h"
#include "absl/types/optional.h"
#include "verible/common/lsp/lsp-protocol.h"
#include "verible/common/strings/line-column-map.h"
#include "verible/common/text/symbol.h"
#include "verible/common/text/token-info.h"
#include "verible/verilog/analysis/symbol-table.h"
#include "verible/verilog/analysis/verilog-analyzer.h"
#include "verible/verilog/analysis/verilog-project.h"
#include "verible/verilog/tools/ls/lsp-parse-buffer.h"

namespace verilog {

// Looks for FileList file for SymbolTableHandler
std::string FindFileList(std::string_view current_dir);

// A class interfacing the SymbolTable with the LSP messages.
// It manages the SymbolTable and its necessary components,
// and provides such information as symbol definitions
// based on LSP requests.
// The provided information is in LSP-friendly format.
class SymbolTableHandler {
 public:
  SymbolTableHandler() = default;

  // Sets the project for the symbol table.
  // VerilogProject requires root, include_paths and corpus to
  // create a base of files that may contain definitions for symbols.
  // Once the project's root is set, a new SymbolTable is created.
  void SetProject(const std::shared_ptr<VerilogProject> &project);

  // Finds the definition for a symbol provided in the DefinitionParams
  // message delivered i.e. in textDocument/definition message.
  // Provides a list of locations with symbol's definitions.
  std::vector<verible::lsp::Location> FindDefinitionLocation(
      const verible::lsp::TextDocumentPositionParams &params,
      const verilog::BufferTrackerContainer &parsed_buffers);

  // Finds the node of the symbol table with definition for a given symbol.
  const SymbolTableNode *FindDefinitionNode(std::string_view symbol);

  // Finds the symbol of the definition for the given identifier.
  const verible::Symbol *FindDefinitionSymbol(std::string_view symbol);

  // Finds references of a symbol provided in the ReferenceParams
  // message delivered in textDocument/references message.
  // Provides a list of references' locations
  std::vector<verible::lsp::Location> FindReferencesLocations(
      const verible::lsp::ReferenceParams &params,
      const verilog::BufferTrackerContainer &parsed_buffers);

  std::optional<verible::lsp::Range> FindRenameableRangeAtCursor(
      const verible::lsp::PrepareRenameParams &params,
      const verilog::BufferTrackerContainer &parsed_buffers);

  verible::lsp::WorkspaceEdit FindRenameLocationsAndCreateEdits(
      const verible::lsp::RenameParams &params,
      const verilog::BufferTrackerContainer &parsed_buffers);

  // Returns TokenInfo for token pointed by the LSP request based on
  // TextDocumentPositionParams. If text is not found, nullopt is returned.
  std::optional<verible::TokenInfo> GetTokenAtTextDocumentPosition(
      const verible::lsp::TextDocumentPositionParams &params,
      const verilog::BufferTrackerContainer &parsed_buffers) const;

  // Creates a symbol table for entire project (public: needed in unit-test)
  std::vector<absl::Status> BuildProjectSymbolTable();

  // Provide new parsed content for the given path. If "content" is nullptr,
  // opens the given file instead.
  void UpdateFileContent(std::string_view path,
                         const verilog::VerilogAnalyzer *parsed);

  // Create a listener to be wired up to a buffer tracker. Whenever we
  // there is a change in the editor, this will update our internal project.
  BufferTrackerContainer::ChangeCallback CreateBufferTrackerListener();

 private:
  // prepares structures for symbol-based requests
  void Prepare();

  // Creates a new symbol table given the VerilogProject in setProject
  // method.
  void ResetSymbolTable();

  // Returns a range in which a token exists in the file by the LSP request
  // based on TextDocumentPositionParams. If text is not found,
  // empty-initialized LineColumnRange is returned.
  verible::LineColumnRange GetTokenRangeAtTextDocumentPosition(
      const verible::lsp::TextDocumentPositionParams &document_cursor,
      const verilog::BufferTrackerContainer &parsed_buffers);

  // Returns a TokenInfo of a token pointed at by the cursor in the file by the
  // LSP request based on TextDocumentPositionParams. If text is not found,
  // nullptr is returned.
  std::optional<verible::TokenInfo> GetTokenInfoAtTextDocumentPosition(
      const verible::lsp::TextDocumentPositionParams &params,
      const verilog::BufferTrackerContainer &parsed_buffers);

  // Returns the Location of the symbol name in source file
  // pointed by the file_origin.
  // If given symbol name is not found, std::nullopt is returned.
  std::optional<verible::lsp::Location> GetLocationFromSymbolName(
      std::string_view symbol_name, const VerilogSourceFile *file_origin);

  // Scans the symbol table tree to find a given symbol.
  // returns pointer to table node with the symbol on success, else nullptr.
  const SymbolTableNode *ScanSymbolTreeForDefinition(
      const SymbolTableNode *context, std::string_view symbol);

  // Internal function for CollectReferences that iterates over
  // ReferenceComponentNodes
  void CollectReferencesReferenceComponents(
      const ReferenceComponentNode *ref, const SymbolTableNode *ref_origin,
      const SymbolTableNode *definition_node,
      std::vector<verible::lsp::Location> *references);

  // Collects all references of a given symbol in the references
  // vector.
  void CollectReferences(const SymbolTableNode *context,
                         const SymbolTableNode *definition_node,
                         std::vector<verible::lsp::Location> *references);

  // Looks for verible.filelist file down in directory structure and loads
  // data to project. It is meant to be executed once per VerilogProject setup
  bool LoadProjectFileList(std::string_view current_dir);

  // Parse all the files in the project.
  void ParseProjectFiles();

  // Path to the filelist file for the project
  std::string filelist_path_;

  // Last timestamp of filelist file - used to check whether SymbolTable
  // should be updated
  absl::optional<std::filesystem::file_time_type> last_filelist_update_;

  // tells that symbol table should be rebuilt due to changes in files
  bool files_dirty_ = true;

  // current VerilogProject for which the symbol table is created
  std::shared_ptr<VerilogProject> curr_project_;
  std::unique_ptr<SymbolTable> symbol_table_;
};

};  // namespace verilog

#endif  // VERILOG_TOOLS_LS_SYMBOL_TABLE_HANDLER_H
