// Copyright 2017-2023 The Verible Authors.
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
#include <functional>
#include <iostream>
#include <map>
#include <memory>
#include <set>
#include <sstream>
#include <string>
#include <vector>

#include "absl/status/status.h"
#include "absl/status/statusor.h"
#include "absl/strings/ascii.h"
#include "absl/strings/match.h"
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
#include "common/util/file_util.h"
#include "common/util/logging.h"
#include "common/util/status_macros.h"
#include "verilog/analysis/default_rules.h"
#include "verilog/analysis/descriptions.h"
#include "verilog/analysis/lint_rule_registry.h"

namespace verilog {

using verible::LineLintRule;
using verible::SyntaxTreeLintRule;
using verible::TextStructureLintRule;
using verible::TokenStreamLintRule;
using verible::container::FindOrNull;

template <typename List>
static const char *MatchesAnyItem(absl::string_view filename,
                                  const List &items) {
  for (const auto item : items) {
    if (absl::StrContains(filename, item)) {
      return item;
    }
  }
  return nullptr;
}

const char *ProjectPolicy::MatchesAnyPath(absl::string_view filename) const {
  return MatchesAnyItem(filename, path_substrings);
}

const char *ProjectPolicy::MatchesAnyExclusions(
    absl::string_view filename) const {
  return MatchesAnyItem(filename, path_exclusions);
}

bool ProjectPolicy::IsValid() const {
  for (const auto &rule : disabled_rules) {
    if (!analysis::IsRegisteredLintRule(rule)) return false;
  }
  for (const auto &rule : enabled_rules) {
    if (!analysis::IsRegisteredLintRule(rule)) return false;
  }
  return true;
}

std::string ProjectPolicy::ListPathGlobs() const {
  return absl::StrJoin(path_substrings.begin(), path_substrings.end(), " | ",
                       [](std::string *out, absl::string_view pattern) {
                         absl::StrAppend(out, "*", pattern, "*");
                       });
}

bool RuleBundle::ParseConfiguration(absl::string_view text, char separator,
                                    std::string *error) {
  // Clear the vector to overwrite any existing value.
  rules.clear();

  bool parsed_correctly = true;
  for (absl::string_view part :
       absl::StrSplit(text, separator, absl::SkipEmpty())) {
    if (separator == '\n') {
      // In configuration files, we can ignore #-comments
      // TODO(hzeller): this will fall short if in the configuration string
      // we expect rules that accept a #-character for some reason. In that
      // case we need to expand this to parse out 'within # quotes' parts.
      // ... then this will finally become a more complex lexer.
      const auto comment_pos = part.find('#');
      if (comment_pos != absl::string_view::npos) {
        part = part.substr(0, comment_pos);
      }
    }
    part = absl::StripAsciiWhitespace(part);
    while (!part.empty() && part[part.size() - 1] == ',') {
      // Not fatal, just report
      absl::StrAppend(error, error->empty() ? "" : "\n", kStrayCommaWarning,
                      " `", part, "`");
      part = part.substr(0, part.size() - 1);
    }

    if (part.empty()) continue;
    // If prefix is '-', the rule is disabled. For symmetry, we also allow
    // '+' to enable rule.
    // Note that part is guaranteed to be at least one character because
    // of absl::SkipEmpty()
    const bool prefix_minus = (part[0] == '-');
    const bool has_prefix = (part[0] == '+' || prefix_minus);

    RuleSetting setting = {!prefix_minus, ""};

    const auto rule_name_with_config = part.substr(has_prefix ? 1 : 0);

    // Independent of the enabled-ness: extract a configuration string
    // if there is any assignment.
    const auto equals_pos = rule_name_with_config.find('=');
    if (equals_pos != absl::string_view::npos) {
      auto config = rule_name_with_config.substr(equals_pos + 1);

      // https://github.com/chipsalliance/verible/issues/1121
      // Compatibility between command line and rules files
      // Trim outer '"' if present
      if (config.size() >= 2 && config.front() == '"' && config.back() == '"') {
        config.remove_suffix(1);
        config.remove_prefix(1);
      }

      setting.configuration.assign(config.data(), config.size());
    }
    const auto rule_name = rule_name_with_config.substr(0, equals_pos);
    const auto rule_name_set = analysis::GetAllRegisteredLintRuleNames();
    const auto rule_iter = rule_name_set.find(rule_name);

    // Check if text is a valid lint rule.
    if (rule_iter == rule_name_set.end()) {
      absl::StrAppend(error, error->empty() ? "" : "\n", kInvalidFlagMessage,
                      " \"", rule_name, "\"");
      // If the rule doesn't exist just ignore it. Take note of this information
      // so it can be reported but keep parsing configuration.
      parsed_correctly = false;
      continue;
    }

    // If we are about to override a rule which was previously configured in the
    // same configuration file, warn the user about it.
    // NOTE: ignore producing a warning if there is no configuration, just
    // disabing/enabling
    if (!setting.configuration.empty() && rules.count(*rule_iter)) {
      absl::StrAppend(error, error->empty() ? "" : "\n", kRepeatedFlagMessage,
                      " \"", rule_name, "\" = ", setting.configuration);
      parsed_correctly = false;
    }

    // Map keys must use canonical registered string_views for guaranteed
    // lifetime, not just any string-equivalent copy.
    rules[*rule_iter] = setting;
  }

  return parsed_correctly;
}

// Parse and unparse for RuleBundle (for commandlineflags)
std::string RuleBundle::UnparseConfiguration(const char separator,
                                             const bool reverse) const {
  std::vector<std::string> switches;
  switches.reserve(rules.size());
  for (const auto &rule : rules) {
    switches.push_back(absl::StrCat(
        // If rule is set off, prepend "-"
        rule.second.enabled ? "" : "-", rule.first,
        // If we have a configuration, append assignment.
        rule.second.configuration.empty() ? "" : "=",
        rule.second.configuration));
  }
  // Concatenates all of rules into text.
  if (reverse) {
    return absl::StrJoin(switches.rbegin(), switches.rend(),
                         std::string(1, separator));
  }

  return absl::StrJoin(switches.begin(), switches.end(),
                       std::string(1, separator));
}

bool LinterConfiguration::RuleIsOn(const analysis::LintRuleId &rule) const {
  const auto *entry = FindOrNull(configuration_, rule);
  if (entry == nullptr) return false;
  return entry->enabled;
}

void LinterConfiguration::UseRuleSet(const RuleSet &rules) {
  configuration_.clear();
  switch (rules) {
    case RuleSet::kAll: {
      for (const auto &rule : analysis::RegisteredTextStructureRulesNames()) {
        TurnOn(rule);
      }
      for (const auto &rule : analysis::RegisteredSyntaxTreeRulesNames()) {
        TurnOn(rule);
      }
      for (const auto &rule : analysis::RegisteredTokenStreamRulesNames()) {
        TurnOn(rule);
      }
      for (const auto &rule : analysis::RegisteredLineRulesNames()) {
        TurnOn(rule);
      }
      break;
    }
    case RuleSet::kNone:
      break;
    case RuleSet::kDefault:
      for (const auto &rule : analysis::kDefaultRuleSet) {
        TurnOn(rule);
      }
  }
}

void LinterConfiguration::UseRuleBundle(const RuleBundle &rule_bundle) {
  const auto name_set = analysis::GetAllRegisteredLintRuleNames();
  for (const auto &rule_pair : rule_bundle.rules) {
    // This needs to use the canonical registered key string_view, which has
    // guaranteed lifetime.
    configuration_[rule_pair.first] = rule_pair.second;
  }
}

void LinterConfiguration::UseProjectPolicy(const ProjectPolicy &policy,
                                           absl::string_view filename) {
  if (const char *matched_path = policy.MatchesAnyPath(filename)) {
    VLOG(1) << "File \"" << filename << "\" matches path \"" << matched_path
            << "\" from project policy [" << policy.name << "], applying it.";
    for (const auto &rule : policy.disabled_rules) {
      VLOG(1) << "  disabling rule: " << rule;
      TurnOff(rule);
    }
    for (const auto &rule : policy.enabled_rules) {
      VLOG(1) << "  enabling rule: " << rule;
      TurnOn(rule);
    }
  }
}

std::set<analysis::LintRuleId> LinterConfiguration::ActiveRuleIds() const {
  std::set<analysis::LintRuleId> result;
  for (const auto &rule_pair : configuration_) {
    if (rule_pair.second.enabled) {
      result.insert(rule_pair.first);
    }
  }
  return result;
}

void LinterConfiguration::GetRuleBundle(RuleBundle *rule_bundle) const {
  rule_bundle->rules = configuration_;
}

// Iterates through all rules that are mentioned and enabled
// in the "config" map. Constructs instances using the
// "factory"-function, and configures them if a configuration string is
// available. Returns a vector of all successfully created instances.
//
// T should be a descendant of verible::LintRule.
template <typename T>
static absl::StatusOr<std::vector<std::unique_ptr<T>>> CreateRules(
    const std::map<analysis::LintRuleId, RuleSetting> &config,
    std::function<std::unique_ptr<T>(const analysis::LintRuleId &)> factory) {
  std::vector<std::unique_ptr<T>> rule_instances;
  for (const auto &rule_pair : config) {
    const RuleSetting &setting = rule_pair.second;
    if (!setting.enabled) continue;

    std::unique_ptr<T> rule_ptr = factory(rule_pair.first);
    if (rule_ptr == nullptr) continue;

    if (!setting.configuration.empty()) {
      if (absl::Status status = rule_ptr->Configure(setting.configuration);
          !status.ok()) {
        std::string error_msg =
            absl::StrCat(rule_pair.first, " ", status.message());
        return absl::InvalidArgumentError(error_msg);
      }
    }

    rule_instances.push_back(std::move(rule_ptr));
  }
  return rule_instances;
}

absl::StatusOr<std::vector<std::unique_ptr<SyntaxTreeLintRule>>>
LinterConfiguration::CreateSyntaxTreeRules() const {
  return CreateRules<SyntaxTreeLintRule>(configuration_,
                                         analysis::CreateSyntaxTreeLintRule);
}

absl::StatusOr<std::vector<std::unique_ptr<TokenStreamLintRule>>>
LinterConfiguration::CreateTokenStreamRules() const {
  return CreateRules<TokenStreamLintRule>(configuration_,
                                          analysis::CreateTokenStreamLintRule);
}

absl::StatusOr<std::vector<std::unique_ptr<LineLintRule>>>
LinterConfiguration::CreateLineRules() const {
  return CreateRules<LineLintRule>(configuration_,
                                   analysis::CreateLineLintRule);
}

absl::StatusOr<std::vector<std::unique_ptr<TextStructureLintRule>>>
LinterConfiguration::CreateTextStructureRules() const {
  return CreateRules<TextStructureLintRule>(
      configuration_, analysis::CreateTextStructureLintRule);
}

bool LinterConfiguration::operator==(const LinterConfiguration &config) const {
  return ActiveRuleIds() == config.ActiveRuleIds();
}

absl::Status LinterConfiguration::AppendFromFile(
    absl::string_view config_filename) {
  // Read local configuration file
  std::string content;

  absl::StatusOr<std::string> config_or =
      verible::file::GetContentAsString(config_filename);
  if (config_or.ok()) {
    RuleBundle local_rules_bundle;
    std::string error;
    local_rules_bundle.ParseConfiguration(*config_or, '\n', &error);
    // Log warnings and errors
    if (!error.empty()) {
      std::cerr << "Using a partial version from " << config_filename
                << ". Found the following issues: " << error << "\n";
    }

    UseRuleBundle(local_rules_bundle);
    return absl::OkStatus();
  }

  return config_or.status();
}

absl::Status LinterConfiguration::ConfigureFromOptions(
    const LinterOptions &options) {
  // Apply the ruleset bundle first.
  // TODO(b/170876028): reduce the number of ways to select a group of rules,
  // migrate these into hosted project configurations.
  UseRuleSet(options.ruleset);

  if (!options.config_file.empty()) {
    RETURN_IF_ERROR(AppendFromFile(options.config_file));

    if (options.rules_config_search) {
      std::cerr << "Explicit config file " << options.config_file
                << " disables --rules_config_search" << std::endl;
    }
  } else if (options.rules_config_search) {
    // Search upward if search is enabled and no configuration file is
    // specified
    static constexpr absl::string_view linter_config = ".rules.verible_lint";
    std::string resolved_config_file;
    if (verible::file::UpwardFileSearch(options.linting_start_file,
                                        linter_config, &resolved_config_file)
            .ok()) {
      const absl::Status config_read_status =
          AppendFromFile(resolved_config_file);

      if (!config_read_status.ok()) {
        std::cerr << resolved_config_file
                  << ": Unable to read rules configuration file "
                  << config_read_status << std::endl;
      }
    }
  }

  // Turn on rules found in config
  UseRuleBundle(options.rules);

  // Apply external waivers
  external_waivers = std::string(options.waiver_files);

  return absl::OkStatus();
}

std::ostream &operator<<(std::ostream &stream,
                         const LinterConfiguration &config) {
  const auto rules = config.ActiveRuleIds();
  return stream << "{ " << absl::StrJoin(rules.begin(), rules.end(), ", ")
                << " }";
}

static const verible::EnumNameMap<RuleSet> &RuleSetEnumStringMap() {
  static const verible::EnumNameMap<RuleSet> kRuleSetEnumStringMap({
      {"all", RuleSet::kAll},
      {"none", RuleSet::kNone},
      {"default", RuleSet::kDefault},
  });
  return kRuleSetEnumStringMap;
}

std::ostream &operator<<(std::ostream &stream, RuleSet rules) {
  return RuleSetEnumStringMap().Unparse(rules, stream);
}

//
// Parse and unparse for ruleset (for commandlineflags)
//
std::string AbslUnparseFlag(const RuleSet &rules) {
  std::ostringstream stream;
  stream << rules;
  return stream.str();
}

bool AbslParseFlag(absl::string_view text, RuleSet *rules, std::string *error) {
  return RuleSetEnumStringMap().Parse(text, rules, error, "--ruleset value");
}

std::string AbslUnparseFlag(const RuleBundle &bundle) {
  return bundle.UnparseConfiguration(',');
}

bool AbslParseFlag(absl::string_view text, RuleBundle *bundle,
                   std::string *error) {
  return bundle->ParseConfiguration(text, ',', error);
}

}  // namespace verilog
