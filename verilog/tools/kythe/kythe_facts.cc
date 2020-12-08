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

#include <algorithm>
#include <iostream>
#include <sstream>
#include <string>
#include <tuple>

#include "absl/strings/escaping.h"
#include "absl/strings/string_view.h"
#include "common/util/spacer.h"

namespace verilog {
namespace kythe {

bool Signature::operator==(const Signature& other) const {
  return names_.size() == other.names_.size() &&
         std::equal(names_.begin(), names_.end(), other.names_.begin());
}

bool Signature::operator<(const Signature& other) const {
  return std::lexicographical_compare(names_.begin(), names_.end(),
                                      other.names_.begin(), other.names_.end());
}

bool Signature::IsNameEqual(absl::string_view name) const {
  return names_.back() == name;
}

std::string Signature::ToString() const {
  std::string signature;
  for (absl::string_view name : names_) {
    if (name.empty()) continue;
    absl::StrAppend(&signature, name, "#");
  }
  return signature;
}

std::string Signature::ToBase64() const {
  return absl::Base64Escape(ToString());
}

bool VName::operator==(const VName& other) const {
  return std::tie(signature, path, language, root, corpus) ==
         std::tie(other.signature, other.path, other.language, other.root,
                  other.corpus);
}

bool VName::operator<(const VName& other) const {
  return std::tie(signature, path, language, root, corpus) <
         std::tie(other.signature, other.path, other.language, other.root,
                  other.corpus);
}

std::ostream& VName::FormatJSON(std::ostream& stream, bool debug,
                                int indentation) const {
  stream << "{\n";
  const verible::Spacer idt(indentation + 2);
  stream << idt << "\"signature\": \""
         << (debug ? signature.ToString() : signature.ToBase64()) << "\",\n"  //
         << idt << "\"path\": \"" << path << "\",\n"                          //
         << idt << "\"language\": \"" << language << "\",\n"                  //
         << idt << "\"root\": \"" << root << "\",\n"                          //
         << idt << "\"corpus\": \"" << corpus << "\"\n"                       //
         << verible::Spacer(indentation) << "}";
  return stream;
}

std::ostream& operator<<(std::ostream& stream, const VName& vname) {
  return vname.FormatJSON(stream, true);
}

bool Fact::operator==(const Fact& other) const {
  return std::tie(node_vname, fact_name, fact_value) ==
         std::tie(other.node_vname, other.fact_name, other.fact_value);
}

bool Fact::operator<(const Fact& other) const {
  return std::tie(node_vname, fact_name, fact_value) <
         std::tie(other.node_vname, other.fact_name, other.fact_value);
}

std::ostream& Fact::FormatJSON(std::ostream& stream, bool debug,
                               int indentation) const {
  const int indent_more = indentation + 2;
  const verible::Spacer idt(indent_more);
  stream << "{\n";
  {
    stream << idt << "\"source\": ";
    node_vname.FormatJSON(stream, debug, indent_more) << ",\n";
  }
  stream << idt << "\"fact_name\": \"" << fact_name << "\",\n"  //
         << idt << "\"fact_value\": \""
         << (debug ? fact_value : absl::Base64Escape(fact_value)) << "\"\n"  //
         << verible::Spacer(indentation) << "}";
  return stream;
}

std::ostream& operator<<(std::ostream& stream, const Fact& fact) {
  return fact.FormatJSON(stream, true);
}

bool Edge::operator==(const Edge& other) const {
  return std::tie(source_node, edge_name, target_node) ==
         std::tie(other.source_node, other.edge_name, other.target_node);
}

bool Edge::operator<(const Edge& other) const {
  return std::tie(source_node, edge_name, target_node) <
         std::tie(other.source_node, other.edge_name, other.target_node);
}

std::ostream& Edge::FormatJSON(std::ostream& stream, bool debug,
                               int indentation) const {
  const int indent_more = indentation + 2;
  const verible::Spacer idt(indent_more);
  stream << "{\n";
  {
    stream << idt << "\"source\": ";
    source_node.FormatJSON(stream, debug, indent_more) << ",\n";
  }
  { stream << idt << "\"edge_kind\": \"" << edge_name << "\",\n"; }
  {
    stream << idt << "\"target\": ";
    target_node.FormatJSON(stream, debug, indent_more) << ",\n";
  }
  { stream << idt << "\"fact_name\": \"/\"\n"; }
  return stream << verible::Spacer(indentation) << "}";
}

std::ostream& operator<<(std::ostream& stream, const Edge& edge) {
  return edge.FormatJSON(stream, true);
}

}  // namespace kythe
}  // namespace verilog
