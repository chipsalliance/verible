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
#include "common/util/cmd_positional_arguments.h"
#include "common/util/file_util.h"
#include "common/util/init_command_line.h"
#include "common/util/subcommand.h"
#include "verilog/analysis/flow_tree.h"
#include "verilog/analysis/verilog_analyzer.h"
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

static absl::Status MultipleCU(
    absl::string_view source_file, std::istream&, std::ostream& outs,
    std::ostream&,
    const std::vector<std::pair<absl::string_view, absl::string_view>>& defines,
    const std::vector<absl::string_view>& include_dirs) {
  std::string source_contents;
  if (auto status = verible::file::GetContents(source_file, &source_contents);
      !status.ok()) {
    std::cerr << "ERROR: passed file can't be open\n.";
    return status;
  }
  verilog::VerilogPreprocess::Config config;
  config.filter_branches = 1;
  config.expand_macros = 1;
  config.include_files = 1;
  config.forward_define = 0;
  verilog::VerilogPreprocess preprocessor(config);

  // Add defines passed with +define+<foo> to the tool.
  for (auto define : defines) {
    if (auto status = preprocessor.AddDefineFromCmdLine(define); !status.ok()) {
      std::cerr << "ERROR: couldn't add macros passed to tool.\n";
      return status;
    }
  }

  // Add search paths for `includes.
  for (auto path : include_dirs) {
    if (auto status = preprocessor.AddIncludeDirFromCmdLine(path);
        !status.ok()) {
      std::cerr << "ERROR: couldn't add include search paths to tool.\n";
      return status;
    }
  }

  verilog::VerilogLexer lexer(source_contents);
  verible::TokenSequence lexed_sequence;
  for (lexer.DoNextToken(); !lexer.GetLastToken().isEOF();
       lexer.DoNextToken()) {
    // For now we will store the syntax tree tokens only, ignoring all the
    // white-space characters. however that should be stored to output the
    // source code just like it was, but with conditionals filtered.
    lexed_sequence.push_back(lexer.GetLastToken());
  }
  verible::TokenStreamView lexed_streamview;

  // Tyring to create a string_view between 2 consecutive tokens to preserve the
  // white-spaces as suggested by fangism. It worked but it solves a different
  // problem, where I have a non-whitespace token stream, and want to preserve
  // the whitespaces in between.
  /* auto it_end=lexed_sequence.end(); */
  /* auto it_prv=lexed_sequence.begin(); */
  /* for(auto it=lexed_sequence.begin();it!=it_end;it++){ */
  /*   if(it==it_prv) continue; */
  /*   std::cout<<it_prv->text(); */
  /*   absl::string_view
   * white_space(it_prv->text().end(),std::distance(it_prv->text().end(),it->text().begin()));
   */
  /*   std::cout<<white_space; */
  /*   it_prv=it; */
  /* } */
  /* std::cout<<(*(it_end-1)).text(); */

  // Initializing the lexed token stream view.
  InitTokenStreamView(lexed_sequence, &lexed_streamview);
  verilog::VerilogPreprocessData preprocessed_data =
      preprocessor.ScanStream(lexed_streamview);
  auto& preprocessed_stream = preprocessed_data.preprocessed_token_stream;
  for (auto u : preprocessed_stream)
    outs << u->text();  // output the preprocessed tokens.
  /* outs << *u << '\n';  // output the preprocessed tokens. */
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
    for (auto token : current_variant) outs << token.text();
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
      absl::StrCat("usage:\n", argv[0], " [options] file [<file>...]\n\n",
                   "options summary:\n",
                   "-multiple_cu: Files are preprocessed in separate "
                   "compilation units.\n",
                   "-strip_comments: Replaces one/multi-line comments with "
                   "equal white-spaces.\n",
                   "-generate_variants: Generates every possible variants "
                   "w.r.t. compiler conditionals.\n");

  // Process invocation args.
  const auto args = verible::InitCommandLine(usage, &argc, &argv);
  if (args.empty()) {
    std::cerr << absl::ProgramUsageMessage() << std::endl;
    return 1;
  }

  // Get flags.
  bool strip_comments_flag = absl::GetFlag(FLAGS_strip_comments);
  bool multiple_cu_flag = absl::GetFlag(FLAGS_multiple_cu);
  bool generate_variants_flag = absl::GetFlag(FLAGS_generate_variants);

  // Check for flags and argument illegal usage.
  if (strip_comments_flag && generate_variants_flag) {  // Illegal usage.
    std::cerr << "ERROR: the flags passed shouldn't be used together.\n\n"
              << absl::ProgramUsageMessage() << std::endl;
    return 1;
  }
  if (args.size() ==
      1) {  // No Files are passed (files are passed as positional arguments).
    std::cerr << "ERROR: No System-Verilog files were passed.\n\n"
              << absl::ProgramUsageMessage() << std::endl;
    return 1;
  }

  verible::CmdPositionalArguments positional_arguments(args);
  if (auto status = positional_arguments.ParseArgs(); !status.ok()) {
    std::cerr << "ERROR: parsing positional arguments failed.\n";
    return 1;
  }

  auto include_dirs = positional_arguments.GetIncludeDirs();
  auto defines = positional_arguments.GetDefines();
  auto files = positional_arguments.GetFiles();

  // Select the operation mode and execute it.
  if (strip_comments_flag) {
    for (auto filename : files) {
      if (auto status = StripComments(filename, std::cin, std::cout, std::cerr);
          !status.ok()) {
        std::cerr << "ERROR: stripping comments failed.\n";
        return 1;
      }
    }
  } else if (generate_variants_flag) {
    for (auto filename : files) {
      if (auto status =
              GenerateVariants(filename, std::cin, std::cout, std::cerr);
          !status.ok()) {
        std::cerr << "ERROR: generating variants failed.\n";
        return 1;
      }
    }
  } else if (multiple_cu_flag) {
    for (auto filename : files) {
      if (auto status = MultipleCU(filename, std::cin, std::cout, std::cerr,
                                   defines, include_dirs);
          !status.ok()) {
        std::cerr
            << "ERROR: preprocessing in multiple comiplation units failed.\n";
        return 1;
      }
    }
  }

  return 0;
}
