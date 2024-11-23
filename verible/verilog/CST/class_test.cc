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

// Unit tests for module-related concrete-syntax-tree functions.
//
// Testing strategy:
// The point of these tests is to validate the structure that is assumed
// about class declaration nodes and the structure that is actually
// created by the parser, so test *should* use the parser-generated
// syntax trees, as opposed to hand-crafted/mocked syntax trees.

#include "verible/verilog/CST/class.h"

#include <memory>
#include <vector>

#include "absl/strings/string_view.h"
#include "gtest/gtest.h"
#include "verible/common/analysis/syntax-tree-search-test-utils.h"
#include "verible/common/analysis/syntax-tree-search.h"
#include "verible/common/text/text-structure.h"
#include "verible/common/util/logging.h"
#include "verible/verilog/CST/match-test-utils.h"
#include "verible/verilog/analysis/verilog-analyzer.h"

#undef ASSERT_OK
#define ASSERT_OK(value) ASSERT_TRUE((value).ok())

namespace verilog {
namespace {

using verible::SyntaxTreeSearchTestCase;
using verible::TextStructureView;
using verible::TreeSearchMatch;

TEST(GetClassNameTest, ClassName) {
  constexpr int kTag = 1;  // value doesn't matter
  const SyntaxTreeSearchTestCase kTestCases[] = {
      {""},
      {"module m(); endmodule: m"},
      {"class ", {kTag, "foo"}, ";\nendclass"},
      {"class ",
       {kTag, "foo"},
       ";\nendclass\n class ",
       {kTag, "bar"},
       ";\n endclass"},
      {"module m();\n class ", {kTag, "foo"}, ";\n endclass\n endmodule: m\n"},
      {"class ",
       {kTag, "foo"},
       ";\nclass ",
       {kTag, "bar"},
       "; endclass\nendclass"},
  };
  for (const auto &test : kTestCases) {
    TestVerilogSyntaxRangeMatches(
        __FUNCTION__, test, [](const TextStructureView &text_structure) {
          const auto &root = text_structure.SyntaxTree();

          const auto decls = FindAllClassDeclarations(*ABSL_DIE_IF_NULL(root));

          std::vector<TreeSearchMatch> names;
          for (const auto &decl : decls) {
            const auto *type = GetClassName(*decl.match);
            names.push_back(TreeSearchMatch{type, {/* ignored context */}});
          }
          return names;
        });
  }
}

TEST(GetClassNameTest, ClassEndLabel) {
  constexpr int kTag = 1;  // value doesn't matter
  const SyntaxTreeSearchTestCase kTestCases[] = {
      {""},
      {"module m(); endmodule: m"},
      {"class foo;\nendclass: ", {kTag, "foo"}},
      {"class foo;\nendclass: ",
       {kTag, "foo"},
       "\n class bar;\n endclass: ",
       {kTag, "bar"}},
      {"module m();\n class foo;\n endclass: ",
       {kTag, "foo"},
       "\n endmodule: m\n"},
      {"class foo;\nclass bar;\n endclass: ",
       {kTag, "bar"},
       "\nendclass: ",
       {kTag, "foo"}},
  };
  for (const auto &test : kTestCases) {
    TestVerilogSyntaxRangeMatches(
        __FUNCTION__, test, [](const TextStructureView &text_structure) {
          const auto &root = text_structure.SyntaxTree();

          const auto decls = FindAllClassDeclarations(*ABSL_DIE_IF_NULL(root));

          std::vector<TreeSearchMatch> names;
          for (const auto &decl : decls) {
            const auto *name = GetClassEndLabel(*decl.match);
            names.push_back(TreeSearchMatch{name, {/* ignored context */}});
          }
          return names;
        });
  }
}

TEST(GetClassNameTest, NoClassEndLabelTest) {
  constexpr absl::string_view kTestCases[] = {
      {"class foo; endclass"},
  };
  for (const auto &test : kTestCases) {
    VerilogAnalyzer analyzer(test, "test-file");
    ASSERT_OK(analyzer.Analyze());
    const auto &root = analyzer.Data().SyntaxTree();

    const auto decls = FindAllClassDeclarations(*ABSL_DIE_IF_NULL(root));
    for (const auto &decl : decls) {
      const auto *type = GetClassEndLabel(*decl.match);
      EXPECT_EQ(type, nullptr);
    }
  }
}

TEST(GetClassMemberTest, GetMemberName) {
  constexpr int kTag = 1;  // value doesn't matter
  const SyntaxTreeSearchTestCase kTestCases[] = {
      {"class foo; endclass"},
      {"module m();\ninitial $display(my_class.", {kTag, "x"}, ");\nendmodule"},
      {"module m();\ninitial $display(my_class.",
       {kTag, "instance1"},
       ".",
       {kTag, "x"},
       ");\nendmodule"},
      {"module m();\ninitial x.",
       {kTag, "y"},
       ".",
       {kTag, "z"},
       " <= p.",
       {kTag, "q"},
       ".",
       {kTag, "r"},
       ";\nendmodule"},
  };
  for (const auto &test : kTestCases) {
    TestVerilogSyntaxRangeMatches(
        __FUNCTION__, test, [](const TextStructureView &text_structure) {
          const auto &root = text_structure.SyntaxTree();
          const auto &members =
              FindAllHierarchyExtensions(*ABSL_DIE_IF_NULL(root));

          std::vector<TreeSearchMatch> names;
          for (const auto &decl : members) {
            const auto *name =
                GetUnqualifiedIdFromHierarchyExtension(*decl.match);
            names.emplace_back(TreeSearchMatch{name, {/* ignored context */}});
          }
          return names;
        });
  }
}

TEST(FindAllModuleDeclarationTest, FindClassParameters) {
  constexpr int kTag = 1;  // value doesn't matter
  const SyntaxTreeSearchTestCase kTestCases[] = {
      {""},
      {"class m;\nendclass\n"},
      {"class m", {kTag, "#(parameter x = 3, parameter y = 4)"}, ";\nendclass"},
      {"class m", {kTag, "#()"}, ";\nendclass"},
      {"class m",
       {kTag, "#(parameter int x = 3,\n parameter logic y = 4)"},
       ";\nendclass"},
      {"class m",
       {kTag, "#(parameter type x = 3,\n parameter logic y = 4)"},
       ";\nendclass"},
  };
  for (const auto &test : kTestCases) {
    TestVerilogSyntaxRangeMatches(
        __FUNCTION__, test, [](const TextStructureView &text_structure) {
          const auto &root = text_structure.SyntaxTree();
          const auto &instances =
              FindAllClassDeclarations(*ABSL_DIE_IF_NULL(root));

          std::vector<TreeSearchMatch> params;
          for (const auto &instance : instances) {
            const auto *decl =
                GetParamDeclarationListFromClassDeclaration(*instance.match);
            if (decl == nullptr) {
              continue;
            }
            params.emplace_back(TreeSearchMatch{decl, {/* ignored context */}});
          }
          return params;
        });
  }
}

TEST(GetClassExtendTest, GetExtendListIdentifiers) {
  constexpr int kTag = 1;  // value doesn't matter
  const SyntaxTreeSearchTestCase kTestCases[] = {
      {""},
      {"class foo; endclass"},
      {"module m();\ninitial $display(my_class.x);\nendmodule"},
      {"class X extends ", {kTag, "Y"}, ";\nendclass"},
      {"class X extends ", {kTag, "Y::K::h"}, ";\nendclass"},
      {"class X extends ", {kTag, "Y::O"}, ";\nendclass"},
  };
  for (const auto &test : kTestCases) {
    TestVerilogSyntaxRangeMatches(
        __FUNCTION__, test, [](const TextStructureView &text_structure) {
          const auto &root = text_structure.SyntaxTree();
          const auto &members =
              FindAllClassDeclarations(*ABSL_DIE_IF_NULL(root));

          std::vector<TreeSearchMatch> identifiers;
          for (const auto &decl : members) {
            const auto *identifier = GetExtendedClass(*decl.match);
            if (identifier == nullptr) {
              continue;
            }
            identifiers.emplace_back(
                TreeSearchMatch{identifier, {/* ignored context */}});
          }
          return identifiers;
        });
  }
}

TEST(GetClassConstructorTest, GetConstructorBody) {
  constexpr int kTag = 1;  // value doesn't matter
  const SyntaxTreeSearchTestCase kTestCases[] = {
      {""},
      {"class foo; endclass"},
      {"module m;endmodule"},
      {"class foo;\nfunction new();\n",
       {kTag, "x = y;"},
       "\nendfunction\nendclass"},
  };
  for (const auto &test : kTestCases) {
    TestVerilogSyntaxRangeMatches(
        __FUNCTION__, test, [](const TextStructureView &text_structure) {
          const auto &root = text_structure.SyntaxTree();
          const auto &constructors =
              FindAllClassConstructors(*ABSL_DIE_IF_NULL(root));

          std::vector<TreeSearchMatch> bodies;
          for (const auto &constructor : constructors) {
            const auto *body =
                GetClassConstructorStatementList(*constructor.match);
            bodies.emplace_back(TreeSearchMatch{body, {/* ignored context */}});
          }
          return bodies;
        });
  }
}

TEST(GetClassConstructorTest, GetNewKeyword) {
  constexpr int kTag = 1;  // value doesn't matter
  const SyntaxTreeSearchTestCase kTestCases[] = {
      {""},
      {"class foo; endclass"},
      {"module m;endmodule"},
      {"class foo;\nfunction ", {kTag, "new"}, "();\n\nendfunction\nendclass"},
  };
  for (const auto &test : kTestCases) {
    TestVerilogSyntaxRangeMatches(
        __FUNCTION__, test, [](const TextStructureView &text_structure) {
          const auto &root = text_structure.SyntaxTree();
          const auto &constructors =
              FindAllClassConstructors(*ABSL_DIE_IF_NULL(root));

          std::vector<TreeSearchMatch> keywords;
          for (const auto &constructor : constructors) {
            const auto *keyword =
                GetNewKeywordFromClassConstructor(*constructor.match);
            keywords.emplace_back(
                TreeSearchMatch{keyword, {/* ignored context */}});
          }
          return keywords;
        });
  }
}

}  // namespace
}  // namespace verilog
