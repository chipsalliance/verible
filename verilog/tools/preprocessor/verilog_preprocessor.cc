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

#include <functional>
#include <iostream>
#include <string>

#include "absl/flags/flag.h"
#include "absl/flags/usage.h"
#include "absl/status/status.h"
#include "absl/strings/str_cat.h"
#include "absl/strings/string_view.h"
#include "common/lexer/token_stream_adapter.h"
#include "common/util/file_util.h"
#include "common/util/init_command_line.h"
#include "common/util/subcommand.h"
#include "verilog/analysis/flow_tree.h"
#include "verilog/analysis/verilog_analyzer.h"
#include "verilog/analysis/verilog_project.h"
#include "verilog/parser/verilog_lexer.h"
#include "verilog/parser/verilog_token_enum.h"
#include "verilog/preprocessor/verilog_preprocess.h"
#include "verilog/transform/strip_comments.h"

using verible::SubcommandArgsRange;

static absl::Status StripComments(absl::string_view source_file, std::istream&,
                                  std::ostream& outs, std::ostream&) {
  std::string source_contents;
  if (auto status = verible::file::GetContents(source_file, &source_contents);
      !status.ok()) {
    std::cerr << "ERROR: passed file can't be open\n.";
    return status;
  }

  // TODO(fangism): parse a subcommand-specific flag for replacement option
  // See documentation for 'replacement' arg of the library function:
  verilog::StripVerilogComments(source_contents, &outs, ' ');

  return absl::OkStatus();
}

static absl::Status MultipleCU(absl::string_view source_file, std::istream&,
                               std::ostream& outs, std::ostream&) {
  std::string source_contents;
  if (auto status = verible::file::GetContents(source_file, &source_contents);
      !status.ok()) {
    std::cerr << "ERROR: passed file can't be open\n.";
    return status;
  }
  verilog::VerilogPreprocess::Config config;
  config.filter_branches = 1;
  // config.expand_macros=1;
  verilog::VerilogPreprocess preprocessor(config);
  verilog::VerilogLexer lexer(source_contents);
  verible::TokenSequence lexed_sequence;
  for (lexer.DoNextToken(); !lexer.GetLastToken().isEOF();
       lexer.DoNextToken()) {
    // For now we will store the syntax tree tokens only, ignoring all the
    // white-space characters. however that should be stored to output the
    // source code just like it was, but with conditionals filtered.
    if (verilog::VerilogLexer::KeepSyntaxTreeTokens(lexer.GetLastToken()))
      lexed_sequence.push_back(lexer.GetLastToken());
  }
  verible::TokenStreamView lexed_streamview;
  // Initializing the lexed token stream view.
  InitTokenStreamView(lexed_sequence, &lexed_streamview);
  verilog::VerilogPreprocessData preprocessed_data =
      preprocessor.ScanStream(lexed_streamview);
  auto& preprocessed_stream = preprocessed_data.preprocessed_token_stream;
  for (auto u : preprocessed_stream)
    outs << *u << '\n';  // output the preprocessed tokens.
  for (auto& u : preprocessed_data.errors)
    outs << u.error_message << '\n';  // for debugging.
  //  parsing just as a trial
  std::string post_preproc;
  for (auto u : preprocessed_stream) post_preproc += std::string{u->text()};
  std::string source_view{post_preproc};
  verilog::VerilogAnalyzer analyzer(source_view, "file1", config);
  auto analyze_status = analyzer.Analyze();
  /* const auto& mydata = analyzer.Data().Contents(); */
  /* outs<<mydata; */

  return absl::OkStatus();
}

static absl::Status GenerateVariants(absl::string_view source_file,
                                     std::istream&, std::ostream& outs,
                                     std::ostream&) {
  std::string source_contents;
  if (auto status = verible::file::GetContents(source_file, &source_contents);
      !status.ok()) {
    std::cerr << "ERROR: passed file can't be open\n.";
    return status;  // check if the the file is not readable or doesn't exist.
  }

  // Lexing the input SV source code.
  verilog::VerilogLexer lexer(source_contents);
  verible::TokenSequence lexed_sequence;
  for (lexer.DoNextToken(); !lexer.GetLastToken().isEOF();
       lexer.DoNextToken()) {
    // For now we will store the syntax tree tokens only, ignoring all the
    // white-space characters. however that should be stored to output the
    // source code just like it was.
    if (verilog::VerilogLexer::KeepSyntaxTreeTokens(lexer.GetLastToken()))
      lexed_sequence.push_back(lexer.GetLastToken());
  }

  // Control flow tree constructing.
  verilog::FlowTree control_flow_tree(lexed_sequence);
  auto status =
      control_flow_tree
          .GenerateControlFlowTree();  // construct the control flow tree.
  status = control_flow_tree.DepthFirstSearch(
      0);  // traverse the tree by dfs from the root (node 0).

  // Printing the token streams of every possible variant.
  int variants_counter = 1;
  for (const auto& current_variant : control_flow_tree.variants_) {
    outs << "Variant number " << variants_counter++ << ":\n";
    for (auto token : current_variant) outs << token << '\n';
    puts("");
  }

  return absl::OkStatus();
}

ABSL_FLAG(bool, strip_comments, false,
          "Replaces comments with white-spaces in files passed.");
ABSL_FLAG(bool, multiple_cu, true,
          "Files are preprocessed in separate compilation units.");
ABSL_FLAG(bool, generate_variants, false,
          "Generates every possible variants w.r.t. compiler conditionals");

int main(int argc, char* argv[]) {
  const std::string usage =
      absl::StrCat("usage:\n", argv[0], " [options] file [<file>...]\n");
  // Process invocation args.
  const auto args = verible::InitCommandLine(usage, &argc, &argv);
  if (args.empty()) {
    std::cerr << absl::ProgramUsageMessage() << std::endl;
    return 1;
  }

  // Get flags.
  const bool strip_comments_flag = absl::GetFlag(FLAGS_strip_comments);
  const bool multiple_cu_flag = absl::GetFlag(FLAGS_multiple_cu);
  const bool generate_variants_flag = absl::GetFlag(FLAGS_generate_variants);

  // Check for flags and argument illegal usage.
  if (strip_comments_flag && generate_variants_flag) {  // Illegal usage.
    std::cerr << "ERROR: the flags passed shouldn't be used together.\n\n"
              << absl::ProgramUsageMessage() << std::endl;
    return 1;
  }
  auto parsed_file_list = verilog::ParseSourceFileListFromCommandline(args);
  if (!parsed_file_list.ok()) {
    std::cerr << parsed_file_list.status();
    return 1;
  }

  auto include_dirs = parsed_file_list->include_dirs;
  auto defines = parsed_file_list->defines;
  auto files = parsed_file_list->file_paths;

  if (files.empty()) {
    // No Files were passed (files should be passed as positional arguments).
    std::cerr << "ERROR: No System-Verilog files were passed.\n\n"
              << absl::ProgramUsageMessage() << std::endl;
    return 1;
  }

  // Select the operation mode and execute it.
  if (strip_comments_flag) {
    for (const auto& filename : files) {
      if (auto status = StripComments(filename, std::cin, std::cout, std::cerr);
          !status.ok()) {
        std::cerr << "ERROR: stripping comments failed.\n";
        return 1;
      }
    }
  } else if (generate_variants_flag) {
    for (const auto& filename : files) {
      if (auto status =
              GenerateVariants(filename, std::cin, std::cout, std::cerr);
          !status.ok()) {
        std::cerr << "ERROR: generating variants failed.\n";
        return 1;
      }
    }
  } else if (multiple_cu_flag) {
    for (const auto& filename : files) {
      if (auto status = MultipleCU(filename, std::cin, std::cout, std::cerr);
          !status.ok()) {
        std::cerr
            << "ERROR: preprocessing in multiple comiplation units failed.\n";
        return 1;
      }
    }
  }

  return 0;
}
