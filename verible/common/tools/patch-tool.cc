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

#include <cstddef>
#include <iostream>
#include <string>
#include <string_view>
#include <utility>

#include "absl/flags/usage.h"
#include "absl/status/status.h"
#include "absl/status/statusor.h"
#include "absl/strings/str_cat.h"
#include "verible/common/strings/patch.h"
#include "verible/common/util/file-util.h"
#include "verible/common/util/init-command-line.h"
#include "verible/common/util/status-macros.h"
#include "verible/common/util/subcommand.h"
#include "verible/common/util/user-interaction.h"

using verible::SubcommandArgsRange;
using verible::SubcommandEntry;

static absl::Status ChangedLines(const SubcommandArgsRange &args,
                                 std::istream &ins, std::ostream &outs,
                                 std::ostream &errs) {
  if (args.empty()) {
    return absl::InvalidArgumentError(
        "Missing patchfile argument.  Use '-' for stdin.");
  }
  const std::string_view patchfile = args[0];
  auto patch_content_or = verible::file::GetContentAsString(patchfile);
  if (!patch_content_or.ok()) return patch_content_or.status();

  verible::PatchSet patch_set;
  RETURN_IF_ERROR(patch_set.Parse(*patch_content_or));

  const verible::FileLineNumbersMap changed_lines(
      patch_set.AddedLinesMap(false));
  for (const auto &file_lines : changed_lines) {
    outs << file_lines.first;
    if (!file_lines.second.empty()) {
      file_lines.second.FormatInclusive(outs << ' ', true);
    }
    outs << std::endl;
  }
  return absl::OkStatus();
}

static absl::Status ApplyPick(const SubcommandArgsRange &args,
                              std::istream &ins, std::ostream &outs,
                              std::ostream &errs) {
  if (args.empty()) {
    return absl::InvalidArgumentError("Missing patchfile argument.");
  }
  const std::string_view patchfile = args[0];
  absl::StatusOr<std::string> patch_contents_or;
  patch_contents_or = verible::file::GetContentAsString(patchfile);
  if (!patch_contents_or.ok()) return patch_contents_or.status();

  verible::PatchSet patch_set;
  RETURN_IF_ERROR(patch_set.Parse(*patch_contents_or));

  return patch_set.PickApplyInPlace(ins, outs);
}

static absl::Status StdinTest(const SubcommandArgsRange &args,
                              std::istream &ins, std::ostream &outs,
                              std::ostream &errs) {
  constexpr size_t kOpenLimit = 10;
  outs << "This is a demo of re-opening std::cin, up to " << kOpenLimit
       << " times.\n"
          "Enter text when prompted.\n"
          "Ctrl-D sends an EOF to start the next file.\n"
          "Ctrl-C terminates the loop and exits the program."
       << std::endl;
  size_t file_count = 0;
  std::string line;
  for (; file_count < kOpenLimit; ++file_count) {
    outs << "==== file " << file_count << " ====" << std::endl;
    while (ins) {
      if (verible::IsInteractiveTerminalSession(outs)) outs << "enter text: ";
      std::getline(ins, line);
      outs << "echo: " << line << std::endl;
    }
    if (ins.eof()) {
      outs << "EOF reached.  Re-opening for next file" << std::endl;
      ins.clear();  // allows std::cin to read the next file
    }
  }
  return absl::OkStatus();
}

static absl::Status CatTest(const SubcommandArgsRange &args, std::istream &ins,
                            std::ostream &outs, std::ostream &errs) {
  size_t file_count = 0;
  for (const auto &arg : args) {
    const absl::StatusOr<std::string> contents_or =
        verible::file::GetContentAsString(arg);
    if (!contents_or.ok()) return contents_or.status();

    outs << "<<<< contents of file[" << file_count << "] (" << arg << ") <<<<"
         << std::endl;
    outs << *contents_or;
    outs << ">>>> end of file[" << file_count << "] (" << arg << ") >>>>"
         << std::endl;
    ++file_count;
  }
  return absl::OkStatus();
}

static const std::pair<std::string_view, SubcommandEntry> kCommands[] = {
    {"changed-lines",  //
     {&ChangedLines,   //
      R"(changed-lines patchfile

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

    // TODO(b/156530527): add options like -p for path pruning
    // TODO(fangism): Support '-' as patchfile.
    {"apply-pick",
     {&ApplyPick,
      R"(apply-pick patchfile
Input:
'patchfile' is a unified-diff file from 'diff -u' or other version-controlled
equivalents like {p4,git,hg,cvs,svn} diff.

Effect:
Modifies patched files in-place, following user selections on which patch
hunks to apply.
)"}},

    {"stdin-test",  //
     {&StdinTest,   //
      R"(Test for re-opening stdin.

This interactivel prompts the user to enter text, separating files with an EOF
(Ctrl-D), and echoes the input text back to stdout.
)",
      false}},
    {"cat-test",  //
     {&CatTest,   //
      R"(Test for (UNIX) cat-like functionality.

Usage: cat-test ARGS...

where each ARG could point to a file on the filesystem or be '-' to read from stdin.
Each '-' will prompt the user to enter text until EOF (Ctrl-D).
Each 'file' echoed back to stdout will be enclosed in banner lines with
<<<< and >>>>.
)",
      false}},
};

int main(int argc, char *argv[]) {
  // Create a registry of subcommands (locally, rather than as a static global).
  verible::SubcommandRegistry commands;
  for (const auto &entry : kCommands) {
    const auto status = commands.RegisterCommand(entry.first, entry.second);
    if (!status.ok()) {
      std::cerr << status.message() << std::endl;
      return 2;  // fatal error
    }
  }

  const std::string usage = absl::StrCat("usage: ", argv[0],
                                         " command args...\n"
                                         "available commands:\n",
                                         commands.ListCommands());

  // Process invocation args.
  const auto args = verible::InitCommandLine(usage, &argc, &argv);
  if (args.size() == 1) {
    std::cerr << absl::ProgramUsageMessage() << std::endl;
    return 1;
  }
  // args[0] is the program name
  // args[1] is the subcommand
  // subcommand args start at [2]
  const SubcommandArgsRange command_args(args.cbegin() + 2, args.cend());

  const auto &sub = commands.GetSubcommandEntry(args[1]);
  // Run the subcommand.
  const auto status = sub.main(command_args, std::cin, std::cout, std::cerr);
  if (!status.ok()) {
    std::cerr << status.message() << std::endl;
    return 1;
  }
  return 0;
}
