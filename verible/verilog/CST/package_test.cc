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

// Unit tests for package-related concrete-syntax-tree functions.
//
// Testing strategy:
// The point of these tests is to validate the structure that is assumed
// about package declaration nodes and the structure that is actually
// created by the parser, so test *should* use the parser-generated
// syntax trees, as opposed to hand-crafted/mocked syntax trees.

#include "verible/verilog/CST/package.h"

#include <memory>
#include <vector>

#include "absl/status/status.h"
#include "gtest/gtest.h"
#include "verible/common/analysis/syntax-tree-search-test-utils.h"
#include "verible/common/analysis/syntax-tree-search.h"
#include "verible/common/text/concrete-syntax-tree.h"
#include "verible/common/text/text-structure.h"
#include "verible/common/text/token-info.h"
#include "verible/common/util/casts.h"
#include "verible/common/util/logging.h"
#include "verible/verilog/CST/match-test-utils.h"
#include "verible/verilog/analysis/verilog-analyzer.h"

#undef EXPECT_OK
#define EXPECT_OK(value) EXPECT_TRUE((value).ok())

#undef ASSERT_OK
#define ASSERT_OK(value) ASSERT_TRUE((value).ok())

namespace verilog {
namespace {

using verible::down_cast;
using verible::SyntaxTreeNode;
using verible::SyntaxTreeSearchTestCase;
using verible::TextStructureView;
using verible::TreeSearchMatch;

TEST(FindAllPackageDeclarationsTest, VariousTests) {
  constexpr int kTag = 1;
  const SyntaxTreeSearchTestCase testcases[] = {
      {""},
      {"module m;\nendmodule\n"},
      {"class c;\nendclass\n"},
      {"function f;\nendfunction\n"},
      {{kTag, "package p; \n endpackage"}, "\n"},
      {"\n", {kTag, "package p2; endpackage"}},
      {{kTag, "package p; \n endpackage"},
       " task sleep; ",
       " endtask\n",
       " class myclass;\n",
       "endclass\n",
       {kTag, "package p; \n endpackage"}},
      {{kTag, "package p1; \n endpackage"},
       "\n",
       "module m; \n",
       "const foo bar; \n",
       "endmodule \n",
       {kTag, "package p2; \n endpackage"}},

      {"function f; ",
       "endfunction ",
       "module m; \n",
       "const foo bar; \n",
       "endmodule \n",
       {kTag, "package p; \n endpackage"}},

      {"`include \"stuff.svh\"\n",
       "`expand_stuff()\n",
       "`expand_with_semi(name);\n",
       "`expand_more(name)\n",
       {kTag, "package p; \n endpackage"}},

      {{kTag, "package p; \n endpackage"},
       "`undef FOOOBAR\n",
       {kTag, "package p; \n endpackage"}},

      {{kTag, "package p; \n endpackage"},
       " let Peace = Love;\n",
       {kTag, "package p; \n endpackage"},
       " let Five() = Two + Two + One;\n",
       " let Min(a,b) = (a < b) ? a : b;\n",
       " let Max(a,b=1) = (a > b) ? a : b;\n",
       {kTag, "package p; \n endpackage"},
       " let Max(untyped a, bit b=1) = (a > b) ? a : b;\n"},

      {"localparam real foo = 3.14;\n",
       "localparam shortreal foo = 159.265;\n",
       "localparam realtime foo = 358.979ns;\n",
       {kTag, "package p; \n endpackage"},
       "  localparam real foo = 323.846;\n"},

      {{kTag, "package p; \n endpackage"},
       " import $unit::arnold;\n",
       " import $unit::*;\n",
       {kTag, "package p; \n endpackage"}},

      {{kTag, "package p; \n endpackage"},
       " export bar::baz;\n",
       " export bar::*;\n",
       {kTag, "package p; \n endpackage"}},
      {{kTag, "package p; \n endpackage"},
       " parameter reg[BITS:0] MR0 = '0;\n"},

      {{kTag, "package p; \n endpackage"},
       " bind scope_x type_y z (.*);\n",
       " bind scope_x type_y z1(.*), z2(.*);\n",
       " bind module_scope : inst_x type_y inst_z(.*);\n",
       {kTag, "package p; \n endpackage"}},

      {{kTag, "package p; \n endpackage"},
       "`ifndef DEBUGGER\n",
       "`endif\n",
       "  int num_packets;\n",
       "`ifdef DEBUGGER\n",
       "`endif\n",
       "  int router_size;\n"},

      {{kTag, "package p; \n endpackage"},
       "  int num_packets;\n",
       "`ifdef DEBUGGER\n",
       "`elsif BORED\n",
       "`else\n",
       "  string source_name;\n",
       "  string dest_name;\n",
       "`endif\n",
       "  int router_size;\n",
       {kTag, "package p; \n endpackage"}},

      {{kTag, "package p; \n endpackage"},
       " virtual a_if b_if;\n",
       "virtual a_if b_if, c_if;\n",
       {kTag, "package p; \n endpackage"},
       "  virtual a_if b_if;\n",
       {kTag, "package p; \n endpackage"}},

      {{kTag, "package p; \n endpackage"},
       "`ifdef DEBUGGER\n",
       "`endif\n",
       {kTag, "package p; \n endpackage"},
       "`ifdef DEBUGGER\n",
       "`ifdef VERBOSE\n",
       "`endif\n",
       "`endif\n",
       {kTag, "package p; \n endpackage"}}};
  for (const auto &test : testcases) {
    TestVerilogSyntaxRangeMatches(
        __FUNCTION__, test, [](const TextStructureView &text_structure) {
          const auto &root = text_structure.SyntaxTree();
          return FindAllPackageDeclarations(*ABSL_DIE_IF_NULL(root));
        });
  }
}

TEST(GetPackageNameTokenTest, VariousPackageTokenTests) {
  constexpr int kTag = 1;
  const SyntaxTreeSearchTestCase testcases[] = {
      {""},
      {"package ", {kTag, "foo"}, "; \n endpackage"},
      {"package ", {kTag, "bar"}, "; \n endpackage"},
      {"package ", {kTag, "foo"}, "; \n endpackage", "\n"},
      {"package ",
       {kTag, "p1"},
       "; \n endpackage",
       " task sleep; ",
       " endtask\n",
       " class myclass;\n",
       "endclass\n",
       "package ",
       {kTag, "p2"},
       "; \n endpackage"},
      {"package ",
       {kTag, "p1"},
       "; \n endpackage",
       "`ifdef DEBUGGER\n",
       "`endif\n",
       "package ",
       {kTag, "p2"},
       "; \n endpackage",
       "`ifdef DEBUGGER\n",
       "`ifdef VERBOSE\n",
       "`endif\n",
       "`endif\n",
       "package ",
       {kTag, "p3"},
       "; \n endpackage"},
      {"package ",
       {kTag, "p1"},
       "; \n endpackage",
       " virtual a_if b_if;\n",
       "virtual a_if b_if, c_if;\n",
       "package ",
       {kTag, "p2"},
       "; \n endpackage",
       "  virtual a_if b_if;\n",
       "package ",
       {kTag, "p3"},
       "; \n endpackage"},
      {"package ",
       {kTag, "p1"},
       "; \n endpackage",
       "  int num_packets;\n",
       "`ifdef DEBUGGER\n",
       "`elsif BORED\n",
       "`else\n",
       "  string source_name;\n",
       "  string dest_name;\n",
       "`endif\n",
       "  int router_size;\n",
       "package ",
       {kTag, "p2"},
       "; \n endpackage"},
      {"package ",
       {kTag, "p1"},
       "; \n endpackage",
       " bind scope_x type_y z (.*);\n",
       " bind scope_x type_y z1(.*), z2(.*);\n",
       " bind module_scope : inst_x type_y inst_z(.*);\n",
       "package ",
       {kTag, "p2"},
       "; \n endpackage"},
      {"package ",
       {kTag, "p1"},
       "; \n endpackage",
       " import $unit::arnold;\n",
       " import $unit::*;\n",
       "package ",
       {kTag, "p2"},
       "; \n endpackage"},
      {"package ",
       {kTag, "p1"},
       "; \n endpackage",
       " export bar::baz;\n",
       " export bar::*;\n",
       "package ",
       {kTag, "p2"},
       "; \n endpackage"},
      {"package ",
       {kTag, "p1"},
       "; \n endpackage",
       " parameter reg[BITS:0] MR0 = '0;\n"},
      {"`include \"stuff.svh\"\n",
       "`expand_stuff()\n",
       "`expand_with_semi(name);\n",
       "`expand_more(name)\n",
       "package ",
       {kTag, "p2"},
       "; \n endpackage"},
      {"package ",
       {kTag, "p1"},
       "; \n endpackage",
       "`undef FOOOBAR\n",
       "package ",
       {kTag, "p2"},
       "; \n endpackage"},
      {"package ",
       {kTag, "p1"},
       "; \n endpackage",
       " let Peace = Love;\n",
       "package ",
       {kTag, "p2"},
       "; \n endpackage",
       " let Five() = Two + Two + One;\n",
       " let Min(a,b) = (a < b) ? a : b;\n",
       " let Max(a,b=1) = (a > b) ? a : b;\n",
       "package ",
       {kTag, "p3"},
       "; \n endpackage",
       " let Max(untyped a, bit b=1) = (a > b) ? a : b;\n"},
      {"localparam real foo = 3.14;\n",
       "localparam shortreal foo = 159.265;\n",
       "localparam realtime foo = 358.979ns;\n",
       "package ",
       {kTag, "p1"},
       "; \n endpackage",
       "  localparam real foo = 323.846;\n"}};

  for (const auto &test : testcases) {
    TestVerilogSyntaxRangeMatches(
        __FUNCTION__, test, [](const TextStructureView &text_structure) {
          const auto &root = text_structure.SyntaxTree();
          const auto declarations =
              FindAllPackageDeclarations(*ABSL_DIE_IF_NULL(root));

          std::vector<TreeSearchMatch> declIdentifiers;
          for (const auto &decl : declarations) {
            const auto *packageToken = GetPackageNameLeaf(*decl.match);
            declIdentifiers.push_back(TreeSearchMatch{packageToken, {}});
          }
          return declIdentifiers;
        });
  }
}

TEST(GetPackageNameTokenTest, RootIsNotAPackage) {
  VerilogAnalyzer analyzer("package foo; endpackage", "");
  EXPECT_OK(analyzer.Analyze());
  const auto &root = analyzer.Data().SyntaxTree();
  // Root node is a description list, not a package.
  EXPECT_EQ(GetPackageNameToken(*ABSL_DIE_IF_NULL(root)), nullptr);
}

TEST(GetPackageNameTokenTest, ValidPackage) {
  VerilogAnalyzer analyzer("package foo; endpackage", "");
  EXPECT_OK(analyzer.Analyze());
  const auto &root = analyzer.Data().SyntaxTree();
  const auto package_declarations = FindAllPackageDeclarations(*root);
  EXPECT_EQ(package_declarations.size(), 1);
  const auto &package_node =
      down_cast<const SyntaxTreeNode &>(*package_declarations.front().match);
  // Root node is a description list, not a package.
  const auto *token = GetPackageNameToken(package_node);
  EXPECT_EQ(token->text(), "foo");
}

TEST(GetPackageNameTest, GetPackageEndLabelName) {
  constexpr int kTag = 1;
  const SyntaxTreeSearchTestCase testcases[] = {
      {""},
      {"package foo;\n endpackage"},
      {"package foo;\n endpackage: ", {kTag, "foo"}},
      {"package foo;\n function int f();\n return 10;\n endfunction: f\n class "
       "c; endclass: c\n "
       "endpackage: ",
       {kTag, "foo"}},
  };

  for (const auto &test : testcases) {
    TestVerilogSyntaxRangeMatches(
        __FUNCTION__, test, [](const TextStructureView &text_structure) {
          const auto &root = text_structure.SyntaxTree();

          const auto declarations =
              FindAllPackageDeclarations(*ABSL_DIE_IF_NULL(root));

          std::vector<TreeSearchMatch> names;
          for (const auto &decl : declarations) {
            const auto *package_name = GetPackageNameEndLabel(*decl.match);
            if (package_name == nullptr) continue;
            names.push_back(TreeSearchMatch{package_name, {}});
          }
          return names;
        });
  }
}

TEST(GetPackageBodyTest, GetPackageItemList) {
  constexpr int kTag = 1;
  const SyntaxTreeSearchTestCase testcases[] = {
      {""},
      {"package foo;\n endpackage"},
      {"package foo;\n endpackage: foo"},
      {"package foo;\n",
       {kTag,
        "function int f();\n return 10;\n endfunction: f\n class "
        "c; endclass: c"},
       "\n ",
       "endpackage: foo"},
  };

  for (const auto &test : testcases) {
    TestVerilogSyntaxRangeMatches(
        __FUNCTION__, test, [](const TextStructureView &text_structure) {
          const auto &root = text_structure.SyntaxTree();

          const auto declarations =
              FindAllPackageDeclarations(*ABSL_DIE_IF_NULL(root));

          std::vector<TreeSearchMatch> lists;
          for (const auto &decl : declarations) {
            const auto *package_item_list = GetPackageItemList(*decl.match);
            if (package_item_list == nullptr) continue;
            lists.push_back(TreeSearchMatch{package_item_list, {}});
          }
          return lists;
        });
  }
}

TEST(PackageImportTest, GetImportedPackageName) {
  constexpr int kTag = 1;  // value doesn't matter
  const SyntaxTreeSearchTestCase kTestCases[] = {
      {""},
      {"module m; endmodule\n"},
      {"package pkg; endpackage\nmodule m();\n import ",
       {kTag, "pkg"},
       "::*;\nendmodule"},
      {"package pkg;\n int my_int;\nendpackage\nmodule m\nimport ",
       {kTag, "pkg"},
       "::*;\nimport ",
       {kTag, "pkg"},
       "::my_int;\n();\nendmodule"},
  };
  for (const auto &test : kTestCases) {
    TestVerilogSyntaxRangeMatches(
        __FUNCTION__, test, [](const TextStructureView &text_structure) {
          const auto &root = text_structure.SyntaxTree();
          const auto decls = FindAllPackageImportItems(*ABSL_DIE_IF_NULL(root));

          std::vector<TreeSearchMatch> names;
          for (const auto &decl : decls) {
            const auto *name = GetImportedPackageName(*decl.match);
            names.emplace_back(TreeSearchMatch{name, {/* ignored context */}});
          }
          return names;
        });
  }
}

TEST(PackageImportTest, GetImportedItemName) {
  constexpr int kTag = 1;  // value doesn't matter
  const SyntaxTreeSearchTestCase kTestCases[] = {
      {""},
      {"module m; endmodule\n"},
      {"package pkg; endpackage\nmodule m();\n import pkg::*;\nendmodule"},
      {"package pkg;\n int my_int;\nendpackage\nmodule m();\n import "
       "pkg::*;\nimport pkg::",
       {kTag, "my_int"},
       ";\nendmodule"},
      {"package pkg;\n int my_int;\nclass "
       "my_class;\nendclass\nendpackage\nmodule m();\n import "
       "pkg::*;\nimport pkg::",
       {kTag, "my_int"},
       ";\nimport pkg::",
       {kTag, "my_class"},
       ";\nendmodule"},
      {"package pkg;\n int my_int;\nclass "
       "my_class;\nendclass\nendpackage\nmodule m\n import "
       "pkg::*;\nimport pkg::",
       {kTag, "my_int"},
       ";\nimport pkg::",
       {kTag, "my_class"},
       ";\n();\n\nendmodule"},
  };
  for (const auto &test : kTestCases) {
    TestVerilogSyntaxRangeMatches(
        __FUNCTION__, test, [](const TextStructureView &text_structure) {
          const auto &root = text_structure.SyntaxTree();
          const auto decls = FindAllPackageImportItems(*ABSL_DIE_IF_NULL(root));

          std::vector<TreeSearchMatch> names;
          for (const auto &decl : decls) {
            const auto *name =
                GeImportedItemNameFromPackageImportItem(*decl.match);
            if (name == nullptr) continue;
            names.emplace_back(TreeSearchMatch{name, {/* ignored context*/}});
          }
          return names;
        });
  }
}

}  // namespace
}  // namespace verilog
