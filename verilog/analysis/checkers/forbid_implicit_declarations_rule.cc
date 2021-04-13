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

#include "verilog/analysis/checkers/forbid_implicit_declarations_rule.h"

#include <set>
#include <string>

#include "absl/strings/str_cat.h"
#include "absl/strings/string_view.h"
#include "common/analysis/citation.h"
#include "common/analysis/lint_rule_status.h"
#include "common/analysis/matcher/bound_symbol_manager.h"
#include "common/text/symbol.h"
#include "common/text/syntax_tree_context.h"
#include "common/text/tree_context_visitor.h"
#include "verilog/CST/identifier.h"
#include "verilog/analysis/descriptions.h"
#include "verilog/analysis/lint_rule_registry.h"
#include "verilog/analysis/symbol_table.h"

namespace verilog {
namespace analysis {

using verible::GetStyleGuideCitation;
using verible::LintRuleStatus;
using verible::LintViolation;
using verible::SyntaxTreeContext;

// Register ForbidImplicitDeclarationsRule
VERILOG_REGISTER_LINT_RULE(ForbidImplicitDeclarationsRule);

// forbid-implicit-net-declarations?
absl::string_view ForbidImplicitDeclarationsRule::Name() {
  return "forbid-implicit-declarations";
}
const char ForbidImplicitDeclarationsRule::kTopic[] = "implicit-declarations";
const char ForbidImplicitDeclarationsRule::kMessage[] =
    "Nets must be declared explicitly.";

std::string ForbidImplicitDeclarationsRule::GetDescription(
    DescriptionType description_type) {
  return absl::StrCat(
      "Checks that there are no occurrences of "
      "implicitly declared nets.");
}

void ForbidImplicitDeclarationsRule::Lint(
    const verible::TextStructureView& text_structure,
    absl::string_view filename) {
  SymbolTable symbol_table(nullptr);

  ParsedVerilogSourceFile* src =
      new ParsedVerilogSourceFile("internal", &text_structure);
  // Already parsed, calling to ensure that VerilogSourceFile internals are in
  // correct state
  const auto status = src->Parse();
  CHECK_EQ(status.ok(), true);

  auto diagnostics = BuildSymbolTable(*src, &symbol_table);
  for (const auto& itr : diagnostics) {
    CHECK_EQ(itr.ok(), true);
  }
  // Skipping resolving stage as implicit declarations are pre-resolved
  // during symbol table building stage

  auto& violations = this->violations_;
  symbol_table.Root().ApplyPreOrder([&violations, &text_structure](
                                        const SymbolTableNode& node) {
    for (const auto& itr : node.Value().local_references_to_bind) {
      ABSL_DIE_IF_NULL(itr.LastLeaf())
          ->ApplyPreOrder([&violations,
                           &text_structure](const ReferenceComponent& node) {
            // Skip unresolved symbols (implicit declarations are pre-resolved)
            if (node.resolved_symbol == nullptr) {
              return;
            }

            const auto& resolved_symbol_node =
                *ABSL_DIE_IF_NULL(node.resolved_symbol);
            const auto& resolved_symbol = resolved_symbol_node.Value();
            const auto& resolved_symbol_identifier =
                *ABSL_DIE_IF_NULL(resolved_symbol_node.Key());

            // Skip pre-resolved symbols that have explicit declarations
            if (resolved_symbol.declared_type.implicit == false) {
              return;
            }

            // Only report reference that caused implicit declarations
            if (node.identifier.begin() == resolved_symbol_identifier.begin()) {
              const auto offset = std::distance(
                  text_structure.Contents().begin(), node.identifier.begin());
              CHECK_GE(offset, 0);
              auto range =
                  text_structure.TokenRangeSpanningOffsets(offset, offset);
              auto token = range.begin();
              CHECK(token != text_structure.TokenStream().end());
              const auto& token_info = *token;
              violations.insert(LintViolation(token_info, kMessage));
            }
          });
    }
  });
}

LintRuleStatus ForbidImplicitDeclarationsRule::Report() const {
  return LintRuleStatus(violations_, Name(), GetStyleGuideCitation(kTopic));
}

}  // namespace analysis
}  // namespace verilog
