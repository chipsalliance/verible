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
// LowRISC format style.

#ifndef VERIBLE_COMMON_UTIL_STYLE_COMPLIANCE_REPORT_H_
#define VERIBLE_COMMON_UTIL_STYLE_COMPLIANCE_REPORT_H_

#include <map>

#include "absl/status/status.h"
#include "absl/strings/string_view.h"
#include "common/formatting/basic_format_style.h"
#include "common/util/logging.h"
#include "common/util/spacer.h"
#include "verilog/formatting/format_style.h"

namespace verible {

class StyleComplianceTestCase {
 public:
  StyleComplianceTestCase(std::string filename,
                          std::string description,
                          std::string code);

  StyleComplianceTestCase(std::string description,
                          std::string input,
                          std::string expected,
                          std::string compliance);

  ~StyleComplianceTestCase() = default;
  StyleComplianceTestCase(StyleComplianceTestCase&&) = default;

  StyleComplianceTestCase() = delete;
  StyleComplianceTestCase(const StyleComplianceTestCase&) = delete;
  StyleComplianceTestCase& operator=(const StyleComplianceTestCase&) = delete;
  StyleComplianceTestCase& operator=(StyleComplianceTestCase&&) = delete;

  std::map<std::string, std::string> GetDescription() const;

  std::string BuildReportEntry() const;

  const verilog::formatter::FormatStyle& GetStyle() const {
    return *ABSL_DIE_IF_NULL(style_);
  }

  bool ShouldFail() const {
    return should_fail_;
  }

  bool Format();

  bool AsExpected() const {
    return formatted_output_ == expected_;
  }

 private:
  std::string filename_;

  std::string description_;

  std::string input_;
  std::string expected_;
  std::string compliance_;

  std::unique_ptr<verilog::formatter::FormatStyle> style_;
  bool should_fail_ = false;

  std::string formatted_output_;
};

class StyleComplianceReport {
 public:
  StyleComplianceReport() = default;
  ~StyleComplianceReport() = default;

  StyleComplianceReport(const StyleComplianceReport&) = delete;
  StyleComplianceReport(const StyleComplianceReport&&) = delete;
  StyleComplianceReport& operator=(const StyleComplianceReport&) = delete;

  void SetProjectName(absl::string_view project_name) {
    project_name_ = std::string{project_name};
  }

  void SetCopyrights(absl::string_view copyrights) {
    copyrights_ = std::string{copyrights};
  }

  void SetAuthors(absl::string_view authors) {
    authors_ = std::string{authors};
  }

  absl::string_view GetProjectName() const {
    return project_name_;
  }

  absl::string_view GetCopyrights() const {
    return copyrights_;
  }

  absl::string_view GetAuthors() const {
    return authors_;
  }

  // Generates Sphinx configuration, e.g.
  // import sphinx_rtd_theme
  //
  // project = '$project_name_'
  // copyright = $copyrights_'
  // author = '$authors_'
  //
  // exclude_patterns = []
  //
  // extensions = [ "sphinx_rtd_theme", ]
  //
  // html_theme = 'sphinx_rtd_theme'
  std::string BuildConfiguration() const;

  std::string BuildHeader() const;

  // FIXME(ldk): Make this const
  // std::string?
  StyleComplianceTestCase BuildTestCase(
      absl::string_view contents, absl::string_view filename);

  StyleComplianceTestCase BuildTestCase(
      absl::string_view description,
      absl::string_view input,
      absl::string_view expected,
      absl::string_view compliance);

 private:
  // Report name
  std::string project_name_ = "LowRISC style compliance report";

  // Report copyrights
  std::string copyrights_ = "2017-2021, The Verible Authors";

  // Report authors
  std::string authors_ = "The Verible Authors";
};

}  // namespace verible

#endif  // VERIBLE_COMMON_UTIL_STYLE_COMPLIANCE_REPORT_H_
