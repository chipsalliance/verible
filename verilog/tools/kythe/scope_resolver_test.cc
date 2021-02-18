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

#include "gtest/gtest.h"
#include "verilog/tools/kythe/kythe_facts.h"

namespace verilog {
namespace kythe {
namespace {

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

TEST(ScopeResolverTests, ScopeResolverLinkedList) {
  /**
   * signature[0] => {
   *   vnames[1],  ==> signature[1]
   *   vnames[2],  ==> signature[2]
   * }
   */

  Scope scope(signatures[0]);
  scope.AddMemberItem(vnames[1]);
  scope.AddMemberItem(vnames[2]);

  ScopeResolver scope_resolver1(Signature("file1"), nullptr);
  scope_resolver1.MapSignatureToScope(signatures[0], scope);

  ScopeResolver scope_resolver(Signature("file2"), &scope_resolver1);
  {
    Scope global_scope(signatures[7]);
    ScopeContext::AutoPop p1(&scope_resolver.GetMutableScopeContext(),
                             &global_scope);
    scope_resolver.AddDefinitionToCurrentScope(vnames[0]);
    {
      const std::vector<std::pair<const VName*, const Scope*>> vnames =
          scope_resolver.SearchForDefinitions({names[0], names[1]});
      EXPECT_EQ(vnames.size(), 2);
      EXPECT_EQ(vnames[1].first->signature, signatures[1]);
    }
    {
      const std::vector<std::pair<const VName*, const Scope*>> vnames =
          scope_resolver.SearchForDefinitions({names[0], names[2]});
      EXPECT_EQ(vnames.size(), 2);
      EXPECT_EQ(vnames[1].first->signature, signatures[2]);
    }
    {
      const std::vector<std::pair<const VName*, const Scope*>> vnames =
          scope_resolver.SearchForDefinitions({names[0], names[3]});
      EXPECT_EQ(vnames.size(), 1);
      EXPECT_EQ(vnames[0].first->signature, signatures[0]);
    }
    {
      const std::vector<std::pair<const VName*, const Scope*>> vnames =
          scope_resolver.SearchForDefinitions({names[2], names[1]});
      EXPECT_TRUE(vnames.empty());
    }
  }
}

TEST(ScopesTest, AppendScope) {
  /**
   * signature[0] => {
   *   vnames[1],  ==> signature[1]
   *   vnames[2],  ==> signature[2]
   * }
   *
   * signature[1] => {
   *   vnames[3],  ==> signature[3];
   *   vnames[4],  ==> signature[4];
   * }
   */

  Scope scope(signatures[0]);
  scope.AddMemberItem(vnames[1]);
  scope.AddMemberItem(vnames[2]);

  Scope scope2(signatures[1]);
  scope2.AddMemberItem(vnames[3]);
  scope2.AddMemberItem(vnames[4]);

  scope2.AppendScope(scope);

  {
    const VName* vname = scope2.SearchForDefinition(names[1]);
    EXPECT_EQ(vname->signature, signatures[1]);
  }
  {
    const VName* vname = scope2.SearchForDefinition(names[2]);
    EXPECT_EQ(vname->signature, signatures[2]);
  }
  {
    const VName* vname = scope2.SearchForDefinition(names[3]);
    EXPECT_EQ(vname->signature, signatures[3]);
  }
  {
    const VName* vname = scope2.SearchForDefinition(names[4]);
    EXPECT_EQ(vname->signature, signatures[4]);
  }
}

TEST(ScopesTest, RemoveMember) {
  /**
   * signature[0] => {
   *   vnames[1],  ==> signature[1]
   *   vnames[2],  ==> signature[2]
   * }
   *
   * signature[1] => {
   *   vnames[3],  ==> signature[3];
   *   vnames[4],  ==> signature[4];
   * }
   */

  Scope scope(signatures[0]);
  scope.AddMemberItem(vnames[1]);
  scope.AddMemberItem(vnames[2]);

  {
    const VName* vname = scope.SearchForDefinition(names[1]);
    EXPECT_EQ(vname->signature, signatures[1]);
  }

  {
    scope.RemoveMember(vnames[1]);
    const VName* vname = scope.SearchForDefinition(names[1]);
    EXPECT_EQ(vname, nullptr);
  }

  {
    const VName* vname = scope.SearchForDefinition(names[2]);
    EXPECT_EQ(vname->signature, signatures[2]);
  }
}

TEST(ScopeResolverTests, SearchForDefinition) {
  ScopeResolver scope_resolver(Signature(""), nullptr);

  /**
   * signature[0] => {
   *   vnames[1],  ==> signature[1]
   *   vnames[2],  ==> signature[2]
   * }
   */

  Scope scope(signatures[0]);
  scope.AddMemberItem(vnames[1]);
  scope.AddMemberItem(vnames[2]);

  {
    Scope global_scope(signatures[7]);
    ScopeContext::AutoPop p1(&scope_resolver.GetMutableScopeContext(),
                             &global_scope);
    scope_resolver.AddDefinitionToCurrentScope(vnames[0]);
    scope_resolver.MapSignatureToScope(signatures[0], scope);
    {
      const std::vector<std::pair<const VName*, const Scope*>> vnames =
          scope_resolver.SearchForDefinitions({names[0], names[1]});
      EXPECT_EQ(vnames.size(), 2);
      EXPECT_EQ(vnames[1].first->signature, signatures[1]);
    }
    {
      const std::vector<std::pair<const VName*, const Scope*>> vnames =
          scope_resolver.SearchForDefinitions({names[0], names[2]});
      EXPECT_EQ(vnames.size(), 2);
      EXPECT_EQ(vnames[1].first->signature, signatures[2]);
    }
    {
      const std::vector<std::pair<const VName*, const Scope*>> vnames =
          scope_resolver.SearchForDefinitions({names[0], names[3]});
      EXPECT_EQ(vnames.size(), 1);
      EXPECT_EQ(vnames[0].first->signature, signatures[0]);
    }
    {
      const std::vector<std::pair<const VName*, const Scope*>> vnames =
          scope_resolver.SearchForDefinitions({names[2], names[1]});
      EXPECT_TRUE(vnames.empty());
    }
  }
}

TEST(ScopeResolverTests, SearchForDefinitionInCurrentScope) {
  ScopeResolver scope_resolver(Signature(""), nullptr);

  /**
   * signature[0] => {
   *   vnames[1],  ==> signature[1]
   *   vnames[2],  ==> signature[2]
   * }
   */

  Scope scope(signatures[0]);
  scope.AddMemberItem(vnames[1]);
  scope.AddMemberItem(vnames[2]);

  {
    ScopeContext::AutoPop p1(&scope_resolver.GetMutableScopeContext(), &scope);
    {
      const VName* vname =
          scope_resolver.SearchForDefinitionInCurrentScope(names[1]);
      EXPECT_EQ(vname->signature, signatures[1]);
    }
    {
      const VName* vname =
          scope_resolver.SearchForDefinitionInCurrentScope(names[2]);
      EXPECT_EQ(vname->signature, signatures[2]);
    }
    {
      const VName* vname =
          scope_resolver.SearchForDefinitionInCurrentScope(names[3]);
      EXPECT_EQ(vname, nullptr);
    }
    {
      const VName* vname =
          scope_resolver.SearchForDefinitionInCurrentScope(names[4]);
      EXPECT_EQ(vname, nullptr);
    }
  }
}

TEST(ScopeResolverTests, DeleteDefinitionFromCurrentScope) {
  ScopeResolver scope_resolver(Signature(""), nullptr);

  /**
   * signature[0] => {
   *   vnames[1],  ==> signature[1]
   *   vnames[2],  ==> signature[2]
   * }
   */

  Scope scope(signatures[0]);
  scope.AddMemberItem(vnames[1]);
  scope.AddMemberItem(vnames[2]);

  {
    ScopeContext::AutoPop p1(&scope_resolver.GetMutableScopeContext(), &scope);
    {
      const VName* vname =
          scope_resolver.SearchForDefinitionInCurrentScope(names[1]);
      EXPECT_EQ(vname->signature, signatures[1]);
    }

    {
      scope_resolver.RemoveDefinitionFromCurrentScope(vnames[1]);
      const VName* vname =
          scope_resolver.SearchForDefinitionInCurrentScope(names[1]);
      EXPECT_EQ(vname, nullptr);
    }
    {
      const VName* vname =
          scope_resolver.SearchForDefinitionInCurrentScope(names[2]);
      EXPECT_EQ(vname->signature, signatures[2]);
    }
  }
}

TEST(ScopeResolverTests, SearchForNestedDefinition) {
  ScopeResolver scope_resolver(Signature(""), nullptr);

  /**
   * signature[0] => {
   *   vnames[1],  ==> signature[1]
   *   vnames[2],  ==> signature[2]
   * }
   *
   * signature[1] => {
   *   vnames[3],  ==> signature[3];
   *   vnames[4],  ==> signature[4];
   * }
   *
   */

  Scope scope(signatures[0]);
  scope.AddMemberItem(vnames[1]);
  scope.AddMemberItem(vnames[2]);

  Scope scope2(signatures[1]);
  scope2.AddMemberItem(vnames[3]);
  scope2.AddMemberItem(vnames[4]);

  {
    Scope global_scope(signatures[7]);
    ScopeContext::AutoPop p1(&scope_resolver.GetMutableScopeContext(),
                             &global_scope);
    scope_resolver.AddDefinitionToCurrentScope(vnames[0]);
    scope_resolver.AddDefinitionToCurrentScope(vnames[1]);
    scope_resolver.AddDefinitionToCurrentScope(vnames[5]);
    scope_resolver.MapSignatureToScope(signatures[0], scope);
    scope_resolver.MapSignatureToScope(signatures[1], scope2);
    {
      const Scope* found_scope = scope_resolver.SearchForScope(signatures[0]);
      EXPECT_NE(found_scope, nullptr);
    }
    {
      const Scope* found_scope = scope_resolver.SearchForScope(signatures[1]);
      EXPECT_NE(found_scope, nullptr);
    }
    {
      const Scope* found_scope = scope_resolver.SearchForScope(signatures[6]);
      EXPECT_EQ(found_scope, nullptr);
    }
    {
      const std::vector<std::pair<const VName*, const Scope*>> vnames =
          scope_resolver.SearchForDefinitions({names[0], names[1]});
      EXPECT_EQ(vnames[1].first->signature, signatures[1]);
    }
    {
      const std::vector<std::pair<const VName*, const Scope*>> vnames =
          scope_resolver.SearchForDefinitions({names[1], names[3]});
      EXPECT_EQ(vnames[1].first->signature, signatures[3]);
    }
  }
}

TEST(VerticalScopeTests, SearchForDefinition) {
  ScopeContext vertical_scope_resolver;

  EXPECT_TRUE(vertical_scope_resolver.empty());
  {
    Scope scope;
    ScopeContext::AutoPop p1(&vertical_scope_resolver, &scope);
    vertical_scope_resolver.top().AddMemberItem(vnames[0]);
    EXPECT_EQ(vertical_scope_resolver.top().GetSignature(), Signature(""));
    EXPECT_EQ(vertical_scope_resolver.top().Members().size(), 1);
    {
      const VName* vname =
          vertical_scope_resolver.SearchForDefinition(names[0]);
      EXPECT_EQ(vname->signature, vnames[0].signature);
    }
    {
      const VName* vname =
          vertical_scope_resolver.SearchForDefinition(names[1]);
      EXPECT_EQ(vname, nullptr);
    }

    vertical_scope_resolver.top().AddMemberItem(vnames[1]);
    EXPECT_EQ(vertical_scope_resolver.top().Members().size(), 2);

    {
      const VName* vname =
          vertical_scope_resolver.SearchForDefinition(names[1]);
      EXPECT_EQ(vname->signature, signatures[1]);
    }
  }
  EXPECT_TRUE(vertical_scope_resolver.empty());
}

TEST(ScopeResolverTests, NestedScopeSearch) {
  ScopeContext scope_resolver;

  EXPECT_TRUE(scope_resolver.empty());
  Scope scope;
  ScopeContext::AutoPop p1(&scope_resolver, &scope);
  {
    scope_resolver.top().AddMemberItem(vnames[0]);
    EXPECT_EQ(scope_resolver.top().GetSignature(), Signature(""));
    EXPECT_EQ(scope_resolver.top().Members().size(), 1);

    const VName* vname = scope_resolver.SearchForDefinition(names[0]);
    EXPECT_EQ(vname->signature, vnames[0].signature);

    Scope scope(signatures[4]);
    ScopeContext::AutoPop p1(&scope_resolver, &scope);
    {
      scope_resolver.top().AddMemberItem(vnames[6]);
      EXPECT_EQ(scope_resolver.top().GetSignature(), signatures[4]);
      EXPECT_EQ(scope_resolver.top().Members().size(), 1);
      {
        const VName* vname = scope_resolver.SearchForDefinition(names[9]);
        EXPECT_EQ(vname->signature, vnames[6].signature);
      }
      {
        const VName* vname = scope_resolver.SearchForDefinition(names[8]);
        EXPECT_EQ(vname, nullptr);
      }
      {
        const VName* vname = scope_resolver.SearchForDefinition(names[0]);
        EXPECT_EQ(vname->signature, vnames[0].signature);
      }

      Scope scope(signatures[6]);
      ScopeContext::AutoPop p1(&scope_resolver, &scope);
      {
        scope_resolver.top().AddMemberItem(vnames[2]);
        EXPECT_EQ(scope_resolver.top().GetSignature(), signatures[6]);
        EXPECT_EQ(scope_resolver.top().Members().size(), 1);
        {
          const VName* vname = scope_resolver.SearchForDefinition(names[2]);
          EXPECT_EQ(vname->signature, vnames[2].signature);
        }
        {
          const VName* vname = scope_resolver.SearchForDefinition(names[9]);
          EXPECT_EQ(vname->signature, vnames[6].signature);
        }
        {
          const VName* vname = scope_resolver.SearchForDefinition(names[8]);
          EXPECT_EQ(vname, nullptr);
        }
        {
          const VName* vname = scope_resolver.SearchForDefinition(names[0]);
          EXPECT_EQ(vname->signature, vnames[0].signature);
        }
      }
    }
    EXPECT_EQ(scope_resolver.size(), 2);
  }
  EXPECT_EQ(scope_resolver.size(), 1);
}

}  // namespace
}  // namespace kythe
}  // namespace verilog
