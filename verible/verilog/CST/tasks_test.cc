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

#include "verible/verilog/CST/tasks.h"

#include <memory>
#include <vector>

#include "absl/strings/string_view.h"
#include "gtest/gtest.h"
#include "verible/common/analysis/syntax-tree-search-test-utils.h"
#include "verible/common/analysis/syntax-tree-search.h"
#include "verible/common/text/concrete-syntax-leaf.h"
#include "verible/common/text/concrete-syntax-tree.h"
#include "verible/common/text/text-structure.h"
#include "verible/common/text/token-info.h"
#include "verible/common/text/tree-utils.h"
#include "verible/common/util/casts.h"
#include "verible/common/util/logging.h"
#include "verible/verilog/CST/identifier.h"
#include "verible/verilog/CST/match-test-utils.h"
#include "verible/verilog/CST/verilog-nonterminals.h"
#include "verible/verilog/analysis/verilog-analyzer.h"
#include "verible/verilog/parser/verilog-token-enum.h"

#undef ASSERT_OK
#define ASSERT_OK(value) ASSERT_TRUE((value).ok())

namespace verilog {
namespace {

using verible::down_cast;
using verible::SyntaxTreeSearchTestCase;
using verible::TextStructureView;
using verible::TreeSearchMatch;

TEST(FindAllTaskDeclarationsTest, Various) {
  constexpr int kTag = 1;  // value doesn't matter
  const SyntaxTreeSearchTestCase kTestCases[] = {
      {""},
      {"class foo; endclass"},
      {"module m(); endmodule: m"},
      {"function f(); endfunction"},
      {{kTag, "task foo();\nendtask"}},
      {{kTag, "task foo();\nendtask : foo"}},
      {{kTag, "task t();\nint x = 1;\nendtask"}},
      {// task inside package
       "package pkg;\n",
       {kTag,
        "task t;\n "
        "int x = 1;"
        "\nendtask"},
       "\nendpackage"},
      {// multiple tasks
       {kTag, "task foo();\nendtask"},
       "\n",
       {kTag, "task foo2();\nendtask"}},
      {// task declaration as class method
       "class bar;\n",
       {kTag, "task foo();\nendtask"},
       " endclass"},
      {// task declaration inside module
       "module bar;\n",
       {kTag, "task foo();\nendtask"},
       " endmodule"},
  };
  for (const auto &test : kTestCases) {
    TestVerilogSyntaxRangeMatches(
        __FUNCTION__, test, [](const TextStructureView &text_structure) {
          const auto &root = text_structure.SyntaxTree();
          return FindAllTaskDeclarations(*ABSL_DIE_IF_NULL(root));
        });
  }
}

TEST(FindAllTaskPrototypesTest, Various) {
  constexpr int kTag = 1;  // value doesn't matter
  const SyntaxTreeSearchTestCase kTestCases[] = {
      {// task declaration is not considered a prototype
       "task foo();\n"
       "endtask"},
      {// task declaration is not considered a prototype
       "class bar;\n"
       "  task foo();\n"
       "  endtask\n"
       "endclass"},
      {// virtual task can be defined and is overridable
       "class bar;\n"
       "  virtual task foo();\n"
       "  endtask\n"
       "endclass"},
      {// pure virtual is a prototype
       "class bar;\n",
       "pure virtual ",
       {kTag, "task foo();"},
       "\nendclass"},
      {// extern is a prototype
       "class bar;\n",
       "extern ",
       {kTag, "task foo();"},
       "\nendclass"},
  };
  for (const auto &test : kTestCases) {
    TestVerilogSyntaxRangeMatches(
        __FUNCTION__, test, [](const TextStructureView &text_structure) {
          const auto &root = text_structure.SyntaxTree();
          return FindAllTaskPrototypes(*ABSL_DIE_IF_NULL(root));
        });
  }
  // Prototype header span the same range as their enclosing prototypes.
  for (const auto &test : kTestCases) {
    TestVerilogSyntaxRangeMatches(
        __FUNCTION__, test, [](const TextStructureView &text_structure) {
          const auto &root = text_structure.SyntaxTree();
          std::vector<TreeSearchMatch> headers;
          for (const auto &proto :
               FindAllTaskPrototypes(*ABSL_DIE_IF_NULL(root))) {
            headers.push_back(TreeSearchMatch{
                GetTaskPrototypeHeader(*proto.match), /* no context */});
          }
          return headers;
        });
  }
}

TEST(TaskPrototypesIdsTest, Various) {
  constexpr int kTag = 1;  // value doesn't matter
  const SyntaxTreeSearchTestCase kTestCases[] = {
      {// pure virtual is a prototype
       "class bar;\n",
       "pure virtual task ",
       {kTag, "foo"},
       "();\n"
       "endclass"},
      {// extern is a prototype
       "class bar;\n",
       "extern task ",
       {kTag, "ggghhh"},
       "();\n"
       "endclass"},
  };
  for (const auto &test : kTestCases) {
    TestVerilogSyntaxRangeMatches(
        __FUNCTION__, test, [](const TextStructureView &text_structure) {
          const auto &root = text_structure.SyntaxTree();
          std::vector<TreeSearchMatch> ids;
          for (const auto &proto :
               FindAllTaskPrototypes(*ABSL_DIE_IF_NULL(root))) {
            const auto *header = GetTaskPrototypeHeader(*proto.match);
            const auto *id = ABSL_DIE_IF_NULL(GetTaskHeaderId(*header));
            EXPECT_TRUE(verible::SymbolCastToNode(*id).MatchesTag(
                NodeEnum::kUnqualifiedId));
            ids.push_back(TreeSearchMatch{id, /* no context */});
          }
          return ids;
        });
  }
}

TEST(FindAllTaskHeadersTest, Various) {
  constexpr int kTag = 1;  // value doesn't matter
  const SyntaxTreeSearchTestCase kTestCases[] = {
      {""},
      {"class c; endclass"},
      {"package c; endpackage"},
      {"module m; endmodule"},
      {"function f; endfunction"},
      {{kTag, "task foo();"},
       "\n"
       "endtask"},
      {"class bar;\n"
       "  ",
       {kTag, "task foo();"},
       "\n"
       "  endtask\n"
       "endclass"},
      {"class bar;\n",
       {kTag, "virtual task foo();"},
       "\n"
       "  endtask\n"
       "endclass"},
      {// pure virtual is a prototype
       "class bar;\n",
       "pure virtual ",
       {kTag, "task foo();"},
       "\nendclass"},
      {// extern is a prototype
       "class bar;\n",
       "extern ",
       {kTag, "task foo();"},
       "\nendclass"},
  };
  for (const auto &test : kTestCases) {
    TestVerilogSyntaxRangeMatches(
        __FUNCTION__, test, [](const TextStructureView &text_structure) {
          const auto &root = text_structure.SyntaxTree();
          return FindAllTaskHeaders(*ABSL_DIE_IF_NULL(root));
        });
  }
}

TEST(GetTaskHeaderTest, DeclarationsHeader) {
  constexpr int kTag = 1;  // value doesn't matter
  const SyntaxTreeSearchTestCase kTestCases[] = {
      {{kTag, "task foo();"}, " endtask"},
      {{kTag, "task my_class::foo();"}, " endtask"},
      {"class c; ", {kTag, "task foo();"}, " endtask endclass"},
      {"module m; ", {kTag, "task foo();"}, " endtask endmodule"},
      {"package p; ", {kTag, "task foo();"}, " endtask endpackage"},
  };
  for (const auto &test : kTestCases) {
    TestVerilogSyntaxRangeMatches(
        __FUNCTION__, test, [](const TextStructureView &text_structure) {
          // Root node is a description list, not a task.
          const auto &root = text_structure.SyntaxTree();
          const auto task_declarations = FindAllTaskDeclarations(*root);
          std::vector<TreeSearchMatch> headers;
          for (const auto &decl : task_declarations) {
            const auto &task_node = verible::SymbolCastToNode(*decl.match);
            headers.push_back(
                TreeSearchMatch{GetTaskHeader(task_node), /* no context */});
          }
          return headers;
        });
  }
}

TEST(GetTaskLifetimeTest, NoLifetimeDeclared) {
  VerilogAnalyzer analyzer("task foo(); endtask", "");
  ASSERT_OK(analyzer.Analyze());
  // Root node is a description list, not a task.
  const auto &root = analyzer.Data().SyntaxTree();
  const auto task_declarations = FindAllTaskDeclarations(*root);
  ASSERT_EQ(task_declarations.size(), 1);
  const auto &task_node =
      verible::SymbolCastToNode(*task_declarations.front().match);
  const auto *lifetime = GetTaskLifetime(task_node);
  EXPECT_EQ(lifetime, nullptr);
}

TEST(GetTaskLifetimeTest, StaticLifetimeDeclared) {
  VerilogAnalyzer analyzer("task static foo(); endtask", "");
  ASSERT_OK(analyzer.Analyze());
  // Root node is a description list, not a task.
  const auto &root = analyzer.Data().SyntaxTree();
  const auto task_declarations = FindAllTaskDeclarations(*root);
  ASSERT_EQ(task_declarations.size(), 1);
  const auto &task_node =
      verible::SymbolCastToNode(*task_declarations.front().match);
  const auto *lifetime = GetTaskLifetime(task_node);
  const auto &leaf = verible::SymbolCastToLeaf(*ABSL_DIE_IF_NULL(lifetime));
  EXPECT_EQ(leaf.get().token_enum(), TK_static);
}

TEST(GetTaskLifetimeTest, AutomaticLifetimeDeclared) {
  VerilogAnalyzer analyzer("task automatic foo(); endtask", "");
  ASSERT_OK(analyzer.Analyze());
  // Root node is a description list, not a task.
  const auto &root = analyzer.Data().SyntaxTree();
  const auto task_declarations = FindAllTaskDeclarations(*root);
  ASSERT_EQ(task_declarations.size(), 1);
  const auto &task_node =
      verible::SymbolCastToNode(*task_declarations.front().match);
  const auto *lifetime = GetTaskLifetime(task_node);
  const auto &leaf = verible::SymbolCastToLeaf(*ABSL_DIE_IF_NULL(lifetime));
  EXPECT_EQ(leaf.get().token_enum(), TK_automatic);
}

TEST(GetTaskIdTest, UnqualifiedIds) {
  constexpr int kTag = 1;  // don't care
  const SyntaxTreeSearchTestCase kTestCases[] = {
      {"task ", {kTag, "foo"}, "(); endtask"},
      {"task automatic ", {kTag, "bar"}, "(); endtask"},
      {"task static ", {kTag, "baz"}, "(); endtask"},
      {"package p; task ", {kTag, "foo"}, "(); endtask endpackage"},
      {"class c; task ", {kTag, "zoo"}, "(); endtask endclass"},
      {"task ", {kTag, "myclass"}, "::", {kTag, "foo"}, "(); endtask"},
  };
  for (const auto &test : kTestCases) {
    TestVerilogSyntaxRangeMatches(
        __FUNCTION__, test, [](const TextStructureView &text_structure) {
          // Root node is a description list, not a task.
          const auto &root = text_structure.SyntaxTree();
          const auto task_declarations = FindAllTaskDeclarations(*root);
          std::vector<TreeSearchMatch> got_ids;
          for (const auto &task_decl : task_declarations) {
            const auto &task_node =
                down_cast<const verible::SyntaxTreeNode &>(*task_decl.match);
            const auto *task_id = GetTaskId(task_node);
            const auto ids = FindAllUnqualifiedIds(*task_id);
            for (const auto &id : ids) {
              got_ids.push_back(TreeSearchMatch{
                  GetIdentifier(*ABSL_DIE_IF_NULL(id.match)),
                  /* no context */});
            }
          }
          return got_ids;
        });
  }
}

TEST(GetTaskIdTest, QualifiedIds) {
  constexpr int kTag = 1;  // value doesn't matter
  const SyntaxTreeSearchTestCase kTestCases[] = {
      {"task foo(); endtask"},
      {"class c; task foo(); endtask endclass"},
      {"task ", {kTag, "myclass::foo"}, "(); endtask"},
  };
  for (const auto &test : kTestCases) {
    TestVerilogSyntaxRangeMatches(
        __FUNCTION__, test, [](const TextStructureView &text_structure) {
          // Root node is a description list, not a task.
          const auto &root = text_structure.SyntaxTree();
          const auto task_declarations = FindAllTaskDeclarations(*root);
          std::vector<TreeSearchMatch> ids;
          for (const auto &decl : task_declarations) {
            const auto &task_node = verible::SymbolCastToNode(*decl.match);
            const auto *task_id = GetTaskId(task_node);
            for (const auto &id : FindAllQualifiedIds(*task_id)) {
              ids.push_back(id);
            }
          }
          return ids;
        });
  }
}

TEST(GetTaskHeaderTest, GetTaskName) {
  constexpr int kTag = 1;  // value doesn't matter
  const SyntaxTreeSearchTestCase kTestCases[] = {
      {""},
      {"module m(); endmodule: m"},
      {"task ", {kTag, "foo"}, "(int a, bit b);\nendtask"},
      {"task ",
       {kTag, "foo"},
       "();\n endtask\n task ",
       {kTag, "bar"},
       "()\n; endtask"},
      {"module my_module;\ntask ",
       {kTag, "inner_task"},
       "(int my_args);\n$display(my_arg2);\nendtask\nendmodule"},
      {"class task_class;\ntask ",
       {kTag, "my_task"},
       "(input int a, b, output int c);\nc = a + b;\nendtask\nendclass"},
      {"package my_pkg;\ntask automatic ",
       {kTag, "my_task"},
       "(input int a, b, output int c);\nc = a + b;\nendtask\nendpackage"},
      {"class m;\n virtual task ",
       {kTag, "my_task"},
       "();\n endtask\n  endclass"},
      {"class m;\n static task ",
       {kTag, "my_task"},
       "();\n endtask\n  endclass"},
  };
  for (const auto &test : kTestCases) {
    TestVerilogSyntaxRangeMatches(
        __FUNCTION__, test, [](const TextStructureView &text_structure) {
          const auto &root = text_structure.SyntaxTree();

          const auto decls = FindAllTaskDeclarations(*ABSL_DIE_IF_NULL(root));

          std::vector<TreeSearchMatch> types;
          for (const auto &decl : decls) {
            const auto *type = GetTaskName(*decl.match);
            types.push_back(TreeSearchMatch{type, {/* ignored context */}});
          }
          return types;
        });
  }
}  // namespace

TEST(GetTaskHeaderTest, GetTaskBody) {
  constexpr int kTag = 1;  // value doesn't matter
  const SyntaxTreeSearchTestCase kTestCases[] = {
      {""},
      {"module m(); endmodule: m"},
      {"task t();\n ", {kTag, "int x = 1;"}, "\nendtask"},
      {"task t;\n ", {kTag, "int x = 1;"}, "\nendtask"},
      {"package pkg;\ntask t;\n ",
       {kTag, "int x = 1;"},
       "\nendtask\nendpackage"},
      {"package pkg;\ntask t();\n ",
       {kTag, "int x = 1;"},
       "\nendtask\nendpackage"},
  };
  for (const auto &test : kTestCases) {
    TestVerilogSyntaxRangeMatches(
        __FUNCTION__, test, [](const TextStructureView &text_structure) {
          const auto &root = text_structure.SyntaxTree();

          const auto decls = FindAllTaskDeclarations(*ABSL_DIE_IF_NULL(root));

          std::vector<TreeSearchMatch> bodies;
          for (const auto &decl : decls) {
            const auto &body = GetTaskStatementList(*decl.match);
            bodies.push_back(TreeSearchMatch{body, {/* ignored context */}});
          }
          return bodies;
        });
  }
}

}  // namespace
}  // namespace verilog
