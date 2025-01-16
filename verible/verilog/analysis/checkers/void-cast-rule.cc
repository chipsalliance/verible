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

#include "verible/verilog/analysis/checkers/void-cast-rule.h"

#include <set>
#include <string>

#include "verible/common/analysis/lint-rule-status.h"
#include "verible/common/analysis/matcher/bound-symbol-manager.h"
#include "verible/common/analysis/matcher/core-matchers.h"
#include "verible/common/analysis/matcher/matcher.h"
#include "verible/common/text/concrete-syntax-leaf.h"
#include "verible/common/text/concrete-syntax-tree.h"
#include "verible/common/text/symbol.h"
#include "verible/common/text/syntax-tree-context.h"
#include "verible/common/text/token-info.h"
#include "verible/common/text/tree-utils.h"
#include "verible/common/util/logging.h"
#include "verible/verilog/CST/verilog-matchers.h"
#include "verible/verilog/analysis/descriptions.h"
#include "verible/verilog/analysis/lint-rule-registry.h"

namespace verilog {
namespace analysis {

using verible::LintRuleStatus;
using verible::LintViolation;
using verible::SyntaxTreeContext;
using verible::SyntaxTreeLeaf;
using verible::SyntaxTreeNode;
using verible::matcher::Matcher;

// Register VoidCastRule
VERILOG_REGISTER_LINT_RULE(VoidCastRule);

const LintRuleDescriptor &VoidCastRule::GetDescriptor() {
  static const LintRuleDescriptor d{
      .name = "void-cast",
      .topic = "void-casts",
      .desc =
          "Checks that void casts do not contain certain function/method "
          "calls. ",
  };
  return d;
}

const std::set<std::string> &VoidCastRule::ForbiddenFunctionsSet() {
  static const auto *forbidden_functions =
      new std::set<std::string>({"uvm_hdl_read"});
  return *forbidden_functions;
}

// Matches against top level function calls within void casts
// For example:
//   void'(foo());
// Here, the leaf representing "foo" will be bound to id
static const Matcher &FunctionMatcher() {
  static const Matcher matcher(
      NodekVoidcast(VoidcastHasExpression(verible::matcher::EachOf(
          ExpressionHasFunctionCall(),
          ExpressionHasReference(FunctionCallHasId().Bind("id"))))));
  return matcher;
}

// Matches against both calls to randomize and randomize methods within
// voidcasts.
// For example:
//   void'(obj.randomize(...));
// Here, the node representing "randomize(...)" will be bound to "id"
//
// For example:
//   void'(randomize(obj));
// Here, the node representing "randomize(obj)" will be bound to "id"
//
static const Matcher &RandomizeMatcher() {
  static const Matcher matcher(NodekVoidcast(VoidcastHasExpression(
      verible::matcher::AnyOf(NonCallHasRandomizeCallExtension().Bind("id"),
                              CallHasRandomizeCallExtension().Bind("id"),
                              ExpressionHasRandomizeCallExtension().Bind("id"),
                              ExpressionHasRandomizeFunction().Bind("id")))));
  return matcher;
}

void VoidCastRule::HandleSymbol(const verible::Symbol &symbol,
                                const SyntaxTreeContext &context) {
  // Check for forbidden function names
  verible::matcher::BoundSymbolManager manager;
  if (FunctionMatcher().Matches(symbol, &manager)) {
    if (const auto *function_id =
            manager.GetAs<verible::SyntaxTreeLeaf>("id")) {
      const auto &bfs = ForbiddenFunctionsSet();
      if (bfs.find(std::string(function_id->get().text())) != bfs.end()) {
        violations_.insert(LintViolation(function_id->get(),
                                         FormatReason(*function_id), context));
      }
    }
  }

  // Check for forbidden calls to randomize
  manager.Clear();
  if (RandomizeMatcher().Matches(symbol, &manager)) {
    if (const auto *randomize_node =
            manager.GetAs<verible::SyntaxTreeNode>("id")) {
      const auto *leaf_ptr = verible::GetLeftmostLeaf(*randomize_node);
      const verible::TokenInfo token = ABSL_DIE_IF_NULL(leaf_ptr)->get();
      violations_.insert(LintViolation(
          token, "randomize() is forbidden within void casts", context));
    }
  }
}

LintRuleStatus VoidCastRule::Report() const {
  return LintRuleStatus(violations_, GetDescriptor());
}

/* static */ std::string VoidCastRule::FormatReason(
    const verible::SyntaxTreeLeaf &leaf) {
  return std::string(leaf.get().text()) +
         " is an invalid call within this void cast";
}

}  // namespace analysis
}  // namespace verilog
