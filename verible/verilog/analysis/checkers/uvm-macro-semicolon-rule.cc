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

#include "verible/verilog/analysis/checkers/uvm-macro-semicolon-rule.h"

#include <string>
#include <string_view>

#include "absl/strings/match.h"
#include "absl/strings/str_cat.h"
#include "verible/common/analysis/lint-rule-status.h"
#include "verible/common/text/concrete-syntax-leaf.h"
#include "verible/common/text/symbol.h"
#include "verible/common/text/syntax-tree-context.h"
#include "verible/common/text/token-info.h"
#include "verible/verilog/CST/context-functions.h"
#include "verible/verilog/CST/macro.h"
#include "verible/verilog/CST/verilog-nonterminals.h"
#include "verible/verilog/analysis/descriptions.h"
#include "verible/verilog/analysis/lint-rule-registry.h"
#include "verible/verilog/parser/verilog-token-enum.h"

namespace verilog {
namespace analysis {

using verible::AutoFix;
using verible::LintViolation;

// Register UvmMacroSemicolonRule
VERILOG_REGISTER_LINT_RULE(UvmMacroSemicolonRule);

const LintRuleDescriptor &UvmMacroSemicolonRule::GetDescriptor() {
  static const LintRuleDescriptor d{
      .name = "uvm-macro-semicolon",
      .topic = "uvm-macro-semicolon-convention",  // TODO(b/155128436): verify
                                                  // style guide anchor name
      .desc = "Checks that no `uvm_* macro calls end with ';'.",
  };
  return d;
}

// Returns a diagnostic message for this lint violation.
static std::string FormatReason(const verible::TokenInfo &macro_id) {
  return absl::StrCat("UVM macro call, ", macro_id.text(),
                      " should not be followed by a semicolon \';\'.");
}

// Returns true if leaf is a macro and matches `uvm_
static bool IsUvmMacroId(const verible::SyntaxTreeLeaf &leaf) {
  const std::string_view text = leaf.get().text();
  const bool starts_with_uvm = absl::StartsWithIgnoreCase(text, "`uvm_");

  if (leaf.Tag().tag == verilog_tokentype::MacroCallId ||
      leaf.Tag().tag == verilog_tokentype::MacroIdItem) {
    return starts_with_uvm;
  }

  const bool ends_with_end = absl::EndsWithIgnoreCase(text, "_end");
  // We don't want to complain about macros like:
  // `UVM_DEFAULT_TIMEOUT, UVM_MAX_STREAMBITS, ...
  if (leaf.Tag().tag == verilog_tokentype::MacroIdentifier) {
    return starts_with_uvm && ends_with_end;
  }

  return false;
}

void UvmMacroSemicolonRule::HandleLeaf(
    const verible::SyntaxTreeLeaf &leaf,
    const verible::SyntaxTreeContext &context) {
  if (ContextIsInsideStatement(context) ||
      context.IsInside(NodeEnum::kMacroCall) ||
      context.IsInside(NodeEnum::kDataDeclaration)) {
    switch (state_) {
      case State::kNormal: {
        if (IsUvmMacroId(leaf)) {
          macro_id_ = leaf.get();
          state_ = State::kCheckMacro;
        }
        break;
      }

      case State::kCheckMacro: {
        if (leaf.Tag().tag == ';') {
          violations_.insert(
              LintViolation(leaf, FormatReason(macro_id_), context,
                            {AutoFix("Remove semicolon at end of macro call",
                                     {leaf.get(), ""})}));
          state_ = State::kNormal;
        } else if (leaf.Tag().tag ==
                   verilog_tokentype::MacroCallCloseToEndLine) {
          state_ = State::kNormal;
        }
        break;
      }
    }
  } else {
    state_ = State::kNormal;
    macro_id_ = verible::TokenInfo::EOFToken();
    return;
  }
}

verible::LintRuleStatus UvmMacroSemicolonRule::Report() const {
  return verible::LintRuleStatus(violations_, GetDescriptor());
}

}  // namespace analysis
}  // namespace verilog
