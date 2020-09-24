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

#include "verilog/tools/kythe/scope_resolver.h"

#include <iostream>
#include <string>

namespace verilog {
namespace kythe {

void Scope::AddMemberItem(const ScopeMemberItem& member_item) {
  members_.push_back(member_item);
}

void Scope::AppendScope(const Scope& scope) {
  for (const ScopeMemberItem& item : scope.Members()) {
    this->AddMemberItem(item);
  }
}

const VName* Scope::SearchForDefinition(absl::string_view name) const {
  for (const ScopeMemberItem& member_item :
       verible::make_range(members_.rbegin(), members_.rend())) {
    if (member_item.vname.signature.IsNameEqual(name)) {
      return &member_item.vname;
    }
  }
  return nullptr;
}

const VName* VerticalScopeResolver::SearchForDefinition(
    absl::string_view name) const {
  for (const auto& scope : verible::make_range(rbegin(), rend())) {
    const VName* result = scope->SearchForDefinition(name);
    if (result != nullptr) {
      return result;
    }
  }
  return nullptr;
}

const Scope* FlattenedScopeResolver::SearchForScope(
    const Signature& signature) const {
  const auto scope = scopes_.find(signature);
  if (scope == scopes_.end()) {
    return nullptr;
  }
  return &scope->second;
}

const VName* FlattenedScopeResolver::SearchForVNameInScope(
    const Signature& signature, absl::string_view name) const {
  const Scope* scope = SearchForScope(signature);
  if (scope == nullptr) {
    return nullptr;
  }
  return scope->SearchForDefinition(name);
}

void FlattenedScopeResolver::MapSignatureToScope(const Signature& signature,
                                                  const Scope& scope) {
  scopes_[signature] = scope;
}

void FlattenedScopeResolver::MapSignatureToScopeOfSignature(
    const Signature& signature, const Signature& other_signature) {
  const Scope* other_scope = SearchForScope(other_signature);
  if (other_scope == nullptr) {
    return;
  }
  MapSignatureToScope(signature, *other_scope);
}

}  // namespace kythe
}  // namespace verilog
