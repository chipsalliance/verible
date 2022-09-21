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

#include "verilog/analysis/verilog_filelist.h"

#include <iostream>
#include <sstream>
#include <string>
#include <vector>

#include "absl/status/status.h"
#include "absl/strings/match.h"
#include "absl/strings/str_split.h"
#include "absl/strings/strip.h"
#include "common/util/file_util.h"
#include "common/util/iterator_range.h"

namespace verilog {
void FileList::Append(const FileList& other) {
  file_paths.insert(file_paths.end(), other.file_paths.begin(),
                    other.file_paths.end());
  preprocessing.include_dirs.insert(preprocessing.include_dirs.end(),
                                    other.preprocessing.include_dirs.begin(),
                                    other.preprocessing.include_dirs.end());
  preprocessing.defines.insert(preprocessing.defines.end(),
                               other.preprocessing.defines.begin(),
                               other.preprocessing.defines.end());
}

FileList ParseSourceFileList(absl::string_view file_list_path,
                             const std::string& file_list_content) {
  // TODO(hzeller): parse +define+ and stash into preprocessing configuration.
  constexpr absl::string_view kIncludeDirPrefix = "+incdir+";
  FileList file_list_out;
  file_list_out.file_list_path = std::string(file_list_path);
  file_list_out.preprocessing.include_dirs.push_back(".");
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
      file_list_out.preprocessing.include_dirs.emplace_back(
          absl::StripPrefix(file_path, kIncludeDirPrefix));
    } else {
      // A regular file
      file_list_out.file_paths.push_back(file_path);
    }
  }
  return file_list_out;
}

absl::StatusOr<FileList> ParseSourceFileListFromFile(
    absl::string_view file_list_file) {
  std::string content;
  const auto read_status = verible::file::GetContents(file_list_file, &content);
  if (!read_status.ok()) return read_status;
  return ParseSourceFileList(file_list_file, content);
}

absl::StatusOr<FileList> ParseSourceFileListFromCommandline(
    const std::vector<absl::string_view>& cmdline) {
  FileList result;
  for (absl::string_view argument : cmdline) {
    if (argument.empty()) continue;
    if (argument[0] != '+') {
      // Then "argument" is a SV file name.
      result.file_paths.push_back(std::string(argument));
      continue;
    }
    // It should be either a define or incdir.
    std::vector<absl::string_view> argument_plus_splitted =
        absl::StrSplit(argument, absl::ByChar('+'), absl::SkipEmpty());
    if (argument_plus_splitted.size() < 2) {
      return absl::InvalidArgumentError(
          absl::StrCat("ERROR: Expected either '+define+' or '+incdir+' "
                       "followed by the parameter but got '",
                       argument, "'"));
    }
    absl::string_view plus_argument_type = argument_plus_splitted[0];
    if (plus_argument_type == "define") {
      for (const absl::string_view define_argument :
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
        result.preprocessing.defines.emplace_back(macro_pair.first,
                                                  macro_pair.second);
      }
    } else if (plus_argument_type == "incdir") {
      for (const absl::string_view incdir_argument :
           verible::make_range(argument_plus_splitted.begin() + 1,
                               argument_plus_splitted.end())) {
        // argument_plus_splitted[0] is 'incdir' so it is safe to skip it.
        result.preprocessing.include_dirs.emplace_back(
            std::string(incdir_argument));
      }
    } else {
      return absl::InvalidArgumentError(absl::StrCat(
          "ERROR: Expected either '+define+' or '+incdir+' but got '",
          plus_argument_type, "'"));
    }
  }
  return result;
}
}  // namespace verilog
