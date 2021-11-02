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
// These are the SymbolKinds defined by the LSP specifcation
// https://microsoft.github.io/language-server-protocol/specifications/specification-3-17/#symbolKind
// Interesting is, that not all of them are actually supported by all
// editors (playing around with Kate (https://kate-editor.org/)). So maybe
// these editors need to be made understanding.
//
// Deliberately no enum class as this enum is used as set of integer constants
// in the context of the Language Server Protocol, so static_cast<int>-ing
// them would be needed every time.
enum SymbolKind {
  File = 1,
  Module = 2,     // SV module. Kate does not seem to support that ?
  Namespace = 3,  // SV labelled begin/end blocks (Kate also has trouble here)
  Package = 4,    // SV package
  Class = 5,      // SV class
  Method = 6,     // SV class -> method
  Property = 7,
  Field = 8,
  Constructor = 9,
  Enum = 10,  // SV enum type
  Interface = 11,
  Function = 12,  // SV function
  Variable = 13,
  Constant = 14,
  String = 15,
  Number = 16,
  Boolean = 17,
  Array = 18,
  Object = 19,
  Key = 20,
  Null = 21,
  EnumMember = 22,  // SV enum member
  Struct = 23,
  Event = 24,
  Operator = 25,
  TypeParameter = 26,
};
}  // namespace lsp
}  // namespace verible
#endif  // COMMON_LSP_LSP_PROTOCOL_ENUMS_H
