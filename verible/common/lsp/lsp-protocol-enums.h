// Copyright 2021 The Verible Authors.
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

#ifndef COMMON_LSP_LSP_PROTOCOL_ENUMS_H
#define COMMON_LSP_LSP_PROTOCOL_ENUMS_H

// Enums are currently not handled yet by jcxxgen, so enumeration are
// done separately here.
namespace verible {
namespace lsp {
// Deliberately no enum class as this enum is used as set of integer constants
// in the context of the Language Server Protocol, so static_cast<int>-ing
// them would be needed every time.
enum DiagnosticSeverity {
  kError = 1,
  kWarning = 2,
  kInfo = 3,
  kHint = 4,
};

// These are the SymbolKinds defined by the LSP specifcation
// https://microsoft.github.io/language-server-protocol/specifications/specification-3-17/#symbolKind
// Interesting is, that not all of them are actually supported by all
// editors (playing around with Kate (https://kate-editor.org/)). So maybe
// these editors need to be made understanding.
// As above, deliberately not enum class
enum SymbolKind {
  kFile = 1,
  kModule = 2,     // SV module. Kate does not seem to support that ?
  kNamespace = 3,  // SV labelled begin/end blocks (Kate also has trouble here)
  kPackage = 4,    // SV package
  kClass = 5,      // SV class
  kMethod = 6,     // SV class -> method
  kProperty = 7,
  kField = 8,
  kConstructor = 9,
  kEnum = 10,  // SV enum type
  kInterface = 11,
  kFunction = 12,  // SV function
  kVariable = 13,
  kConstant = 14,
  kString = 15,
  kNumber = 16,
  kBoolean = 17,
  kArray = 18,
  kObject = 19,
  kKey = 20,
  kNull = 21,
  kEnumMember = 22,  // SV enum member
  kStruct = 23,
  kEvent = 24,
  kOperator = 25,
  kTypeParameter = 26,
};

// These are the InlayHintKinds defined by the LSP specification.
// https://microsoft.github.io/language-server-protocol/specifications/lsp/3.17/specification/#inlayHintKind
enum InlayHintKind {
  kType = 1,
  kParameter = 2,
};
}  // namespace lsp
}  // namespace verible
#endif  // COMMON_LSP_LSP_PROTOCOL_ENUMS_H
