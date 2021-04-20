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

#ifndef VERIBLE_VERILOG_FORMATTING_FORMATTER_LOWRISC_STYLE_TEST_CASES_H_
#define VERIBLE_VERILOG_FORMATTING_FORMATTER_LOWRISC_STYLE_TEST_CASES_H_

#include <utility>

#include "verilog/formatting/lowrisc_format_style.h"

namespace verilog {
namespace formatter {
namespace tests {

struct ComplianceTestCase {
  absl::string_view description;

  LowRISCFormatStyle style;

  absl::string_view input;
  absl::string_view expected;

  absl::string_view compliant;
};

std::pair<const ComplianceTestCase*, size_t>
GetLowRISCComplianceTestCases();

}  // namespace tests
}  // namespace formatter
}  // namespace verilog

#endif  // VERIBLE_VERILOG_FORMATTING_FORMATTER_LOWRISC_STYLE_TEST_CASES_H_
