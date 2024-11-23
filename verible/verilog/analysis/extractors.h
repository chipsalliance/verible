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

// This file defines helper functions to gather the descriptions of all the
// rules in order to produce documentation (CLI help text and markdown) about
// the lint rules.

#ifndef VERIBLE_VERILOG_ANALYSIS_EXTRACTORS_H_
#define VERIBLE_VERILOG_ANALYSIS_EXTRACTORS_H_

#include <set>
#include <string>

#include "absl/status/status.h"
#include "absl/strings/string_view.h"
#include "verible/verilog/preprocessor/verilog-preprocess.h"

namespace verilog {
namespace analysis {

// Collect all identifiers under module header subtree in the CTS.
// This could be useful when interface names are required to be
// preserved.
absl::Status CollectInterfaceNames(
    absl::string_view content, std::set<std::string> *if_names,
    const verilog::VerilogPreprocess::Config &preprocess_config);

}  // namespace analysis
}  // namespace verilog

#endif  // VERIBLE_VERILOG_ANALYSIS_EXTRACTORS_H_
