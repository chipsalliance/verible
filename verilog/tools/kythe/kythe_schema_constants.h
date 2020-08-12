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

#include <string>

namespace verible {
namespace kythe {

// Facts for kythe.
const std::string kFactText = "/kythe/text";
const std::string kFactNodeKind = "/kythe/node/kind";
const std::string kFactAnchorEnd = "/kythe/loc/end";
const std::string kFactAnchorStart = "/kythe/loc/start";

}  // namespace kythe
}  // namespace verible

#endif  // VERIBLE_VERILOG_TOOLS_KYTHE_KYTHE_SCHEMA_CONSTANTS_H_
