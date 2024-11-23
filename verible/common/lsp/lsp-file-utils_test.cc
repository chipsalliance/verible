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

#include <string>

#include "absl/strings/str_cat.h"
#include "gtest/gtest.h"

namespace verible {
namespace lsp {

std::string PathPrefix(const std::string &path) {
#ifdef _WIN32
  return absl::StrCat("y:/", path);
#else
  return absl::StrCat("/", path);
#endif
}

std::string URIPrefix(const std::string &path) {
#ifdef _WIN32
  return absl::StrCat("file:///y%3a/", path);
#else
  return absl::StrCat("file:///", path);
#endif
}

TEST(URIDecodingTest, SimplePathDecoding) {
  ASSERT_EQ(PathPrefix("test-project/add.sv"),
            LSPUriToPath(URIPrefix("test-project/add.sv")));
}

TEST(URIEncodingTest, SimplePathEncoding) {
  ASSERT_EQ(URIPrefix("test-project/add.sv"),
            PathToLSPUri(PathPrefix("test-project/add.sv")));
}

TEST(URIDecodingtest, PathWithSpacesDecoding) {
  ASSERT_EQ(PathPrefix("test project/my module.sv"),
            LSPUriToPath(URIPrefix("test%20project/my%20module.sv")));
}

TEST(URIEncodingTest, PathWithSpacesEncoding) {
  ASSERT_EQ(URIPrefix("test%20project/my%20module.sv"),
            PathToLSPUri(PathPrefix("test project/my module.sv")));
}

TEST(URIDecodingtest, PathWithConsecutiveEscapes) {
  ASSERT_EQ(PathPrefix("test project/my    module.sv"),
            LSPUriToPath(URIPrefix("test%20project/my%20%20%20%20module.sv")));
}

TEST(URIEncodingTest, PathWithConsecutiveEscapes) {
  ASSERT_EQ(URIPrefix("test%20project/my%20%20%20%20module.sv"),
            PathToLSPUri(PathPrefix("test project/my    module.sv")));
}

TEST(URIDecodingTest, InvalidHex) {
  ASSERT_EQ(PathPrefix("test-project/my-module-%qz.sv"),
            LSPUriToPath(URIPrefix("test-project/my-module-%qz.sv")));
}

TEST(URIDecodingTest, HexConversions) {
  ASSERT_EQ(PathPrefix("test-project/my-module-%q"),
            LSPUriToPath(URIPrefix("test-project/my-module-%q")));
  ASSERT_EQ(PathPrefix("test-project/my-module-%xyz"),
            LSPUriToPath(URIPrefix("test-project/my-module-%xyz")));
  ASSERT_EQ(PathPrefix(" "), LSPUriToPath(URIPrefix("%20")));
  ASSERT_EQ(PathPrefix("%2"), LSPUriToPath(URIPrefix("%2")));
  ASSERT_EQ(PathPrefix("%"), LSPUriToPath(URIPrefix("%")));
}

}  // namespace lsp
}  // namespace verible
