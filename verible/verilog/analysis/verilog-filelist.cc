// Copyright 2022 The Verible Authors.
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

#include "verible/verilog/analysis/verilog-filelist.h"

#include <iosfwd>
#include <sstream>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include "absl/status/status.h"
#include "absl/strings/ascii.h"
#include "absl/strings/match.h"
#include "absl/strings/str_cat.h"
#include "absl/strings/str_split.h"
#include "absl/strings/strip.h"
#include "verible/common/util/file-util.h"
#include "verible/common/util/iterator-range.h"

namespace verilog {
absl::Status AppendFileListFromContent(std::string_view file_list_path,
                                       const std::string &file_list_content,
                                       FileList *append_to) {
  constexpr std::string_view kIncludeDirPrefix = "+incdir+";
  constexpr std::string_view kDefineMacroPrefix = "+define+";
  append_to->preprocessing.include_dirs.emplace_back(
      ".");  // Should we do that?
  std::string file_path;
  std::istringstream stream(file_list_content);
  while (std::getline(stream, file_path)) {
    absl::RemoveExtraAsciiWhitespace(&file_path);
    // Ignore blank lines
    if (file_path.empty()) continue;
    // Ignore "# ..." comments
    if (file_path.front() == '#') continue;
    // Ignore "// ..." comments
    if (absl::StartsWith(file_path, "//")) continue;

    if (absl::StartsWith(file_path, kIncludeDirPrefix)) {
      // Handle includes
      // TODO(karimtera): split directories by comma, to allow multiple dirs.
      append_to->preprocessing.include_dirs.emplace_back(
          absl::StripPrefix(file_path, kIncludeDirPrefix));
      continue;
    }

    if (absl::StartsWith(file_path, kDefineMacroPrefix)) {
      // Handle defines
      std::string_view definition =
          absl::StripPrefix(file_path, kDefineMacroPrefix);
      std::pair<std::string, std::string> define_value = absl::StrSplit(
          definition, absl::MaxSplits('=', 1), absl::SkipEmpty());
      if (!define_value.second.empty()) {
        append_to->preprocessing.defines.emplace_back(define_value.first,
                                                      define_value.second);
      }
      continue;
    }

    if (file_path[0] == '+' || file_path[0] == '-') {
      // Ignore unsupported parameter
      continue;
    }

    // A regular file
    append_to->file_paths.push_back(file_path);
  }
  return absl::OkStatus();
}

absl::Status AppendFileListFromFile(std::string_view file_list_file,
                                    FileList *append_to) {
  auto content_or = verible::file::GetContentAsString(file_list_file);
  if (!content_or.ok()) return content_or.status();
  return AppendFileListFromContent(file_list_file, *content_or, append_to);
}

absl::Status AppendFileListFromCommandline(
    const std::vector<std::string_view> &cmdline, FileList *append_to) {
  for (std::string_view argument : cmdline) {
    if (argument.empty()) continue;
    if (argument[0] != '+') {
      // Then "argument" is a SV file name.
      append_to->file_paths.push_back(std::string(argument));
      continue;
    }
    // It should be either a define or incdir.
    std::vector<std::string_view> argument_plus_splitted =
        absl::StrSplit(argument, absl::ByChar('+'), absl::SkipEmpty());
    if (argument_plus_splitted.size() < 2) {
      return absl::InvalidArgumentError(
          absl::StrCat("ERROR: Expected either '+define+' or '+incdir+' "
                       "followed by the parameter but got '",
                       argument, "'"));
    }
    std::string_view plus_argument_type = argument_plus_splitted[0];
    if (plus_argument_type == "define") {
      for (const std::string_view define_argument :
           verible::make_range(argument_plus_splitted.begin() + 1,
                               argument_plus_splitted.end())) {
        // argument_plus_splitted[0] is 'define' so it is safe to skip it.
        // define_argument is something like <macro1>=<value>.
        std::pair<std::string, std::string> macro_pair = absl::StrSplit(
            define_argument, absl::MaxSplits('=', 1), absl::SkipEmpty());
        if (absl::StrContains(define_argument, '=') &&
            macro_pair.second.empty()) {
          return absl::InvalidArgumentError(
              "ERROR: Expected '+define+<macro>[=<value>]', but '<value>' "
              "after '=' is missing");
        }
        // add the define argument.
        append_to->preprocessing.defines.emplace_back(macro_pair.first,
                                                      macro_pair.second);
      }
    } else if (plus_argument_type == "incdir") {
      for (const std::string_view incdir_argument :
           verible::make_range(argument_plus_splitted.begin() + 1,
                               argument_plus_splitted.end())) {
        // argument_plus_splitted[0] is 'incdir' so it is safe to skip it.
        append_to->preprocessing.include_dirs.emplace_back(
            std::string(incdir_argument));
      }
    } else {
      return absl::InvalidArgumentError(absl::StrCat(
          "ERROR: Expected either '+define+' or '+incdir+' but got '+",
          plus_argument_type, "+'"));
    }
  }
  return absl::OkStatus();
}

std::string FileList::ToString() const {
  std::stringstream buffer;
  for (const auto &definition : preprocessing.defines) {
    buffer << "+define+" << definition.name << "=" << definition.value << '\n';
  }
  for (std::string_view include : preprocessing.include_dirs) {
    buffer << "+incdir+" << include << '\n';
  }
  for (std::string_view path : file_paths) {
    buffer << path << '\n';
  }
  return buffer.str();
}

}  // namespace verilog
