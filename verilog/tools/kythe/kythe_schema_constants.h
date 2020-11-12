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

// Kythe Nodes.
constexpr absl::string_view kNodeAnchor = "anchor";
constexpr absl::string_view kNodeRecord = "record";
constexpr absl::string_view kNodeInterface = "interface";
constexpr absl::string_view kNodePackage = "package";
constexpr absl::string_view kNodeMacro = "macro";
constexpr absl::string_view kNodeConstant = "constant";
constexpr absl::string_view kNodeFile = "file";
constexpr absl::string_view kNodeBuiltin = "tbuiltin";
constexpr absl::string_view kSubkindModule = "module";
constexpr absl::string_view kSubkindConstructor = "constructor";
constexpr absl::string_view kSubkindProgram = "program";
constexpr absl::string_view kCompleteDefinition = "definition";
constexpr absl::string_view kInComplete = "incomplete";
constexpr absl::string_view kNodeVariable = "variable";
constexpr absl::string_view kNodeFunction = "function";
constexpr absl::string_view kNodeTAlias = "talias";

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
constexpr absl::string_view kEdgeRefExpands = "/kythe/edge/ref/expands";
constexpr absl::string_view kEdgeRefCall = "/kythe/edge/ref/call";
constexpr absl::string_view kEdgeRefImports = "/kythe/edge/ref/imports";
constexpr absl::string_view kEdgeExtends = "/kythe/edge/extends";
constexpr absl::string_view kEdgeRefIncludes = "/kythe/edge/ref/includes";
constexpr absl::string_view kEdgeTyped = "/kythe/edge/typed";
constexpr absl::string_view kEdgeOverrides = "/kythe/edge/overrides";

}  // namespace kythe
}  // namespace verilog

#endif  // VERIBLE_VERILOG_TOOLS_KYTHE_KYTHE_SCHEMA_CONSTANTS_H_
