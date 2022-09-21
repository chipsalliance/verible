// Copyright 2017-2022 The Verible Authors.
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

#ifndef VERIBLE_VERILOG_ANALYSIS_VERILOG_FILELIST_H_
#define VERIBLE_VERILOG_ANALYSIS_VERILOG_FILELIST_H_

#include <string>
#include <vector>

#include "absl/status/statusor.h"
#include "absl/strings/string_view.h"

namespace verilog {

// TODO(karimtera): Using "MacroDefiniton" struct might be better.
struct TextMacroDefinition {
  TextMacroDefinition(std::string name, std::string value)
      : name(std::move(name)), value(std::move(value)){};
  std::string name;
  std::string value;
  bool operator==(const TextMacroDefinition& macro_definition) const {
    return name == macro_definition.name && value == macro_definition.value;
  }
};

// File list for compiling a System Verilog project.
// TODO: ideally, all the strings would be string_views, but we need to make
//       sure to have the relevant backing store live.
// TODO: are there cases in which files and incdirs are interleaved so that
//       the first files don't see all the incdirs yet, but after more incdirs
//       are added, all of them are relevant ? If so, this would require some
//       restructering.
// TODO: document if files are relative to tool invocation or relative to
//       file_list_path (the latter makes more sense, but I think currently that
//       is underspecified).
// TODO: Alongside previous: also introduce file_list_root field ?
struct FileList {
  // A struct holding information relevant to "VerilogPreprocess" preprocessor.
  struct PreprocessingInfo {
    // Directories where to search for the included files.
    std::vector<std::string> include_dirs;

    // Defined macros.
    std::vector<TextMacroDefinition> defines;
  };

  // Path to the file list.
  std::string file_list_path;

  // Ordered list of files to compile.
  std::vector<std::string> file_paths;

  // Information relevant to the preprocessor.
  PreprocessingInfo preprocessing;

  // Merge other file list into this one, essentially concatenating all
  // vectors of information at the end.
  // Modifies this FileList except "file_list_path", which is left untouched.
  void Append(const FileList& other);
};

// Reads in a list of files line-by-line from 'file_list_file'. The include
// directories are prefixed by "+incdir+"
absl::StatusOr<FileList> ParseSourceFileListFromFile(
    absl::string_view file_list_file);

// Reads in a list of files line-by-line from the given string. The include
// directories are prefixed by "+incdir+"
FileList ParseSourceFileList(absl::string_view file_list_path,
                             const std::string& file_list_content);

// Parse positional parameters from command line and extract files, +incdir+ and
// +define+.
absl::StatusOr<FileList> ParseSourceFileListFromCommandline(
    const std::vector<absl::string_view>& cmdline);

}  // namespace verilog
#endif  // VERIBLE_VERILOG_ANALYSIS_VERILOG_FILELIST_H_
