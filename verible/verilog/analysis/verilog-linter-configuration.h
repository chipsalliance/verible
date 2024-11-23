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

#ifndef VERIBLE_VERILOG_ANALYSIS_VERILOG_LINTER_CONFIGURATION_H_
#define VERIBLE_VERILOG_ANALYSIS_VERILOG_LINTER_CONFIGURATION_H_

#include <iosfwd>
#include <map>
#include <memory>
#include <set>
#include <string>
#include <vector>

#include "absl/status/status.h"
#include "absl/status/statusor.h"
#include "absl/strings/string_view.h"
#include "verible/common/analysis/line-lint-rule.h"
#include "verible/common/analysis/syntax-tree-lint-rule.h"
#include "verible/common/analysis/text-structure-lint-rule.h"
#include "verible/common/analysis/token-stream-lint-rule.h"
#include "verible/verilog/analysis/descriptions.h"

namespace verilog {

// Setting for a configuration
struct RuleSetting {
  bool enabled;
  std::string configuration;
};

// Error to be shown when an invalid flag is
// encountered while parsing a configuration
inline constexpr absl::string_view kInvalidFlagMessage = "[ERR] Invalid flag";

// Warning to be shown when we parse a configuration file that configures the
// same rule more than once.
inline constexpr absl::string_view kRepeatedFlagMessage =
    "[WARN] Repeated flag in the configuration. Last provided value will be "
    "used";

// Warning to be shown when an stray comma is
// encountered while parsing a configuration
inline constexpr absl::string_view kStrayCommaWarning =
    "[WARN] Ignoring stray comma at the end of configuration";

// Enum denoting a ruleset
//   kNone     no rules are enabled
//   kDefault  default ruleset is enabled
//   kAll      all rules are enabled
enum class RuleSet { kNone, kDefault, kAll };

// Pair of functions that perform stringification and destringification
// in order to support commandline flags.
//
// AbslUnparseFlag takes the parsed representation of a ruleset
// and converts it into string representation.
std::string AbslUnparseFlag(const RuleSet &rules);

// ParseFlags takes a source string text and a target Ruleset.
// It attempts to parse text into a RuleSet and put that RuleSet into rules
// If successful, returns true.
// Otherwise, sets string error to the a error message and returns false.
bool AbslParseFlag(absl::string_view text, RuleSet *rules, std::string *error);

// Container class for parse/unparse flags
// Keys must be the exact string_views in registered maps (not just string
// equivalent) for the lifetime guarantee.
struct RuleBundle {
  std::map<absl::string_view, RuleSetting> rules;
  // Parse configuration from input. Separator between rules is 'separator',
  // typically that would be a comma or newline.
  bool ParseConfiguration(absl::string_view text, char separator,
                          std::string *error);
  // Unparse the rules structure back to a string. Separator between rules
  // is 'separator', typically that would be a comma or newline. The String
  // is constructed by iterrating over the map, with 'reverse=true' used to
  // construct the string in reverse order.
  std::string UnparseConfiguration(char separator, bool reverse = true) const;
};

// Pair of functions that perform stringification and destringification
// in order to support commandline flags.
//
// AbslUnparseFlag takes the parsed representation of a flag (RuleBundle)
// and converts it into string representation.
std::string AbslUnparseFlag(const RuleBundle &bundle);

// ParseFlags takes a source string text and a target RuleBundle.
// It clears bundle and parses text into it by checking to make sure
// each rule is registered with the LintRuleRegistry.
// If a rule is prepended with '-', then it is mapped to RuleSetting::kRuleOff
// Otherwise, it is mapped to RuleSetting::kRuleOn
// If all rules are parsed successfully, returns true.
// Otherwise, sets string error to the a error message containing the
// invalid lint rule and returns false.
bool AbslParseFlag(absl::string_view text, RuleBundle *bundle,
                   std::string *error);

// ProjectPolicy is needed in a transitional period when new rules are
// becoming enabled while pre-existing code is still following their
// respective project conventions and guidelines.  These blanket waivers
// are intended to minimize agony on current projects, while allowing
// the full set of rules to take effect on new projects.
struct ProjectPolicy {
  // A short name for policy, for diagnostic purposes.
  absl::string_view name;

  // Raw string to check for being part of the path.  Not a regex pattern.
  // Apply this exemption only if substring occurs in the file path.
  std::vector<const char *> path_substrings;

  // Raw string to check for being part of the path.  Not a regex pattern.
  // Files that match the exclusion will not be analyzed.
  // This is suitable for paths that contain files that no human will ever
  // read.
  std::vector<const char *> path_exclusions;

  // Reviewers to involve for policy changes.  At least two.
  std::vector<const char *> owners;

  // Names of lint rules to disable.
  std::vector<const char *> disabled_rules;

  // Names of lint rules to enable (takes precedence over disabled_rules).
  std::vector<const char *> enabled_rules;

  // Returns a path if filename matches any of path_substrings,
  // otherwise nullptr.
  const char *MatchesAnyPath(absl::string_view filename) const;

  // Returns a path if filename matches any of path_exclusions,
  // otherwise nullptr.
  const char *MatchesAnyExclusions(absl::string_view filename) const;

  // Returns true if all disabled_rules and enabled_rules refer to registered
  // rules.  This helps catch typos.
  bool IsValid() const;

  // Returns a glob pattern for shell case statement: "*path1* | *path2* | ..."
  std::string ListPathGlobs() const;
};

struct LinterOptions {
  // strings, ints, bools, and unprocessed values from flags, no other derived
  // information. Reasonable default values may be specified here for each
  // member

  // The base set of rules used by linter
  const RuleSet ruleset;
  // Bundle of rules to enable/disable in addition to the base set
  const RuleBundle &rules;
  // Path to a file with extra linter configuration, applied on top of the
  // base 'ruleset' and extra 'rules'
  std::string config_file;
  // Enables upward config file search
  bool rules_config_search;
  // Defines the starting point for the upward config search algorithm,
  // usually set to the currently linted file
  std::string linting_start_file;
  // Path to the external waivers configuration file
  std::string waiver_files;
};

// LinterConfiguration is used for tracking enabled lint rules
// Individual LintRules are defined LintRuleRegistry. Their names are the
// strings that they are registered under.
//
// Usage
//   LinterConfiguration config;
//   config.UseRuleSet(RuleSet::kDefault);
//   config.TurnOn("rule-1");
//   config.TurnOn("rule-2");
//   linter.Configure(config);
//
class LinterConfiguration {
 public:
  // Constructor has no rules enabled by default;
  LinterConfiguration() = default;

  void TurnOn(const analysis::LintRuleId &rule) {
    configuration_[rule] = {true, ""};
  }

  void TurnOff(const analysis::LintRuleId &rule) {
    configuration_[rule] = {false, ""};
  }

  bool RuleIsOn(const analysis::LintRuleId &rule) const;

  // Clears configuration and updates to passed ruleset.
  // Behavior is as follows:
  //   RuleSet::kAll      Turns on all registered rules
  //   RuleSet::kNone     Turns off all rules
  //   RuleSet::kDefault  Turns on default set of rules as defined in
  //                      analysis/default_rules.h
  //
  // Note that additional rules can be layered on top of a ruleset via
  // TurnOn/TurnOff/UseRuleBundle.
  void UseRuleSet(const RuleSet &rules);

  // Updates LinterConfiguration to enabled/disable all lint rules
  // in rule_bundle
  void UseRuleBundle(const RuleBundle &rule_bundle);

  // Gets a rule_bundle of the LinterConfiguration
  void GetRuleBundle(RuleBundle *rule_bundle) const;

  // Adjust set of active rules based on the filename.
  void UseProjectPolicy(const ProjectPolicy &policy,
                        absl::string_view filename);

  // Return the keys of enabled lint rules, sorted.
  std::set<analysis::LintRuleId> ActiveRuleIds() const;

  // Creates instances of every enabled syntax tree rule
  absl::StatusOr<std::vector<std::unique_ptr<verible::SyntaxTreeLintRule>>>
  CreateSyntaxTreeRules() const;

  // Creates instances of every enabled token stream rule
  absl::StatusOr<std::vector<std::unique_ptr<verible::TokenStreamLintRule>>>
  CreateTokenStreamRules() const;

  // Creates instances of every enabled line rule
  absl::StatusOr<std::vector<std::unique_ptr<verible::LineLintRule>>>
  CreateLineRules() const;

  // Creates instances of every enabled text structure lint rule
  absl::StatusOr<std::vector<std::unique_ptr<verible::TextStructureLintRule>>>
  CreateTextStructureRules() const;

  // Path to external lint waivers configuration file
  std::string external_waivers;

  // Returns true if configurations are equivalent.
  bool operator==(const LinterConfiguration &) const;

  bool operator!=(const LinterConfiguration &r) const { return !(*this == r); }

  // Appends linter rules configuration from a file
  absl::Status AppendFromFile(absl::string_view filename);

  // Generates configuration forn LinterOptions
  absl::Status ConfigureFromOptions(const LinterOptions &options);

 private:
  // map of all enabled rules
  std::map<analysis::LintRuleId, RuleSetting> configuration_;
};

std::ostream &operator<<(std::ostream &, const LinterConfiguration &);

}  // namespace verilog

#endif  // VERIBLE_VERILOG_ANALYSIS_VERILOG_LINTER_CONFIGURATION_H_
