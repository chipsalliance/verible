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

#include "absl/status/status.h"
#include "absl/strings/string_view.h"
#include "common/util/simple_zip.h"
#include "third_party/proto/kythe/analysis.pb.h"

namespace verilog {
namespace kythe {

// Creator of Kythe Kzip archives based on the compilation unit
// (https://kythe.io/docs/kythe-kzip.html).
class KzipCreator final {
 public:
  // Initializes the archive. Crashes if initialization fails.
  explicit KzipCreator(absl::string_view output_path);
  ~KzipCreator();

  // Adds source code file to the Kzip. Returns its SHA digest.
  std::string AddSourceFile(absl::string_view path, absl::string_view content);

  // Adds compilation unit to the Kzip.
  absl::Status AddCompilationUnit(
      const ::kythe::proto::IndexedCompilation& unit);

 private:
  std::unique_ptr<FILE, decltype(&fclose)> zip_file_;
  std::unique_ptr<verible::zip::Encoder> archive_ = nullptr;
};

}  // namespace kythe
}  // namespace verilog

#endif  // VERIBLE_VERILOG_TOOLS_KYTHE_KZIP_CREATOR_H_
