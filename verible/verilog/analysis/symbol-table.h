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

#include <cstddef>
#include <functional>
#include <iosfwd>
#include <map>
#include <memory>
#include <set>
#include <string>
#include <utility>
#include <vector>

#include "absl/status/status.h"
#include "absl/status/statusor.h"
#include "absl/strings/string_view.h"
#include "verible/common/strings/compare.h"
#include "verible/common/text/symbol.h"
#include "verible/common/util/map-tree.h"
#include "verible/common/util/vector-tree.h"
#include "verible/verilog/analysis/verilog-project.h"

namespace verilog {

struct SymbolInfo;  // forward declaration, defined below

// SymbolTableNode represents a named element in the syntax.
// When it represents a scope, it may have named subtrees.
// The string_view key carries positional information, it corresponds to a
// substring owned by a VerilogSourceFile (which must outlive the symbol table),
// and can be used to look up file origin and position within file.
using SymbolTableNode =
    verible::MapTree<absl::string_view, SymbolInfo, verible::StringViewCompare>;

std::ostream &SymbolTableNodeFullPath(std::ostream &, const SymbolTableNode &);

// Classify what type of element a particular symbol is defining.
enum class SymbolMetaType {
  kRoot,
  kClass,
  kModule,
  kGenerate,  // loop or conditional generate block
  kPackage,
  kParameter,
  kTypeAlias,  // typedef
  kDataNetVariableInstance,
  kFunction,  // includes constructors
  kTask,
  kStruct,
  kEnumType,
  kEnumConstant,
  kInterface,

  // The following enums represent classes/groups of the above types,
  // and are used for validating metatypes of symbol references.

  kUnspecified,  // matches all of the above (any type)
  kCallable,     // matches only kFunction or kTask
};

std::ostream &operator<<(std::ostream &, SymbolMetaType);

absl::string_view SymbolMetaTypeAsString(SymbolMetaType type);

// This classifies the type of reference that a single identifier is.
enum class ReferenceType {
  // The base identifier in any chain.
  // These symbols are resolved by potentially searching up-scope from the
  // current context.
  kUnqualified,

  // Similar to kUnqualified in that it is in base position, however, this
  // symbol must be resolved only locally in the current context, without any
  // upward search.  This is suitable for out-of-line definitions, where the
  // base (in "base::member") must be resolved in only the enclosing scope.
  kImmediate,

  // ::id (for packages, and class static members)
  // These symbols are resolved by searching in the parent symbol's
  // context (or its imported/inherited namespaces).
  kDirectMember,

  // .id (for object of struct/class type members)
  // These symbols are resolved by searching in the parent object's *type* scope
  // (or its imported/inherited namespaces).
  kMemberOfTypeOfParent,
};

std::ostream &operator<<(std::ostream &, ReferenceType);

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
  // This string_view also carries positional information: it can be used to
  // locate the originating VerilogSourceFile via
  // VerilogProject::LookupFileOrigin() and in-file position from the file's
  // LineColumnMap.
  const absl::string_view identifier;

  // What kind of reference is this, and how should it be resolved?
  // See enum definition above.
  const ReferenceType ref_type;

  // Inform the symbol resolver that this symbol must be of a certain metatype.
  // SymbolInfo::kUnspecified is interpreted as "any metatype" for cases
  // where it is not known in advance.
  const SymbolMetaType required_metatype = SymbolMetaType::kUnspecified;

  // This points to the definition with which this symbol was resolved.
  // During symbol table construction, this remains nullptr.
  // If symbol resolution succeeds, this will be updated to non-nullptr.
  // This may remain as nullptr if symbol resolution fails.
  // Symbol table merge operations may invalidate these pointers, so make sure
  // merges are done before attempting symbol resolution.
  // This should only be set by ResolveSymbol().
  // TODO: privatize this member.
  const SymbolTableNode *resolved_symbol = nullptr;

 public:
  absl::Status MatchesMetatype(SymbolMetaType) const;

  // Resolves this symbol and verifies that metatypes are compatible, which is
  // reflected in the returned Status.
  absl::Status ResolveSymbol(const SymbolTableNode &);

  // Only print ref_type and identifier.
  std::ostream &PrintPathComponent(std::ostream &) const;

  // Print everything, showing symbol path if it is resolved.
  std::ostream &PrintVerbose(std::ostream &) const;

  // Structural consistency check.
  void VerifySymbolTableRoot(const SymbolTableNode *root) const;
};

std::ostream &operator<<(std::ostream &, const ReferenceComponent &);

// A node in a tree of *dependent* hierchical references.
// An expression like "x.y.z" will form a linear chain, where resolving 'y'
// depends on 'x', and resolving 'z' depends on 'y'. Named ports are manifest as
// wide nodes: in "f(.a(...), .b(...))", both 'a' and 'b' depend on resolving
// 'f' (and thus are siblings).
using ReferenceComponentNode = verible::VectorTree<ReferenceComponent>;

// Human-readable representation of a node's path from root.
std::ostream &ReferenceNodeFullPath(std::ostream &,
                                    const ReferenceComponentNode &);

// View a ReferenceComponentNode's children as an ordered map, keyed by
// reference string. A single node may have multiple references to the same
// child id, and they are expected to resolve the same way, so one of them is
// arbitrarily chosen to be included in the map, while the others are dropped.
// Primarily for debugging and visualization.
using ReferenceComponentMap =
    std::map<absl::string_view, const ReferenceComponentNode *,
             verible::StringViewCompare>;
ReferenceComponentMap ReferenceComponentNodeMapView(
    const ReferenceComponentNode &);

// Represents any (chained) qualified or unqualified reference.
struct DependentReferences {
  // Sequence of identifiers in a chain like "a.b.c", or "x::y::z".
  // The first element always has ReferenceType::kUnqualified.
  // This is wrapped in a unique_ptr to guarantee address stability on-move.
  std::unique_ptr<ReferenceComponentNode> components;

 public:
  DependentReferences() = default;
  explicit DependentReferences(
      std::unique_ptr<ReferenceComponentNode> components)
      : components(std::move(components)) {}
  // move-only
  DependentReferences(const DependentReferences &) = delete;
  DependentReferences(DependentReferences &&) = default;
  DependentReferences &operator=(const DependentReferences &) = delete;
  DependentReferences &operator=(DependentReferences &&) = delete;

  // Returns true of no references were collected.
  bool Empty() const { return components == nullptr; }

  // Returns the current terminal descendant.
  const ReferenceComponentNode *LastLeaf() const;

  // Returns the last type component of a reference tree.
  // e.g. from "A#(.B())::C#(.D())" -> "C"
  const ReferenceComponentNode *LastTypeComponent() const;
  ReferenceComponentNode *LastTypeComponent();

  // Structural consistency check.
  // When traversing an unqualified or qualified reference, use this to grow a
  // new leaf node in the reference tree.
  // Returns a pointer to the new node.
  ReferenceComponentNode *PushReferenceComponent(
      const ReferenceComponent &component);

  // Structural consistency check.
  void VerifySymbolTableRoot(const SymbolTableNode *root) const;

  // Attempt to resolve all symbol references.
  void Resolve(const SymbolTableNode &context,
               std::vector<absl::Status> *diagnostics) const;

  // Attempt to resolve only local symbol references.
  void ResolveLocally(const SymbolTableNode &context) const;

  // Attempt to only resolve the base of the reference (the first component).
  absl::StatusOr<SymbolTableNode *> ResolveOnlyBaseLocally(
      SymbolTableNode *context);
};

std::ostream &operator<<(std::ostream &, const DependentReferences &);

// Contains information about a type used to declare data/instances/variables.
struct DeclarationTypeInfo {
  // Pointer to the syntax tree origin, e.g. a NodeEnum::kDataType node.
  // This is useful for diagnostic that detail the relevant text,
  // which can be recovered by StringSpanOfSymbol(const verible::Symbol&).
  const verible::Symbol *syntax_origin = nullptr;

  // holds optional string_view describing direction of the port
  absl::string_view direction;

  // holds additional type specifications, used mostly in multiline definitions
  // of ports
  std::vector<const verible::Symbol *> type_specifications;

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
  const ReferenceComponentNode *user_defined_type = nullptr;

  // Indicates that this is implicit declaration.
  // FIXME(ldk): Check if this could be replaced by user_defined_type pointing
  //    to default implicit type.
  bool implicit = false;

 public:
  // Structural consistency check.
  void VerifySymbolTableRoot(const SymbolTableNode *root) const;
};

std::ostream &operator<<(std::ostream &, const DeclarationTypeInfo &);

// This data type holds information about what each SystemVerilog symbol is.
// An alternative implementation could be done using an abstract base class,
// and subclasses for each element type.
struct SymbolInfo {
  // What is this symbol? (package, module, type, variable, etc.)
  SymbolMetaType metatype;

  // This symbol's name is stored in the .Key() of the SymbolTableNode that
  // encloses this structure.  (This is SymbolTableNode::Value().)

  // In which file is this considered "defined"?
  // Technically, this can be recovered by looking up a string_view range of
  // text in VerilogProject (StringSpanOfSymbol(*syntax_origin)).
  const VerilogSourceFile *file_origin = nullptr;

  // Pointer to the syntax tree origin.
  // An easy way to view this text is StringSpanOfSymbol(*syntax_origin).
  // Reminder: Parts of the syntax tree may originate from included files.
  const verible::Symbol *syntax_origin = nullptr;

  // vector to additional definition entries, e.g. for port definitions
  // TODO (glatosinski): I guess we should include more information here rather
  // than just string_view pointing to the symbol, or add string_view pointing
  // to the symbol in the Symbol class
  std::vector<absl::string_view> supplement_definitions;

  // bool telling if the given symbol is a port identifier
  bool is_port_identifier = false;

  // What is the type associated with this symbol?
  // Only applicable to typed elements: variables, nets, instances,
  // typedefs, etc.
  // This is only set after resolving type references.
  DeclarationTypeInfo declared_type;

  // Collection of generated scope names that exists for the sake of persistent
  // string memory storage (since all other symbol table node keys rely on
  // string_views that belong the source file's string memory buffer).
  // We cannot use std::string directly due to the move-ability requirements
  // along with the possibility of short-string-optimization.
  // std::vector is move-able on reallocation and unique_ptr guarantees
  // move-stability.
  std::vector<std::unique_ptr<const std::string>> anonymous_scope_names;

  // TODO(fangism): symbol attributes
  // visibility: is this symbol (as a member of its parent) public?
  //   ports and parameters can be public, whereas local variables are not.
  // Is this class-member static or non-static?
  // Is this definition complete or only a forward declaration?

  // TODO(fangism): imported symbols and namespaces
  // Applicable to packages, classes, modules.
  // These should be looked up when searching for symbols (without copying).

  // For elements with inheritance only: this points to a base class.
  // Currently limited to single-inheritance.
  // TODO: extend to support multiple-implementation and interface-class
  // multiple inheritance.
  DeclarationTypeInfo parent_type;

  // Collection of references to resolve and bind that appear in the same
  // context. There is no sequential ordering dependency among these references,
  // theoretically, they could all be resolved in parallel.
  // This does not need to be insertion-iterator-stable.
  std::vector<DependentReferences> local_references_to_bind;
  // TODO(fangism): make this searchable by substring offsets.

  // TODO(fangism): cache/memoize the resolution of this node so that resolution
  // can be triggered on-demand, and also avoid duplicate work.

 public:  // methods
  SymbolInfo() = default;
  explicit SymbolInfo(SymbolMetaType metatype,
                      const VerilogSourceFile *file_origin = {},
                      const verible::Symbol *syntax_origin = {},
                      DeclarationTypeInfo declared_type = {})
      : metatype(metatype),
        file_origin(file_origin),
        syntax_origin(syntax_origin),
        declared_type(std::move(declared_type)) {}

  // move-only
  SymbolInfo(const SymbolInfo &) = delete;
  SymbolInfo(SymbolInfo &&) = default;
  SymbolInfo &operator=(const SymbolInfo &) = delete;
  SymbolInfo &operator=(SymbolInfo &&) = delete;

  // Generate a scope name whose string memory lives and moves with this object.
  // 'base' is used as part of the generated name.
  absl::string_view CreateAnonymousScope(absl::string_view base);

  // Attempt to resolve all symbol references.
  void Resolve(const SymbolTableNode &context,
               std::vector<absl::Status> *diagnostics);

  // Attempt to resolve only symbols local to 'context' (no upward search).
  void ResolveLocally(const SymbolTableNode &context);

  // Internal consistency check.
  void VerifySymbolTableRoot(const SymbolTableNode *root) const;

  // Show definition info of this symbol.
  std::ostream &PrintDefinition(std::ostream &stream, size_t indent = 0) const;

  // Show references that are to be resolved starting with this node's scope.
  std::ostream &PrintReferences(std::ostream &stream, size_t indent = 0) const;

  // Functor to compare string starting address, for positional sorting.
  struct StringAddressCompare {
    using is_transparent = void;  // heterogeneous lookup

    static absl::string_view ToString(absl::string_view s) { return s; }
    static absl::string_view ToString(const DependentReferences *ref) {
      return ref->components->Value().identifier;
    }

    template <typename L, typename R>
    bool operator()(L l, R r) const {
      static constexpr std::less<const void *> compare_address;
      return compare_address(ToString(l).begin(), ToString(r).begin());
    }
  };

  using address_ordered_set_type =
      std::set<const DependentReferences *, StringAddressCompare>;

  using references_map_view_type =
      std::map<absl::string_view, address_ordered_set_type,
               verible::StringViewCompare>;

  // For testing only, quickly find reference candidates by name, and positional
  // occurence.  The outer map is ordered by string contents, and the inner
  // associative container is ordered by substring memory address as a secondary
  // key. The nested map is storage-inefficient (compared to a flat
  // std::vector), and is only suitable for testing.
  references_map_view_type LocalReferencesMapViewForTesting() const;
};

// This map type represents the global namespace of preprocessor macro
// definitions.
// The string_view key should be a substring of text whose memory is owned by
// any VerilogSourceFile that defines the macro.
// TODO(fangism): This should come from a preprocessor and possibly maintained
// per translation-unit in multi-file compilation mode.
using MacroSymbolMap =
    std::map<absl::string_view, SymbolInfo, verible::StringViewCompare>;

// SymbolTable maintains a named hierarchy of named symbols and scopes for
// SystemVerilog.  This can be built up separately per translation unit,
// or in a unified table across all translation units.
//
// Typical usage:
//   VerilogProject project(...);
//   project.OpenTranslationUnit(...);  // open files in loop
//
//   SymbolTable symbol_table(&project);
//
//   std::vector<absl::Status> diagnostics;
//   symbol_table.Build(&diagnostics);
//   // ... optional work ...
//
//   symbol_table.Resolve(&diagnostics);
//   // report diagnostics
//   // navigate results from symbol_table.Root().
//
class SymbolTable {
 public:
  class Builder;  // implementation detail
  class Tester;   // test-only

 public:
  // If 'project' is nullptr, caller assumes responsibility for managing files
  // and string memory, otherwise string memory is owned by 'project'.
  explicit SymbolTable(VerilogProject *project)
      : project_(project),
        symbol_table_root_(SymbolInfo{SymbolMetaType::kRoot}) {}

  // can become move-able when needed
  SymbolTable(const SymbolTable &) = delete;
  SymbolTable(SymbolTable &&) = delete;
  SymbolTable &operator=(const SymbolTable &) = delete;
  SymbolTable &operator=(SymbolTable &&) = delete;

  ~SymbolTable() { CheckIntegrity(); }

  const SymbolTableNode &Root() const { return symbol_table_root_; }

  const VerilogProject *Project() const { return project_; }

  // TODO(fangism): multi-translation-unit merge operation,
  // to be done before any symbol resolution

  // Incrementally construct the symbol table from one translation unit.
  // This gives the user control over the ordering of processing.
  // It is safe to build the same unit multiple times, subsequent invocations
  // will not change the symbol table structure, but will give duplicate symbol
  // diagnostics.
  void BuildSingleTranslationUnit(absl::string_view referenced_file_name,
                                  std::vector<absl::Status> *diagnostics);

  // Construct symbol table definitions and references hierarchically, but do
  // not attempt to resolve the symbols.
  // The ordering of translation units processing is implementation defined,
  // and should not be relied upon, but this only maatters when there are
  // duplicate definitions among translation units.
  void Build(std::vector<absl::Status> *diagnostics);

  // Lookup all symbol references, and bind references where successful.
  // Only attempt to resolve after merging symbol tables.
  void Resolve(std::vector<absl::Status> *diagnostics);

  // A "weaker" version of Resolve() that only attempts to resolve symbol
  // references to definitions belonging to the same scope as the reference
  // (without upward search).
  // This can dramatically prune the number of unresolved references that
  // require root-level resolution.
  // No diagnostics are collected because silent no-change-on-failure behavior
  // is intended.
  void ResolveLocallyOnly();

  // Print only the information about symbols defined (no references).
  // This will print the results of Build().
  std::ostream &PrintSymbolDefinitions(std::ostream &) const;

  // Print only the information about symbol references, and possibly resolved
  // links to definitions.
  // This will print the results of Build() or Resolve().
  std::ostream &PrintSymbolReferences(std::ostream &) const;

 protected:  // methods
  // Direct mutation is only intended for the Builder implementation.
  SymbolTableNode &MutableRoot() { return symbol_table_root_; }

  // Verify internal structural and pointer consistency.
  void CheckIntegrity() const;

 private:  // data
  // This owns all files used to construct the symbol table and therefore,
  // owns all string_views inside the symbol table and outlives objects of
  // this class.
  // The project needs to be mutable for the sake of opening `include-d files
  // encountered during tree traversal.
  // TODO(fangism): once a preprocessor is implemented, includes can be expanded
  // before symbol table construction, and this can become read-only.
  VerilogProject *const project_;

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
// 'source' should already be Parse()d in advance, so that its syntax tree can
// be accessed.
// If 'project' is provided, then it can be used to open preprocessing-included
// files, otherwise include directives will be ignored.
std::vector<absl::Status> BuildSymbolTable(const VerilogSourceFile &source,
                                           SymbolTable *symbol_table,
                                           VerilogProject *project = nullptr);

}  // namespace verilog

#endif  // VERIBLE_VERILOG_ANALYSIS_SYMBOL_TABLE_H_
