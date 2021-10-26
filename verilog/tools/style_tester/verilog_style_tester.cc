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

#include <algorithm>
#include <iostream>
#include <iterator>
#include <memory>
#include <sstream>  // IWYU pragma: keep  // for ostringstream
#include <string>   // for string, allocator, etc
#include <vector>

#include "absl/flags/flag.h"
#include "absl/status/status.h"
#include "absl/strings/str_cat.h"
#include "absl/strings/string_view.h"
#include "common/formatting/basic_format_style.h"
#include "common/text/concrete_syntax_tree.h"
#include "common/util/enum_flags.h"
#include "common/util/file_util.h"
#include "common/util/init_command_line.h"
#include "common/util/logging.h"  // for operator<<, LOG, LogMessage, etc
#include "verilog/formatting/lowrisc_format_style.h"
#include "verilog/formatting/formatter.h"
#include "verilog/formatting/style_compliance_report.h"
#include "verilog/formatting/formatter_lowrisc_style_test_cases.h"


ABSL_FLAG(bool, dump_header, false, "Print report header");


ABSL_FLAG(bool, dump_configuration, false, "Print Sphinx configuration");


ABSL_FLAG(bool, dump_internal, false, "Dump internal test suite");


int main(int argc, char** argv) {
  const auto usage =
      absl::StrCat("usage: ", argv[0], " [options] <file> [<file>...]");
  const auto args = verible::InitCommandLine(usage, &argc, &argv);
  verible::StyleComplianceReport report;

  if (absl::GetFlag(FLAGS_dump_configuration)) {
    std::cout << report.BuildConfiguration();
    return 0;
  }

  if (absl::GetFlag(FLAGS_dump_header)) {
    std::cout << report.BuildHeader();
  }

  if (absl::GetFlag(FLAGS_dump_internal)) {
    std::pair<const verilog::formatter::tests::ComplianceTestCase*,
              size_t> kComplianceTestCases =
        verilog::formatter::tests::GetLowRISCComplianceTestCases();

    const auto* test_cases = ABSL_DIE_IF_NULL(std::get<0>(kComplianceTestCases));
    size_t n = std::get<1>(kComplianceTestCases);

    for (size_t i = 0 ; i < n ; ++i) {
      const auto& test_case = test_cases[i];

      std::cout << report.BuildTestCase(test_case.description,
                                        test_case.input,
                                        test_case.expected,
                                        test_case.compliant).BuildReportEntry();
    }

    if (!verible::empty_range(args.begin() + 1, args.end())) {
      std::cout << report.BuildTestCase("External test suite", "", "", "").BuildReportEntry();
    }
  }

  int exit_status = 0;
  // All positional arguments are file names.  Exclude program name.
  for (const auto filename :
       verible::make_range(args.begin() + 1, args.end())) {
    std::string content;
    if (!verible::file::GetContents(filename, &content).ok()) {
      exit_status = 1;
      continue;
    }

    auto test_case = report.BuildTestCase(content, filename);
    bool succeeded = test_case.Format();

    if (!test_case.ShouldFail() && (!succeeded || !test_case.AsExpected())) {
      // Break?
      exit_status = 1;
    }

    std::cout << test_case.BuildReportEntry();
  }

  return exit_status;
}
