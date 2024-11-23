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

#include "verible/verilog/analysis/checkers/undersized-binary-literal-rule.h"

#include <initializer_list>

#include "absl/status/status.h"
#include "gtest/gtest.h"
#include "verible/common/analysis/linter-test-utils.h"
#include "verible/common/analysis/syntax-tree-linter-test-utils.h"
#include "verible/verilog/analysis/verilog-analyzer.h"
#include "verible/verilog/parser/verilog-token-enum.h"

namespace verilog {
namespace analysis {
namespace {

using verible::LintTestCase;
using verible::RunApplyFixCases;
using verible::RunConfiguredLintTestCases;

TEST(UndersizedBinaryLiteralTest, ConfigurationPass) {
  UndersizedBinaryLiteralRule rule;
  absl::Status status;
  EXPECT_TRUE((status = rule.Configure("")).ok()) << status.message();
  EXPECT_TRUE((status = rule.Configure("bin:true;oct:true;hex:true")).ok())
      << status.message();
}

TEST(UndersizedBinaryLiteralTest, TooShortBinaryNumbers) {
  constexpr int kToken = TK_BinDigits;
  const std::initializer_list<LintTestCase> kTestCases = {
      {""},
      {"localparam x = 0;"},
      {"localparam x = 1;"},
      {"localparam x = '0;"},
      {"localparam x = '1;"},
      {"localparam x = 2'b0;"},                     // exception granted for 0
      {"localparam x = 3'b", {kToken, "00"}, ";"},  // only 2 0s for 3 bits
      {"localparam x = 32'b0;"},                    // exception granted for 0
      {"localparam x = 2'b", {kToken, "1"}, ";"},
      {"localparam x = 2'b?;"},  // exception granted for ?
      {"localparam x = 2'b", {kToken, "x"}, ";"},
      {"localparam x = 2'b", {kToken, "z"}, ";"},
      {"localparam x = 2'b ", {kToken, "1"}, ";"},    // with space after base
      {"localparam x = 2'b ", {kToken, "_1_"}, ";"},  // with underscores
      {"localparam x = 1'b0;"},
      {"localparam x = 1'b1;"},
      {"localparam x = 32'd20;"},  // decimal numbers not treated
      {"localparam x = 8'b 0001_1000;"},
      {"localparam x = 8'b ", {kToken, "001_1000"}, ";"},
      {"localparam x = 8'b ", {kToken, "0001_100"}, ";"},
      {"localparam x = 8'b ", {kToken, "????_xx1"}, ";"},
      {"localparam x = 8'b ", {kToken, "1??_xz10"}, ";"},
      {"localparam x = 0 + 2'b", {kToken, "1"}, ";"},
      {"localparam x = 3'b", {kToken, "10"}, " + 3'b", {kToken, "1"}, ";"},
      {"localparam x = 2'b ", {kToken, "x"}, " & 2'b ", {kToken, "1"}, ";"},
      {"localparam x = 5'b?????;"},
      {"localparam x = 5'b?;"},                     // exception granted for ?
      {"localparam x = 5'b", {kToken, "??"}, ";"},  // only 2 ?s for 5 bits
  };

  RunConfiguredLintTestCases<VerilogAnalyzer, UndersizedBinaryLiteralRule>(
      kTestCases, "bin:true");
}

TEST(UndersizedBinaryLiteralTest, BinaryNumbersConfiguredDontCare) {
  const std::initializer_list<LintTestCase> kTestCases = {
      {"localparam x = 0;"},
      {"localparam x = 1;"},
      {"localparam x = 3'b00;"},
      {"localparam x = 32'b000;"},
  };

  RunConfiguredLintTestCases<VerilogAnalyzer, UndersizedBinaryLiteralRule>(
      kTestCases, "bin:false");
}

TEST(UndersizedBinaryLiteralTest, TooShortHexNumbers) {
  constexpr int kToken = TK_HexDigits;
  const std::initializer_list<LintTestCase> kTestCases = {
      {"localparam x = 16'h0;"},  // Exception granted for single 0 and ?
      {"localparam x = 16'h?;"},

      {"localparam x = 16'h", {kToken, "00"}, ";"},
      {"localparam x = 16'h", {kToken, "??"}, ";"},

      {"localparam x = 1'h1;"},
      {"localparam x = 4'hf;"},
      {"localparam x = 5'h", {kToken, "f"}, ";"},
      {"localparam x = 5'h1f;"},
      {"localparam x = 16'h0001;"},
      {"localparam x = 16'h00_01;"},
      {"localparam x = 16'h", {kToken, "001"}, ";"},
      {"localparam x = 16'h", {kToken, "0_01"}, ";"},
      {"localparam x = 2'habcd;"},  // Note: truncated values are ok for this
                                    // rule
  };

  RunConfiguredLintTestCases<VerilogAnalyzer, UndersizedBinaryLiteralRule>(
      kTestCases, "hex:true");
}

TEST(UndersizedBinaryLiteralTest, HexNumbersConfiguredDontCare) {
  const std::initializer_list<LintTestCase> kTestCases = {
      {"localparam x = 16'h0;"},
      {"localparam x = 16'h?;"},
      {"localparam x = 5'hf;"},
      {"localparam x = 16'h001;"},
  };

  RunConfiguredLintTestCases<VerilogAnalyzer, UndersizedBinaryLiteralRule>(
      kTestCases, "hex:false");
}

TEST(UndersizedBinaryLiteralTest, TooShortOctalNumbers) {
  constexpr int kToken = TK_OctDigits;
  const std::initializer_list<LintTestCase> kTestCases = {
      {"localparam x = 12'o0;"},  // Exception granted for 0 and ?
      {"localparam x = 12'o?;"},

      {"localparam x = 12'o", {kToken, "00"}, ";"},
      {"localparam x = 12'o", {kToken, "??"}, ";"},

      {"localparam x = 1'o1;"},
      {"localparam x = 3'o7;"},
      {"localparam x = 8'o777;"},  // Note: truncated values are ok for this
                                   // rule
      {"localparam x = 9'o777;"},
      {"localparam x = 9'o7_7_7;"},
      {"localparam x = 9'o", {kToken, "77"}, ";"},
      {"localparam x = 9'o", {kToken, "7_7"}, ";"},
      {"localparam x = 4'o17;"},
  };

  RunConfiguredLintTestCases<VerilogAnalyzer, UndersizedBinaryLiteralRule>(
      kTestCases, "oct:true");
}

TEST(UndersizedBinaryLiteralTest, OctalNumbersConfiguredDontCare) {
  const std::initializer_list<LintTestCase> kTestCases = {
      {"localparam x = 9'o77;"},
      {"localparam x = 12'o77;"},
  };

  RunConfiguredLintTestCases<VerilogAnalyzer, UndersizedBinaryLiteralRule>(
      kTestCases, "oct:false");
}

TEST(UndersizedBinaryLiteralTest, DecimalNumbersNeverCare) {
  const std::initializer_list<LintTestCase> kTestCases = {
      {"localparam x = 32'd42;"},
      {"localparam x = 32'd123456789;"},
  };

  RunConfiguredLintTestCases<VerilogAnalyzer, UndersizedBinaryLiteralRule>(
      kTestCases, "");
}

TEST(UndersizedBinaryLiteralTest, ApplyAutoFix) {
  const std::initializer_list<verible::AutoFixInOut> kTestCases = {
      {"localparam x = 32'hAB;", "localparam x = 32'h000000AB;"},
      {"localparam x = 16'hAB;", "localparam x = 16'h00AB;"},
      {"localparam x = 9'hAB;", "localparam x = 9'h0AB;"},
      {"localparam x = 8'b101;", "localparam x = 8'b00000101;"},
      {"localparam x = 9'o7;", "localparam x = 9'o007;"},
      {"localparam x = 8'o7;", "localparam x = 8'o007;"},
  };
  RunApplyFixCases<VerilogAnalyzer, UndersizedBinaryLiteralRule>(
      kTestCases, "bin:true;hex:true;oct:true;autofix:true");
}

TEST(UndersizedBinaryLiteralRule, AutoFixDigitZeroProvideUnsizedAlternative) {
  // In the case lint_zero is configured, we provide alternatives to write
  // just a simple '0

  // Alternatives the auto fix offers
  constexpr int kFirstFix = 0;
  constexpr int kSecondFix = 1;
  constexpr int kThirdFix = 2;

  const std::initializer_list<verible::AutoFixInOut> kTestCases = {
      // First suggested alternative: replace just with simple '0
      {"localparam x = 32'h0;", "localparam x = '0;", kFirstFix},
      // We only apply this for unsigned values
      {"localparam x = 32'sh0;", "localparam x = 32'sh00000000;", kFirstFix},

      // Next alternative is the standard expansion
      {"localparam x = 32'h0;", "localparam x = 32'h00000000;", kSecondFix},

      // Third alternative would be what we anyway would do with single digit
      // suggestions: convert to decimal.
      {"localparam x = 32'h0;", "localparam x = 32'd0;", kThirdFix},
  };
  RunApplyFixCases<VerilogAnalyzer, UndersizedBinaryLiteralRule>(
      kTestCases,
      "bin:true;hex:true;oct:true;lint_zero:true;"
      "autofix:true");
}

TEST(UndersizedBinaryLiteralRule, AutoFixSingleDigitProvideDecimalAlternative) {
  // Alternatives the auto fix offers
  constexpr int kFirstFix = 0;
  constexpr int kSecondFix = 1;
  const std::initializer_list<verible::AutoFixInOut> kTestCases = {
      // First choice: zero expand
      {"localparam x = 32'h1;", "localparam x = 32'h00000001;", kFirstFix},
      {"localparam x = 32'sh1;", "localparam x = 32'sh00000001;", kFirstFix},
      {"localparam x = 32'h9;", "localparam x = 32'h00000009;", kFirstFix},
      {"localparam x = 32'sh9;", "localparam x = 32'sh00000009;", kFirstFix},

      // Second choice: convert to decimal
      {"localparam x = 32'h1;", "localparam x = 32'd1;", kSecondFix},
      {"localparam x = 32'sh1;", "localparam x = 32'sd1;", kSecondFix},
      {"localparam x = 32'h9;", "localparam x = 32'd9;", kSecondFix},
      {"localparam x = 32'sh9;", "localparam x = 32'sd9;", kSecondFix},
  };
  RunApplyFixCases<VerilogAnalyzer, UndersizedBinaryLiteralRule>(
      kTestCases,
      "bin:true;hex:true;oct:true;"
      "autofix:true");
}

TEST(UndersizedBinaryLiteralRule, AutoFixProvideInferredSize) {
  // Alternatives the auto fix offers
  constexpr int kFirstFix = 0;
  constexpr int kSecondFix = 1;
  const std::initializer_list<verible::AutoFixInOut> kTestCases = {
      // First choice: zero expand
      {"localparam x = 32'h10;", "localparam x = 32'h00000010;", kFirstFix},
      {"localparam x = 3'b01;", "localparam x = 3'b001;", kFirstFix},
      {"localparam x = 8'o77;", "localparam x = 8'o077;", kFirstFix},

      // Second choice: Adjust size to inferred size
      {"localparam x = 32'h10;", "localparam x = 8'h10;", kSecondFix},
      {"localparam x = 3'b01;", "localparam x = 2'b01;", kSecondFix},
      {"localparam x = 8'o77;", "localparam x = 6'o77;", kSecondFix},
  };
  RunApplyFixCases<VerilogAnalyzer, UndersizedBinaryLiteralRule>(
      kTestCases,
      "bin:true;hex:true;oct:true;"
      "autofix:true");
}

}  // namespace
}  // namespace analysis
}  // namespace verilog
