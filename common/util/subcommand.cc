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

#include "common/util/subcommand.h"

#include <iostream>

#include "absl/status/status.h"
#include "absl/strings/str_cat.h"
#include "absl/strings/str_join.h"

namespace verible {

SubcommandRegistry::SubcommandRegistry()
    : subcommand_map_{
          // Every registry comes initialized with its own generic 'help'
          // command.
          {"help",
           {[this](const SubcommandArgsRange& args, std::istream& ins,
                   std::ostream& outs,
                   std::ostream& errs) { return Help(args, ins, outs, errs); },
            "help [command]\n"
            "Prints command help.  "
            "With no command or unknown command, this lists available "
            "commands.\n"}},

          {"error",
           {[this](const SubcommandArgsRange& args, std::istream& ins,
                   std::ostream& outs, std::ostream& errs) {
              const auto unused_status = Help(args, ins, outs, errs);
              return absl::InvalidArgumentError("Unknown subcommand.");
            },
            "same as 'help', but exits non-zero to signal a user-error\n",
            false}},
      } {}

const SubcommandEntry& SubcommandRegistry::GetSubcommandEntry(
    absl::string_view command) const {
  const SubcommandMap& commands(subcommand_map_);
#if __cplusplus >= 201402L
  const auto iter = commands.find(command);  // heterogenous lookup
#else
  // without heterogenous lookup
  const auto iter = commands.find(std::string(command.begin(), command.end()));
#endif
  if (iter == commands.end()) {
    // Command not found, print help and exit non-zero.
    return commands.find("error")->second;
  }
  return iter->second;
}

absl::Status SubcommandRegistry::RegisterCommand(
    absl::string_view name, const SubcommandEntry& command) {
  const auto p = subcommand_map_.emplace(
#if __cplusplus >= 201402L
      name,  // heterogenous lookup
#else
      std::string(name.begin(), name.end()),  // without heterogenous lookup
#endif
      command);
  if (!p.second) {
    return absl::InvalidArgumentError(absl::StrCat(
        "A function named \"", name, "\" has already been registered."));
  }
  return absl::OkStatus();
}

absl::Status SubcommandRegistry::Help(const SubcommandArgsRange& args,
                                      std::istream&, std::ostream&,
                                      std::ostream& errs) const {
  if (args.empty()) {
    errs << "available commands:\n" << ListCommands() << std::endl;
    return absl::OkStatus();
  }

  const SubcommandEntry& entry = GetSubcommandEntry(args.front());
  errs << entry.usage << std::endl;
  return absl::OkStatus();
}

std::string SubcommandRegistry::ListCommands() const {
  const SubcommandMap& commands(subcommand_map_);
  std::vector<absl::string_view> public_commands;
  for (const auto& command : commands) {
    // List only public commands.
    if (command.second.show_in_help) {
      public_commands.emplace_back(command.first);
    }
  }
  return absl::StrCat("  ", absl::StrJoin(public_commands, "\n  "), "\n");
}

}  // namespace verible
