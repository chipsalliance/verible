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

#ifndef VERIBLE_VERILOG_TOOLS_EXTRACTOR_VERILOG_EXTRACTOR_TYPES_H
#define VERIBLE_VERILOG_TOOLS_EXTRACTOR_VERILOG_EXTRACTOR_TYPES_H

#include <iosfwd>
#include <string>

namespace verilog {

// IndexingFactType is a datatype that indicates the subclass of an
// IndexingFactNode
enum class IndexingFactType {
  // BEGIN GENERATE -- do not delete
  kFile,
  kMacro,
  kModule,
  kPackage,
  kMacroCall,
  kClass,
  kClassInstance,
  kModuleInstance,
  kVariableDefinition,
  kVariableReference,
  kFunctionOrTask,
  kFunctionCall,
  kPackageImport,
  kMemberReference,
  // END GENERATE -- do not delete
};

// Stringify's IndexingFactType. If IndexingFactType does not have a string
// definition, returns a string stating this.
std::string IndexingFactTypeEnumToString(IndexingFactType indexing_fact_type);

std::ostream& operator<<(std::ostream& stream, const IndexingFactType& e);

}  // namespace verilog

#endif  // VERIBLE_VERILOG_TOOLS_EXTRACTOR_VERILOG_EXTRACTOR_TYPES_H
