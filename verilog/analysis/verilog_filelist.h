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
  // Path to the file list.
  std::string file_list_path;
  // Information relevant to the preprocessor.
  PreprocessingInfo preprocessing;
};

}  // namespace verilog
#endif  // VERIBLE_VERILOG_ANALYSIS_VERILOG_FILELIST_H_
