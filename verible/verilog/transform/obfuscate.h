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

#ifndef VERIBLE_VERILOG_TRANSFORM_OBFUSCATE_H_
#define VERIBLE_VERILOG_TRANSFORM_OBFUSCATE_H_

#include <iosfwd>
#include <string>
#include <string_view>

#include "absl/status/status.h"
#include "verible/common/strings/obfuscator.h"

namespace verilog {

// Returns an identifier ([alpha][alnum]*) of equal length to input, and
// makes sure that it is a valid symbol identifier, not another Verilog keyword.
std::string RandomEqualLengthSymbolIdentifier(std::string_view in);

// Obfuscates Verilog code.  Identifiers are randomized as equal length
// replacements, and transformations are recorded (in subst) and re-applied
// to the same strings seen.  Input code only needs to be lexically valid,
// not necessary syntactically valid.  Transformations apply to macro
// arguments and macro definition bodies.
// Returned status signals success or possible an internal error.
absl::Status ObfuscateVerilogCode(std::string_view content,
                                  std::ostream *output,
                                  verible::IdentifierObfuscator *subst);

}  // namespace verilog

#endif  // VERIBLE_VERILOG_TRANSFORM_OBFUSCATE_H_
