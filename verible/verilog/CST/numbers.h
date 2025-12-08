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

#ifndef VERIBLE_VERILOG_CST_NUMBERS_H_
#define VERIBLE_VERILOG_CST_NUMBERS_H_

#include <iosfwd>
#include <string>
#include <string_view>

namespace verilog {
namespace analysis {

// BasedNumber subdivides the information in a TK_BasedNumber token
// from verilog.lex.
// Generally, this has the form: \'[sS]?{base}[ ]*{digits}
// See verilog.lex for more details.
//
// Examples:
//   'b111 is (binary, unsigned, '111')
//   'h aaaa_5555 is (hex, unsigned, 'aaaa5555'
struct BasedNumber {
  // Numeric base, one of [bdho].
  char base;
  // True if number was annotated as signed, otherwise is unsigned.
  bool signedness;
  // This is the literal value with underscores removed.
  std::string literal;

  // False if there is an error parsing a BasedNumber.
  bool ok;

  // Construct parses a literal string based on
  // '{Dec|Bin|Oct|Hex}{Base|Digits}' from verilog.lex.
  // base_sign is lexed as one token, e.g. 'b, 'sb (signed).
  BasedNumber(std::string_view base_sign, std::string_view digits);

  BasedNumber(char base_, bool sign_, std::string_view text)
      : base(base_), signedness(sign_), literal(text), ok(true) {}

  bool operator==(const BasedNumber &rhs) const {
    return base == rhs.base && signedness == rhs.signedness &&
           literal == rhs.literal && ok && rhs.ok;
  }
};

std::ostream &operator<<(std::ostream &stream, const BasedNumber &number);

}  // namespace analysis
}  // namespace verilog

#endif  // VERIBLE_VERILOG_CST_NUMBERS_H_
