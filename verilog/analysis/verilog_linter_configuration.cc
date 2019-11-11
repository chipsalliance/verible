// Copyright 2017-2019 The Verible Authors.
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

#include "verilog/analysis/verilog_linter_configuration.h"

#include <algorithm>
#include <initializer_list>
#include <map>
#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "absl/strings/str_cat.h"
#include "absl/strings/str_join.h"
#include "absl/strings/str_split.h"
#include "absl/strings/string_view.h"
#include "common/analysis/line_lint_rule.h"
#include "common/analysis/syntax_tree_lint_rule.h"
#include "common/analysis/text_structure_lint_rule.h"
#include "common/analysis/token_stream_lint_rule.h"
#include "common/util/container_util.h"
#include "common/util/enum_flags.h"
#include "common/util/logging.h"
#include "verilog/analysis/default_rules.h"
#include "verilog/analysis/lint_rule_registry.h"

namespace verilog {

using verible::LineLintRule;
using verible::SyntaxTreeLintRule;
using verible::TextStructureLintRule;
using verible::TokenStreamLintRule;
using verible::container::FindOrNull;

template <typename List>
static const char* MatchesAnyItem(absl::string_view filename,
                                  const List& items) {
  for (const auto item : items) {
    if (filename.find(item) != absl::string_view::npos) {
      return item;
    }
  }
  return nullptr;
}

const char* ProjectPolicy::MatchesAnyPath(absl::string_view filename) const {
  return MatchesAnyItem(filename, path_substrings);
}

const char* ProjectPolicy::MatchesAnyExclusions(
    absl::string_view filename) const {
  return MatchesAnyItem(filename, path_exclusions);
}

bool ProjectPolicy::IsValid() const {
  for (const auto rule : disabled_rules) {
    if (!analysis::IsRegisteredLintRule(rule)) return false;
  }
  for (const auto rule : enabled_rules) {
    if (!analysis::IsRegisteredLintRule(rule)) return false;
  }
  return true;
}

std::string ProjectPolicy::ListPathGlobs() const {
  return absl::StrJoin(path_substrings.begin(), path_substrings.end(), " | ",
                       [](std::string* out, absl::string_view pattern) {
                         absl::StrAppend(out, "*", pattern, "*");
                       });
}

bool LinterConfiguration::RuleIsOn(const analysis::LintRuleId& rule) const {
  const auto* entry = FindOrNull(configuration_, rule);
  if (entry == nullptr) return false;
  return *entry == RuleSetting::kRuleOn;
}

void LinterConfiguration::UseRuleSet(const RuleSet& rules) {
  configuration_.clear();
  switch (rules) {
    case RuleSet::kAll: {
      for (const auto& rule : analysis::RegisteredTextStructureRulesNames()) {
        TurnOn(rule);
      }
      for (const auto& rule : analysis::RegisteredSyntaxTreeRulesNames()) {
        TurnOn(rule);
      }
      for (const auto& rule : analysis::RegisteredTokenStreamRulesNames()) {
        TurnOn(rule);
      }
      for (const auto& rule : analysis::RegisteredLineRulesNames()) {
        TurnOn(rule);
      }
      break;
    }
    case RuleSet::kNone:
      break;
    case RuleSet::kDefault:
      for (const auto& rule : analysis::kDefaultRuleSet) {
        TurnOn(rule);
      }
  }
}

void LinterConfiguration::UseRuleBundle(const RuleBundle& rule_bundle) {
  const auto name_set = analysis::GetAllRegisteredLintRuleNames();
  for (const auto& rule_pair : rule_bundle.rules) {
    // This needs to use the canonical registered key string_view, which has
    // guaranteed lifetime.
    configuration_[rule_pair.first] = rule_pair.second;
  }
}

void LinterConfiguration::UseProjectPolicy(const ProjectPolicy& policy,
                                           absl::string_view filename) {
  if (const char* matched_path = policy.MatchesAnyPath(filename)) {
    VLOG(1) << "File \"" << filename << "\" matches path \"" << matched_path
            << "\" from project policy [" << policy.name << "], applying it.";
    for (const auto rule : policy.disabled_rules) {
      VLOG(1) << "  disabling rule: " << rule;
      TurnOff(rule);
    }
    for (const auto rule : policy.enabled_rules) {
      VLOG(1) << "  enabling rule: " << rule;
      TurnOn(rule);
    }
  }
}

std::set<analysis::LintRuleId> LinterConfiguration::ActiveRuleIds() const {
  std::set<analysis::LintRuleId> result;
  for (const auto& rule_pair : configuration_) {
    if (rule_pair.second == RuleSetting::kRuleOn) {
      result.insert(rule_pair.first);
    }
  }
  return result;
}

// Iterates through all stored syntax tree rules that are turned on.
// Looks up each of those rules in registry_ in order to construct instances
// that are added to the returned vector.
std::vector<std::unique_ptr<SyntaxTreeLintRule>>
LinterConfiguration::CreateSyntaxTreeRules() const {
  std::vector<std::unique_ptr<SyntaxTreeLintRule>> rule_instances;
  for (const auto& rule_pair : configuration_) {
    if (rule_pair.second == RuleSetting::kRuleOn) {
      std::unique_ptr<SyntaxTreeLintRule> rule_ptr =
          analysis::CreateSyntaxTreeLintRule(rule_pair.first);
      if (rule_ptr != nullptr) {
        rule_instances.push_back(std::move(rule_ptr));
      }
    }
  }
  return rule_instances;
}

// Iterates through all stored token-stream rules that are turned on.
// Looks up each of those rules in registry_ in order to construct instances
// that are added to the returned vector.
std::vector<std::unique_ptr<TokenStreamLintRule>>
LinterConfiguration::CreateTokenStreamRules() const {
  std::vector<std::unique_ptr<TokenStreamLintRule>> rule_instances;
  for (const auto& rule_pair : configuration_) {
    if (rule_pair.second == RuleSetting::kRuleOn) {
      std::unique_ptr<TokenStreamLintRule> rule_ptr =
          analysis::CreateTokenStreamLintRule(rule_pair.first);
      if (rule_ptr != nullptr) {
        rule_instances.push_back(std::move(rule_ptr));
      }
    }
  }
  return rule_instances;
}

// Iterates through all stored line-based rules that are turned on.
// Looks up each of those rules in registry_ in order to construct instances
// that are added to the returned vector.
std::vector<std::unique_ptr<LineLintRule>>
LinterConfiguration::CreateLineRules() const {
  std::vector<std::unique_ptr<LineLintRule>> rule_instances;
  for (const auto& rule_pair : configuration_) {
    if (rule_pair.second == RuleSetting::kRuleOn) {
      std::unique_ptr<LineLintRule> rule_ptr =
          analysis::CreateLineLintRule(rule_pair.first);
      if (rule_ptr != nullptr) {
        rule_instances.push_back(std::move(rule_ptr));
      }
    }
  }
  return rule_instances;
}

bool LinterConfiguration::operator==(const LinterConfiguration& config) const {
  return ActiveRuleIds() == config.ActiveRuleIds();
}

std::ostream& operator<<(std::ostream& stream,
                         const LinterConfiguration& config) {
  const auto rules = config.ActiveRuleIds();
  return stream << "{ " << absl::StrJoin(rules.begin(), rules.end(), ", ")
                << " }";
}

// Iterates through all stored line-based rules that are turned on.
// Looks up each of those rules in registry_ in order to construct instances
// that are added to the returned vector.
std::vector<std::unique_ptr<TextStructureLintRule>>
LinterConfiguration::CreateTextStructureRules() const {
  std::vector<std::unique_ptr<TextStructureLintRule>> rule_instances;
  for (const auto& rule_pair : configuration_) {
    if (rule_pair.second == RuleSetting::kRuleOn) {
      std::unique_ptr<TextStructureLintRule> rule_ptr =
          analysis::CreateTextStructureLintRule(rule_pair.first);
      if (rule_ptr != nullptr) {
        rule_instances.push_back(std::move(rule_ptr));
      }
    }
  }
  return rule_instances;
}

static const std::initializer_list<std::pair<const absl::string_view, RuleSet>>
    kRuleSetEnumStringMap = {
        {"all", RuleSet::kAll},
        {"none", RuleSet::kNone},
        {"default", RuleSet::kDefault},
};

std::ostream& operator<<(std::ostream& stream, RuleSet rules) {
  static const auto* flag_map =
      verible::MakeEnumToStringMap(kRuleSetEnumStringMap);
  return stream << flag_map->find(rules)->second;
}

//
// Parse and unparse for ruleset (for commandlineflags)
//
std::string AbslUnparseFlag(const RuleSet& rules) {
  std::ostringstream stream;
  stream << rules;
  return stream.str();
}

bool AbslParseFlag(absl::string_view text, RuleSet* rules, std::string* error) {
  static const auto* flag_map =
      verible::MakeStringToEnumMap(kRuleSetEnumStringMap);
  return EnumMapParseFlag(*flag_map, text, rules, error);
}

// Parse and unparse for RuleBundle (for commandlineflags)
std::string AbslUnparseFlag(const RuleBundle& bundle) {
  std::vector<std::string> switches;
  for (const auto& rule : bundle.rules) {
    switches.push_back(absl::StrCat(
        // If rule is set off, prepend "-"
        rule.second == RuleSetting::kRuleOn ? "" : "-", rule.first));
  }
  // Concatenates all of rules into text.
  return absl::StrJoin(switches.rbegin(), switches.rend(), ",");
}

bool AbslParseFlag(absl::string_view text, RuleBundle* bundle,
                   std::string* error) {
  // Clear the vector to overwrite any existing value.
  bundle->rules.clear();

  for (absl::string_view part : absl::StrSplit(text, ',', absl::SkipEmpty())) {
    // Get the prefix so we can check for "-"
    // Note that part is guaranteed to be at least one character because
    // of absl::SkipEmpty()
    const char prefix = part[0];

    absl::string_view stripped_rule_text = part;
    RuleSetting setting = RuleSetting::kRuleOn;

    // Update stripped_rule_text and setting if prefix is "-"
    if (prefix == '-') {
      setting = RuleSetting::kRuleOff;
      if (part.size() >= 2)
        stripped_rule_text = part.substr(1, part.size() - 1);
      else
        stripped_rule_text = "";
    }

    const auto rule_name_set = analysis::GetAllRegisteredLintRuleNames();
    const auto rule_iter = rule_name_set.find(stripped_rule_text);

    // Check if text is a valid lint rule.
    if (rule_iter == rule_name_set.end()) {
      *error = absl::StrCat("invalid flag \"", stripped_rule_text, "\"");
      return false;
    } else {
      // Map keys must use canonical registered string_views for guaranteed
      // lifetime, not just any string-equivalent copy.
      bundle->rules[*rule_iter] = setting;
    }
  }

  return true;
}

}  // namespace verilog
