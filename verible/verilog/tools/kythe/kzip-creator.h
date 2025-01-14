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

#ifndef VERIBLE_VERILOG_TOOLS_KYTHE_KZIP_CREATOR_H_
#define VERIBLE_VERILOG_TOOLS_KYTHE_KZIP_CREATOR_H_

#include <cstdio>
#include <memory>
#include <string>
#include <string_view>

#include "absl/status/status.h"
#include "third_party/proto/kythe/analysis.pb.h"
#include "verible/common/util/simple-zip.h"

namespace verilog {
namespace kythe {

// Creator of Kythe Kzip archives based on the compilation unit
// (https://kythe.io/docs/kythe-kzip.html).
class KzipCreator final {
 public:
  // Initializes the archive. Crashes if initialization fails.
  explicit KzipCreator(std::string_view output_path);

  // Adds source code file to the Kzip. Returns its digest.
  std::string AddSourceFile(std::string_view path, std::string_view content);

  // Adds compilation unit to the Kzip.
  absl::Status AddCompilationUnit(
      const ::kythe::proto::IndexedCompilation &unit);

 private:
  struct file_closer {
    void operator()(FILE *f) const noexcept { fclose(f); }
  };
  std::unique_ptr<FILE, file_closer> zip_file_;
  verible::zip::Encoder archive_;
};

}  // namespace kythe
}  // namespace verilog

#endif  // VERIBLE_VERILOG_TOOLS_KYTHE_KZIP_CREATOR_H_
