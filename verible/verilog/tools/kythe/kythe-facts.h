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

#include <cstddef>
#include <iostream>
#include <string>
#include <vector>

#include "absl/strings/string_view.h"

namespace verilog {
namespace kythe {

inline constexpr absl::string_view kDefaultKytheLanguage = "verilog";
inline constexpr absl::string_view kEmptyKytheLanguage;

// Hash-based form of signature for fast and lightweight comparision.
struct SignatureDigest {
  std::vector<size_t> rolling_hash;

  size_t Hash() const { return rolling_hash.back(); }

  bool operator==(const SignatureDigest &d) const {
    return rolling_hash.size() == d.rolling_hash.size() &&
           rolling_hash.back() == d.rolling_hash.back();
  }

  friend std::ostream &operator<<(std::ostream &os, const SignatureDigest &d) {
    os << "{.Hash=" << d.Hash() << "}";
    return os;
  }
};
template <typename H>
H AbslHashValue(H state, const SignatureDigest &d) {
  return H::combine(std::move(state), d.rolling_hash);
}

// Unique identifier for Kythe facts.
class Signature {
 public:
  explicit Signature(absl::string_view name = "") : names_({name}) {}

  Signature(const Signature &parent, absl::string_view name)
      : names_(parent.Names()) {
    names_.push_back(name);
  }

  bool operator==(const Signature &other) const {
    return names_ == other.names_;
  }
  bool operator!=(const Signature &other) const { return !(*this == other); }

  // Returns the signature concatenated as a string.
  std::string ToString() const;

  // Returns the signature concatenated as a string in base 64.
  std::string ToBase64() const;

  const std::vector<absl::string_view> &Names() const { return names_; }

  // Returns signature's short form for fast and lightweight comparision.
  SignatureDigest Digest() const;

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
  std::vector<absl::string_view> names_;
};
template <typename H>
H AbslHashValue(H state, const Signature &v) {
  return H::combine(std::move(state), v.Names());
}

// Node vector name for kythe facts.
struct VName {
  bool operator==(const VName &other) const;

  std::ostream &FormatJSON(std::ostream &, bool debug,
                           int indentation = 0) const;
  // Path for the file the VName is extracted from.
  absl::string_view path;

  // A directory path or project identifier inside the Corpus.
  absl::string_view root;

  // Unique identifier for this VName.
  Signature signature;

  // The corpus of source code this VName belongs to.
  absl::string_view corpus;

  // The language this VName belongs to.
  absl::string_view language = kDefaultKytheLanguage;
};
template <typename H>
H AbslHashValue(H state, const VName &v) {
  return H::combine(std::move(state), v.path, v.root, v.signature, v.corpus);
}

std::ostream &operator<<(std::ostream &, const VName &);

// Facts for kythe.
// For more information:
// https://www.kythe.io/docs/kythe-storage.html#_a_id_termfact_a_fact
// https://www.kythe.io/docs/schema/writing-an-indexer.html#_modeling_kythe_entries
struct Fact {
  Fact(const VName &vname, absl::string_view name, absl::string_view value)
      : node_vname(vname), fact_name(name), fact_value(value) {}

  bool operator==(const Fact &other) const;
  bool operator!=(const Fact &other) const { return !(*this == other); }

  std::ostream &FormatJSON(std::ostream &, bool debug,
                           int indentation = 0) const;
  // The VName of the node this fact is about.
  const VName node_vname;

  // The name identifying this fact.
  // This is one of the constant strings in "kythe_schema_constants.h"
  const absl::string_view fact_name;

  // The given value to this fact.
  const std::string fact_value;
};
template <typename H>
H AbslHashValue(H state, const Fact &v) {
  return H::combine(std::move(state), v.node_vname, v.fact_name, v.fact_value);
}

std::ostream &operator<<(std::ostream &, const Fact &);

// Edges for kythe.
// For more information:
// https://www.kythe.io/docs/schema/writing-an-indexer.html#_modeling_kythe_entries
struct Edge {
  Edge(const VName &source, absl::string_view name, const VName &target)
      : source_node(source), edge_name(name), target_node(target) {}

  bool operator==(const Edge &other) const;
  bool operator!=(const Edge &other) const { return !(*this == other); }

  std::ostream &FormatJSON(std::ostream &, bool debug,
                           int indentation = 0) const;

  // The VName of the source node of this edge.
  const VName source_node;

  // The edge name which identifies the edge kind.
  // This is one of the constant strings from "kythe_schema_constants.h"
  const absl::string_view edge_name;

  // The VName of the target node of this edge.
  const VName target_node;
};
template <typename H>
H AbslHashValue(H state, const Edge &v) {
  return H::combine(std::move(state), v.source_node, v.target_node,
                    v.edge_name);
}

std::ostream &operator<<(std::ostream &, const Edge &);

}  // namespace kythe
}  // namespace verilog

#endif  // VERIBLE_VERILOG_TOOLS_KYTHE_KYTHE_FACTS_H_
