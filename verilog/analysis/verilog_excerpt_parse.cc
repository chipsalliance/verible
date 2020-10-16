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

#include "verilog/analysis/verilog_excerpt_parse.h"

#include <functional>
#include <map>
#include <memory>
#include <string>

#include "absl/memory/memory.h"
#include "absl/status/status.h"
#include "absl/strings/ascii.h"
#include "absl/strings/str_cat.h"
#include "absl/strings/string_view.h"
#include "common/text/text_structure.h"
#include "common/util/container_util.h"
#include "common/util/logging.h"
#include "verilog/analysis/verilog_analyzer.h"

namespace verilog {

using verible::container::FindOrNull;

// Function template to create any mini-parser for Verilog.
// 'prolog' and 'epilog' are text that wrap around the 'text' argument to
// form a whole Verilog source.
// The returned analyzer's text structure will discard parsed information
// about the prolog and epilog, leaving only the substructure of interest.
static std::unique_ptr<VerilogAnalyzer> AnalyzeVerilogConstruct(
    absl::string_view prolog, absl::string_view text, absl::string_view epilog,
    absl::string_view filename) {
  VLOG(2) << __FUNCTION__;
  CHECK(epilog.empty() || absl::ascii_isspace(epilog[0]))
      << "epilog text must begin with a whitespace to prevent unintentional "
         "token-joining and escaped-identifier extension.";
  const std::string analyze_text = absl::StrCat(prolog, text, epilog);
  // Disable parser directive comments because a specific parser
  // is already being selected.
  auto analyzer_ptr = absl::make_unique<VerilogAnalyzer>(
      analyze_text, filename,
      /* use_parser_directive_comments_ */ false);

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
    absl::string_view text, absl::string_view filename) {
  return AnalyzeVerilogConstruct("module foo;\nproperty p;\n", text,
                                 "\nendproperty;\nendmodule;\n", filename);
}

std::unique_ptr<VerilogAnalyzer> AnalyzeVerilogStatements(
    absl::string_view text, absl::string_view filename) {
  return AnalyzeVerilogConstruct("function foo();\n", text, "\nendfunction\n",
                                 filename);
}

std::unique_ptr<VerilogAnalyzer> AnalyzeVerilogExpression(
    absl::string_view text, absl::string_view filename) {
  return AnalyzeVerilogConstruct("module foo;\nif (", text,
                                 " ) $error;\nendmodule\n", filename);
  // $error in this context is an elaboration system task
  // The space before the ) is critical to accommodate escaped identifiers.
  // Without the space, lexing an escaped identifier would consume part
  // of the epilog text.
}

std::unique_ptr<VerilogAnalyzer> AnalyzeVerilogModuleBody(
    absl::string_view text, absl::string_view filename) {
  return AnalyzeVerilogConstruct("module foo;\n", text, "\nendmodule\n",
                                 filename);
}

std::unique_ptr<VerilogAnalyzer> AnalyzeVerilogClassBody(
    absl::string_view text, absl::string_view filename) {
  return AnalyzeVerilogConstruct("class foo;\n", text, "\nendclass\n",
                                 filename);
}

std::unique_ptr<VerilogAnalyzer> AnalyzeVerilogPackageBody(
    absl::string_view text, absl::string_view filename) {
  return AnalyzeVerilogConstruct("package foo;\n", text, "\nendpackage\n",
                                 filename);
}

std::unique_ptr<VerilogAnalyzer> AnalyzeVerilogLibraryMap(
    absl::string_view text, absl::string_view filename) {
  // The prolog/epilog strings come from verilog.lex as token enums:
  // PD_LIBRARY_SYNTAX_BEGIN and PD_LIBRARY_SYNTAX_END.
  // These are used in verilog.y to enclose the complete library_description
  // grammar rule.
  return AnalyzeVerilogConstruct(
      "`____verible_verilog_library_begin____\n", text,
      "\n`____verible_verilog_library_end____\n", filename);
}

std::unique_ptr<VerilogAnalyzer> AnalyzeVerilogWithMode(
    absl::string_view text, absl::string_view filename,
    absl::string_view mode) {
  static const auto* func_map =
      new std::map<absl::string_view,
                   std::function<std::unique_ptr<VerilogAnalyzer>(
                       absl::string_view, absl::string_view)>>{
          {"parse-as-statements", &AnalyzeVerilogStatements},
          {"parse-as-expression", &AnalyzeVerilogExpression},
          {"parse-as-module-body", &AnalyzeVerilogModuleBody},
          {"parse-as-class-body", &AnalyzeVerilogClassBody},
          {"parse-as-package-body", &AnalyzeVerilogPackageBody},
          {"parse-as-property-spec", &AnalyzeVerilogPropertySpec},
          {"parse-as-library-map", &AnalyzeVerilogLibraryMap},
      };
  auto func_ptr = FindOrNull(*func_map, mode);
  if (func_ptr == nullptr) return nullptr;
  return (*func_ptr)(text, filename);
}

}  // namespace verilog
