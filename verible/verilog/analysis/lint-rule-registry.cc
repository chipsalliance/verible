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

#include "verible/verilog/analysis/lint-rule-registry.h"

#include <memory>
#include <set>
#include <vector>

#include "absl/container/node_hash_map.h"
#include "verible/common/analysis/line-lint-rule.h"
#include "verible/common/analysis/syntax-tree-lint-rule.h"
#include "verible/common/analysis/text-structure-lint-rule.h"
#include "verible/common/analysis/token-stream-lint-rule.h"
#include "verible/common/util/container-util.h"
#include "verible/common/util/logging.h"
#include "verible/verilog/analysis/descriptions.h"

namespace verilog {
namespace analysis {

using verible::LineLintRule;
using verible::SyntaxTreeLintRule;
using verible::TextStructureLintRule;
using verible::TokenStreamLintRule;
using verible::container::FindOrNull;

namespace {
// Used to export function local static pointer to avoid global variables
template <typename RuleType>
absl::node_hash_map<LintRuleId, LintRuleInfo<RuleType>> *GetLintRuleRegistry() {
  static auto *registry =
      new absl::node_hash_map<LintRuleId, LintRuleInfo<RuleType>>();
  return registry;
}

// LintRuleRegistry is a template for interacting with lint rule registries.
// It is expected to be implicitly instantiated for every LintRule type.
template <typename RuleType>
class LintRuleRegistry {
 public:
  // Create and returns an instance of RuleType identified by rule.
  // Returns nullptr if rule is not registered.
  static std::unique_ptr<RuleType> CreateLintRule(const LintRuleId &rule) {
    auto *create_func = FindOrNull(*GetLintRuleRegistry<RuleType>(), rule);
    return create_func ? (create_func->lint_rule_generator)() : nullptr;
  }

  // Returns true if registry holds a LintRule named rule.
  static bool ContainsLintRule(const LintRuleId &rule) {
    const auto reg = GetLintRuleRegistry<RuleType>();
    return reg->find(rule) != reg->end();
  }

  // Returns a sequence of registered rule names.
  static std::vector<LintRuleId> GetRegisteredRulesNames() {
    const auto *registry = GetLintRuleRegistry<RuleType>();
    std::vector<LintRuleId> rule_ids;
    rule_ids.reserve(registry->size());
    for (const auto &rule_bundle : *registry) {
      rule_ids.push_back(rule_bundle.first);
    }
    return rule_ids;
  }

  // Registers a lint rule with the appropriate registry.
  static void Register(const LintDescriptionFun &descriptor,
                       const LintRuleGeneratorFun<RuleType> &creator) {
    LintRuleInfo<RuleType> info;
    info.lint_rule_generator = creator;
    info.description = descriptor;
    (*GetLintRuleRegistry<RuleType>())[descriptor().name] = info;
  }

  // Returns the description of the specific rule, formatted for description
  // type passed in.
  static LintRuleDescriptor GetRuleDescription(const LintRuleId &rule) {
    auto *create_func = FindOrNull(*GetLintRuleRegistry<RuleType>(), rule);
    return (ABSL_DIE_IF_NULL(create_func)->description)();
  }

  // Adds each rule name and a struct of information describing the rule to the
  // map passed in.
  static void GetRegisteredRuleDescriptions(LintRuleDescriptionsMap *rule_map) {
    const auto *registry = GetLintRuleRegistry<RuleType>();
    for (const auto &rule_bundle : *registry) {
      (*rule_map)[rule_bundle.first].descriptor =
          GetRuleDescription(rule_bundle.first);
    }
  }

  LintRuleRegistry() = delete;
  LintRuleRegistry(const LintRuleRegistry &) = delete;
  LintRuleRegistry &operator=(const LintRuleRegistry &) = delete;
};

}  // namespace

template <typename RuleType>
LintRuleRegisterer<RuleType>::LintRuleRegisterer(
    const LintDescriptionFun &descriptor,
    const LintRuleGeneratorFun<RuleType> &creator) {
  LintRuleRegistry<RuleType>::Register(descriptor, creator);
}

bool IsRegisteredLintRule(const LintRuleId &rule_name) {
  return LintRuleRegistry<SyntaxTreeLintRule>::ContainsLintRule(rule_name) ||
         LintRuleRegistry<TokenStreamLintRule>::ContainsLintRule(rule_name) ||
         LintRuleRegistry<LineLintRule>::ContainsLintRule(rule_name) ||
         LintRuleRegistry<TextStructureLintRule>::ContainsLintRule(rule_name);
}

// The following functions are LintRule-type-specific:

std::vector<LintRuleId> RegisteredSyntaxTreeRulesNames() {
  return LintRuleRegistry<SyntaxTreeLintRule>::GetRegisteredRulesNames();
}

std::unique_ptr<SyntaxTreeLintRule> CreateSyntaxTreeLintRule(
    const LintRuleId &rule_name) {
  return LintRuleRegistry<SyntaxTreeLintRule>::CreateLintRule(rule_name);
}

std::vector<LintRuleId> RegisteredTokenStreamRulesNames() {
  return LintRuleRegistry<TokenStreamLintRule>::GetRegisteredRulesNames();
}

std::unique_ptr<TokenStreamLintRule> CreateTokenStreamLintRule(
    const LintRuleId &rule_name) {
  return LintRuleRegistry<TokenStreamLintRule>::CreateLintRule(rule_name);
}

std::vector<LintRuleId> RegisteredLineRulesNames() {
  return LintRuleRegistry<LineLintRule>::GetRegisteredRulesNames();
}

std::unique_ptr<LineLintRule> CreateLineLintRule(const LintRuleId &rule_name) {
  return LintRuleRegistry<LineLintRule>::CreateLintRule(rule_name);
}

std::vector<LintRuleId> RegisteredTextStructureRulesNames() {
  return LintRuleRegistry<TextStructureLintRule>::GetRegisteredRulesNames();
}

std::unique_ptr<TextStructureLintRule> CreateTextStructureLintRule(
    const LintRuleId &rule_name) {
  return LintRuleRegistry<TextStructureLintRule>::CreateLintRule(rule_name);
}

std::set<LintRuleId> GetAllRegisteredLintRuleNames() {
  std::set<LintRuleId> result;
  for (const auto name : RegisteredSyntaxTreeRulesNames()) {
    result.insert(name);
  }
  for (const auto name : RegisteredTokenStreamRulesNames()) {
    result.insert(name);
  }
  for (const auto name : RegisteredLineRulesNames()) {
    result.insert(name);
  }
  for (const auto name : RegisteredTextStructureRulesNames()) {
    result.insert(name);
  }
  return result;
}

LintRuleDescriptionsMap GetAllRuleDescriptions() {
  // Map that will hold the information to print about each rule.
  LintRuleDescriptionsMap res;
  LintRuleRegistry<SyntaxTreeLintRule>::GetRegisteredRuleDescriptions(&res);
  LintRuleRegistry<TokenStreamLintRule>::GetRegisteredRuleDescriptions(&res);
  LintRuleRegistry<LineLintRule>::GetRegisteredRuleDescriptions(&res);
  LintRuleRegistry<TextStructureLintRule>::GetRegisteredRuleDescriptions(&res);
  return res;
}

// Explicit template class instantiations
template class LintRuleRegisterer<LineLintRule>;
template class LintRuleRegisterer<SyntaxTreeLintRule>;
template class LintRuleRegisterer<TextStructureLintRule>;
template class LintRuleRegisterer<TokenStreamLintRule>;

}  // namespace analysis
}  // namespace verilog
