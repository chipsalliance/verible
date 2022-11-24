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

#include "common/strings/line_column_map.h"

namespace verilog {

bool LSPUriToPath(absl::string_view uri, std::string &path) {
  static const std::string fileschemeprefix = "file://";
  auto found = uri.find(fileschemeprefix);
  if (found == std::string::npos) {
    return false;
  }
  if (found != 0) {
    return false;
  }
  std::string res{uri.substr(found + fileschemeprefix.size())};
  path = res;
  return true;
}

void SymbolTableHandler::setProject(
    absl::string_view root, const std::vector<std::string> &include_paths,
    absl::string_view corpus) {
  currproject = std::make_unique<VerilogProject>(root, include_paths, corpus);
  resetSymbolTable();
}

void SymbolTableHandler::resetSymbolTable() {
  checkedfiles.clear();
  symboltable = std::make_unique<SymbolTable>(currproject.get());
}

void SymbolTableHandler::buildSymbolTableFor(VerilogSourceFile &file) {
  auto result = BuildSymbolTable(file, symboltable.get(), currproject.get());
}

std::vector<verible::lsp::Location> SymbolTableHandler::findDefinition(
    const verible::lsp::DefinitionParams &params,
    const verilog::BufferTrackerContainer &parsed_buffers) {
  std::string filepath;
  if (!LSPUriToPath(params.textDocument.uri, filepath)) {
    std::cerr << "Could not convert URI " << params.textDocument.uri
              << " to filesystem path." << std::endl;
    return {};
  }
  auto relativepath = currproject->GetRelativePathToSource(filepath);
  // TODO add checking parsed buffers / raw buffers to get the newest state of
  // files and fallback to reading files from filesystem only when necessary.
  if (checkedfiles.find(relativepath) == checkedfiles.end()) {
    // File hasn't been tracked yet in the symbol table, add it
    auto openedfile = currproject->OpenTranslationUnit(relativepath);
    // TODO check parse status
    auto parsestatus = (*openedfile)->Parse();
    if (!openedfile.ok()) {
      LOG(WARNING) << "Could not open [" << filepath << "] in project ["
                   << currproject->TranslationUnitRoot() << "]" << std::endl;
      return {};
    }
    auto buildstatus =
        BuildSymbolTable(**openedfile, symboltable.get(), currproject.get());
    std::cerr << "Symbol definitions:" << std::endl << std::endl;
    symboltable->PrintSymbolDefinitions(std::cerr);
    std::cerr << std::endl;
  }
  auto parsedbuffer =
      parsed_buffers.FindBufferTrackerOrNull(params.textDocument.uri)
          ->current();
  if (!parsedbuffer) {
    LOG(ERROR) << "Buffer not found among opened buffers:  "
               << params.textDocument.uri << std::endl;
    return {};
  }
  const verible::LineColumn cursor{params.position.line,
                                   params.position.character};
  const verible::TextStructureView &text = parsedbuffer->parser().Data();

  const auto cursor_token = text.FindTokenAt(cursor);
  auto symbol = cursor_token.text();
  LOG(INFO) << "Checking definition for symbol:  " << symbol << std::endl;
  auto reffile = currproject->LookupRegisteredFile(relativepath);
  if (!reffile) {
    LOG(ERROR) << "Unable to lookup " << params.textDocument.uri << std::endl;
    return {};
  }

  auto &root = symboltable->Root();

  auto found = root.Find(symbol);

  if (found == root.end()) {
    LOG(INFO) << "Symbol " << symbol << " not found in symbol table"
              << std::endl;
  }

  // found->

  // auto symbol = absl::StrSplit(reffile->GetContent(), '\n').
  return {};
}

};  // namespace verilog
