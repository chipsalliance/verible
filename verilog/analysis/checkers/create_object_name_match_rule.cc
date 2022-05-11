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

#include "verilog/analysis/checkers/create_object_name_match_rule.h"

#include <cstddef>
#include <memory>
#include <set>
#include <string>

#include "absl/strings/str_cat.h"
#include "absl/strings/string_view.h"
#include "common/analysis/lint_rule_status.h"
#include "common/analysis/matcher/bound_symbol_manager.h"
#include "common/text/concrete_syntax_leaf.h"
#include "common/text/concrete_syntax_tree.h"
#include "common/text/symbol.h"
#include "common/text/syntax_tree_context.h"
#include "common/text/token_info.h"
#include "common/text/tree_utils.h"
#include "common/util/casts.h"
#include "verilog/CST/expression.h"
#include "verilog/CST/verilog_matchers.h"
#include "verilog/CST/verilog_nonterminals.h"
#include "verilog/CST/verilog_tree_print.h"
#include "verilog/analysis/descriptions.h"
#include "verilog/analysis/lint_rule_registry.h"
#include "verilog/parser/verilog_token_enum.h"

namespace verilog {
namespace analysis {

using verible::down_cast;
using verible::LintRuleStatus;
using verible::LintViolation;
using verible::SyntaxTreeContext;
using verible::SyntaxTreeLeaf;
using verible::SyntaxTreeNode;
using verible::TokenInfo;
using verible::matcher::Matcher;

// Register CreateObjectNameMatchRule
VERILOG_REGISTER_LINT_RULE(CreateObjectNameMatchRule);

const LintRuleDescriptor& CreateObjectNameMatchRule::GetDescriptor() {
  static const LintRuleDescriptor d{
      .name = "create-object-name-match",
      .topic = "uvm-naming",
      .desc =
          "Checks that the 'name' argument of `type_id::create()` "
          "matches the name of the variable to which it is assigned.",
  };
  return d;
}

// Matches against assignments to typename::type_id::create() calls.
//
// For example:
//   var_h = mytype::type_id::create("var_h", ...);
//
// Here, the LHS var_h will be bound to "lval" (only for simple references),
// the qualified function call (mytype::type_id::create) will be bound to
// "func", and the list of function call arguments will be bound to "args".
static const Matcher& CreateAssignmentMatcher() {
  // function-local static to avoid initialization-ordering problems
  static const Matcher matcher(NodekNetVariableAssignment(
      PathkLPValue(PathkReferenceCallBase().Bind("lval_ref")),
      RValueIsFunctionCall(FunctionCallIsQualified().Bind("func"),
                           FunctionCallArguments().Bind("args"))));
  return matcher;
}

// Returns true if the underyling unqualified identifiers matches `name`,
// and returns false if the necessary conditions are not met.
// TODO(fangism): This function will be useful to many other analyses.
// Make public and refactor.
static bool UnqualifiedIdEquals(const SyntaxTreeNode& node,
                                absl::string_view name) {
  if (node.MatchesTag(NodeEnum::kUnqualifiedId)) {
    if (!node.children().empty()) {
      // The one-and-only child is the SymbolIdentifier token
      const auto& leaf_ptr =
          down_cast<const SyntaxTreeLeaf*>(node.children().front().get());
      if (leaf_ptr != nullptr) {
        const TokenInfo& token = leaf_ptr->get();
        return token.token_enum() == SymbolIdentifier && token.text() == name;
      }
    }
  }
  return false;
}

// Returns true if the qualified call is in the form "<any>::type_id::create".
// TODO(fangism): Refactor into QualifiedCallEndsWith().
static bool QualifiedCallIsTypeIdCreate(
    const SyntaxTreeNode& qualified_id_node) {
  const auto& children = qualified_id_node.children();
  const size_t num_children = children.size();
  // Allow for more than 3 segments, in case of package qualification, e.g.
  // my_pkg::class_type::type_id::create.
  // 5: 3 segments + 2 separators (in alternation), e.g. A::B::C
  if (qualified_id_node.children().size() >= 5) {
    const auto* create_leaf_ptr =
        down_cast<const SyntaxTreeNode*>(children.back().get());
    const auto* type_id_leaf_ptr =
        down_cast<const SyntaxTreeNode*>(children[num_children - 3].get());
    if (create_leaf_ptr != nullptr && type_id_leaf_ptr != nullptr) {
      return UnqualifiedIdEquals(*create_leaf_ptr, "create") &&
             UnqualifiedIdEquals(*type_id_leaf_ptr, "type_id");
    }
  }
  return false;
}

// Returns string_view of `text` with outermost double-quotes removed.
// If `text` is not wrapped in quotes, return it as-is.
static absl::string_view StripOuterQuotes(absl::string_view text) {
  if (!text.empty() && text[0] == '\"') {
    return text.substr(1, text.length() - 2);
  }
  return text;
}

// Returns token information for a single string literal expression, or nullptr
// if the expression is not a string literal.
// `expr_node` should be a SyntaxTreeNode tagged as an expression.
static const TokenInfo* ExtractStringLiteralToken(
    const SyntaxTreeNode& expr_node) {
  if (!expr_node.MatchesTag(NodeEnum::kExpression)) return nullptr;

  // this check is limited to only checking string literal leaf tokens
  if (expr_node.children().front().get()->Kind() !=
      verible::SymbolKind::kLeaf) {
    return nullptr;
  }

  const auto* leaf_ptr =
      down_cast<const SyntaxTreeLeaf*>(expr_node.children().front().get());
  if (leaf_ptr != nullptr) {
    const TokenInfo& token = leaf_ptr->get();
    if (token.token_enum() == TK_StringLiteral) {
      return &token;
    }
  }
  return nullptr;
}

// Returns the first expression from an argument list, if it exists.
static const SyntaxTreeNode* GetFirstExpressionFromArgs(
    const SyntaxTreeNode& args_node) {
  if (!args_node.children().empty()) {
    const auto& first_arg = args_node.children().front();
    if (const auto* first_expr =
            down_cast<const SyntaxTreeNode*>(first_arg.get())) {
      return first_expr;
    }
  }
  return nullptr;
}

// Returns a diagnostic message for this lint violation.
static std::string FormatReason(absl::string_view decl_name,
                                absl::string_view name_text) {
  return absl::StrCat(
      "The \'name\' argument of type_id::create() must match the name of "
      "the variable to which it is assigned: ",
      decl_name, ", got: ", name_text, ". ");
}

void CreateObjectNameMatchRule::HandleSymbol(const verible::Symbol& symbol,
                                             const SyntaxTreeContext& context) {
  // Check for assignments that match the pattern.
  verible::matcher::BoundSymbolManager manager;
  if (!CreateAssignmentMatcher().Matches(symbol, &manager)) return;

  // Extract named bindings for matched nodes within this match.

  const auto* lval_ref = manager.GetAs<SyntaxTreeNode>("lval_ref");
  if (lval_ref == nullptr) return;
  const TokenInfo* lval_id = ReferenceIsSimpleIdentifier(*lval_ref);
  if (lval_id == nullptr) return;
  if (lval_id->token_enum() != SymbolIdentifier) return;

  const auto* call = manager.GetAs<SyntaxTreeNode>("func");
  const auto* args = manager.GetAs<SyntaxTreeNode>("args");
  if (call == nullptr) return;
  if (args == nullptr) return;
  if (!QualifiedCallIsTypeIdCreate(*call)) return;

  // The first argument is a string that must match the variable name, lval.
  if (const auto* expr = GetFirstExpressionFromArgs(*args)) {
    if (const TokenInfo* name_token = ExtractStringLiteralToken(*expr)) {
      if (StripOuterQuotes(name_token->text()) != lval_id->text()) {
        violations_.insert(LintViolation(
            *name_token, FormatReason(lval_id->text(), name_token->text())));
      }
    }
  }
}

LintRuleStatus CreateObjectNameMatchRule::Report() const {
  return LintRuleStatus(violations_, GetDescriptor());
}

}  // namespace analysis
}  // namespace verilog
