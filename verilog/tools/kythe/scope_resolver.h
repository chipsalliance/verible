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

#ifndef VERIBLE_VERILOG_TOOLS_KYTHE_SCOPE_RESOLVER_H_
#define VERIBLE_VERILOG_TOOLS_KYTHE_SCOPE_RESOLVER_H_

#include <map>
#include <set>
#include <vector>

#include "absl/strings/string_view.h"
#include "common/util/auto_pop_stack.h"
#include "common/util/iterator_range.h"
#include "verilog/tools/kythe/kythe_facts.h"

namespace verilog {
namespace kythe {

// Used to wrap whatever needs to be recorded in a scope item.
struct ScopeMemberItem {
  ScopeMemberItem(const VName& vname) : vname(vname) {}

  bool operator==(const ScopeMemberItem& other) const;
  bool operator<(const ScopeMemberItem& other) const;

  // VName of this member.
  VName vname;
};

// Represents some scope in SystemVerilog code like class or module.
class Scope {
 public:
  Scope() = default;
  explicit Scope(const Signature& signature) : signature_(signature) {}

  // Appends the given VName to the members of this scope.
  void AddMemberItem(const ScopeMemberItem& member_item);

  // Appends the member of the given scope to the current scope.
  void AppendScope(const Scope& scope);

  const std::set<ScopeMemberItem>& Members() const { return members_; }
  const Signature& GetSignature() const { return signature_; }

  // Searches for the given reference_name in the current scope and returns its
  // VName or nullptr if not found.
  const VName* SearchForDefinition(absl::string_view name) const;

 private:
  // Signature of the owner of this scope.
  Signature signature_;

  // list of the members inside this scope.
  std::set<ScopeMemberItem> members_;
};

// Container with a stack of Scopes to hold the accessible scopes during
// traversing an Indexing Facts Tree.
// This is used to get the definitions of some reference.
//
// This is modified during tree traversal because in case of entering new
// scope the new scope is resolved first and after that it's added to the
// containing scope and the next scope is being analyzed.
// e.g
// package pkg;
//    int x;
//    function int my_fun();
//      return x;
//    endfunction
// endpackage
//
// Vertical scope when extracting my_fun:
// The Global Scope
// {
//    "pkg"
// }
// Pkg Scope
// {
//   pkg#x
// }
// when trying to find a definition for "x" in "return x" ScopeContext
// try to find the first matching definition starting from the innermost/nearest
// scope or returns a nullptr if not found.
class ScopeContext : public verible::AutoPopStack<Scope*> {
 public:
  typedef verible::AutoPopStack<Scope*> base_type;

  // member class to handle push and pop of stack safely
  using AutoPop = base_type::AutoPop;

  // returns the top VName of the stack
  Scope& top() { return *ABSL_DIE_IF_NULL(base_type::top()); }

  // TODO(minatoma): improve performance and memory for this function.
  //
  // This function uses string matching to find the definition of some
  // variable in reverse order of the current scopes.
  //
  // Improvement can be replacing the string matching to comparison based on
  // integers or enums and reshaping the scope to be one vector instead of
  // vector of vectors.
  //
  // Search function to get the VName of a definitions of some reference.
  // It loops over the scopes in reverse order and loops over every scope in
  // reverse order to find a definition for the variable with given prefix
  // signature.
  // e.g
  // {
  //    bar#module,
  //    foo#module,
  // }
  // {
  //    other scope,
  // }
  // Given bar#module it return the whole VName of that definition.
  // And if more than one match is found the first would be returned.
  const VName* SearchForDefinition(absl::string_view name) const;
};

// Keeps track and saves the explored scopes with a <key, value> and maps every
// signature to its scope.
//
// ScopeResolver saves the scopes generated while traversing the
// IndexingFactsTree so that they can be use to find some definition.
// Here the scopes are saved in a flattened manner instead of tree like
// hierarchy (in a structure which looks like symbol tables).
// e.g
// class m;
//    int x;
//    function int my_fun();
//        int x;
//        return x;
//    endfunction
// endclass
//
// The generated scope would be:
// {
//    "m": {
//      m#x,
//      m#my_fun
//    },
//    "my_fun": {
//      m#my_fun#x
//    }
// }
// this way definitions can be found using the signatures.
class ScopeResolver {
 public:
  ScopeResolver(const Signature& global_scope_signature,
                const ScopeResolver* previous_file_scope_resolver)
      : previous_file_scope_resolver_(previous_file_scope_resolver),
        global_scope_signature_(global_scope_signature) {}

  // TODO(minatoma): add overloaded function which takes anchors (and add
  // tests).
  // TODO(minatoma): returns scopes with VNames to decrease search time for
  // scopes.
  // Searches for the definitions of the given names.
  const std::vector<const VName*> SearchForDefinitions(
      const std::vector<std::string>& names) const;

  // Adds the VNames of the definitions it given scope to the scope context.
  void AppendScopeToScopeContext(const Scope& scope);

  // Adds a ScopeMemberItem of a definition to the scope context.
  void AddDefinitionToScopeContext(const ScopeMemberItem& new_member);

  // Searches for a scope with the given signature in the scopes.
  const Scope* SearchForScope(const Signature& signature) const;

  // Maps the given signature to the given scope.
  void MapSignatureToScope(const Signature& signature, const Scope& scope);

  // Maps the given signature to the scope of the other signature.
  void MapSignatureToScopeOfSignature(const Signature& signature,
                                      const Signature& other_signature);

  ScopeContext& GetMutableScopeContext() { return scope_context_; }

 private:
  // Searches for a definition with the given name in the scope context and if
  // not found searches the global scopes of the previous files' scopes (returns
  // nullptr if a definitions is not found).
  const VName* SearchForDefinitionInScopeContext(absl::string_view name) const;

  // Searches for a definition with the given name in the global scope of this
  // ScopeResolver.
  const VName* SearchForDefinitionInGlobalScope(absl::string_view name) const;

  // Searches for a definition with the given name in the scope with the given
  // signature.
  const VName* SearchForDefinitionInScope(const Signature& signature,
                                          absl::string_view name) const;

  // Keeps track of scopes and definitions inside the scopes of ancestors as
  // the visitor traverses the facts tree.
  ScopeContext scope_context_;

  // Saves signatures alongside with their inner members (scope).
  // This is used for resolving references to some variables after using
  // import pkg::*. or other member access like class_Type::my_var.
  // e.g
  // package pkg1;
  //   function my_fun(); endfunction
  //   class my_class; endclass
  // endpackage
  //
  // package pkg2;
  //   function my_fun(); endfunction
  //   class my_class; endclass
  // endpackage
  //
  // Creates the following:
  // {
  //   "pkg1": ["my_fun", "my_class"],
  //   "pkg2": ["my_fun", "my_class"]
  // }
  std::map<Signature, Scope> scopes_;

  // Pointer to the previous file's discovered scopes (if a previous file
  // exists). This is used for definition finding in cross-file referencing.
  // This forms a null-terminated singly-linked list across files.
  const ScopeResolver* previous_file_scope_resolver_;

  // The signature of the global scope of this ScopeResolver.
  const Signature global_scope_signature_;
};

}  // namespace kythe
}  // namespace verilog

#endif  // VERIBLE_VERILOG_TOOLS_KYTHE_SCOPE_RESOLVER_H_
