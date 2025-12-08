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

#include "verible/verilog/analysis/checkers/module-filename-rule.h"

#include <algorithm>
#include <iterator>
#include <memory>
#include <set>
#include <string>
#include <string_view>
#include <vector>

#include "absl/status/status.h"
#include "absl/strings/str_cat.h"
#include "absl/strings/str_split.h"
#include "verible/common/analysis/lint-rule-status.h"
#include "verible/common/analysis/syntax-tree-search.h"
#include "verible/common/text/concrete-syntax-leaf.h"
#include "verible/common/text/config-utils.h"
#include "verible/common/text/symbol.h"
#include "verible/common/text/text-structure.h"
#include "verible/common/text/token-info.h"
#include "verible/common/util/file-util.h"
#include "verible/common/util/logging.h"
#include "verible/verilog/CST/module.h"
#include "verible/verilog/CST/verilog-nonterminals.h"
#include "verible/verilog/analysis/descriptions.h"
#include "verible/verilog/analysis/lint-rule-registry.h"

namespace verilog {
namespace analysis {

using verible::LintRuleStatus;
using verible::TextStructureView;

// Register the lint rule
VERILOG_REGISTER_LINT_RULE(ModuleFilenameRule);

static constexpr std::string_view kMessage =
    "Declared module does not match the first dot-delimited component "
    "of file name: ";

const LintRuleDescriptor &ModuleFilenameRule::GetDescriptor() {
  static const LintRuleDescriptor d{
      .name = "module-filename",
      .topic = "file-names",
      .desc =
          "If a module is declared, checks that at least one module matches "
          "the first dot-delimited component of the file name. Depending on "
          "configuration, it is also allowed to replace underscore with dashes "
          "in filenames.",
      .param = {{"allow-dash-for-underscore", "false",
                 "Allow dashes in the filename where there are dashes in the "
                 "module name"}},
  };
  return d;
}

static bool ModuleNameMatches(const verible::Symbol &s, std::string_view name) {
  const auto *module_leaf = GetModuleName(s);
  return module_leaf && module_leaf->get().text() == name;
}

void ModuleFilenameRule::Lint(const TextStructureView &text_structure,
                              std::string_view filename) {
  if (verible::file::IsStdin(filename)) {
    return;
  }

  const auto &tree = text_structure.SyntaxTree();
  if (tree == nullptr) return;

  // Find all module declarations.
  auto module_matches = FindAllModuleDeclarations(*tree);

  // If there are no modules in this source unit, suppress finding.
  if (module_matches.empty()) return;

  // Remove nested module declarations
  std::vector<verible::TreeSearchMatch> module_cleaned;
  module_cleaned.reserve(module_matches.size());
  std::back_insert_iterator<std::vector<verible::TreeSearchMatch>> back_it(
      module_cleaned);
  std::remove_copy_if(module_matches.begin(), module_matches.end(), back_it,
                      [](verible::TreeSearchMatch &m) {
                        return m.context.IsInside(NodeEnum::kModuleDeclaration);
                      });

  // See if any names match the stem of the filename.
  const std::string_view basename = verible::file::Basename(filename);

  std::vector<std::string_view> basename_components =
      absl::StrSplit(basename, '.');
  std::string unitname(basename_components[0].begin(),
                       basename_components[0].end());
  if (unitname.empty()) return;

  if (allow_dash_for_underscore_) {
    // If we allow for dashes, let's first convert them back to underscore.
    std::replace(unitname.begin(), unitname.end(), '-', '_');
  }

  // If there is at least one module with a matching name, suppress finding.
  if (std::any_of(module_cleaned.begin(), module_cleaned.end(),
                  [=](const verible::TreeSearchMatch &m) {
                    return ModuleNameMatches(*m.match, unitname);
                  })) {
    return;
  }

  // Only report a violation on the last module declaration.
  const verible::Symbol &last_module = *module_cleaned.back().match;
  const auto *last_module_id = GetModuleName(last_module);
  if (!last_module_id) LOG(ERROR) << "Couldn't extract module name";
  if (last_module_id) {
    const std::string autofix_msg =
        absl::StrCat("Rename module to '", unitname, "' to match filename");

    auto autofix =
        verible::AutoFix(autofix_msg, {last_module_id->get(), unitname});

    const verible::SyntaxTreeLeaf *module_end_label =
        GetModuleEndLabel(last_module);
    if (module_end_label) {
      autofix.AddEdits({{module_end_label->get(), unitname}});
    }

    verible::LintViolation violation = verible::LintViolation(
        last_module_id->get(), absl::StrCat(kMessage, "\"", unitname, "\""),
        {autofix});

    violations_.insert(violation);
  }
}

LintRuleStatus ModuleFilenameRule::Report() const {
  return LintRuleStatus(violations_, GetDescriptor());
}

absl::Status ModuleFilenameRule::Configure(std::string_view configuration) {
  using verible::config::SetBool;
  return verible::ParseNameValues(
      configuration,
      {{"allow-dash-for-underscore", SetBool(&allow_dash_for_underscore_)}});
}
}  // namespace analysis
}  // namespace verilog
