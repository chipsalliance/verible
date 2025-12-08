// Copyright 2023 The Verible Authors.
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
//

#ifndef VERIBLE_COMMON_LSP_LSP_FILE_UTILS_H
#define VERIBLE_COMMON_LSP_LSP_FILE_UTILS_H

#include <string>
#include <string_view>

namespace verible::lsp {
// Converts file:// scheme entries to actual system paths.
// If other scheme is provided, method returns empty string_view.
// TODO (glatosinski) current resolving of LSP URIs is very naive
// and supports only narrow use cases of file:// specifier.
std::string LSPUriToPath(std::string_view uri);

// Converts filesystem paths to file:// scheme entries.
std::string PathToLSPUri(std::string_view path);
}  // namespace verible::lsp

#endif  // VERIBLE_COMMON_LSP_LSP_FILE_UTILS_H
