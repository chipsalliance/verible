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

#include "verilog/CST/identifier.h"

#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "gmock/gmock.h"
#include "gtest/gtest.h"
#include "absl/strings/string_view.h"
#include "common/analysis/syntax_tree_search.h"
#include "common/analysis/syntax_tree_search_test_utils.h"
#include "common/text/concrete_syntax_leaf.h"
#include "common/text/text_structure.h"
#include "common/text/token_info.h"
#include "common/util/logging.h"
#include "verilog/CST/match_test_utils.h"
#include "verilog/analysis/verilog_analyzer.h"

#undef ASSERT_OK
#define ASSERT_OK(value) ASSERT_TRUE((value).ok())

namespace verilog {
namespace {

using verible::SyntaxTreeSearchTestCase;
using verible::TextStructureView;
using verible::TreeSearchMatch;

// Finds all qualified ids are found.
TEST(IdIsQualifiedTest, VariousIds) {
  // Each test should have only 1 id, qualified or unqualified
  constexpr std::pair<absl::string_view, int> kTestCases[] = {
      {"function foo(); endfunction", 0 /* foo */},
      {"function myclass::foo(); endfunction", 1 /* myclass::foo */},
      {"task goo(); endtask", 0 /* goo */},
      {"task fff::goo(); endtask", 1 /* fff::goo */},
  };
  for (const auto test : kTestCases) {
    VerilogAnalyzer analyzer(test.first, "");
    ASSERT_OK(analyzer.Analyze());
    const auto& root = analyzer.Data().SyntaxTree();
    const auto q_ids = FindAllQualifiedIds(*root);
    ASSERT_EQ(q_ids.size(), test.second);
    if (!q_ids.empty()) {
      for (const auto& id : q_ids) {
        EXPECT_TRUE(IdIsQualified(*id.match));
      }
    } else {
      const auto u_ids = FindAllUnqualifiedIds(*root);
      for (const auto& id : u_ids) {
        EXPECT_FALSE(IdIsQualified(*id.match));
      }
    }
  }
}

// Tests that all expected unqualified ids are found.
TEST(GetIdentifierTest, UnqualifiedIds) {
  constexpr int kTag = 1;  // value doesn't matter
  const SyntaxTreeSearchTestCase kTestCases[] = {
      {"function ", {kTag, "foo"}, "(); endfunction"},
      {"function void ", {kTag, "foo"}, "(); endfunction"},
      {"function ", {kTag, "type_t"}, " ", {kTag, "foo"}, "(); endfunction"},
      {"function automatic ", {kTag, "bar"}, "(); endfunction"},
      {"function static ", {kTag, "baz"}, "(); endfunction"},
      {"package p; function ", {kTag, "foo"}, "(); endfunction endpackage"},
      {"class c; function ", {kTag, "zoo"}, "(); endfunction endclass"},
      {"function ", {kTag, "myclass"}, "::", {kTag, "foo"}, "(); endfunction"},
      {"task ", {kTag, "goo"}, "(); endtask"},
      {"task ", {kTag, "fff"}, "::", {kTag, "goo"}, "(); endtask"},
      {"function ",
       {kTag, "foo1"},
       "(); endfunction function ",
       {kTag, "foo2"},
       "(); endfunction"},
      {"int ", {kTag, "t"}, ";"},  // symbol identifier
      {"int", {kTag, "`t"}, ";"},  // macro identifier
      {"wire branch;"},            // branch is an AMS keyword
      {{kTag, "tree"}, " ", {kTag, "bark"}, ";"},
      {{kTag, "p_pkg"}, "::", {kTag, "tree"}, " ", {kTag, "bark"}, ";"},
      {{kTag, "p_pkg"}, "::", {kTag, "tree"}, "#(11) ", {kTag, "bark"}, ";"},
  };
  // Test GetIdentifier
  for (const auto& test : kTestCases) {
    VLOG(1) << "[GetIdentifier] code:\n" << test.code;
    TestVerilogSyntaxRangeMatches(
        __FUNCTION__, test, [](const TextStructureView& text_structure) {
          const auto& root = text_structure.SyntaxTree();
          const auto ids = FindAllUnqualifiedIds(*root);
          std::vector<verible::TreeSearchMatch> got_ids;
          for (const auto& id : ids) {
            const verible::SyntaxTreeLeaf* base = GetIdentifier(*id.match);
            got_ids.push_back(TreeSearchMatch{base, /* ignored context */});
          }
          return got_ids;
        });
  }
  // Test AutoUnwrapIdentifier
  for (const auto& test : kTestCases) {
    VLOG(1) << "[AutoUnwrapIdentifier] code:\n" << test.code;
    TestVerilogSyntaxRangeMatches(
        __FUNCTION__, test, [](const TextStructureView& text_structure) {
          const auto& root = text_structure.SyntaxTree();
          const auto ids = FindAllUnqualifiedIds(*root);
          std::vector<verible::TreeSearchMatch> got_ids;
          for (const auto& id : ids) {
            const verible::SyntaxTreeLeaf* base =
                AutoUnwrapIdentifier(*id.match);
            if (base == nullptr) continue;
            got_ids.push_back(TreeSearchMatch{base, /* ignored context */});
            EXPECT_EQ(AutoUnwrapIdentifier(*base), base);  // check convergence
          }
          return got_ids;
        });
  }
}

TEST(GetIdentifierTest, IdentifierUnpackedDimensions) {
  constexpr int kTag = 1;  // value doesn't matter
  const SyntaxTreeSearchTestCase kTestCases[] = {
      {"module m();\n",
       "input ",
       {kTag, "a"},
       " ,",
       {kTag, "b"},
       " ,",
       {kTag, "c"},
       ";\nendmodule"},
      {"module m();\n",
       "input wire ",
       {kTag, "a"},
       " ,",
       {kTag, "b"},
       "[0:4] ,",
       {kTag, "c"},
       ";\nendmodule"},
      {"module m();\n",
       "input ",
       {kTag, "a"},
       " ,",
       {kTag, "b"},
       "[0:4] ,",
       {kTag, "c"},
       ";\nendmodule"},
  };
  for (const auto& test : kTestCases) {
    TestVerilogSyntaxRangeMatches(
        __FUNCTION__, test, [](const TextStructureView& text_structure) {
          const auto& root = text_structure.SyntaxTree();
          const auto decls =
              FindAllIdentifierUnpackedDimensions(*ABSL_DIE_IF_NULL(root));

          std::vector<TreeSearchMatch> identifiers;
          for (const auto& decl : decls) {
            const auto* identifier =
                GetSymbolIdentifierFromIdentifierUnpackedDimensions(
                    *decl.match);
            identifiers.push_back(
                TreeSearchMatch{identifier, {/* ignored context */}});
          }
          return identifiers;
        });
  }
}

// Tests that all expected symbol identifiers are found.
TEST(FindAllSymbolIdentifierTest, VariousIds) {
  constexpr int kTag = 1;  // value doesn't matter
  const SyntaxTreeSearchTestCase kTestCases[] = {
      {"function ", {kTag, "foo"}, "(); endfunction"},
      {"function ", {kTag, "myclass"}, "::", {kTag, "foo"}, "(); endfunction"},
      {"task ", {kTag, "goo"}, "(); endtask"},
      {"task ", {kTag, "fff"}, "::", {kTag, "goo"}, "(); endtask"},
      {"class ", {kTag, "cls"}, ";\nendclass"},
      {"package ", {kTag, "pkg"}, ";\nendpackage"},
      {"module ", {kTag, "top"}, "\n",
       "import ", {kTag, "pkg"}, "::*;\n",
       "(input ", {kTag, "a"}, ");\n",
       "endmodule"},
  };
  for (const auto& test : kTestCases) {
    TestVerilogSyntaxRangeMatches(
        __FUNCTION__, test, [](const TextStructureView& text_structure) {
          const auto& root = text_structure.SyntaxTree();
          const auto symb_ids = FindAllSymbolIdentifierLeafs(*root);
          std::vector<TreeSearchMatch> identifiers;
          for (const auto& symb_id : symb_ids) {
            identifiers.push_back(
                TreeSearchMatch{symb_id.match, {/* ignored context */}});
          }
          return identifiers;
        });
  }
}

}  // namespace
}  // namespace verilog
