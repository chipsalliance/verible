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

// File for registering lint rules so that they can by dynamically turned on
// and off at runtime. The goal is to provide a single place to register
// lint rules.
//
// To register an implemented rule, call
// VERILOG_REGISTER_LINT_RULE in source file.
//
// This will have the following effects:
//    1. Allow that rule to be used in commandline flags that accept vectors
//       of LintRuleEnum's
//    2. Allow the rule to be used by any component that dynamically loads
//       rules from LintRuleRegistry
//
#ifndef VERIBLE_VERILOG_ANALYSIS_LINT_RULE_REGISTRY_H_
#define VERIBLE_VERILOG_ANALYSIS_LINT_RULE_REGISTRY_H_

#include <functional>
#include <map>
#include <memory>
#include <set>
#include <string>
#include <vector>

#include "absl/strings/string_view.h"
#include "common/analysis/line_lint_rule.h"
#include "common/analysis/syntax_tree_lint_rule.h"
#include "common/analysis/text_structure_lint_rule.h"
#include "common/analysis/token_stream_lint_rule.h"
#include "common/strings/compare.h"
#include "verilog/analysis/descriptions.h"

namespace verilog {
namespace analysis {

template <typename RuleType>
class LintRuleRegisterer;

using LintRuleId = absl::string_view;

template <typename RuleType>
using LintRuleGenerator = std::function<std::unique_ptr<RuleType>()>;
using LintDescription = std::function<std::string(DescriptionType)>;

template <typename RuleType>
struct LintRuleInfo {
  LintRuleGenerator<RuleType> lint_rule_generator;
  LintDescription description;
};

struct LintRuleDescriptionInfo {
  std::string description;
  bool default_enabled = false;
};

using LintRuleDescriptionsMap =
    std::map<LintRuleId, LintRuleDescriptionInfo, verible::StringViewCompare>;

// Helper macro to register a LintRule with LintRuleRegistry. In order to have
// a global registry, some static initialization is needed. This macros
// centralizes this unsafe code into one place in order to prevent mistakes.
//
// Args:
//   class_name: the name of the class to register
//
// Usage:
// (in my_lint_rule.h):
//  class MyLintRule : LintRuleType {
//   public:
//    using rule_type = LintRuleType;
//    static absl::string_view Name();
//    static std::string GetDescription(DescriptionType);
//    ... implement internals ...
//  }
//
// (in my_lint_rule.cc):
// VERILOG_REGISTER_LINT_RULE(MyLintRule);
//
// Name() needs to be a function and not a constant to guarantee proper
// string initialization (from first invocation during static global
// initialization, when registration happens), and must be backed by string
// memory with guaranteed lifetime.  e.g.
//
// absl::string_view MyLintRule::Name() {
//   return "my-lint-rule";  // safely initialized function-local string literal
// }
//
#define VERILOG_REGISTER_LINT_RULE(class_name)                               \
  static verilog::analysis::LintRuleRegisterer<class_name::rule_type>        \
      __##class_name##__registerer(                                          \
          class_name::Name(),                                                \
          []() {                                                             \
            return std::unique_ptr<class_name::rule_type>(new class_name()); \
          },                                                                 \
          class_name::GetDescription);

// Static objects of type LintRuleRegisterer are used to register concrete
// parsers in LintRuleRegistry. Users are expected to create these objects
// using the VERILOG_REGISTER_LINT_RULE macro.
template <typename RuleType>
class LintRuleRegisterer {
 public:
  LintRuleRegisterer(const LintRuleId& rule,
                     const LintRuleGenerator<RuleType>& creator,
                     const LintDescription& descriptor);
};

// Returns true if rule_name refers to a known lint rule.
bool IsRegisteredLintRule(const LintRuleId& rule_name);

// Returns sequence of syntax tree rule names.
std::vector<LintRuleId> RegisteredSyntaxTreeRulesNames();

// Returns a syntax tree lint rule object corresponding the rule_name.
std::unique_ptr<verible::SyntaxTreeLintRule> CreateSyntaxTreeLintRule(
    const LintRuleId& rule_name);

// Returns sequence of token stream rule names.
std::vector<LintRuleId> RegisteredTokenStreamRulesNames();

// Returns a token stream lint rule object corresponding the rule_name.
std::unique_ptr<verible::TokenStreamLintRule> CreateTokenStreamLintRule(
    const LintRuleId& rule_name);

// Returns sequence of line rule names.
std::vector<LintRuleId> RegisteredLineRulesNames();

// Returns a token stream lint rule object corresponding the rule_name.
std::unique_ptr<verible::LineLintRule> CreateLineLintRule(
    const LintRuleId& rule_name);

// Returns sequence of text structure rule names.
std::vector<LintRuleId> RegisteredTextStructureRulesNames();

// Returns a token stream lint rule object corresponding the rule_name.
std::unique_ptr<verible::TextStructureLintRule> CreateTextStructureLintRule(
    const LintRuleId& rule_name);

// Returns set of all registered lint rule names.
// When storing string_views to the lint rule keys, use the ones returned in
// this set, because their lifetime is guaranteed by the registration process.
std::set<LintRuleId> GetAllRegisteredLintRuleNames();

// Returns a map mapping each rule to a struct of information about the rule to
// print.
LintRuleDescriptionsMap GetAllRuleDescriptionsHelpFlag();

// Returns a map mapping each rule to a struct of information about the rule in
// order to create the markdown describing the lint rules.
LintRuleDescriptionsMap GetAllRuleDescriptionsMarkdown();

}  // namespace analysis
}  // namespace verilog

#endif  // VERIBLE_VERILOG_ANALYSIS_LINT_RULE_REGISTRY_H_
