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

#include "verible/verilog/CST/verilog-treebuilder-utils.h"

#include "gtest/gtest.h"
#include "verible/common/text/tree-builder-test-util.h"
#include "verible/common/text/tree-utils.h"

namespace verilog {
namespace {

using verible::Leaf;

TEST(MakeParenGroupTest, Normal) {
  const auto node =
      MakeParenGroup(Leaf('(', "("), Leaf(1, "1"), Leaf(')', ")"));
  EXPECT_EQ(verible::SymbolCastToNode(*node).size(), 3);
}

TEST(MakeParenGroupTest, ErrorRecovered) {
  const auto node1 = MakeParenGroup(Leaf('(', "("), nullptr, Leaf(')', ")"));
  EXPECT_EQ(verible::SymbolCastToNode(*node1).size(), 3);
  const auto node2 = MakeParenGroup(Leaf('(', "("), nullptr, nullptr);
  EXPECT_EQ(verible::SymbolCastToNode(*node2).size(), 3);
}

TEST(MakeParenGroupTest, MissingOpenParen) {
  EXPECT_DEATH(MakeParenGroup(nullptr, Leaf(1, "1"), Leaf(')', ")")), "");
}

TEST(MakeParenGroupTest, MissingCloseParen) {
  EXPECT_DEATH(MakeParenGroup(Leaf('(', "("), Leaf(1, "1"), nullptr), "");
}

TEST(MakeParenGroupTest, WrongOpen) {
  EXPECT_DEATH(MakeParenGroup(Leaf('[', "["), Leaf(1, "1"), Leaf(')', ")")),
               "\\[");
}

TEST(MakeParenGroupTest, WrongClose) {
  EXPECT_DEATH(MakeParenGroup(Leaf('(', "("), Leaf(1, "1"), Leaf('}', "}")),
               "\\}");
}

}  // namespace
}  // namespace verilog
