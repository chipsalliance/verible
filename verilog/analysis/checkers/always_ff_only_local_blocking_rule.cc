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
#include "common/text/symbol.h"
#include "common/text/syntax_tree_context.h"
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

// Register AlwaysFFOnlyLocalBlockingRule
VERILOG_REGISTER_LINT_RULE(AlwaysFFOnlyLocalBlockingRule);

absl::string_view AlwaysFFOnlyLocalBlockingRule::Name() {
  return "always-ff-only-local-blocking";
}
const char AlwaysFFOnlyLocalBlockingRule::kTopic[] = "sequential-logic";
const char AlwaysFFOnlyLocalBlockingRule::kMessage[] =
    "Use blocking assignments only for locals inside \'always_ff\' sequential "
    "blocks.";

std::string AlwaysFFOnlyLocalBlockingRule::GetDescription(
    DescriptionType description_type) {
  return absl::StrCat("Checks that there are no occurrences of ",
                      "blocking assignment to non-locals in sequential logic.");
}

void AlwaysFFOnlyLocalBlockingRule::HandleSymbol(
    verible::Symbol const &symbol, SyntaxTreeContext const &context) {
  static Matcher const always_ff_matcher{NodekAlwaysStatement(AlwaysFFKeyword())};
  static Matcher const block_matcher{NodekBlockItemStatementList()};
  static Matcher const decl_matcher{NodekDataDeclaration()};
  static Matcher const var_matcher{NodekRegisterVariable()};
  static Matcher const asgn_blocking_matcher{NodekNetVariableAssignment()};
  static Matcher const asgn_modify_matcher{NodekAssignModifyStatement()};
  static Matcher const asgn_incdec_matcher{NodekIncrementDecrementExpression()};
  static Matcher const ident_matcher{NodekUnqualifiedId()};

  // Determine depth in syntax tree and discard state from branches already left
  int const depth = context.size();
  if (depth <= this->inside) this->inside = 0;
  while (depth <= scopes.top().first) {
    scopes.pop();
    VLOG(4) << "POPped to scope DEPTH=" << scopes.top().first
            << "; #LOCALS=" << scopes.top().second << std::endl;
  }
  locals.resize(scopes.top().second);

  verible::matcher::BoundSymbolManager symbol_man;

  // Check for entering an always_ff block
  if (always_ff_matcher.Matches(symbol, &symbol_man)) {
    VLOG(4) << "always_ff @DEPTH=" << depth << std::endl;
    this->inside = depth;
  } else if (this->inside) {
    // Open up begin-end block
    if (block_matcher.Matches(symbol, &symbol_man)) {
      VLOG(4) << "PUSHing scope: DEPTH=" << depth
              << "; #LOCALs inherited=" << locals.size() << std::endl;
      scopes.emplace(depth, locals.size());
    }
    // Collect local variable declarations
    else if (decl_matcher.Matches(symbol, &symbol_man)) {
      auto &cnt = scopes.top().second;
      for (auto const &var : SearchSyntaxTree(symbol, var_matcher)) {
        if (auto const *const node =
                dynamic_cast<verible::SyntaxTreeNode const *>(var.match)) {
          if (auto const *const ident =
                  dynamic_cast<verible::SyntaxTreeLeaf const *>(
                      node->children()[0].get())) {
            absl::string_view const name = ident->get().text();
            VLOG(4) << "Registering '" << name << '\'' << std::endl;
            locals.emplace_back(name);
            cnt++;
          }
        }
      }
    }
    // Check for blocking assignments of various kinds outside loop headers
    else if (!context.IsInside(NodeEnum::kLoopHeader)) {
      verible::Symbol const *check_root = nullptr;
      if (asgn_blocking_matcher.Matches(symbol, &symbol_man) ||
          asgn_modify_matcher.Matches(symbol, &symbol_man)) {
        if (auto const *const node =
                dynamic_cast<verible::SyntaxTreeNode const *>(&symbol)) {
          // Check all left-hand-side variables to potentially waive the rule
          check_root = /* lhs */ dynamic_cast<verible::SyntaxTreeNode const *>(
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
        for (auto const &var : SearchSyntaxTree(*check_root, ident_matcher)) {
          if (var.context.IsInside(NodeEnum::kDimensionScalar)) continue;
          if (var.context.IsInside(NodeEnum::kDimensionSlice)) continue;
          if (var.context.IsInside(NodeEnum::kHierarchyExtension)) continue;

          bool found = false;
          if (auto const *const varn =
                  dynamic_cast<verible::SyntaxTreeNode const *>(var.match)) {
            if (auto const *const ident =
                    dynamic_cast<verible::SyntaxTreeLeaf const *>(
                        varn->children()[0].get())) {
              found = std::find(locals.begin(), locals.end(),
                                ident->get().text()) != locals.end();
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

LintRuleStatus AlwaysFFOnlyLocalBlockingRule::Report() const {
  return LintRuleStatus(violations_, Name(), GetStyleGuideCitation(kTopic));
}

}  // namespace analysis
}  // namespace verilog
