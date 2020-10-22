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

#include "verilog/analysis/checkers/one_module_per_file_rule.h"

#include <algorithm>
#include <iterator>
#include <memory>
#include <set>
#include <string>
#include <vector>

#include "absl/strings/str_cat.h"
#include "absl/strings/str_split.h"
#include "absl/strings/string_view.h"
#include "common/analysis/citation.h"
#include "common/analysis/lint_rule_status.h"
#include "common/analysis/syntax_tree_search.h"
#include "common/text/symbol.h"
#include "common/text/text_structure.h"
#include "common/text/token_info.h"
#include "common/util/file_util.h"
#include "verilog/CST/module.h"
#include "verilog/CST/verilog_matchers.h"
#include "verilog/analysis/descriptions.h"
#include "verilog/analysis/lint_rule_registry.h"

namespace verilog {
namespace analysis {

using verible::GetStyleGuideCitation;
using verible::LintRuleStatus;
using verible::TextStructureView;

// Register the lint rule
VERILOG_REGISTER_LINT_RULE(OneModulePerFileRule);

absl::string_view OneModulePerFileRule::Name() { return "one-module-per-file"; }
const char OneModulePerFileRule::kTopic[] = "file-extensions";
const char OneModulePerFileRule::kMessage[] =
    "Each file should have only one module declaration. Found: ";

std::string OneModulePerFileRule::GetDescription(
    DescriptionType description_type) {
  return absl::StrCat(
      "Checks that at most one module is declared per file. See ",
      GetStyleGuideCitation(kTopic), ".");
}

void OneModulePerFileRule::Lint(const TextStructureView& text_structure,
                                absl::string_view) {
  const auto& tree = text_structure.SyntaxTree();
  if (tree == nullptr) return;

  auto module_matches = FindAllModuleDeclarations(*tree);
  if (module_matches.empty()) {
    return;
  }

  // Nested module declarations are allowed, remove those
  std::vector<verible::TreeSearchMatch> module_cleaned;
  module_cleaned.reserve(module_matches.size());
  std::back_insert_iterator<std::vector<verible::TreeSearchMatch>> back_it(
      module_cleaned);
  std::remove_copy_if(module_matches.begin(), module_matches.end(), back_it,
                      [](verible::TreeSearchMatch& m) {
                        return m.context.IsInside(NodeEnum::kModuleDeclaration);
                      });

  if (module_cleaned.size() > 1) {
    // Report second module declaration
    const auto& second_module_id = GetModuleName(*module_cleaned[1].match);
    violations_.insert(verible::LintViolation(
        second_module_id.get(), absl::StrCat(kMessage, module_cleaned.size())));
  }
}

LintRuleStatus OneModulePerFileRule::Report() const {
  return LintRuleStatus(violations_, Name(), GetStyleGuideCitation(kTopic));
}

}  // namespace analysis
}  // namespace verilog
