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

#include "verible/verilog/analysis/checkers/one-module-per-file-rule.h"

#include <algorithm>
#include <iterator>
#include <memory>
#include <set>
#include <vector>

#include "absl/strings/str_cat.h"
#include "absl/strings/string_view.h"
#include "verible/common/analysis/lint-rule-status.h"
#include "verible/common/analysis/syntax-tree-search.h"
#include "verible/common/text/text-structure.h"
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
VERILOG_REGISTER_LINT_RULE(OneModulePerFileRule);

static constexpr absl::string_view kMessage =
    "Each file should have only one module declaration. Found: ";

const LintRuleDescriptor &OneModulePerFileRule::GetDescriptor() {
  static const LintRuleDescriptor d{
      .name = "one-module-per-file",
      .topic = "file-extensions",
      .desc = "Checks that at most one module is declared per file.",
  };
  return d;
}

void OneModulePerFileRule::Lint(const TextStructureView &text_structure,
                                absl::string_view) {
  const auto &tree = text_structure.SyntaxTree();
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
                      [](verible::TreeSearchMatch &m) {
                        return m.context.IsInside(NodeEnum::kModuleDeclaration);
                      });

  if (module_cleaned.size() > 1) {
    // Report second module declaration
    const auto *second_module_id = GetModuleName(*module_cleaned[1].match);
    if (!second_module_id) LOG(ERROR) << "Couldn't extract module name";
    if (second_module_id) {
      violations_.insert(verible::LintViolation(
          second_module_id->get(),
          absl::StrCat(kMessage, module_cleaned.size())));
    }
  }
}

LintRuleStatus OneModulePerFileRule::Report() const {
  return LintRuleStatus(violations_, GetDescriptor());
}

}  // namespace analysis
}  // namespace verilog
