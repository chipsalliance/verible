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

#include "verible/verilog/CST/declaration.h"

#include <vector>

#include "gtest/gtest.h"
#include "verible/common/analysis/syntax-tree-search-test-utils.h"
#include "verible/common/analysis/syntax-tree-search.h"
#include "verible/common/text/text-structure.h"
#include "verible/common/text/tree-utils.h"
#include "verible/common/util/logging.h"
#include "verible/verilog/CST/match-test-utils.h"

#undef ASSERT_OK
#define ASSERT_OK(value) ASSERT_TRUE((value).ok())

namespace verilog {
namespace {

using verible::SyntaxTreeSearchTestCase;
using verible::TextStructureView;
using verible::TreeSearchMatch;

TEST(FindAllDataDeclarations, CountMatches) {
  constexpr int kTag = 1;  // value doesn't matter
  const SyntaxTreeSearchTestCase kTestCases[] = {
      {""},
      {"module m;\nendmodule\n"},
      {"class c;\nendclass\n"},
      {"function f;\nendfunction\n"},
      {"package p;\nendpackage\n"},
      {"task t;\nendtask\n"},
      {{kTag, "foo bar;"}, "\n"},
      {{kTag, "foo bar, baz;"}, "\n"},
      {{kTag, "foo bar;"}, "\n", {kTag, "foo baz;"}, "\n"},
      {"module m;\n"
       "  ",
       {kTag, "foo bar, baz;"},
       "\n"
       "endmodule\n"},
      {"module m;\n",
       {kTag, "foo bar;"},
       "\n",
       {kTag, "foo baz;"},
       "\n"
       "endmodule\n"},
  };
  for (const auto &test : kTestCases) {
    TestVerilogSyntaxRangeMatches(
        __FUNCTION__, test, [](const TextStructureView &text_structure) {
          const auto &root = text_structure.SyntaxTree();
          return FindAllDataDeclarations(*ABSL_DIE_IF_NULL(root));
        });
  }
}

TEST(FindAllNetVariablesTest, Various) {
  constexpr int kTag = 1;  // value doesn't matter
  const SyntaxTreeSearchTestCase kTestCases[] = {
      // bar is inside kVariableDeclarationAssignment
      // {"foo ", {kTag, "bar"}, ";\n"},
      {""},
      {"module m; endmodule\n"},
      {"module m;\nwire ", {kTag, "bar"}, ";\nendmodule\n"},
      {"module m;\nwire ", {kTag, "w"}, ", ", {kTag, "x"}, ";\nendmodule\n"},
      {"module m;\nwire ", {kTag, "bar[N]"}, ";\nendmodule\n"},
      {"module m;\nwire ", {kTag, "bar[N-1:0]"}, ";\nendmodule\n"},
      {"module m;\nwire [M]", {kTag, "bar"}, ";\nendmodule\n"},
      {"module m;\nwire [M]", {kTag, "bar[N]"}, ";\nendmodule\n"},
      {"module m;\nwire [M][B]", {kTag, "bar[N][C]"}, ";\nendmodule\n"},
      {"module m;\nwire ",
       {kTag, "w[2]"},
       ", ",
       {kTag, "x[4]"},
       ";\nendmodule\n"},
      {"module m1;\nwire ",
       {kTag, "baz"},
       ";\nendmodule\n"
       "module m2;\nwire ",
       {kTag, "bar"},
       ";\nendmodule\n"},
      {"module m;\nlogic bar;\nendmodule\n"},
      {"module m;\nreg bar;\nendmodule\n"},
  };
  for (const auto &test : kTestCases) {
    TestVerilogSyntaxRangeMatches(
        __FUNCTION__, test, [](const TextStructureView &text_structure) {
          const auto &root = text_structure.SyntaxTree();
          return FindAllNetVariables(*ABSL_DIE_IF_NULL(root));
        });
  }
}

TEST(FindAllRegisterVariablesTest, Various) {
  constexpr int kTag = 1;  // value doesn't matter
  const SyntaxTreeSearchTestCase kTestCases[] = {
      // bar is inside kVariableDeclarationAssignment
      // {"foo ", {kTag, "bar"}, ";\n"},
      {"module m;\nlogic ", {kTag, "bar"}, ";\nendmodule\n"},
      {"module m;\nlogic ", {kTag, "bar[8]"}, ";\nendmodule\n"},
      {"module m;\nlogic [4]", {kTag, "bar"}, ";\nendmodule\n"},
      {"module m;\nreg ", {kTag, "bar"}, ";\nendmodule\n"},
      {"module m;\nfoo ", {kTag, "bar"}, ";\nendmodule\n"},
      {"module m;\nfoo ", {kTag, "bar"}, ", ", {kTag, "baz"}, ";\nendmodule\n"},
      {"module m;\nlogic ",
       {kTag, "bar"},
       ";\nlogic ",
       {kTag, "baz"},
       ";\nendmodule\n"},
      {"module m;\nwire bar;\nendmodule\n"},
  };
  for (const auto &test : kTestCases) {
    TestVerilogSyntaxRangeMatches(
        __FUNCTION__, test, [](const TextStructureView &text_structure) {
          const auto &root = text_structure.SyntaxTree();
          return FindAllRegisterVariables(*ABSL_DIE_IF_NULL(root));
        });
  }
}

TEST(FindAllGateInstancesTest, Various) {
  constexpr int kTag = 1;  // value doesn't matter
  const SyntaxTreeSearchTestCase kTestCases[] = {
      // bar is inside kVariableDeclarationAssignment
      // {"foo ", {kTag, "bar"}, ";\n"},
      {"module m;\nlogic bar;\nendmodule\n"},
      {"module m;\nreg bar;\nendmodule\n"},
      {"module m;\nfoo bar;\nendmodule\n"},
      {"module m;\nfoo ", {kTag, "bar()"}, ";\nendmodule\n"},
      {"module m;\nfoo ",
       {kTag, "bar()"},
       ", ",
       {kTag, "baz()"},
       ";\nendmodule\n"},
      {"module m;\nfoo ",
       {kTag, "bar()"},
       ";\ngoo ",
       {kTag, "baz()"},
       ";\nendmodule\n"},
      {"module m;\nfoo ", {kTag, "bar(baz)"}, ";\nendmodule\n"},
      {"module m;\nfoo ", {kTag, "bar(baz, blah)"}, ";\nendmodule\n"},
      {"module m;\nfoo ", {kTag, "bar(.baz)"}, ";\nendmodule\n"},
      {"module m;\nfoo ", {kTag, "bar(.baz(baz))"}, ";\nendmodule\n"},
      {"module m;\nfoo ", {kTag, "bar(.baz(baz), .c(c))"}, ";\nendmodule\n"},
      {"module m;\nfoo #() ", {kTag, "bar()"}, ";\nendmodule\n"},
      {"module m;\nfoo #(N) ", {kTag, "bar()"}, ";\nendmodule\n"},
      {"module m;\nfoo #(.N(N)) ", {kTag, "bar()"}, ";\nendmodule\n"},
      {"module m;\nfoo #(M, N) ", {kTag, "bar()"}, ";\nendmodule\n"},
      {"module m;\nfoo #(.N(N), .M(M)) ", {kTag, "bar()"}, ";\nendmodule\n"},
      {"module m;\nfoo #(.N(N), .M(M)) ",
       {kTag, "bar()"},
       ",",
       {kTag, "blah()"},
       ";\nendmodule\n"},
  };
  for (const auto &test : kTestCases) {
    TestVerilogSyntaxRangeMatches(
        __FUNCTION__, test, [](const TextStructureView &text_structure) {
          const auto &root = text_structure.SyntaxTree();
          return FindAllGateInstances(*ABSL_DIE_IF_NULL(root));
        });
  }
}

TEST(FindAllGateInstancesTest, FindArgumentListOfGateInstance) {
  constexpr int kTag = 1;  // value doesn't matter
  const SyntaxTreeSearchTestCase kTestCases[] = {
      // bar is inside kVariableDeclarationAssignment
      // {"foo ", {kTag, "bar"}, ";\n"},
      {"module m;\nlogic bar;\nendmodule\n"},
      {"module m;\nreg bar;\nendmodule\n"},
      {"module m;\nfoo bar;\nendmodule\n"},
      {"module m;\nfoo bar", {kTag, "()"}, ";\nendmodule\n"},
      {"module m;\nfoo bar",
       {kTag, "()"},
       ", baz",
       {kTag, "()"},
       ";\nendmodule\n"},
      {"module m;\nfoo bar",
       {kTag, "()"},
       ";\ngoo baz",
       {kTag, "()"},
       ";\nendmodule\n"},
      {"module m;\nfoo bar", {kTag, "(baz)"}, ";\nendmodule\n"},
      {"module m;\nfoo bar", {kTag, "(baz, blah)"}, ";\nendmodule\n"},
      {"module m;\nfoo bar", {kTag, "(.baz)"}, ";\nendmodule\n"},
      {"module m;\nfoo bar", {kTag, "(.baz(baz))"}, ";\nendmodule\n"},
      {"module m;\nfoo bar", {kTag, "(.baz(baz), .c(c))"}, ";\nendmodule\n"},
      {"module m;\nfoo #() bar", {kTag, "()"}, ";\nendmodule\n"},
      {"module m;\nfoo #(N) bar", {kTag, "()"}, ";\nendmodule\n"},
      {"module m;\nfoo #(.N(N)) bar", {kTag, "()"}, ";\nendmodule\n"},
      {"module m;\nfoo #(M, N) bar", {kTag, "()"}, ";\nendmodule\n"},
      {"module m;\nfoo #(.N(N), .M(M)) bar", {kTag, "()"}, ";\nendmodule\n"},
      {"module m;\nfoo #(.N(N), .M(M))  bar",
       {kTag, "()"},
       ",blah",
       {kTag, "()"},
       ";\nendmodule\n"},
  };
  for (const auto &test : kTestCases) {
    TestVerilogSyntaxRangeMatches(
        __FUNCTION__, test, [](const TextStructureView &text_structure) {
          const auto &root = text_structure.SyntaxTree();
          const auto &instances = FindAllGateInstances(*ABSL_DIE_IF_NULL(root));

          std::vector<TreeSearchMatch> paren_groups;
          for (const auto &decl : instances) {
            const auto *paren_group =
                GetParenGroupFromModuleInstantiation(*decl.match);
            paren_groups.emplace_back(
                TreeSearchMatch{paren_group, {/* ignored context */}});
          }
          return paren_groups;
        });
  }
}

TEST(GetQualifiersOfDataDeclarationTest, NoQualifiers) {
  constexpr int kTag = 1;  // value doesn't matter
  const SyntaxTreeSearchTestCase kTestCases[] = {
      // each of these test cases should match exactly one data declaration
      // and have no qualifiers
      {{kTag, "foo bar;"}, "\n"},
      {"module m;\n",
       {kTag, "foo bar;"},
       "\n"
       "endmodule\n"},
      {"class c;\n",
       {kTag, "int foo;"},
       "\n"
       "endclass\n"},
      {"package p;\n",
       {kTag, "int foo;"},
       "\n"
       "endpackage\n"},
      {"function f;\n",
       {kTag, "logic bar;"},
       "\n"
       "endfunction\n"},
      {"task t;\n",
       {kTag, "logic bar;"},
       "\n"
       "endtask\n"},
  };
  for (const auto &test : kTestCases) {
    TestVerilogSyntaxRangeMatches(
        __FUNCTION__, test, [](const TextStructureView &text_structure) {
          const auto &root = text_structure.SyntaxTree();
          auto decls = FindAllDataDeclarations(*ABSL_DIE_IF_NULL(root));

          // Verify that quals is either nullptr or empty or contains only
          // nullptrs.
          for (const auto &decl : decls) {
            const auto *quals = GetQualifiersOfDataDeclaration(*decl.match);
            if (quals != nullptr) {
              for (const auto &child : quals->children()) {
                EXPECT_EQ(child, nullptr)
                    << "unexpected qualifiers:\n"
                    << verible::RawTreePrinter(*child) << "\nfailed on:\n"
                    << text_structure.Contents();
              }
            }
          }
          return decls;
        });
  }
}

TEST(GetTypeOfDataDeclarationTest, ExplicitTypes) {
  constexpr int kTag = 1;  // value doesn't matter
  const SyntaxTreeSearchTestCase kTestCases[] = {
      // each of these test cases should match exactly one data declaration
      // and have no qualifiers
      {{kTag, "foo"}, " bar;\n"},
      {{kTag, "foo"}, " bar, baz;\n"},
      {"const ", {kTag, "foo"}, " bar;\n"},
      {"const ", {kTag, "foo#(1)"}, " bar;\n"},
      {"const ", {kTag, "foo#(.N(1))"}, " bar;\n"},
      {"const ", {kTag, "foo#(1, 2, 3)"}, " bar;\n"},
      {"static ", {kTag, "foo"}, " bar;\n"},
      {"var static ", {kTag, "foo"}, " bar;\n"},
      {"automatic ", {kTag, "foo"}, " bar;\n"},

      {"class c;\n",
       {kTag, "int"},
       " foo;\n"
       "endclass\n"},
      {"class c;\n"
       "const static ",
       {kTag, "int"},
       " foo;\n"
       "endclass\n"},

      {"class c;\n"
       "function f;\n"
       "const ",
       {kTag, "int"},
       " foo;\n"
       "endfunction\n"
       "endclass\n"},

      {"class c;\n",
       "function f;\n",
       "const ",
       {kTag, "int"},
       " foo;\n",
       {kTag, "bit"},
       " bar;\n",
       "endfunction\n",
       "endclass\n"},

      {"class c;\n",
       "function f;\n",
       "const ",
       {kTag, "int"},
       " foo;\n",
       "endfunction\n",
       "endclass\n",
       "class d;\n",
       "function g;\n",
       {kTag, "bit"},
       " bar;\n",
       "endfunction\n",
       "endclass\n"},
  };
  for (const auto &test : kTestCases) {
    TestVerilogSyntaxRangeMatches(
        __FUNCTION__, test, [](const TextStructureView &text_structure) {
          const auto &root = text_structure.SyntaxTree();
          const auto decls = FindAllDataDeclarations(*ABSL_DIE_IF_NULL(root));

          std::vector<TreeSearchMatch> types;
          for (const auto &decl : decls) {
            const auto *type =
                GetInstantiationTypeOfDataDeclaration(*decl.match);
            types.emplace_back(TreeSearchMatch{type, {/* ignored context */}});
          }
          return types;
        });
  }
}

TEST(GetQualifiersOfDataDeclarationTest, SomeQualifiers) {
  constexpr int kTag = 1;  // value doesn't matter
  const SyntaxTreeSearchTestCase kTestCases[] = {
      // each of these test cases should match exactly one data declaration
      // and have no qualifiers
      {{kTag, "const"}, " foo bar;\n"},
      {{kTag, "const"}, " foo#(1) bar;\n"},
      {{kTag, "const"}, " foo bar, baz;\n"},
      {{kTag, "static"}, " foo bar;\n"},
      {{kTag, "automatic"}, " foo bar;\n"},
      {{kTag, "var"}, " foo bar;\n"},
      {{kTag, "var static"}, " foo bar;\n"},
      {{kTag, "const static"}, " foo bar;\n"},
      {"class c;\n",
       {kTag, "const static"},
       " int foo;\n"
       "endclass\n"},
      {"class c;\n",
       {kTag, "const"},
       " int foo;\n"
       "endclass\n"},
      {"class c;\n"
       "function f;\n",
       {kTag, "const"},
       " int foo;\n"
       "endfunction\n"
       "endclass\n"},
      {"class c;\n"
       "function f;\n",
       {kTag, "const"},
       " int foo;\n",
       {kTag, "const"},
       " bit bar;\n"
       "endfunction\n"
       "endclass\n"},
      {"class c;\n"
       "function f;\n",
       {kTag, "const"},
       " int foo;\n",
       "endfunction\n"
       "endclass\n"
       "class d;\n"
       "function g;\n",
       {kTag, "const"},
       " bit bar;\n"
       "endfunction\n"
       "endclass\n"},
  };
  for (const auto &test : kTestCases) {
    TestVerilogSyntaxRangeMatches(
        __FUNCTION__, test, [](const TextStructureView &text_structure) {
          const auto &root = text_structure.SyntaxTree();
          const auto decls = FindAllDataDeclarations(*ABSL_DIE_IF_NULL(root));

          std::vector<TreeSearchMatch> quals;
          for (const auto &decl : decls) {
            const auto *qual = GetQualifiersOfDataDeclaration(*decl.match);
            if (qual != nullptr) {
              quals.push_back(TreeSearchMatch{qual, {/* ignored context */}});
            } else {
              EXPECT_NE(qual, nullptr) << "decl:\n"
                                       << verible::RawTreePrinter(*decl.match);
            }
          }
          return quals;
        });
  }
}

TEST(GetInstanceListFromDataDeclarationTest, InstanceLists) {
  constexpr int kTag = 1;  // value doesn't matter
  const SyntaxTreeSearchTestCase kTestCases[] = {
      // each of these test cases should match exactly one data declaration
      // and have no qualifiers
      {"foo ", {kTag, "bar"}, ";\n"},
      {"foo ", {kTag, "bar = 0"}, ";\n"},
      {"foo ", {kTag, "bar, baz"}, ";\n"},
      {"foo ", {kTag, "bar = 1, baz = 2"}, ";\n"},
      {"foo#(1) ", {kTag, "bar"}, ";\n"},
      {"foo#(1,2) ", {kTag, "bar,baz,bam"}, ";\n"},
      {"const foo ", {kTag, "bar = 0"}, ";\n"},
      {"static foo ", {kTag, "bar = 0"}, ";\n"},
      {"class c;\n"
       "  foo ",
       {kTag, "bar"},
       ";\n"
       "endclass\n"},
      {"class c;\n"
       "  foo ",
       {kTag, "barr, bazz"},
       ";\n"
       "endclass\n"},
      {"class c;\n"
       "  const int ",
       {kTag, "barr, bazz"},
       ";\n"
       "endclass\n"},
      {"class c;\n"
       "  const int ",
       {kTag, "barr=3, bazz=4"},
       ";\n"
       "endclass\n"},
      {"function f;\n"
       "  foo ",
       {kTag, "bar"},
       ";\n"
       "endfunction\n"},
      {"function f;\n"
       "  foo ",
       {kTag, "bar, baz"},
       ";\n"
       "endfunction\n"},
      {"task t;\n"
       "  foo ",
       {kTag, "bar"},
       ";\n"
       "endtask\n"},
      {"task t;\n"
       "  foo ",
       {kTag, "bar, baz"},
       ";\n"
       "endtask\n"},
      {"package p;\n"
       "  foo ",
       {kTag, "bar"},
       ";\n"
       "endpackage\n"},
      {"package p;\n"
       "  foo ",
       {kTag, "bar, baz"},
       ";\n"
       "endpackage\n"},
  };
  for (const auto &test : kTestCases) {
    TestVerilogSyntaxRangeMatches(
        __FUNCTION__, test, [](const TextStructureView &text_structure) {
          const auto &root = text_structure.SyntaxTree();
          const auto decls = FindAllDataDeclarations(*ABSL_DIE_IF_NULL(root));

          std::vector<TreeSearchMatch> inst_lists;
          for (const auto &decl : decls) {
            const auto &insts = GetInstanceListFromDataDeclaration(*decl.match);
            inst_lists.push_back(
                TreeSearchMatch{insts, {/* ignored context */}});
          }

          return inst_lists;
        });
  }
}

TEST(GetVariableDeclarationAssign, VariableName) {
  constexpr int kTag = 1;  // value doesn't matter
  const SyntaxTreeSearchTestCase kTestCases[] = {
      {""},
      {"module m;\nendmodule\n"},
      {"class class_c;\nendclass\nmodule m;\nclass_c c = new();\nendmodule"},
      {"package pkg;\nint ",
       {kTag, "x"},
       ", ",
       {kTag, "y"},
       ";\nbit ",
       {kTag, "b1"},
       ", ",
       {kTag, "b2"},
       ";\nlogic ",
       {kTag, "l1"},
       ", ",
       {kTag, "l2"},
       ";\nstring ",
       {kTag, "s1"},
       ", ",
       {kTag, "s2"},
       ";\nendpackage"},
      {"class class_c;\nint ",
       {kTag, "x"},
       ", ",
       {kTag, "y"},
       ";\nbit ",
       {kTag, "b1"},
       ", ",
       {kTag, "b2"},
       ";\nlogic ",
       {kTag, "l1"},
       ", ",
       {kTag, "l2"},
       ";\nstring ",
       {kTag, "s1"},
       ", ",
       {kTag, "s2"},
       ";\nendclass"},
      // `branch` lexed as a (AMS) keyword, not identifier.
      {"class m;\n some_type ", {kTag, "branch"}, ";\nendclass"}};
  for (const auto &test : kTestCases) {
    TestVerilogSyntaxRangeMatches(
        __FUNCTION__, test, [](const TextStructureView &text_structure) {
          const auto &root = text_structure.SyntaxTree();
          const auto decls =
              FindAllVariableDeclarationAssignment(*ABSL_DIE_IF_NULL(root));

          std::vector<TreeSearchMatch> names;
          for (const auto &decl : decls) {
            const auto *name =
                GetUnqualifiedIdFromVariableDeclarationAssignment(*decl.match);
            names.emplace_back(TreeSearchMatch{name, {/* ignored context */}});
          }
          return names;
        });
  }
}

TEST(GetTypeFromDeclaration, GetTypeName) {
  constexpr int kTag = 1;  // value doesn't matter
  const SyntaxTreeSearchTestCase kTestCases[] = {
      {""},
      {"module m();\nendmodule"},
      {"module m();\n",
       {kTag, "some_type"},
       " x;\n",
       {kTag, "some_type"},
       " m();\n",
       {kTag, "some_type"},
       " x = new;\nendmodule"},
      {"class x;\nvirtual ", {kTag, "y"}, " m;\nendclass"}};
  for (const auto &test : kTestCases) {
    TestVerilogSyntaxRangeMatches(
        __FUNCTION__, test, [](const TextStructureView &text_structure) {
          const auto &root = text_structure.SyntaxTree();
          const auto &instances =
              FindAllDataDeclarations(*ABSL_DIE_IF_NULL(root));

          std::vector<TreeSearchMatch> names;
          for (const auto &decl : instances) {
            const auto *name =
                GetTypeIdentifierFromDataDeclaration(*decl.match);
            names.emplace_back(TreeSearchMatch{name, {/* ignored context */}});
          }
          return names;
        });
  }
}

TEST(GetStructTypeFromDeclaration, GetStructOrUnionOrEnumType) {
  constexpr int kTag = 1;  // value doesn't matter
  const SyntaxTreeSearchTestCase kTestCases[] = {
      {""},
      {"module m();\nendmodule"},
      {"package pkg;\nendpackage"},
      {"module m();\n",
       {kTag, "struct {int x;}"},
       " var1;\n",
       {kTag, "union {int x;}"},
       " var1;\n",
       {kTag, "enum {x}"},
       " var1;\nendmodule"},
      {"package pkg;\n",
       {kTag, "struct {int x;}"},
       " var1;\n",
       {kTag, "union {int x;}"},
       " var1;\n",
       {kTag, "enum {x}"},
       " var1;\nendpackage"},
  };
  for (const auto &test : kTestCases) {
    TestVerilogSyntaxRangeMatches(
        __FUNCTION__, test, [](const TextStructureView &text_structure) {
          const auto &root = text_structure.SyntaxTree();
          const auto &instances =
              FindAllDataDeclarations(*ABSL_DIE_IF_NULL(root));

          std::vector<TreeSearchMatch> types;
          for (const auto &decl : instances) {
            const auto *type =
                GetStructOrUnionOrEnumTypeFromDataDeclaration(*decl.match);
            if (type == nullptr) {
              continue;
            }
            types.emplace_back(TreeSearchMatch{type, {/* ignored context */}});
          }
          return types;
        });
  }
}

TEST(GetVariableDeclarationAssign,
     FindTrailingAssignOfVariableDeclarationAssign) {
  constexpr int kTag = 1;  // value doesn't matter
  const SyntaxTreeSearchTestCase kTestCases[] = {
      {""},
      {"module m;\nendmodule\n"},
      {"class class_c;\nendclass\nmodule m;\nclass_c c = new();\nendmodule"},
      {"package pkg;\n int x ",
       {kTag, "= 4"},
       ", y ",
       {kTag, "= 4"},
       ";\nlogic k ",
       {kTag, "= fun_call()"},
       ";\nendpackage"},
      {"class cls;\n int x ",
       {kTag, "= 4"},
       ", y ",
       {kTag, "= 4"},
       ";\nlogic k ",
       {kTag, "= fun_call()"},
       ";\nendclass"},
  };
  for (const auto &test : kTestCases) {
    TestVerilogSyntaxRangeMatches(
        __FUNCTION__, test, [](const TextStructureView &text_structure) {
          const auto &root = text_structure.SyntaxTree();
          const auto &instances =
              FindAllVariableDeclarationAssignment(*ABSL_DIE_IF_NULL(root));

          std::vector<TreeSearchMatch> paren_groups;
          for (const auto &decl : instances) {
            const auto *paren_group =
                GetTrailingExpressionFromVariableDeclarationAssign(*decl.match);
            paren_groups.emplace_back(
                TreeSearchMatch{paren_group, {/* ignored context */}});
          }
          return paren_groups;
        });
  }
}

TEST(FindAllRegisterVariablesTest, FindTrailingAssignOfRegisterVariable) {
  constexpr int kTag = 1;  // value doesn't matter
  const SyntaxTreeSearchTestCase kTestCases[] = {
      {""},
      {"module m;\nendmodule\n"},
      {"class class_c;\nendclass\nmodule m;\nclass_c c ",
       {kTag, "= new()"},
       ";\nendmodule"},
      {"module module_m();\n int x ",
       {kTag, "= 4"},
       ", y ",
       {kTag, "= 4"},
       ";\nlogic k ",
       {kTag, "= fun_call()"},
       ";\nendmodule"},
      {"task tsk();\n int x ",
       {kTag, "= 4"},
       ", y ",
       {kTag, "= 4"},
       ";\nlogic k ",
       {kTag, "= fun_call()"},
       ";\nendtask"},
      {"function int fun();\n int x ",
       {kTag, "= 4"},
       ", y ",
       {kTag, "= 4"},
       ";\nlogic k ",
       {kTag, "= fun_call()"},
       ";\nreturn 1;\nendfunction"},
  };
  for (const auto &test : kTestCases) {
    TestVerilogSyntaxRangeMatches(
        __FUNCTION__, test, [](const TextStructureView &text_structure) {
          const auto &root = text_structure.SyntaxTree();
          const auto &instances =
              FindAllRegisterVariables(*ABSL_DIE_IF_NULL(root));

          std::vector<TreeSearchMatch> paren_groups;
          for (const auto &decl : instances) {
            const auto *paren_group =
                GetTrailingExpressionFromRegisterVariable(*decl.match);
            paren_groups.emplace_back(
                TreeSearchMatch{paren_group, {/* ignored context */}});
          }
          return paren_groups;
        });
  }
}

TEST(FindAllDataDeclarationTest, FindDataDeclarationParameters) {
  constexpr int kTag = 1;  // value doesn't matter
  const SyntaxTreeSearchTestCase kTestCases[] = {
      {""},
      {"module m;\nendmodule\n"},
      {"module m;\n module_type ", {kTag, "#(2, 2)"}, " y1();\nendmodule"},
      {"module m;\n module_type ",
       {kTag, "#(.P(2), .P2(2))"},
       " y1();\nendmodule"},
      {"module m;\n module_type ",
       {kTag, "#(.P(2), .P1(3))"},
       "y1();\nendmodule"},
      {"module m;\n module_type ", {kTag, "#(x, y)"}, "y1();\nendmodule"},
  };
  for (const auto &test : kTestCases) {
    VLOG(1) << "code:\n" << test.code;
    TestVerilogSyntaxRangeMatches(
        __FUNCTION__, test, [](const TextStructureView &text_structure) {
          const auto &root = text_structure.SyntaxTree();
          const auto &decls = FindAllDataDeclarations(*ABSL_DIE_IF_NULL(root));

          std::vector<TreeSearchMatch> params;
          for (const auto &decl : decls) {
            VLOG(1) << "decl: " << verible::StringSpanOfSymbol(*decl.match);
            const auto *param_list =
                GetParamListFromDataDeclaration(*decl.match);
            if (param_list == nullptr) {
              continue;
            }
            params.emplace_back(
                TreeSearchMatch{param_list, {/* ignored context */}});
          }
          return params;
        });
  }
}

TEST(GetVariableDeclarationAssign,
     FindUnpackedDimensionOfVariableDeclarationAssign) {
  constexpr int kTag = 1;  // value doesn't matter
  const SyntaxTreeSearchTestCase kTestCases[] = {
      {""},
      {"module m;\nendmodule\n"},
      {"class class_c;\nendclass\nmodule m;\nclass_c c = new();\nendmodule"},
      {"package pkg;\nint x",
       {kTag, "[k:y]"},
       " = 4, y ",
       {kTag, "[k:y]"},
       " = 4;\nlogic k ",
       {kTag, "[k:y]"},
       " = fun_call();\nendpackage"},
      {"class cls;\n int x ",
       {kTag, "[k:y]"},
       " = 4, y ",
       {kTag, "[k:y]"},
       " = 4;\nlogic k ",
       {kTag, "[k:y]"},
       " = fun_call();\nendclass"},
  };
  for (const auto &test : kTestCases) {
    TestVerilogSyntaxRangeMatches(
        __FUNCTION__, test, [](const TextStructureView &text_structure) {
          const auto &root = text_structure.SyntaxTree();
          const auto &instances =
              FindAllVariableDeclarationAssignment(*ABSL_DIE_IF_NULL(root));

          std::vector<TreeSearchMatch> unpacked_dimensions;
          for (const auto &decl : instances) {
            const auto *unpacked_dimension =
                GetUnpackedDimensionFromVariableDeclarationAssign(*decl.match);
            unpacked_dimensions.emplace_back(
                TreeSearchMatch{unpacked_dimension, {/* ignored context */}});
          }
          return unpacked_dimensions;
        });
  }
}

TEST(FindAllRegisterVariablesTest, FindUnpackedDimensionOfRegisterVariable) {
  constexpr int kTag = 1;  // value doesn't matter
  const SyntaxTreeSearchTestCase kTestCases[] = {
      {""},
      {"module m;\nendmodule\n"},
      {"module module_m();\n int x ",
       {kTag, "[k:y]"},
       " = 4, y ",
       {kTag, "[k:y]"},
       "= 4;\nlogic k ",
       {kTag, "[k:y]"},
       "= fun_call();\nendmodule"},
      {"task tsk();\n int x ",
       {kTag, "[k:y]"},
       "= 4, y ",
       {kTag, "[k:y]"},
       "= 4;\nlogic k ",
       {kTag, "[k:y]"},
       "= fun_call();\nendtask"},
      {"function int fun();\n int x ",
       {kTag, "[k:y]"},
       "= 4, y ",
       {kTag, "[k:y]"},
       "= 4;\nlogic k ",
       {kTag, "[k:y]"},
       "= fun_call();\nreturn 1;\nendfunction"},
      {"task tsk();\n int [k:y] x ",
       {kTag, "[k:y]"},
       "= 4, y ",
       {kTag, "[k:y]"},
       "= 4;\nlogic [k:y] k ",
       {kTag, "[k:y]"},
       "= fun_call();\nendtask"},
      {"function int fun();\n int [k:y] x ",
       {kTag, "[k:y]"},
       "= 4, y ",
       {kTag, "[k:y]"},
       "= 4;\nlogic [k:y] k ",
       {kTag, "[k:y]"},
       "= fun_call();\nreturn 1;\nendfunction"},
  };
  for (const auto &test : kTestCases) {
    TestVerilogSyntaxRangeMatches(
        __FUNCTION__, test, [](const TextStructureView &text_structure) {
          const auto &root = text_structure.SyntaxTree();
          const auto &instances =
              FindAllRegisterVariables(*ABSL_DIE_IF_NULL(root));

          std::vector<TreeSearchMatch> unpacked_dimensions;
          for (const auto &decl : instances) {
            const auto *unpacked_dimension =
                GetUnpackedDimensionFromRegisterVariable(*decl.match);
            unpacked_dimensions.emplace_back(
                TreeSearchMatch{unpacked_dimension, {/* ignored context */}});
          }
          return unpacked_dimensions;
        });
  }
}

TEST(GetVariableDeclaration, FindPackedDimensionFromDataDeclaration) {
  constexpr int kTag = 1;  // value doesn't matter
  const SyntaxTreeSearchTestCase kTestCases[] = {
      {""},
      {"module m;\nendmodule\n"},
      {"module m;\n string ",
       {kTag, "[x:y]"},
       "s;\nint ",
       {kTag, "[k:y]"},
       " v1;\n logic ",
       {kTag, "[k:y]"},
       "v2, v3;\nendmodule"},
      {"class m;\n int ",
       {kTag, "[k:y]"},
       " v1;\n logic ",
       {kTag, "[k:y]"},
       " v2, v3;\nendclass"},
      {"class c;\n uint ", {kTag, "[k][y]"}, " v1;\n", "endclass"},
      {"class c;\n uint ", {kTag, "[k][y]"}, " v1 [0:N];\n", "endclass"},
      {"class c;\n foo_pkg::bar ", {kTag, "[k:0][y:0]"}, " v1;\n", "endclass"},
      {"class c;\n foo#(24) ", {kTag, "[k:0][y]"}, " v1;\n", "endclass"},
      {"class c;\n foo#(24)::bar_t ", {kTag, "[k][y:0]"}, " v1;\n", "endclass"},
      {"class c;\n uint ",
       {kTag, "[k:y]"},
       " v1;\n foo_pkg::bar ",
       {kTag, "[k:y]"},
       " v2, v3;\nendclass"},
      {"package m;\n int ",
       {kTag, "[k:y]"},
       " v1 = 2;\n logic ",
       {kTag, "[k:y]"},
       " v2 = 2;\nendpackage"},
      {"function m();\n int ",
       {kTag, "[k:y]"},
       " v1;\n logic ",
       {kTag, "[k:y]"},
       " v2, v3;\nendfunction"},
      {"package m;\n int ",
       {kTag, "[k:y]"},
       " v1 [x:y] = 2;\n logic ",
       {kTag, "[k:y]"},
       " v2 [x:y] = 2;\nendpackage"},
      {"function m();\n int ",
       {kTag, "[k:y]"},
       " v1 [x:y];\n logic ",
       {kTag, "[k:y]"},
       " v2 [x:y], v3 [x:y];\nendfunction"},
      {"package c;\n uint ", {kTag, "[x:y]"}, " x;\nendpackage"},
      {"package c;\n bar_pkg::foo x[N];\nendpackage"},
      {"package c;\n bar_pkg::foo ", {kTag, "[x]"}, " x[N];\nendpackage"},
      {"package c;\n bar_pkg::foo ", {kTag, "[x]"}, " x;\nendpackage"},
      {"package c;\n bar_pkg::foo ", {kTag, "[x:y]"}, " x;\nendpackage"},
      {"package c;\n bar_pkg::foo ", {kTag, "[x][y]"}, " x;\nendpackage"},
      {"package c;\n bar#(foo)::baz ", {kTag, "[x+1][y-1]"}, " x;\nendpackage"},
      {"class c;\n class_type x;\nendclass"},
  };
  for (const auto &test : kTestCases) {
    VLOG(1) << "code:\n" << test.code;
    TestVerilogSyntaxRangeMatches(
        __FUNCTION__, test, [](const TextStructureView &text_structure) {
          const auto &root = text_structure.SyntaxTree();
          const auto &instances =
              FindAllDataDeclarations(*ABSL_DIE_IF_NULL(root));

          std::vector<TreeSearchMatch> packed_dimensions;
          for (const auto &decl : instances) {
            const auto *packed_dimension =
                GetPackedDimensionFromDataDeclaration(*decl.match);
            if (packed_dimension == nullptr) {
              continue;
            }
            packed_dimensions.emplace_back(
                TreeSearchMatch{packed_dimension, {/* ignored context */}});
          }
          return packed_dimensions;
        });
  }
}

}  // namespace
}  // namespace verilog
