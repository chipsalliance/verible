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

#include "verible/common/lsp/lsp-file-utils.h"

#include <algorithm>
#include <cctype>
#include <filesystem>
#include <string>

#include "absl/strings/escaping.h"
#include "absl/strings/match.h"
#include "absl/strings/str_cat.h"
#include "absl/strings/string_view.h"

namespace verible::lsp {

static constexpr absl::string_view kFileSchemePrefix = "file://";

namespace {

bool NeedsEscape(char c) {
  if (isalnum(c)) return false;
  switch (c) {
    case '-':
    case '/':
    case '.':
    case '_':
    case '~':
    case '\\':
      return false;
    default:
      return true;
  }
}

std::string DecodeURI(absl::string_view uri) {
  std::string result;
  result.reserve(uri.size() - 2 * std::count(uri.begin(), uri.end(), '%'));
  absl::string_view::size_type pos = 0;

  while (pos < uri.size()) {
    if (uri[pos] == '%') {
      pos++;
      if (pos + 2 <= uri.size() && std::isxdigit(uri[pos]) &&
          std::isxdigit(uri[pos + 1])) {
        std::string hex = absl::HexStringToBytes(uri.substr(pos, 2));
        absl::StrAppend(&result, hex.length() == 1 ? hex : uri.substr(pos, 2));
        pos += 2;
      } else {
        absl::StrAppend(&result, "%");
      }
    }
    absl::string_view::size_type nextpos = uri.find('%', pos);
    if (nextpos > absl::string_view::npos) nextpos = uri.size();
    absl::StrAppend(&result, uri.substr(pos, nextpos - pos));
    pos = nextpos;
  }
  return result;
}

std::string EncodeURI(absl::string_view uri) {
  std::string result;

  absl::string_view::size_type pos = 0;

  int prevpos = 0;
  while (pos < uri.size()) {
    if (NeedsEscape(uri[pos])) {
      absl::StrAppend(&result, uri.substr(prevpos, pos - prevpos));
      absl::StrAppend(&result, "%", absl::Hex(uri[pos], absl::kZeroPad2));
      prevpos = ++pos;
    } else {
      pos++;
    }
  }
  absl::StrAppend(&result, uri.substr(prevpos, pos - prevpos));
  return result;
}
}  // namespace

std::string LSPUriToPath(absl::string_view uri) {
  if (!absl::StartsWith(uri, kFileSchemePrefix)) return "";
  std::string path = DecodeURI(uri.substr(kFileSchemePrefix.size()));
  // In Windows, paths in URIs are represented as
  // file:///c:/Users/user/project/file.sv
  // Which results in paths:
  // /c:/Users/user/project/file.sv
  // The prefix "/" needs to be removed from the path
#ifdef _WIN32
  if (path.length() >= 3 && path[0] == '/' && isalpha(path[1]) &&
      path[2] == ':') {
    path = path.substr(1);
  }
#endif
  return path;
}

std::string PathToLSPUri(absl::string_view path) {
  std::filesystem::path p(path.begin(), path.end());
  std::string normalized_path;
  normalized_path = std::filesystem::absolute(p).string();
  std::transform(normalized_path.cbegin(), normalized_path.cend(),
                 normalized_path.begin(),
                 [](char c) { return c == '\\' ? '/' : c; });
#ifdef _WIN32
  if (normalized_path.length() >= 2 && isalpha(normalized_path[0]) &&
      normalized_path[1] == ':') {
    // In Windows, paths in URIs are represented as
    // file:///c:/Users/user/project/file.sv
    // Which results in paths:
    // /c:/Users/user/project/file.sv
    // The prefix "/" needs to be added
    normalized_path = absl::StrCat("/", normalized_path);
  }
#endif
  normalized_path = EncodeURI(normalized_path);
  return absl::StrCat(kFileSchemePrefix, normalized_path);
}

}  // namespace verible::lsp
