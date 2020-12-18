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

#include <iosfwd>
#include <map>

#include "absl/status/status.h"
#include "absl/strings/string_view.h"
#include "common/strings/compare.h"
#include "common/text/symbol.h"
#include "common/util/map_tree.h"
#include "common/util/vector_tree.h"
#include "verilog/analysis/verilog_project.h"

namespace verilog {

struct SymbolInfo;  // forward declaration, defined below

// SymbolTableNode represents a named element in the syntax.
// When it represents a scope, it may have named subtrees.
using SymbolTableNode =
    verible::MapTree<absl::string_view, SymbolInfo, verible::StringViewCompare>;

std::ostream& SymbolTableNodeFullPath(std::ostream&, const SymbolTableNode&);

// Classify what type of element a particular symbol is defining.
enum class SymbolType {
  kClass,
  kModule,
  kPackage,
  kParameter,
  kTypeAlias,
  kDataNetVariableInstance,
  kFunction,
  kTask,
  kInterface,
};

// This classifies the type of reference that a single identifier is.
enum class ReferenceType {
  // The base identifier in any chain.
  // These symbols are resolved by potentially searching up-scope from the
  // current context.
  kUnqualified,

  // ::id (for packages, and class static members)
  // These symbols are resolved by searching in the parent symbol's
  // context (or its imported/inherited namespaces).
  kStaticMember,

  // .id (for object of struct/class type members)
  // These symbols are resolved by searching in the parent object's *type* scope
  // (or its imported/inherited namespaces).
  kObjectMember,
};

std::ostream& operator<<(std::ostream&, ReferenceType);

// References may form "trees" of dependencies (ReferenceComponentNode).
// ReferenceComponent is the data portion of each node an a reference tree.
// The overall tree structure drives the order in which references are resolved.
//
// Examples:
//   * "a::b" and "a.b", resolving "b" depends on resolving "a" first.
//   * "foo bar(.x(y));" resolving "x" depends on typeof(bar) -> resolving
//        "foo", but "y" is only resolved using local context, and forms its own
//        independent reference chain.
struct ReferenceComponent {
  // This is the token substring of the identifier being referenced.
  // String memory is expected to be owned by a VerilogSourceFile.
  const absl::string_view identifier;

  // What kind of reference is this, and how should it be resolved?
  // See enum definition above.
  const ReferenceType ref_type;

  // This points to the definition with which this symbol was resolved.
  // During symbol table construction, this remains nullptr.
  // If symbol resolution succeeds, this will be updated to non-nullptr.
  // This may remain as nullptr if symbol resolution fails.
  // Symbol table merge operations may invalidate these pointers, so make sure
  // merges are done before attempting symbol resolution.
  const SymbolTableNode* resolved_symbol = nullptr;

 public:
  ReferenceComponent(const ReferenceComponent&) = default;  // safe to copy
  ReferenceComponent(ReferenceComponent&&) = default;
  ReferenceComponent& operator=(const ReferenceComponent&) = delete;
  ReferenceComponent& operator=(ReferenceComponent&&) = delete;

  // Structural consistency check.
  void VerifySymbolTableRoot(const SymbolTableNode* root) const;
};

std::ostream& operator<<(std::ostream&, const ReferenceComponent&);

// A node in a tree of *dependent* hierchical references.
// An expression like "x.y.z" will form a linear chain, where resolving 'y'
// depends on 'x', and resolving 'z' depends on 'y'. Named ports are manifest as
// wide nodes: in "f(.a(...), .b(...))", both 'a' and 'b' depend on resolving
// 'f' (and thus are siblings).
using ReferenceComponentNode = verible::VectorTree<ReferenceComponent>;

// Human-readable representation of a node's path from root.
std::ostream& ReferenceNodeFullPath(std::ostream&,
                                    const ReferenceComponentNode&);

// View a ReferenceComponentNode's children as an ordered map, keyed by
// reference string. A single node may have multiple references to the same
// child id, and they are expected to resolve the same way, so one of them is
// arbitrarily chosen to be included in the map.
// Primarily for debugging and visualization.
using ReferenceComponentMap =
    std::map<absl::string_view, const ReferenceComponentNode*,
             verible::StringViewCompare>;
ReferenceComponentMap ReferenceComponentNodeMapView(
    const ReferenceComponentNode&);

// Represents any (chained) qualified or unqualified reference.
struct DependentReferences {
  // Sequence of identifiers in a chain like "a.b.c", or "x::y::z".
  // The first element always has ReferenceType::kUnqualified.
  // This is wrapped in a unique_ptr to guarantee address stability on-move.
  std::unique_ptr<ReferenceComponentNode> components;

 public:
  DependentReferences() = default;
  // move-only
  DependentReferences(const DependentReferences&) = delete;
  DependentReferences(DependentReferences&&) = default;
  DependentReferences& operator=(const DependentReferences&) = delete;
  DependentReferences& operator=(DependentReferences&&) = delete;

  // Returns true of no references were collected.
  bool Empty() const { return components == nullptr; }

  // Returns the current terminal descendant.
  const ReferenceComponentNode* LastLeaf() const;

  // When traversing an unqualified or qualified reference, use this to grow a
  // new leaf node in the reference tree.
  void PushReferenceComponent(const ReferenceComponent& component);

  // Structural consistency check.
  void VerifySymbolTableRoot(const SymbolTableNode* root) const;

  // Attempt to resolve all symbol references.
  void Resolve(const SymbolTableNode& context,
               std::vector<absl::Status>* diagnostics);
};

// Contains information about a type used to declare data/instances/variables.
struct DeclarationTypeInfo {
  // Pointer to the syntax tree origin, e.g. a NodeEnum::kDataType node.
  // This is useful for diagnostic that detail the relevant text,
  // which can be recovered by StringSpanOfSymbol(const verible::Symbol&).
  const verible::Symbol* syntax_origin = nullptr;

  // Pointer to the reference node that represents a user-defined type, if
  // applicable.
  // For built-in and primitive types, this is left as nullptr.
  // This will be used to lookup named members of instances/objects of this
  // type.
  //
  // These pointers should point to an element inside a
  // DependentReferences::components in the same SymbolTable.
  // (Note: to verify this membership at run-time could be expensive because it
  // would require checking all DeclarationTypeInfos in the SymbolTable
  // hierarchy.)
  //
  // This pointer must remain stable, even as reference trees grow, which
  // mandates reserve()-ing ReferenceComponentNodes' children one-time in
  // advance, and only ever moving ReferenceComponents, never copying them.
  const ReferenceComponentNode* user_defined_type = nullptr;

 public:
  DeclarationTypeInfo() = default;

  // copy-able, move-able, assignable
  DeclarationTypeInfo(const DeclarationTypeInfo&) = default;
  DeclarationTypeInfo(DeclarationTypeInfo&&) = default;
  DeclarationTypeInfo& operator=(const DeclarationTypeInfo&) = default;
  DeclarationTypeInfo& operator=(DeclarationTypeInfo&&) = default;

  // Structural consistency check.
  void VerifySymbolTableRoot(const SymbolTableNode* root) const;
};

// This data type holds information about what each SystemVerilog symbol is.
// An alternative implementation could be done using an abstract base class,
// and subclasses for each element type.
struct SymbolInfo {
  // What is this symbol? (package, module, type, variable, etc.)
  SymbolType type;

  // This symbol's name is stored in the .Key() of the SymbolTableNode that
  // encloses this structure.  (This is SymbolTableNode::Value().)

  // In which file is this considered "defined"?
  // Technically, this can be recovered by looking up a string_view range of
  // text in VerilogProject.
  const VerilogSourceFile* file_origin = nullptr;

  // Pointer to the syntax tree origin.
  // Reminder: Parts of the syntax tree may originate from included files.
  const verible::Symbol* syntax_origin = nullptr;

  // What is the type associated with this symbol?
  // Only applicable to typed elements: variables, nets, instances, etc.
  // This is only set after resolving type references.
  DeclarationTypeInfo declared_type;

  // TODO(fangism): symbol attributes
  // visibility: is this symbol (as a member of its parent) public?
  //   ports and parameters can be public, whereas local variables are not.
  // Is this class-member static or non-static?
  // Is this definition complete or only a forward declaration?

  // TODO(fangism): imported symbols and namespaces
  // Applicable to packages, classes, modules.
  // These should be looked up when searching for symbols (without copying).

  // Collection of references to resolve and bind that appear in the same
  // context. There is no sequential ordering dependency among these references,
  // theoretically, they could all be resolved in parallel.
  std::vector<DependentReferences> local_references_to_bind;
  // TODO(fangism): make this searchable by substring offsets.

  // TODO(fangism): cache/memoize the resolution of this node so that resolution
  // can be triggered on-demand, and also avoid duplicate work.

 public:  // methods
  SymbolInfo() = default;

  // move-only
  SymbolInfo(const SymbolInfo&) = delete;
  SymbolInfo(SymbolInfo&&) = default;
  SymbolInfo& operator=(const SymbolInfo&) = delete;
  SymbolInfo& operator=(SymbolInfo&&) = delete;

  // Attempt to resolve all symbol references.
  void Resolve(const SymbolTableNode& context,
               std::vector<absl::Status>* diagnostics);

  // Internal consistency check.
  void VerifySymbolTableRoot(const SymbolTableNode* root) const;
};

// This map type represents the global namespace of preprocessor macro
// definitions.
// TODO(fangism): This should come from a preprocessor and possibly maintained
// per translation-unit in multi-file compilation mode.
using MacroSymbolMap =
    std::map<absl::string_view, SymbolInfo, verible::StringViewCompare>;

// SymbolTable maintains a named hierarchy of named symbols and scopes for
// SystemVerilog.  This can be built up separately per translation unit,
// or in a unified table across all translation units.
class SymbolTable {
 public:
  class Builder;  // implementation detail
  class Tester;   // test-only

 public:
  // If 'project' is nullptr, caller assumes responsibility for managing files
  // and string memory, otherwise string memory is owned by 'project'.
  explicit SymbolTable(VerilogProject* project) : project_(project) {}

  // can become move-able when needed
  SymbolTable(const SymbolTable&) = delete;
  SymbolTable(SymbolTable&&) = delete;
  SymbolTable& operator=(const SymbolTable&) = delete;
  SymbolTable& operator=(SymbolTable&&) = delete;

  ~SymbolTable() { CheckIntegrity(); }

  const SymbolTableNode& Root() const { return symbol_table_root_; }

  // TODO(fangism): multi-translation-unit merge operation,
  // to be done before any symbol resolution

  // Lookup all symbol references, and bind references where successful.
  // Only attempt to resolve after merging symbol tables.
  void Resolve(std::vector<absl::Status>* diagnostics);

 protected:  // methods
  // Direct mutation is only intended for the Builder implementation.
  SymbolTableNode& MutableRoot() { return symbol_table_root_; }

  // Verify internal structural and pointer consistency.
  void CheckIntegrity() const;

 private:  // data
  // This owns all string_views inside the symbol table and outlives objects of
  // this class.
  // The project needs to be mutable for the sake of opening `include-d files
  // encountered during tree traversal.
  // TODO(fangism): once a preprocessor is implemented, includes can be expanded
  // before symbol table construction, and this can become read-only.
  VerilogProject* const project_;

  // Global symbol table root for SystemVerilog language elements:
  // modules, packages, classes, tasks, functions, interfaces, etc.
  // Known limitation: All of the above elements share the same namespace,
  // but the language actually compartmentalizes namespaces of some element
  // types.
  SymbolTableNode symbol_table_root_;

  // All macro definitions/references interact through this global namespace.
  MacroSymbolMap macro_symbols_;
};

// Construct a partial symbol table and bindings locations from a single source
// file.  This does not actually resolve symbol references, there is an
// opportunity to merge symbol tables across files before resolving references.
std::vector<absl::Status> BuildSymbolTable(const VerilogSourceFile&,
                                           SymbolTable*);

}  // namespace verilog

#endif  // VERIBLE_VERILOG_ANALYSIS_SYMBOL_TABLE_H_
