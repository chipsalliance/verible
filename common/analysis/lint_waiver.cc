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
#include <regex>  // NOLINT
#include <set>
#include <utility>
#include <vector>

#include "absl/strings/str_split.h"
#include "absl/strings/string_view.h"
#include "common/analysis/command_file_lexer.h"
#include "common/strings/comment_utils.h"
#include "common/strings/line_column_map.h"
#include "common/text/text_structure.h"
#include "common/text/token_info.h"
#include "common/text/token_stream_view.h"
#include "common/util/container_iterator_range.h"
#include "common/util/file_util.h"
#include "common/util/iterator_range.h"
#include "common/util/logging.h"

namespace verible {

void LintWaiver::WaiveOneLine(absl::string_view rule_name, int line_number) {
  WaiveLineRange(rule_name, line_number, line_number + 1);
}

void LintWaiver::WaiveLineRange(absl::string_view rule_name, int line_begin,
                                int line_end) {
  LineNumberSet& line_set = waiver_map_[rule_name];
  line_set.Add({line_begin, line_end});
}

void LintWaiver::WaiveWithRegex(absl::string_view rule_name,
                                const std::string& regex_str) {
  RegexVector& regex_vector = waiver_re_map_[rule_name];
  auto regex_iter = regex_cache_.find(regex_str);

  if (regex_iter == regex_cache_.end()) {
    regex_cache_[regex_str] = std::regex(regex_str);
  }

  regex_vector.push_back(&regex_cache_[regex_str]);
}

void LintWaiver::RegexToLines(absl::string_view contents,
                              const LineColumnMap& line_map) {
  for (const auto& rule : waiver_re_map_) {
    for (const auto* re : rule.second) {
      for (std::cregex_iterator i(contents.begin(), contents.end(), *re);
           i != std::cregex_iterator(); i++) {
        std::cmatch match = *i;
        WaiveOneLine(rule.first, line_map(match.position()).line);
      }
    }
  }
}

bool LintWaiver::RuleIsWaivedOnLine(absl::string_view rule_name,
                                    int line_number) const {
  const auto* line_set = verible::container::FindOrNull(waiver_map_, rule_name);
  return line_set != nullptr && LineNumberSetContains(*line_set, line_number);
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

void LintWaiverBuilder::ProcessLine(const TokenRange& tokens, int line_number) {
  // TODO(fangism): [optimization] Use a SmallVector, or function-local
  // static to avoid re-allocation in every call.  This method does not
  // need to be re-entrant.
  std::vector<absl::string_view> lint_directives;

  // Determine whether line is blank, where whitespace still counts as blank.
  const bool line_is_blank =
      std::all_of(tokens.begin(), tokens.end(), is_token_whitespace_);
  if (line_is_blank) {
    unapplied_oneline_waivers_.clear();
    return;
  }

  // Determine whether line contains any non-space, non-comment tokens.
  const bool line_has_tokens =
      std::any_of(tokens.begin(), tokens.end(), [=](const TokenInfo& t) {
        return !(is_token_whitespace_(t) || is_token_comment_(t));
      });

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
          StripCommentAndSpacePadding(token.text());
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
  const int total_lines = text_structure.Lines().size();
  const auto& tokens = text_structure.TokenStream();
  for (int i = 0; i < total_lines; ++i) {
    const auto token_range = text_structure.TokenRangeOnLine(i);
    const int begin_dist = std::distance(tokens.begin(), token_range.begin());
    const int end_dist = std::distance(tokens.begin(), token_range.end());
    CHECK_LE(0, begin_dist);
    CHECK_LE(begin_dist, end_dist);
    CHECK_LE(end_dist, tokens.size());
    ProcessLine(token_range, i);
  }

  // Apply regex waivers
  lint_waiver_.RegexToLines(text_structure.Contents(),
                            text_structure.GetLineColumnMap());

  // Flush out any remaining open-ranges, so that those waivers take effect
  // until the end-of-file.
  // TODO(b/78064145): Detect these as suspiciously unbalanced waiver uses.
  for (const auto& open_range : waiver_open_ranges_) {
    lint_waiver_.WaiveLineRange(open_range.first, open_range.second,
                                total_lines);
  }
  waiver_open_ranges_.clear();
}

template <typename... T>
static std::string WaiveCommandErrorFmt(LineColumn pos,
                                        absl::string_view filename,
                                        absl::string_view msg,
                                        const T&... args) {
  return absl::StrCat(filename, ":", pos.line + 1, ":", pos.column + 1,
                      ": command error: ", msg, args...);
}

template <typename... T>
static absl::Status WaiveCommandError(LineColumn pos,
                                      absl::string_view filename,
                                      absl::string_view msg, const T&... args) {
  return absl::InvalidArgumentError(
      WaiveCommandErrorFmt(pos, filename, msg, args...));
}

static absl::Status WaiveCommandHandler(
    const TokenRange& tokens, absl::string_view waive_file,
    absl::string_view waive_content, absl::string_view lintee_filename,
    const LineColumnMap& line_map, LintWaiver* waiver,
    const std::set<absl::string_view>& active_rules) {
  absl::string_view rule;

  absl::string_view option;
  absl::string_view val;

  int line_start = -1;
  int line_end = -1;
  std::string regex;

  bool can_use_regex = false;
  bool can_use_lineno = false;
  bool location_match = true;

  LineColumn token_pos;
  LineColumn regex_token_pos = {};

  for (const auto& token : tokens) {
    token_pos = line_map(token.left(waive_content));

    switch (token.token_enum()) {
      case CFG_TK_COMMAND:
        // Verify that this command is supported by this handler
        if (token.text() != "waive") {
          return absl::InvalidArgumentError("Invalid command handler called");
        }
        break;
      case CFG_TK_ERROR:
        return WaiveCommandError(token_pos, waive_file, "Configuration error");
      case CFG_TK_PARAM:
      case CFG_TK_FLAG:
        return WaiveCommandError(token_pos, waive_file,
                                 "Unsupported argument: ", token.text());
      case CFG_TK_FLAG_WITH_ARG:
        option = token.text();
        break;
      case CFG_TK_ARG:

        val = token.text();

        if (option == "rule") {
          for (auto r : active_rules) {
            if (val == r) {
              rule = r;
              break;
            }
          }

          if (rule.empty()) {
            return WaiveCommandError(token_pos, waive_file,
                                     "Invalid rule: ", val);
          }

          break;
        }

        if (option == "line") {
          size_t range = val.find(':');
          if (range != absl::string_view::npos) {
            // line range
            if (!absl::SimpleAtoi(val.substr(0, range), &line_start) ||
                !absl::SimpleAtoi(val.substr(range + 1, val.length() - range),
                                  &line_end)) {
              return WaiveCommandError(token_pos, waive_file,
                                       "Unable to parse range: ", val);
            }
          } else {
            // single line
            if (!absl::SimpleAtoi(val, &line_start)) {
              return WaiveCommandError(token_pos, waive_file,
                                       "Unable to parse line number: ", val);
            }
            line_end = line_start;
          }

          if (line_start < 1) {
            return WaiveCommandError(token_pos, waive_file,
                                     "Invalid line number: ", val);
          } else if (line_start > line_end) {
            return WaiveCommandError(token_pos, waive_file,
                                     "Invalid line range: ", val);
          }

          can_use_lineno = true;
          continue;
        }

        if (option == "regex") {
          regex = std::string(val);
          can_use_regex = true;

          // Save a copy to token pos in case the regex is invalid
          regex_token_pos = token_pos;
          continue;
        }

        if (option == "location") {
          const std::string file_match_regex(val);
          try {
            const std::regex file_matcher(file_match_regex);
            location_match =
                std::regex_search(std::string(lintee_filename), file_matcher);
          } catch (const std::regex_error& e) {
            return WaiveCommandError(token_pos, waive_file,
                                     "--location regex is invalid");
          }
          continue;
        }

        return WaiveCommandError(token_pos, waive_file,
                                 "Unsupported flag: ", option);

      case CFG_TK_NEWLINE:
        if (!location_match) return absl::OkStatus();

        // Check if everything required has been set
        if (rule.empty()) {
          return WaiveCommandError(token_pos, waive_file,
                                   "Insufficient waiver configuration");
        }

        if (can_use_regex && can_use_lineno) {
          return WaiveCommandError(
              token_pos, waive_file,
              "Regex and line flags are mutually exclusive");
        }

        if (can_use_regex) {
          try {
            waiver->WaiveWithRegex(rule, regex);
          } catch (const std::regex_error& e) {
            auto* reason = e.what();

            return WaiveCommandError(regex_token_pos, waive_file,
                                     "Invalid regex: ", reason);
          }
        }

        if (can_use_lineno) {
          waiver->WaiveLineRange(rule, line_start - 1, line_end);
        }

        if (!can_use_regex && !can_use_lineno) {
          std::string content;
          absl::Status status =
              verible::file::GetContents(lintee_filename, &content);
          if (!status.ok()) {
            return WaiveCommandError(token_pos, waive_file, status.ToString());
          }

          const size_t number_of_lines =
              std::count(content.begin(), content.end(), '\n');
          waiver->WaiveLineRange(rule, 1, number_of_lines);
        }

        return absl::OkStatus();
      case CFG_TK_COMMENT:
        /* Ignore comments */
        break;
      default:
        return WaiveCommandError(token_pos, waive_file, "Expecting arguments");
    }
  }

  return absl::OkStatus();
}

using HandlerFun = std::function<absl::Status(
    const TokenRange&, absl::string_view waive_file,
    absl::string_view waive_content, absl::string_view lintee_filename,
    const LineColumnMap&, LintWaiver*, const std::set<absl::string_view>&)>;
static const std::map<absl::string_view, HandlerFun>& GetCommandHandlers() {
  // allocated once, never freed
  static const auto* handlers = new std::map<absl::string_view, HandlerFun>{
      // Right now, we only have one handler
      {"waive", WaiveCommandHandler},
  };
  return *handlers;
}

absl::Status LintWaiverBuilder::ApplyExternalWaivers(
    const std::set<absl::string_view>& active_rules,
    absl::string_view lintee_filename, absl::string_view waiver_filename,
    absl::string_view waivers_config_content) {
  if (waivers_config_content.empty()) {
    return absl::Status(absl::StatusCode::kInternal,
                        "Broken waiver config handle");
  }

  CommandFileLexer lexer(waivers_config_content);
  const LineColumnMap line_map(waivers_config_content);
  LineColumn command_pos;

  const auto& handlers = GetCommandHandlers();

  std::vector<TokenRange> commands = lexer.GetCommandsTokenRanges();

  bool all_commands_ok = true;
  for (const auto c_range : commands) {
    const auto command = make_container_range(c_range.begin(), c_range.end());

    command_pos = line_map(command.begin()->left(waivers_config_content));

    if (command[0].token_enum() == CFG_TK_COMMENT) {
      continue;
    }

    // The very first Token in 'command' should be an actual command
    if (command.empty() || command[0].token_enum() != CFG_TK_COMMAND) {
      LOG(ERROR) << WaiveCommandErrorFmt(command_pos, waiver_filename,
                                         "Not a command: ", command[0].text());
      all_commands_ok = false;
      continue;
    }

    // Check if command is supported
    auto handler_iter = handlers.find(command[0].text());
    if (handler_iter == handlers.end()) {
      LOG(ERROR) << WaiveCommandErrorFmt(
          command_pos, waiver_filename,
          "Command not supported: ", command[0].text());
      all_commands_ok = false;
      continue;
    }

    auto status = handler_iter->second(command, waiver_filename,
                                       waivers_config_content, lintee_filename,
                                       line_map, &lint_waiver_, active_rules);
    if (!status.ok()) {
      // Mark the return value to be false, but continue parsing the config
      // file anyway
      all_commands_ok = false;
      LOG(ERROR) << status.message();
    }
  }

  if (all_commands_ok) {
    return absl::OkStatus();
  } else {
    return absl::InvalidArgumentError("Errors applying external waivers.");
  }
}

}  // namespace verible
