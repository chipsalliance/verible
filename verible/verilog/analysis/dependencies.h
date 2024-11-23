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

#ifndef VERIBLE_VERILOG_ANALYSIS_DEPENDENCIES_H_
#define VERIBLE_VERILOG_ANALYSIS_DEPENDENCIES_H_

#include <functional>
#include <iosfwd>
#include <map>
#include <set>

#include "absl/strings/string_view.h"
#include "verible/common/strings/compare.h"
#include "verible/verilog/analysis/symbol-table.h"
#include "verible/verilog/analysis/verilog-project.h"

namespace verilog {

// Graph of inter-file dependencies based on what root-level symbols are defined
// and referenced.
// All data members are initialized only and const and publicly accessible.
// All VerilogSourceFile pointers herein are non-nullptr.
// Internal associative structures are ordered, for determinism and linear-time
// merge/set operations.
struct FileDependencies {
  // === Types

  // A node is represented as a source file.
  // Edges are just a pair of nodes.
  using node_type = const VerilogSourceFile *;

  // A set of strings, whose memory is owned outside of this data structure.
  using SymbolNameSet = std::set<absl::string_view, verible::StringViewCompare>;

  // Sort by referenced file name.
  using FileCompare = VerilogSourceFile::Less;

  // The outer key is a referencing file [R].
  // The inner key is a defining file [D].
  // The inner value is a collection of symbols that the referencing file
  //   expects from the defining file [{S}].
  // Combined, this represents files R that depend on files D for defining
  // symbols {S}.
  using file_deps_graph_type =
      std::map<node_type, std::map<node_type, SymbolNameSet, FileCompare>,
               FileCompare>;

  struct SymbolData {
    // Which file defines this symbol (first)?
    const VerilogSourceFile *definer = nullptr;

    // Which files might reference this symbol?
    // Elements are never nullptr.
    std::set<const VerilogSourceFile *, FileCompare> referencers;
  };

  // Map of symbol name to definition and references (files).
  // string_view keys must be backed by memory that outlives this class's
  // objects.  Typically, this is owned by VerilogSourceFile inside
  // VerilogProject.
  using symbol_index_type =
      std::map<absl::string_view, SymbolData, verible::StringViewCompare>;

  // === Fields (in order of initialization and computation)
  // All fields are const-initialized and public.

  // Tracks where symbols are defined and referenced
  const symbol_index_type root_symbols_index;

  // Adjacency-list representation of dependency graph.
  const file_deps_graph_type file_deps;

  // === Methods

  // Extract dependency information from a symbol table.
  // The symbol table only needs to be built (.Build()), and need not be
  // Resolve()d.
  // Once initialized, all data members are const.
  explicit FileDependencies(const SymbolTable &symbol_table);

  bool Empty() const;

  // Visit every edge with a function.
  // This can print or export data.
  void TraverseDependencyEdges(
      const std::function<void(const node_type &, const node_type &,
                               const SymbolNameSet &)> &edge_func) const;

  std::ostream &PrintGraph(std::ostream &) const;

  // TODO: print unresolved references (no definition found)
};

std::ostream &operator<<(std::ostream &, const FileDependencies &);

}  // namespace verilog

#endif  // VERIBLE_VERILOG_ANALYSIS_DEPENDENCIES_H_
