// Copyright 2017-2020 The Verible Authors.
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

#include "verilog/analysis/checkers/struct_union_name_style_rule.h"

#include <algorithm>
#include <set>
#include <string>

#include "absl/strings/match.h"
#include "absl/strings/str_cat.h"
#include "absl/strings/str_split.h"
#include "common/analysis/citation.h"
#include "common/analysis/lint_rule_status.h"
#include "common/analysis/matcher/bound_symbol_manager.h"
#include "common/analysis/matcher/matcher.h"
#include "common/strings/naming_utils.h"
#include "common/text/config_utils.h"
#include "common/text/symbol.h"
#include "common/text/syntax_tree_context.h"
#include "verilog/CST/type.h"
#include "verilog/CST/verilog_matchers.h"
#include "verilog/analysis/lint_rule_registry.h"

namespace verilog {
namespace analysis {

VERILOG_REGISTER_LINT_RULE(StructUnionNameStyleRule);

using verible::GetStyleGuideCitation;
using verible::LintRuleStatus;
using verible::LintViolation;
using verible::SyntaxTreeContext;
using verible::matcher::Matcher;

absl::string_view StructUnionNameStyleRule::Name() {
  return "struct-union-name-style";
}
const char StructUnionNameStyleRule::kTopic[] = "struct-union-conventions";
const char StructUnionNameStyleRule::kMessageStruct[] = "Struct names";
const char StructUnionNameStyleRule::kMessageUnion[] = "Union names";

std::string StructUnionNameStyleRule::GetDescription(
    DescriptionType description_type) {
  static std::string basic_desc = absl::StrCat(
      "Checks that ", Codify("struct", description_type), " and ",
      Codify("union", description_type),
      " names use lower_snake_case naming convention and end with '_t', or "
      "match the optional regular expression format. See ",
      GetStyleGuideCitation(kTopic), ".");
  if (description_type == DescriptionType::kHelpRulesFlag) {
    return absl::StrCat(basic_desc,
                        "Parameters: exceptions:String with exceptions;"
                        "name_regex:regex rule");
  } else {
    return absl::StrCat(
        basic_desc,
        "\n##### Parameters\n"
        " * `exceptions` (Comma-separated list of allowed "
        "upper-case elements, such as unit-names"
        ". Default: Empty)\n"
        "* `name_regex` (The regex rule validating the names. Default: Empty)");
  }
}

static const Matcher &TypedefMatcher() {
  static const Matcher matcher(NodekTypeDeclaration());
  return matcher;
}

void StructUnionNameStyleRule::HandleSymbol(const verible::Symbol &symbol,
                                            const SyntaxTreeContext &context) {
  verible::matcher::BoundSymbolManager manager;
  if (TypedefMatcher().Matches(symbol, &manager)) {
    // TODO: This can be changed to checking type of child (by index) when we
    // have consistent shape for all kTypeDeclaration nodes.
    const bool is_struct = !FindAllStructTypes(symbol).empty();
    if (!is_struct && FindAllUnionTypes(symbol).empty()) return;
    const char *const msg = is_struct ? kMessageStruct : kMessageUnion;

    const auto *identifier_leaf = GetIdentifierFromTypeDeclaration(symbol);
    const auto name = ABSL_DIE_IF_NULL(identifier_leaf)->get().text();
    if (name_regex_.has_value()) {
      if (!std::regex_match(std::string(name), *name_regex_)) {
        violations_.insert(LintViolation(*identifier_leaf,
                                         "Regex rule does not match", context));
      }
      return;
    }

    if (!absl::EndsWith(name, "_t")) {
      violations_.insert(
          LintViolation(*identifier_leaf,
                        absl::StrCat(msg, " have to ends with _t"), context));
      return;
    }
    if (name[0] == '_') {
      violations_.insert(LintViolation(
          *identifier_leaf, absl::StrCat(msg, " can't start with _"), context));
      return;
    }

    for (const auto &ns : absl::StrSplit(name, '_')) {
      if (std::all_of(ns.begin(), ns.end(), [](char c) {
            return absl::ascii_islower(c) || absl::ascii_isdigit(c);
          })) {
        continue;
      }
      if (!absl::ascii_isdigit(*ns.begin())) {
        violations_.insert(LintViolation(
            *identifier_leaf,
            "Section with unit names need to start with digit", context));
        return;
      }
      if (exceptions_.find(std::string(ns)) != exceptions_.end()) {
        continue;  // number + unit exception found
      }
      const auto &alpha =
          std::find_if(ns.begin(), ns.end(), absl::ascii_isalpha);
      const auto ns_substr = std::string(alpha, ns.end());
      if (exceptions_.find(ns_substr) == exceptions_.end()) {
        violations_.insert(LintViolation(*identifier_leaf,
                                         "found digit followed by unit that is "
                                         "not configured as allowed exception",
                                         context));
        return;
      }
    }
  }
}

absl::Status StructUnionNameStyleRule::Configure(
    absl::string_view configuration) {
  using verible::config::SetString;
  std::string raw_tokens, name_regex;
  auto status = verible::ParseNameValues(
      configuration, {{"exceptions", SetString(&raw_tokens)},
                      {"name_regex", SetString(&name_regex)}});
  if (!status.ok()) return status;

  if (!name_regex.empty()) {
    try {
      name_regex_ = name_regex;
    } catch (const std::regex_error &e) {
      return absl::Status(absl::StatusCode::kInvalidArgument,
                          "Invalid regex specified");
    }
  }

  if (!raw_tokens.empty()) {
    const auto &exceptions = absl::StrSplit(raw_tokens, ',');
    for (const auto &ex : exceptions) {
      const auto &e =
          std::find_if_not(ex.begin(), ex.end(), absl::ascii_isalnum);
      if (e != ex.end()) {
        return absl::Status(absl::StatusCode::kInvalidArgument,
                            "The exception can be composed of digits and "
                            "alphabetic characters only");
      }
      const auto &alpha =
          std::find_if(ex.begin(), ex.end(), absl::ascii_isalpha);
      if (alpha == ex.end()) {
        return absl::Status(
            absl::StatusCode::kInvalidArgument,
            "The exception have to contain at least one alphabetic character");
      }
      const auto &digit = std::find_if(alpha, ex.end(), absl::ascii_isdigit);
      if (digit != ex.end()) {
        return absl::Status(absl::StatusCode::kInvalidArgument,
                            "Digits after the unit are not allowed");
      }
      exceptions_.emplace(ex);
    }
  }
  return absl::OkStatus();
}

LintRuleStatus StructUnionNameStyleRule::Report() const {
  return LintRuleStatus(violations_, Name(), GetStyleGuideCitation(kTopic));
}

}  // namespace analysis
}  // namespace verilog
