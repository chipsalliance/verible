// Copyright 2024 The Verible Authors.
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

#include "verible/verilog/tools/ls/inlay-hint.h"

#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include "verible/common/analysis/syntax-tree-search.h"
#include "verible/common/lsp/lsp-protocol.h"
#include "verible/common/strings/line-column-map.h"
#include "verible/common/text/concrete-syntax-leaf.h"
#include "verible/common/text/concrete-syntax-tree.h"
#include "verible/common/text/symbol.h"
#include "verible/common/text/text-structure.h"
#include "verible/common/text/token-info.h"
#include "verible/common/text/tree-utils.h"
#include "verible/verilog/CST/declaration.h"
#include "verible/verilog/CST/port.h"
#include "verible/verilog/analysis/symbol-table.h"
#include "verible/verilog/tools/ls/lsp-parse-buffer.h"
#include "verible/verilog/tools/ls/symbol-table-handler.h"

namespace verilog {
namespace {

using verible::Symbol;
using verible::SyntaxTreeLeaf;
using verible::SyntaxTreeNode;

// Information about a port that we want to show in an inlay hint.
struct PortHintInfo {
  std::string_view direction;  // "input", "output", "inout"
  std::string type_string;     // e.g., "[7:0]", "logic [15:0]"
};

// Format a port hint label from direction and type info.
std::string FormatPortHint(const PortHintInfo &info) {
  std::string result;

  if (!info.direction.empty()) {
    result = std::string(info.direction);
  }

  if (!info.type_string.empty()) {
    if (!result.empty()) {
      result += " ";
    }
    result += info.type_string;
  }

  return result;
}

// Extract simplified type string from a syntax origin.
// This tries to extract packed dimensions like "[7:0]" from the type.
std::string ExtractTypeString(const Symbol *syntax_origin) {
  if (!syntax_origin) return "";
  std::string_view full_text = verible::StringSpanOfSymbol(*syntax_origin);
  // Return the full type text for now; could be refined to extract just
  // dimensions.
  return std::string(full_text);
}

// Look up port information from a module's symbol table node.
std::optional<PortHintInfo> GetPortInfoFromModule(
    const SymbolTableNode *module_node, std::string_view port_name) {
  if (!module_node) return std::nullopt;

  // The module's children contain its ports and other declarations.
  for (const auto &child : module_node->Children()) {
    if (child.first == port_name) {
      const SymbolInfo &port_info = child.second.Value();
      // Check if this is actually a port
      if (port_info.metatype == SymbolMetaType::kDataNetVariableInstance) {
        PortHintInfo hint;
        hint.direction = port_info.declared_type.direction;
        hint.type_string =
            ExtractTypeString(port_info.declared_type.syntax_origin);
        return hint;
      }
    }
  }

  return std::nullopt;
}

// Check if a position is within the requested range.
bool IsInRange(const verible::LineColumn &pos,
               const verible::lsp::Range &range) {
  if (pos.line < range.start.line || pos.line > range.end.line) {
    return false;
  }
  if (pos.line == range.start.line && pos.column < range.start.character) {
    return false;
  }
  if (pos.line == range.end.line && pos.column > range.end.character) {
    return false;
  }
  return true;
}

// Main class for building inlay hints.
class InlayHintBuilder {
 public:
  InlayHintBuilder(SymbolTableHandler *symbol_table_handler,
                   const BufferTrackerContainer &tracker_container,
                   const verible::lsp::InlayHintParams &params)
      : symbol_table_handler_(symbol_table_handler),
        tracker_container_(tracker_container),
        params_(params) {}

  std::vector<verible::lsp::InlayHint> Build() {
    const BufferTracker *tracker =
        tracker_container_.FindBufferTrackerOrNull(params_.textDocument.uri);
    if (!tracker) return {};

    std::shared_ptr<const ParsedBuffer> parsed_buffer = tracker->current();
    if (!parsed_buffer) return {};

    text_structure_ = &parsed_buffer->parser().Data();
    const verible::ConcreteSyntaxTree &tree =
        parsed_buffer->parser().SyntaxTree();
    if (!tree) return {};

    // Find all module instantiations and process them.
    ProcessTree(*tree);

    return std::move(hints_);
  }

 private:
  void ProcessTree(const Symbol &tree) {
    // Find all data declarations (which include module instantiations).
    std::vector<verible::TreeSearchMatch> data_decls =
        FindAllDataDeclarations(tree);

    for (const auto &match : data_decls) {
      ProcessDataDeclaration(*match.match);
    }
  }

  void ProcessDataDeclaration(const Symbol &data_decl) {
    // Get the module type name from the instantiation.
    const Symbol *type_id = GetTypeIdentifierFromDataDeclaration(data_decl);
    if (!type_id) return;

    std::string_view module_name = verible::StringSpanOfSymbol(*type_id);
    if (module_name.empty()) return;

    // Look up the module definition in the symbol table.
    const SymbolTableNode *module_node =
        symbol_table_handler_->FindDefinitionNode(module_name);
    if (!module_node) return;

    // Verify it's actually a module or interface.
    if (module_node->Value().metatype != SymbolMetaType::kModule &&
        module_node->Value().metatype != SymbolMetaType::kInterface) {
      return;
    }

    // Find all gate instances within this declaration.
    std::vector<verible::TreeSearchMatch> gate_instances =
        FindAllGateInstances(data_decl);

    for (const auto &inst_match : gate_instances) {
      ProcessGateInstance(*inst_match.match, module_node);
    }
  }

  void ProcessGateInstance(const Symbol &gate_instance,
                           const SymbolTableNode *module_node) {
    // Get the paren group containing port connections.
    const SyntaxTreeNode *paren_group =
        GetParenGroupFromModuleInstantiation(gate_instance);
    if (!paren_group) return;

    // Find all named port connections within the paren group.
    std::vector<verible::TreeSearchMatch> named_ports =
        FindAllActualNamedPort(*paren_group);

    for (const auto &port_match : named_ports) {
      ProcessNamedPort(*port_match.match, module_node);
    }
  }

  void ProcessNamedPort(const Symbol &named_port,
                        const SymbolTableNode *module_node) {
    // Get the port name from the connection.
    const SyntaxTreeLeaf *port_name_leaf = GetActualNamedPortName(named_port);
    if (!port_name_leaf) return;

    const verible::TokenInfo &token = port_name_leaf->get();
    std::string_view port_name = token.text();

    // Calculate the position for the inlay hint.
    // Position it after the port name (before the opening parenthesis).
    std::string_view base_text = text_structure_->Contents();
    int offset = token.right(base_text);
    verible::LineColumn line_col = text_structure_->GetLineColAtOffset(offset);

    // Check if the position is within the requested range.
    if (!IsInRange(line_col, params_.range)) return;

    // Look up port information from the module.
    std::optional<PortHintInfo> port_info =
        GetPortInfoFromModule(module_node, port_name);
    if (!port_info) return;

    // Format the hint label.
    std::string label = FormatPortHint(*port_info);
    if (label.empty()) return;

    // Create the inlay hint.
    verible::lsp::InlayHint hint;
    hint.position.line = line_col.line;
    hint.position.character = line_col.column;
    hint.label = ": " + label;
    hint.kind = 2;  // InlayHintKind::kParameter
    hint.paddingLeft = false;
    hint.paddingRight = true;

    hints_.push_back(std::move(hint));
  }

  SymbolTableHandler *symbol_table_handler_;
  const BufferTrackerContainer &tracker_container_;
  const verible::lsp::InlayHintParams &params_;
  const verible::TextStructureView *text_structure_ = nullptr;
  std::vector<verible::lsp::InlayHint> hints_;
};

}  // namespace

std::vector<verible::lsp::InlayHint> GenerateInlayHints(
    SymbolTableHandler *symbol_table_handler,
    const BufferTrackerContainer &tracker,
    const verible::lsp::InlayHintParams &params) {
  InlayHintBuilder builder(symbol_table_handler, tracker, params);
  return builder.Build();
}

}  // namespace verilog
