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
#ifndef VERIBLE_COMMON_TEXT_CONFIG_UTILS_H_
#define VERIBLE_COMMON_TEXT_CONFIG_UTILS_H_

#include <cstdint>
#include <functional>
#include <initializer_list>
#include <memory>
#include <string>
#include <string_view>
#include <vector>

#include "absl/status/status.h"
#include "re2/re2.h"

namespace verible {
namespace config {
using ConfigValueSetter = std::function<absl::Status(std::string_view)>;

struct NVConfigSpec {
  const char *name;
  ConfigValueSetter set_value;
};
}  // namespace config

// A utility function to parse name/value pairs from a string and directly
// set user values.
//
// The "config_string" is the string to parse. It contains a list
// of colon-separated name:value-pairs. These name/value pairs are seaprated
// by semicolon or newline. So 'panic:no; answer:42' would be a valid string.
// (We might adopt a more elaborate syntax in the future).
//
// For a parsed name/value pair, a setter associated with that name is called
// with the value.
//
// The caller defines what names to expect with the "spec"
// specification. This is a list of allowed names with respective setters.
// The setters are functors accepting the value and ingest it or return an
// error. For convenience, there are a few pre-defined setter-factories
// provided in the verible::config namespace.
//
// This allows for fairly compact code that can directly modify variables
// you're interested in with all the parse errors taken care of.
//
// Sample call:
// return ParseNameValues(configuration_string,
//                        {{"indentation", SetInt(&indentation_)},
//                         {"value_separator", SetString(&value_separator)}});
//
// This function returns with an error if the "config_string" contains a named
// parameter not found in "spec".
//
// If none of the called setters returns an error (which otherwise is returned),
// this function returns with an absl::OkStatus().
absl::Status ParseNameValues(
    std::string_view config_string,
    const std::initializer_list<config::NVConfigSpec> &spec);

namespace config {

// A few utility functions generating setters for ParseNameValues() allowing
// to parse values directly into variables.

ConfigValueSetter SetInt(int *value);

// Set an integer value and validate that it is in [minimum...maximum] range.
ConfigValueSetter SetInt(int *value, int minimum, int maximum);
ConfigValueSetter SetBool(bool *value);
ConfigValueSetter SetString(std::string *value);

// Set a string, but verify that it is only one of a limited set. The
// set is provided as vector for simplicity and to allow the caller to
// impose a particular importance order when returning an error.
ConfigValueSetter SetStringOneOf(std::string *value,
                                 const std::vector<std::string_view> &allowed);

// Set a bitmap from the given values, a '|'-separated list of named bits
// to be set. The bit-names provided in the configuration-string can come
// in any order and are not case sensitive.
//
// The sequence in the 'choices' array determines the bit to be set. So a bit
// named in choices[5] modifies (1<<5). Given the uint32 value, this allows
// up to 32 choices.
ConfigValueSetter SetNamedBits(uint32_t *value,
                               const std::vector<std::string_view> &choices);

// Set a Regex
ConfigValueSetter SetRegex(std::unique_ptr<re2::RE2> *regex);

}  // namespace config
}  // namespace verible

#endif  // VERIBLE_COMMON_TEXT_CONFIG_UTILS_H_
