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

#include "verible/verilog/analysis/verilog-excerpt-parse.h"

#include <functional>
#include <map>
#include <memory>
#include <string>
#include <string_view>

#include "absl/status/status.h"
#include "absl/strings/ascii.h"
#include "absl/strings/str_cat.h"
#include "verible/common/text/text-structure.h"
#include "verible/common/util/container-util.h"
#include "verible/common/util/logging.h"
#include "verible/verilog/analysis/verilog-analyzer.h"

// TODO(hzeller): All these are constructing strings with prefix and postfix.
// And often, these constructs are used in fallback situations in which we
// couldn't parse something and try again in a different setting. That means
// it generates a new string copy and forces full lexing again.
//
// Investigate if we can just take the original token stream and add tokens
// in front and back (should work if these tokens don't bring the lexer into
// a specific state)
// https://github.com/chipsalliance/verible/issues/1519

namespace verilog {

using verible::container::FindOrNull;

// Function template to create any mini-parser for Verilog.
// 'prolog' and 'epilog' are text that wrap around the 'text' argument to
// form a whole Verilog source.
// The returned analyzer's text structure will discard parsed information
// about the prolog and epilog, leaving only the substructure of interest.
static std::unique_ptr<VerilogAnalyzer> AnalyzeVerilogConstruct(
    std::string_view prolog, std::string_view text, std::string_view epilog,
    std::string_view filename,
    const VerilogPreprocess::Config &preprocess_config) {
  VLOG(2) << __FUNCTION__;
  CHECK(epilog.empty() || absl::ascii_isspace(epilog[0]))
      << "epilog text must begin with a whitespace to prevent unintentional "
         "token-joining and escaped-identifier extension.";
  const std::string analyze_text = absl::StrCat(prolog, text, epilog);
  // Disable parser directive comments because a specific parser
  // is already being selected.
  auto analyzer_ptr = std::make_unique<VerilogAnalyzer>(analyze_text, filename,
                                                        preprocess_config);

  if (!ABSL_DIE_IF_NULL(analyzer_ptr)->Analyze().ok()) {
    VLOG(2) << __FUNCTION__ << ": Analyze() failed.  code:\n" << analyze_text;
    // Continue to processes, even if there's an error, so that token
    // string_views can be properly rebased.
    // There may or may not be a formed syntax tree.
  }

  // Trim off prolog and epilog from internal text structure to make it look
  // as if only text were analyzed.
  analyzer_ptr->MutableData().FocusOnSubtreeSpanningSubstring(prolog.length(),
                                                              text.length());
  // TODO(b/129905498): Strengthen check on the resulting syntax tree root,
  // by qualifying it against an expected nonterminal node type(s).
  VLOG(2) << "end of " << __FUNCTION__;
  return analyzer_ptr;  // Let caller check analyzer_ptr's status.
}

std::unique_ptr<VerilogAnalyzer> AnalyzeVerilogPropertySpec(
    std::string_view text, std::string_view filename,
    const VerilogPreprocess::Config &preprocess_config) {
  return AnalyzeVerilogConstruct("module foo;\nproperty p;\n", text,
                                 "\nendproperty;\nendmodule;\n", filename,
                                 preprocess_config);
}

std::unique_ptr<VerilogAnalyzer> AnalyzeVerilogStatements(
    std::string_view text, std::string_view filename,
    const VerilogPreprocess::Config &preprocess_config) {
  return AnalyzeVerilogConstruct("function foo();\n", text, "\nendfunction\n",
                                 filename, preprocess_config);
}

std::unique_ptr<VerilogAnalyzer> AnalyzeVerilogExpression(
    std::string_view text, std::string_view filename,
    const VerilogPreprocess::Config &preprocess_config) {
  return AnalyzeVerilogConstruct("module foo;\nif (", text,
                                 " ) $error;\nendmodule\n", filename,
                                 preprocess_config);
  // $error in this context is an elaboration system task
  // The space before the ) is critical to accommodate escaped identifiers.
  // Without the space, lexing an escaped identifier would consume part
  // of the epilog text.
}

std::unique_ptr<VerilogAnalyzer> AnalyzeVerilogModuleBody(
    std::string_view text, std::string_view filename,
    const VerilogPreprocess::Config &preprocess_config) {
  return AnalyzeVerilogConstruct("module foo;\n", text, "\nendmodule\n",
                                 filename, preprocess_config);
}

std::unique_ptr<VerilogAnalyzer> AnalyzeVerilogClassBody(
    std::string_view text, std::string_view filename,
    const VerilogPreprocess::Config &preprocess_config) {
  return AnalyzeVerilogConstruct("class foo;\n", text, "\nendclass\n", filename,
                                 preprocess_config);
}

std::unique_ptr<VerilogAnalyzer> AnalyzeVerilogPackageBody(
    std::string_view text, std::string_view filename,
    const VerilogPreprocess::Config &preprocess_config) {
  return AnalyzeVerilogConstruct("package foo;\n", text, "\nendpackage\n",
                                 filename, preprocess_config);
}

std::unique_ptr<VerilogAnalyzer> AnalyzeVerilogLibraryMap(
    std::string_view text, std::string_view filename,
    const VerilogPreprocess::Config &preprocess_config) {
  // The prolog/epilog strings come from verilog.lex as token enums:
  // PD_LIBRARY_SYNTAX_BEGIN and PD_LIBRARY_SYNTAX_END.
  // These are used in verilog.y to enclose the complete library_description
  // grammar rule.
  return AnalyzeVerilogConstruct(
      "`____verible_verilog_library_begin____\n", text,
      "\n`____verible_verilog_library_end____\n", filename, preprocess_config);
}

std::unique_ptr<VerilogAnalyzer> AnalyzeVerilogWithMode(
    std::string_view text, std::string_view filename, std::string_view mode,
    const VerilogPreprocess::Config &preprocess_config) {
  static const auto *func_map =
      new std::map<std::string_view,
                   std::function<std::unique_ptr<VerilogAnalyzer>(
                       std::string_view, std::string_view,
                       const VerilogPreprocess::Config &)>>{
          {"parse-as-statements", &AnalyzeVerilogStatements},
          {"parse-as-expression", &AnalyzeVerilogExpression},
          {"parse-as-module-body", &AnalyzeVerilogModuleBody},
          {"parse-as-class-body", &AnalyzeVerilogClassBody},
          {"parse-as-package-body", &AnalyzeVerilogPackageBody},
          {"parse-as-property-spec", &AnalyzeVerilogPropertySpec},
          {"parse-as-library-map", &AnalyzeVerilogLibraryMap},
      };
  const auto *func_ptr = FindOrNull(*func_map, mode);
  if (!func_ptr) return nullptr;
  return (*func_ptr)(text, filename, preprocess_config);
}

}  // namespace verilog
