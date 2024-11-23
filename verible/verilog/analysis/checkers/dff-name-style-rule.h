// Copyright 2017-2023 The Verible Authors.
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

#ifndef VERIBLE_VERILOG_ANALYSIS_CHECKERS_CONSTRAINT_NAME_STYLE_RULE_H_
#define VERIBLE_VERILOG_ANALYSIS_CHECKERS_CONSTRAINT_NAME_STYLE_RULE_H_

#include <cstdint>
#include <memory>
#include <optional>
#include <set>
#include <string>
#include <utility>
#include <vector>

#include "absl/status/status.h"
#include "absl/strings/str_split.h"
#include "absl/strings/string_view.h"
#include "re2/re2.h"
#include "verible/common/analysis/lint-rule-status.h"
#include "verible/common/analysis/syntax-tree-lint-rule.h"
#include "verible/common/text/symbol.h"
#include "verible/common/text/syntax-tree-context.h"
#include "verible/verilog/analysis/descriptions.h"

namespace verilog {
namespace analysis {

// DffNameStyleRule checks that D flip flops use appropriate naming conventions
// for both their input and outputs.
//
// The rule can be configured via specifying a comma-separated list of
// suffixes (one for input, one for output). Providing an empty list
// means no checks for the corresponding field
//
// For example, the defaults are equivalent to:
//  +dff-name-style=output:reg,r,ff,q;input:next,n,d
//  Which gets expanded into
//  `valid_output_suffixes` = { "reg", "r", "ff", "q" }
//  `valid_input_suffixes` = { "next", "n", "d" }
//
// Given a nonblocking assignment inside an always_ff block
// we will then check the left-hand side (lhs) and right
// hand side (rhs) of the assignment against the valid
// suffixes
//
// Given "data_q <= data_n", we will check that:
//   - data_q ends with any of { "_reg", "_r", "_ff", "_q" }
//   - data_n ends with any of { "_next", "_n", "_d" }
//
// If those checks succeed, we will also check that their prefixes
// are equal.
//  data_q <= data_n -> OK
//  data_q <= something_n -> WRONG: "data" != "something"
//
// Following [1] we also allow for using trailing numbers
// in the identifiers to specify the pipeline stage where
// a given signal comes from.
//
// Under this convention, data_q3 should be driven by the
// previous stage data_q2
//
// The rule might not always be applicable. Apart from manual waiving, there are
// two supported ways to disable the checks:
//   1. Using the `waive_ifs_with_conditions` argument, we can specify certain
//   `if`s under which the rule shouldn't apply. For example:
//        if(!rst_ni) data_q <= '{default: 0};
//   2. `waive_lhs_regex` lets us disable the check for some nonblocking
//   assignments.
//
//
// clang-format off
// [1] https://github.com/lowRISC/style-guides/blob/9b47bff75b19696e23a43f38ee7161112705e1e3/VerilogCodingStyle.md#suffixes
// clang-format on
class DffNameStyleRule : public verible::SyntaxTreeLintRule {
 public:
  using rule_type = verible::SyntaxTreeLintRule;
  static constexpr absl::string_view kDefaultInputSuffixes = "next,n,d";
  static constexpr absl::string_view kDefaultOutputSuffixes = "reg,r,ff,q";
  static constexpr absl::string_view kDefaultWaiveRegex = "(?i)mem.*";
  static constexpr absl::string_view kDefaultWaiveConditions =
      "!rst_ni,flush_i,!rst_ni || flush_i,flush_i || !rst_ni";

  static const LintRuleDescriptor &GetDescriptor();

  void HandleSymbol(const verible::Symbol &symbol,
                    const verible::SyntaxTreeContext &context) final;

  verible::LintRuleStatus Report() const final;

  absl::Status Configure(absl::string_view) final;

  // Identifiers can optionally include a trailing number
  // indicating the pipeline stage where the signal originates from.
  //
  // This function returns the identifier without the pipeline stage, and the
  // integer value of the pipeline stage (> kFirstValidPipeStage) if present
  //
  // Examples:
  // ExtractPipelineStage("data_q") => {"data_q", {})}
  // ExtractPipelineStage("data_q1") => {"data_q1", {})}
  // ExtractPipelineStage("data_q2") => {"data_q", 2)}
  // https://github.com/lowRISC/style-guides/blob/9b47bff75b19696e23a43f38ee7161112705e1e3/VerilogCodingStyle.md#suffixes-for-signals-and-types
  static std::pair<absl::string_view, std::optional<uint64_t>>
  ExtractPipelineStage(absl::string_view id);

 private:
  absl::string_view CheckSuffix(const verible::SyntaxTreeContext &context,
                                const verible::Symbol &root,
                                absl::string_view id,
                                const std::vector<std::string> &suffixes);

  void HandleBlockingAssignments(const verible::Symbol &symbol,
                                 const verible::SyntaxTreeContext &context);

  void HandleNonBlockingAssignments(
      const verible::Symbol &non_blocking_assignment,
      const verible::SyntaxTreeContext &context);

  // Extract the individual suffixes from the comma separated list
  // coming from configuration.
  // "q,ff,reg" => { "q", "ff", "reg" }
  //
  // Used to initialize `valid_input_suffixes` and `valid_output_suffixes`
  static std::vector<std::string> ProcessSuffixes(absl::string_view config);

  std::set<verible::LintViolation> violations_;

  std::vector<std::string> valid_input_suffixes =
      ProcessSuffixes(kDefaultInputSuffixes);
  std::vector<std::string> valid_output_suffixes =
      ProcessSuffixes(kDefaultOutputSuffixes);

  // Waive `if` branches we do not want to take into account. e.g:
  // if(!rst_ni)
  //   data_q <= SOME_DEFAULT_VALUE;
  // Exact matching with respect to the waive conditions is required (the only
  // exception are leading and trailing whitespaces which are removed)
  std::vector<std::string> waive_ifs_with_conditions =
      absl::StrSplit(kDefaultWaiveConditions, ',');

  // Regex to waive specific variables. Intended for (but not limited to)
  // things like memories
  //   mem[addr] <= value;
  std::unique_ptr<re2::RE2> waive_lhs_regex =
      std::make_unique<re2::RE2>(kDefaultWaiveRegex);

  // (*) Valid integers span from 1 to n
  static constexpr uint64_t kFirstValidPipeStage = 1;
};

}  // namespace analysis
}  // namespace verilog

#endif  // VERIBLE_VERILOG_ANALYSIS_CHECKERS_NAMING_CONVENTIONS_H_
