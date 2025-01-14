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

#include "verible/common/analysis/lint-waiver.h"

#include <algorithm>
#include <cstddef>
#include <functional>
#include <iterator>
#include <map>
#include <memory>
#include <set>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include "absl/status/status.h"
#include "absl/status/statusor.h"
#include "absl/strings/numbers.h"
#include "absl/strings/str_cat.h"
#include "absl/strings/str_split.h"
#include "re2/re2.h"
#include "verible/common/analysis/command-file-lexer.h"
#include "verible/common/strings/comment-utils.h"
#include "verible/common/strings/line-column-map.h"
#include "verible/common/strings/position.h"
#include "verible/common/text/text-structure.h"
#include "verible/common/text/token-info.h"
#include "verible/common/text/token-stream-view.h"
#include "verible/common/util/container-iterator-range.h"
#include "verible/common/util/container-util.h"
#include "verible/common/util/file-util.h"
#include "verible/common/util/iterator-range.h"
#include "verible/common/util/logging.h"

namespace verible {

void LintWaiver::WaiveOneLine(std::string_view rule_name, int line_number) {
  WaiveLineRange(rule_name, line_number, line_number + 1);
}

void LintWaiver::WaiveLineRange(std::string_view rule_name, int line_begin,
                                int line_end) {
  LineNumberSet &line_set = waiver_map_[rule_name];
  line_set.Add({line_begin, line_end});
}

const RE2 *LintWaiver::GetOrCreateCachedRegex(std::string_view regex_str) {
  auto found = regex_cache_.find(regex_str);
  if (found != regex_cache_.end()) {
    return found->second.get();
  }

  auto inserted = regex_cache_.emplace(
      regex_str, std::make_unique<re2::RE2>(regex_str, re2::RE2::Quiet));
  return inserted.first->second.get();
}

absl::Status LintWaiver::WaiveWithRegex(std::string_view rule_name,
                                        std::string_view regex_str) {
  const std::string regex_as_group = absl::StrCat("(", regex_str, ")");
  const RE2 *regex = GetOrCreateCachedRegex(regex_as_group);
  if (!regex->ok()) {
    return absl::InvalidArgumentError(
        absl::StrCat("Invalid regex: ", regex->error()));
  }

  waiver_re_map_[rule_name].push_back(regex);
  return absl::OkStatus();
}

void LintWaiver::RegexToLines(std::string_view contents,
                              const LineColumnMap &line_map) {
  for (const auto &rule : waiver_re_map_) {
    for (const RE2 *re : rule.second) {
      std::string_view walk = contents;
      std::string_view match;
      while (RE2::FindAndConsume(&walk, *re, &match)) {
        const size_t pos = match.begin() - contents.begin();
        WaiveOneLine(rule.first, line_map.LineAtOffset(pos));
        if (match.empty()) {
          if (match.end() == contents.end()) break;
          walk = contents.substr(pos + 1);
        }
      }
    }
  }
}

bool LintWaiver::RuleIsWaivedOnLine(std::string_view rule_name,
                                    int line_number) const {
  const auto *line_set = verible::container::FindOrNull(waiver_map_, rule_name);
  return line_set != nullptr && LineNumberSetContains(*line_set, line_number);
}

bool LintWaiver::Empty() const {
  for (const auto &rule_waiver : waiver_map_) {
    if (!rule_waiver.second.empty()) {
      return false;
    }
  }
  return true;
}

std::string_view LintWaiverBuilder::ExtractWaivedRuleFromComment(
    std::string_view comment_text,
    std::vector<std::string_view> *comment_tokens) const {
  // Look for directives of the form: <tool_name> <directive> <rule_name>
  // Addition text beyond the last argument is ignored, so it could
  // contain more comment text.
  auto &tokens = *comment_tokens;
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

void LintWaiverBuilder::ProcessLine(const TokenRange &tokens, int line_number) {
  // TODO(fangism): [optimization] Use a SmallVector, or function-local
  // static to avoid re-allocation in every call.  This method does not
  // need to be re-entrant.
  std::vector<std::string_view> lint_directives;

  // Determine whether line is blank, where whitespace still counts as blank.
  const bool line_is_blank =
      std::all_of(tokens.begin(), tokens.end(), is_token_whitespace_);
  if (line_is_blank) {
    unapplied_oneline_waivers_.clear();
    return;
  }

  // Determine whether line contains any non-space, non-comment tokens.
  const bool line_has_tokens =
      std::any_of(tokens.begin(), tokens.end(), [this](const TokenInfo &t) {
        return !(is_token_whitespace_(t) || is_token_comment_(t));
      });

  if (line_has_tokens) {
    // Apply un-applied one-line waivers, and then reset them.
    for (const auto &rule : unapplied_oneline_waivers_) {
      lint_waiver_.WaiveOneLine(rule, line_number);
    }
    unapplied_oneline_waivers_.clear();
  }

  // Find all directives on this line.
  std::vector<std::string_view> comment_tokens;  // Re-use in loop.
  for (const auto &token : tokens) {
    if (is_token_comment_(token)) {
      // Lex the comment text.
      const std::string_view comment_text =
          StripCommentAndSpacePadding(token.text());
      comment_tokens = absl::StrSplit(comment_text, ' ', absl::SkipEmpty());
      // TODO(fangism): Support different waiver lexers.
      const std::string_view waived_rule =
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
    const TextStructureView &text_structure) {
  const int total_lines = text_structure.Lines().size();
  const auto &tokens = text_structure.TokenStream();
  for (int i = 0; i < total_lines; ++i) {
    const auto token_range = text_structure.TokenRangeOnLine(i);
    const int begin_dist = std::distance(tokens.begin(), token_range.begin());
    const int end_dist = std::distance(tokens.begin(), token_range.end());
    CHECK_LE(0, begin_dist);
    CHECK_LE(begin_dist, end_dist);
    CHECK_LE(end_dist, static_cast<int>(tokens.size()));
    ProcessLine(token_range, i);
  }

  // Apply regex waivers
  lint_waiver_.RegexToLines(text_structure.Contents(),
                            text_structure.GetLineColumnMap());

  // Flush out any remaining open-ranges, so that those waivers take effect
  // until the end-of-file.
  // TODO(b/78064145): Detect these as suspiciously unbalanced waiver uses.
  for (const auto &open_range : waiver_open_ranges_) {
    lint_waiver_.WaiveLineRange(open_range.first, open_range.second,
                                total_lines);
  }
  waiver_open_ranges_.clear();
}

template <typename... T>
static std::string WaiveCommandErrorFmt(LineColumn pos,
                                        std::string_view filename,
                                        std::string_view msg,
                                        const T &...args) {
  return absl::StrCat(filename, ":", pos.line + 1, ":", pos.column + 1,
                      ": command error: ", msg, args...);
}

template <typename... T>
static absl::Status WaiveCommandError(LineColumn pos, std::string_view filename,
                                      std::string_view msg, const T &...args) {
  return absl::InvalidArgumentError(
      WaiveCommandErrorFmt(pos, filename, msg, args...));
}

static absl::Status WaiveCommandHandler(
    const TokenRange &tokens, std::string_view waive_file,
    std::string_view waive_content, std::string_view lintee_filename,
    const LineColumnMap &line_map, LintWaiver *waiver,
    const std::set<std::string_view> &active_rules) {
  std::string_view rule;

  std::string_view option;
  std::string_view val;

  int line_start = -1;
  int line_end = -1;
  std::string regex;

  bool can_use_regex = false;
  bool can_use_lineno = false;
  bool location_match = true;

  LineColumn token_pos;
  LineColumn regex_token_pos = {};

  for (const auto &token : tokens) {
    token_pos =
        line_map.GetLineColAtOffset(waive_content, token.left(waive_content));

    switch (token.token_enum()) {
      case CommandFileLexer::ConfigToken::kCommand:
        // Verify that this command is supported by this handler
        if (token.text() != "waive") {
          return absl::InvalidArgumentError("Invalid command handler called");
        }
        break;
      case CommandFileLexer::ConfigToken::kError:
        return WaiveCommandError(token_pos, waive_file, "Configuration error");
      case CommandFileLexer::ConfigToken::kParam:
      case CommandFileLexer::ConfigToken::kFlag:
        return WaiveCommandError(token_pos, waive_file,
                                 "Unsupported argument: ", token.text());
      case CommandFileLexer::ConfigToken::kFlagWithArg:
        option = token.text();
        break;
      case CommandFileLexer::ConfigToken::kArg:

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
          if (range != std::string_view::npos) {
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
          }
          if (line_start > line_end) {
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
          const RE2 file_match_regex(val);
          if (!file_match_regex.ok()) {
            return WaiveCommandError(token_pos, waive_file,
                                     "--location regex is invalid");
          }
          location_match = RE2::PartialMatch(lintee_filename, file_match_regex);
          continue;
        }

        return WaiveCommandError(token_pos, waive_file,
                                 "Unsupported flag: ", option);

      case CommandFileLexer::ConfigToken::kNewline:
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
          if (auto status = waiver->WaiveWithRegex(rule, regex); !status.ok()) {
            return WaiveCommandError(regex_token_pos, waive_file,
                                     status.message());
          }
        }

        if (can_use_lineno) {
          waiver->WaiveLineRange(rule, line_start - 1, line_end);
        }

        if (!can_use_regex && !can_use_lineno) {
          absl::StatusOr<std::string> content_or =
              verible::file::GetContentAsString(lintee_filename);
          if (!content_or.ok()) {
            return WaiveCommandError(token_pos, waive_file,
                                     content_or.status().ToString());
          }

          const size_t number_of_lines =
              std::count(content_or->begin(), content_or->end(), '\n');
          waiver->WaiveLineRange(rule, 1, number_of_lines);
        }

        return absl::OkStatus();
      case CommandFileLexer::ConfigToken::kComment:
        /* Ignore comments */
        break;
      default:
        return WaiveCommandError(token_pos, waive_file, "Expecting arguments");
    }
  }

  return absl::OkStatus();
}

using HandlerFun = std::function<absl::Status(
    const TokenRange &, std::string_view waive_file,
    std::string_view waive_content, std::string_view lintee_filename,
    const LineColumnMap &, LintWaiver *, const std::set<std::string_view> &)>;
static const std::map<std::string_view, HandlerFun> &GetCommandHandlers() {
  // allocated once, never freed
  static const auto *handlers = new std::map<std::string_view, HandlerFun>{
      // Right now, we only have one handler
      {"waive", WaiveCommandHandler},
  };
  return *handlers;
}

absl::Status LintWaiverBuilder::ApplyExternalWaivers(
    const std::set<std::string_view> &active_rules,
    std::string_view lintee_filename, std::string_view waiver_filename,
    std::string_view waivers_config_content) {
  if (waivers_config_content.empty()) {
    return {absl::StatusCode::kInternal, "Broken waiver config handle"};
  }

  CommandFileLexer lexer(waivers_config_content);
  const LineColumnMap line_map(waivers_config_content);
  LineColumn command_pos;

  const auto &handlers = GetCommandHandlers();

  std::vector<TokenRange> commands = lexer.GetCommandsTokenRanges();

  bool all_commands_ok = true;
  for (const auto &c_range : commands) {
    const auto command = make_container_range(c_range.begin(), c_range.end());

    command_pos = line_map.GetLineColAtOffset(
        waivers_config_content, command.begin()->left(waivers_config_content));

    if (command[0].token_enum() == CommandFileLexer::ConfigToken::kComment) {
      continue;
    }

    // The very first Token in 'command' should be an actual command
    if (command.empty() ||
        command[0].token_enum() != CommandFileLexer::ConfigToken::kCommand) {
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
  }
  return absl::InvalidArgumentError("Errors applying external waivers.");
}

}  // namespace verible
