// Copyright 2017-2020 The Verible Authors.
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

#include "verilog/tools/kythe/kzip_creator.h"

#include <openssl/sha.h>

#include <array>
#include <cstdio>
#include <string>
#include <vector>

#include "absl/status/status.h"
#include "absl/strings/escaping.h"
#include "absl/strings/string_view.h"
#include "common/util/file_util.h"
#include "common/util/simple_zip.h"
#include "third_party/proto/kythe/analysis.pb.h"

namespace verilog {
namespace kythe {
namespace {

constexpr absl::string_view kProtoUnitRoot = "root/pbunits/";
constexpr absl::string_view kFileRoot = "root/files/";

std::string SHA256Digest(absl::string_view content) {
  std::array<unsigned char, SHA256_DIGEST_LENGTH> buf;
  ::SHA256(reinterpret_cast<const unsigned char*>(content.data()),
           content.size(), buf.data());
  return absl::BytesToHexString(
      absl::string_view(reinterpret_cast<const char*>(buf.data()), buf.size()));
}

constexpr int kKZipCompressionLevel = 9;
}  // namespace

KzipCreator::KzipCreator(absl::string_view output_path)
    : zip_file_(fopen(std::string(output_path).c_str(), "wb"), &fclose),
      archive_(kKZipCompressionLevel, [this](absl::string_view s) {
        return fwrite(s.data(), 1, s.size(), zip_file_.get()) == s.size();
      }) {
  // Create the directory structure.
  archive_.AddFile("root/", verible::zip::MemoryByteSource(""));
  archive_.AddFile(kFileRoot, verible::zip::MemoryByteSource(""));
  archive_.AddFile(kProtoUnitRoot, verible::zip::MemoryByteSource(""));
}

std::string KzipCreator::AddSourceFile(absl::string_view path,
                                       absl::string_view content) {
  std::string digest = SHA256Digest(content);
  const std::string archive_path = verible::file::JoinPath(kFileRoot, digest);
  archive_.AddFile(archive_path, verible::zip::MemoryByteSource(content));
  return digest;
}

absl::Status KzipCreator::AddCompilationUnit(
    const ::kythe::proto::IndexedCompilation& unit) {
  std::string content;
  if (!unit.SerializeToString(&content)) {
    return absl::InternalError("Failed to serialize the compilation unit");
  }
  const std::string digest = SHA256Digest(content);
  const std::string archive_path =
      verible::file::JoinPath(kProtoUnitRoot, digest);
  archive_.AddFile(archive_path, verible::zip::MemoryByteSource(content));
  return absl::OkStatus();
}

}  // namespace kythe
}  // namespace verilog
