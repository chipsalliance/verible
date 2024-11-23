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
#include <utility>

#include "absl/strings/string_view.h"
#include "gtest/gtest.h"

namespace verilog {
namespace analysis {
namespace {

struct BasedNumberTestCase {
  absl::string_view base;
  absl::string_view digits;
  BasedNumber expected;
};

// Tests that BasedNumber literals are parsed correctly.
TEST(BasedNumberTest, ParseLiteral) {
  const BasedNumberTestCase test_cases[] = {
      {"\'b", "1", {'b', false, "1"}},
      {"\'b", "1101", {'b', false, "1101"}},
      {"\'b", "_1_1_0_1_", {'b', false, "1101"}},
      {"\'sb", "1100_0011", {'b', true, "11000011"}},
      {"\'b", "1101", {'b', false, "1101"}},
      {"\'b", "xz01", {'b', false, "xz01"}},
      {"\'B", "0", {'b', false, "0"}},
      {"\'sB", "1", {'b', true, "1"}},
      {"\'SB", "0_0", {'b', true, "00"}},
      {"\'d", "12", {'d', false, "12"}},
      {"\'D", "12", {'d', false, "12"}},
      {"\'o", "66", {'o', false, "66"}},
      {"\'O", "44", {'o', false, "44"}},
      {"\'sO", "44", {'o', true, "44"}},
      {"\'h", "F00D", {'h', false, "F00D"}},
      {"\'H", "FEED_face", {'h', false, "FEEDface"}},
      {"\'sh", "ADee", {'h', true, "ADee"}},
  };
  for (const auto &test : test_cases) {
    const BasedNumber actual(test.base, test.digits);
    EXPECT_TRUE(actual.ok);
    EXPECT_EQ(test.expected, actual);
  }
}

// Tests that invalid inputs are marked as not OK.
TEST(BasedNumberTest, ParseInvalidLiterals) {
  const std::pair<absl::string_view, absl::string_view> test_cases[] = {
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
