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

// Define Verilog parser methods.

#include "verible/verilog/parser/verilog-parser.h"

#include "absl/flags/flag.h"
#include "verible/common/parser/parser-param.h"
#include "verible/common/util/value-saver.h"

// This flag is referenced in verilog_parse_wrapper (verilog_parser.h),
// where it controls tracing of the parser.
ABSL_FLAG(bool, verilog_trace_parser, false, "Trace verilog parser");

namespace verilog {

// Bison-generated parsing function.
// Its implementation is in the generated file verilog.tab.cc,
// from verilog.y, by the genyacc rule for 'verilog_y'.
extern int verilog_parse(::verible::ParserParam *param);

// Controls yacc/bison's detailed verilog traces
extern int verilog_debug;  // symbol defined in bison-generated verilog.tab.cc

// parser wrapper to enable debug traces
int verilog_parse_wrapper(::verible::ParserParam *param) {
  const verible::ValueSaver<int> save_global_debug(
      &verilog_debug, absl::GetFlag(FLAGS_verilog_trace_parser) ? 1 : 0);
  return verilog_parse(param);
}

}  // namespace verilog
