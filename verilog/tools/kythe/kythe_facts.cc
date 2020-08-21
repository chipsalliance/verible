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

#include <iostream>
#include <string>

#include "absl/strings/substitute.h"
#include "verilog/tools/kythe/kythe_facts.h"

namespace verilog {
namespace kythe {

std::string VName::ToString() const {
  return absl::Substitute(
      R"({"signature": "$0","path": "$1","language": "$2","root": "$3","corpus": "$4"})",
      signature_base_64, path, language, root, corpus);
}

std::ostream& operator<<(std::ostream& stream, const VName& vname) {
  stream << vname.ToString();
  return stream;
}

std::ostream& operator<<(std::ostream& stream, const Fact& fact) {
  stream << absl::Substitute(
      R"({"source": $0,"fact_name": "$1","fact_value": "$2"})",
      fact.node_vname.ToString(), fact.fact_name, fact.fact_value);
  return stream;
}

std::ostream& operator<<(std::ostream& stream, const Edge& edge) {
  stream << absl::Substitute(
      R"({"source": $0,"edge_kind": "$1","target": $2,"fact_name": "/"})",
      edge.source_node.ToString(), edge.edge_name, edge.target_node.ToString());
  return stream;
}

}  // namespace kythe
}  // namespace verilog
