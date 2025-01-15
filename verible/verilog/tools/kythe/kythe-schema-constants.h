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

#include <string_view>

namespace verilog {
namespace kythe {

// Kythe Nodes.
inline constexpr std::string_view kNodeAnchor = "anchor";
inline constexpr std::string_view kNodeRecord = "record";
inline constexpr std::string_view kNodeInterface = "interface";
inline constexpr std::string_view kNodePackage = "package";
inline constexpr std::string_view kNodeMacro = "macro";
inline constexpr std::string_view kNodeConstant = "constant";
inline constexpr std::string_view kNodeFile = "file";
inline constexpr std::string_view kNodeBuiltin = "tbuiltin";
inline constexpr std::string_view kSubkindModule = "module";
inline constexpr std::string_view kSubkindConstructor = "constructor";
inline constexpr std::string_view kSubkindProgram = "program";
inline constexpr std::string_view kCompleteDefinition = "definition";
inline constexpr std::string_view kInComplete = "incomplete";
inline constexpr std::string_view kNodeVariable = "variable";
inline constexpr std::string_view kNodeFunction = "function";
inline constexpr std::string_view kNodeTAlias = "talias";

// Facts for kythe.
inline constexpr std::string_view kFactText = "/kythe/text";
inline constexpr std::string_view kFactNodeKind = "/kythe/node/kind";
inline constexpr std::string_view kFactSubkind = "/kythe/subkind";
inline constexpr std::string_view kFactComplete = "/kythe/complete";
inline constexpr std::string_view kFactAnchorEnd = "/kythe/loc/end";
inline constexpr std::string_view kFactAnchorStart = "/kythe/loc/start";

// Edges for kythe.
inline constexpr std::string_view kEdgeDefinesBinding =
    "/kythe/edge/defines/binding";
inline constexpr std::string_view kEdgeChildOf = "/kythe/edge/childof";
inline constexpr std::string_view kEdgeRef = "/kythe/edge/ref";
inline constexpr std::string_view kEdgeRefExpands = "/kythe/edge/ref/expands";
inline constexpr std::string_view kEdgeRefCall = "/kythe/edge/ref/call";
inline constexpr std::string_view kEdgeRefImports = "/kythe/edge/ref/imports";
inline constexpr std::string_view kEdgeExtends = "/kythe/edge/extends";
inline constexpr std::string_view kEdgeRefIncludes = "/kythe/edge/ref/includes";
inline constexpr std::string_view kEdgeTyped = "/kythe/edge/typed";
inline constexpr std::string_view kEdgeOverrides = "/kythe/edge/overrides";

}  // namespace kythe
}  // namespace verilog

#endif  // VERIBLE_VERILOG_TOOLS_KYTHE_KYTHE_SCHEMA_CONSTANTS_H_
