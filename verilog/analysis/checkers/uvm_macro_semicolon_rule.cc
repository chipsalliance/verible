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

#include "verilog/analysis/checkers/uvm_macro_semicolon_rule.h"

#include <string>

#include "absl/strings/str_cat.h"
#include "absl/strings/match.h"
#include "common/analysis/citation.h"
#include "common/analysis/lint_rule_status.h"
#include "common/text/concrete_syntax_leaf.h"
#include "common/text/symbol.h"
#include "common/text/syntax_tree_context.h"
#include "common/text/token_info.h"
#include "common/util/container_util.h"
#include "verilog/analysis/descriptions.h"
#include "verilog/analysis/lint_rule_registry.h"
#include "verilog/CST/macro.h"

namespace verilog {
namespace analysis {

using verible::GetVerificationCitation;
using verible::LintViolation;

// Register UvmMacroSemicolonRule
VERILOG_REGISTER_LINT_RULE(UvmMacroSemicolonRule);

absl::string_view UvmMacroSemicolonRule::Name() { return "uvm-macro-semicolon"; }
const char UvmMacroSemicolonRule::kTopic[] = "uvm-macro-semicolon-convention";

const char UvmMacroSemicolonRule::kMessage[] = "UVM macro calls should NOT end with ';'";

std::string UvmMacroSemicolonRule::GetDescription(
    DescriptionType description_type) {
  return absl::StrCat("Checks that no `uvm_* macro calls end with ';'. See ",
                      GetVerificationCitation(kTopic), ".");
}

static bool IsMacroCall(const verible::Symbol& symbol) {
  if (symbol.Kind() == verible::SymbolKind::kNode) {
    const auto& node = SymbolCastToNode(symbol);
    return node.MatchesTag(NodeEnum::kMacroCall);
  }
  return false;
}

static bool IsMacroCallClose(const verible::SymbolPtr & symbol) {
  if (symbol->Kind() == verible::SymbolKind::kLeaf) {
    const auto& leaf = SymbolCastToLeaf(*symbol.get());
    return leaf.get().token_enum == verilog_tokentype::MacroCallCloseToEndLine;
  }
  return false;
}


void UvmMacroSemicolonRule::HandleSymbol(
    const verible::Symbol& symbol, const verible::SyntaxTreeContext& context) {
  
  if(IsMacroCall(symbol)) {
    const auto& macro_call_id = GetMacroCallId(symbol);

    if(absl::StartsWithIgnoreCase(std::string(macro_call_id.text),"`uvm_")) {
      auto &paren_group = GetMacroCallParenGroup(symbol);      
      auto parameters = std::count_if(paren_group.children().begin(), paren_group.children().end(), 
                        IsMacroCallClose);
      if(!parameters) {
        violations_.insert( LintViolation(symbol, kMessage, context) );
      }
    }
  }
}


verible::LintRuleStatus UvmMacroSemicolonRule::Report() const {
  return verible::LintRuleStatus(violations_, Name(),
                                 GetVerificationCitation(kTopic));
}


}  // namespace analysis
}  // namespace verilog
