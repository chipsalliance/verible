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

#ifndef VERIBLE_COMMON_UTIL_SUBCOMMAND_H_
#define VERIBLE_COMMON_UTIL_SUBCOMMAND_H_

#include <functional>
#include <iosfwd>
#include <map>
#include <string>
#include <utility>
#include <vector>

#include "absl/status/status.h"
#include "absl/strings/string_view.h"
#include "verible/common/strings/compare.h"
#include "verible/common/util/container-iterator-range.h"

namespace verible {

// This should be the same type returned by InitCommandLine in
// common/util/init_command_line.h
using SubcommandArgs = std::vector<absl::string_view>;
using SubcommandArgsIterator = SubcommandArgs::const_iterator;
using SubcommandArgsRange =
    verible::container_iterator_range<SubcommandArgsIterator>;

// Currently this function type is hard-coded, but this could eventually
// become a template parameter.
using SubcommandFunction =
    std::function<absl::Status(const SubcommandArgsRange &, std::istream &ins,
                               std::ostream &outs, std::ostream &errs)>;

// Represents a function selected by the user.
struct SubcommandEntry {
  SubcommandEntry(SubcommandFunction fun, absl::string_view usage,
                  bool show_in_help = true)
      : main(std::move(fun)), usage(usage), show_in_help(show_in_help) {}

  // sub-main function
  const SubcommandFunction main;

  // maybe a const std::string 'brief' for a half-line description

  // full description of command
  const std::string usage;

  // If true, display this subcommand in 'help'.
  const bool show_in_help;
};

// SubcommandRegistry is a structure for holding a map of functions.
class SubcommandRegistry {
 public:
  SubcommandRegistry();

  // Not intended for copy/move-ing.
  SubcommandRegistry(const SubcommandRegistry &) = delete;
  SubcommandRegistry(SubcommandRegistry &&) = delete;
  SubcommandRegistry &operator=(const SubcommandRegistry &) = delete;
  SubcommandRegistry &operator=(SubcommandRegistry &&) = delete;

  // Add a function to this map.
  // Returned status is an error if a function already exists with the given
  // name.
  absl::Status RegisterCommand(absl::string_view name, const SubcommandEntry &);

  // Lookup a function in this map by name.
  const SubcommandEntry &GetSubcommandEntry(absl::string_view command) const;

  // Print a help summary of all registered commands.
  std::string ListCommands() const;

 protected:
  // Every command map comes with a built-in 'help' command.
  absl::Status Help(const SubcommandArgsRange &args, std::istream &,
                    std::ostream &, std::ostream &errs) const;

 private:
  using SubcommandMap =
      std::map<std::string, SubcommandEntry, StringViewCompare>;

  // The map of names to functions.
  SubcommandMap subcommand_map_;

  // Sequence in which commands have been registered for display purposes.
  std::vector<std::string> command_listing_;
};  // class SubcommandRegistry

}  // namespace verible

#endif  // VERIBLE_COMMON_UTIL_SUBCOMMAND_H_
