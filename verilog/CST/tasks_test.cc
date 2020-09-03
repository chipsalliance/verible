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

#include "verilog/CST/tasks.h"

#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "absl/strings/string_view.h"
#include "common/analysis/syntax_tree_search.h"
#include "common/analysis/syntax_tree_search_test_utils.h"
#include "common/text/concrete_syntax_leaf.h"
#include "common/text/concrete_syntax_tree.h"
#include "common/text/symbol.h"
#include "common/text/text_structure.h"
#include "common/text/token_info.h"
#include "common/util/casts.h"
#include "common/util/logging.h"
#include "gmock/gmock.h"
#include "gtest/gtest.h"
#include "verilog/CST/identifier.h"
#include "verilog/analysis/verilog_analyzer.h"
#include "verilog/parser/verilog_token_enum.h"

#undef ASSERT_OK
#define ASSERT_OK(value) ASSERT_TRUE((value).ok())

namespace verilog {
namespace {

using verible::down_cast;
using verible::SyntaxTreeSearchTestCase;
using verible::TreeSearchMatch;

TEST(FindAllTaskDeclarationsTest, EmptySource) {
  VerilogAnalyzer analyzer("", "");
  ASSERT_OK(analyzer.Analyze());
  const auto& root = analyzer.Data().SyntaxTree();
  const auto task_declarations =
      FindAllTaskDeclarations(*ABSL_DIE_IF_NULL(root));
  EXPECT_TRUE(task_declarations.empty());
}

TEST(FindAllTaskDeclarationsTest, OnlyClass) {
  VerilogAnalyzer analyzer("class foo; endclass", "");
  ASSERT_OK(analyzer.Analyze());
  const auto& root = analyzer.Data().SyntaxTree();
  const auto task_declarations =
      FindAllTaskDeclarations(*ABSL_DIE_IF_NULL(root));
  EXPECT_TRUE(task_declarations.empty());
}

TEST(FindAllTaskDeclarationsTest, OnlyModule) {
  VerilogAnalyzer analyzer("module foo; endmodule", "");
  ASSERT_OK(analyzer.Analyze());
  const auto& root = analyzer.Data().SyntaxTree();
  const auto task_declarations =
      FindAllTaskDeclarations(*ABSL_DIE_IF_NULL(root));
  EXPECT_TRUE(task_declarations.empty());
}

TEST(FindAllTaskDeclarationsTest, OneTask) {
  VerilogAnalyzer analyzer("task foo(); endtask", "");
  ASSERT_OK(analyzer.Analyze());
  const auto& root = analyzer.Data().SyntaxTree();
  const auto task_declarations =
      FindAllTaskDeclarations(*ABSL_DIE_IF_NULL(root));
  ASSERT_EQ(task_declarations.size(), 1);
}

TEST(FindAllTaskDeclarationsTest, TwoTasks) {
  VerilogAnalyzer analyzer(R"(
task foo(); endtask
task foo2(); endtask
)",
                           "");
  ASSERT_OK(analyzer.Analyze());
  const auto& root = analyzer.Data().SyntaxTree();
  const auto task_declarations =
      FindAllTaskDeclarations(*ABSL_DIE_IF_NULL(root));
  EXPECT_EQ(task_declarations.size(), 2);
}

TEST(FindAllTaskDeclarationsTest, TaskInsideClass) {
  VerilogAnalyzer analyzer("class bar; task foo(); endtask endclass", "");
  ASSERT_OK(analyzer.Analyze());
  const auto& root = analyzer.Data().SyntaxTree();
  const auto task_declarations =
      FindAllTaskDeclarations(*ABSL_DIE_IF_NULL(root));
  EXPECT_EQ(task_declarations.size(), 1);
}

TEST(FindAllTaskDeclarationsTest, TaskInsideModule) {
  VerilogAnalyzer analyzer("module bar; task foo(); endtask endmodule", "");
  ASSERT_OK(analyzer.Analyze());
  const auto& root = analyzer.Data().SyntaxTree();
  const auto task_declarations =
      FindAllTaskDeclarations(*ABSL_DIE_IF_NULL(root));
  EXPECT_EQ(task_declarations.size(), 1);
}

// TODO(kathuriac): Add test case for task inside cross_body_item see
// (verilog.y)

TEST(GetTaskHeaderTest, Header) {
  const char* kTestCases[] = {
      "task foo(); endtask",
      "class c; task foo(); endtask endclass",
      "module m; task foo(); endtask endmodule",
  };
  for (const auto test : kTestCases) {
    VerilogAnalyzer analyzer(test, "");
    ASSERT_OK(analyzer.Analyze());
    // Root node is a description list, not a task.
    const auto& root = analyzer.Data().SyntaxTree();
    const auto task_declarations = FindAllTaskDeclarations(*root);
    ASSERT_EQ(task_declarations.size(), 1);
    const auto& task_node = down_cast<const verible::SyntaxTreeNode&>(
        *task_declarations.front().match);
    GetTaskHeader(task_node);
    // Reaching here is success.  Function has internal checks already.
  }
}

TEST(GetTaskLifetimeTest, NoLifetimeDeclared) {
  VerilogAnalyzer analyzer("task foo(); endtask", "");
  ASSERT_OK(analyzer.Analyze());
  // Root node is a description list, not a task.
  const auto& root = analyzer.Data().SyntaxTree();
  const auto task_declarations = FindAllTaskDeclarations(*root);
  ASSERT_EQ(task_declarations.size(), 1);
  const auto& task_node = down_cast<const verible::SyntaxTreeNode&>(
      *task_declarations.front().match);
  const auto* lifetime = GetTaskLifetime(task_node);
  EXPECT_EQ(lifetime, nullptr);
}

TEST(GetTaskLifetimeTest, StaticLifetimeDeclared) {
  VerilogAnalyzer analyzer("task static foo(); endtask", "");
  ASSERT_OK(analyzer.Analyze());
  // Root node is a description list, not a task.
  const auto& root = analyzer.Data().SyntaxTree();
  const auto task_declarations = FindAllTaskDeclarations(*root);
  ASSERT_EQ(task_declarations.size(), 1);
  const auto& task_node = down_cast<const verible::SyntaxTreeNode&>(
      *task_declarations.front().match);
  const auto* lifetime = GetTaskLifetime(task_node);
  const auto* leaf = down_cast<const verible::SyntaxTreeLeaf*>(lifetime);
  EXPECT_EQ(leaf->get().token_enum(), TK_static);
}

TEST(GetTaskLifetimeTest, AutomaticLifetimeDeclared) {
  VerilogAnalyzer analyzer("task automatic foo(); endtask", "");
  ASSERT_OK(analyzer.Analyze());
  // Root node is a description list, not a task.
  const auto& root = analyzer.Data().SyntaxTree();
  const auto task_declarations = FindAllTaskDeclarations(*root);
  ASSERT_EQ(task_declarations.size(), 1);
  const auto& task_node = down_cast<const verible::SyntaxTreeNode&>(
      *task_declarations.front().match);
  const auto* lifetime = GetTaskLifetime(task_node);
  const auto* leaf = down_cast<const verible::SyntaxTreeLeaf*>(lifetime);
  EXPECT_EQ(leaf->get().token_enum(), TK_automatic);
}

TEST(GetTaskIdTest, UnqualifiedIds) {
  const std::pair<std::string, std::vector<absl::string_view>> kTestCases[] = {
      {"task foo(); endtask", {"foo"}},
      {"task automatic bar(); endtask", {"bar"}},
      {"task static baz(); endtask", {"baz"}},
      {"package p; task foo(); endtask endpackage", {"foo"}},
      {"class c; task zoo(); endtask endclass", {"zoo"}},
      {"task myclass::foo(); endtask", {"myclass", "foo"}},
  };
  for (const auto test : kTestCases) {
    VerilogAnalyzer analyzer(test.first, "");
    ASSERT_OK(analyzer.Analyze());
    // Root node is a description list, not a task.
    const auto& root = analyzer.Data().SyntaxTree();
    const auto task_declarations = FindAllTaskDeclarations(*root);
    ASSERT_EQ(task_declarations.size(), 1);
    const auto& task_node = down_cast<const verible::SyntaxTreeNode&>(
        *task_declarations.front().match);
    const auto* task_id = GetTaskId(task_node);
    const auto ids = FindAllUnqualifiedIds(*task_id);
    std::vector<absl::string_view> got_ids;
    for (const auto& id : ids) {
      const verible::SyntaxTreeLeaf* base = GetIdentifier(*id.match);
      got_ids.push_back(ABSL_DIE_IF_NULL(base)->get().text());
    }
    EXPECT_EQ(got_ids, test.second);
  }
}

TEST(GetTaskIdTest, QualifiedIds) {
  const std::pair<std::string, int> kTestCases[] = {
      {"task foo(); endtask", 0},
      {"task myclass::foo(); endtask", 1},
  };
  for (const auto test : kTestCases) {
    VerilogAnalyzer analyzer(test.first, "");
    ASSERT_OK(analyzer.Analyze());
    // Root node is a description list, not a task.
    const auto& root = analyzer.Data().SyntaxTree();
    const auto task_declarations = FindAllTaskDeclarations(*root);
    ASSERT_EQ(task_declarations.size(), 1);
    const auto& task_node = down_cast<const verible::SyntaxTreeNode&>(
        *task_declarations.front().match);
    const auto* task_id = GetTaskId(task_node);
    const auto ids = FindAllQualifiedIds(*task_id);
    EXPECT_EQ(ids.size(), test.second);
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
  };
  for (const auto& test : kTestCases) {
    const absl::string_view code(test.code);
    VerilogAnalyzer analyzer(code, "test-file");
    const auto code_copy = analyzer.Data().Contents();
    ASSERT_OK(analyzer.Analyze()) << "failed on:\n" << code;
    const auto& root = analyzer.Data().SyntaxTree();

    const auto decls = FindAllTaskDeclarations(*ABSL_DIE_IF_NULL(root));

    std::vector<TreeSearchMatch> types;
    for (const auto& decl : decls) {
      const auto* type = GetTaskName(*decl.match);
      types.push_back(TreeSearchMatch{type, {/* ignored context */}});
    }

    std::ostringstream diffs;
    EXPECT_TRUE(test.ExactMatchFindings(types, code_copy, &diffs))
        << "failed on:\n"
        << code << "\ndiffs:\n"
        << diffs.str();
  }
}  // namespace

}  // namespace
}  // namespace verilog
