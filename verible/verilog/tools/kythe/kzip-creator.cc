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

#include "verible/verilog/tools/kythe/kzip-creator.h"

#include <cstdio>
#include <string>
#include <string_view>

#include "absl/status/status.h"
#include "third_party/proto/kythe/analysis.pb.h"
#include "verible/common/util/file-util.h"
#include "verible/common/util/sha256.h"
#include "verible/common/util/simple-zip.h"

namespace verilog {
namespace kythe {
namespace {

constexpr std::string_view kProtoUnitRoot = "root/pbunits/";
constexpr std::string_view kFileRoot = "root/files/";

constexpr int kKZipCompressionLevel = 9;

}  // namespace

KzipCreator::KzipCreator(std::string_view output_path)
    : zip_file_(fopen(std::string(output_path).c_str(), "wb")),
      archive_(kKZipCompressionLevel, [this](std::string_view s) {
        return fwrite(s.data(), 1, s.size(), zip_file_.get()) == s.size();
      }) {
  // Create the directory structure.
  archive_.AddFile("root/", verible::zip::MemoryByteSource(""));
  archive_.AddFile(kFileRoot, verible::zip::MemoryByteSource(""));
  archive_.AddFile(kProtoUnitRoot, verible::zip::MemoryByteSource(""));
}

std::string KzipCreator::AddSourceFile(std::string_view path,
                                       std::string_view content) {
  std::string digest = verible::Sha256Hex(content);
  const std::string archive_path = verible::file::JoinPath(kFileRoot, digest);
  archive_.AddFile(archive_path, verible::zip::MemoryByteSource(content));
  return digest;
}

absl::Status KzipCreator::AddCompilationUnit(
    const ::kythe::proto::IndexedCompilation &unit) {
  std::string content;
  if (!unit.SerializeToString(&content)) {
    return absl::InternalError("Failed to serialize the compilation unit");
  }
  const std::string digest = verible::Sha256Hex(content);
  const std::string archive_path =
      verible::file::JoinPath(kProtoUnitRoot, digest);
  archive_.AddFile(archive_path, verible::zip::MemoryByteSource(content));
  return absl::OkStatus();
}

}  // namespace kythe
}  // namespace verilog
