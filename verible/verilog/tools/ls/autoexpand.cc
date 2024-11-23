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

#include "verible/verilog/tools/ls/autoexpand.h"

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <functional>
#include <iterator>
#include <memory>
#include <optional>
#include <ostream>
#include <sstream>
#include <string>
#include <utility>
#include <variant>
#include <vector>

#include "absl/container/flat_hash_map.h"
#include "absl/container/flat_hash_set.h"
#include "absl/container/node_hash_map.h"
#include "absl/status/status.h"
#include "absl/strings/numbers.h"
#include "absl/strings/str_cat.h"
#include "absl/strings/string_view.h"
#include "re2/re2.h"
#include "verible/common/analysis/syntax-tree-search.h"
#include "verible/common/lsp/lsp-protocol.h"
#include "verible/common/strings/line-column-map.h"
#include "verible/common/strings/position.h"
#include "verible/common/text/symbol.h"
#include "verible/common/text/text-structure.h"
#include "verible/common/text/token-info.h"
#include "verible/common/text/tree-utils.h"
#include "verible/common/util/logging.h"
#include "verible/verilog/CST/declaration.h"
#include "verible/verilog/CST/dimensions.h"
#include "verible/verilog/CST/module.h"
#include "verible/verilog/CST/net.h"
#include "verible/verilog/CST/port.h"
#include "verible/verilog/CST/type.h"
#include "verible/verilog/CST/verilog-matchers.h"  // IWYU pragma: keep
#include "verible/verilog/CST/verilog-nonterminals.h"
#include "verible/verilog/analysis/verilog-analyzer.h"
#include "verible/verilog/formatting/format-style-init.h"
#include "verible/verilog/formatting/format-style.h"
#include "verible/verilog/formatting/formatter.h"
#include "verible/verilog/tools/ls/lsp-parse-buffer.h"
#include "verible/verilog/tools/ls/symbol-table-handler.h"

namespace verilog {
using verible::FindLastSubtree;
using verible::Interval;
using verible::LineColumn;
using verible::LineColumnRange;
using verible::StringSpanOfSymbol;
using verible::Symbol;
using verible::SymbolCastToNode;
using verible::SymbolKind;
using verible::SymbolPtr;
using verible::SyntaxTreeLeaf;
using verible::SyntaxTreeNode;
using verible::TextStructureView;
using verible::TokenInfo;
using verible::TreeSearchMatch;
using verible::lsp::CodeAction;
using verible::lsp::CodeActionParams;
using verible::lsp::TextEdit;
using verilog::VerilogAnalyzer;
using verilog::formatter::FormatStyle;
using verilog::formatter::InitializeFromFlags;

namespace {
// Possible kinds of AUTO
enum class AutoKind {
  kAutoarg,
  kAutoinst,
  kAutoinput,
  kAutoinout,
  kAutooutput,
  kAutowire,
  kAutoreg,
};

// Takes a TextStructureView and generates LSP TextEdits for AUTO expansion
class AutoExpander {
 public:
  // TODO: move most of these items to private

  // An AUTO matched in the buffer text
  struct Match {
    absl::string_view auto_span;     // Span of the entire AUTO
    absl::string_view comment_span;  // Span of the AUTO pragma comment
  };

  // A single AUTO expansion in terms of the replaced span and expanded text
  struct Expansion {
    absl::string_view replaced_span;  // Span that is to be replaced
    std::string new_text;             // Text to replace the span with
  };

  // Represents a port connection
  struct Connection {
    std::string port_name;  // The name of the port in the module instance
    bool emit_dimensions;   // If true, when emitted, the connection should be
                            // annotated with the signal dimensions
  };

  // Stores information about the instance the port is connected to
  struct ConnectedInstance {
    std::optional<absl::string_view> instance;  // Name of the instance a port
                                                // is connected to
    absl::string_view type;  // Type of the instance a port is connected to
  };

  // A SystemVerilog range [msb:lsb]
  struct DimensionRange {
    int64_t msb;
    int64_t lsb;
  };

  // A dimension can be a range, an unsigned integer, or, if it cannot be
  // interpreted as one of these, a string
  using Dimension = std::variant<absl::string_view, size_t, DimensionRange>;

  // Iterates through the given dimension vectors and returns a new one with
  // each element being the maximum of corresponding original dimensions.
  static std::vector<Dimension> MaxDimensions(
      const std::vector<Dimension> &first,
      const std::vector<Dimension> &second);

  // Representation of a net-like, base type for Port and Wire (you probably
  // want to use those)
  struct Net {
    std::string name;                            // Name of the net
    std::vector<ConnectedInstance> conn_inst;    // What instances is
                                                 // it connected to?
    std::vector<Dimension> packed_dimensions;    // This net's packed dimensions
    std::vector<Dimension> unpacked_dimensions;  // This net's unpacked
                                                 // dimensions

    // Writes the port's identifier with packed and unpacked dimensions to the
    // output stream
    void EmitIdWithDimensions(std::ostream &output) const;

    // Returns true if the port is connected to any instance
    bool IsConnected() const { return !conn_inst.empty(); }

    // Adds the given connected instance to the net's list of connections, and
    // makes the packed dimensions the max of the current dimensions and the
    // ones provided
    void AddConnection(const ConnectedInstance &connected,
                       const std::vector<Dimension> &new_packed_dimensions) {
      conn_inst.push_back(connected);
      packed_dimensions =
          AutoExpander::MaxDimensions(packed_dimensions, new_packed_dimensions);
    }
  };

  // A port, with direction and some meta-info
  struct Port final : Net {
    enum class Direction { kInput, kInout, kOutput };
    enum class Declaration { kUndeclared, kAutogenerated, kDeclared };

    Direction direction;      // Direction of the port
    Declaration declaration;  // Is it user-declared or autogenerated
    absl::string_view::const_iterator it;  // Location of the port's declaration
                                           // in the source file

    // Writes the port's direction to the output stream
    void EmitDirection(std::ostream &output) const;

    // Writes a comment describing the port's connection to the output stream
    void EmitConnectionComment(std::ostream &output) const;
  };

  // A wire generated by AUTO expansion
  struct Wire final : Net {
    // Writes a comment describing the port's connection to the output stream
    void EmitConnectionComment(std::ostream &output) const;
  };

  // Represents an AUTO_TEMPLATE
  struct Template {
    using Map = absl::flat_hash_map<absl::string_view, std::vector<Template>>;

    absl::string_view::const_iterator it;   // Location of the template in the
                                            // source file
    std::shared_ptr<RE2> instance_name_re;  // Regex for matching the instance
                                            // name. Shared between templates
                                            // declared at the same place
    absl::flat_hash_map<absl::string_view, Connection>
        connections;  // Map of instance ports ports to connected module ports
  };

  enum class PortDeclStyle {
    kColonSeparator,
    kCommaSeparator,
    kCommaSeparatorExceptLast
  };

  // Module information relevant to AUTO expansion
  class Module {
   public:
    explicit Module(const Symbol &module)
        : symbol_(module), name_(GetModuleName(symbol_)->get().text()) {
      RetrieveModuleHeaderPorts();
      RetrieveModuleBodyPorts();
    }

    // Writes all port names that match the predicate to the output stream,
    // under the specified heading comment
    void EmitNonAnsiPortList(
        std::ostream &output, absl::string_view heading,
        const std::function<bool(const Port &)> &pred) const;

    // Writes port connections to all ports to the output stream, under the
    // specified heading comment
    void EmitPortConnections(std::ostream &output,
                             absl::string_view instance_name,
                             absl::string_view header,
                             const std::function<bool(const Port &)> &pred,
                             const Template *tmpl) const;

    // Writes declarations of ports that fulfill the given predicate to the
    // output stream
    void EmitPortDeclarations(
        std::ostream &output, PortDeclStyle style,
        const std::function<bool(const Port &)> &pred) const;

    // Writes wire declarations of undeclared output ports to the output stream,
    // with the provided span defining which existing wires were autogenerated
    void EmitUndeclaredWireDeclarations(std::ostream &output,
                                        absl::string_view auto_span) const;

    // Writes reg declarations of unconnected output ports to the output stream,
    // with the provided span defining which existing regs were autogenerated
    void EmitUnconnectedOutputRegDeclarations(
        std::ostream &output, absl::string_view auto_span) const;

    // Calls the closure on each port and the name of the port that should be
    // connected to it. If a template is given, the connected port name is taken
    // from the template, otherwise it's the same as the port name.
    void GenerateConnections(
        absl::string_view instance_name, const Template *tmpl,
        const std::function<void(const Port &, const Connection &)> &fun) const;

    // Set an existing port's connection, or create a new port with the given
    // name, direction, and connection
    void AddGeneratedConnection(
        const std::string &port_name, Port::Direction direction,
        const ConnectedInstance &connected,
        const std::vector<Dimension> &packed_dimensions,
        const std::vector<Dimension> &unpacked_dimensions);

    // Sort ports by location in the source
    void SortPortsByLocation();

    // Gets all AUTO_TEMPLATEs from the module
    void RetrieveAutoTemplates();

    // Gets all dependencies of the module (modules instantiated within it)
    void RetrieveDependencies(
        const absl::node_hash_map<absl::string_view, Module> &modules);

    // Retrieves the matching template from a typename -> template map
    const Template *GetAutoTemplate(
        absl::string_view type_id, absl::string_view instance_name,
        absl::string_view::const_iterator instance_it) const;

    // Returns true if the module depends on (uses) a given module
    bool DependsOn(const Module *module) const {
      if (this == module) return false;
      absl::flat_hash_set<const Module *> visited;
      return DependsOn(module, &visited);
    }

    // Returns true if any ports fulfill the given predicate
    bool AnyPorts(const std::function<bool(const Port &)> &pred) {
      return std::find_if(ports_.begin(), ports_.end(), pred) != ports_.end();
    }

    // Calls the given function on each port
    void ForEachPort(const std::function<void(Port &)> &fun) {
      std::for_each(ports_.begin(), ports_.end(), fun);
    }

    // Erase all ports that fulfill the given predicate
    void ErasePortsIf(const std::function<bool(const Port &)> &pred) {
      const auto it = std::remove_if(ports_.begin(), ports_.end(), pred);
      ports_.erase(it, ports_.end());
    }

    // Returns the Symbol representing this module
    const verible::Symbol &Symbol() const { return symbol_; }

    // Returns the module name
    absl::string_view Name() const { return name_; }

   private:
    //  Gets ports from the header of the module
    void RetrieveModuleHeaderPorts();

    // Gets ports from the body of the module
    void RetrieveModuleBodyPorts();

    // Store the given port in the internal vector
    void PutDeclaredPort(const SyntaxTreeNode &port_node);

    // Recurses into dependencies to check if we depend on a given module.
    // Stores visited modules in a set to avoid infinite loops. For big
    // dependency graphs one should build a proper graph and do a
    // topological sort. However, these dependencies (and the dependent) are all
    // from a single file, and usually there is only one module per file. This
    // should be fast enough for unusual cases where there are multiple modules
    // in a single file.
    bool DependsOn(const Module *module,
                   absl::flat_hash_set<const Module *> *visited) const;

    // The symbol that represents this module
    const verible::Symbol &symbol_;

    // The name of this module
    const absl::string_view name_;

    // This module's ports
    std::vector<Port> ports_;

    // New wires to emit (if not already defined)
    std::vector<Wire> wires_to_generate_;

    // This module's direct dependencies
    absl::flat_hash_set<const Module *> dependencies_;

    // This module's AUTO_TEMPLATEs
    Template::Map templates_;
  };

  AutoExpander(const TextStructureView &text_structure,
               SymbolTableHandler *symbol_table_handler)
      : text_structure_(text_structure),
        symbol_table_handler_(symbol_table_handler) {
    expand_span_ = text_structure_.Contents();
  }

  AutoExpander(const TextStructureView &text_structure,
               SymbolTableHandler *symbol_table_handler,
               Interval<size_t> line_range)
      : AutoExpander(text_structure, symbol_table_handler) {
    size_t min = line_range.min < text_structure.Lines().size()
                     ? line_range.min
                     : text_structure.Lines().size() - 1;
    size_t max = line_range.max < text_structure.Lines().size()
                     ? line_range.max
                     : text_structure.Lines().size() - 1;
    const auto begin = text_structure.Lines()[min].begin();
    const auto end = text_structure.Lines()[max].end();
    const size_t length = static_cast<size_t>(std::distance(begin, end));
    expand_span_ = absl::string_view(begin, length);
  }

  AutoExpander(const TextStructureView &text_structure,
               SymbolTableHandler *symbol_table_handler,
               const absl::flat_hash_set<AutoKind> &allowed_autos)
      : AutoExpander(text_structure, symbol_table_handler) {
    allowed_autos_ = allowed_autos;
  }

  // Retrieves port names from a module declared before the given location
  absl::flat_hash_set<absl::string_view> GetPortsListedBefore(
      const Symbol &module, absl::string_view::const_iterator it) const;

  // Retrieves port names from a module instance connected before the given
  // location
  absl::flat_hash_set<absl::string_view> GetPortsConnectedBefore(
      const Symbol &instance, absl::string_view::const_iterator it) const;

  // Expands AUTOARG for the given module
  std::optional<Expansion> ExpandAutoarg(const Module &module) const;

  // Expands AUTOINST for the given module instance
  std::optional<Expansion> ExpandAutoinst(Module *module,
                                          const Symbol &instance,
                                          absl::string_view type_id);

  // Expands AUTO<port-direction/data-type> for the given module
  // Limitation: this only detects ports from AUTOINST. This limitation is also
  // present in the original Emacs Verilog-mode.
  std::optional<Expansion> ExpandAutoDeclarations(
      const Module &module, Match match, absl::string_view description,
      const std::function<void(const Module &, std::ostream &)> &emit) const;

  // Expands AUTOINPUT/AUTOINOUT/AUTOOUTPUT for the given module
  std::optional<Expansion> ExpandAutoPorts(Module *module,
                                           std::optional<Match> match,
                                           Port::Direction direction) const;

  // Expands AUTOWIRE for the given module
  std::optional<Expansion> ExpandAutowire(const Module &module) const;

  // Expands AUTOREG for the given module
  std::optional<Expansion> ExpandAutoreg(const Module &module) const;

  // Expands all AUTOs in the buffer
  std::vector<Expansion> Expand();

  // Find kinds of AUTO used in the expand span
  absl::flat_hash_set<AutoKind> FindAutoKinds();

 private:
  // Matches the given regex and erases ports from the module that are in the
  // match span
  std::optional<Match> FindMatchAndErasePorts(AutoExpander::Module *module,
                                              AutoKind kind, const RE2 &re);

  // Finds the span that should be replaced in the symbol (from the start of
  // the comment span to the end of the symbol span. Used by AUTOARG and
  // AUTOINST)
  std::optional<absl::string_view> FindSpanToReplace(
      const Symbol &symbol, absl::string_view auto_span) const;

  // Checks if the given AUTO kind should be expanded
  bool ShouldExpand(const AutoKind kind) const {
    return allowed_autos_.empty() || allowed_autos_.contains(kind);
  }

  // Span in which expansions are allowed
  absl::string_view expand_span_;

  // Kinds of AUTOs that can be expanded (all if this set is empty)
  absl::flat_hash_set<AutoKind> allowed_autos_;

  // Text structure of the buffer to expand AUTOs in
  const TextStructureView &text_structure_;

  // Symbol table wrapper for the language server
  SymbolTableHandler *symbol_table_handler_;

  // Gathered module information (module name -> module info)
  absl::node_hash_map<absl::string_view, Module> modules_;

  // Regex for finding any AUTOs
  static const LazyRE2 auto_re_;

  // Regex for finding AUTOARG comments
  static const LazyRE2 autoarg_re_;

  // Regex for finding AUTOINST comments
  static const LazyRE2 autoinst_re_;

  // Regexes for AUTO_TEMPLATE comments
  static const LazyRE2 autotemplate_re_;
  static const LazyRE2 autotemplate_type_re_;
  static const LazyRE2 autotemplate_conn_re_;

  // Regexes for AUTOINPUT/AUTOOUTPUT/AUTOINOUT/AUTOWIRE/AUTOREG comments
  static const LazyRE2 autoinput_re_;
  static const LazyRE2 autooutput_re_;
  static const LazyRE2 autoinout_re_;
  static const LazyRE2 autowire_re_;
  static const LazyRE2 autoreg_re_;
};

const LazyRE2 AutoExpander::auto_re_{
    R"(/\*\s*(AUTOARG|AUTOINST|AUTOINPUT|AUTOINOUT|AUTOOUTPUT|AUTOWIRE|AUTOREG)\s*\*/)"};

const LazyRE2 AutoExpander::autoarg_re_{R"((/\*\s*AUTOARG\s*\*/))"};

const LazyRE2 AutoExpander::autoinst_re_{R"((/\*\s*AUTOINST\s*\*/))"};

// AUTO_TEMPLATE regex breakdown:
// The entire expression is wrapped in () so the first capturing group is the
// entire match.
// /\*                               – start of comment
// (?:\s*\S+\s+AUTO_TEMPLATE\s*\n)*  – optional other AUTO_TEMPLATE types, end
//                                     with newline
// \s*\S+\s+AUTO_TEMPLATE            – at least one AUTO_TEMPLATE is required
// \s*(?:"([^"])*")?                 – optional instance name regex
// \s*\(?:[\s\S]*?\);                – parens with port connections
// \s*\*/                            – end of comment
const LazyRE2 AutoExpander::autotemplate_re_{
    R"((/\*(?:\s*\S+\s+AUTO_TEMPLATE\s*\n)*\s*\S+\s+AUTO_TEMPLATE\s*(?:"([^"]*)\")?\s*\([\s\S]*?\);\s*\*/))"};

// AUTO_TEMPLATE type regex: the first capturing group is the instance type
const LazyRE2 AutoExpander::autotemplate_type_re_{R"((\S+)\s+AUTO_TEMPLATE)"};

// AUTO_TEMPLATE connection regex breakdown:
// \.\s*      – starts with a dot
// ([^\s(]+?) – first group, at least one character other than whitespace or
//              opening paren
// \s*\(\s*   – optional whitespace, opening paren, optional whitespace again
// ([^\s(]+?) – second group, same as the first one
// \s*(\[\])? – optional third group, capturing '[]'
// \s*\)*     – optional whitespace, closing paren
const LazyRE2 AutoExpander::autotemplate_conn_re_{
    R"(\.\s*([^\s(]+?)\s*\(\s*([^\s(]+?)\s*(\[\])?\s*\))"};

// AUTOINPUT/OUTPUT/INOUT/WIRE/REG regex breakdown:
// The entire expression is wrapped in () so the first capturing group is the
// entire match.
// (/\*\s* ... \s*\*/\s*?\n)            – starting comment
// (?:\s*//.*\n)?                       – optional starting comment
//                                        ("Beginning of automatic...")
// (?: ... )?                           – an optional non-capturing group:
//   [\s\S]*?                           – any text (usually port
//                                        declarations)
//   [^\S\r\n]*// End of automatics.*\n – ended by an "End of automatics"
//                                        comment
#define MAKE_AUTODECL_REGEX(decl_kind) \
  R"(((/\*\s*AUTO)" decl_kind          \
  R"(\s*\*/\s*?)(?:\s*//.*)?(?:[\s\S]*?[^\S\r\n]*// End of automatics.*)?))"
const LazyRE2 AutoExpander::autoinput_re_{MAKE_AUTODECL_REGEX("INPUT")};
const LazyRE2 AutoExpander::autoinout_re_{MAKE_AUTODECL_REGEX("INOUT")};
const LazyRE2 AutoExpander::autooutput_re_{MAKE_AUTODECL_REGEX("OUTPUT")};
const LazyRE2 AutoExpander::autowire_re_{MAKE_AUTODECL_REGEX("WIRE")};
const LazyRE2 AutoExpander::autoreg_re_{MAKE_AUTODECL_REGEX("REG")};

using Dimension = AutoExpander::Dimension;
using DimensionRange = AutoExpander::DimensionRange;

// Fallback for the case of comparing two dimensions if there is no obvious
// maximum. Simply returns the first given dimension.
template <typename T, typename U>
static Dimension MaxDimension(const T first, const U second) {
  return first;
}

// Returns the greater of the two given dimensions.
template <>
Dimension MaxDimension(const size_t first, const size_t second) {
  return std::max(first, second);
}

// Returns a range that can fit the two given dimensions.
template <>
Dimension MaxDimension(const DimensionRange first,
                       const DimensionRange second) {
  int64_t max = std::max(std::max(first.msb, first.lsb),
                         std::max(second.msb, second.lsb));
  int64_t min = std::min(std::min(first.msb, first.lsb),
                         std::min(second.msb, second.lsb));
  if (first.msb >= first.lsb) {
    return DimensionRange{.msb = max, .lsb = min};
  }
  return DimensionRange{.msb = min, .lsb = max};
}

// Converts the first dimension to a DimensionRange, and then returns the
// maximum of it and the given range.
template <>
Dimension MaxDimension(const size_t first, const DimensionRange second) {
  return MaxDimension(
      DimensionRange{.msb = static_cast<int64_t>(first) - 1, .lsb = 0}, second);
}

// Converts the second dimension to a DimensionRange, and then returns the
// maximum of it and the given range.
template <>
Dimension MaxDimension(const DimensionRange first, const size_t second) {
  return MaxDimension(
      first, DimensionRange{.msb = static_cast<int64_t>(second) - 1, .lsb = 0});
}

std::vector<Dimension> AutoExpander::MaxDimensions(
    const std::vector<Dimension> &first, const std::vector<Dimension> &second) {
  if (first.empty() && second.size() == 1) return second;
  if (second.empty() && first.size() == 1) return first;
  if (first.size() != second.size()) LOG(ERROR) << "Mismatched dimensions";
  std::vector<Dimension> dims;
  auto it1 = first.begin(), it2 = second.begin();
  while (it1 != first.end() && it2 != second.end()) {
    const auto dims1 = *it1, dims2 = *it2;
    std::visit(
        [&](const auto d1) {
          std::visit(
              [&](const auto d2) { dims.push_back(MaxDimension(d1, d2)); },
              dims2);
        },
        dims1);
    ++it1;
    ++it2;
  }
  return dims;
}

std::ostream &operator<<(std::ostream &os, const DimensionRange range) {
  os << range.msb << ":" << range.lsb;
  return os;
}

std::ostream &operator<<(std::ostream &os, const Dimension dim) {
  std::visit([&os](auto &&arg) { os << '[' << arg << ']'; }, dim);
  return os;
}

void AutoExpander::Net::EmitIdWithDimensions(std::ostream &output) const {
  if (!packed_dimensions.empty()) {
    for (const Dimension &dimension : packed_dimensions) {
      output << dimension;
    }
    output << ' ';
  }
  output << name;
  for (const Dimension &dimension : unpacked_dimensions) {
    output << dimension;
  }
}

void AutoExpander::Port::EmitDirection(std::ostream &output) const {
  switch (direction) {
    case Port::Direction::kInput:
      output << "input ";
      break;
    case Port::Direction::kInout:
      output << "inout ";
      break;
    case Port::Direction::kOutput:
      output << "output ";
      break;
    default:
      LOG(ERROR) << "Incorrect port direction";
      break;
  }
}

void AutoExpander::Port::EmitConnectionComment(std::ostream &output) const {
  if (conn_inst.empty()) return;
  const auto &instance = conn_inst[0].instance;
  if (!instance) return;
  const absl::string_view type = conn_inst[0].type;
  switch (direction) {
    case Direction::kInput:
      output << "  // To " << *instance << " of " << type;
      break;
    case Direction::kInout:
      output << "  // To/From " << *instance << " of " << type;
      break;
    case Direction::kOutput:
      output << "  // From " << *instance << " of " << type;
      break;
    default:
      LOG(ERROR) << "Incorrect port direction";
      return;
  }
  if (conn_inst.size() > 1) output << ", ...";
  // TODO: Print as many instance names as we can without going against the
  // formatter?
}

void AutoExpander::Wire::EmitConnectionComment(std::ostream &output) const {
  if (conn_inst.empty()) return;
  const auto &instance = conn_inst[0].instance;
  if (!instance) return;
  output << "  // To/From " << *instance << " of " << conn_inst[0].type;
  if (conn_inst.size() > 1) output << ", ..";
}

void AutoExpander::Module::EmitNonAnsiPortList(
    std::ostream &output, const absl::string_view heading,
    const std::function<bool(const Port &)> &pred) const {
  bool first = true;
  for (const Port &port : ports_) {
    if (!pred(port)) continue;
    if (first) {
      if (output.tellp() != 0) output << ',';
      output << '\n' << "// " << heading << '\n';
      first = false;
    } else {
      output << ',';
    }
    output << port.name;
  }
}

void AutoExpander::Module::EmitPortConnections(
    std::ostream &output, const absl::string_view instance_name,
    const absl::string_view header,
    const std::function<bool(const Port &)> &pred, const Template *tmpl) const {
  bool first = true;
  GenerateConnections(
      instance_name, tmpl, [&](const Port &port, const Connection &connected) {
        if (!pred(port)) return;
        if (first) {
          if (output.tellp() != 0) output << ',';
          output << '\n' << "// " << header;
          first = false;
        } else {
          output << ',';
        }
        output << '\n' << '.' << port.name << '(' << connected.port_name;
        if (!connected.emit_dimensions) {
          output << ')';
          return;
        }
        if (port.packed_dimensions.size() > 1 ||
            !port.unpacked_dimensions.empty()) {
          output << "/*";
          for (const Dimension &dimension : port.packed_dimensions) {
            output << dimension;
          }
          if (!port.unpacked_dimensions.empty()) {
            output << '.';
            for (const Dimension &dimension : port.unpacked_dimensions) {
              output << dimension;
            }
          }
          output << "*/";
        } else if (port.packed_dimensions.size() == 1) {
          output << port.packed_dimensions[0];
        }
        output << ')';
      });
}

// Checks if two string spans are overlapping
bool SpansOverlapping(const absl::string_view first,
                      const absl::string_view second) {
  return first.end() > second.begin() && first.begin() < second.end();
}

void AutoExpander::Module::EmitUndeclaredWireDeclarations(
    std::ostream &output, const absl::string_view auto_span) const {
  absl::flat_hash_set<absl::string_view> declared_wires;
  for (const auto &reg : FindAllNetVariables(symbol_)) {
    const SyntaxTreeLeaf *const net_name_leaf =
        GetNameLeafOfNetVariable(*reg.match);
    const absl::string_view net_name = net_name_leaf->get().text();
    if (!SpansOverlapping(net_name, auto_span)) {
      declared_wires.insert(net_name);
    }
  }

  for (const Port &port : ports_) {
    if (port.direction != Port::Direction::kInput &&
        port.declaration == Port::Declaration::kUndeclared &&
        port.IsConnected() && !declared_wires.contains(port.name)) {
      output << "wire ";
      port.EmitIdWithDimensions(output);
      output << ';';
      port.EmitConnectionComment(output);
      output << '\n';
    }
  }
  for (const Wire &wire : wires_to_generate_) {
    output << "wire ";
    wire.EmitIdWithDimensions(output);
    output << ';';
    wire.EmitConnectionComment(output);
    output << '\n';
  }
}

void AutoExpander::Module::EmitUnconnectedOutputRegDeclarations(
    std::ostream &output, const absl::string_view auto_span) const {
  absl::flat_hash_set<absl::string_view> declared_regs;
  for (const auto &reg : FindAllRegisterVariables(symbol_)) {
    const SyntaxTreeLeaf *const reg_name_leaf =
        GetNameLeafOfRegisterVariable(*reg.match);
    const absl::string_view reg_name = reg_name_leaf->get().text();
    if (!SpansOverlapping(reg_name, auto_span)) {
      declared_regs.insert(reg_name);
    }
  }

  for (const Port &port : ports_) {
    if (port.direction == Port::Direction::kOutput &&
        port.declaration == Port::Declaration::kDeclared &&
        !port.IsConnected() && !declared_regs.contains(port.name)) {
      output << "reg ";
      port.EmitIdWithDimensions(output);
      output << ";\n";
    }
  }
}

void AutoExpander::Module::GenerateConnections(
    absl::string_view instance_name, const Template *tmpl,
    const std::function<void(const Port &, const Connection &)> &fun) const {
  for (const Port &port : ports_) {
    Connection connected{.port_name = port.name, .emit_dimensions = true};
    if (tmpl) {
      const auto it = tmpl->connections.find(port.name);
      if (it != tmpl->connections.end()) connected = it->second;
    }
    size_t pos = connected.port_name.find('@');
    while (pos != std::string::npos) {
      connected.port_name.replace(pos, 1, instance_name.begin(),
                                  instance_name.length());
      pos = connected.port_name.find('@', pos);
    }
    fun(port, connected);
  }
}

void AutoExpander::Module::AddGeneratedConnection(
    const std::string &port_name, const Port::Direction direction,
    const ConnectedInstance &connected,
    const std::vector<Dimension> &packed_dimensions,
    const std::vector<Dimension> &unpacked_dimensions) {
  const auto name_matches = [&](const Net &net) {
    return net.name == port_name;
  };
  const auto wire_it = std::find_if(wires_to_generate_.begin(),
                                    wires_to_generate_.end(), name_matches);
  // If there is already a wire with the same name, add the connection to it.
  // This wire is a connection between multiple instances.
  if (wire_it != wires_to_generate_.end()) {
    wire_it->AddConnection(connected, packed_dimensions);
    return;
  }
  const auto port_it = std::find_if(ports_.begin(), ports_.end(), name_matches);
  // Else look for an existing port with this name. If there is one, and it has
  // the same direction, reuse it. If its direction differs, convert it to a new
  // wire.
  if (port_it != ports_.end()) {
    Port &port = *port_it;
    if (port.direction == direction) {
      port.AddConnection(connected, packed_dimensions);
    } else {
      wires_to_generate_.push_back(
          {{.name = port_name,
            .conn_inst = port.conn_inst,
            .packed_dimensions = AutoExpander::MaxDimensions(
                port.packed_dimensions, packed_dimensions),
            .unpacked_dimensions = unpacked_dimensions}});
      wires_to_generate_.back().AddConnection(connected, packed_dimensions);
      ports_.erase(port_it, port_it + 1);
    }
    return;
  }
  // There are no wires or ports of the given name. Just make a new port.
  ports_.push_back({
      {port_name, {connected}, packed_dimensions, unpacked_dimensions},
      direction,
      Port::Declaration::kUndeclared,
      nullptr,
  });
}

void AutoExpander::Module::SortPortsByLocation() {
  // Stable sort is needed here, as ports autogenerated via AUTOINPUT,
  // AUTOOUTPUT, and AUTOINOUT get assigned one location, which is the start of
  // the corresponding `AUTO` comment. Using unstable sort results in a random
  // order of ports.
  std::stable_sort(
      ports_.begin(), ports_.end(),
      [](const Port &left, const Port &right) { return left.it < right.it; });
}

void AutoExpander::Module::RetrieveAutoTemplates() {
  absl::string_view autotmpl_search_span = StringSpanOfSymbol(symbol_);
  absl::string_view autotmpl_span;
  absl::string_view autotmpl_inst_name;
  while (RE2::FindAndConsume(&autotmpl_search_span, *autotemplate_re_,
                             &autotmpl_span, &autotmpl_inst_name)) {
    Template tmpl{.it = autotmpl_span.begin()};
    if (!autotmpl_inst_name.empty()) {
      tmpl.instance_name_re = std::make_shared<RE2>(autotmpl_inst_name);
      if (!tmpl.instance_name_re->ok()) {
        LOG(ERROR) << "Invalid regex in AUTO template: " << autotmpl_inst_name;
        continue;
      }
    }

    absl::string_view autotmpl_conn_search_span = autotmpl_span;
    absl::string_view instance_port_name;
    absl::string_view module_port_name;
    absl::string_view dimensions;
    while (RE2::FindAndConsume(&autotmpl_conn_search_span,
                               *autotemplate_conn_re_, &instance_port_name,
                               &module_port_name, &dimensions)) {
      tmpl.connections.insert(
          std::make_pair(instance_port_name,
                         Connection{.port_name = std::string(module_port_name),
                                    .emit_dimensions = !dimensions.empty()}));
    }

    absl::string_view autotmpl_type_search_span = autotmpl_span;
    absl::string_view instance_type_name;
    while (RE2::FindAndConsume(&autotmpl_type_search_span,
                               *autotemplate_type_re_, &instance_type_name)) {
      templates_[instance_type_name].push_back(tmpl);
    }
  }
}

void AutoExpander::Module::RetrieveDependencies(
    const absl::node_hash_map<absl::string_view, Module> &modules) {
  for (const auto &data : FindAllDataDeclarations(symbol_)) {
    const verible::Symbol *const type_id_node =
        GetTypeIdentifierFromDataDeclaration(*data.match);
    // Some data declarations do not have a type id, ignore those
    if (!type_id_node) continue;
    const absl::string_view dependency_name = StringSpanOfSymbol(*type_id_node);
    const auto it = modules.find(dependency_name);
    if (it != modules.end()) {
      dependencies_.insert(&it->second);
    }
  }
}

const AutoExpander::Template *AutoExpander::Module::GetAutoTemplate(
    const absl::string_view type_id, const absl::string_view instance_name,
    const absl::string_view::const_iterator instance_it) const {
  const auto it = templates_.find(type_id);
  if (it == templates_.end()) return nullptr;
  const Template *matching_tmpl = nullptr;
  // Linear search for the matching template (there should be very few
  // templates per type, often just one)
  for (const Template &tmpl : it->second) {
    if (instance_it < tmpl.it) break;
    if (tmpl.instance_name_re) {
      if (!RE2::FullMatch(instance_name, *tmpl.instance_name_re)) continue;
    }
    matching_tmpl = &tmpl;
  }
  return matching_tmpl;
}

void AutoExpander::Module::EmitPortDeclarations(
    std::ostream &output, const PortDeclStyle style,
    const std::function<bool(const Port &)> &pred) const {
  const auto end = std::find_if(ports_.crbegin(), ports_.crend(), pred).base();
  for (auto it = ports_.cbegin(); it != end; it++) {
    const Port &port = *it;
    if (!pred(port)) continue;
    port.EmitDirection(output);
    port.EmitIdWithDimensions(output);
    if (style == PortDeclStyle::kColonSeparator) {
      output << ';';
    } else if (style == PortDeclStyle::kCommaSeparator || it + 1 < end) {
      output << ',';
    }
    port.EmitConnectionComment(output);
    output << '\n';
  }
}

void AutoExpander::Module::RetrieveModuleHeaderPorts() {
  const auto module_ports = GetModulePortDeclarationList(symbol_);
  if (!module_ports) return;
  for (const SymbolPtr &port : module_ports->children()) {
    if (port->Kind() == SymbolKind::kLeaf) continue;
    const SyntaxTreeNode &port_node = SymbolCastToNode(*port);
    const NodeEnum tag = NodeEnum(port_node.Tag().tag);
    if (tag == NodeEnum::kPortDeclaration) {
      PutDeclaredPort(port_node);
    }
  }
}

void AutoExpander::Module::RetrieveModuleBodyPorts() {
  for (const auto &port : FindAllModulePortDeclarations(symbol_)) {
    PutDeclaredPort(SymbolCastToNode(*port.match));
  }
}

// Converts kDimensionScalar and kDimensionRange nodes to Dimensions. Parses
// them as integers or ranges if possible, falls back to a string span.
std::vector<AutoExpander::Dimension> GetDimensionsFromNodes(
    const std::vector<TreeSearchMatch> &dimension_nodes) {
  using Dimension = AutoExpander::Dimension;
  std::vector<Dimension> dimensions;
  dimensions.reserve(dimension_nodes.size());
  for (auto &dimension : dimension_nodes) {
    for (const auto &scalar :
         SearchSyntaxTree(*dimension.match, NodekDimensionScalar())) {
      size_t size;
      const Symbol &scalar_value = *SymbolCastToNode(*scalar.match)[1];
      const absl::string_view span = StringSpanOfSymbol(scalar_value);
      const bool result = absl::SimpleAtoi(span, &size);
      dimensions.push_back(result ? Dimension{size} : Dimension{span});
    }
    for (const auto &range :
         SearchSyntaxTree(*dimension.match, NodekDimensionRange())) {
      const Symbol *left = GetDimensionRangeLeftBound(*range.match);
      const Symbol *right = GetDimensionRangeRightBound(*range.match);
      int64_t msb, lsb;
      const bool left_result =
          absl::SimpleAtoi(StringSpanOfSymbol(*left), &msb);
      const bool right_result =
          absl::SimpleAtoi(StringSpanOfSymbol(*right), &lsb);
      if (left_result && right_result) {
        dimensions.push_back(
            AutoExpander::DimensionRange{.msb = msb, .lsb = lsb});
      } else {
        const absl::string_view left_span = StringSpanOfSymbol(*left);
        const absl::string_view right_span = StringSpanOfSymbol(*right);
        dimensions.push_back(absl::string_view{
            left_span.begin(), static_cast<size_t>(std::distance(
                                   left_span.begin(), right_span.end()))});
      }
    }
  }
  return dimensions;
}

void AutoExpander::Module::PutDeclaredPort(const SyntaxTreeNode &port_node) {
  const NodeEnum tag = NodeEnum(port_node.Tag().tag);
  const SyntaxTreeLeaf *const dir_leaf =
      tag == NodeEnum::kPortDeclaration
          ? GetDirectionFromPortDeclaration(port_node)
          : GetDirectionFromModulePortDeclaration(port_node);
  const SyntaxTreeLeaf *const id_leaf =
      tag == NodeEnum::kPortDeclaration
          ? GetIdentifierFromPortDeclaration(port_node)
          : GetIdentifierFromModulePortDeclaration(port_node);
  if (!dir_leaf || !id_leaf) return;
  const absl::string_view dir_span = dir_leaf->get().text();
  const std::string name{id_leaf->get().text()};
  std::vector<Dimension> packed_dimensions =
      GetDimensionsFromNodes(FindAllPackedDimensions(port_node));
  std::vector<Dimension> unpacked_dimensions =
      GetDimensionsFromNodes(FindAllUnpackedDimensions(port_node));

  Port::Direction direction;
  if (dir_span == "input") {
    direction = Port::Direction::kInput;
  } else if (dir_span == "inout") {
    direction = Port::Direction::kInout;
  } else if (dir_span == "output") {
    direction = Port::Direction::kOutput;
  } else {
    LOG(ERROR) << "Incorrect port direction";
    return;
  }

  ports_.push_back({
      {
          name,
          {},
          std::move(packed_dimensions),
          std::move(unpacked_dimensions),
      },
      direction,
      Port::Declaration::kDeclared,
      dir_span.begin(),
  });
}

bool AutoExpander::Module::DependsOn(
    const Module *module, absl::flat_hash_set<const Module *> *visited) const {
  const bool already_visited = !visited->insert(this).second;
  if (already_visited) return false;
  for (const Module *dependency : dependencies_) {
    if (dependency == module) return true;
    if (dependency->DependsOn(module, visited)) return true;
  }
  return false;
}

absl::flat_hash_set<absl::string_view> AutoExpander::GetPortsListedBefore(
    const Symbol &module, const absl::string_view::const_iterator it) const {
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

    const TokenInfo &port_id_token = port_id_node->get();
    if (port_id_token.text().end() <= it) {
      ports_before.insert(port_id_token.text());
    }
  }
  return ports_before;
}

absl::flat_hash_set<absl::string_view> AutoExpander::GetPortsConnectedBefore(
    const Symbol &instance, const absl::string_view::const_iterator it) const {
  absl::flat_hash_set<absl::string_view> ports_before;
  for (const auto &port : FindAllActualNamedPort(instance)) {
    const SyntaxTreeLeaf *const id_node = GetActualNamedPortName(*port.match);
    if (!id_node) {
      LOG(WARNING) << "Named port connection with no identifier? Ignoring";
      continue;
    }
    const TokenInfo &id_token = id_node->get();
    if (id_token.text().end() <= it) {
      ports_before.insert(id_token.text());
    }
  }
  return ports_before;
}

// Does a regex search in the span of the given symbol, returns match
std::optional<AutoExpander::Match> FindMatchInSymbol(const Symbol &symbol,
                                                     const RE2 &re) {
  const absl::string_view symbol_span = StringSpanOfSymbol(symbol);
  absl::string_view match;
  absl::string_view comment;
  if (RE2::PartialMatch(symbol_span, re, &match, &comment)) {
    return AutoExpander::Match{
        .auto_span = {match.begin(), match.length()},
        .comment_span = {comment.begin(), comment.length()}};
  }
  return std::nullopt;
}

// Does a regex search in the span of the given symbol, returns matched span
std::optional<absl::string_view> FindSpanInSymbol(const Symbol &symbol,
                                                  const RE2 &re) {
  const absl::string_view symbol_span = StringSpanOfSymbol(symbol);
  absl::string_view match;
  if (RE2::PartialMatch({symbol_span.data(), symbol_span.length()}, re,
                        &match)) {
    return absl::string_view{match.begin(), match.length()};
  }
  return std::nullopt;
}

// Returns the deepest node that contains the given span
const Symbol *FindNodeContainingSpan(const Symbol &root,
                                     const absl::string_view span) {
  return FindLastSubtree(&root, [span](const Symbol &sym) {
    auto sym_span = StringSpanOfSymbol(sym);
    return span.begin() >= sym_span.begin() && sym_span.end() >= span.end();
  });
}

// Returns true if the given span is directly under the port declaration list
// (or the port paren group if there is no port declaration list)
bool IsSpanDirectlyUnderPortDeclarationList(const Symbol &port_parens,
                                            const absl::string_view span) {
  if (const Symbol *symbol = FindNodeContainingSpan(port_parens, span)) {
    return symbol == &port_parens ||
           NodeEnum(symbol->Tag().tag) == NodeEnum::kPortDeclarationList;
  }
  return false;
}

std::optional<AutoExpander::Expansion> AutoExpander::ExpandAutoarg(
    const Module &module) const {
  const SyntaxTreeNode *const port_parens =
      GetModulePortParenGroup(module.Symbol());
  if (!ShouldExpand(AutoKind::kAutoarg)) return std::nullopt;
  if (!port_parens) return std::nullopt;  // No port paren group, so no AUTOARG
  auto auto_span = FindSpanInSymbol(*port_parens, *autoarg_re_);
  if (!auto_span) return std::nullopt;
  auto replaced_span = FindSpanToReplace(*port_parens, *auto_span);
  if (!replaced_span) return std::nullopt;
  if (!IsSpanDirectlyUnderPortDeclarationList(*port_parens, *auto_span)) {
    LOG(ERROR) << "Not expanding AUTOARG. Incorrect context";
    return std::nullopt;
  }

  // Ports listed before the comment should not be redeclared
  const auto predeclared_ports =
      GetPortsListedBefore(module.Symbol(), auto_span->begin());

  std::ostringstream new_text;
  module.EmitNonAnsiPortList(new_text, "Inputs", [&](const Port &port) {
    return port.direction == Port::Direction::kInput &&
           !predeclared_ports.contains(port.name);
  });
  module.EmitNonAnsiPortList(new_text, "Inouts", [&](const Port &port) {
    return port.direction == Port::Direction::kInout &&
           !predeclared_ports.contains(port.name);
  });
  module.EmitNonAnsiPortList(new_text, "Outputs", [&](const Port &port) {
    return port.direction == Port::Direction::kOutput &&
           !predeclared_ports.contains(port.name);
  });

  return Expansion{.replaced_span = *replaced_span,
                   .new_text = absl::StrCat(*auto_span, new_text.str())};
}

// Returns true if the given span is directly under the port actual list (or
// instance paren group if there is no port actual list)
bool IsSpanDirectlyUnderPortActualList(const Symbol &instance_parens,
                                       const absl::string_view span) {
  if (const Symbol *symbol = FindNodeContainingSpan(instance_parens, span)) {
    return symbol == &instance_parens ||
           NodeEnum(symbol->Tag().tag) == NodeEnum::kPortActualList;
  }
  return false;
}

std::optional<AutoExpander::Expansion> AutoExpander::ExpandAutoinst(
    Module *module, const Symbol &instance, absl::string_view type_id) {
  if (!ShouldExpand(AutoKind::kAutoinst)) return std::nullopt;
  const SyntaxTreeNode *parens = GetParenGroupFromModuleInstantiation(instance);

  auto auto_span = FindSpanInSymbol(*parens, *autoinst_re_);
  if (!auto_span) return std::nullopt;
  auto replaced_span = FindSpanToReplace(*parens, *auto_span);
  if (!replaced_span) return std::nullopt;
  if (!IsSpanDirectlyUnderPortActualList(*parens, *auto_span)) {
    LOG(ERROR) << "Not expanding AUTOINST. Incorrect context";
    return std::nullopt;
  }

  const Symbol *const type_def =
      symbol_table_handler_->FindDefinitionSymbol(type_id);
  if (!type_def) {
    LOG(ERROR) << "AUTOINST: No definition found for module type: " << type_id;
    return std::nullopt;
  }
  if (NodeEnum(type_def->Tag().tag) != NodeEnum::kModuleDeclaration) {
    LOG(ERROR) << "AUTOINST: Instance type " << type_id
               << " is not a module, but a '" << NodeEnum(type_def->Tag().tag)
               << "'";
    return std::nullopt;
  }
  if (!modules_.contains(type_id)) {
    modules_.insert(std::make_pair(type_id, Module(*type_def)));
  }
  const Module &inst_module = modules_.at(type_id);

  // Find an AUTO_TEMPLATE that matches this instance
  const verible::TokenInfo *instance_name_token =
      GetModuleInstanceNameTokenInfoFromGateInstance(instance);
  if (!instance_name_token) {
    LOG(ERROR) << "AUTOINST: Instance with no name, aborting";
    return std::nullopt;
  }
  const absl::string_view instance_name = instance_name_token->text();
  const Template *const tmpl = module->GetAutoTemplate(
      inst_module.Name(), instance_name, StringSpanOfSymbol(instance).begin());

  // Ports connected before the AUTOINST comment should be ignored
  const auto preconnected_ports =
      GetPortsConnectedBefore(instance, auto_span->begin());

  std::ostringstream new_text;
  inst_module.EmitPortConnections(
      new_text, instance_name, "Inputs",
      [&](const Port &port) {
        return port.direction == Port::Direction::kInput &&
               port.declaration != Port::Declaration::kUndeclared &&
               !preconnected_ports.contains(port.name);
      },
      tmpl);
  inst_module.EmitPortConnections(
      new_text, instance_name, "Inouts",
      [&](const Port &port) {
        return port.direction == Port::Direction::kInout &&
               port.declaration != Port::Declaration::kUndeclared &&
               !preconnected_ports.contains(port.name);
      },
      tmpl);
  inst_module.EmitPortConnections(
      new_text, instance_name, "Outputs",
      [&](const Port &port) {
        return port.direction == Port::Direction::kOutput &&
               port.declaration != Port::Declaration::kUndeclared &&
               !preconnected_ports.contains(port.name);
      },
      tmpl);

  // The module's port connections need to be updated, as new ones may have
  // been generated
  inst_module.GenerateConnections(
      instance_name, tmpl, [&](const Port &port, const Connection &connected) {
        if (port.declaration == Port::Declaration::kUndeclared) return;
        ConnectedInstance connection{.instance = instance_name,
                                     .type = type_id};
        module->AddGeneratedConnection(connected.port_name, port.direction,
                                       connection, port.packed_dimensions,
                                       port.unpacked_dimensions);
      });

  return Expansion{.replaced_span = *replaced_span,
                   .new_text = absl::StrCat(*auto_span, new_text.str())};
}

std::optional<AutoExpander::Expansion> AutoExpander::ExpandAutoDeclarations(
    const Module &module, const Match match,
    const absl::string_view description,
    const std::function<void(const Module &, std::ostream &)> &emit) const {
  std::stringstream new_text;
  new_text << match.comment_span << "\n// Beginning of automatic "
           << description << '\n';
  const int64_t length_before_emit = new_text.tellp();
  emit(module, new_text);
  if (length_before_emit == new_text.tellp()) {
    if (match.auto_span != match.comment_span) {
      return Expansion{.replaced_span = match.auto_span,
                       .new_text = std::string{match.comment_span}};
    }
    return std::nullopt;
  }
  new_text << "// End of automatics";
  return Expansion{.replaced_span = match.auto_span,
                   .new_text = new_text.str()};
}

// Returns true if the span is directly under the module item list (or the
// module if there is no module item list)
bool IsSpanDirectlyUnderModule(const Symbol &module,
                               const absl::string_view span) {
  if (const Symbol *symbol = FindNodeContainingSpan(module, span)) {
    return symbol == &module ||
           NodeEnum(symbol->Tag().tag) == NodeEnum::kModuleItemList;
  }
  return false;
}

std::optional<AutoExpander::Expansion> AutoExpander::ExpandAutoPorts(
    Module *module, const std::optional<Match> match,
    const Port::Direction direction) const {
  if (!match) return std::nullopt;
  const absl::string_view module_span = StringSpanOfSymbol(module->Symbol());
  const auto begin = match->auto_span.end();
  auto end = module_span.end();
  const SyntaxTreeNode *const port_parens =
      GetModulePortParenGroup(module->Symbol());

  const bool in_header = port_parens && IsSpanDirectlyUnderPortDeclarationList(
                                            *port_parens, match->auto_span);
  if (!in_header &&
      !IsSpanDirectlyUnderModule(module->Symbol(), match->auto_span)) {
    LOG(ERROR) << "Not expanding AUTO ports. Incorrect context";
    return std::nullopt;
  }

  if (port_parens) end = StringSpanOfSymbol(*port_parens).end();
  const bool last = !module->AnyPorts([begin, end](const Port &port) {
    return port.it >= begin && port.it < end;
  });

  const PortDeclStyle style =
      in_header ? last ? PortDeclStyle::kCommaSeparatorExceptLast
                       : PortDeclStyle::kCommaSeparator
                : PortDeclStyle::kColonSeparator;
  const absl::string_view description = direction == Port::Direction::kInput
                                            ? "inputs (from autoinst inputs)"
                                        : direction == Port::Direction::kInout
                                            ? "inouts (from autoinst inouts)"
                                            : "outputs (from autoinst outputs)";

  if (!SpansOverlapping(match->auto_span, expand_span_)) return std::nullopt;
  auto result = ExpandAutoDeclarations(
      *module, *match, description,
      [direction, style](const Module &module, std::ostream &output) {
        module.EmitPortDeclarations(
            output, style, [direction](const Port &port) {
              return port.declaration == Port::Declaration::kUndeclared &&
                     port.direction == direction;
            });
      });

  module->ForEachPort([direction](Port &port) {
    if (port.declaration == Port::Declaration::kUndeclared &&
        port.direction == direction) {
      port.declaration = Port::Declaration::kAutogenerated;
    }
  });
  return result;
}

std::optional<AutoExpander::Expansion> AutoExpander::ExpandAutowire(
    const Module &module) const {
  if (!ShouldExpand(AutoKind::kAutowire)) return std::nullopt;
  const auto match = FindMatchInSymbol(module.Symbol(), *autowire_re_);
  if (!match) return std::nullopt;
  if (!SpansOverlapping(match->auto_span, expand_span_)) {
    return std::nullopt;
  }
  if (!IsSpanDirectlyUnderModule(module.Symbol(), match->auto_span)) {
    LOG(ERROR) << "Not expanding AUTOWIRE. Incorrect context";
    return std::nullopt;
  }
  return ExpandAutoDeclarations(
      module, *match, "wires (for undeclared instantiated-module outputs)",
      [match](const Module &module, std::ostream &output) {
        module.EmitUndeclaredWireDeclarations(output, match->auto_span);
      });
}

std::optional<AutoExpander::Expansion> AutoExpander::ExpandAutoreg(
    const Module &module) const {
  if (!ShouldExpand(AutoKind::kAutoreg)) return std::nullopt;
  const auto match = FindMatchInSymbol(module.Symbol(), *autoreg_re_);
  if (!match) return std::nullopt;
  if (!SpansOverlapping(match->auto_span, expand_span_)) {
    return std::nullopt;
  }
  if (!IsSpanDirectlyUnderModule(module.Symbol(), match->auto_span)) {
    LOG(ERROR) << "Not expanding AUTOREG. Incorrect context";
    return std::nullopt;
  }
  return ExpandAutoDeclarations(
      module, *match, "regs (for this module's undeclared outputs)",
      [match](const Module &module, std::ostream &output) {
        module.EmitUnconnectedOutputRegDeclarations(output, match->auto_span);
      });
}

std::optional<AutoExpander::Match> AutoExpander::FindMatchAndErasePorts(
    AutoExpander::Module *module, const AutoKind kind, const RE2 &re) {
  if (!ShouldExpand(kind)) return std::nullopt;
  const auto match = FindMatchInSymbol(module->Symbol(), re);
  if (match) {
    if (SpansOverlapping(StringSpanOfSymbol(module->Symbol()),
                         match->auto_span)) {
      module->ErasePortsIf([match](const AutoExpander::Port &port) {
        return port.it >= match->auto_span.begin() &&
               port.it < match->auto_span.end();
      });
    }
  }
  return match;
}

// Constructs a function that assigns the match's source location to undeclared
// ports of the specified direction
auto SetUndeclaredPortLocations(AutoExpander::Module *module,
                                const AutoExpander::Match match,
                                const AutoExpander::Port::Direction direction) {
  const auto it = match.auto_span.begin();
  return [it, direction](AutoExpander::Port &port) {
    if (port.declaration == AutoExpander::Port::Declaration::kUndeclared &&
        port.direction == direction) {
      port.it = it;
    }
  };
}

std::vector<AutoExpander::Expansion> AutoExpander::Expand() {
  std::vector<Expansion> expansions;
  if (!text_structure_.SyntaxTree()) {
    LOG(ERROR)
        << "Cannot perform AUTO expansion: failed to retrieve a syntax tree";
    return {};
  }
  std::vector<Module *> buffer_modules;  // Ordered list of all modules
                                         // in the buffer being modified
  for (const auto &mod_decl :
       FindAllModuleDeclarations(*text_structure_.SyntaxTree())) {
    Module module(*mod_decl.match);
    buffer_modules.push_back(
        &modules_.insert(std::make_pair(module.Name(), std::move(module)))
             .first->second);
  }
  for (auto module : buffer_modules) {
    module->RetrieveDependencies(modules_);
  }
  // Sort modules in the buffer based on a dependency graph, so that AUTOs are
  // expanded in order
  std::sort(buffer_modules.begin(), buffer_modules.end(),
            [](const Module *left, const Module *right) {
              return right->DependsOn(left);
            });
  for (Module *const module : buffer_modules) {
    // Ports declared in AUTOINPUT/AUTOINOUT/AUTOOUTPUT must be removed from
    // the module, as they should be regenerated every time (in case they get
    // removed or their names change)
    const auto autoinput_match =
        FindMatchAndErasePorts(module, AutoKind::kAutoinput, *autoinput_re_);
    const auto autoinout_match =
        FindMatchAndErasePorts(module, AutoKind::kAutoinout, *autoinout_re_);
    const auto autooutput_match =
        FindMatchAndErasePorts(module, AutoKind::kAutooutput, *autooutput_re_);
    // Do AUTOINST expansion
    module->RetrieveAutoTemplates();
    for (const auto &data : FindAllDataDeclarations(module->Symbol())) {
      const Symbol *const type_id_node =
          GetTypeIdentifierFromDataDeclaration(*data.match);
      // Some data declarations do not have a type id, ignore those
      if (!type_id_node) continue;
      const absl::string_view type_id = StringSpanOfSymbol(*type_id_node);
      for (const auto &instance : FindAllGateInstances(*data.match)) {
        if (const auto expansion =
                ExpandAutoinst(module, *instance.match, type_id)) {
          expansions.push_back(*expansion);
        }
      }
    }
    // Set AUTO port locations. This has to be done before any port expansions
    // so that ExpandAutoPorts() has the correct locations.
    if (autoinput_match) {
      module->ForEachPort(SetUndeclaredPortLocations(module, *autoinput_match,
                                                     Port::Direction::kInput));
    }
    if (autoinout_match) {
      module->ForEachPort(SetUndeclaredPortLocations(module, *autoinout_match,
                                                     Port::Direction::kInout));
    }
    if (autooutput_match) {
      module->ForEachPort(SetUndeclaredPortLocations(module, *autooutput_match,
                                                     Port::Direction::kOutput));
    }
    // Expand AUTO port declarations
    if (const auto expansion =
            ExpandAutoPorts(module, autoinput_match, Port::Direction::kInput)) {
      expansions.push_back(*expansion);
    }
    if (const auto expansion =
            ExpandAutoPorts(module, autoinout_match, Port::Direction::kInout)) {
      expansions.push_back(*expansion);
    }
    if (const auto expansion = ExpandAutoPorts(module, autooutput_match,
                                               Port::Direction::kOutput)) {
      expansions.push_back(*expansion);
    }
    // Expand AUTO wire/reg declarations
    if (const auto expansion = ExpandAutowire(*module)) {
      expansions.push_back(*expansion);
    }
    if (const auto expansion = ExpandAutoreg(*module)) {
      expansions.push_back(*expansion);
    }
    module->SortPortsByLocation();  // Ports need to be sorted by location in
                                    // source file to ensure AUTOARG stability
    // AUTOARG
    if (const auto expansion = ExpandAutoarg(*module)) {
      expansions.push_back(*expansion);
    }
  }
  return expansions;
}

absl::flat_hash_set<AutoKind> AutoExpander::FindAutoKinds() {
  absl::flat_hash_set<AutoKind> kinds;
  absl::string_view search_span = expand_span_;
  absl::string_view auto_str;
  while (RE2::FindAndConsume(&search_span, *auto_re_, &auto_str)) {
    if (auto_str == "AUTOARG") {
      kinds.insert(AutoKind::kAutoarg);
    } else if (auto_str == "AUTOINST") {
      kinds.insert(AutoKind::kAutoinst);
    } else if (auto_str == "AUTOINPUT") {
      kinds.insert(AutoKind::kAutoinput);
    } else if (auto_str == "AUTOINOUT") {
      kinds.insert(AutoKind::kAutoinout);
    } else if (auto_str == "AUTOOUTPUT") {
      kinds.insert(AutoKind::kAutooutput);
    } else if (auto_str == "AUTOWIRE") {
      kinds.insert(AutoKind::kAutowire);
    } else if (auto_str == "AUTOREG") {
      kinds.insert(AutoKind::kAutoreg);
    } else {
      LOG(ERROR) << "Invalid AUTO comment string";
    }
  }
  return kinds;
}

std::optional<absl::string_view> AutoExpander::FindSpanToReplace(
    const Symbol &symbol, const absl::string_view auto_span) const {
  const absl::string_view symbol_span = StringSpanOfSymbol(symbol);
  const size_t replaced_length = static_cast<size_t>(
      std::distance(auto_span.begin(), symbol_span.end() - 1));
  const absl::string_view replaced_span{auto_span.begin(), replaced_length};
  if (!SpansOverlapping(replaced_span, expand_span_)) {
    return std::nullopt;
  }
  return replaced_span;
}

// Returns an iterator pointing at the next expansion
// not overlapping with *start
// TODO: It's a template for readability, use C++20 concepts here
template <typename It>
It GetNextNonOverlappingExpansion(It start, It end) {
  It next = start;
  do {
    ++next;
  } while (next != end &&
           SpansOverlapping(start->replaced_span, next->replaced_span));
  return next;
}

// Applies the given AUTO expansions to the text structure, returning the
// resulting text
std::string ApplyExpansions(
    const TextStructureView &text_structure,
    const std::vector<AutoExpander::Expansion> &expansions) {
  std::string text;
  auto begin = text_structure.Contents().begin();
  for (auto it = expansions.begin(), next = it; it != expansions.end();
       it = next) {
    next = GetNextNonOverlappingExpansion(it, expansions.end());
    // If the next expansion does not overlap with ours, we can expand
    if (next == it + 1) {
      const AutoExpander::Expansion &expansion = *it;
      text.append(begin, expansion.replaced_span.begin());
      text.append(expansion.new_text);
      begin = expansion.replaced_span.end();
    } else {
      // TODO: Notify the user about this
      LOG(ERROR) << "Ignoring " << std::distance(it, next)
                 << " overlapping AUTO expansions";
    }
  }
  text.append(begin, text_structure.Contents().end());
  return text;
}

// Converts AutoExpander::Expansion structs to LSP TextEdits and performs
// formatting on them if possible
std::vector<TextEdit> ConvertAutoExpansionsToFormattedTextEdits(
    const TextStructureView &text_structure,
    std::vector<AutoExpander::Expansion> expansions) {
  std::sort(expansions.begin(), expansions.end(),
            [](const AutoExpander::Expansion &first,
               const AutoExpander::Expansion &second) {
              return first.replaced_span.begin() < second.replaced_span.begin();
            });

  // To format expansions, we need to apply them to the source. Then we only
  // call the formatter once, but with formatting ranges limited to
  // autogenerated code. The result is a single TextEdit that replaces the
  // entire file. This is orders of magnitude faster than formatting individual
  // TextEdits.
  std::string text = ApplyExpansions(text_structure, expansions);

  VerilogAnalyzer analyzer(text, "<autoexpand-reformat>");
  const absl::Status analyze_status = analyzer.Analyze();
  if (!analyze_status.ok()) {
    LOG(ERROR) << "AUTO expansion produced invalid syntax. Aborting.";
    return {};
  }

  FormatStyle format_style;
  InitializeFromFlags(&format_style);

  int64_t line_diff_acc = 0;
  verible::LineNumberSet lines;
  for (const AutoExpander::Expansion &expansion : expansions) {
    const LineColumnRange range =
        text_structure.GetRangeForText(expansion.replaced_span);
    // Formatter expects 1-indexed lines, hence the +1
    const int start_line =
        static_cast<int>(range.start.line + line_diff_acc + 1);
    const int new_text_line_count =
        std::count(expansion.new_text.begin(), expansion.new_text.end(), '\n') +
        1;
    const int end_line = static_cast<int>(start_line + new_text_line_count);
    const int line_diff =
        range.start.line + new_text_line_count - range.end.line - 1;
    line_diff_acc += line_diff;
    lines.Add({start_line, end_line});
  }

  std::string formatted;
  const absl::Status format_status =
      FormatVerilog(analyzer.Data(), "", format_style, &formatted, lines);

  std::string &new_text = text;
  if (format_status.ok()) {
    new_text = formatted;
  } else {
    LOG(ERROR) << "Failed to format AUTO expanded code";
  }

  const LineColumn linecol =
      text_structure.GetRangeForText(text_structure.Contents()).end;
  return {{.range =
               {
                   .start = {.line = 0, .character = 0},
                   .end = {.line = linecol.line, .character = linecol.column},
               },
           .newText = std::move(new_text)}};
}

}  // namespace

std::vector<CodeAction> GenerateAutoExpandCodeActions(
    SymbolTableHandler *symbol_table_handler,
    const BufferTracker *const tracker, const CodeActionParams &p) {
  Interval<size_t> line_range{static_cast<size_t>(p.range.start.line),
                              static_cast<size_t>(p.range.end.line)};
  if (!tracker) return {};
  const auto current = tracker->current();
  if (!current) return {};  // Can only expand if we have latest version
  const TextStructureView &text_structure = current->parser().Data();
  AutoExpander range_expander(text_structure, symbol_table_handler, line_range);
  const auto &auto_kinds = range_expander.FindAutoKinds();
  if (auto_kinds.empty()) return {};

  AutoExpander full_expander(text_structure, symbol_table_handler);
  const auto &expansions_full = full_expander.Expand();
  if (expansions_full.empty()) return {};
  std::vector<CodeAction> result;
  result.emplace_back(CodeAction{
      .title = "Expand all AUTOs in file",
      .kind = "refactor.rewrite",
      .edit = {.changes = {{p.textDocument.uri,
                            ConvertAutoExpansionsToFormattedTextEdits(
                                text_structure, expansions_full)}}},
  });

  const auto &expansions_range = range_expander.Expand();
  if (expansions_range.empty() ||
      expansions_range.size() == expansions_full.size()) {
    return result;
  }
  result.push_back({
      .title = expansions_range.size() > 1
                   ? "Expand all AUTOs in selected range"
                   : "Expand this AUTO",
      .kind = "refactor.rewrite",
      .edit = {.changes = {{p.textDocument.uri,
                            ConvertAutoExpansionsToFormattedTextEdits(
                                text_structure, expansions_range)}}},
  });

  AutoExpander kind_expander(text_structure, symbol_table_handler, auto_kinds);
  const auto &expansions_kind = kind_expander.Expand();
  if (expansions_kind.empty() ||
      expansions_kind.size() == expansions_range.size()) {
    return result;
  }
  result.push_back({
      .title = expansions_range.size() > 1
                   ? "Expand all AUTOs of same kinds as selected"
                   : "Expand all AUTOs of same kind as this one",
      .kind = "refactor.rewrite",
      .edit = {.changes = {{p.textDocument.uri,
                            ConvertAutoExpansionsToFormattedTextEdits(
                                text_structure, expansions_kind)}}},
  });
  return result;
}

}  // namespace verilog
