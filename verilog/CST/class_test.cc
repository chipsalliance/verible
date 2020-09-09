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

#include "verilog/CST/class.h"

#include <memory>
#include <sstream>
#include <vector>

#include "gmock/gmock.h"
#include "gtest/gtest.h"
#include "common/analysis/syntax_tree_search_test_utils.h"
#include "common/text/concrete_syntax_tree.h"
#include "common/text/symbol.h"
#include "common/text/text_structure.h"
#include "common/text/token_info.h"
#include "common/text/token_info_test_util.h"
#include "common/util/casts.h"
#include "common/util/logging.h"
#include "common/util/range.h"
#include "verilog/analysis/verilog_analyzer.h"

#undef ASSERT_OK
#define ASSERT_OK(value) ASSERT_TRUE((value).ok())

namespace verilog {
namespace {

using verible::SyntaxTreeSearchTestCase;
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
  for (const auto& test : kTestCases) {
    const absl::string_view code(test.code);
    VerilogAnalyzer analyzer(code, "test-file");
    const auto code_copy = analyzer.Data().Contents();
    ASSERT_OK(analyzer.Analyze()) << "failed on:\n" << code;
    const auto& root = analyzer.Data().SyntaxTree();

    const auto decls = FindAllClassDeclarations(*ABSL_DIE_IF_NULL(root));

    std::vector<TreeSearchMatch> names;
    for (const auto& decl : decls) {
      const auto& type = GetClassName(*decl.match);
      names.push_back(TreeSearchMatch{&type, {/* ignored context */}});
    }

    std::ostringstream diffs;
    EXPECT_TRUE(test.ExactMatchFindings(names, code_copy, &diffs))
        << "failed on:\n"
        << code << "\ndiffs:\n"
        << diffs.str();
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
  for (const auto& test : kTestCases) {
    const absl::string_view code(test.code);
    VerilogAnalyzer analyzer(code, "test-file");
    const auto code_copy = analyzer.Data().Contents();
    ASSERT_OK(analyzer.Analyze()) << "failed on:\n" << code;
    const auto& root = analyzer.Data().SyntaxTree();

    const auto decls = FindAllClassDeclarations(*ABSL_DIE_IF_NULL(root));

    std::vector<TreeSearchMatch> names;
    for (const auto& decl : decls) {
      const auto* name = GetClassEndLabel(*decl.match);
      names.push_back(TreeSearchMatch{name, {/* ignored context */}});
    }

    std::ostringstream diffs;
    EXPECT_TRUE(test.ExactMatchFindings(names, code_copy, &diffs))
        << "failed on:\n"
        << code << "\ndiffs:\n"
        << diffs.str();
  }
}

TEST(GetClassNameTest, NoClassEndLabelTest) {
  constexpr absl::string_view kTestCases[] = {
      {"class foo; endclass"},
  };
  for (const auto& test : kTestCases) {
    VerilogAnalyzer analyzer(test, "test-file");
    ASSERT_OK(analyzer.Analyze());
    const auto& root = analyzer.Data().SyntaxTree();

    const auto decls = FindAllClassDeclarations(*ABSL_DIE_IF_NULL(root));
    for (const auto& decl : decls) {
      const auto* type = GetClassEndLabel(*decl.match);
      EXPECT_EQ(type, nullptr);
    }
  }
}

}  // namespace
}  // namespace verilog
