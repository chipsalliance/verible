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
#include "absl/strings/string_view.h"
#include "absl/strings/substitute.h"

namespace verilog {
namespace kythe {

// TODO(minatoma): change this string comparison to a tuple comparison.
bool Signature::operator==(const Signature& other) const {
  return this->ToString() == other.ToString();
}

// TODO(minatoma): change this string comparison to a tuple comparison.
bool Signature::operator<(const Signature& other) const {
  return this->ToString() < other.ToString();
}

void Signature::AppendName(absl::string_view name) {
  names_.push_back(std::string(name));
}

bool Signature::IsNameEqual(absl::string_view name) const {
  return names_.back() == name;
}

std::string Signature::ToString() const {
  std::string signature = "";
  for (absl::string_view name : names_) {
    if (name.empty()) continue;
    absl::StrAppend(&signature, name, "#");
  }
  return signature;
}

std::string Signature::ToBase64() const {
  return absl::Base64Escape(ToString());
}

// TODO(minatoma): change this string comparison to a tuple comparison.
bool VName::operator==(const VName& other) const {
  return this->ToString() == other.ToString();
}

// TODO(minatoma): change this string comparison to a tuple comparison.
bool VName::operator<(const VName& other) const {
  return this->ToString() < other.ToString();
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

// TODO(minatoma): change this string comparison to a tuple comparison.
bool Fact::operator==(const Fact& other) const {
  return this->ToString() == other.ToString();
}

// TODO(minatoma): change this string comparison to a tuple comparison.
bool Fact::operator<(const Fact& other) const {
  return this->ToString() < other.ToString();
}

std::string Fact::ToString() const {
  return absl::Substitute(
      R"({"source": $0,"fact_name": "$1","fact_value": "$2"})",
      node_vname.ToString(), fact_name, absl::Base64Escape(fact_value));
}

std::ostream& operator<<(std::ostream& stream, const Fact& fact) {
  stream << fact.ToString();
  return stream;
}

// TODO(minatoma): change this string comparison to a tuple comparison.
bool Edge::operator==(const Edge& other) const {
  return this->ToString() == other.ToString();
}

// TODO(minatoma): change this string comparison to a tuple comparison.
bool Edge::operator<(const Edge& other) const {
  return this->ToString() < other.ToString();
}

std::string Edge::ToString() const {
  return absl::Substitute(
      R"({"source": $0,"edge_kind": "$1","target": $2,"fact_name": "/"})",
      source_node.ToString(), edge_name, target_node.ToString());
}

std::ostream& operator<<(std::ostream& stream, const Edge& edge) {
  stream << edge.ToString();
  return stream;
}

}  // namespace kythe
}  // namespace verilog
