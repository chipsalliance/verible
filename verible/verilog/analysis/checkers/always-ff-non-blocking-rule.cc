// Copyright 2017-2023 The Verible Authors.
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
#include <iterator>
#include <ostream>
#include <set>
#include <string>
#include <vector>

#include "absl/status/status.h"
#include "absl/strings/str_cat.h"
#include "absl/strings/string_view.h"
#include "verible/common/analysis/lint-rule-status.h"
#include "verible/common/analysis/matcher/bound-symbol-manager.h"
#include "verible/common/analysis/matcher/matcher.h"
#include "verible/common/analysis/syntax-tree-search.h"
#include "verible/common/text/concrete-syntax-leaf.h"
#include "verible/common/text/concrete-syntax-tree.h"
#include "verible/common/text/config-utils.h"
#include "verible/common/text/symbol.h"
#include "verible/common/text/syntax-tree-context.h"
#include "verible/common/text/tree_utils.h"
#include "verible/common/util/casts.h"
#include "verible/common/util/logging.h"
#include "verible/verilog/CST/expression.h"
#include "verible/verilog/CST/statement.h"
#include "verible/verilog/CST/verilog-matchers.h"
#include "verible/verilog/CST/verilog-nonterminals.h"
#include "verible/verilog/analysis/descriptions.h"
#include "verible/verilog/analysis/lint-rule-registry.h"

namespace verilog {
namespace analysis {

using verible::AutoFix;
using verible::LintRuleStatus;
using verible::LintViolation;
using verible::SearchSyntaxTree;
using verible::SyntaxTreeContext;
using verible::matcher::Matcher;

//- Info --------------------------------------------------------------------
// Register AlwaysFFNonBlockingRule
VERILOG_REGISTER_LINT_RULE(AlwaysFFNonBlockingRule);

static constexpr absl::string_view kMessage =
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
    const absl::string_view configuration) {
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

  std::vector<AutoFix> autofixes;

  // Rule may be waived if complete lhs consists of local variables
  //  -> determine root of lhs
  const verible::Symbol *check_root = nullptr;
  absl::string_view lhs_id;

  verible::matcher::BoundSymbolManager symbol_man;
  if (asgn_blocking_matcher.Matches(symbol, &symbol_man)) {
    const verible::SyntaxTreeNode *node = &verible::SymbolCastToNode(symbol);
    check_root = verilog::GetNetVariableAssignmentLhs(*node);
    lhs_id = verible::StringSpanOfSymbol(*check_root);

    const verible::SyntaxTreeLeaf *equals =
        verilog::GetNetVariableAssignmentOperator(*node);

    autofixes.emplace_back(AutoFix(
        "Substitute blocking assignment '=' for nonblocking assignment '<='",
        {equals->get(), "<="}));
  } else {
    // Not interested in any other blocking assignments unless flagged
    if (!catch_modifying_assignments_) return;

    // These autofixes require substituting the whole expression
    absl::string_view original = verible::StringSpanOfSymbol(symbol);
    if (asgn_modify_matcher.Matches(symbol, &symbol_man)) {
      const verible::SyntaxTreeNode *node = &verible::SymbolCastToNode(symbol);
      const verible::SyntaxTreeNode &lhs = *GetAssignModifyLhs(*node);
      const verible::SyntaxTreeNode &rhs = *GetAssignModifyRhs(*node);

      lhs_id = verible::StringSpanOfSymbol(lhs);
      check_root = &lhs;

      bool needs_parenthesis = NeedsParenthesis(rhs);
      absl::string_view start_rhs_expr = needs_parenthesis ? " (" : " ";
      absl::string_view end_rhs_expr = needs_parenthesis ? ");" : ";";

      // Extract just the operation. Just '+' from '+='
      const absl::string_view op =
          verible::StringSpanOfSymbol(*GetAssignModifyOperator(*node))
              .substr(0, 1);

      const std::string fix =
          absl::StrCat(lhs_id, " <= ", lhs_id, " ", op, start_rhs_expr,
                       verible::StringSpanOfSymbol(rhs), end_rhs_expr);

      autofixes.emplace_back(
          AutoFix("Substitute assignment operator for equivalent "
                  "nonblocking assignment",
                  {original, fix}));
    } else if (asgn_incdec_matcher.Matches(symbol, &symbol_man)) {
      const verible::SyntaxTreeNode *operand =
          GetIncrementDecrementOperand(symbol);

      check_root = operand;
      lhs_id = verible::StringSpanOfSymbol(*operand);

      // Extract just the operation. Just '+' from '++'
      const absl::string_view op =
          verible::StringSpanOfSymbol(*GetIncrementDecrementOperator(symbol))
              .substr(0, 1);

      // Equivalent nonblocking assignment
      // {'x++', '++x'} become 'x <= x + 1'
      // {'x--', '--x'} become 'x <= x - 1'
      const std::string fix =
          absl::StrCat(lhs_id, " <= ", lhs_id, " ", op, " 1;");

      autofixes.emplace_back(
          AutoFix("Substitute increment/decrement operator for "
                  "equivalent nonblocking assignment",
                  {original, fix}));
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
  if (waived) return;

  // Dont autofix if the faulting expression is inside an expression
  // Example: "p <= k++"
  if (IsAutoFixSafe(symbol, lhs_id) &&
      !context.IsInside(NodeEnum::kExpression)) {
    violations_.insert(LintViolation(symbol, kMessage, context, autofixes));
  } else {
    violations_.insert(LintViolation(symbol, kMessage, context));
  }
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
      CollectLocalReferences(symbol);
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
          const absl::string_view name = ident->get().text();
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

bool AlwaysFFNonBlockingRule::NeedsParenthesis(
    const verible::Symbol &rhs) const {
  // Avoid inserting parenthesis for simple expressions
  // For example: x &= 1 -> x <= x & 1, and not x <= x & (1)
  // This could be more precise, but checking every specific
  // case where parenthesis are needed is hard. Adding them
  // doesn't hurt and the user can remove them if needed.
  const bool complex_rhs_expr =
      verible::GetLeftmostLeaf(rhs) != verible::GetRightmostLeaf(rhs);
  if (!complex_rhs_expr) return false;

  // Check if expression is wrapped in parenthesis
  const verible::Symbol *inner = verilog::UnwrapExpression(rhs);
  if (inner->Kind() == verible::SymbolKind::kLeaf) return false;

  return !verible::SymbolCastToNode(*inner).MatchesTag(NodeEnum::kParenGroup);
}

void AlwaysFFNonBlockingRule::CollectLocalReferences(
    const verible::Symbol &root) {
  const Matcher reference_matcher{NodekReference()};

  const std::vector<verible::TreeSearchMatch> references_ =
      verible::SearchSyntaxTree(root, reference_matcher);

  // Precompute the StringSpan of every identifier referenced to.
  // Avoid recomputing it several times
  references.resize(references_.size());
  std::transform(references_.cbegin(), references_.cend(), references.begin(),
                 [](const verible::TreeSearchMatch &match) {
                   return ReferenceWithId{
                       match, verible::StringSpanOfSymbol(*match.match)};
                 });
}

bool AlwaysFFNonBlockingRule::IsAutoFixSafe(
    const verible::Symbol &faulting_assignment,
    absl::string_view lhs_id) const {
  std::vector<ReferenceWithId>::const_iterator itr = references.end();

  // Let's assume that 'x' is the variable affected by the faulting
  // assignment. In order to ensure that the autofix is safe we have
  // to ensure that there is no later reference to 'x'
  //
  // Can't autofix        Can autofix
  // begin                begin
  //   x = x + 1;           x = x + 1;
  //   y = x;               y <= y + 1;
  // end                  end
  //
  // In practical terms: we'll scan the 'references' vector for kReferences
  // to 'x' that appear after the faulting assignment in the always_ff block

  const Matcher reference_matcher{NodekReference()};

  // Extract kReferences inside the faulting expression
  const std::vector<verible::TreeSearchMatch> references_ =
      verible::SearchSyntaxTree(faulting_assignment, reference_matcher);

  // ref points to the latest reference to 'x' in our faulting expression
  // 'x++', 'x = x + 1', 'x &= x + 1'
  //  ^          ^             ^
  //  We need this to know from where to start searching for later references
  //  to 'x', and decide whether the AutoFix is safe or not
  const verible::Symbol *ref =
      std::find_if(std::rbegin(references_), std::rend(references_),
                   [&](const verible::TreeSearchMatch &m) {
                     return verible::StringSpanOfSymbol(*m.match) == lhs_id;
                   })
          ->match;

  // Shouldn't happen, sanity check to avoid crashes
  if (!ref) return false;

  // Find the last reference to 'x' in the faulting assignment
  itr = std::find_if(
      std::begin(references), std::end(references),
      [&](const ReferenceWithId &r) { return r.match.match == ref; });

  // Search from the last reference to 'x' onwards. If there is any,
  // we can't apply the autofix safely
  itr = std::find_if(
      std::next(itr), std::end(references),
      [&](const ReferenceWithId &ref) { return ref.id == lhs_id; });

  // Let's say 'x' is affected by a flagged operation { 'x = 1', 'x++', ... }
  // We can safely autofix if after the flagged operation there are no
  // more references to 'x'
  return references.end() == itr;
}

}  // namespace analysis
}  // namespace verilog
