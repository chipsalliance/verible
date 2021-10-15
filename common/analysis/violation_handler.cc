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

#include "common/analysis/violation_handler.h"

#include "common/analysis/rdformat.h"
#include "nlohmann/json.hpp"

namespace verible {
namespace {

void SuggestionFromEdit(const verible::ReplacementEdit& edit,
                        const verible::LineColumn& line_column_start,
                        const verible::LineColumn& line_column_end,
                        verible::rdformat::Suggestion* suggestion) {
  auto start_line = line_column_start.line + 1;
  auto start_column = line_column_start.column + 1;
  suggestion->text = edit.replacement;
  suggestion->range.start.column = start_column;
  suggestion->range.start.has_column = true;
  suggestion->range.start.line = start_line;
  suggestion->range.start.has_line = true;
  suggestion->range.end.column = line_column_end.column + 1;
  suggestion->range.end.has_column = true;
  suggestion->range.end.line = line_column_end.line + 1;
  suggestion->range.end.has_line = true;
  suggestion->range.has_end = true;
}

}  // namespace

void ViolationPrinter::HandleViolations(
    const std::set<LintViolationWithStatus>& violations, absl::string_view base,
    absl::string_view path) {
  verible::LintStatusFormatter formatter(base);
  for (auto violation : violations) {
    formatter.FormatViolation(stream_, *violation.violation, base, path,
                              violation.status->url,
                              violation.status->lint_rule_name);
    (*stream_) << std::endl;
  }
}

void RDJsonPrinter::HandleViolations(
    const std::set<LintViolationWithStatus>& violations, absl::string_view base,
    absl::string_view path) {
  using nlohmann::json;

  verible::LineColumnMap line_col_map = verible::LineColumnMap(base);
  // Severity is the same for all the violations.
  // When we have different severities for specific violations
  //   use it inside "Diagnostic" class instead of here.
  for (auto violation : violations) {
    auto line_col = line_col_map(violation.violation->token.left(base));
    auto start_line = line_col.line + 1;
    auto start_column = line_col.column + 1;

    // this represents a single violation in RDFormat
    auto diagnostic = verible::rdformat::Diagnostic();
    diagnostic.source.name = "verible";
    diagnostic.has_source = true;
    diagnostic.severity = "WARNING";
    diagnostic.has_severity = true;

    diagnostic.message = violation.violation->reason;
    diagnostic.location.path = std::string(path);
    diagnostic.location.range.start.column = start_column;
    diagnostic.location.range.start.has_column = true;
    diagnostic.location.range.start.line = start_line;
    diagnostic.location.range.start.has_line = true;
    diagnostic.location.has_range = true;

    if (!violation.violation->autofixes.empty()) {
      diagnostic.has_suggestions = true;
    }
    // diagnostic can store multiple suggestions
    //   store the info about them now
    for (auto const& fix : violation.violation->autofixes) {
      for (auto const& edit : fix.Edits()) {
        verible::rdformat::Suggestion suggestion;
        // Suggestion range starts where the
        //   *removed code* ("edit.fragment") starts
        //   and ends where the *removed code* ends.
        // Which means we don't consider the length of the replacement text here
        auto line_col_end = line_col_map(violation.violation->token.left(base) +
                                         edit.fragment.length());
        SuggestionFromEdit(edit, line_col, line_col_end, &suggestion);
        diagnostic.suggestions.push_back(suggestion);
      }
    }
    json jdiagnostic = json::object();
    diagnostic.Serialize(&jdiagnostic);
    (*stream_) << jdiagnostic << std::endl;
  }
}

}  // namespace verible
