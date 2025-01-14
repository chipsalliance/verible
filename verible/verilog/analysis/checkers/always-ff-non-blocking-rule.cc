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

#include "verible/verilog/analysis/checkers/always-ff-non-blocking-rule.h"

#include <algorithm>
#include <ostream>
#include <set>
#include <string_view>

#include "absl/status/status.h"
#include "verible/common/analysis/lint-rule-status.h"
#include "verible/common/analysis/matcher/bound-symbol-manager.h"
#include "verible/common/analysis/matcher/matcher.h"
#include "verible/common/analysis/syntax-tree-search.h"
#include "verible/common/text/concrete-syntax-leaf.h"
#include "verible/common/text/concrete-syntax-tree.h"
#include "verible/common/text/config-utils.h"
#include "verible/common/text/symbol.h"
#include "verible/common/text/syntax-tree-context.h"
#include "verible/common/util/casts.h"
#include "verible/common/util/logging.h"
#include "verible/verilog/CST/verilog-matchers.h"
#include "verible/verilog/CST/verilog-nonterminals.h"
#include "verible/verilog/analysis/descriptions.h"
#include "verible/verilog/analysis/lint-rule-registry.h"

namespace verilog {
namespace analysis {

using verible::LintRuleStatus;
using verible::LintViolation;
using verible::SearchSyntaxTree;
using verible::SyntaxTreeContext;
using verible::matcher::Matcher;

//- Info --------------------------------------------------------------------
// Register AlwaysFFNonBlockingRule
VERILOG_REGISTER_LINT_RULE(AlwaysFFNonBlockingRule);

static constexpr std::string_view kMessage =
    "Use blocking assignments, at most, for locals inside "
    "'always_ff' sequential blocks.";

const LintRuleDescriptor &AlwaysFFNonBlockingRule::GetDescriptor() {
  static const LintRuleDescriptor d{
      .name = "always-ff-non-blocking",
      .topic = "sequential-logic",
      .desc =
          "Checks that blocking assignments are, at most, targeting "
          "locals in sequential logic.",
      .param = {{"catch_modifying_assignments", "false"},
                {"waive_for_locals", "false"}},
  };
  return d;
}

LintRuleStatus AlwaysFFNonBlockingRule::Report() const {
  return LintRuleStatus(violations_, GetDescriptor());
}

//- Configuration -----------------------------------------------------------
absl::Status AlwaysFFNonBlockingRule::Configure(
    const std::string_view configuration) {
  using verible::config::SetBool;
  return verible::ParseNameValues(
      configuration, {
                         {"catch_modifying_assignments",
                          SetBool(&catch_modifying_assignments_)},
                         {"waive_for_locals", SetBool(&waive_for_locals_)},
                     });
}

//- Processing --------------------------------------------------------------
void AlwaysFFNonBlockingRule::HandleSymbol(const verible::Symbol &symbol,
                                           const SyntaxTreeContext &context) {
  //- Process and filter context before locating blocking assigments --------

  // Detect entering and leaving of always_ff blocks
  if (!InsideBlock(symbol, context.size())) return;

  // Collect local variable declarations
  if (LocalDeclaration(symbol)) return;

  // Drop out if inside a loop header
  if (context.IsInside(NodeEnum::kLoopHeader)) return;

  //- Check for blocking assignments of various kinds -----------------------
  static const Matcher asgn_blocking_matcher{NodekNetVariableAssignment()};
  static const Matcher asgn_modify_matcher{NodekAssignModifyStatement()};
  static const Matcher asgn_incdec_matcher{NodekIncrementDecrementExpression()};
  static const Matcher ident_matcher{NodekUnqualifiedId()};

  // Rule may be waived if complete lhs consists of local variables
  //  -> determine root of lhs
  const verible::Symbol *check_root = nullptr;

  verible::matcher::BoundSymbolManager symbol_man;
  if (asgn_blocking_matcher.Matches(symbol, &symbol_man)) {
    if (const auto *const node =
            verible::down_cast<const verible::SyntaxTreeNode *>(&symbol)) {
      check_root =
          /* lhs */ verible::down_cast<const verible::SyntaxTreeNode *>(
              node->front().get());
    }
  } else {
    // Not interested in any other blocking assignments unless flagged
    if (!catch_modifying_assignments_) return;

    if (asgn_modify_matcher.Matches(symbol, &symbol_man)) {
      if (const auto *const node =
              verible::down_cast<const verible::SyntaxTreeNode *>(&symbol)) {
        check_root =
            /* lhs */ verible::down_cast<const verible::SyntaxTreeNode *>(
                node->front().get());
      }
    } else if (asgn_incdec_matcher.Matches(symbol, &symbol_man)) {
      check_root = &symbol;
    } else {
      // Not a blocking assignment
      return;
    }
  }

  // Waive rule if syntax subtree containing relevant variables was found
  // and all turn out to be local
  bool waived = false;
  if (waive_for_locals_ && check_root) {
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
                    varn->front().get())) {
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
}  // HandleSymbol()

bool AlwaysFFNonBlockingRule::InsideBlock(const verible::Symbol &symbol,
                                          const int depth) {
  static const Matcher always_ff_matcher{
      NodekAlwaysStatement(AlwaysFFKeyword())};
  static const Matcher block_matcher{NodekBlockItemStatementList()};

  // Discard state from branches already left
  if (depth <= inside_) inside_ = 0;
  while (depth <= scopes_.top().syntax_tree_depth) {
    scopes_.pop();
    VLOG(4) << "POPped to scope DEPTH=" << scopes_.top().syntax_tree_depth
            << "; #locals_=" << scopes_.top().inherited_local_count
            << std::endl;
  }
  locals_.resize(scopes_.top().inherited_local_count);

  verible::matcher::BoundSymbolManager symbol_man;
  if (!inside_) {
    // Not analyzing an always_ff block. Entering a new one?
    if (always_ff_matcher.Matches(symbol, &symbol_man)) {
      VLOG(4) << "always_ff @DEPTH=" << depth << std::endl;
      inside_ = depth;
    }
    return false;
  }

  // We are inside an always_ff block

  // Opening a begin-end block
  if (block_matcher.Matches(symbol, &symbol_man)) {
    VLOG(4) << "PUSHing scope: DEPTH=" << depth
            << "; #locals_ inherited=" << locals_.size() << std::endl;
    scopes_.emplace(Scope{depth, locals_.size()});
    return false;
  }

  return true;
}  // InsideBlock()

bool AlwaysFFNonBlockingRule::LocalDeclaration(const verible::Symbol &symbol) {
  static const Matcher decl_matcher{NodekDataDeclaration()};
  static const Matcher var_matcher{NodekRegisterVariable()};

  verible::matcher::BoundSymbolManager symbol_man;
  if (decl_matcher.Matches(symbol, &symbol_man)) {
    auto &count = scopes_.top().inherited_local_count;
    for (const auto &var : SearchSyntaxTree(symbol, var_matcher)) {
      if (const auto *const node =
              verible::down_cast<const verible::SyntaxTreeNode *>(var.match)) {
        if (const auto *const ident =
                verible::down_cast<const verible::SyntaxTreeLeaf *>(
                    node->front().get())) {
          const std::string_view name = ident->get().text();
          VLOG(4) << "Registering '" << name << '\'' << std::endl;
          locals_.emplace_back(name);
          count++;
        }
      }
    }
    return true;
  }
  return false;
}  // LocalDeclaration()

}  // namespace analysis
}  // namespace verilog
