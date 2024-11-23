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

#include "verible/verilog/CST/verilog-nonterminals.h"

#include <sstream>
#include <string>

#include "gtest/gtest.h"
#include "verible/common/text/constants.h"

namespace verilog {
namespace {

// This test verifies that all valid enums have a valid string representation.
TEST(VerilogNonterminalsTest, ToString) {
  constexpr int start = verible::kUntagged;
  for (int i = start; i < static_cast<int>(NodeEnum::kInvalidTag); ++i) {
    const std::string name = NodeEnumToString(NodeEnum(i));
    // All valid enums start with 'k'.
    EXPECT_EQ(name[0], 'k')
        << "Error with enum value " << i << " starting from " << start;
  }
}

// Test that operator<< prints human-readable name of enum.
TEST(VerilogNonterminalsTest, StreamOperator) {
  std::ostringstream stream;
  stream << NodeEnum::kModuleDeclaration;
  EXPECT_EQ(stream.str(), "kModuleDeclaration");
}

TEST(VerilogNonterminalsTest, BadValueToStringOver) {
  const std::string name =
      NodeEnumToString(NodeEnum(static_cast<int>(NodeEnum::kInvalidTag) + 1));
  EXPECT_NE(name[0], 'k');
}

TEST(VerilogNonterminalsTest, BadValueToStringUnder) {
  const std::string name = NodeEnumToString(NodeEnum(verible::kUntagged - 1));
  EXPECT_NE(name[0], 'k');
}

}  // namespace
}  // namespace verilog
