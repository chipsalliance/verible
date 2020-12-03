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

#ifndef VERIBLE_VERILOG_ANALYSIS_JSON_DIAGNOSTICS_H_
#define VERIBLE_VERILOG_ANALYSIS_JSON_DIAGNOSTICS_H_

#include "json/value.h"

namespace verilog {

class VerilogAnalyzer;

// Returns JSON list with information about errors.
Json::Value GetLinterTokenErrorsAsJson(const verilog::VerilogAnalyzer* analyzer);

}  // namespace verilog

#endif  // VERIBLE_VERILOG_ANALYSIS_JSON_DIAGNOSTICS_H_
