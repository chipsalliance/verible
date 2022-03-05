// Copyright 2017-2022 The Verible Authors.
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

#include <vector>

#include "verilog/analysis/verilog_linter_configuration.h"

namespace verilog {
// This is an empty stub-implementation of how a built-in policy could
// look like.
// See declaration in verilog_linter_configuration.h
const std::vector<ProjectPolicy>& GetBuiltinProjectPolicies() {
  static const std::vector<ProjectPolicy> kBuiltin{
      /*  Example
      {
        // name of policy
        "pocket_calculator",

        // paths affected (substring matched)
        {"src/pocket_calculator"},

        // paths excluded (substring matched)
        {"generated/stuff"},

        // policy reviewers
        {"trustyreviewer1", "trustyreviewer2"},

        // disabled rules
        {"undesirable-rule", "really-undesirable-rule"},

        // enabled rules
        {"opt-in-rule1", "opt-in-rule2"},
      },
      */
  };
  return kBuiltin;
}
}  // namespace verilog
