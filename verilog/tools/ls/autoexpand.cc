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
#include "verilog/CST/identifier.h"
#include "verilog/CST/module.h"
#include "verilog/CST/port.h"
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
  // Information about an AUTO comment
  struct AutoComment {
    LineColumn linecol;          // Line and column where the comment starts
    int start_offset;            // Byte offset of the comment in the buffer
    absl::string_view contents;  // The contents of the comment
  };

  AutoExpander(const TextStructureView &text_structure)
      : text_structure_(text_structure), context_(text_structure.Contents()) {
    // Get the indentation from the format style
    FormatStyle format_style;
    InitializeFromFlags(&format_style);
    line_end_str_ = "\n" + std::string(format_style.indentation_spaces, ' ');
  }

  // Write given port names to the output stream, under the specified header
  // comment
  void EmitPortDeclarations(absl::string_view header,
                            const std::vector<absl::string_view> &port_names,
                            std::ostream &output);

  // Find the location and span of an AUTO comment given
  absl::optional<AutoComment> FindInSpan(const std::regex &re,
                                         absl::string_view span);

  // Retrieves port names from a module declared before the given offset
  std::set<absl::string_view> GetPortsDeclaredBefore(const Symbol &module,
                                                     int offset);

  // Expand AUTOARG for the given module (skip given port set)
  std::string ExpandAutoarg(
      const Symbol &module,
      const std::set<absl::string_view> &predeclared_ports);

  // Expand all AUTOs in the buffer
  std::vector<TextEdit> Expand();

 private:
  // Text structure of the buffer to expand AUTOs in
  const TextStructureView &text_structure_;

  // Token context created from the buffer contents
  const TokenInfo::Context context_;

  // String to add at the end of each generated line
  std::string line_end_str_;

  // Regex for finding AUTOARG comments
  static std::regex autoarg_re_;
};

std::regex AutoExpander::autoarg_re_{R"(/\*\s*AUTOARG\s*\*/)"};

void AutoExpander::EmitPortDeclarations(
    const absl::string_view header,
    const std::vector<absl::string_view> &port_names, std::ostream &output) {
  if (port_names.empty()) return;
  output << line_end_str_ << "// " << header << line_end_str_;
  bool first = true;
  for (auto port : port_names) {
    if (!first) {
      output << ',' << line_end_str_;
    }
    first = false;
    output << port;
  }
}

absl::optional<AutoExpander::AutoComment> AutoExpander::FindInSpan(
    const std::regex &re, const absl::string_view span) {
  std::match_results<const char *> match;
  if (!std::regex_search(span.begin(), span.end(), match, re)) return {};
  const int start_offset =
      std::distance(context_.base.begin(), span.begin() + match.position());
  return {{.linecol = text_structure_.GetLineColAtOffset(start_offset),
           .start_offset = start_offset,
           .contents = {span.begin() + match.position(),
                        static_cast<size_t>(match.length())}}};
}

std::set<absl::string_view> AutoExpander::GetPortsDeclaredBefore(
    const Symbol &module, const int offset) {
  std::set<absl::string_view> ports_before;
  const auto all_ports = GetModulePortDeclarationList(module);
  if (!all_ports) return {};
  for (const SymbolPtr &port : all_ports->children()) {
    if (port->Kind() == SymbolKind::kLeaf) continue;
    // Find the identifier of the port
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
    // If there is no identifier, continue
    if (!port_id_node) continue;
    // Check if this port is declared before `offset`
    const absl::string_view port_id = port_id_node->get().text();
    const int port_offset =
        std::distance(context_.base.begin(), port_id.begin());
    if (port_offset < offset) {
      // If so, add it to the set
      ports_before.insert(port_id);
    }
  }
  return ports_before;
}

std::string AutoExpander::ExpandAutoarg(
    const Symbol &module,
    const std::set<absl::string_view> &predeclared_ports) {
  std::vector<absl::string_view> input_port_names;
  std::vector<absl::string_view> inout_port_names;
  std::vector<absl::string_view> output_port_names;
  const auto ports = FindAllModulePortDeclarations(module);
  for (const auto &port : ports) {
    // Get the direction and identifier leaves
    const SyntaxTreeLeaf *const dir_leaf =
        GetDirectionFromModulePortDeclaration(*port.match);
    const SyntaxTreeLeaf *const id_leaf =
        GetIdentifierFromModulePortDeclaration(*port.match);
    // Get the direction and identifier string views
    const absl::string_view dir = dir_leaf->get().text();
    const absl::string_view name = id_leaf->get().text();
    // If declared before AUTOARG, do not print it
    if (predeclared_ports.find(name) != predeclared_ports.end()) continue;
    // Else put in the right container
    if (dir == "input") {
      input_port_names.push_back(name);
    } else if (dir == "inout") {
      inout_port_names.push_back(name);
    } else if (dir == "output") {
      output_port_names.push_back(name);
    }
  }
  // Emit the port names under the correct heading
  std::ostringstream output;
  EmitPortDeclarations("Inputs", input_port_names, output);
  if (output.tellp() != 0 && !inout_port_names.empty()) {
    output << ',';
  }
  EmitPortDeclarations("Inouts", inout_port_names, output);
  if (output.tellp() != 0 && !output_port_names.empty()) {
    output << ',';
  }
  EmitPortDeclarations("Outputs", output_port_names, output);
  if (output.tellp() != 0) {
    output << line_end_str_;
  }
  return output.str();
}

std::vector<TextEdit> AutoExpander::Expand() {
  std::vector<TextEdit> edits;
  if (!text_structure_.SyntaxTree()) return {};
  const auto modules = FindAllModuleDeclarations(*text_structure_.SyntaxTree());
  for (const auto &module : modules) {
    // Find the port list paren group
    const SyntaxTreeNode *const port_parens =
        GetModulePortParenGroup(*module.match);
    if (!port_parens) continue;
    // Find the port list span
    const absl::string_view port_paren_span = StringSpanOfSymbol(*port_parens);
    // Find the AUTOARG comment
    const std::optional<AutoComment> comment =
        FindInSpan(autoarg_re_, port_paren_span);
    if (!comment.has_value()) continue;
    // Find the range which should be replaced by the generated port list
    const LineColumn start_linecol = comment.value().linecol;
    const LineColumn end_linecol =
        text_structure_.GetRangeForText(port_paren_span).end;
    // Find the ports declared before the comment
    const auto predeclared_ports =
        GetPortsDeclaredBefore(*module.match, comment.value().start_offset);
    // Generate the code to replace the port list with
    const std::string new_text =
        absl::StrCat(comment.value().contents,
                     ExpandAutoarg(*module.match, predeclared_ports));
    edits.push_back(
        TextEdit{.range =
                     {
                         .start = {.line = start_linecol.line,
                                   .character = start_linecol.column},
                         .end = {.line = end_linecol.line,
                                 .character = end_linecol.column - 1},
                     },
                 .newText = new_text});
  }
  return edits;
}

}  // namespace

std::vector<TextEdit> GenerateAutoExpandTextEdits(
    const BufferTracker *const tracker) {
  if (!tracker) return {};
  const ParsedBuffer *const current = tracker->current();
  if (!current) return {};  // Can only expand if we have latest version
  AutoExpander expander(current->parser().Data());
  return expander.Expand();
}

std::vector<CodeAction> GenerateAutoExpandCodeActions(
    const BufferTracker *const tracker, const CodeActionParams &p) {
  auto edits = GenerateAutoExpandTextEdits(tracker);
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
  auto it = std::remove_if(edits.begin(), edits.end(), [&](TextEdit &edit) {
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
