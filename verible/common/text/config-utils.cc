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

#include "verible/common/text/config-utils.h"

#include <algorithm>
#include <cstdint>
#include <initializer_list>
#include <iterator>
#include <limits>
#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "absl/status/status.h"
#include "absl/strings/ascii.h"
#include "absl/strings/match.h"
#include "absl/strings/numbers.h"
#include "absl/strings/str_cat.h"
#include "absl/strings/str_join.h"
#include "absl/strings/str_split.h"
#include "absl/strings/string_view.h"
#include "re2/re2.h"
#include "verible/common/util/logging.h"

namespace verible {
using absl::string_view;
using config::NVConfigSpec;

// TODO(hzeller): consider using flex for a more readable tokenization that
// can also much easier deal with whitespaces, strings etc.
absl::Status ParseNameValues(string_view config_string,
                             const std::initializer_list<NVConfigSpec> &spec) {
  if (config_string.empty()) return absl::OkStatus();

  for (const string_view single_config :
       absl::StrSplit(config_string, ';', absl::SkipEmpty())) {
    const std::pair<string_view, string_view> nv_pair =
        absl::StrSplit(single_config, ':');
    const auto value_config = std::find_if(  // linear search
        spec.begin(), spec.end(),
        [&nv_pair](const NVConfigSpec &s) { return nv_pair.first == s.name; });
    if (value_config == spec.end()) {
      std::string available;
      for (const auto &s : spec) {
        if (!available.empty()) available.append(", ");
        available.append("'").append(s.name).append("'");
      }
      const bool plural = spec.size() > 1;
      return absl::InvalidArgumentError(absl::StrCat(
          nv_pair.first, ": unknown parameter; supported ",
          (plural ? "parameters are " : "parameter is "), available));
    }
    if (!value_config->set_value) return absl::OkStatus();  // consume, not use.
    absl::Status result = value_config->set_value(nv_pair.second);
    if (!result.ok()) {
      // We always prepend the parameter name first in the message for
      // diagnostic usefulness of the error message.
      // Also, this way, the functor can just worry about parsing and does
      // not need to know the name of the value.
      return absl::InvalidArgumentError(
          absl::StrCat(nv_pair.first, ": ", result.message()));
    }
  }
  return absl::OkStatus();
}

namespace config {
ConfigValueSetter SetInt(int *value, int minimum, int maximum) {
  CHECK(value) << "Must provide pointer to integer to store.";
  return [value, minimum, maximum](string_view v) {
    int parsed_value;
    if (!absl::SimpleAtoi(v, &parsed_value)) {
      return absl::InvalidArgumentError(
          absl::StrCat("'", v, "': Cannot parse integer"));
    }
    if (parsed_value < minimum || parsed_value > maximum) {
      return absl::InvalidArgumentError(absl::StrCat(
          parsed_value, " out of range [", minimum, "...", maximum, "]"));
    }
    *value = parsed_value;
    return absl::OkStatus();
  };
}

ConfigValueSetter SetInt(int *value) {
  return SetInt(value, std::numeric_limits<int>::min(),
                std::numeric_limits<int>::max());
}

ConfigValueSetter SetBool(bool *value) {
  CHECK(value) << "Must provide pointer to boolean to store.";
  return [value](string_view v) {
    // clang-format off
    if (v.empty() || v == "1"
        || absl::EqualsIgnoreCase(v, "true")
        || absl::EqualsIgnoreCase(v, "on")) {
      *value = true;
      return absl::OkStatus();
    }
    if (v == "0"
        || absl::EqualsIgnoreCase(v, "false")
        || absl::EqualsIgnoreCase(v, "off")) {
      *value = false;
      return absl::OkStatus();
    }
    // clange-format on

    // We accept 1 and 0 silently to encourage somewhat
    // consistent use of values.
    return absl::InvalidArgumentError("Boolean value should be one of "
                                      "'true', 'on' or 'false', 'off'");
  };
}

// TODO(hzeller): For strings, consider to allow quoted strings that then also
// allow C-Escapes. But that would require that ParseNameValues() can properly
// deal with whitespace, which it might after converted to flex.

ConfigValueSetter SetString(std::string* value) {
  return [value](string_view v) {
           value->assign(v.data(), v.length());
           return absl::OkStatus();
         };
}

ConfigValueSetter SetStringOneOf(std::string* value,
                                 const std::vector<string_view>& allowed) {
  CHECK(value) << "Must provide pointer to string to store.";
  return [value, allowed](string_view v) {
           auto item = find(allowed.begin(), allowed.end(), v);
           if (item == allowed.end()) {
             // If the list only contains one element, provide a more suited
             // error message.
             if (allowed.size() == 1) {
               return absl::InvalidArgumentError(
                   absl::StrCat("Value can only be '", allowed[0], "'; got '",
                                v, "'"));
             }
             return absl::InvalidArgumentError(
                 absl::StrCat("Value can only be one of ['",
                              absl::StrJoin(allowed, "', '"),
                              "']; got '", v, "'"));
           }
           value->assign(v.data(), v.length());
           return absl::OkStatus();
         };
}

ConfigValueSetter SetNamedBits(
    uint32_t* value,
    const std::vector<absl::string_view>& choices) {
  CHECK(value) << "Must provide pointer to uint32_t to store.";
  CHECK_LE(choices.size(), 32) << "Too many choices for 32-bit bitmap";
  return [value, choices](string_view v) {
           uint32_t result = 0;
           for (auto bitname : absl::StrSplit(v, '|', absl::SkipWhitespace())) {
             bitname = absl::StripAsciiWhitespace(bitname);
             const auto item_pos = find_if(
                 choices.begin(), choices.end(),
                 [bitname](string_view c) {
                   return absl::EqualsIgnoreCase(bitname, c);
                 });
             if (item_pos == choices.end()) {
               return absl::InvalidArgumentError(
                   absl::StrCat("'", bitname,
                                "' is not in the available choices {",
                                absl::StrJoin(choices, ", "), "}"));
             }
             result |= (1 << (std::distance(choices.begin(), item_pos)));
           }
           // Parsed all bits successfully.
           *value = result;
           return absl::OkStatus();
         };
}

ConfigValueSetter SetRegex(std::unique_ptr<re2::RE2> *regex) {
  CHECK(regex) << "Must provide pointer to a RE2 to store.";
  return [regex](string_view v) {
    *regex = std::make_unique<re2::RE2>(v, re2::RE2::Quiet);
    if((*regex)->ok()) {
      return absl::OkStatus();
    }

    std::string error_msg =
          absl::StrCat("Failed to parse regular expression: ", (*regex)->error());
    return absl::InvalidArgumentError(error_msg);
  };
}

}  // namespace config
}  // namespace verible
