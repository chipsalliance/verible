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

#include <vector>

#include "absl/strings/string_view.h"

namespace verilog {
namespace kythe {

bool ScopeMemberItem::operator<(const ScopeMemberItem& other) const {
  return this->vname < other.vname;
}

void Scope::AddMemberItem(const ScopeMemberItem& member_item) {
  members_.insert(member_item);
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

void Scope::RemoveMember(const ScopeMemberItem& member) {
  members_.erase(member);
}

const VName* ScopeContext::SearchForDefinition(absl::string_view name) const {
  for (const auto& scope : verible::make_range(rbegin(), rend())) {
    const VName* result = scope->SearchForDefinition(name);
    if (result != nullptr) {
      return result;
    }
  }
  return nullptr;
}

void ScopeResolver::RemoveDefinitionFromCurrentScope(const VName& vname) {
  scope_context_.top().RemoveMember(vname);
}

void ScopeResolver::MapSignatureToScope(const Signature& signature,
                                        const Scope& scope) {
  scopes_[signature] = scope;  // copy-assign
}

void ScopeResolver::AppendScopeToCurrentScope(const Scope& scope) {
  scope_context_.top().AppendScope(scope);
}

void ScopeResolver::AddDefinitionToCurrentScope(
    const ScopeMemberItem& new_member) {
  scope_context_.top().AddMemberItem(new_member);
}

const VName* ScopeResolver::SearchForDefinitionInGlobalScope(
    absl::string_view reference_name) const {
  const VName* definition =
      SearchForDefinitionInScope(global_scope_signature_, reference_name);
  if (definition != nullptr) {
    return definition;
  }
  if (previous_file_scope_resolver_ != nullptr) {
    return previous_file_scope_resolver_->SearchForDefinitionInGlobalScope(
        reference_name);
  }
  return nullptr;
}

const VName* ScopeResolver::SearchForDefinitionInScopeContext(
    absl::string_view reference_name) const {
  return scope_context_.SearchForDefinition(reference_name);
}

const VName* ScopeResolver::SearchForDefinitionInCurrentScope(
    absl::string_view name) const {
  return scope_context_.top().SearchForDefinition(name);
}

const std::vector<std::pair<const VName*, const Scope*>>
ScopeResolver::SearchForDefinitions(
    const std::vector<absl::string_view>& names) const {
  std::vector<std::pair<const VName*, const Scope*>> definitions;
  if (names.empty()) {
    return definitions;
  }

  // Try to find the definition in the scopes of the current file.
  const VName* definition = SearchForDefinitionInScopeContext(names[0]);

  // Try to find the definition in the previous files' scopes.
  if (definition == nullptr && previous_file_scope_resolver_ != nullptr) {
    // This is a linear-time search over files.
    definition =
        previous_file_scope_resolver_->SearchForDefinitionInGlobalScope(
            names[0]);
  }

  if (definition == nullptr) {
    return definitions;
  }

  const Scope* current_scope = SearchForScope(definition->signature);
  definitions.push_back({definition, current_scope});

  // Iterate over the names and try to find the definition in the current scope.
  for (const auto& name : verible::make_range(names.begin() + 1, names.end())) {
    if (current_scope == nullptr) {
      break;
    }
    const VName* definition = current_scope->SearchForDefinition(name);
    if (definition == nullptr) {
      break;
    }
    current_scope = SearchForScope(definition->signature);
    definitions.push_back({definition, current_scope});
  }

  return definitions;
}

const Scope* ScopeResolver::SearchForScope(const Signature& signature) const {
  const auto scope = scopes_.find(signature);
  if (scope != scopes_.end()) {
    return &scope->second;
  }

  // Try to find the definition in the previous files' scopes.
  // This is a linear-time search over files.
  if (previous_file_scope_resolver_ != nullptr) {
    return previous_file_scope_resolver_->SearchForScope(signature);
  }

  return nullptr;
}

const VName* ScopeResolver::SearchForDefinitionInScope(
    const Signature& signature, absl::string_view name) const {
  const Scope* scope = SearchForScope(signature);
  if (scope == nullptr) {
    return nullptr;
  }
  return scope->SearchForDefinition(name);
}

}  // namespace kythe
}  // namespace verilog
