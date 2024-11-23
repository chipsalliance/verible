// Copyright 2017-2021 The Verible Authors.
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

#include "verible/verilog/analysis/checkers/truncated-numeric-literal-rule.h"

#include <initializer_list>
#include <string>

#include "absl/strings/str_cat.h"
#include "absl/strings/string_view.h"
#include "gtest/gtest.h"
#include "verible/common/analysis/linter-test-utils.h"
#include "verible/common/analysis/syntax-tree-linter-test-utils.h"
#include "verible/verilog/analysis/verilog-analyzer.h"
#include "verible/verilog/parser/verilog-token-enum.h"

namespace verilog {
namespace analysis {
namespace {

using verible::LintTestCase;
using verible::RunConfiguredLintTestCases;

TEST(TruncatedNumericLiteralRuleTest, TruncatedBinaryNumbers) {
  constexpr int kToken = TK_BinDigits;
  const std::initializer_list<LintTestCase> kTestCases = {
      {""},
      {"localparam x = 0;"},
      {"localparam x = 1;"},
      {"localparam x = 1'b?;"},
      {"localparam x = 1'bz;"},
      {"localparam x = 1'bx;"},
      {"localparam x = 1'b", {kToken, "zz"}, ";"},
      {"localparam x = 1'b", {kToken, "xx"}, ";"},
      {"localparam x = 1'b", {kToken, "??"}, ";"},

      // Not doing macro expansion yet, but we know that it uses at least 1 bit
      {"localparam x = 1'b`SOME_MACRO;"},
      {"localparam x = 0'b", {MacroIdentifier, "`SOME_MACRO"}, ";"},

      {"localparam x = 0'b", {kToken, "0"}, ";"},  // Even a zero uses one bit
      {"localparam x = 0'b", {kToken, "1"}, ";"},
      {"localparam x = 3'b111;"},
      {"localparam x = 3'b00000111;"},
      {"localparam x = 3'b11_1;"},
      {"localparam x = 3'b", {kToken, "1111"}, ";"},
      {"localparam x = 3'b", {kToken, "00001111"}, ";"},
  };

  RunConfiguredLintTestCases<VerilogAnalyzer, TruncatedNumericLiteralRule>(
      kTestCases, "");
}

TEST(TruncatedNumericLiteralRuleTest, TooShortHexNumbers) {
  constexpr int kToken = TK_HexDigits;
  const std::string superlong(1001, 'F');
  const std::string exp = absl::StrCat("localparam x = 4004'h", superlong, ";");
  const absl::string_view good_long_expression = exp;

  const std::initializer_list<LintTestCase> kTestCases = {
      {"localparam x = 1'h1;"},
      {"localparam x = 0'h", {kToken, "0"}, ";"},
      {"localparam x = 0'h", {kToken, "1"}, ";"},
      {"localparam x = 4'h?;"},
      {"localparam x = 3'h?;"},
      {"localparam x = 1'h?;"},  // ? can mean anything 1..4 bits, all ok.
      {"localparam x = 4'h", {kToken, "??"}, ";"},  // Two digits exceed 4 bit
      {"localparam x = 5'h??;"},  // Minimum that two digits use is 5 bits

      // Same spiel with z and x
      {"localparam x = 3'hz;"},
      {"localparam x = 4'h", {kToken, "zz"}, ";"},
      {"localparam x = 5'hzz;"},

      {"localparam x = 3'hx;"},
      {"localparam x = 4'h", {kToken, "xx"}, ";"},
      {"localparam x = 5'hxx;"},

      {"localparam x = 4'h", {kToken, "xz"}, ";"},  // or a mix of these

      // Not doing macro expansion yet, but we know that it uses at least 1 bit
      {"localparam x = 1'h`SOME_MACRO;"},
      {"localparam x = 0'h", {MacroIdentifier, "`SOME_MACRO"}, ";"},

      {"localparam x = 4'hf;"},
      {"localparam x = 6'h2f;"},
      {"localparam x = 6'h2_f;"},
      {"localparam x = 6'h0000000002f;"},  // MSB fits ? good.
      {"localparam x = 5'h", {kToken, "2f"}, ";"},
      {"localparam x = 5'h", {kToken, "00000002f"}, ";"},
      {"localparam x = 5'h1f;"},
      {"localparam x = 16'habcd;"},
      {"localparam x = 15'h", {kToken, "abcd"}, ";"},
      {"localparam x = 16'hab_cd;"},

      {good_long_expression},
      {"localparam x = 4003'h", {kToken, superlong}, ";"},

      {"localparam x = -16'hffff;"},  // TODO: should we complain about -(-1)?
  };

  RunConfiguredLintTestCases<VerilogAnalyzer, TruncatedNumericLiteralRule>(
      kTestCases, "");
}

TEST(TruncatedNumericLiteralRuleTest, TruncatedOctalNumbers) {
  constexpr int kToken = TK_OctDigits;
  const std::initializer_list<LintTestCase> kTestCases = {
      {"localparam x = 1'o1;"},
      {"localparam x = 3'o7;"},
      {"localparam x = 2'o3;"},
      {"localparam x = 2'o", {kToken, "4"}, ";"},
      {"localparam x = 2'o", {kToken, "7"}, ";"},
      {"localparam x = 8'o377;"},
      {"localparam x = 8'o000000377;"},
      {"localparam x = 8'o", {kToken, "477"}, ";"},
  };

  RunConfiguredLintTestCases<VerilogAnalyzer, TruncatedNumericLiteralRule>(
      kTestCases, "");
}

TEST(TruncatedNumericLiteralRuleTest, TruncatedDecimalNumbers) {
  constexpr int kToken = TK_DecDigits;
  // A number longer than can be parsed as double to force fallback of fallback.
  const std::string superlong(500, '9');
  const std::string exp = absl::StrCat("localparam x = 1661'd", superlong, ";");
  const absl::string_view good_long_expression = exp;

  const std::initializer_list<LintTestCase> kTestCases = {
      {"localparam x = 1'd1;"},
      {"localparam x = 0'd", {kToken, "0"}, ";"},
      {"localparam x = 0'd", {kToken, "1"}, ";"},

      {"localparam x = 1'dz;"},  // Not dealing with special digits
      {"localparam x = 1'dx;"},
      {"localparam x = 1'd?;"},

      // Negative numbers: we really only look that the literal bits fit
      {"localparam x = -4'd15;"},
      {"localparam x = -4'd", {kToken, "16"}, ";"},

      // 16 bit boundary
      {"localparam x = 16'd65535;"},
      {"localparam x = 16'd", {kToken, "65536"}, ";"},
      {"localparam x = 17'd65536;"},

      // TODO: should we warn about implicit negative numbers ?
      {"localparam x = -16'd65535;"},

      // 32 Bit.
      {"localparam x = 32'd4294967295;"},                    // 2^32-1
      {"localparam x = 32'd", {kToken, "4294967296"}, ";"},  // too long
      {"localparam x = 33'd4294967296;"},  // needs one more bit

      // 64 bit
      {"localparam x = 64'd18446744073709551615;"},  // 2^64-1
      {"localparam x = 64'd", {kToken, "18446744073709551616"}, ";"},
      {"localparam x = 65'd18446744073709551616;"},

      // 2^100-1.
      {"localparam x = 100'd1267650600228229401496703205375;"},
      {"localparam x = 100'd",  // Value +1 doesn't fit in 100 bits.
       {kToken, "1267650600228229401496703205376"},
       ";"},

      // 2^128-1
      {"localparam x = 128'd340282366920938463463374607431768211455;"},
      {"localparam x = 127'd",  // ... but doesn't fit in 127 bits.
       {kToken, "340282366920938463463374607431768211455"},
       ";"},

      /*
       * For larger than 128 bit numbers, we only do best effort, but making
       * sure to not give false positives, only false negatives.
       *
       * In practice, super long decimals are probably not something someone
       * would use in code anyway, so best effort is probably good enough.
       */

      // This number (2^128, so 1<<129) is dealt with by the heuristic,
      // and accurately detected as not fitting into 128 bits.
      // The heuristic would estimate 128 bits, but we also know that
      // we need to be at least 129 bits if we entered the heuristic realm :)
      {"localparam x = 128'd",
       {kToken, "340282366920938463463374607431768211456"},
       ";"},
      {"localparam x = 129'd340282366920938463463374607431768211456;"},

      /* larger numbers will only be somewhat accurate and we underestimate
       * the number of bits, so there will be some non-reported issues,
       * false negatives.
       */

      // This number, 2^145-1 is accepted with 145 bit precision.
      {"localparam x = 145'd44601490397061246283071436545296723011960831;"},

      // We correctly recognize that this can't be represented in 144 bits.
      {"localparam x = 144'd",
       {kToken, "44601490397061246283071436545296723011960831"},
       ";"},

      // However, due to our heuristic, we don't actually recognize that this
      // one-more-bit number 2^145 will need 146 bits...
      {"localparam x = 145'd44601490397061246283071436545296723011960832;"},

      // In fact, we don't really notice any change beyond the first 15 or 16
      // first digits or so (as we internally parse it as double) and still
      // accept this as 145 bits even though it really needs 146 bits by now.
      // Erring on the side of not complaining.
      // And who describes huge numbers in decimal anyway...
      {"localparam x = 145'd",
       {kToken, "44601490397061700000000000000000000000000000"},
       //----------------------^ this needed to change from 2 to >= 6
       ";"},

      // This one should be ceil(log(10^500-1)/log(2)) = 1661 bits
      // long, but we only start to complain below 1658 bits.
      {good_long_expression},
      {"localparam x = 1657'd", {kToken, superlong}, ";"},
  };

  RunConfiguredLintTestCases<VerilogAnalyzer, TruncatedNumericLiteralRule>(
      kTestCases, "");
}

}  // namespace
}  // namespace analysis
}  // namespace verilog
