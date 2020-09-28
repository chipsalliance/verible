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

  bool operator==(const Signature& other) const;
  bool operator<(const Signature& other) const;

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

  bool operator==(const VName& other) const;
  bool operator<(const VName& other) const;

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

// Facts for kythe.
// For more information:
// https://www.kythe.io/docs/kythe-storage.html#_a_id_termfact_a_fact
// https://www.kythe.io/docs/schema/writing-an-indexer.html#_modeling_kythe_entries
struct Fact {
  Fact(const VName& vname, absl::string_view name, absl::string_view value)
      : node_vname(vname), fact_name(name), fact_value(value) {}

  bool operator==(const Fact& other) const;
  bool operator<(const Fact& other) const;

  std::string ToString() const;

  // The vname of the node this fact is about.
  const VName node_vname;

  // The name identifying this fact.
  const std::string fact_name;

  // The given value to this fact.
  const std::string fact_value;
};

std::ostream& operator<<(std::ostream&, const Fact&);

// Edges for kythe.
// For more information:
// https://www.kythe.io/docs/schema/writing-an-indexer.html#_modeling_kythe_entries
struct Edge {
  Edge(const VName& source, absl::string_view name, const VName& target)
      : source_node(source), edge_name(name), target_node(target) {}

  bool operator==(const Edge& other) const;
  bool operator<(const Edge& other) const;

  std::string ToString() const;

  // The vname of the source node of this edge.
  const VName source_node;

  // The edge name which identifies the edge kind.
  const std::string edge_name;

  // The vname of the target node of this edge.
  const VName target_node;
};

std::ostream& operator<<(std::ostream&, const Edge&);

}  // namespace kythe
}  // namespace verilog

#endif  // VERIBLE_VERILOG_TOOLS_KYTHE_KYTHE_FACTS_H_
