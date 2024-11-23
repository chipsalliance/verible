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

// The library contains parsers for various constructs in SystemVerilog.
//
// These mini-parsers actually wrap around the main verilog_parse() in a manner
// that make them look-and-feel like standalone parsers.

#ifndef VERIBLE_VERILOG_ANALYSIS_VERILOG_EXCERPT_PARSE_H_
#define VERIBLE_VERILOG_ANALYSIS_VERILOG_EXCERPT_PARSE_H_

#include <memory>

#include "absl/strings/string_view.h"
#include "verible/verilog/analysis/verilog-analyzer.h"
#include "verible/verilog/preprocessor/verilog-preprocess.h"

namespace verilog {

// The interface for these functions should all be:
//   std::unique_ptr<VerilogAnalyzer> (absl::string_view text,
//                                     absl::string_view filename,
//                                     const VerilogPreprocess::Config& config);
// );

// Analyzes test as Verilog property_spec
std::unique_ptr<VerilogAnalyzer> AnalyzeVerilogPropertySpec(
    absl::string_view text, absl::string_view filename,
    const VerilogPreprocess::Config &preprocess_config);

// Analyzes text as Verilog statements.
std::unique_ptr<VerilogAnalyzer> AnalyzeVerilogStatements(
    absl::string_view text, absl::string_view filename,
    const VerilogPreprocess::Config &preprocess_config);

// Analyzes text as any Verilog expression.
std::unique_ptr<VerilogAnalyzer> AnalyzeVerilogExpression(
    absl::string_view text, absl::string_view filename,
    const VerilogPreprocess::Config &preprocess_config);

// Analyzes text as any Verilog module body.
std::unique_ptr<VerilogAnalyzer> AnalyzeVerilogModuleBody(
    absl::string_view text, absl::string_view filename,
    const VerilogPreprocess::Config &preprocess_config);

// Analyzes text as any Verilog class body.
std::unique_ptr<VerilogAnalyzer> AnalyzeVerilogClassBody(
    absl::string_view text, absl::string_view filename,
    const VerilogPreprocess::Config &preprocess_config);

// Analyzes text as any Verilog package body.
std::unique_ptr<VerilogAnalyzer> AnalyzeVerilogPackageBody(
    absl::string_view text, absl::string_view filename,
    const VerilogPreprocess::Config &preprocess_config);

// TODO(fangism): analogous functions for: function, task, ...

// Analyzes text as any Verilog library map.
std::unique_ptr<VerilogAnalyzer> AnalyzeVerilogLibraryMap(
    absl::string_view text, absl::string_view filename,
    const VerilogPreprocess::Config &preprocess_config);

// Analyzes text in the selected parsing `mode`.
std::unique_ptr<VerilogAnalyzer> AnalyzeVerilogWithMode(
    absl::string_view text, absl::string_view filename, absl::string_view mode,
    const VerilogPreprocess::Config &preprocess_config);

}  // namespace verilog

#endif  // VERIBLE_VERILOG_ANALYSIS_VERILOG_EXCERPT_PARSE_H_
