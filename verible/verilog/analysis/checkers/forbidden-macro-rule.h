
#ifndef VERIBLE_VERILOG_ANALYSIS_CHECKERS_FORBIDDEN_MACRO_RULE_H_
#define VERIBLE_VERILOG_ANALYSIS_CHECKERS_FORBIDDEN_MACRO_RULE_H_

#include <set>
#include <string>
#include <string_view>

#include "absl/status/status.h"
#include "verible/common/analysis/lint-rule-status.h"
#include "verible/common/analysis/syntax-tree-lint-rule.h"
#include "verible/common/text/concrete-syntax-leaf.h"
#include "verible/common/text/symbol.h"
#include "verible/common/text/syntax-tree-context.h"
#include "verible/verilog/analysis/descriptions.h"

namespace verilog {
namespace analysis {

class ForbiddenMacroRule : public verible::SyntaxTreeLintRule {
 public:
  using rule_type = verible::SyntaxTreeLintRule;

  static const LintRuleDescriptor &GetDescriptor();

  absl::Status Configure(std::string_view configuration) final;

  void HandleSymbol(const verible::Symbol &symbol,
                    const verible::SyntaxTreeContext &context) final;

  verible::LintRuleStatus Report() const final;

 private:
  static std::string FormatReason(const verible::SyntaxTreeLeaf &leaf);

  std::set<std::string> forbidden_macros_;
  std::set<verible::LintViolation> violations_;
};

}  // namespace analysis
}  // namespace verilog

#endif  // VERIBLE_VERILOG_ANALYSIS_CHECKERS_FORBIDDEN_MACRO_RULE_H_
