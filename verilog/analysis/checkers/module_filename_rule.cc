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

#include "verilog/analysis/checkers/module_filename_rule.h"

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
VERILOG_REGISTER_LINT_RULE(ModuleFilenameRule);

absl::string_view ModuleFilenameRule::Name() { return "module-filename"; }
const char ModuleFilenameRule::kTopic[] = "file-names";
const char ModuleFilenameRule::kMessage[] =
    "Declared module does not match the first dot-delimited component "
    "of file name: ";

std::string ModuleFilenameRule::GetDescription(
    DescriptionType description_type) {
  return absl::StrCat(
      "If a module is declared, checks that at least one module matches "
      "the first dot-delimited component of the file name.  See ",
      GetStyleGuideCitation(kTopic), ".");
}

static bool ModuleNameMatches(const verible::Symbol& s,
                              absl::string_view name) {
  const auto& token_info = GetModuleNameToken(s);
  return token_info.text == name;
}

void ModuleFilenameRule::Lint(const TextStructureView& text_structure,
                              absl::string_view filename) {
  const auto& tree = text_structure.SyntaxTree();
  if (tree == nullptr) return;

  // Find all module declarations.
  auto module_matches = FindAllModuleDeclarations(*tree);

  // If there are no modules in this source unit, suppress finding.
  if (module_matches.empty()) return;

  // Remove nested module declarations
  std::vector<verible::TreeSearchMatch> module_cleaned;
  module_cleaned.reserve(module_matches.size());
  std::back_insert_iterator<std::vector<verible::TreeSearchMatch>> back_it(module_cleaned);
  std::remove_copy_if(module_matches.begin(),
                      module_matches.end(),
                      back_it,
              [](verible::TreeSearchMatch& m) {
                  return m.context.IsInside(NodeEnum::kModuleDeclaration);
              });

  // See if any names match the stem of the filename.
  const absl::string_view basename = verible::file::Basename(filename);
  std::vector<absl::string_view> basename_components =
      absl::StrSplit(basename, '.');
  const absl::string_view unitname = basename_components[0];
  if (unitname.empty()) return;

  auto matching_module_iter =
      std::find_if(module_cleaned.begin(), module_cleaned.end(),
                   [=](const verible::TreeSearchMatch& m) {
                     return ModuleNameMatches(*m.match, unitname);
                   });


  // If there is at least one module with a matching name, suppress finding.
  if (matching_module_iter != module_cleaned.end()) return;

  // Only report a violation on the last module declaration.
  const auto& last_module_id = GetModuleNameToken(*module_cleaned.back().match);
  violations_.insert(verible::LintViolation(
      last_module_id, absl::StrCat(kMessage, "\"", unitname, "\"")));
}

LintRuleStatus ModuleFilenameRule::Report() const {
  return LintRuleStatus(violations_, Name(), GetStyleGuideCitation(kTopic));
}

}  // namespace analysis
}  // namespace verilog
