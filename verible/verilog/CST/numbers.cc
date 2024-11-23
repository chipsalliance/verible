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

#include <algorithm>
#include <cctype>
#include <iterator>
#include <ostream>
#include <string>

#include "absl/strings/match.h"
#include "absl/strings/string_view.h"
#include "absl/strings/strip.h"
#include "verible/common/util/logging.h"

namespace verilog {
namespace analysis {

std::ostream &operator<<(std::ostream &stream, const BasedNumber &number) {
  if (number.ok) {
    stream << "base:" << number.base << " signed:" << number.signedness
           << " literal:" << number.literal;
  } else {
    stream << "<invalid>";
  }
  return stream;
}

BasedNumber::BasedNumber(absl::string_view base_sign, absl::string_view digits)
    : ok(false) {
  // See definition of 'based_number' nonterminal rule in verilog.y.
  if (!absl::ConsumePrefix(&base_sign, "\'")) return;

  // Optional signedness is before the base.
  signedness = absl::ConsumePrefix(&base_sign, "s") ||
               absl::ConsumePrefix(&base_sign, "S");

  // Next character is the base: [bBdDhHoO].
  CHECK_EQ(base_sign.length(), 1);
  base = std::tolower(base_sign[0]);
  static const absl::string_view valid_bases("bdho");
  if (!absl::StrContains(valid_bases, base)) return;

  // Filter out underscores.
  literal.reserve(digits.length());
  std::remove_copy(digits.begin(), digits.end(), std::back_inserter(literal),
                   '_');

  // No additional validation is done on the remaining literal string.
  ok = true;
}

}  // namespace analysis
}  // namespace verilog
