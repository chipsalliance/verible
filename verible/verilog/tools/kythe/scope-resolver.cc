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

#include "verible/verilog/tools/kythe/scope-resolver.h"

#include <optional>
#include <string>
#include <vector>

#include "absl/container/flat_hash_set.h"
#include "absl/strings/str_cat.h"
#include "absl/strings/string_view.h"
#include "verible/common/util/logging.h"
#include "verible/verilog/tools/kythe/kythe-facts.h"

namespace verilog {
namespace kythe {

void ScopeResolver::SetCurrentScope(const Signature &scope) {
  if (current_scope_ == scope && !current_scope_digest_.rolling_hash.empty()) {
    return;
  }
  current_scope_digest_ = scope.Digest();
  current_scope_ = scope;
  if (enable_debug_) {
    scope_to_string_debug_.emplace(scope.Digest(), scope.ToString());
  }
  VLOG(2) << "Set scope to: " << ScopeDebug(scope.Digest());
}

void ScopeResolver::RemoveDefinitionFromCurrentScope(const VName &vname) {
  absl::string_view name = vname.signature.Names().back();
  auto scopes = variable_to_scoped_vname_.find(name);
  if (scopes == variable_to_scoped_vname_.end()) {
    VLOG(1) << "No definition for '" << name << "'. Nothing to remove.";
    return;
  }
  SignatureDigest current_scope_digest = CurrentScopeDigest();
  VLOG(2) << "Remove " << name << " from " << ScopeDebug(current_scope_digest);
  for (auto iter = scopes->second.begin(); iter != scopes->second.end();
       ++iter) {
    if (iter->instantiation_scope == current_scope_digest) {
      scopes->second.erase(iter);
      break;
    }
  }

  auto current_scope_names = scope_to_vnames_.find(current_scope_digest);
  if (current_scope_names == scope_to_vnames_.end()) {
    return;
  }
  auto &vnames = current_scope_names->second;
  for (auto iter = vnames.begin(); iter != vnames.end(); ++iter) {
    if (*iter == vname) {
      vnames.erase(iter);
      break;
    }
  }
}

void ScopeResolver::AppendScopeToCurrentScope(
    const SignatureDigest &source_scope) {
  AppendScopeToScope(source_scope, CurrentScopeDigest());
}

void ScopeResolver::AppendScopeToScope(
    const SignatureDigest &source_scope,
    const SignatureDigest &destination_scope) {
  auto scope_vnames = scope_to_vnames_.find(source_scope);
  if (scope_vnames == scope_to_vnames_.end()) {
    VLOG(2) << "Can't find scope " << ScopeDebug(source_scope)
            << " to append it to the current scope";
    return;
  }
  if (source_scope == destination_scope) {
    // The source and destination scope are equal. Nothing to add.
    return;
  }

  for (const auto &vn : scope_vnames->second) {
    const std::optional<ScopedVname> vn_type =
        FindScopeAndDefinition(vn.signature.Names().back(), source_scope);
    if (!vn_type) {
      continue;
    }
    variable_to_scoped_vname_[vn.signature.Names().back()].insert(
        ScopedVname{.type_scope = vn_type->type_scope,
                    .instantiation_scope = destination_scope,
                    .vname = vn});
    scope_to_vnames_[destination_scope].insert(vn);
  }
}

void ScopeResolver::AddDefinitionToCurrentScope(const VName &new_member) {
  AddDefinitionToCurrentScope(new_member, new_member.signature.Digest());
}

void ScopeResolver::AddDefinitionToCurrentScope(
    const VName &new_member, const SignatureDigest &type_scope) {
  // Remove the existing definition -- overwrite it with the new which has
  // updated information about types.
  RemoveDefinitionFromCurrentScope(new_member);

  auto current_scope_digest = CurrentScopeDigest();
  variable_to_scoped_vname_[new_member.signature.Names().back()].insert(
      ScopedVname{.type_scope = type_scope,
                  .instantiation_scope = current_scope_digest,
                  .vname = new_member});
  scope_to_vnames_[current_scope_digest].insert(new_member);
}

std::optional<ScopedVname> ScopeResolver::FindScopeAndDefinition(
    absl::string_view name, const SignatureDigest &scope_focus) {
  VLOG(2) << "Find definition for '" << name << "' within scope "
          << ScopeDebug(scope_focus);
  auto scope = variable_to_scoped_vname_.find(name);
  if (scope == variable_to_scoped_vname_.end()) {
    VLOG(2) << "Failed to find definition for '" << name << "' within scope "
            << ScopeDebug(scope_focus) << " (unregistered name)";
    return {};
  }
  const ScopedVname *match = nullptr;
  for (auto &scope_member : scope->second) {
    SignatureDigest digest = scope_member.instantiation_scope;
    if (scope_focus.rolling_hash.size() < digest.rolling_hash.size() ||
        (match != nullptr &&
         digest.rolling_hash.size() <
             match->instantiation_scope.rolling_hash.size())) {
      // Mismatch, or not interesting (worse match).
      VLOG(2) << "Scope resolution mismatch for '" << name << "' at scope "
              << ScopeDebug(digest);
      continue;
    }
    if (scope_focus.rolling_hash[digest.rolling_hash.size() - 1] ==
        digest.Hash()) {
      match = &scope_member;
    }
  }
  if (match != nullptr) {
    VLOG(2) << "Found definition for '" << name << "' within scope "
            << ScopeDebug(scope_focus);
    return *match;
  }
  VLOG(2) << "Failed to find definition for '" << name << "' within scope "
          << ScopeDebug(scope_focus);
  return {};
}

std::optional<ScopedVname> ScopeResolver::FindScopeAndDefinition(
    absl::string_view name) {
  return FindScopeAndDefinition(name, CurrentScopeDigest());
}

const absl::flat_hash_set<VName> &ScopeResolver::ListScopeMembers(
    const SignatureDigest &scope_digest) const {
  const static absl::flat_hash_set<VName> kEmptyMemberList;
  auto scope = scope_to_vnames_.find(scope_digest);
  if (scope == scope_to_vnames_.end()) {
    return kEmptyMemberList;
  }
  return scope->second;
}

std::string ScopeResolver::ScopeDebug(const SignatureDigest &scope) const {
  if (!enable_debug_) {
    return "UNKNOWN (debug off)";
  }
  const auto s = scope_to_string_debug_.find(scope);
  if (s == scope_to_string_debug_.end()) {
    return absl::StrCat("UNKNOWN ", scope.Hash());
  }
  return absl::StrCat(s->second, " H: ", scope.Hash());
}

}  // namespace kythe
}  // namespace verilog
