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

#include "common/analysis/lint_waiver.h"

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <functional>
#include <iterator>
#include <map>
#include <set>
#include <utility>
#include <vector>

#include "absl/strings/str_split.h"
#include "absl/strings/string_view.h"
#include "common/strings/comment_utils.h"
#include "common/text/text_structure.h"
#include "common/text/token_info.h"
#include "common/text/token_stream_view.h"
#include "common/util/iterator_range.h"
#include "common/util/logging.h"

namespace verible {

void LintWaiver::WaiveOneLine(absl::string_view rule_name, size_t line_number) {
  WaiveLineRange(rule_name, line_number, line_number + 1);
}

void LintWaiver::WaiveLineRange(absl::string_view rule_name, size_t line_begin,
                                size_t line_end) {
  LineSet& line_set = waiver_map_[rule_name];
  if (line_set.size() < line_end) {
    line_set.resize(line_end);
  }
  for (size_t line = line_begin; line < line_end; ++line) {
    line_set[line] = true;
  }
}

bool LintWaiver::RuleIsWaivedOnLine(absl::string_view rule_name,
                                    size_t line_number) const {
  const auto* line_set = verible::container::FindOrNull(waiver_map_, rule_name);
  return line_set != nullptr && LineSetContains(*line_set, line_number);
}

bool LintWaiver::Empty() const {
  for (const auto& rule_waiver : waiver_map_) {
    if (!rule_waiver.second.empty()) {
      return false;
    }
  }
  return true;
}

absl::string_view LintWaiverBuilder::ExtractWaivedRuleFromComment(
    absl::string_view comment_text,
    std::vector<absl::string_view>* comment_tokens) const {
  // Look for directives of the form: <tool_name> <directive> <rule_name>
  // Addition text beyond the last argument is ignored, so it could
  // contain more comment text.
  auto& tokens = *comment_tokens;
  // TODO(fangism): Stop splitting after 3 tokens, everything after that is
  // ignored.  Use something like absl::MaxSplits, but works with multi-spaces.
  tokens = absl::StrSplit(comment_text, ' ', absl::SkipEmpty());
  if (tokens.size() >= 3) {
    if (tokens[0] == waiver_trigger_keyword_) {
      if (tokens[1] == waive_one_line_keyword_ ||
          tokens[1] == waive_range_start_keyword_ ||
          tokens[1] == waive_range_stop_keyword_) {
        // TODO(b/73512873): Support waiving multiple rules in one command.
        return tokens[2];  // name of waived rule
      }
    }
  }
  return "";
}

void LintWaiverBuilder::ProcessLine(const TokenRange& tokens,
                                    size_t line_number) {
  // TODO(fangism): [optimization] Use a SmallVector, or function-local
  // static to avoid re-allocation in every call.  This method does not
  // need to be re-entrant.
  std::vector<absl::string_view> lint_directives;

  // Determine whether line is blank, where whitespace still counts as blank.
  const bool line_is_blank =
      std::find_if_not(tokens.begin(), tokens.end(), is_token_whitespace_) ==
      tokens.end();
  if (line_is_blank) {
    unapplied_oneline_waivers_.clear();
    return;
  }

  // Determine whether line contains any non-space, non-comment tokens.
  const bool line_has_tokens =
      std::find_if(tokens.begin(), tokens.end(), [=](const TokenInfo& t) {
        return !(is_token_whitespace_(t) || is_token_comment_(t));
      }) != tokens.end();

  if (line_has_tokens) {
    // Apply un-applied one-line waivers, and then reset them.
    for (const auto& rule : unapplied_oneline_waivers_) {
      lint_waiver_.WaiveOneLine(rule, line_number);
    }
    unapplied_oneline_waivers_.clear();
  }

  // Find all directives on this line.
  std::vector<absl::string_view> comment_tokens;  // Re-use in loop.
  for (const auto& token : tokens) {
    if (is_token_comment_(token)) {
      // Lex the comment text.
      const absl::string_view comment_text =
          StripCommentAndSpacePadding(token.text);
      comment_tokens = absl::StrSplit(comment_text, ' ', absl::SkipEmpty());
      // TODO(fangism): Support different waiver lexers.
      const absl::string_view waived_rule =
          ExtractWaivedRuleFromComment(comment_text, &comment_tokens);
      if (!waived_rule.empty()) {
        // If there are any significant tokens on this line, apply to this
        // line, otherwise defer until the next line.
        const auto command = comment_tokens[1];
        if (command == waive_one_line_keyword_) {
          if (line_has_tokens) {
            lint_waiver_.WaiveOneLine(waived_rule, line_number);
          } else {
            unapplied_oneline_waivers_.insert(waived_rule);
          }
        } else if (command == waive_range_start_keyword_) {
          waiver_open_ranges_.insert(std::make_pair(waived_rule, line_number));
          // Ignore failed attempts to re-insert, the first entry of any
          // duplicates should win because that encompasses the largest
          // applicable range.
        } else if (command == waive_range_stop_keyword_) {
          const auto range_start_iter = waiver_open_ranges_.find(waived_rule);
          if (range_start_iter != waiver_open_ranges_.end()) {
            // Waive the range from start to this line.
            lint_waiver_.WaiveLineRange(waived_rule, range_start_iter->second,
                                        line_number);
            // Reset the range for this rule.
            waiver_open_ranges_.erase(range_start_iter);
          }
          // else ignore unbalanced stop-range directive (could be mistaken rule
          // name).
        }
      }
    }
  }
}

void LintWaiverBuilder::ProcessTokenRangesByLine(
    const TextStructureView& text_structure) {
  const size_t total_lines = text_structure.Lines().size();
  const auto& tokens = text_structure.TokenStream();
  for (size_t i = 0; i < total_lines; ++i) {
    const auto token_range = text_structure.TokenRangeOnLine(i);
    const int begin_dist = std::distance(tokens.begin(), token_range.begin());
    const int end_dist = std::distance(tokens.begin(), token_range.end());
    CHECK_LE(0, begin_dist);
    CHECK_LE(begin_dist, end_dist);
    CHECK_LE(end_dist, tokens.size());
    ProcessLine(token_range, i);
  }

  // Flush out any remaining open-ranges, so that those waivers take effect
  // until the end-of-file.
  // TODO(b/78064145): Detect these as suspiciously unbalanced waiver uses.
  for (const auto& open_range : waiver_open_ranges_) {
    lint_waiver_.WaiveLineRange(open_range.first, open_range.second,
                                total_lines);
  }
  waiver_open_ranges_.clear();
}

}  // namespace verible
