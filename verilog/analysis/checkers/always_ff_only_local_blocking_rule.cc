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

#include "verilog/analysis/checkers/always_ff_only_local_blocking_rule.h"

#include <set>
#include <string>

#include "absl/strings/str_cat.h"
#include "absl/strings/string_view.h"
#include "common/analysis/citation.h"
#include "common/analysis/lint_rule_status.h"
#include "common/analysis/matcher/bound_symbol_manager.h"
#include "common/analysis/matcher/matcher.h"
#include "common/analysis/syntax_tree_search.h"
#include "common/text/config_utils.h"
#include "common/text/symbol.h"
#include "common/text/syntax_tree_context.h"
#include "common/util/casts.h"
#include "verilog/CST/verilog_matchers.h"  // IWYU pragma: keep
#include "verilog/analysis/descriptions.h"
#include "verilog/analysis/lint_rule_registry.h"

namespace verilog {
namespace analysis {

using verible::GetStyleGuideCitation;
using verible::LintRuleStatus;
using verible::LintViolation;
using verible::SearchSyntaxTree;
using verible::SyntaxTreeContext;
using verible::matcher::Matcher;

//- Info --------------------------------------------------------------------

// Register AlwaysFFOnlyLocalBlockingRule
VERILOG_REGISTER_LINT_RULE(AlwaysFFOnlyLocalBlockingRule);

absl::string_view AlwaysFFOnlyLocalBlockingRule::Name() {
  return "always-ff-only-local-blocking";
}
char const AlwaysFFOnlyLocalBlockingRule::kTopic[] = "sequential-logic";
char const AlwaysFFOnlyLocalBlockingRule::kMessage[] =
    "Use blocking assignments only for locals inside "
    "'always_ff' sequential blocks.";

std::string AlwaysFFOnlyLocalBlockingRule::GetDescription(
    DescriptionType description_type) {
  return
    "Checks that there are no occurrences of "
    "blocking assignment to non-locals in sequential logic.";
}

LintRuleStatus AlwaysFFOnlyLocalBlockingRule::Report() const {
  return LintRuleStatus(violations_, Name(), GetStyleGuideCitation(kTopic));
}

//- Configuration -----------------------------------------------------------
absl::Status AlwaysFFOnlyLocalBlockingRule::Configure(
    const absl::string_view configuration) {

  using verible::config::SetBool;
  return
    verible::ParseNameValues(configuration, {
        {"catch_modifying_assigns", SetBool(&catch_modifying_assigns_)},
        {"waive_for_locals",        SetBool(&waive_for_locals_)},
      }
    );
}

//- Processing --------------------------------------------------------------
void AlwaysFFOnlyLocalBlockingRule::HandleSymbol(
    const verible::Symbol &symbol, const SyntaxTreeContext &context) {
  static const Matcher always_ff_matcher{NodekAlwaysStatement(AlwaysFFKeyword())};
  static const Matcher block_matcher{NodekBlockItemStatementList()};
  static const Matcher decl_matcher{NodekDataDeclaration()};
  static const Matcher var_matcher{NodekRegisterVariable()};
  static const Matcher asgn_blocking_matcher{NodekNetVariableAssignment()};
  static const Matcher asgn_modify_matcher{NodekAssignModifyStatement()};
  static const Matcher asgn_incdec_matcher{NodekIncrementDecrementExpression()};
  static const Matcher ident_matcher{NodekUnqualifiedId()};

  // Determine depth in syntax tree and discard state from branches already left
  const int depth = context.size();
  if (depth <= inside_) inside_ = 0;
  while (depth <= scopes_.top().first) {
    scopes_.pop();
    VLOG(4) << "POPped to scope DEPTH=" << scopes_.top().first
            << "; #locals_=" << scopes_.top().second << std::endl;
  }
  locals_.resize(scopes_.top().second);

  verible::matcher::BoundSymbolManager symbol_man;

  if (always_ff_matcher.Matches(symbol, &symbol_man)) {
    // Check for entering an always_ff block
    VLOG(4) << "always_ff @DEPTH=" << depth << std::endl;
    inside_ = depth;
  } else if (inside_) {
    // Open up begin-end block
    if (block_matcher.Matches(symbol, &symbol_man)) {
      VLOG(4) << "PUSHing scope: DEPTH=" << depth
              << "; #locals_ inherited=" << locals_.size() << std::endl;
      scopes_.emplace(depth, locals_.size());
    } else if (decl_matcher.Matches(symbol, &symbol_man)) {
      // Collect local variable declarations
      auto &count = scopes_.top().second;
      for (const auto &var : SearchSyntaxTree(symbol, var_matcher)) {
        if (const auto *const node =
                verible::down_cast<const verible::SyntaxTreeNode *>(var.match)) {
          if (const auto *const ident =
                  verible::down_cast<const verible::SyntaxTreeLeaf *>(
                      node->children()[0].get())) {
            const absl::string_view name = ident->get().text();
            VLOG(4) << "Registering '" << name << '\'' << std::endl;
            locals_.emplace_back(name);
            count++;
          }
        }
      }
    } else if (!context.IsInside(NodeEnum::kLoopHeader)) {
      // Check for blocking assignments of various kinds outside loop headers
      const verible::Symbol *check_root = nullptr;
      if (asgn_blocking_matcher.Matches(symbol, &symbol_man) ||
          asgn_modify_matcher.Matches(symbol, &symbol_man)) {
        if (const auto *const node =
                dynamic_cast<const verible::SyntaxTreeNode *>(&symbol)) {
          // Check all left-hand-side variables to potentially waive the rule
          check_root = /* lhs */ verible::down_cast<const verible::SyntaxTreeNode *>(
              node->children()[0].get());
        }
      } else if (asgn_incdec_matcher.Matches(symbol, &symbol_man)) {
        // Check all mentioned variables to potentially waive the rule
        check_root = &symbol;
      } else {
        // No blocking assignment
        return;
      }

      // Waive rule if syntax subtree containing relevant variables was found
      // and all turn out to be local
      bool waived = false;
      if (check_root) {
        waived = true;
        for (const auto &var : SearchSyntaxTree(*check_root, ident_matcher)) {
          if (var.context.IsInside(NodeEnum::kDimensionScalar)) continue;
          if (var.context.IsInside(NodeEnum::kDimensionSlice)) continue;
          if (var.context.IsInside(NodeEnum::kHierarchyExtension)) continue;

          bool found = false;
          if (const auto *const varn =
                  verible::down_cast<const verible::SyntaxTreeNode *>(var.match)) {
            if (const auto *const ident =
                    verible::down_cast<const verible::SyntaxTreeLeaf *>(
                        varn->children()[0].get())) {
              found = std::find(locals_.begin(), locals_.end(),
                                ident->get().text()) != locals_.end();
              VLOG(4) << "LHS='" << ident->get().text() << "' FOUND=" << found
                      << std::endl;
            }
          }
          waived &= found;
        }
      }

      // Enqueue detected violation unless waived
      if (!waived) violations_.insert(LintViolation(symbol, kMessage, context));
    }
  }
}

}  // namespace analysis
}  // namespace verilog
