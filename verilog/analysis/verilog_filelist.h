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
#include <string_view>
#include <utility>
#include <vector>

#include "absl/status/status.h"

namespace verilog {

// TODO(karimtera): Using "MacroDefiniton" struct might be better.
struct TextMacroDefinition {
  TextMacroDefinition(std::string name, std::string value)
      : name(std::move(name)), value(std::move(value)){};
  std::string name;
  std::string value;

  bool operator==(const TextMacroDefinition &other) const {
    return name == other.name && value == other.value;
  }
};

// File list for compiling a System Verilog project.
// TODO: ideally, all the strings would be string_views, but we need to make
//       sure to have the relevant backing store live.
// TODO: are there cases in which files and incdirs are interleaved so that
//       the first files don't see all the incdirs yet, but after more incdirs
//       are added, all of them are relevant ? If so, this would require some
//       restructering.
// TODO: Introduce file_list_root field ?
struct FileList {
  // A struct holding information relevant to "VerilogPreprocess" preprocessor.
  struct PreprocessingInfo {
    // Directories where to search for the included files.
    std::vector<std::string> include_dirs;

    // Defined macros.
    std::vector<TextMacroDefinition> defines;
  };

  // Ordered list of files to compile.
  std::vector<std::string> file_paths;

  // Information relevant to the preprocessor.
  PreprocessingInfo preprocessing;

  // Returns the file list in Icarus Verilog format
  // (http://iverilog.wikia.com/wiki/Command_File_Format)
  std::string ToString() const;
};

// Reads in a list of files line-by-line from "file_list_file" and
// appends it to the given filelist.
// Sets the "file_list_path" in FileList.
// +incdir+ adds to include directories (TODO: +define+).
//
// TODO: Maybe remove this as it creates an ephemeral strings with the content.
// If we always use an externally owned string_view (see ..FromContent() below)
// we can replace std::string in FileList with string_views; that way, it is
// always possible to pinpoint back to the owning string view in case we want
// to provide detailed error messages.
absl::Status AppendFileListFromFile(std::string_view file_list_file,
                                    FileList *append_to);

// Reads in a list of files line-by-line from the given string. The include
// directories are prefixed by "+incdir+" (TODO: +define+)
absl::Status AppendFileListFromContent(std::string_view file_list_path,
                                       const std::string &file_list_content,
                                       FileList *append_to);

// Parse positional parameters from command line and extract files,
// +incdir+ and +define+ and appends to FileList.
// TODO: Also support --file_list_path (and -f), --file_list_root
absl::Status AppendFileListFromCommandline(
    const std::vector<std::string_view> &cmdline, FileList *append_to);

}  // namespace verilog
#endif  // VERIBLE_VERILOG_ANALYSIS_VERILOG_FILELIST_H_
