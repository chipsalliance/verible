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
#include <vector>

#include "absl/strings/string_view.h"
#include "common/util/auto_pop_stack.h"
#include "common/util/iterator_range.h"
#include "verilog/tools/kythe/kythe_facts.h"

namespace verilog {
namespace kythe {

// Used to wrap whatever needs to be recored in a scope item.
struct ScopeMemberItem {
  ScopeMemberItem(const VName& vname) : vname(vname) {}

  // VNames of the members inside this scope.
  VName vname;
};

// Represents some scope in SystemVerilog code like class or module.
class Scope {
 public:
  Scope() {}
  Scope(const Signature& signature) : signature(signature) {}

  // Appedns the given vname to the members of this scope.
  void AddMemberItem(const ScopeMemberItem& vname);

  // Appends the member of the given scope to the current scope.
  void AppendScope(const Scope& scope);

  const std::vector<ScopeMemberItem>& Members() const { return members; }
  const Signature& GetSignature() const { return signature; }

  // Searches for the given reference_name in the current scope and returns its
  // VName.
  const VName* SearchForDefinition(absl::string_view name) const;

 private:
  // Signature of the owner of this scope.
  Signature signature;

  // list of the members inside this scope.
  std::vector<ScopeMemberItem> members;
};

// Container with a stack of Scopes to hold the accessible scopes during
// traversing an Indexing Facts Tree.
// This is used to get the definitions of some reference.
//
// This is modified during tree traversal because in case of entering new
// scope the new scope is resolved first and after that it's added to the
// containing scope and the next scope is being analyzed.
class VerticalScopeResolver : public verible::AutoPopStack<Scope*> {
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
  // TODO(minatoma): consider using vector<pair<name, type>> for signature.
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
class HorizontalScopeResolver {
 public:
  // Searches for a VName with the given name in the scope with the given
  // signature.
  const VName* SearchForVNameInScope(const Signature& signature,
                                     absl::string_view name) const;

  // Searches for a scope with the given signature in the scopes.
  const Scope* SearchForScope(const Signature& signature) const;

  // Maps the given signature to the given scope.
  void MapSignatureToScope(const Signature& signature, const Scope& scope);

  // Maps the given signature to the scope of the other signature.
  void MapSignatureToScopeOfSignature(const Signature& signature,
                                      const Signature& other_signature);

 private:
  // Saved packages signatures alongside with their inner members.
  // This is used for resolving references to some variables after using
  // import pkg::*. e.g package pkg1;
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
};

}  // namespace kythe
}  // namespace verilog

#endif  // VERIBLE_VERILOG_TOOLS_KYTHE_SCOPE_RESOLVER_H_
