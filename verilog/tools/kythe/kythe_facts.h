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

#ifndef VERIBLE_VERILOG_TOOLS_KYTHE_KYTHE_FACTS_H_
#define VERIBLE_VERILOG_TOOLS_KYTHE_KYTHE_FACTS_H_

#include <iosfwd>
#include <string>

#include "absl/strings/escaping.h"
#include "absl/strings/string_view.h"

namespace verilog {
namespace kythe {

// Node vector name for kythe facts.
struct VName {
  explicit VName(absl::string_view path, absl::string_view signature = "",
                 absl::string_view root = "",
                 absl::string_view language = "verilog",
                 absl::string_view corpus = "https://github.com/google/verible")
      : signature(signature),
        signature_base_64(absl::Base64Escape(signature)),
        path(path),
        language(language),
        corpus(corpus),
        root(root) {}

  std::string ToString() const;

  // Unique identifier for this VName.
  std::string signature;

  // Unique identifier for this VName in base 64.
  std::string signature_base_64;

  // Path for the file the name is extracted from.
  std::string path;

  // The language this name belongs to.
  std::string language;

  // The corpus of source code this VName belongs to.
  std::string corpus;

  // A directory path or project identifier inside the Corpus.
  std::string root;
};

std::ostream& operator<<(std::ostream&, const VName&);

}  // namespace kythe
}  // namespace verilog

#endif  // VERIBLE_VERILOG_TOOLS_KYTHE_KYTHE_FACTS_H_
