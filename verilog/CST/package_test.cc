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

#include "verilog/CST/package.h"

#include <memory>
#include <vector>

#include "gmock/gmock.h"
#include "gtest/gtest.h"
#include "absl/status/status.h"
#include "common/text/concrete_syntax_tree.h"
#include "common/text/symbol.h"
#include "common/text/text_structure.h"
#include "common/text/token_info.h"
#include "common/util/casts.h"
#include "common/util/logging.h"
#include "verilog/analysis/verilog_analyzer.h"

#include "common/analysis/syntax_tree_search.h"
#include "common/analysis/syntax_tree_search_test_utils.h"


#undef EXPECT_OK
#define EXPECT_OK(value) EXPECT_TRUE((value).ok())

#undef ASSERT_OK
#define ASSERT_OK(value) ASSERT_TRUE((value).ok())

namespace verilog {
namespace {

using verible::down_cast;
using verible::SyntaxTreeNode;
using verible::SyntaxTreeSearchTestCase;
using verible::TreeSearchMatch;


TEST(FindAllPackageDeclarationsTest, EmptySource) {
  VerilogAnalyzer analyzer("", "");
  EXPECT_OK(analyzer.Analyze());
  const auto& root = analyzer.Data().SyntaxTree();
  const auto package_declarations =
      FindAllPackageDeclarations(*ABSL_DIE_IF_NULL(root));
  EXPECT_TRUE(package_declarations.empty());
}

TEST(FindAllPackageDeclarationsTest, NonPackage) {
  VerilogAnalyzer analyzer("class foo; endclass", "");
  EXPECT_OK(analyzer.Analyze());
  const auto& root = analyzer.Data().SyntaxTree();
  const auto package_declarations =
      FindAllPackageDeclarations(*ABSL_DIE_IF_NULL(root));
  EXPECT_TRUE(package_declarations.empty());
}

TEST(FindAllPackageDeclarationsTest, OnePackage) {
  VerilogAnalyzer analyzer("package mod; endpackage", "");
  EXPECT_OK(analyzer.Analyze());
  const auto& root = analyzer.Data().SyntaxTree();
  const auto package_declarations =
      FindAllPackageDeclarations(*ABSL_DIE_IF_NULL(root));
  EXPECT_EQ(package_declarations.size(), 1);
}

TEST(FindAllPackageDeclarationsTest, MultiPackages) {
  VerilogAnalyzer analyzer(R"(
package pkg1;
endpackage
module mod;
endmodule
package pkg2;
endpackage
class c;
endclass
  )",
                           "");
  EXPECT_OK(analyzer.Analyze());
  const auto& root = analyzer.Data().SyntaxTree();
  const auto package_declarations =
      FindAllPackageDeclarations(*ABSL_DIE_IF_NULL(root));
  EXPECT_EQ(package_declarations.size(), 2);
}

TEST(FindAllPackageDeclarationsTest2, MultiPackages2) {
  //constexpr int kTag = 1;
  const SyntaxTreeSearchTestCase testcases[] = {
       {"package p;\n",
       "endpackage\n"},
       {"package mod; endpackage"}

  };

    const auto & test = testcases[1];
    const absl::string_view code(test.code);
    VerilogAnalyzer analyzer(code, "test-file");
    //const auto code_copy = analyzer.Data().Contents();
    ASSERT_OK(analyzer.Analyze()) << "failed on:\n" << code;
    const auto& root = analyzer.Data().SyntaxTree();

    const auto decls = FindAllPackageDeclarations(*root);

    //EXPECT_EQ(decls.size(), 1);
    //std::vector<TreeSearchMatch> types;
    const auto& package_node =
    down_cast<const SyntaxTreeNode&>(*decls.front().match);
    const auto& token = GetPackageNameToken(package_node);
    EXPECT_EQ(token.text(), "mod");
    //std::ostringstream diffs;


}

TEST(GetPackageNameTokenTest, RootIsNotAPackage) {
  VerilogAnalyzer analyzer("package foo; endpackage", "");
  EXPECT_OK(analyzer.Analyze());
  const auto& root = analyzer.Data().SyntaxTree();
  // Root node is a description list, not a package.
  EXPECT_DEATH(GetPackageNameToken(*ABSL_DIE_IF_NULL(root)),
               "kDescriptionList vs. kPackageDeclaration");
}

TEST(GetPackageNameTokenTest, ValidPackage) {
  VerilogAnalyzer analyzer("package foo; endpackage", "");
  EXPECT_OK(analyzer.Analyze());
  const auto& root = analyzer.Data().SyntaxTree();
  const auto package_declarations = FindAllPackageDeclarations(*root);
  EXPECT_EQ(package_declarations.size(), 1);
  const auto& package_node =
      down_cast<const SyntaxTreeNode&>(*package_declarations.front().match);
  // Root node is a description list, not a package.
  const auto& token = GetPackageNameToken(package_node);
  EXPECT_EQ(token.text(), "foo");
}

}  // namespace
}  // namespace verilog
