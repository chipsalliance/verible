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

#include "verible/verilog/CST/verilog-nonterminals.h"

#include <ostream>
#include <string>

#include "absl/strings/str_cat.h"

namespace verilog {

std::string NodeEnumToString(NodeEnum node_enum) {
  switch (node_enum) {
#define CONSIDER(val) \
  case NodeEnum::val: \
    return #val;
#include "verible/verilog/CST/verilog_nonterminals_foreach.inc"  // IWYU pragma: keep

#undef CONSIDER
    default:
      return absl::StrCat("No Associated String: ",
                          static_cast<int>(node_enum));
  }
}

std::ostream &operator<<(std::ostream &stream, const NodeEnum &e) {
  return stream << NodeEnumToString(e);
}

bool IsPreprocessingNode(NodeEnum node_enum) {
  switch (node_enum) {
    case NodeEnum::kPreprocessorIfdefClause:
    case NodeEnum::kPreprocessorIfndefClause:
    case NodeEnum::kPreprocessorElsifClause:
    case NodeEnum::kPreprocessorElseClause:
    case NodeEnum::kPreprocessorDefine:
    case NodeEnum::kPreprocessorUndef:
    case NodeEnum::kPreprocessorInclude: {
      return true;
    }
    default:
      return false;
  }
}

}  // namespace verilog
