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

#include "verilog/analysis/checkers/dff_name_style_rule.h"

#include <algorithm>
#include <cctype>
#include <charconv>
#include <cstdint>
#include <iterator>
#include <optional>
#include <set>
#include <string>
#include <system_error>
#include <utility>
#include <vector>

#include "absl/status/status.h"
#include "absl/strings/match.h"
#include "absl/strings/str_cat.h"
#include "absl/strings/str_join.h"
#include "absl/strings/str_split.h"
#include "absl/strings/string_view.h"
#include "common/analysis/lint_rule_status.h"
#include "common/analysis/matcher/bound_symbol_manager.h"
#include "common/analysis/matcher/core_matchers.h"
#include "common/analysis/matcher/matcher.h"
#include "common/analysis/syntax_tree_search.h"
#include "common/text/concrete_syntax_leaf.h"
#include "common/text/concrete_syntax_tree.h"
#include "common/text/config_utils.h"
#include "common/text/symbol.h"
#include "common/text/syntax_tree_context.h"
#include "common/text/token_info.h"
#include "common/text/tree_utils.h"
#include "verilog/CST/expression.h"
#include "verilog/CST/statement.h"
#include "verilog/CST/verilog_matchers.h"
#include "verilog/CST/verilog_nonterminals.h"
#include "verilog/analysis/descriptions.h"
#include "verilog/analysis/lint_rule_registry.h"
#include "verilog/parser/verilog_token_enum.h"

namespace verilog {
namespace analysis {

using verible::LintRuleStatus;
using verible::LintViolation;
using verible::SyntaxTreeContext;
using verible::config::SetString;
using verible::matcher::Matcher;

// Register DffNameStyleRule.
VERILOG_REGISTER_LINT_RULE(DffNameStyleRule);

const LintRuleDescriptor &DffNameStyleRule::GetDescriptor() {
  static const LintRuleDescriptor d{
      .name = "dff-name-style",
      .topic = "dff-name-style",
      .desc =
          "Checks that D Flip-Flops use appropiate naming conventions in both "
          "input and output ports.",
      .param = {
          {"input", kDefaultInputSuffixes,
           "Comma separated list of allowed suffixes for the input port. "
           "Suffixes should not include the preceding \"_\". Empty field "
           "means no checks for the input port."},
          {"output", kDefaultOutputSuffixes,
           "Comma separated list of allowed suffixes for the output port. "
           "Should not include the preceding \"_\". Empty field means no "
           "checks for the output port."}}};
  return d;
}

static const Matcher &AlwaysFFMatcher() {
  static const Matcher matcher(NodekAlwaysStatement(AlwaysFFKeyword()));
  return matcher;
}

// Ensure that variables being driven by a blocking assigment don't follow the
// naming convention of DFF outputs
void DffNameStyleRule::HandleBlockingAssignments(
    const verible::Symbol &symbol, const verible::SyntaxTreeContext &context) {
  const verible::SyntaxTreeNode &node = SymbolCastToNode(symbol);
  if (node.empty()) return;

  const verible::Symbol *driven_variable = node.front().get();

  // Only "++var" has its identifier in a position other than the 1st child
  if (node.MatchesTag(NodeEnum::kIncrementDecrementExpression)) {
    driven_variable = GetIncrementDecrementOperand(symbol);
  }

  absl::string_view lhs_str = verible::StringSpanOfSymbol(*driven_variable);

  const bool found_id =
      std::any_of(valid_output_suffixes.cbegin(), valid_output_suffixes.cend(),
                  [&](const std::string &suffix) -> bool {
                    return lhs_str.size() > suffix.size() &&
                           absl::EndsWith(lhs_str, suffix);
                  });

  if (!found_id) return;
  violations_.insert(LintViolation(
      *driven_variable,
      absl::StrCat(lhs_str,
                   " should be driven by a nonblocking assignment "
                   "inside an always_ff block"),
      context));
}

// Ensure that variables being driven by a nonblocking assigment follow the
// naming convention of DFF outputs
void DffNameStyleRule::HandleNonBlockingAssignments(
    const verible::Symbol &non_blocking_assignment,
    const verible::SyntaxTreeContext &context) {
  const verible::SyntaxTreeNode &node =
      verible::SymbolCastToNode(non_blocking_assignment);

  const verible::Symbol &lhs = *GetNonBlockingAssignmentLhs(node);
  const verible::SyntaxTreeNode &rhs_expr = *GetNonBlockingAssignmentRhs(node);

  absl::string_view lhs_str = verible::StringSpanOfSymbol(lhs);
  absl::string_view rhs_str = verible::StringSpanOfSymbol(rhs_expr);

  auto [clean_lhs_str, lhs_pipe_stage] = ExtractPipelineStage(lhs_str);
  // Check if the string without the pipeline number has a valid format
  absl::string_view lhs_base =
      CheckSuffix(context, lhs, clean_lhs_str, valid_output_suffixes);
  // If the base is empty, lhs is wrongly formatted. Stop making more checks
  if (lhs_base.empty()) return;

  // TODO: Intention is to waive at least numeric constants
  //  - macros/function calls should be considered too?
  //  - make it more precise? expand expression.h:ConstantIntegerValue?
  if (verible::DescendThroughSingletons(rhs_expr)->Kind() ==
      verible::SymbolKind::kLeaf) {
    return;
  }

  // Pipeline stage present on the lhs
  // ID_suffixN <= expr;
  if (uint64_t lhs_stage = lhs_pipe_stage.value_or(0)) {
    // "data_qN" should be driven by "data_qN-1", but
    // "data_q2" should be driven by "data_q", not "data_q1"
    std::string expected_rhs(clean_lhs_str.cbegin(), clean_lhs_str.size());
    if (lhs_stage != kFirstValidPipeStage) {
      absl::StrAppend(&expected_rhs, lhs_stage - 1);
    }

    // Note: mixing suffixes when using pipeline identifiers
    // is not allowed
    //  data_q2 <= data_q  -> OK
    //  data_q2 <= data_ff -> WRONG
    if (rhs_str != expected_rhs) {
      violations_.insert(LintViolation(
          rhs_expr, absl::StrCat(rhs_str, " Should be ", expected_rhs),
          context));
    }
    return;
  }

  absl::string_view rhs_base =
      CheckSuffix(context, rhs_expr, rhs_str, valid_input_suffixes);

  // If the rhs is wrongly formatted, there is no need to check that the
  // bases match
  if (rhs_base.empty()) return;

  if (lhs_base != rhs_base) {
    // Bases should be equal
    //  "a_q <= a_n" -> OK
    //  "a_q <= b_n" -> WRONG
    violations_.insert(LintViolation(
        non_blocking_assignment,
        absl::StrCat("Both parts before the suffix should be equal, but \"",
                     lhs_base, "\" != \"", rhs_base, "\""),
        context));
  }
}

void DffNameStyleRule::HandleSymbol(const verible::Symbol &symbol,
                                    const SyntaxTreeContext &context) {
  verible::matcher::BoundSymbolManager manager;
  if (valid_input_suffixes.empty() && valid_output_suffixes.empty()) return;

  // Types of assignments intended for DFF inputs
  static const Matcher blocking_assignment_matcher = verible::matcher::AnyOf(
      NodekContinuousAssignmentStatement(), NodekNetVariableAssignment(),
      NodekNetDeclarationAssignment(), NodekAssignModifyStatement(),
      NodekIncrementDecrementExpression());
  if (blocking_assignment_matcher.Matches(symbol, &manager)) {
    HandleBlockingAssignments(symbol, context);
    return;
  }

  if (!AlwaysFFMatcher().Matches(symbol, &manager)) {
    return;
  }

  // Once we encounter an always_ff statement, extract inner non
  // blocking assignments.
  std::vector<verible::TreeSearchMatch> non_blocking_assignments =
      FindAllNonBlockingAssignments(symbol);

  for (const verible::TreeSearchMatch &tree_match : non_blocking_assignments) {
    HandleNonBlockingAssignments(*tree_match.match, context);
  }
}

absl::string_view DffNameStyleRule::CheckSuffix(
    const verible::SyntaxTreeContext &context, const verible::Symbol &root,
    absl::string_view id, const std::vector<std::string> &suffixes) {
  // Identifier is split between base and suffix:
  // "myid_q" => {"myid", "_q"}
  absl::string_view base;
  // If there are no patterns to check against, everything passes the check
  if (suffixes.empty()) return base;

  // Lhs and Rhs should be plain variable identifiers
  const verible::SyntaxTreeLeaf *leftmost_leaf = verible::GetLeftmostLeaf(root);
  const verible::SyntaxTreeLeaf *rightmost_leaf =
      verible::GetRightmostLeaf(root);

  // TODO(iencinas): this is too restrictive, as it only allows plain variables
  // - Allow indexing: var_suffix[] (multiple levels?)
  // - If covering all cases is too hard, perhaps "ignore" such cases instead of
  // complaining?
  // a[2], a.b, ..., are not allowed
  // Only simple identifiers and exceptions
  if (leftmost_leaf != rightmost_leaf ||
      leftmost_leaf->get().token_enum() != SymbolIdentifier) {
    violations_.insert(LintViolation(
        root,
        absl::StrCat(
            id, " Should be a simple reference, ending with a valid suffix: {",
            absl::StrJoin(suffixes, ","), "}"),
        context));

    return base;
  }

  absl::string_view suffix_match;
  // Check if id conforms to any valid suffix
  const bool id_ok = std::any_of(suffixes.cbegin(), suffixes.cend(),
                                 [&](const std::string &suffix) -> bool {
                                   if (absl::EndsWith(id, suffix)) {
                                     base = absl::string_view(
                                         id.begin(), id.size() - suffix.size());
                                     suffix_match = suffix;
                                     return true;
                                   }
                                   return false;
                                 });

  // Exact match: id: "_q", suffix: "_q", there is no base
  bool exact_match = suffix_match.size() == id.size();
  if (id_ok && !exact_match) return base;

  // The identifier can't be just the suffix we're matching against
  const std::string lint_message =
      exact_match
          ? absl::StrCat(
                "A valid identifier should not exactly match a valid prefix \"",
                id, "\" == \"", suffix_match, "\"")
          : absl::StrCat(id, " should end with a valid suffix: {",
                         absl::StrJoin(suffixes, ","), "}");
  violations_.insert(LintViolation(root, lint_message, context));

  return base;
}

absl::Status DffNameStyleRule::Configure(absl::string_view configuration) {
  // If configuration is empty, stick to the default
  if (configuration.empty()) return absl::OkStatus();

  std::string output, input;
  absl::Status status = verible::ParseNameValues(
      configuration,
      {{"output", SetString(&output)}, {"input", SetString(&input)}});

  if (!status.ok()) return status;

  valid_input_suffixes = ProcessSuffixes(input);
  valid_output_suffixes = ProcessSuffixes(output);

  return status;
}

std::vector<std::string> DffNameStyleRule::ProcessSuffixes(
    absl::string_view config) {
  // Split input string: "q,ff,reg" => {"q", "ff", "reg"}
  std::vector<absl::string_view> split_suffixes =
      absl::StrSplit(config, ',', absl::SkipEmpty());

  std::vector<std::string> result(split_suffixes.size());

  // Prepend an underscore to the suffixes to check against them
  // {"q", "ff", "reg"} => {"_q", "_ff", "_reg"}
  const auto prepend_underscore = [](absl::string_view str) -> std::string {
    return absl::StrCat("_", str);
  };
  std::transform(split_suffixes.cbegin(), split_suffixes.cend(), result.begin(),
                 prepend_underscore);

  return result;
}

std::pair<absl::string_view, std::optional<uint64_t> >
DffNameStyleRule::ExtractPipelineStage(absl::string_view id) {
  // Find the number of trailing digits inside the identifier
  absl::string_view::const_reverse_iterator last_non_num = std::find_if(
      id.rbegin(), id.rend(), [](unsigned char c) { return !std::isdigit(c); });
  uint64_t num_digits =
      static_cast<uint64_t>(std::distance(id.rbegin(), last_non_num));

  // If there are no trailing digits, or the id is just composed
  // of digits
  if (num_digits == 0 || num_digits == id.size()) return {id, {}};

  // Extract the integer value for the pipeline stage
  uint64_t pipe_stage;
  std::from_chars_result result = std::from_chars(
      id.cbegin() + id.size() - num_digits, id.cend(), pipe_stage);

  // https://en.cppreference.com/w/cpp/utility/from_chars
  // Check whether:
  // - There are errors parsing the string
  // - There are non-numeric characters inside the range (shouldn't!)
  // - The pipeline stage number is in the valid range
  if (result.ec != std::errc() || result.ptr != id.end() ||
      pipe_stage < kFirstValidPipeStage) {
    return {id, {}};
  }

  // Return the id without the trailing digits so we can do the suffix check
  // and the value for the pipeline stage
  id.remove_suffix(num_digits);
  return {id, pipe_stage};
}

LintRuleStatus DffNameStyleRule::Report() const {
  return LintRuleStatus(violations_, GetDescriptor());
}

}  // namespace analysis
}  // namespace verilog
