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
#include <vector>

#include "verible/common/analysis/line-lint-rule.h"
#include "verible/common/analysis/syntax-tree-lint-rule.h"
#include "verible/common/analysis/text-structure-lint-rule.h"
#include "verible/common/analysis/token-stream-lint-rule.h"
#include "verible/common/strings/compare.h"
#include "verible/verilog/analysis/descriptions.h"

namespace verilog {
namespace analysis {

template <typename RuleType>
class LintRuleRegisterer;

template <typename RuleType>
using LintRuleGeneratorFun = std::function<std::unique_ptr<RuleType>()>;
using LintDescriptionFun = std::function<LintRuleDescriptor()>;

template <typename RuleType>
struct LintRuleInfo {
  LintRuleGeneratorFun<RuleType> lint_rule_generator;
  LintDescriptionFun description;
};

struct LintRuleDefaultConfig {
  LintRuleDescriptor descriptor;
  bool default_enabled = false;
};

using LintRuleDescriptionsMap =
    std::map<LintRuleId, LintRuleDefaultConfig, verible::StringViewCompare>;

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
//    static std::string_view Name();
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
// std::string_view MyLintRule::Name() {
//   return "my-lint-rule";  // safely initialized function-local string literal
// }
//
// TODO(hzeller): once the class does not contain a state, extract the name
// and description from the instance to avoid weird static initialization.
#define VERILOG_REGISTER_LINT_RULE(class_name)                           \
  static verilog::analysis::LintRuleRegisterer<class_name::rule_type>    \
      __##class_name##__registerer(class_name::GetDescriptor, []() {     \
        return std::unique_ptr<class_name::rule_type>(new class_name()); \
      });

// Static objects of type LintRuleRegisterer are used to register concrete
// parsers in LintRuleRegistry. Users are expected to create these objects
// using the VERILOG_REGISTER_LINT_RULE macro.
template <typename RuleType>
class LintRuleRegisterer {
 public:
  LintRuleRegisterer(const LintDescriptionFun &descriptor,
                     const LintRuleGeneratorFun<RuleType> &creator);
};

// Returns true if rule_name refers to a known lint rule.
bool IsRegisteredLintRule(const LintRuleId &rule_name);

// Returns sequence of syntax tree rule names.
std::vector<LintRuleId> RegisteredSyntaxTreeRulesNames();

// Returns a syntax tree lint rule object corresponding the rule_name.
std::unique_ptr<verible::SyntaxTreeLintRule> CreateSyntaxTreeLintRule(
    const LintRuleId &rule_name);

// Returns sequence of token stream rule names.
std::vector<LintRuleId> RegisteredTokenStreamRulesNames();

// Returns a token stream lint rule object corresponding the rule_name.
std::unique_ptr<verible::TokenStreamLintRule> CreateTokenStreamLintRule(
    const LintRuleId &rule_name);

// Returns sequence of line rule names.
std::vector<LintRuleId> RegisteredLineRulesNames();

// Returns a token stream lint rule object corresponding the rule_name.
std::unique_ptr<verible::LineLintRule> CreateLineLintRule(
    const LintRuleId &rule_name);

// Returns sequence of text structure rule names.
std::vector<LintRuleId> RegisteredTextStructureRulesNames();

// Returns a token stream lint rule object corresponding the rule_name.
std::unique_ptr<verible::TextStructureLintRule> CreateTextStructureLintRule(
    const LintRuleId &rule_name);

// Returns set of all registered lint rule names.
// When storing string_views to the lint rule keys, use the ones returned in
// this set, because their lifetime is guaranteed by the registration process.
std::set<LintRuleId> GetAllRegisteredLintRuleNames();

// Returns a map mapping each rule to a struct of information about the rule to
// print.
LintRuleDescriptionsMap GetAllRuleDescriptions();

}  // namespace analysis
}  // namespace verilog

#endif  // VERIBLE_VERILOG_ANALYSIS_LINT_RULE_REGISTRY_H_
