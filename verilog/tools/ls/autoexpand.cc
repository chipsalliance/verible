// Copyright 2023 The Verible Authors.
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

#include "verilog/tools/ls/autoexpand.h"

#include <algorithm>
#include <regex>

#include "common/text/text_structure.h"
#include "verilog/CST/declaration.h"
#include "verilog/CST/expression.h"
#include "verilog/CST/identifier.h"
#include "verilog/CST/module.h"
#include "verilog/CST/port.h"
#include "verilog/CST/type.h"
#include "verilog/formatting/format_style_init.h"
#include "verilog/formatting/formatter.h"

namespace verilog {
using verible::LineColumn;
using verible::StringSpanOfSymbol;
using verible::Symbol;
using verible::SymbolKind;
using verible::SymbolPtr;
using verible::SyntaxTreeLeaf;
using verible::SyntaxTreeNode;
using verible::TextStructureView;
using verible::TokenInfo;
using verible::lsp::CodeAction;
using verible::lsp::CodeActionParams;
using verible::lsp::TextEdit;
using verilog::formatter::FormatStyle;
using verilog::formatter::InitializeFromFlags;

namespace {

// Takes a TextStructureView and generates LSP TextEdits for AUTO expansion
class AutoExpander {
 public:
  // Information about a text span
  struct Span {
    LineColumn linecol;          // Line and column where the span starts
    int start_offset;            // Byte offset of the span in the buffer
    absl::string_view contents;  // The contents of the span
  };

  // Simple representation of a port
  struct Port {
    enum { INPUT, INOUT, OUTPUT } direction;  // Direction of the port
    absl::string_view name;                   // Name of the port
  };

  // Module information relevant to AUTO expansion
  class Module {
   public:
    // Writes all port names to the output stream, under the specified heading
    // comment
    void EmitPortDeclarations(
        std::ostream &output, absl::string_view indent,
        absl::string_view header,
        const std::function<bool(const Port &)> &pred) const;

    // Writes port connections to all ports to the output stream, under the
    // specified heading comment
    void EmitPortConnections(
        std::ostream &output, absl::string_view indent,
        absl::string_view header,
        const std::function<bool(const Port &)> &pred) const;

    // Gets ports from the header of the module
    void RetrieveModuleHeaderPorts(const Symbol &module);

    // Gets ports from the body of the module
    void RetrieveModuleBodyPorts(const Symbol &module);

   private:
    // Store the given port in the internal vector
    void PutPort(const SyntaxTreeLeaf *dir_leaf, const SyntaxTreeLeaf *id_leaf);

    // This module's ports
    std::vector<Port> ports_;
  };

  AutoExpander(const TextStructureView &text_structure,
               SymbolTableHandler &symbol_table_handler)
      : text_structure_(text_structure),
        context_(text_structure.Contents()),
        symbol_table_handler(symbol_table_handler) {
    // Get the indentation from the format style
    FormatStyle format_style;
    InitializeFromFlags(&format_style);
    indent_ = std::string(format_style.indentation_spaces, ' ');
  }

  // Finds the location and span of a match of a given regex
  absl::optional<Span> FindInSpan(const std::regex &re,
                                  absl::string_view span) const;

  // Retrieves port names from a module declared before the given offset
  absl::flat_hash_set<absl::string_view> GetPortsListedBefore(
      const Symbol &module, int offset) const;

  // Retrieves port names from a module instance connected before the given
  // offset
  absl::flat_hash_set<absl::string_view> GetPortsConnectedBefore(
      const Symbol &instance, int offset) const;

  // Expands AUTOARG for the given module
  std::optional<TextEdit> ExpandAutoarg(const Symbol &module) const;

  // Expands AUTOINST for the given module instance
  std::optional<TextEdit> ExpandAutoinst(const Symbol &instance,
                                         absl::string_view type_id) const;

  // Expands all AUTOs in the buffer
  std::vector<TextEdit> Expand() const;

 private:
  // Text structure of the buffer to expand AUTOs in
  const TextStructureView &text_structure_;

  // Token context created from the buffer contents
  const TokenInfo::Context context_;

  // Symbol table wrapper for the language server
  SymbolTableHandler &symbol_table_handler;

  // String to add at the end of each generated line
  std::string indent_;

  // Regex for finding AUTOARG comments
  static const std::regex autoarg_re_;

  // Regex for finding AUTOINST comments
  static const std::regex autoinst_re_;
};

const std::regex AutoExpander::autoarg_re_{R"(/\*\s*AUTOARG\s*\*/)"};

const std::regex AutoExpander::autoinst_re_{R"(/\*\s*AUTOINST\s*\*/)"};

void AutoExpander::Module::EmitPortDeclarations(
    std::ostream &output, const absl::string_view indent,
    const absl::string_view header,
    const std::function<bool(const Port &)> &pred) const {
  bool first = true;
  for (const Port &port : ports_) {
    if (!pred(port)) continue;
    if (first) {
      if (output.tellp() != 0) output << ',';
      output << '\n' << indent << "// " << header << '\n' << indent;
      first = false;
    } else {
      output << ", ";
    }
    output << port.name;
  }
}

void AutoExpander::Module::EmitPortConnections(
    std::ostream &output, const absl::string_view indent,
    const absl::string_view header,
    const std::function<bool(const Port &)> &pred) const {
  bool first = true;
  for (const Port &port : ports_) {
    if (!pred(port)) continue;
    if (first) {
      if (output.tellp() != 0) output << ',';
      output << '\n' << indent << "// " << header;
      first = false;
    } else {
      output << ',';
    }
    output << '\n' << indent << '.' << port.name << '(' << port.name << ")";
  }
}

void AutoExpander::Module::RetrieveModuleHeaderPorts(const Symbol &module) {
  const auto module_ports = GetModulePortDeclarationList(module);
  if (module_ports) {
    for (const SymbolPtr &port : module_ports->children()) {
      if (port->Kind() == SymbolKind::kLeaf) continue;
      const SyntaxTreeNode &port_node = SymbolCastToNode(*port);
      const NodeEnum tag = NodeEnum(port_node.Tag().tag);
      if (tag == NodeEnum::kPortDeclaration) {
        const SyntaxTreeLeaf *const dir_leaf =
            GetDirectionFromPortDeclaration(*port);
        const SyntaxTreeLeaf *const id_leaf =
            GetIdentifierFromPortDeclaration(*port);
        PutPort(dir_leaf, id_leaf);
      }
    }
  }
}

void AutoExpander::Module::RetrieveModuleBodyPorts(const Symbol &module) {
  for (const auto &port : FindAllModulePortDeclarations(module)) {
    const SyntaxTreeLeaf *const dir_leaf =
        GetDirectionFromModulePortDeclaration(*port.match);
    const SyntaxTreeLeaf *const id_leaf =
        GetIdentifierFromModulePortDeclaration(*port.match);
    PutPort(dir_leaf, id_leaf);
  }
}

void AutoExpander::Module::PutPort(const SyntaxTreeLeaf *const dir_leaf,
                                   const SyntaxTreeLeaf *const id_leaf) {
  if (!dir_leaf || !id_leaf) return;
  const absl::string_view dir = dir_leaf->get().text();
  const absl::string_view name = id_leaf->get().text();
  if (dir == "input") {
    ports_.push_back({Port::INPUT, name});
  } else if (dir == "inout") {
    ports_.push_back({Port::INOUT, name});
  } else if (dir == "output") {
    ports_.push_back({Port::OUTPUT, name});
  }
}

absl::optional<AutoExpander::Span> AutoExpander::FindInSpan(
    const std::regex &re, const absl::string_view span) const {
  std::cmatch match;
  if (!std::regex_search(span.begin(), span.end(), match, re)) return {};
  const int start_offset =
      std::distance(context_.base.begin(), span.begin() + match.position());
  return {{.linecol = text_structure_.GetLineColAtOffset(start_offset),
           .start_offset = start_offset,
           .contents = {span.begin() + match.position(),
                        static_cast<size_t>(match.length())}}};
}

absl::flat_hash_set<absl::string_view> AutoExpander::GetPortsListedBefore(
    const Symbol &module, const int offset) const {
  absl::flat_hash_set<absl::string_view> ports_before;
  const auto all_ports = GetModulePortDeclarationList(module);
  if (!all_ports) return {};

  for (const SymbolPtr &port : all_ports->children()) {
    if (port->Kind() == SymbolKind::kLeaf) continue;
    const SyntaxTreeNode &port_node = SymbolCastToNode(*port);
    const NodeEnum tag = NodeEnum(port_node.Tag().tag);
    const SyntaxTreeLeaf *port_id_node = nullptr;

    if (tag == NodeEnum::kPortDeclaration) {
      port_id_node = GetIdentifierFromPortDeclaration(port_node);
    } else if (tag == NodeEnum::kPort) {
      const SyntaxTreeNode *const port_ref_node =
          GetPortReferenceFromPort(port_node);
      if (port_ref_node) {
        port_id_node = GetIdentifierFromPortReference(*port_ref_node);
      }
    }
    if (!port_id_node) {
      LOG(WARNING) << "Unhandled type of port declaration or port declaration "
                      "with no identifier. Ignoring";
      continue;
    }

    const absl::string_view port_id = port_id_node->get().text();
    const int port_offset =
        std::distance(context_.base.begin(), port_id.begin());
    if (port_offset < offset) {
      ports_before.insert(port_id);
    }
  }
  return ports_before;
}

absl::flat_hash_set<absl::string_view> AutoExpander::GetPortsConnectedBefore(
    const Symbol &instance, const int offset) const {
  absl::flat_hash_set<absl::string_view> ports_before;
  for (const auto &port : FindAllActualNamedPort(instance)) {
    const SyntaxTreeLeaf *const id_node = GetActualNamedPortName(*port.match);
    if (!id_node) {
      LOG(WARNING) << "Named port connection with no identifier? Ignoring";
      continue;
    }
    const absl::string_view name = id_node->get().text();
    const int port_offset = std::distance(context_.base.begin(), name.begin());
    if (port_offset < offset) {
      ports_before.insert(name);
    }
  }
  return ports_before;
}

std::optional<TextEdit> AutoExpander::ExpandAutoarg(
    const Symbol &module) const {
  const SyntaxTreeNode *const port_parens = GetModulePortParenGroup(module);
  if (!port_parens) return {};  // No port paren group, so no AUTOARG
  const absl::string_view port_paren_span = StringSpanOfSymbol(*port_parens);

  const std::optional<Span> autoarg = FindInSpan(autoarg_re_, port_paren_span);
  if (!autoarg) return {};

  Module module_info;
  module_info.RetrieveModuleBodyPorts(module);

  // Ports listed before the comment should not be redeclared
  const auto predeclared_ports =
      GetPortsListedBefore(module, autoarg->start_offset);

  std::ostringstream new_text;
  module_info.EmitPortDeclarations(
      new_text, indent_, "Inputs", [&](const Port &port) {
        return port.direction == Port::INPUT &&
               !predeclared_ports.contains(port.name);
      });
  module_info.EmitPortDeclarations(
      new_text, indent_, "Inouts", [&](const Port &port) {
        return port.direction == Port::INOUT &&
               !predeclared_ports.contains(port.name);
      });
  module_info.EmitPortDeclarations(
      new_text, indent_, "Outputs", [&](const Port &port) {
        return port.direction == Port::OUTPUT &&
               !predeclared_ports.contains(port.name);
      });
  if (new_text.tellp() != 0) {
    new_text << '\n' << indent_;
  }

  const LineColumn start_linecol = autoarg->linecol;
  const LineColumn end_linecol =
      text_structure_.GetRangeForText(port_paren_span).end;
  return TextEdit{.range =
                      {
                          .start = {.line = start_linecol.line,
                                    .character = start_linecol.column},
                          .end = {.line = end_linecol.line,
                                  .character = end_linecol.column - 1},
                      },
                  .newText = absl::StrCat(autoarg->contents, new_text.str())};
}

std::optional<TextEdit> AutoExpander::ExpandAutoinst(
    const Symbol &instance, absl::string_view type_id) const {
  const SyntaxTreeNode *const parens =
      GetParenGroupFromModuleInstantiation(instance);
  const absl::string_view paren_span = StringSpanOfSymbol(*parens);
  const std::optional<Span> autoinst = FindInSpan(autoinst_re_, paren_span);
  if (!autoinst) return {};
  const Symbol *const type_def =
      symbol_table_handler.FindDefinitionSymbol(type_id);
  if (!type_def) {
    LOG(ERROR) << "AUTOINST: No definition found for module type: " << type_id;
    return {};
  }

  Module module_info;
  module_info.RetrieveModuleHeaderPorts(*type_def);
  module_info.RetrieveModuleBodyPorts(*type_def);

  // Ports connected before the AUTOINST comment should be ignored
  const auto preconnected_ports =
      GetPortsConnectedBefore(instance, autoinst->start_offset);

  std::ostringstream new_text;
  const std::string conn_indent = absl::StrCat(indent_, indent_);
  module_info.EmitPortConnections(
      new_text, conn_indent, "Inputs", [&](const Port &port) {
        return port.direction == Port::INPUT &&
               !preconnected_ports.contains(port.name);
      });
  module_info.EmitPortConnections(
      new_text, conn_indent, "Inouts", [&](const Port &port) {
        return port.direction == Port::INOUT &&
               !preconnected_ports.contains(port.name);
      });
  module_info.EmitPortConnections(
      new_text, conn_indent, "Outputs", [&](const Port &port) {
        return port.direction == Port::OUTPUT &&
               !preconnected_ports.contains(port.name);
      });

  const LineColumn start_linecol = autoinst.value().linecol;
  const LineColumn end_linecol =
      text_structure_.GetRangeForText(paren_span).end;
  return TextEdit{.range =
                      {
                          .start = {.line = start_linecol.line,
                                    .character = start_linecol.column},
                          .end = {.line = end_linecol.line,
                                  .character = end_linecol.column - 1},
                      },
                  .newText = absl::StrCat(autoinst->contents, new_text.str())};
}

std::vector<TextEdit> AutoExpander::Expand() const {
  std::vector<TextEdit> edits;
  if (!text_structure_.SyntaxTree()) {
    LOG(ERROR)
        << "Cannot perform AUTO expansion: failed to retrieve a syntax tree";
    return {};
  }
  const auto modules = FindAllModuleDeclarations(*text_structure_.SyntaxTree());
  for (const auto &module : modules) {
    // AUTOINST
    for (const auto &data : FindAllDataDeclarations(*module.match)) {
      const Symbol *const type_id_node =
          GetTypeIdentifierFromDataDeclaration(*data.match);
      // Some data declarations like 'reg' do not have a type id, ignore those
      if (!type_id_node) continue;
      const absl::string_view type_id = StringSpanOfSymbol(*type_id_node);
      for (const auto &instance : FindAllGateInstances(*data.match)) {
        if (const auto edit = ExpandAutoinst(*instance.match, type_id)) {
          edits.push_back(*edit);
        }
      }
    }
    // AUTOARG
    if (const auto edit = ExpandAutoarg(*module.match)) {
      edits.push_back(*edit);
    }
  }
  return edits;
}

}  // namespace

std::vector<TextEdit> GenerateAutoExpandTextEdits(
    SymbolTableHandler &symbol_table_handler,
    const BufferTracker *const tracker) {
  if (!tracker) return {};
  const ParsedBuffer *const current = tracker->current();
  if (!current) return {};  // Can only expand if we have latest version
  AutoExpander expander(current->parser().Data(), symbol_table_handler);
  return expander.Expand();
}

std::vector<CodeAction> GenerateAutoExpandCodeActions(
    SymbolTableHandler &symbol_table_handler,
    const BufferTracker *const tracker, const CodeActionParams &p) {
  auto edits = GenerateAutoExpandTextEdits(symbol_table_handler, tracker);
  if (edits.empty()) return {};
  std::vector<CodeAction> result;
  // Make a code action for expanding all AUTOs in the current buffer
  result.emplace_back(CodeAction{
      .title = "Expand all AUTOs in file",
      .kind = "refactor.rewrite",
      .diagnostics = {},
      .isPreferred = false,
      .edit = {.changes = {{p.textDocument.uri, edits}}},
  });
  // Remove edits outside of the code action range
  const auto it =
      std::remove_if(edits.begin(), edits.end(), [&](TextEdit &edit) {
        return p.range.end.line < edit.range.start.line ||
               p.range.start.line > edit.range.end.line;
      });
  edits.erase(it, edits.end());
  // If no remaining edits, just return the global action
  if (edits.empty()) return result;
  // Else make a local code action as well
  result.emplace_back(CodeAction{
      .title = edits.size() > 1 ? "Expand all AUTOs in selected range"
                                : "Expand this AUTO",
      .kind = "refactor.rewrite",
      .diagnostics = {},
      .isPreferred = false,
      .edit = {.changes = {{p.textDocument.uri, edits}}},
  });
  return result;
}

}  // namespace verilog
