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

#include <functional>
#include <iostream>
#include <map>
#include <string>

#include "absl/flags/usage.h"
#include "absl/status/status.h"
#include "absl/strings/str_cat.h"
#include "absl/strings/str_join.h"
#include "absl/strings/string_view.h"
#include "common/strings/compare.h"
#include "common/strings/patch.h"
#include "common/util/container_iterator_range.h"
#include "common/util/file_util.h"
#include "common/util/init_command_line.h"

// TODO(fangism): refactor into common/utils/ for other subcommand-using tools
using SubcommandArgs = std::vector<char*>;
using SubcommandArgsIterator = SubcommandArgs::const_iterator;
using SubcommandArgsRange =
    verible::container_iterator_range<SubcommandArgsIterator>;

using SubcommandFunction =
    std::function<absl::Status(const SubcommandArgsRange&, std::istream& ins,
                               std::ostream& outs, std::ostream& errs)>;

// Represents a function selected by the user.
struct SubcommandEntry {
  // sub-main function
  const SubcommandFunction main;

  // maybe a const std::string 'brief' for a half-line description

  // full description of command
  const std::string usage;
};

using SubcommandMap =
    std::map<std::string, SubcommandEntry, verible::StringViewCompare>;

// forward declarations of subcommand functions
static absl::Status ChangedLines(const SubcommandArgsRange&, std::istream&,
                                 std::ostream&, std::ostream&);
static absl::Status Help(const SubcommandArgsRange&, std::istream&,
                         std::ostream&, std::ostream&);

static absl::Status Error(const SubcommandArgsRange& args, std::istream& ins,
                          std::ostream& outs, std::ostream& errs) {
  const auto unused_status = Help(args, ins, outs, errs);
  return absl::InvalidArgumentError("Unknown subcommand.");
}

static const SubcommandMap& GetSubcommandMap() {
  static const auto* kCommands = new SubcommandMap{
      {"help",
       {&Help,
        "help [command]\n"
        "Prints command help.  "
        "With no command or unknown command, this lists available "
        "commands.\n"}},

      // TODO(fangism): this should be a hidden command
      {"error",
       {&Error, "same as 'help', but exits non-zero to signal a user-error\n"}},

      {"changed-lines", {&ChangedLines, R"(changed-lines patchfile

Input:
  'patchfile' is a unified-diff file from 'diff -u' or other version-controlled
  equivalents like {p4,git,hg,cvs,svn} diff.  Use '-' to read from stdin.

Output: (stdout)
  This prints output in the following format per line:

    filename [line-ranges]

  where line-ranges (optional) is suitable for tools that accept a set of lines
  to operate on, e.g. "1-4,8,21-42".
  line-ranges is omitted for files that are considered new in the patchfile.
)"}},

      // TODO(b/156530527): "apply-pick" interactive mode
  };
  return *kCommands;
}

absl::Status ChangedLines(const SubcommandArgsRange& args, std::istream& ins,
                          std::ostream& outs, std::ostream& errs) {
  if (args.empty()) {
    return absl::InvalidArgumentError(
        "Missing patchfile argument.  Use '-' for stdin.");
  }
  const auto patchfile = args[0];
  std::string patch_contents;
  {
    const auto status = verible::file::GetContents(patchfile, &patch_contents);
    if (!status.ok()) return status;
  }
  verible::PatchSet patch_set;
  {
    const auto status = patch_set.Parse(patch_contents);
    if (!status.ok()) return status;
  }
  const verible::FileLineNumbersMap changed_lines(
      patch_set.AddedLinesMap(false));
  for (const auto& file_lines : changed_lines) {
    outs << file_lines.first;
    if (!file_lines.second.empty()) {
      file_lines.second.FormatInclusive(outs << ' ', true);
    }
    outs << std::endl;
  }
  return absl::OkStatus();
}

static std::string ListCommands() {
  static const SubcommandMap& commands(GetSubcommandMap());
  return absl::StrCat("  ",
                      absl::StrJoin(commands, "\n  ",
                                    [](std::string* out,
                                       const SubcommandMap::value_type& entry) {
                                      *out += entry.first;  // command name
                                    }),
                      "\n");
}

static const SubcommandEntry& GetSubcommandEntry(absl::string_view command) {
  static const SubcommandMap& commands(GetSubcommandMap());
#if __cplusplus >= 201402L
  const auto iter = commands.find(command);  // heterogenous lookup
#else
  // without hetergenous lookup
  const auto iter = commands.find(std::string(command));
#endif
  if (iter == commands.end()) {
    // Command not found, print help and exit non-zero.
    return commands.find("error")->second;
  }
  return iter->second;
}

absl::Status Help(const SubcommandArgsRange& args, std::istream&, std::ostream&,
                  std::ostream& errs) {
  if (args.empty()) {
    errs << "available commands:\n" << ListCommands() << std::endl;
    return absl::OkStatus();
  }

  const SubcommandEntry& entry = GetSubcommandEntry(args.front());
  errs << entry.usage << std::endl;
  return absl::OkStatus();
}

int main(int argc, char* argv[]) {
  const std::string usage = absl::StrCat("usage: ", argv[0],
                                         " command args...\n"
                                         "available commands:\n",
                                         ListCommands());

  const auto args = verible::InitCommandLine(usage, &argc, &argv);
  if (args.size() == 1) {
    std::cerr << absl::ProgramUsageMessage() << std::endl;
    return 1;
  }
  // args[0] is the program name
  // args[1] is the subcommand
  // subcommand args start at [2]
  const SubcommandArgsRange command_args(args.cbegin() + 2, args.cend());

  const auto& sub = GetSubcommandEntry(args[1]);
  // Run the subcommand.
  const auto status = sub.main(command_args, std::cin, std::cout, std::cerr);
  if (!status.ok()) {
    std::cerr << status.message() << std::endl;
    return 1;
  }
  return 0;
}
