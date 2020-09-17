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

#include "verilog/CST/declaration.h"

#include <vector>

#include "common/analysis/syntax_tree_search.h"
#include "common/analysis/syntax_tree_search_test_utils.h"
#include "common/text/text_structure.h"
#include "common/text/tree_utils.h"
#include "gmock/gmock.h"
#include "gtest/gtest.h"
#include "verilog/CST/match_test_utils.h"

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
  for (const auto& test : kTestCases) {
    TestVerilogSyntaxRangeMatches(
        __FUNCTION__, test, [](const TextStructureView& text_structure) {
          const auto& root = text_structure.SyntaxTree();
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
  for (const auto& test : kTestCases) {
    TestVerilogSyntaxRangeMatches(
        __FUNCTION__, test, [](const TextStructureView& text_structure) {
          const auto& root = text_structure.SyntaxTree();
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
  for (const auto& test : kTestCases) {
    TestVerilogSyntaxRangeMatches(
        __FUNCTION__, test, [](const TextStructureView& text_structure) {
          const auto& root = text_structure.SyntaxTree();
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
  for (const auto& test : kTestCases) {
    TestVerilogSyntaxRangeMatches(
        __FUNCTION__, test, [](const TextStructureView& text_structure) {
          const auto& root = text_structure.SyntaxTree();
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
  for (const auto& test : kTestCases) {
    TestVerilogSyntaxRangeMatches(
        __FUNCTION__, test, [](const TextStructureView& text_structure) {
          const auto& root = text_structure.SyntaxTree();
          const auto& instances = FindAllGateInstances(*ABSL_DIE_IF_NULL(root));

          std::vector<TreeSearchMatch> paren_groups;
          for (const auto& decl : instances) {
            const auto& paren_group =
                GetParenGroupFromModuleInstantiation(*decl.match);
            paren_groups.emplace_back(
                TreeSearchMatch{&paren_group, {/* ignored context */}});
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
  for (const auto& test : kTestCases) {
    TestVerilogSyntaxRangeMatches(
        __FUNCTION__, test, [](const TextStructureView& text_structure) {
          const auto& root = text_structure.SyntaxTree();
          const auto decls = FindAllDataDeclarations(*ABSL_DIE_IF_NULL(root));

          // Verify that quals is either nullptr or empty or contains only
          // nullptrs.
          for (const auto& decl : decls) {
            const auto* quals = GetQualifiersOfDataDeclaration(*decl.match);
            if (quals != nullptr) {
              for (const auto& child : quals->children()) {
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
      {"class c;\n"
       "function f;\n"
       "const ",
       {kTag, "int"},
       " foo;\n",
       {kTag, "bit"},
       " bar;\n"
       "endfunction\n"
       "endclass\n"},
      {"class c;\n"
       "function f;\n"
       "const ",
       {kTag, "int"},
       " foo;\n",
       "endfunction\n"
       "endclass\n"
       "class d;\n"
       "function g;\n",
       {kTag, "bit"},
       " bar;\n"
       "endfunction\n"
       "endclass\n"},
  };
  for (const auto& test : kTestCases) {
    TestVerilogSyntaxRangeMatches(
        __FUNCTION__, test, [](const TextStructureView& text_structure) {
          const auto& root = text_structure.SyntaxTree();
          const auto decls = FindAllDataDeclarations(*ABSL_DIE_IF_NULL(root));

          std::vector<TreeSearchMatch> types;
          for (const auto& decl : decls) {
            const auto& type = GetTypeOfDataDeclaration(*decl.match);
            types.emplace_back(TreeSearchMatch{&type, {/* ignored context */}});
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
  for (const auto& test : kTestCases) {
    TestVerilogSyntaxRangeMatches(
        __FUNCTION__, test, [](const TextStructureView& text_structure) {
          const auto& root = text_structure.SyntaxTree();
          const auto decls = FindAllDataDeclarations(*ABSL_DIE_IF_NULL(root));

          std::vector<TreeSearchMatch> quals;
          for (const auto& decl : decls) {
            const auto* qual = GetQualifiersOfDataDeclaration(*decl.match);
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
  for (const auto& test : kTestCases) {
    TestVerilogSyntaxRangeMatches(
        __FUNCTION__, test, [](const TextStructureView& text_structure) {
          const auto& root = text_structure.SyntaxTree();
          const auto decls = FindAllDataDeclarations(*ABSL_DIE_IF_NULL(root));

          std::vector<TreeSearchMatch> inst_lists;
          for (const auto& decl : decls) {
            const auto& insts = GetInstanceListFromDataDeclaration(*decl.match);
            inst_lists.push_back(
                TreeSearchMatch{&insts, {/* ignored context */}});
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
  };
  for (const auto& test : kTestCases) {
    TestVerilogSyntaxRangeMatches(
        __FUNCTION__, test, [](const TextStructureView& text_structure) {
          const auto& root = text_structure.SyntaxTree();
          const auto decls =
              FindAllVariableDeclarationAssignment(*ABSL_DIE_IF_NULL(root));

          std::vector<TreeSearchMatch> names;
          for (const auto& decl : decls) {
            const auto& name =
                GetUnqualifiedIdFromVariableDeclarationAssignment(*decl.match);
            names.emplace_back(TreeSearchMatch{&name, {/* ignored context */}});
          }
          return names;
        });
  }
}

}  // namespace
}  // namespace verilog
