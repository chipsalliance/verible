
#include "verible/verilog/analysis/checkers/forbidden-macro-rule.h"

#include <set>
#include <string>
#include <string_view>

#include "absl/status/status.h"
#include "absl/strings/str_split.h"
#include "verible/common/analysis/lint-rule-status.h"
#include "verible/common/analysis/matcher/bound-symbol-manager.h"
#include "verible/common/analysis/matcher/matcher.h"
#include "verible/common/text/config-utils.h"
#include "verible/common/text/concrete-syntax-leaf.h"
#include "verible/common/text/symbol.h"
#include "verible/common/text/syntax-tree-context.h"
#include "verible/common/text/token-info.h"
#include "verible/verilog/CST/verilog-matchers.h"
#include "verible/verilog/analysis/descriptions.h"
#include "verible/verilog/analysis/lint-rule-registry.h"

namespace verilog {
namespace analysis {

using verible::matcher::Matcher;

VERILOG_REGISTER_LINT_RULE(ForbiddenMacroRule);

const LintRuleDescriptor &ForbiddenMacroRule::GetDescriptor() {
  static const LintRuleDescriptor d{
      .name = "forbidden-macro",
      .topic = "uvm-logging",
      .desc = "Checks that configured forbidden macro calls are not used. "
              "By default, the `uvm_warning macro is forbidden.",
      .param = {{"forbidden_macros", "`uvm_warning",
                 "Comma-separated list of forbidden macro names"}},
  };
  return d;
}

static const Matcher &MacroCallMatcher() {
  static const Matcher matcher(MacroCallIdLeaf().Bind("name"));
  return matcher;
}

absl::Status ForbiddenMacroRule::Configure(
    std::string_view configuration) {
  using verible::config::SetString;

  std::string raw_macros = "`uvm_warning";
  const auto status = verible::ParseNameValues(
      configuration, {{"forbidden_macros", SetString(&raw_macros)}});
  if (!status.ok()) {
    return status;
  }

  forbidden_macros_.clear();
  for (const auto &macro : absl::StrSplit(raw_macros, ',')) {
    if (!macro.empty()) {
      forbidden_macros_.insert(std::string(macro));
    }
  }

  return absl::OkStatus();
}

void ForbiddenMacroRule::HandleSymbol(
    const verible::Symbol &symbol, const verible::SyntaxTreeContext &context) {
  verible::matcher::BoundSymbolManager manager;
  if (MacroCallMatcher().Matches(symbol, &manager)) {
    if (const verible::SyntaxTreeLeaf *leaf = manager.GetAsLeaf("name")) {
      if (forbidden_macros_.contains(std::string(leaf->get().text()))) {
        violations_.insert(
            verible::LintViolation(leaf->get(), FormatReason(*leaf), context));
      }
    }
  }
}

verible::LintRuleStatus ForbiddenMacroRule::Report() const {
  return verible::LintRuleStatus(violations_, GetDescriptor());
}

std::string ForbiddenMacroRule::FormatReason(
    const verible::SyntaxTreeLeaf &leaf) {
  const std::string function_name(leaf.get().text());
  return function_name + " is a forbidden macro.";
}

}  // namespace analysis
}  // namespace verilog
