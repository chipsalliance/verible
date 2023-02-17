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

#include "verilog/tools/ls/lsp-file-utils.h"

#include <filesystem>

#include "absl/strings/match.h"
#include "absl/strings/str_cat.h"
#include "absl/strings/string_view.h"

namespace verilog {

static constexpr absl::string_view kFileSchemePrefix = "file://";

absl::string_view LSPUriToPath(absl::string_view uri) {
  if (!absl::StartsWith(uri, kFileSchemePrefix)) return "";
  return uri.substr(kFileSchemePrefix.size());
}

std::string PathToLSPUri(absl::string_view path) {
  std::filesystem::path p(path.begin(), path.end());
  return absl::StrCat(kFileSchemePrefix, std::filesystem::absolute(p).string());
}

}  // namespace verilog
