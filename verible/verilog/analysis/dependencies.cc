// Copyright 2017-2020 The Verible Authors.
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

#include "verible/verilog/analysis/dependencies.h"

#include <functional>
#include <iostream>

#include "absl/strings/string_view.h"
#include "verible/common/strings/display-utils.h"
#include "verible/common/util/logging.h"
#include "verible/verilog/analysis/symbol-table.h"
#include "verible/verilog/analysis/verilog-project.h"

namespace verilog {

static FileDependencies::symbol_index_type CreateSymbolMapFromSymbolTable(
    const SymbolTableNode &root, const VerilogProject *project) {
  VLOG(1) << __FUNCTION__ << ": collecting definitions";
  CHECK(project != nullptr)
      << "VerilogProject* is required for dependency analysis.";
  FileDependencies::symbol_index_type symbols_index;

  using SymbolData = FileDependencies::SymbolData;

  // Collect definers of root-level symbols.
  for (const SymbolTableNode::key_value_type &child : root) {
    const absl::string_view symbol_name(child.first);
    const VerilogSourceFile *file_origin = child.second.Value().file_origin;
    if (file_origin == nullptr) continue;

    SymbolData &symbol_data(symbols_index[symbol_name]);
    if (symbol_data.definer == nullptr) {
      // Take the first definition, arbitrarily.
      symbol_data.definer = file_origin;
    }
  }

  // Collect all unqualified and unresolved references from all scopes.
  VLOG(1) << __FUNCTION__ << ": collecting references";
  root.ApplyPreOrder([&symbols_index, project](const SymbolTableNode &node) {
    const SymbolInfo &symbol_info(node.Value());
    for (const DependentReferences &ref :
         symbol_info.local_references_to_bind) {
      // Only look at the root reference node, which is unqualified.
      const ReferenceComponent &ref_comp(ref.components->Value());
      const absl::string_view ref_id(ref_comp.identifier);
      VLOG(2) << "  referenced id: " << ref_id;

      const VerilogSourceFile *ref_file_origin =
          project->LookupFileOrigin(ref_id);
      if (ref_file_origin == nullptr) continue;  // unknown file

      if (ref_comp.resolved_symbol != nullptr) {
        const VerilogSourceFile *def_file_origin =
            ref_comp.resolved_symbol->Value().file_origin;
        if (ref_file_origin == def_file_origin) {
          continue;  // skip already resolved symbols in same file
        }
      }

      VLOG(2) << "  registering reference edge";
      SymbolData &symbol_data(symbols_index[ref_id]);
      symbol_data.referencers.insert(ref_file_origin);
    }
  });
  VLOG(1) << "end of " << __FUNCTION__;
  return symbols_index;  // move
}

// Struct for printing readable dependency edge.
struct DepEdge {
  const VerilogSourceFile *const ref;
  const VerilogSourceFile *const def;
  const FileDependencies::SymbolNameSet &symbols;
};

static std::ostream &operator<<(std::ostream &stream, const DepEdge &dep) {
  return stream << '"' << dep.ref->ReferencedPath() << "\" depends on \""
                << dep.def->ReferencedPath() << "\" for symbols "
                << verible::SequenceFormatter(dep.symbols, ", ", "{ ", " }");
}

static FileDependencies::file_deps_graph_type
CreateFileDependenciesFromSymbolMap(
    const FileDependencies::symbol_index_type &symbol_map) {
  VLOG(1) << __FUNCTION__;
  FileDependencies::file_deps_graph_type file_deps;
  for (const auto &symbol_entry : symbol_map) {
    const absl::string_view symbol_name(symbol_entry.first);
    const FileDependencies::SymbolData &symbol_info(symbol_entry.second);
    const VerilogSourceFile *def = symbol_info.definer;
    // If no definition is found, then do not create any edges for it.
    if (def == nullptr) continue;

    for (const VerilogSourceFile *ref : symbol_info.referencers) {
      // Skip self-edges.
      if (ref == def) continue;
      VLOG(2) << DepEdge{.ref = ref, .def = def, .symbols = {symbol_name}};
      file_deps[ref][def].insert(symbol_name);
    }
  }
  VLOG(1) << "end of " << __FUNCTION__;
  return file_deps;  // move
}

FileDependencies::FileDependencies(const SymbolTable &symbol_table)
    : root_symbols_index(CreateSymbolMapFromSymbolTable(
          symbol_table.Root(), symbol_table.Project())),
      file_deps(CreateFileDependenciesFromSymbolMap(root_symbols_index)) {
  // All the work is done by the initializers.
}

bool FileDependencies::Empty() const {
  for (const auto &ref : file_deps) {
    for (const auto &def : ref.second) {
      if (!def.second.empty()) return false;
    }
  }
  return true;
}

void FileDependencies::TraverseDependencyEdges(
    const std::function<void(const node_type &, const node_type &,
                             const SymbolNameSet &)> &edge_func) const {
  for (const auto &tail : file_deps) {
    for (const auto &head : tail.second) {
      edge_func(tail.first, head.first, head.second);
    }
  }
}

std::ostream &FileDependencies::PrintGraph(std::ostream &stream) const {
  TraverseDependencyEdges([&stream](const node_type &ref, const node_type &def,
                                    const SymbolNameSet &symbols) {
    stream << DepEdge{.ref = ref, .def = def, .symbols = symbols} << std::endl;
  });
  return stream;
}

std::ostream &operator<<(std::ostream &stream, const FileDependencies &deps) {
  return deps.PrintGraph(stream);
}

}  // namespace verilog
