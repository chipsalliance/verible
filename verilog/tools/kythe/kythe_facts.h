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
#include <vector>

#include "absl/strings/string_view.h"

namespace verilog {
namespace kythe {

// Unique identifier for Kythe facts.
class Signature {
 public:
  Signature(absl::string_view name = "")
      : names_(std::vector<std::string>{std::string(name)}) {}

  Signature(const Signature& parent, absl::string_view name) {
    names_ = parent.Names();
    names_.push_back(std::string(name));
  }

  bool operator==(const Signature& o) const;
  bool operator<(const Signature& o) const;

  // Returns the the signature concatenated as a string.
  std::string ToString() const;

  // Returns the the signature concatenated as a string in base 64.
  std::string ToBase64() const;

  // Checks whether this signature represents the same given variable in its
  // scope.
  bool IsNameEqual(absl::string_view) const;

  // Appends variable name to the end of the current signature.
  void AppendName(absl::string_view);

  const std::vector<std::string> Names() const { return names_; }

 private:
  // List that uniquely determines this signature and differentiates it from any
  // other signature.
  // This list represents the name of some signature in a scope.
  // e.g
  // class m;
  //    int x;
  // endclass
  //
  // for "m" ==> ["m"]
  // for "x" ==> ["m", "x"]
  std::vector<std::string> names_;
};

// Node vector name for kythe facts.
struct VName {
  explicit VName(absl::string_view path = "",
                 const Signature& signature = Signature(),
                 absl::string_view root = "",
                 absl::string_view language = "verilog",
                 // TODO(minatoma): change the corpus if needed.
                 absl::string_view corpus = "")
      : signature(signature),
        path(path),
        language(language),
        corpus(corpus),
        root(root) {}

  std::string ToString() const;

  // Unique identifier for this VName.
  Signature signature;

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
