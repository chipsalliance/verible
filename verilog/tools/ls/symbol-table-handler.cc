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

namespace verilog {

void SymbolTableHandler::setProject(
    absl::string_view root, const std::vector<std::string> &include_paths,
    absl::string_view corpus) {
  currproject = std::make_shared<VerilogProject>(root, include_paths, corpus);
  resetSymbolTable();
}

void SymbolTableHandler::resetSymbolTable() {
  checkedfiles.clear();
  symboltable = std::make_shared<SymbolTable>(currproject.get());
}

void SymbolTableHandler::buildSymbolTableFor(VerilogSourceFile &file) {
  auto result = BuildSymbolTable(file, symboltable.get(), currproject.get());
}

std::vector<verible::lsp::Location> SymbolTableHandler::findDefinition(
    const verible::lsp::DefinitionParams &params) {
  auto filepath = params.textDocument.uri;
  if (checkedfiles.find(filepath) == checkedfiles.end()) {
    // File hasn't been tracked yet in the symbol table, add it
    auto openedfile = currproject->OpenTranslationUnit(filepath);
    if (!openedfile.ok()) {
      std::cerr << "Could not open " << filepath << " in project "
                << currproject->TranslationUnitRoot() << std::endl;
      return {};
    }
    auto buildstatus =
        BuildSymbolTable(**openedfile, symboltable.get(), currproject.get());
    std::cerr << "Symbol definitions:" << std::endl << std::endl;
    symboltable->PrintSymbolDefinitions(std::cerr);
    std::cerr << std::endl;
  }
  return {};
}

};  // namespace verilog
