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

#ifndef VERIBLE_VERILOG_TOOLS_KYTHE_KYTHE_SCHEMA_CONSTANTS_H_
#define VERIBLE_VERILOG_TOOLS_KYTHE_KYTHE_SCHEMA_CONSTANTS_H_

#include "absl/strings/string_view.h"

namespace verilog {
namespace kythe {

// Const Signatures,
constexpr absl::string_view kModuleBuiltin = "module#builtin";

// Kythe Nodes.
constexpr absl::string_view kNodeAnchor = "anchor";
constexpr absl::string_view kNodeRecord = "record";
constexpr absl::string_view kNodeFile = "file";
constexpr absl::string_view kNodeBuiltin = "tbuiltin";
constexpr absl::string_view kSubkindModule = "module";
constexpr absl::string_view kCompleteDefinition = "definition";
constexpr absl::string_view kNodeVariable = "variable";

// Facts for kythe.
constexpr absl::string_view kFactText = "/kythe/text";
constexpr absl::string_view kFactNodeKind = "/kythe/node/kind";
constexpr absl::string_view kFactSubkind = "/kythe/subkind";
constexpr absl::string_view kFactComplete = "/kythe/complete";
constexpr absl::string_view kFactAnchorEnd = "/kythe/loc/end";
constexpr absl::string_view kFactAnchorStart = "/kythe/loc/start";

// Edges for kythe.
constexpr absl::string_view kEdgeDefinesBinding = "/kythe/edge/defines/binding";
constexpr absl::string_view kEdgeChildOf = "/kythe/edge/childof";
constexpr absl::string_view kEdgeRef = "/kythe/edge/ref";
constexpr absl::string_view kEdgeTyped = "/kythe/edge/typed";

}  // namespace kythe
}  // namespace verilog

#endif  // VERIBLE_VERILOG_TOOLS_KYTHE_KYTHE_SCHEMA_CONSTANTS_H_
