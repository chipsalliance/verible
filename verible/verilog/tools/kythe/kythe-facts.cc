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

#include "verible/verilog/tools/kythe/kythe-facts.h"

#include <cstddef>
#include <iostream>
#include <string>
#include <vector>

#include "absl/hash/hash.h"
#include "absl/log/check.h"
#include "absl/strings/escaping.h"
#include "absl/strings/str_cat.h"
#include "absl/strings/string_view.h"
#include "verible/common/util/spacer.h"

namespace verilog {
namespace kythe {
namespace {

// Returns a hash value produced by merging two hash values.
size_t CombineHash(size_t existing, size_t addition) {
  // Taken from boost::hash_combine. Maybe replace with AbslHashValue::combine.
  return existing ^ (addition + 0x9e3779b9 + (existing << 6) + (existing >> 2));
}

// Returns a rolling hash (https://en.wikipedia.org/wiki/Rolling_hash) of the
// signature names. NOTE: the first name (the file) is skipped and replaced with
// 0.
//
// The rolling hash of a vector produces a vector of an equal size where each
// element is a combined hash of all previous elements.
// res[0] = 0  // Global scope hash
// res[1] = hash(0, name[0])
// res[2] = hash(0, name[0], name[1])
// ...
// res[N] = hash(0, name[0], name[1], ..., name[N])
std::vector<size_t> RollingHash(const std::vector<absl::string_view> &names) {
  if (names.size() <= 1) {
    return {0};  // Global scope
  }

  std::vector<size_t> hashes = {0};  // Prefix with the global scope
  hashes.reserve(names.size());
  size_t previous = 0;
  // Start from 2nd element --> skip the filename.
  for (size_t i = 1; i < names.size(); ++i) {
    previous = CombineHash(previous, absl::HashOf(names[i]));
    hashes.push_back(previous);
  }
  CHECK_EQ(hashes.size(), names.size());
  return hashes;
}

}  // namespace

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

SignatureDigest Signature::Digest() const {
  return SignatureDigest{.rolling_hash = RollingHash(Names())};
}

bool VName::operator==(const VName &other) const {
  return path == other.path && root == other.root && corpus == other.corpus &&
         signature == other.signature && language == other.language;
}

std::ostream &VName::FormatJSON(std::ostream &stream, bool debug,
                                int indentation) const {
  // Output new line only in debug mode.
  const std::string separator = debug ? "\n" : "";
  stream << "{" << separator;
  const verible::Spacer idt(indentation + 2);
  stream << idt << "\"signature\": \""
         << (debug ? signature.ToString() : signature.ToBase64()) << "\","
         << separator << idt << "\"path\": \"" << path << "\"," << separator
         << idt << "\"language\": \"" << language << "\"," << separator << idt
         << "\"root\": \"" << root << "\"," << separator << idt
         << "\"corpus\": \"" << corpus << "\"" << separator
         << verible::Spacer(indentation) << "}";
  return stream;
}

std::ostream &operator<<(std::ostream &stream, const VName &vname) {
  return vname.FormatJSON(stream, /*debug=*/true);
}

bool Fact::operator==(const Fact &other) const {
  return fact_value == other.fact_value && fact_name == other.fact_name &&
         node_vname == other.node_vname;
}

std::ostream &Fact::FormatJSON(std::ostream &stream, bool debug,
                               int indentation) const {
  // Output new line only in debug mode.
  const std::string separator = debug ? "\n" : "";
  // Indent entries in debug mode
  const int indent_more = debug ? indentation + 2 : 0;
  const verible::Spacer idt(indent_more);
  stream << "{" << separator;
  {
    stream << idt << "\"source\": ";
    node_vname.FormatJSON(stream, debug, indent_more) << "," << separator;
  }
  stream << idt << "\"fact_name\": \"" << fact_name << "\"," << separator  //
         << idt << "\"fact_value\": \""
         << (debug ? fact_value : absl::Base64Escape(fact_value)) << "\""
         << separator  //
         << verible::Spacer(indentation) << "}";
  return stream;
}

std::ostream &operator<<(std::ostream &stream, const Fact &fact) {
  return fact.FormatJSON(stream, /*debug=*/true);
}

bool Edge::operator==(const Edge &other) const {
  return edge_name == other.edge_name && source_node == other.source_node &&
         target_node == other.target_node;
}

std::ostream &Edge::FormatJSON(std::ostream &stream, bool debug,
                               int indentation) const {
  // Output new line only in debug mode.
  const std::string separator = debug ? "\n" : "";
  // Indent entries in debug mode
  const int indent_more = debug ? indentation + 2 : 0;
  const verible::Spacer idt(indent_more);
  stream << "{" << separator;
  {
    stream << idt << "\"source\": ";
    source_node.FormatJSON(stream, debug, indent_more) << "," << separator;
  }
  { stream << idt << "\"edge_kind\": \"" << edge_name << "\"," << separator; }
  {
    stream << idt << "\"target\": ";
    target_node.FormatJSON(stream, debug, indent_more) << "," << separator;
  }
  { stream << idt << "\"fact_name\": \"/\"" << separator; }
  return stream << verible::Spacer(indentation) << "}";
}

std::ostream &operator<<(std::ostream &stream, const Edge &edge) {
  return edge.FormatJSON(stream, /*debug=*/true);
}

}  // namespace kythe
}  // namespace verilog
