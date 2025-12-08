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

#include <optional>
#include <string>
#include <string_view>
#include <utility>

#include "absl/container/flat_hash_map.h"
#include "absl/container/flat_hash_set.h"
#include "absl/container/node_hash_map.h"
#include "verible/verilog/tools/kythe/kythe-facts.h"

namespace verilog {
namespace kythe {

// VName tied to the Scopes where it's defined and instantiated.
struct ScopedVname {
  // Where this type was defined.
  SignatureDigest type_scope;
  // Where the variable of this type was instantiated (or equal to the
  // type_scope in case this VName is a type definition).
  SignatureDigest instantiation_scope;
  VName vname;
  bool operator==(const ScopedVname &other) const {
    return type_scope == other.type_scope &&
           instantiation_scope == other.instantiation_scope &&
           vname == other.vname;
  }
};
template <typename H>
H AbslHashValue(H state, const ScopedVname &v) {
  return H::combine(std::move(state), v.type_scope, v.instantiation_scope,
                    v.vname.signature.Digest());
}

// ScopeResolver enables resolving a symbol to its definition (to make it
// possible to distinguish one variable or type declaration from another). This
// is done by tracking the program scopes -- syntax elements like function,
// modules and loops create scopes which can contain variables. Multiple
// variables of equal name can co-exist in different scopes.
//
// The resolver keeps track of the active scope. Each scope encodes the whole
// hierarcy. In the following example
//
//  package my_pkg;
//
//  class my_class;
//    int my_var;
//    virtual function int my_function();
//      return my_var;
//    endfunction
//  endclass
//
//  endpackage : my_pkg
//
// We have the following scopes:
// /my_pkg
// /my_pkg/my_class
// /my_pkg/my_class/my_function
// Scope resolver marks `my_var` as the member of /my_pkg/my_class and is able
// to resolve its reference inside `my_function` (by exploring the scopes bottom
// up and comparing the substrings).
class ScopeResolver {
 public:
  explicit ScopeResolver(const Signature &top_scope) {
    SetCurrentScope(top_scope);
  }

  ScopeResolver(const ScopeResolver &) = delete;
  ScopeResolver(ScopeResolver &&) = delete;
  ScopeResolver &operator=(const ScopeResolver &) = delete;
  ScopeResolver &operator=(ScopeResolver &&) = delete;

  void SetCurrentScope(const Signature &scope);

  const Signature &CurrentScope() { return current_scope_; }

  // Returns the scope and definition of the symbol under the given name. The
  // search is restricted to the current scope.
  std::optional<ScopedVname> FindScopeAndDefinition(std::string_view name);

  // Returns the scope and definition of the symbol under the given name. The
  // search is restricted to the provided scope.
  std::optional<ScopedVname> FindScopeAndDefinition(
      std::string_view name, const SignatureDigest &scope);

  static SignatureDigest GlobalScope() { return Signature("").Digest(); }

  // Adds the members of the given scope to the current scope.
  void AppendScopeToCurrentScope(const SignatureDigest &source_scope);

  // Adds the members of the source scope to the destination scope.
  void AppendScopeToScope(const SignatureDigest &source_scope,
                          const SignatureDigest &destination_scope);

  // Removes the given VName from the current scope.
  void RemoveDefinitionFromCurrentScope(const VName &vname);

  // Adds a definition & its type to the current scope.
  void AddDefinitionToCurrentScope(const VName &new_member,
                                   const SignatureDigest &type_scope);

  // Adds a definition without external type to the current scope.
  void AddDefinitionToCurrentScope(const VName &new_member);

  const absl::flat_hash_set<VName> &ListScopeMembers(
      const SignatureDigest &scope_digest) const;

  // Returns human readable description of the scope.
  std::string ScopeDebug(const SignatureDigest &scope) const;

  const SignatureDigest &CurrentScopeDigest() const {
    return current_scope_digest_;
  }

  // When set, the scope resolver will collect human readable descriptions of
  // the scope for easier debugging.
  void EnableDebug() { enable_debug_ = true; }

 private:
  // Mapping from the symbol name to all scopes where it's present.
  absl::flat_hash_map<std::string, absl::flat_hash_set<ScopedVname>>
      variable_to_scoped_vname_;

  // Mapping from scope to all its members. NOTE: requires pointer stability!
  absl::node_hash_map<SignatureDigest, absl::flat_hash_set<VName>>
      scope_to_vnames_;

  // Maps the scope to the human readable description. Available only when debug
  // is enabled.
  absl::flat_hash_map<SignatureDigest, std::string> scope_to_string_debug_;

  SignatureDigest current_scope_digest_;
  Signature current_scope_;
  bool enable_debug_ = false;
};

}  // namespace kythe
}  // namespace verilog

#endif  // VERIBLE_VERILOG_TOOLS_KYTHE_SCOPE_RESOLVER_H_
