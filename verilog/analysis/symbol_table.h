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

#ifndef VERIBLE_VERILOG_ANALYSIS_SYMBOL_TABLE_H_
#define VERIBLE_VERILOG_ANALYSIS_SYMBOL_TABLE_H_

#include <map>

#include "absl/strings/string_view.h"
#include "common/strings/compare.h"
#include "common/text/symbol.h"
#include "common/util/map_tree.h"
#include "verilog/analysis/verilog_project.h"

namespace verilog {

struct SymbolInfo;  // forward declaration, defined below

// SymbolTableNode represents a named element in the syntax.
// When it represents a scope, it may have named subtrees.
using SymbolTableNode =
    verible::MapTree<absl::string_view, SymbolInfo, verible::StringViewCompare>;

// Classify what type of element a particular symbol is defining.
enum class SymbolType {
  kClass,
  kModule,
  kPackage,
  kTypeAlias,
  kDataNetVariableInstance,
  kFunction,
  kTask,
  kInterface,
};

// This data type holds information about what each SystemVerilog symbol is.
// An alternative implementation could be done using an abstract base class,
// and subclasses for each element type.
struct SymbolInfo {
  // What is this symbol? (package, module, type, variable, etc.)
  SymbolType type;

  // In which file is this considered "defined"?
  const VerilogSourceFile* file_origin = nullptr;

  // Pointer to the syntax tree origin.
  const verible::Symbol* syntax_origin = nullptr;

  // What is the type associated with this symbol?
  // Only applicable to typed elements: variables, nets, instances, etc.
  // This is only set after resolving type references.
  const SymbolTableNode* declared_type = nullptr;

  // TODO(fangism): symbol attributes
  // visibility: is this symbol (as a member of its parent) public?
  //   ports and parameters can be public, whereas local variables are not.
  // Is this class-member static or non-static?
  // Is this definition complete or only a forward declaration?

  // TODO(fangism): imported symbols and namespaces
  // Applicable to packages, classes, modules.
  // These should be looked up when searching for symbols (without copying).
};

// This map type represents the global namespace of preprocessor macro
// definitions.
// TODO(fangism): this should come from a preprocessor
using MacroSymbolMap =
    std::map<absl::string_view, SymbolInfo, verible::StringViewCompare>;

// SymbolTable maintains a named hierarchy of named symbols and scopes for
// SystemVerilog.
class SymbolTable {
 public:
  // If 'project' is nullptr, caller assumes responsibility for managing files
  // and string memory, otherwise string memory is owned by 'project'.
  explicit SymbolTable(VerilogProject* project) : project_(project) {}

  const SymbolTableNode& Root() const { return symbol_table_root_; }
  SymbolTableNode& MutableRoot() { return symbol_table_root_; }

  // TODO(fangism): merge operation, to be done before any symbol resolution

 private:
  // This owns all string_views inside the symbol table and outlives objects of
  // this class.
  // The project needs to be mutable for the sake of opening `include-d files
  // encountered during tree traversal.
  // TODO(fangism): once a preprocessor is implemented, includes can be expanded
  // before symbol table construction, and this can become read-only.
  VerilogProject* const project_ = nullptr;

  // Global symbol table root for SystemVerilog language elements:
  // modules, packages, classes, tasks, functions, interfaces, etc.
  // Known limitation: All of the above elements share the same namespace,
  // but the language actually compartmentalizes namespaces of some element
  // types.
  SymbolTableNode symbol_table_root_;

  // All macro definitions/references interact through this global namespace.
  MacroSymbolMap macro_symbols_;
};

// Construct a partial symbol table from a single source file.
std::vector<absl::Status> BuildSymbolTable(const VerilogSourceFile&,
                                           SymbolTable*);

}  // namespace verilog

#endif  // VERIBLE_VERILOG_ANALYSIS_SYMBOL_TABLE_H_
