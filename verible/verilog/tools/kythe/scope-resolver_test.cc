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

#include <vector>

#include "absl/strings/string_view.h"
#include "gmock/gmock.h"
#include "gtest/gtest.h"
#include "verible/verilog/tools/kythe/kythe-facts.h"

namespace verilog {
namespace kythe {
namespace {

using ::testing::UnorderedElementsAreArray;

constexpr absl::string_view names[] = {
    "signature0", "signature1", "signature2", "signature3",
    "signature4", "signature5", "signature6", "signature7",
    "signature8", "signature9", "",
};

static const std::vector<Signature> signatures{
    Signature(names[0]),
    Signature(names[1]),
    Signature(names[2]),
    Signature(names[3]),
    Signature(names[4]),
    Signature(Signature(names[5]), names[6]),
    Signature(Signature(Signature(names[7]), names[8]), names[9]),
    Signature(names[10]),
};

static const std::vector<VName> vnames{
    {.path = "", .root = "", .signature = signatures[0], .corpus = ""},
    {.path = "", .root = "", .signature = signatures[1], .corpus = ""},
    {.path = "", .root = "", .signature = signatures[2], .corpus = ""},
    {.path = "", .root = "", .signature = signatures[3], .corpus = ""},
    {.path = "", .root = "", .signature = signatures[4], .corpus = ""},
    {.path = "", .root = "", .signature = signatures[5], .corpus = ""},
    {.path = "", .root = "", .signature = signatures[6], .corpus = ""},
    {.path = "", .root = "", .signature = signatures[7], .corpus = ""},
};

TEST(ScopeResolverTests, CurrentScope) {
  ScopeResolver scope_resolver(signatures[6]);
  EXPECT_EQ(scope_resolver.CurrentScopeDigest(), signatures[6].Digest());

  scope_resolver.SetCurrentScope(signatures[5]);
  EXPECT_EQ(scope_resolver.CurrentScopeDigest(), signatures[5].Digest());
}

TEST(ScopeResolverTests, AddAndFindDefinition) {
  ScopeResolver scope_resolver(signatures[6]);
  scope_resolver.AddDefinitionToCurrentScope(vnames[0]);
  scope_resolver.AddDefinitionToCurrentScope(vnames[1], signatures[5].Digest());

  const auto def_without_type =
      scope_resolver.FindScopeAndDefinition(vnames[0].signature.Names().back());
  const auto def_with_type =
      scope_resolver.FindScopeAndDefinition(vnames[1].signature.Names().back());
  const auto unknown_def =
      scope_resolver.FindScopeAndDefinition(vnames[2].signature.Names().back());

  ASSERT_TRUE(def_without_type.has_value());
  if (def_without_type.has_value()) {  // make clang-tidy happy.
    EXPECT_EQ(def_without_type->instantiation_scope,
              scope_resolver.CurrentScopeDigest());
    EXPECT_EQ(def_without_type->type_scope, vnames[0].signature.Digest());
    EXPECT_EQ(def_without_type->vname.signature, vnames[0].signature);
  }

  ASSERT_TRUE(def_with_type.has_value());
  if (def_with_type.has_value()) {  // make clang-tidy happy.
    EXPECT_EQ(def_with_type->instantiation_scope,
              scope_resolver.CurrentScopeDigest());
    EXPECT_EQ(def_with_type->type_scope, signatures[5].Digest());
    EXPECT_EQ(def_with_type->vname.signature, vnames[1].signature);
  }

  EXPECT_FALSE(unknown_def.has_value());
}

TEST(ScopeResolverTests, FindDefinitionInDifferentScope) {
  ScopeResolver scope_resolver(signatures[5]);
  scope_resolver.AddDefinitionToCurrentScope(vnames[0]);
  scope_resolver.SetCurrentScope(signatures[0]);

  absl::string_view name = vnames[0].signature.Names().back();
  const auto def_in_current_scope = scope_resolver.FindScopeAndDefinition(name);
  EXPECT_FALSE(def_in_current_scope.has_value());

  const auto def_in_correct_scope =
      scope_resolver.FindScopeAndDefinition(name, signatures[5].Digest());
  EXPECT_TRUE(def_in_correct_scope.has_value());
}

TEST(ScopeResolverTests, RemoveDefinition) {
  ScopeResolver scope_resolver(signatures[5]);
  scope_resolver.AddDefinitionToCurrentScope(vnames[0]);

  absl::string_view name = vnames[0].signature.Names().back();
  const auto def_in_current_scope = scope_resolver.FindScopeAndDefinition(name);
  EXPECT_TRUE(def_in_current_scope.has_value());

  scope_resolver.RemoveDefinitionFromCurrentScope(vnames[0]);
  const auto removed_def = scope_resolver.FindScopeAndDefinition(name);
  EXPECT_FALSE(removed_def.has_value());
}

TEST(ScopeResolverTests, SameNameVariableInMultipleScopes) {
  constexpr absl::string_view name = "a";
  Signature sig1(signatures[0], name);
  VName var1{.path = "", .root = "", .signature = sig1, .corpus = ""};
  Signature sig2(signatures[5], name);
  VName var2{.path = "", .root = "", .signature = sig2, .corpus = ""};

  ScopeResolver scope_resolver(signatures[6]);
  scope_resolver.AddDefinitionToCurrentScope(var1);
  scope_resolver.SetCurrentScope(signatures[0]);
  scope_resolver.AddDefinitionToCurrentScope(var2);

  const auto def_var1 =
      scope_resolver.FindScopeAndDefinition(name, signatures[6].Digest());
  ASSERT_TRUE(def_var1.has_value());
  if (def_var1.has_value()) {  // make clang-tidy happy
    EXPECT_EQ(def_var1->vname.signature, sig1);
  }

  const auto def_var2 =
      scope_resolver.FindScopeAndDefinition(name, signatures[0].Digest());
  ASSERT_TRUE(def_var2.has_value());
  if (def_var2.has_value()) {  // make clang-tidy happy
    EXPECT_EQ(def_var2->vname.signature, sig2);
  }
  const auto def_var_current_scope =
      scope_resolver.FindScopeAndDefinition(name);
  ASSERT_TRUE(def_var_current_scope.has_value());
  if (def_var_current_scope.has_value()) {  // make clang-tidy happy.
    EXPECT_EQ(def_var_current_scope->vname.signature, sig2);
  }
}

TEST(ScopeResolverTests, ListScopeMembers) {
  ScopeResolver scope_resolver(signatures[6]);
  scope_resolver.AddDefinitionToCurrentScope(vnames[0]);
  scope_resolver.AddDefinitionToCurrentScope(vnames[1]);
  scope_resolver.AddDefinitionToCurrentScope(vnames[2]);
  scope_resolver.SetCurrentScope(signatures[0]);
  scope_resolver.AddDefinitionToCurrentScope(vnames[3]);

  EXPECT_THAT(scope_resolver.ListScopeMembers(signatures[6].Digest()),
              UnorderedElementsAreArray({vnames[0], vnames[1], vnames[2]}));
  EXPECT_THAT(scope_resolver.ListScopeMembers(signatures[6].Digest()),
              UnorderedElementsAreArray({vnames[0], vnames[1], vnames[2]}));
}

TEST(ScopeResolverTests, AppendScope) {
  ScopeResolver scope_resolver(signatures[5]);
  scope_resolver.AddDefinitionToCurrentScope(vnames[0]);
  scope_resolver.SetCurrentScope(signatures[0]);

  absl::string_view name = vnames[0].signature.Names().back();
  const auto def_in_current_scope = scope_resolver.FindScopeAndDefinition(name);
  EXPECT_FALSE(def_in_current_scope.has_value());

  const auto def_in_correct_scope =
      scope_resolver.FindScopeAndDefinition(name, signatures[5].Digest());
  EXPECT_TRUE(def_in_correct_scope.has_value());

  scope_resolver.AppendScopeToScope(signatures[5].Digest(),
                                    signatures[0].Digest());
  const auto def_in_current_scope_post_appending =
      scope_resolver.FindScopeAndDefinition(name);
  EXPECT_TRUE(def_in_current_scope_post_appending.has_value());
}

}  // namespace
}  // namespace kythe
}  // namespace verilog
