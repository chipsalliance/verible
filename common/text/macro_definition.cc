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

#include "common/text/macro_definition.h"

#include <iterator>
#include <map>
#include <string>
#include <vector>

#include "absl/status/status.h"
#include "absl/strings/str_cat.h"
#include "absl/strings/string_view.h"
#include "common/text/token_info.h"
#include "common/util/container_util.h"

namespace verible {

using container::FindOrNull;

bool MacroDefinition::AppendParameter(const MacroParameterInfo& param_info) {
  is_callable_ = true;
  // Record position of this parameter.
  const bool inserted = parameter_positions_
                            .insert({std::string(param_info.name.text()),
                                     parameter_info_array_.size()})
                            .second;
  parameter_info_array_.push_back(param_info);
  return inserted;
}

absl::Status MacroDefinition::PopulateSubstitutionMap(
    const std::vector<TokenInfo>& macro_call_args,
    substitution_map_type* arg_map) const {
  if (macro_call_args.size() != parameter_info_array_.size()) {
    return absl::InvalidArgumentError(
        absl::StrCat("Error calling macro ", name_.text(), " with ",
                     macro_call_args.size(), " arguments, but definition has ",
                     parameter_info_array_.size(), " formal parameters."));
    // TODO(fangism): also allow one blank argument when number of formals is 0.
  }
  auto actuals_iter = macro_call_args.begin();
  const auto actuals_end = macro_call_args.end();
  auto formals_iter = parameter_info_array_.begin();
  for (; actuals_iter != actuals_end; ++actuals_iter, ++formals_iter) {
    auto& replacement_text = (*arg_map)[formals_iter->name.text()];
    if (!actuals_iter->text().empty()) {
      // Actual text is provided.
      replacement_text = *actuals_iter;
    } else if (!formals_iter->default_value.text().empty()) {
      // Use default parameter value.
      replacement_text = formals_iter->default_value;
    }
    // else leave blank as empty string.
  }
  return absl::OkStatus();
}

absl::Status MacroDefinition::PopulateSubstitutionMap(
    const std::vector<DefaultTokenInfo>& macro_call_args,
    substitution_map_type* arg_map) const {
  if (macro_call_args.size() != parameter_info_array_.size()) {
    return absl::InvalidArgumentError(
        absl::StrCat("Error calling macro ", name_.text(), " with ",
                     macro_call_args.size(), " arguments, but definition has ",
                     parameter_info_array_.size(), " formal parameters."));
    // TODO(fangism): also allow one blank argument when number of formals is 0.
  }
  auto actuals_iter = macro_call_args.begin();
  const auto actuals_end = macro_call_args.end();
  auto formals_iter = parameter_info_array_.begin();
  for (; actuals_iter != actuals_end; ++actuals_iter, ++formals_iter) {
    auto& replacement_text = (*arg_map)[formals_iter->name.text()];
    if (!actuals_iter->text().empty()) {
      // Actual text is provided.
      replacement_text = *actuals_iter;
    } else if (!formals_iter->default_value.text().empty()) {
      // Use default parameter value.
      replacement_text = formals_iter->default_value;
    }
    // else leave blank as empty string.
  }
  return absl::OkStatus();
}

const TokenInfo& MacroDefinition::SubstituteText(
    const substitution_map_type& substitution_map, const TokenInfo& token_info,
    int actual_token_enum) {
  if (actual_token_enum == 0 ||
      (actual_token_enum == token_info.token_enum())) {
    const auto* replacement = FindOrNull(substitution_map, token_info.text());
    if (replacement) {
      // Substitute formal parameter for actual text.
      return *replacement;
    }
  }
  // Didn't match enum type nor find map entry, so don't substitute.
  return token_info;
}

}  // namespace verible
