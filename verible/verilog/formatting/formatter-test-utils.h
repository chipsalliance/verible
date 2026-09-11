// Copyright 2017-2026 The Verible Authors.
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

// Shared helpers for formatter end-to-end tests.
// Penalty-sensitive cases belong in formatter-tuning_test.cc.

#ifndef VERIBLE_VERILOG_FORMATTING_FORMATTER_TEST_UTILS_H_
#define VERIBLE_VERILOG_FORMATTING_FORMATTER_TEST_UTILS_H_

#include <cstddef>
#include <sstream>
#include <string_view>

#include "gtest/gtest.h"
#include "verible/common/util/logging.h"
#include "verible/verilog/formatting/format-style.h"
#include "verible/verilog/formatting/formatter.h"

#undef EXPECT_OK
#define EXPECT_OK(value) EXPECT_TRUE((value).ok())

#undef ASSERT_OK
#define ASSERT_OK(value) ASSERT_TRUE((value).ok())

namespace verilog {
namespace formatter {

struct FormatterTestCase {
  std::string_view input;
  std::string_view expected;
};

// Runs FormatVerilog on each case and checks status + exact expected output.
template <typename CaseRange>
void RunFormatterTestCases(const FormatStyle &style, const CaseRange &cases) {
  for (const auto &test_case : cases) {
    VLOG(1) << "code-to-format:\n" << test_case.input << "<EOF>";
    std::ostringstream stream;
    const auto status =
        FormatVerilog(test_case.input, "<filename>", style, stream);
    EXPECT_OK(status) << status.message();
    EXPECT_EQ(stream.str(), test_case.expected) << "code:\n" << test_case.input;
  }
}

// Convenience: column_limit 40 / indent 2 / wrap 4 — the default E2E style.
template <typename CaseRange>
void RunFormatterTestCases40(const CaseRange &cases) {
  FormatStyle style;
  style.column_limit = 40;
  style.indentation_spaces = 2;
  style.wrap_spaces = 4;
  RunFormatterTestCases(style, cases);
}

}  // namespace formatter
}  // namespace verilog

#endif  // VERIBLE_VERILOG_FORMATTING_FORMATTER_TEST_UTILS_H_
