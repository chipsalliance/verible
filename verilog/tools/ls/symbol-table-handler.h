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

#include <memory>
#include <unordered_set>
#include <vector>

#include "absl/status/statusor.h"
#include "absl/strings/string_view.h"
#include "common/lsp/lsp-protocol.h"
#include "verilog/analysis/symbol_table.h"
#include "verilog/analysis/verilog_project.h"

namespace verilog {

// A class interfacing the SymbolTable with the LSP messages.
// It manages the SymbolTable and its necessary components,
// and provides such information as symbol definitions
// based on LSP requests.
// The provided information is in LSP-friendly format.
class SymbolTableHandler {
 public:
  SymbolTableHandler(){};

  // Sets the project for the symbol table.
  // VerilogProject requires root, include_paths and corpus to
  // create a base of files that may contain definitions for symbols.
  // Once the project's root is set, a new SymbolTable is created.
  void setProject(absl::string_view root,
                  const std::vector<std::string> &include_paths,
                  absl::string_view corpus);

  // Creates a new symbol table given the VerilogProject in setProject
  // method.
  void resetSymbolTable();

  // Fills the symbol table for a given verilog source file.
  void buildSymbolTableFor(VerilogSourceFile &file);

  // Finds the definition for a symbol provided in the DefinitionParams
  // message delivered i.e. in textDocument/definition message.
  // Provides a list of locations with symbol's definitions.
  std::vector<verible::lsp::Location> findDefinition(
      const verible::lsp::DefinitionParams &params);

 private:
  std::shared_ptr<VerilogProject> currproject =
      nullptr;  // current VerilogProject for which the symbol table is created
  std::shared_ptr<SymbolTable> symboltable = nullptr;  // symbol table structure
  std::unordered_set<std::string>
      checkedfiles;  // set of checked files to prevent unnecessary calls for
                     // creating a symbol table for already seen files
};

};  // namespace verilog

#endif  // VERILOG_TOOLS_LS_SYMBOL_TABLE_HANDLER_H
