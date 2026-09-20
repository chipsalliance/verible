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

#include "verible/verilog/CST/numbers.h"

#include <sstream>
#include <string_view>
#include <utility>

#include "gtest/gtest.h"

namespace verilog {
namespace analysis {
namespace {

struct BasedNumberTestCase {
  std::string_view base;
  std::string_view digits;
  BasedNumber expected;
};

// Tests that BasedNumber literals are parsed correctly.
TEST(BasedNumberTest, ParseLiteral) {
  const BasedNumberTestCase test_cases[] = {
      {.base = "\'b", .digits = "1", .expected = {'b', false, "1"}},
      {.base = "\'b", .digits = "1101", .expected = {'b', false, "1101"}},
      {.base = "\'b", .digits = "_1_1_0_1_", .expected = {'b', false, "1101"}},
      {.base = "\'sb",
       .digits = "1100_0011",
       .expected = {'b', true, "11000011"}},
      {.base = "\'b", .digits = "1101", .expected = {'b', false, "1101"}},
      {.base = "\'b", .digits = "xz01", .expected = {'b', false, "xz01"}},
      {.base = "\'B", .digits = "0", .expected = {'b', false, "0"}},
      {.base = "\'sB", .digits = "1", .expected = {'b', true, "1"}},
      {.base = "\'SB", .digits = "0_0", .expected = {'b', true, "00"}},
      {.base = "\'d", .digits = "12", .expected = {'d', false, "12"}},
      {.base = "\'D", .digits = "12", .expected = {'d', false, "12"}},
      {.base = "\'o", .digits = "66", .expected = {'o', false, "66"}},
      {.base = "\'O", .digits = "44", .expected = {'o', false, "44"}},
      {.base = "\'sO", .digits = "44", .expected = {'o', true, "44"}},
      {.base = "\'h", .digits = "F00D", .expected = {'h', false, "F00D"}},
      {.base = "\'H",
       .digits = "FEED_face",
       .expected = {'h', false, "FEEDface"}},
      {.base = "\'sh", .digits = "ADee", .expected = {'h', true, "ADee"}},
  };
  for (const auto &test : test_cases) {
    const BasedNumber actual(test.base, test.digits);
    EXPECT_TRUE(actual.ok);
    EXPECT_EQ(test.expected, actual);
  }
}

// Tests that invalid inputs are marked as not OK.
TEST(BasedNumberTest, ParseInvalidLiterals) {
  const std::pair<std::string_view, std::string_view> test_cases[] = {
      {"", ""},
      {"xx", ""},
      {"", "96"},
      {"1'b", "1"},  // valid literals start with '
  };
  for (const auto &test : test_cases) {
    const BasedNumber actual(test.first, test.second);
    EXPECT_FALSE(actual.ok);
  }
}

// Tests that human-readable representation of BasedNumber looks right.
TEST(BasedNumberTest, PrintString) {
  const BasedNumber test({'b', false, "1"});
  std::ostringstream stream;
  stream << test;
  EXPECT_EQ(stream.str(), "base:b signed:0 literal:1");
}

// Tests an invalid BasedNumber is printed as such.
TEST(BasedNumberTest, PrintStringInvalid) {
  const BasedNumber test("xx", "");
  std::ostringstream stream;
  stream << test;
  EXPECT_EQ(stream.str(), "<invalid>");
}

}  // namespace
}  // namespace analysis
}  // namespace verilog
