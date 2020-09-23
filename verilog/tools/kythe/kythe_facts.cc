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

#include "verilog/tools/kythe/kythe_facts.h"

#include <iostream>
#include <string>

#include "absl/strings/escaping.h"
#include "absl/strings/substitute.h"

namespace verilog {
namespace kythe {

bool Signature::operator==(const Signature& other) const {
  return this->ToString() == other.ToString();
}

bool Signature::operator<(const Signature& other) const {
  return this->ToString() < other.ToString();
}

void Signature::AppendName(absl::string_view name) {
  names.push_back(std::string(name));
}

bool Signature::IsEqualToIgnoringScope(absl::string_view name) const {
  return names.back() == name;
}

std::string Signature::ToString() const {
  std::string signature = "";
  for (const std::string name : names) {
    if (name.empty()) continue;
    signature += name + "#";
  }
  return signature;
}

std::string Signature::ToBase64() const {
  return absl::Base64Escape(ToString());
}

std::string VName::ToString() const {
  return absl::Substitute(
      R"({"signature": "$0","path": "$1","language": "$2","root": "$3","corpus": "$4"})",
      signature.ToBase64(), path, language, root, corpus);
}

std::ostream& operator<<(std::ostream& stream, const VName& vname) {
  stream << vname.ToString();
  return stream;
}

}  // namespace kythe
}  // namespace verilog
