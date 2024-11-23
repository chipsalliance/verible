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
// about module declaration nodes and the structure that is actually
// created by the parser, so test *should* use the parser-generated
// syntax trees, as opposed to hand-crafted/mocked syntax trees.

#include "verible/verilog/CST/module.h"

#include <memory>
#include <vector>

#include "gtest/gtest.h"
#include "verible/common/analysis/syntax-tree-search-test-utils.h"
#include "verible/common/analysis/syntax-tree-search.h"
#include "verible/common/text/text-structure.h"
#include "verible/common/util/logging.h"
#include "verible/verilog/CST/match-test-utils.h"
#include "verible/verilog/analysis/verilog-analyzer.h"

#undef EXPECT_OK
#define EXPECT_OK(value) EXPECT_TRUE((value).ok())

#undef ASSERT_OK
#define ASSERT_OK(value) ASSERT_TRUE((value).ok())

namespace verilog {
namespace {

using verible::SyntaxTreeSearchTestCase;
using verible::TextStructureView;
using verible::TreeSearchMatch;

TEST(FindAllModuleDeclarationsTest, EmptySource) {
  VerilogAnalyzer analyzer("", "");
  EXPECT_OK(analyzer.Analyze());
  const auto &root = analyzer.Data().SyntaxTree();
  const auto module_declarations =
      FindAllModuleDeclarations(*ABSL_DIE_IF_NULL(root));
  EXPECT_TRUE(module_declarations.empty());
}

TEST(FindAllModuleDeclarationsTest, NonModule) {
  VerilogAnalyzer analyzer("class foo; endclass", "");
  EXPECT_OK(analyzer.Analyze());
  const auto &root = analyzer.Data().SyntaxTree();
  const auto module_declarations =
      FindAllModuleDeclarations(*ABSL_DIE_IF_NULL(root));
  EXPECT_TRUE(module_declarations.empty());
}

TEST(FindAllModuleDeclarationsTest, OneModule) {
  VerilogAnalyzer analyzer("module mod; endmodule", "");
  EXPECT_OK(analyzer.Analyze());
  const auto &root = analyzer.Data().SyntaxTree();
  const auto module_declarations =
      FindAllModuleDeclarations(*ABSL_DIE_IF_NULL(root));
  EXPECT_EQ(module_declarations.size(), 1);
}

TEST(FindAllModuleDeclarationsTest, MultiModules) {
  VerilogAnalyzer analyzer(R"(
module mod1;
endmodule
package p;
endpackage
module mod2(input foo);
endmodule
class c;
endclass
)",
                           "");
  EXPECT_OK(analyzer.Analyze());
  const auto &root = analyzer.Data().SyntaxTree();
  const auto module_declarations =
      FindAllModuleDeclarations(*ABSL_DIE_IF_NULL(root));
  EXPECT_EQ(module_declarations.size(), 2);
}

TEST(GetModuleNameTokenTest, RootIsNotAModule) {
  VerilogAnalyzer analyzer("module foo; endmodule", "");
  EXPECT_OK(analyzer.Analyze());
  const auto &root = analyzer.Data().SyntaxTree();
  // Root node is a description list, not a module.
  // If this happens, it is a programmer error, not user error.
  EXPECT_EQ(GetModuleName(*ABSL_DIE_IF_NULL(root)), nullptr);
}

TEST(GetModuleNameTokenTest, ValidModule) {
  constexpr int kTag = 1;  // value doesn't matter
  const SyntaxTreeSearchTestCase kTestCases[] = {
      {""},
      {"interface m;\nendinterface\n"},
      {"module ", {kTag, "foo"}, "; endmodule"},
  };
  for (const auto &test : kTestCases) {
    TestVerilogSyntaxRangeMatches(
        __FUNCTION__, test, [](const TextStructureView &text_structure) {
          const auto &root = text_structure.SyntaxTree();
          const auto &programs =
              FindAllModuleDeclarations(*ABSL_DIE_IF_NULL(root));

          std::vector<TreeSearchMatch> names;
          for (const auto &instance : programs) {
            const auto *name = GetModuleName(*instance.match);
            names.emplace_back(TreeSearchMatch{name, {/* ignored context */}});
          }
          return names;
        });
  }
}

TEST(GetModuleNameTokenTest, ValidInterface) {
  constexpr int kTag = 1;  // value doesn't matter
  const SyntaxTreeSearchTestCase kTestCases[] = {
      {""},
      {"module m;\nendmodule\n"},
      {"interface ", {kTag, "foo"}, "; endinterface"},
  };
  for (const auto &test : kTestCases) {
    TestVerilogSyntaxRangeMatches(
        __FUNCTION__, test, [](const TextStructureView &text_structure) {
          const auto &root = text_structure.SyntaxTree();
          const auto &programs =
              FindAllInterfaceDeclarations(*ABSL_DIE_IF_NULL(root));

          std::vector<TreeSearchMatch> names;
          for (const auto &instance : programs) {
            const auto *name = GetModuleName(*instance.match);
            names.emplace_back(TreeSearchMatch{name, {/* ignored context */}});
          }
          return names;
        });
  }
}

TEST(GetModuleNameTokenTest, ValidProgram) {
  constexpr int kTag = 1;  // value doesn't matter
  const SyntaxTreeSearchTestCase kTestCases[] = {
      {""},
      {"module m;\nendmodule\n"},
      {"program ", {kTag, "foo"}, "; endprogram"},
  };
  for (const auto &test : kTestCases) {
    TestVerilogSyntaxRangeMatches(
        __FUNCTION__, test, [](const TextStructureView &text_structure) {
          const auto &root = text_structure.SyntaxTree();
          const auto &programs =
              FindAllProgramDeclarations(*ABSL_DIE_IF_NULL(root));

          std::vector<TreeSearchMatch> names;
          for (const auto &instance : programs) {
            const auto *name = GetModuleName(*instance.match);
            names.emplace_back(TreeSearchMatch{name, {/* ignored context */}});
          }
          return names;
        });
  }
}

TEST(GetModulePortDeclarationListTest, ModulePorts) {
  constexpr int kTag = 1;  // value doesn't matter
  const SyntaxTreeSearchTestCase kTestCases[] = {
      // No () ports lists.
      {"module m;\nendmodule\n"},
      {"module m\t;  \n  endmodule\n"},
      {"module m;\nfunction f;\nendfunction\nendmodule\n"},
      {"module m", {kTag, "()"}, ";\nendmodule\n"},
      {"module m    ", {kTag, "()"}, "   ;\nendmodule\n"},
      {"module m", {kTag, "(input clk)"}, ";\nendmodule\n"},
      {"module m", {kTag, "(  input   clk  )"}, ";\nendmodule\n"},
      {"module m", {kTag, "(\ninput   clk\n)"}, ";\nendmodule\n"},
      {"module m", {kTag, "(\ninput   clk,\noutput foo\n)"}, ";\nendmodule\n"},
  };
  for (const auto &test : kTestCases) {
    TestVerilogSyntaxRangeMatches(
        __FUNCTION__, test, [](const TextStructureView &text_structure) {
          const auto &root = text_structure.SyntaxTree();
          const auto &modules =
              FindAllModuleDeclarations(*ABSL_DIE_IF_NULL(root));

          std::vector<TreeSearchMatch> groups;
          for (const auto &instance : modules) {
            const auto *group = GetModulePortParenGroup(*instance.match);
            if (group == nullptr) {
              continue;
            }
            groups.emplace_back(
                TreeSearchMatch{group, {/* ignored context */}});
          }
          return groups;
        });
  }
}

TEST(FindAllModuleDeclarationTest, FindModuleParameters) {
  constexpr int kTag = 1;  // value doesn't matter
  const SyntaxTreeSearchTestCase kTestCases[] = {
      {""},
      {"module m;\nendmodule\n"},
      {"module m",
       {kTag, "#(parameter x = 3, parameter y = 4)"},
       "();\nendmodule"},
      {"module m", {kTag, "#()"}, "();\nendmodule"},
      {"module m",
       {kTag, "#(parameter int x = 3,\n parameter logic y = 4)"},
       "();\nendmodule"},
  };
  for (const auto &test : kTestCases) {
    TestVerilogSyntaxRangeMatches(
        __FUNCTION__, test, [](const TextStructureView &text_structure) {
          const auto &root = text_structure.SyntaxTree();
          const auto &instances =
              FindAllModuleDeclarations(*ABSL_DIE_IF_NULL(root));

          std::vector<TreeSearchMatch> params;
          for (const auto &instance : instances) {
            const auto *decl =
                GetParamDeclarationListFromModuleDeclaration(*instance.match);
            if (decl == nullptr) {
              continue;
            }
            params.emplace_back(TreeSearchMatch{decl, {/* ignored context */}});
          }
          return params;
        });
  }
}

TEST(FindAllInterfaceDeclarationTest, FindInterfaceParameters) {
  constexpr int kTag = 1;  // value doesn't matter
  const SyntaxTreeSearchTestCase kTestCases[] = {
      {""},
      {"interface m;\nendinterface"},
      {"interface m",
       {kTag, "#(parameter x = 3, parameter y = 4)"},
       "();\nendinterface"},
      {"interface m", {kTag, "#()"}, "();\nendinterface"},
      {"interface m",
       {kTag, "#(parameter int x = 3,\n parameter logic y = 4)"},
       "();\nendinterface"},
  };
  for (const auto &test : kTestCases) {
    TestVerilogSyntaxRangeMatches(
        __FUNCTION__, test, [](const TextStructureView &text_structure) {
          const auto &root = text_structure.SyntaxTree();
          const auto &instances =
              FindAllInterfaceDeclarations(*ABSL_DIE_IF_NULL(root));

          std::vector<TreeSearchMatch> params;
          for (const auto &instance : instances) {
            const auto *decl = GetParamDeclarationListFromInterfaceDeclaration(
                *instance.match);
            if (decl == nullptr) {
              continue;
            }
            params.emplace_back(TreeSearchMatch{decl, {/* ignored context */}});
          }
          return params;
        });
  }
}

TEST(GetModulePortDeclarationListTest, ModulePortList) {
  constexpr int kTag = 1;  // value doesn't matter
  const SyntaxTreeSearchTestCase kTestCases[] = {
      {""},
      {"module m(", {kTag, "input clk"}, ");\nendmodule\n"},
      {"module m(", {kTag, "input clk, y"}, ");\nendmodule\n"},
      {"module m;\nendmodule\n"},
  };
  for (const auto &test : kTestCases) {
    TestVerilogSyntaxRangeMatches(
        __FUNCTION__, test, [](const TextStructureView &text_structure) {
          const auto &root = text_structure.SyntaxTree();
          const auto &instances =
              FindAllModuleDeclarations(*ABSL_DIE_IF_NULL(root));

          std::vector<TreeSearchMatch> lists;
          for (const auto &instance : instances) {
            const auto *list = GetModulePortDeclarationList(*instance.match);
            if (list == nullptr) {
              continue;
            }
            lists.emplace_back(TreeSearchMatch{list, {/* ignored context */}});
          }
          return lists;
        });
  }
}

TEST(GetInterfacePortDeclarationListTest, InterfacePortList) {
  constexpr int kTag = 1;  // value doesn't matter
  const SyntaxTreeSearchTestCase kTestCases[] = {
      {""},
      {"interface m(", {kTag, "input clk"}, ");\nendinterface\n"},
      {"interface m(", {kTag, "input clk, y"}, ");\nendinterface\n"},
      {"interface m;\nendinterface\n"},
  };
  for (const auto &test : kTestCases) {
    TestVerilogSyntaxRangeMatches(
        __FUNCTION__, test, [](const TextStructureView &text_structure) {
          const auto &root = text_structure.SyntaxTree();
          const auto &instances =
              FindAllInterfaceDeclarations(*ABSL_DIE_IF_NULL(root));

          std::vector<TreeSearchMatch> lists;
          for (const auto &instance : instances) {
            const auto *list = GetModulePortDeclarationList(*instance.match);
            if (list == nullptr) {
              continue;
            }
            lists.emplace_back(TreeSearchMatch{list, {/* ignored context */}});
          }
          return lists;
        });
  }
}

TEST(GetProgramPortDeclarationListTest, ProgramPortList) {
  constexpr int kTag = 1;  // value doesn't matter
  const SyntaxTreeSearchTestCase kTestCases[] = {
      {""},
      {"program m(", {kTag, "input clk"}, ");\nendprogram\n"},
      {"program m(", {kTag, "input clk, y"}, ");\nendprogram\n"},
      {"program m;\nendprogram\n"},
  };
  for (const auto &test : kTestCases) {
    TestVerilogSyntaxRangeMatches(
        __FUNCTION__, test, [](const TextStructureView &text_structure) {
          const auto &root = text_structure.SyntaxTree();
          const auto &instances =
              FindAllProgramDeclarations(*ABSL_DIE_IF_NULL(root));

          std::vector<TreeSearchMatch> lists;
          for (const auto &instance : instances) {
            const auto *list = GetModulePortDeclarationList(*instance.match);
            if (list == nullptr) {
              continue;
            }
            lists.emplace_back(TreeSearchMatch{list, {/* ignored context */}});
          }
          return lists;
        });
  }
}

TEST(FindModuleEndTest, ModuleEndName) {
  constexpr int kTag = 1;  // value doesn't matter
  const SyntaxTreeSearchTestCase kTestCases[] = {
      {""},
      {"interface m;\nendinterface"},
      {"module m;\nendmodule"},
      {"program m;\nendprogram"},
      {"module m;\nendmodule: ", {kTag, "m"}},
      {"interface m;\nendinterface: m"},
      {"program m;\nendprogram: m"},
  };
  for (const auto &test : kTestCases) {
    TestVerilogSyntaxRangeMatches(
        __FUNCTION__, test, [](const TextStructureView &text_structure) {
          const auto &root = text_structure.SyntaxTree();
          const auto &instances =
              FindAllModuleDeclarations(*ABSL_DIE_IF_NULL(root));

          std::vector<TreeSearchMatch> labels;
          for (const auto &instance : instances) {
            const auto *label = GetModuleEndLabel(*instance.match);
            if (label == nullptr) {
              continue;
            }
            labels.emplace_back(
                TreeSearchMatch{label, {/* ignored context */}});
          }
          return labels;
        });
  }
}

TEST(FindInterfaceEndTest, InterfaceEndName) {
  constexpr int kTag = 1;  // value doesn't matter
  const SyntaxTreeSearchTestCase kTestCases[] = {
      {""},
      {"interface m;\nendinterface"},
      {"module m;\nendmodule"},
      {"program m;\nendprogram"},
      {"module m;\nendmodule: m"},
      {"interface m;\nendinterface: ", {kTag, "m"}},
      {"program m;\nendprogram: m"},
  };
  for (const auto &test : kTestCases) {
    TestVerilogSyntaxRangeMatches(
        __FUNCTION__, test, [](const TextStructureView &text_structure) {
          const auto &root = text_structure.SyntaxTree();
          const auto &instances =
              FindAllInterfaceDeclarations(*ABSL_DIE_IF_NULL(root));

          std::vector<TreeSearchMatch> labels;
          for (const auto &instance : instances) {
            const auto *label = GetModuleEndLabel(*instance.match);
            if (label == nullptr) {
              continue;
            }
            labels.emplace_back(
                TreeSearchMatch{label, {/* ignored context */}});
          }
          return labels;
        });
  }
}

TEST(FindProgramEndTest, ProgramEndName) {
  constexpr int kTag = 1;  // value doesn't matter
  const SyntaxTreeSearchTestCase kTestCases[] = {
      {""},
      {"interface m;\nendinterface"},
      {"module m;\nendmodule"},
      {"program m;\nendprogram"},
      {"module m;\nendmodule: m"},
      {"interface m;\nendinterface: m"},
      {"program m;\nendprogram: ", {kTag, "m"}},
  };
  for (const auto &test : kTestCases) {
    TestVerilogSyntaxRangeMatches(
        __FUNCTION__, test, [](const TextStructureView &text_structure) {
          const auto &root = text_structure.SyntaxTree();
          const auto &instances =
              FindAllProgramDeclarations(*ABSL_DIE_IF_NULL(root));

          std::vector<TreeSearchMatch> labels;
          for (const auto &instance : instances) {
            const auto *label = GetModuleEndLabel(*instance.match);
            if (label == nullptr) {
              continue;
            }
            labels.emplace_back(
                TreeSearchMatch{label, {/* ignored context */}});
          }
          return labels;
        });
  }
}

// ModuleHeader should be included in a ProgramDeclaration
TEST(FindAllModuleHeadersTest, MatchesProgram) {
  VerilogAnalyzer analyzer("program p;\nendprogram\n", "");
  EXPECT_OK(analyzer.Analyze());
  const auto &root = analyzer.Data().SyntaxTree();
  const auto module_headers = FindAllModuleHeaders(*ABSL_DIE_IF_NULL(root));
  EXPECT_EQ(module_headers.size(), 1);
}

// ModuleHeader should be included in a InterfaceDeclaration
TEST(FindAllModuleHeadersTest, MatchesInterface) {
  VerilogAnalyzer analyzer("interface m;\nendinterface\n", "");
  EXPECT_OK(analyzer.Analyze());
  const auto &root = analyzer.Data().SyntaxTree();
  const auto module_headers = FindAllModuleHeaders(*ABSL_DIE_IF_NULL(root));
  EXPECT_EQ(module_headers.size(), 1);
}

// A class should not include ModuleHeader since they have ClassHeader
TEST(FindAllModuleHeadersTest, ClassNoMatch) {
  VerilogAnalyzer analyzer("class foo; endclass", "");
  EXPECT_OK(analyzer.Analyze());
  const auto &root = analyzer.Data().SyntaxTree();
  const auto module_headers = FindAllModuleHeaders(*ABSL_DIE_IF_NULL(root));
  EXPECT_TRUE(module_headers.empty());
}

}  // namespace
}  // namespace verilog
