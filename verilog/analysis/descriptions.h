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

// This file defines helper functions to gather the descriptions of all the
// rules in order to produce documentation (CLI help text and markdown) about
// the lint rules.

#ifndef VERIBLE_VERILOG_ANALYSIS_DESCRIPTIONS_H_
#define VERIBLE_VERILOG_ANALYSIS_DESCRIPTIONS_H_

#include <string>
#include <string_view>
#include <vector>

namespace verilog {
namespace analysis {

using LintRuleId = std::string_view;

struct LintConfigParameterDescriptor {
  std::string_view name;
  std::string default_value;
  std::string description;
};

struct LintRuleDescriptor {
  LintRuleId name;         // ID/name of the rule.
  std::string_view topic;  // section in style-guide
  std::string desc;        // Detailed description.
  std::vector<LintConfigParameterDescriptor> param;
};

}  // namespace analysis
}  // namespace verilog

#endif  // VERIBLE_VERILOG_ANALYSIS_DESCRIPTIONS_H_
