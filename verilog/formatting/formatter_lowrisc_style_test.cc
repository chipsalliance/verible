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

#include "verilog/formatting/formatter.h"

#include <memory>
#include <sstream>
#include <string>
#include <vector>

#include <fstream>

#include "absl/status/status.h"
#include "absl/strings/match.h"
#include "absl/strings/string_view.h"
#include "common/formatting/align.h"
#include "common/strings/position.h"
#include "common/text/text_structure.h"
#include "common/util/logging.h"
#include "gmock/gmock.h"
#include "gtest/gtest.h"
#include "verilog/analysis/verilog_analyzer.h"
#include "verilog/formatting/format_style.h"
#include "verilog/formatting/formatter_lowrisc_style_test_cases.h"

#undef EXPECT_OK
#define EXPECT_OK(value) EXPECT_TRUE((value).ok())

#undef ASSERT_OK
#define ASSERT_OK(value) ASSERT_TRUE((value).ok())

namespace verilog {
namespace formatter {

namespace {

using absl::StatusCode;
using verible::AlignmentPolicy;
using verible::IndentationStyle;
using verible::LineNumberSet;

TEST(FormatterLowRISCStyleTest, ComplianceTest) {
  std::pair<const verilog::formatter::tests::ComplianceTestCase*,
            size_t> kComplianceTestCases =
      verilog::formatter::tests::GetLowRISCComplianceTestCases();

  const auto* test_cases = ABSL_DIE_IF_NULL(std::get<0>(kComplianceTestCases));
  size_t n = std::get<1>(kComplianceTestCases);

  for (size_t i = 0 ; i < n ; ++i) {
    std::ostringstream stream;

    VLOG(1) << "code-to-format:\n" << test_cases[i].input << "<EOF>";
    const auto status =
        FormatVerilog(test_cases[i].input, "<filename>", test_cases[i].style, stream);
    EXPECT_OK(status) << status.message();
    EXPECT_EQ(stream.str(), test_cases[i].expected) << "code:\n" << test_cases[i].input;
  }
}

}  // namespace
}  // namespace formatter
}  // namespace verilog
