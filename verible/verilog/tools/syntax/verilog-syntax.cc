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

// verilog_syntax is a simple command-line utility to check Verilog syntax
// for the given file.
//
// Example usage:
// verilog_syntax --verilog_trace_parser files...

#include <algorithm>
#include <functional>
#include <iomanip>
#include <iostream>
#include <memory>
#include <sstream>  // IWYU pragma: keep  // for ostringstream
#include <string>   // for string, allocator, etc
#include <utility>
#include <vector>

#include "absl/flags/flag.h"
#include "absl/status/status.h"
#include "absl/strings/str_cat.h"
#include "absl/strings/string_view.h"
#include "nlohmann/json.hpp"
#include "verible/common/strings/mem-block.h"
#include "verible/common/text/concrete-syntax-tree.h"
#include "verible/common/text/parser-verifier.h"
#include "verible/common/text/text-structure.h"
#include "verible/common/text/token-info-json.h"
#include "verible/common/text/token-info.h"
#include "verible/common/util/enum-flags.h"
#include "verible/common/util/file-util.h"
#include "verible/common/util/init-command-line.h"
#include "verible/common/util/iterator-range.h"
#include "verible/common/util/logging.h"  // for operator<<, LOG, LogMessage, etc
#include "verible/verilog/CST/verilog-tree-json.h"
#include "verible/verilog/CST/verilog-tree-print.h"
#include "verible/verilog/analysis/json-diagnostics.h"
#include "verible/verilog/analysis/verilog-analyzer.h"
#include "verible/verilog/analysis/verilog-excerpt-parse.h"
#include "verible/verilog/parser/verilog-parser.h"
#include "verible/verilog/parser/verilog-token-classifications.h"
#include "verible/verilog/parser/verilog-token-enum.h"
#include "verible/verilog/parser/verilog-token.h"

// Controls parser selection behavior
enum class LanguageMode {
  // May try multiple language options starting with SV, stops on first success.
  kAutoDetect,
  // Strict SystemVerilog 2017, no automatic trying of alternative parsing modes
  kSystemVerilog,
  // Verilog library map sub-language only.  LRM Chapter 33.
  kVerilogLibraryMap,
};

static const verible::EnumNameMap<LanguageMode> &LanguageModeStringMap() {
  static const verible::EnumNameMap<LanguageMode> kLanguageModeStringMap({
      {"auto", LanguageMode::kAutoDetect},
      {"sv", LanguageMode::kSystemVerilog},
      {"lib", LanguageMode::kVerilogLibraryMap},
  });
  return kLanguageModeStringMap;
}

static std::ostream &operator<<(std::ostream &stream, LanguageMode mode) {
  return LanguageModeStringMap().Unparse(mode, stream);
}

static bool AbslParseFlag(absl::string_view text, LanguageMode *mode,
                          std::string *error) {
  return LanguageModeStringMap().Parse(text, mode, error, "--flag value");
}

static std::string AbslUnparseFlag(const LanguageMode &mode) {
  std::ostringstream stream;
  stream << mode;
  return stream.str();
}

ABSL_FLAG(
    LanguageMode, lang, LanguageMode::kAutoDetect,  //
    "Selects language variant to parse.  Options:\n"
    "  auto: SystemVerilog-2017, but may auto-detect alternate parsing modes\n"
    "  sv: strict SystemVerilog-2017, with explicit alternate parsing modes\n"
    "  lib: Verilog library map language (LRM Ch. 33)\n");

ABSL_FLAG(
    bool, export_json, false,
    "Uses JSON for output. Intended to be used as an input for other tools.");
ABSL_FLAG(bool, printtree, false, "Whether or not to print the tree");
ABSL_FLAG(bool, printtokens, false, "Prints all lexed and filtered tokens");
ABSL_FLAG(bool, printrawtokens, false,
          "Prints all lexed tokens, including filtered ones.");
ABSL_FLAG(int, error_limit, 0,
          "Limit the number of syntax errors reported.  "
          "(0: unlimited)");
ABSL_FLAG(
    bool, verifytree, false,
    "Verifies that all tokens are parsed into tree, prints unmatched tokens");

ABSL_FLAG(bool, show_diagnostic_context, false,
          "prints an additional "
          "line on which the diagnostic was found,"
          "followed by a line with a position marker");

using nlohmann::json;
using verible::ConcreteSyntaxTree;
using verible::ParserVerifier;
using verible::TextStructureView;
using verilog::VerilogAnalyzer;

static std::unique_ptr<VerilogAnalyzer> ParseWithLanguageMode(
    const std::shared_ptr<verible::MemBlock> &content,
    absl::string_view filename,
    const verilog::VerilogPreprocess::Config &preprocess_config) {
  switch (absl::GetFlag(FLAGS_lang)) {
    case LanguageMode::kAutoDetect:
      return VerilogAnalyzer::AnalyzeAutomaticMode(content, filename,
                                                   preprocess_config);
    case LanguageMode::kSystemVerilog: {
      auto analyzer = std::make_unique<VerilogAnalyzer>(content, filename,
                                                        preprocess_config);
      const auto status = ABSL_DIE_IF_NULL(analyzer)->Analyze();
      if (!status.ok()) std::cerr << status.message() << std::endl;
      return analyzer;
    }
    case LanguageMode::kVerilogLibraryMap:
      return verilog::AnalyzeVerilogLibraryMap(content->AsStringView(),
                                               filename, preprocess_config);
  }
  return nullptr;
}

// Prints all tokens in view that are not matched in root.
static void VerifyParseTree(const TextStructureView &text_structure) {
  const ConcreteSyntaxTree &root = text_structure.SyntaxTree();
  if (root == nullptr) return;
  // TODO(fangism): this seems like a good method for TextStructureView.
  ParserVerifier verifier(*root, text_structure.GetTokenStreamView());
  auto unmatched = verifier.Verify();

  if (unmatched.empty()) {
    std::cout << std::endl << "All tokens matched." << std::endl;
  } else {
    std::cout << std::endl << "Unmatched Tokens:" << std::endl;
    for (const auto &token : unmatched) {
      std::cout << token << std::endl;
    }
  }
}

static bool ShouldIncludeTokenText(const verible::TokenInfo &token) {
  const verilog_tokentype tokentype =
      static_cast<verilog_tokentype>(token.token_enum());
  absl::string_view type_str = verilog::TokenTypeToString(tokentype);
  // Don't include token's text for operators, keywords, or anything that is a
  // part of Verilog syntax. For such types, TokenTypeToString() is equal to
  // token's text. Exception has to be made for identifiers, because things like
  // "PP_Identifier" or "SymbolIdentifier" (which are values returned by
  // TokenTypeToString()) could be used as Verilog identifier.
  return verilog::IsIdentifierLike(tokentype) || (token.text() != type_str);
}

static int AnalyzeOneFile(
    const std::shared_ptr<verible::MemBlock> &content,
    absl::string_view filename,
    const verilog::VerilogPreprocess::Config &preprocess_config,
    json *json_out) {
  int exit_status = 0;
  const auto analyzer =
      ParseWithLanguageMode(content, filename, preprocess_config);
  const auto lex_status = ABSL_DIE_IF_NULL(analyzer)->LexStatus();
  const auto parse_status = analyzer->ParseStatus();

  if (!lex_status.ok() || !parse_status.ok()) {
    const std::vector<std::string> syntax_error_messages(
        analyzer->LinterTokenErrorMessages(
            absl::GetFlag(FLAGS_show_diagnostic_context)));
    const int error_limit = absl::GetFlag(FLAGS_error_limit);
    int error_count = 0;
    if (!absl::GetFlag(FLAGS_export_json)) {
      const std::vector<std::string> syntax_error_messages(
          analyzer->LinterTokenErrorMessages(
              absl::GetFlag(FLAGS_show_diagnostic_context)));
      for (const auto &message : syntax_error_messages) {
        std::cout << message << std::endl;
        ++error_count;
        if (error_limit != 0 && error_count >= error_limit) break;
      }
    } else {
      (*json_out)["errors"] =
          verilog::GetLinterTokenErrorsAsJson(analyzer.get(), error_limit);
    }
    exit_status = 1;
  }
  const bool parse_ok = parse_status.ok();

  std::function<void(std::ostream &, int)> token_translator;
  if (!absl::GetFlag(FLAGS_export_json)) {
    token_translator = [](std::ostream &stream, int e) {
      stream << verilog::verilog_symbol_name(e);
    };
  } else {
    token_translator = [](std::ostream &stream, int e) {
      stream << verilog::TokenTypeToString(static_cast<verilog_tokentype>(e));
    };
  }
  const verible::TokenInfo::Context context(analyzer->Data().Contents(),
                                            token_translator);
  // Check for printtokens flag, print all filtered tokens if on.
  if (absl::GetFlag(FLAGS_printtokens)) {
    if (!absl::GetFlag(FLAGS_export_json)) {
      std::cout << std::endl << "Lexed and filtered tokens:" << std::endl;
      for (const auto &t : analyzer->Data().GetTokenStreamView()) {
        t->ToStream(std::cout, context) << std::endl;
      }
    } else {
      json &tokens = (*json_out)["tokens"] = json::array();
      const auto &token_stream = analyzer->Data().GetTokenStreamView();
      for (const auto &t : token_stream) {
        tokens.push_back(
            verible::ToJson(*t, context, ShouldIncludeTokenText(*t)));
      }
    }
  }

  // Check for printrawtokens flag, print all tokens if on.
  if (absl::GetFlag(FLAGS_printrawtokens)) {
    if (!absl::GetFlag(FLAGS_export_json)) {
      std::cout << std::endl << "All lexed tokens:" << std::endl;
      for (const auto &t : analyzer->Data().TokenStream()) {
        t.ToStream(std::cout, context) << std::endl;
      }
    } else {
      json &tokens = (*json_out)["rawtokens"] = json::array();
      const auto &token_stream = analyzer->Data().TokenStream();
      for (const auto &t : token_stream) {
        tokens.push_back(
            verible::ToJson(t, context, ShouldIncludeTokenText(t)));
      }
    }
  }

  const auto &text_structure = analyzer->Data();
  const auto &syntax_tree = text_structure.SyntaxTree();

  // check for printtree flag, and print tree if on
  if (absl::GetFlag(FLAGS_printtree) && syntax_tree != nullptr) {
    if (!absl::GetFlag(FLAGS_export_json)) {
      std::cout << std::endl
                << "Parse Tree"
                << (!parse_ok ? " (incomplete due to syntax errors):" : ":")
                << std::endl;
      verilog::PrettyPrintVerilogTree(*syntax_tree, analyzer->Data().Contents(),
                                      &std::cout);
    } else {
      (*json_out)["tree"] = verilog::ConvertVerilogTreeToJson(
          *syntax_tree, analyzer->Data().Contents());
    }
  }

  // Check for verifytree, verify tree and print unmatched if on.
  if (absl::GetFlag(FLAGS_verifytree)) {
    if (!parse_ok) {
      std::cout << std::endl
                << "Note: verifytree will fail because syntax errors caused "
                   "sections of text to be dropped during error-recovery."
                << std::endl;
    }
    VerifyParseTree(text_structure);
  }

  return exit_status;
}

int main(int argc, char **argv) {
  const auto usage =
      absl::StrCat("usage: ", argv[0], " [options] <file> [<file>...]");
  const auto args = verible::InitCommandLine(usage, &argc, &argv);

  json json_out;

  int exit_status = 0;
  // All positional arguments are file names.  Exclude program name.
  for (absl::string_view filename :
       verible::make_range(args.begin() + 1, args.end())) {
    auto content_status = verible::file::GetContentAsMemBlock(filename);
    if (!content_status.status().ok()) {
      std::cerr << content_status.status().message() << std::endl;
      exit_status = 1;
      continue;
    }
    std::shared_ptr<verible::MemBlock> content = std::move(*content_status);

    // TODO(hzeller): is there ever a situation in which we do not want
    // to use the preprocessor ?
    const verilog::VerilogPreprocess::Config preprocess_config{
        .filter_branches = true,
    };
    json file_json;
    int file_status =
        AnalyzeOneFile(content, filename, preprocess_config, &file_json);
    exit_status = std::max(exit_status, file_status);
    if (absl::GetFlag(FLAGS_export_json)) {
      json_out[std::string{filename.begin(), filename.end()}] =
          std::move(file_json);
    }
  }

  if (absl::GetFlag(FLAGS_export_json)) {
    std::cout << std::setw(2) << json_out << std::endl;
  }

  return exit_status;
}
