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

#ifndef VERIBLE_COMMON_ANALYSIS_LINT_WAIVER_H_
#define VERIBLE_COMMON_ANALYSIS_LINT_WAIVER_H_

#include <map>
#include <memory>
#include <set>
#include <string>
#include <utility>
#include <vector>

#include "absl/container/flat_hash_map.h"
#include "absl/status/status.h"
#include "absl/strings/string_view.h"
#include "re2/re2.h"
#include "verible/common/strings/line-column-map.h"
#include "verible/common/strings/position.h"
#include "verible/common/text/text-structure.h"
#include "verible/common/text/token-stream-view.h"
#include "verible/common/util/container-util.h"

namespace verible {

// LintWaiver maintains a set of line ranges per lint rule that should be
// exempt from each rule.
class LintWaiver {
  // Vector of un-owned regexes.
  using RegexVector = std::vector<const re2::RE2 *>;

 public:
  LintWaiver() = default;

  // Construction either done in Builder function or LintWaiverBuilder class
  // defined below.
  // void Initialize(const LintWaiverBuilder&);  // or configuration

  // Scan lines for comments with linter directives.
  // Lines with only space/comment tokens will apply to the next line that
  // doesn't contain only spaces and newlines.  Blank line will cancel the
  // waiver.
  // Waiver comments on lines with other tokens will only waive that line.

  // Adds a single line to the set of waived lines for a single rule.
  void WaiveOneLine(absl::string_view rule_name, int line_number);

  // Adds a range [line_begin, line_end) over which a waiver applies.
  void WaiveLineRange(absl::string_view rule_name, int line_begin,
                      int line_end);

  // Adds a regular expression which will be used to apply a waiver.
  absl::Status WaiveWithRegex(absl::string_view rule_name,
                              absl::string_view regex);

  // Converts the prepared regular expressions to line numbers and applies the
  // waivers.
  void RegexToLines(absl::string_view content, const LineColumnMap &line_map);

  // Returns true if `line_number` should be waived for a particular rule.
  bool RuleIsWaivedOnLine(absl::string_view rule_name, int line_number) const;

  // Returns true if there are no lines waived for any rules.
  bool Empty() const;

  // TODO(hzeller): The following methods break abstraction and are only
  // for performance. Reconsider if this is worth it.
  const LineNumberSet *LookupLineNumberSet(absl::string_view rule_name) const {
    return verible::container::FindOrNull(waiver_map_, rule_name);
  }

  // Test if a particular line is included in the set.
  static bool LineNumberSetContains(const LineNumberSet &line_set, int line) {
    return line_set.Contains(line);
  }

 private:
  const RE2 *GetOrCreateCachedRegex(absl::string_view regex_str);

  // Keys in the maps below are the names of the waived rules. They can be
  // string_view because the static strings for each lint rule class exist,
  // and will outlive all LintWaiver objects. This applies to both waiver_map_
  // and waiver_re_map_.
  absl::flat_hash_map<absl::string_view, LineNumberSet> waiver_map_;
  absl::flat_hash_map<absl::string_view, RegexVector> waiver_re_map_;

  absl::flat_hash_map<std::string, std::unique_ptr<re2::RE2>> regex_cache_;
};

// LintWaiverBuilder is a language-agnostic helper class for constructing
// LintWaiver maps.  Objects of this builder type become language-specific
// through function hooks passed to the constructor.
// Alternately, a derived class can bind the constructor arguments for a
// language-specific implementation.
//
// A waiver comment on its own line applies the waiver to the next
// non-comment-line.
//
//   1: // tool_name rule_name waive
//   2: other text, this line is waived
//
// A waiver comment on a line with other non-comment text waives its own line:
//
//   1: blah blah  // tool_name rule_name waive // waives this line only
//
// TODO(fangism): Support lint waiver directives in multi-line block comments.
class LintWaiverBuilder {
 public:
  // 'is_comment' returns true if the token passed is considered a comment.
  // 'is_space' returns true if token represents whitespace.
  // 'trigger' is the string that triggers waiver processing, often a tool name.
  //   The first argument after the trigger is the name of the rule to waive.
  // 'waive_command' is the second argument after the trigger, and is the
  //   command for 'waive-one-line'.
  LintWaiverBuilder(TokenFilterPredicate &&is_comment,
                    TokenFilterPredicate &&is_space, absl::string_view trigger,
                    absl::string_view waive_line_command,
                    absl::string_view waive_start_command,
                    absl::string_view waive_stop_command)
      : waiver_trigger_keyword_(trigger),
        waive_one_line_keyword_(waive_line_command),
        waive_range_start_keyword_(waive_start_command),
        waive_range_stop_keyword_(waive_stop_command),
        is_token_comment_(std::move(is_comment)),
        is_token_whitespace_(std::move(is_space)) {}

  // Takes a single line's worth of tokens and determines updates to the set of
  // waived lines.  Pass a slice of tokens using make_range.
  void ProcessLine(const TokenRange &tokens, int line_number);

  // Takes a lexically analyzed text structure and determines the entire set of
  // waived lines.  This can be more easily unit-tested using
  // TextStructureTokenized from text_structure_test_utils.h.
  void ProcessTokenRangesByLine(const TextStructureView &);

  // Takes a set of active linter rules and the affected filename to be linted,
  // and applies waivers from waiver_filename and its content.
  absl::Status ApplyExternalWaivers(
      const std::set<absl::string_view> &active_rules,
      absl::string_view lintee_filename, absl::string_view waiver_filename,
      absl::string_view waivers_config_content);

  const LintWaiver &GetLintWaiver() const { return lint_waiver_; }

 protected:
  // Parses a comment and extracts a waived rule name.
  // If text does not match the waived form, then return an empty string.
  // `comment_tokens` is just re-used memory to avoid re-allocation.
  absl::string_view ExtractWaivedRuleFromComment(
      absl::string_view comment_text,
      std::vector<absl::string_view> *comment_tokens) const;

  // Special string that leads a comment that is a waiver directive
  // Typically, name of linter tool is used here.
  absl::string_view waiver_trigger_keyword_;

  // Command to waive one line, either the current line if there are tokens
  // on the current line or the next non-comment-non-blank-line.
  absl::string_view waive_one_line_keyword_;  // e.g. "waive"

  // Command pair to start and stop waiving ranges of lines.
  // e.g. "waive-start", "waive-stop"
  absl::string_view waive_range_start_keyword_;
  absl::string_view waive_range_stop_keyword_;

  // Returns true if token is a comment.
  TokenFilterPredicate is_token_comment_;

  // Returns true if token is a whitespace (still considered blank).
  TokenFilterPredicate is_token_whitespace_;

  // This holds the set of to-be-applied lint waivers.
  // Element string_views point to string memory that outlives this builder.
  std::set<absl::string_view> unapplied_oneline_waivers_;

  // This holds the set of open ranges of lines, keyed by rule name.
  // Value is the lower-bound of each encountered waiver range.
  // string_view keys point to string memory that outlives this builder.
  std::map<absl::string_view, int> waiver_open_ranges_;

  // Set of waived lines per rule.
  LintWaiver lint_waiver_;
};

}  // namespace verible

#endif  // VERIBLE_COMMON_ANALYSIS_LINT_WAIVER_H_
