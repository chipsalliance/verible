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

void verilog::FileList::Append(const FileList &other) {
  file_paths.insert(file_paths.end(), other.file_paths.begin(),
                    other.file_paths.end());
  preprocessing.include_dirs.insert(preprocessing.include_dirs.end(),
                                    other.preprocessing.include_dirs.begin(),
                                    other.preprocessing.include_dirs.end());
  preprocessing.defines.insert(preprocessing.defines.end(),
                               other.preprocessing.defines.begin(),
                               other.preprocessing.defines.end());
}
