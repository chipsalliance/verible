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
#include "common/text/concrete_syntax_leaf.h"
#include "common/text/text_structure.h"
#include "common/text/token_info.h"
#include "common/util/logging.h"
#include "verilog/analysis/verilog_analyzer.h"

#undef ASSERT_OK
#define ASSERT_OK(value) ASSERT_TRUE((value).ok())

namespace verilog {
namespace {

// Finds all qualified ids are found.
TEST(IdIsQualifiedTest, VariousIds) {
  // Each test should have only 1 id, qualified or unqualified
  const std::pair<std::string, int> kTestCases[] = {
      {"function foo(); endfunction", 0 /* foo */},
      {"function myclass::foo(); endfunction", 1 /* myclass::foo */},
      {"task goo(); endtask", 0 /* goo */},
      {"task fff::goo(); endtask", 1 /* fff::goo */},
  };
  for (const auto test : kTestCases) {
    VerilogAnalyzer analyzer(test.first, "");
    ASSERT_OK(analyzer.Analyze());
    const auto& root = analyzer.Data().SyntaxTree();
    auto q_ids = FindAllQualifiedIds(*root);
    ASSERT_EQ(q_ids.size(), test.second);
    if (!q_ids.empty()) {
      for (const auto& id : q_ids) {
        EXPECT_TRUE(IdIsQualified(*id.match));
      }
    } else {
      auto u_ids = FindAllUnqualifiedIds(*root);
      for (const auto& id : u_ids) {
        EXPECT_FALSE(IdIsQualified(*id.match));
      }
    }
  }
}

// Tests that all expected unqualified ids are found.
TEST(GetIdentifierTest, UnqualifiedIds) {
  const std::pair<std::string, std::vector<absl::string_view>> kTestCases[] = {
      {"function foo(); endfunction", {"foo"}},
      {"function void foo(); endfunction", {"foo"}},
      {"function type_t foo(); endfunction", {"type_t", "foo"}},
      {"function automatic bar(); endfunction", {"bar"}},
      {"function static baz(); endfunction", {"baz"}},
      {"package p; function foo(); endfunction endpackage", {"foo"}},
      {"class c; function zoo(); endfunction endclass", {"zoo"}},
      {"function myclass::foo(); endfunction", {"myclass", "foo"}},
      {"task goo(); endtask", {"goo"}},
      {"task fff::goo(); endtask", {"fff", "goo"}},
      {"function foo1(); endfunction function foo2(); endfunction",
       {"foo1", "foo2"}},
  };
  for (const auto test : kTestCases) {
    VerilogAnalyzer analyzer(test.first, "");
    ASSERT_OK(analyzer.Analyze());
    const auto& root = analyzer.Data().SyntaxTree();
    const auto ids = FindAllUnqualifiedIds(*root);
    {
      std::vector<absl::string_view> got_ids;
      for (const auto& id : ids) {
        const verible::SyntaxTreeLeaf* base = GetIdentifier(*id.match);
        got_ids.push_back(ABSL_DIE_IF_NULL(base)->get().text);
      }
      EXPECT_EQ(got_ids, test.second);
    }
    {
      std::vector<absl::string_view> got_ids;
      for (const auto& id : ids) {
        const verible::SyntaxTreeLeaf* base = AutoUnwrapIdentifier(*id.match);
        got_ids.push_back(ABSL_DIE_IF_NULL(base)->get().text);
        EXPECT_EQ(AutoUnwrapIdentifier(*base), base);  // check convergence
      }
      EXPECT_EQ(got_ids, test.second);
    }
  }
}

}  // namespace
}  // namespace verilog
