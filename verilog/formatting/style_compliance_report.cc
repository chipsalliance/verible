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

#include <sstream>

#include "absl/strings/str_split.h"
#include "absl/strings/str_cat.h"
#include "common/util/logging.h"
#include "verilog/formatting/formatter.h"
#include "verilog/formatting/format_style.h"
#include "verilog/formatting/style_compliance_report.h"
#include "verilog/formatting/lowrisc_format_style.h"

namespace verible {

std::string StyleComplianceReport::BuildConfiguration() const {
  std::ostringstream configuration;

  configuration << "import sphinx_rtd_theme\n";

  configuration << "\n";

  configuration << "project = '";
  configuration << project_name_;
  configuration << "'\n";

  configuration << "copyright = '";
  configuration << copyrights_;
  configuration << "'\n";

  configuration << "author = '";
  configuration << authors_;
  configuration << "'\n";

  configuration << "\n";

  configuration << "exclude_patterns = []\n";

  configuration << "\n";

  configuration << "extensions = [ \"sphinx_rtd_theme\", ]\n";
  configuration << "\n";

  // Optional extra options
  // configuration << "html_theme_options = {\n";
  // configuration << "  'collapse_navigation' : True,\n";
  // configuration << "  'navigation_depth' : 1,\n";
  // configuration << "  'titles_only' : True,\n";
  // configuration << "}\n";
  // configuration << "\n";

  configuration << "html_theme = 'sphinx_rtd_theme'\n";

  return configuration.str();
}

std::string StyleComplianceReport::BuildHeader() const {
  std::ostringstream header;

  header << ".. |hr| raw:: html\n";
  header << "\n";
  header << "    <hr />\n\n";

  header << project_name_ << std::endl;
  header << verible::Spacer(project_name_.size(), '=') << std::endl;

  header << ".. toctree::\n";
  header << "   :maxdepth: 1\n";
  header << "   :caption: Contents:\n";
  header << "\n";

  return header.str();
}

absl::string_view trim(absl::string_view s) {
  if (s.size() == 0) {
    return s;
  }

  const auto pos = s.find_first_not_of(' ');
  const auto n   = s.find_last_not_of(' ');

  return s.substr(pos, pos - n);
}

std::map<std::string, std::string> StyleComplianceTestCase::GetDescription() const {
  std::map<std::string, std::string> ret;
  const auto& description = description_;

  std::vector<absl::string_view> splitted_description =
      absl::StrSplit(description, '\n');

  const int tag_max_size = 20;
  std::map<std::string, std::string>::iterator itr = ret.end();

  for (const auto& line : splitted_description) {
    // end of description field
    if (line.size() == 0) {
      itr = ret.end();
      continue ;
    }

    if (itr == ret.end()) {
      const auto tag_pos = line.find(':');
      if (tag_pos == absl::string_view::npos || tag_pos > tag_max_size) {
        if (ret.find("title") == ret.end()) {
          // didn't find, defaulting to title
          std::pair<
              std::map<std::string, std::string>::iterator,
              bool> r = ret.insert(std::pair<std::string, std::string>("title", line));
          itr = r.first;
        } else if (ret.find("info") == ret.end() && ret.size() == 1) {
          std::pair<
              std::map<std::string, std::string>::iterator,
              bool> r = ret.insert(std::pair<std::string, std::string>("info", line));
          itr = r.first;
        //} else {
        //  std::pair<
        //      std::map<std::string, std::string>::iterator,
        //      bool> r = ret.insert(std::pair<std::string, std::string>("unknown", line));
        //  itr = r.first;
        }
      } else {
        absl::string_view tag_name = line.substr(0, tag_pos);
        std::pair<
            std::map<std::string, std::string>::iterator,
            bool> r = ret.insert(std::pair<std::string, std::string>(
                tag_name,
                trim(line.substr(tag_pos+1))));
        itr = r.first;
      }
    } else {
      if (itr->second.size() > 0) {
        itr->second += "\n";
      }
      itr->second += std::string{trim(line)};
    }
  }

  return ret;
}

StyleComplianceTestCase::StyleComplianceTestCase(std::string filename,
                                                 std::string description,
                                                 std::string code) {
  filename_ = filename;

  description_ = description;

  input_ = code;
  expected_ = code;
  compliance_ = code;

  // Default format style (basic)
  if (style_ == nullptr) {
    style_ = std::make_unique<verilog::formatter::FormatStyle>();
  }

  const auto desc = GetDescription();

  const auto style_itr = desc.find("style");
  if (style_itr != desc.end()) {
    std::vector<absl::string_view> tags = absl::StrSplit(style_itr->second, " ");
    const auto& style_name = tags[0];
    if (!style_name.compare("lowrisc")) {
      style_ = std::make_unique<verilog::formatter::LowRISCFormatStyle>();
    }

    for (const auto itr : tags) {
      const std::vector<absl::string_view> vars = absl::StrSplit(itr, "=");

      if (!vars[0].compare("column_limit")) {
        bool noerror = absl::SimpleAtoi(
            vars[1], &ABSL_DIE_IF_NULL(style_)->column_limit);
        CHECK_EQ(noerror, true);
      }
    }
  }

  const auto should_fail_itr = desc.find("should_fail");
  if (should_fail_itr != desc.end()) {
    bool noerror = absl::SimpleAtob(should_fail_itr->second, &should_fail_);
    CHECK_EQ(noerror, true);
  }
}

StyleComplianceTestCase::StyleComplianceTestCase(std::string description,
                                                 std::string input,
                                                 std::string expected,
                                                 std::string compliance) {
  filename_ = "<internal>";

  description_ = description;

  input_ = input;
  expected_ = expected;
  compliance_ = compliance;
}

StyleComplianceTestCase StyleComplianceReport::BuildTestCase(
    absl::string_view contents, absl::string_view filename) {
  // split contents
  std::vector<absl::string_view> lines = absl::StrSplit(contents, '\n');
  std::string description, code;

  auto itr = lines.begin();
  for (; itr != lines.end() && absl::StartsWith(*itr, "//"); ++itr) {
    const auto at = itr->find_first_not_of("/ \t");
    if (at != absl::string_view::npos) {
      absl::StrAppend(&description, itr->substr(at), "\n");
    } else {
      // empty line
      absl::StrAppend(&description, "\n");
    }
  }

  // skip empty lines
  for (; itr != lines.end() && itr->empty() ; ++itr) ;

  for (; itr != lines.end(); ++itr) {
    absl::StrAppend(&code, *itr, "\n");
  }

  VLOG(4) << "desc:\n" << description << "\ncode:\n" << code;
  return StyleComplianceTestCase(std::string{filename}, description, code);
}

StyleComplianceTestCase StyleComplianceReport::BuildTestCase(
      absl::string_view description,
      absl::string_view input,
      absl::string_view expected,
      absl::string_view compliance) {
  return StyleComplianceTestCase(
      std::string{description}, std::string{input},
      std::string{expected}, std::string{compliance});
}

bool StyleComplianceTestCase::Format() {
  std::ostringstream stream;

  const auto status =
      verilog::formatter::FormatVerilog(
          input_, filename_, *ABSL_DIE_IF_NULL(style_), stream);

  formatted_output_ = stream.str();
  return status.ok();
}

std::string StyleComplianceTestCase::BuildReportEntry() const {
  std::ostringstream out;

  std::map<std::string, std::string> desc = GetDescription();

  auto title = desc.find("title");
  if (title != desc.end()) {
    out << title->second << "\n";
    if (input_.size() == 0) {
      out << verible::Spacer(title->second.size(), '=');
    } else {
      out << verible::Spacer(title->second.size(), '-');
    }
    out << "\n\n";
  }

  auto info = desc.find("info");
  if (info != desc.end()) {
    for (const auto itr : absl::StrSplit(info->second, '\n')) {
      out << itr << "\n";
    }
    out << "\n";
  }

  auto gh_issue = desc.find("gh_issue");
  if (gh_issue != desc.end()) {
    out << ".. note::\n\n";
    out << "    " << "GitHub issue(s):\n";
    for (auto rel : absl::StrSplit(gh_issue->second, '\n')) {
      const auto number = rel.rfind('/');
      if (number != absl::string_view::npos) {
        out << "    `#" << rel.substr(number+1) <<
               " <" << rel << ">`_\n";
      } else {
        out << " `url: <" << rel << ">`_\n";
      }
    }
    out << "\n";
  }

  if (input_.size() == 0) {
    return out.str();
  }

  if (compliance_.size() > 0) {
    out << ".. code-block:: systemverilog\n\n";
    for (const auto line : absl::StrSplit(compliance_, '\n')) {
      out << "   " << line << "\n";
    }
    out << "\n";

    if (compliance_ != formatted_output_) {
      out << ".. error::\n    Formatter generated output:\n\n";

      out << "  .. code-block:: systemverilog\n\n";
      for (const auto line : absl::StrSplit(formatted_output_, '\n')) {
        out << "     " << line << "\n";
      }

      out << "\n";
    }
  } else {
    out << "\nExample code:\n\n";
    out << ".. code-block:: systemverilog\n\n";
    for (const auto line : absl::StrSplit(formatted_output_, '\n')) {
      out << "   " << line << "\n";
    }
    out << "\n";
  }

  out << "\n\n|hr|\n\n";

  return out.str();
}

}  // namespace verible
