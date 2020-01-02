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

#include "verilog/analysis/descriptions.h"

#include <string>

#include "absl/strings/str_cat.h"
#include "absl/strings/string_view.h"

namespace verilog {
namespace analysis {

std::string Codify(absl::string_view code, DescriptionType description_type) {
  if (description_type == DescriptionType::kMarkdown) {
    // If a backtick is found, then wrap code with two backticks.
    const auto pos = code.find('`');
    if (pos == absl::string_view::npos)
      return absl::StrCat("`", code, "`");
    else
      return absl::StrCat("`` ", code, "``");
  } else {
    return absl::StrCat("\'", code, "\'");
  }
}

}  // namespace analysis
}  // namespace verilog
