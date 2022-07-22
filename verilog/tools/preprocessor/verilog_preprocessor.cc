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
#include "verilog/parser/verilog_lexer.h"
#include "verilog/parser/verilog_token_enum.h"
#include "verilog/preprocessor/verilog_preprocess.h"
#include "verilog/transform/strip_comments.h"

using verible::SubcommandArgsRange;

static absl::Status StripComments(const SubcommandArgsRange& args,
                                  std::istream&, std::ostream& outs,
                                  std::ostream&) {
  if (args.empty()) {
    return absl::InvalidArgumentError(
        "Missing file argument.  Use '-' for stdin.");
  }
  const char* source_file = args[0];
  std::string source_contents;
  if (auto status = verible::file::GetContents(source_file, &source_contents);
      !status.ok()) {
    return status;
  }

  char replace_char;
  if (args.size() == 1) {
    replace_char = ' ';
  } else if (args.size() == 2) {
    absl::string_view replace_str(args[1]);
    if (replace_str.empty())
      replace_char = '\0';
    else if (replace_str.length() == 1)
      replace_char = replace_str[0];
    else
      return absl::InvalidArgumentError(
          "Replacement must be a single character.");
  } else {
    return absl::InvalidArgumentError("Too many arguments.");
  }

  verilog::StripVerilogComments(source_contents, &outs, replace_char);

  return absl::OkStatus();
}

static absl::Status MultipleCU(const char* source_file, std::istream&,
                               std::ostream& outs, std::ostream&) {
  std::string source_contents;
  if (auto status = verible::file::GetContents(source_file, &source_contents);
      !status.ok()) {
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

  /* TODO(karimtera): regarding conditionals
   1) Modify VerilogPreprocess config to have a configuration to generate SV
   source codes for all possible variants. 2) Then use parser, directly from
   VerilogAnalyzer or from VerilogParser to have less dependences. 3) Now, we
   should have multiple trees, we need to merge them as described by Tom in
   Verible's issue. 4) Finally, travese the tree and output the chosen path
   based on definitions.
  */
  return absl::OkStatus();
}

static absl::Status GenerateVariants(const char* source_file, std::istream&,
                                     std::ostream& outs, std::ostream&) {
  std::string source_contents;
  if (auto status = verible::file::GetContents(source_file, &source_contents);
      !status.ok()) {
    return status;
  }
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

  verilog::FlowTree control_flow_tree(lexed_sequence);
  auto status = control_flow_tree.GenerateControlFlowTree();
  status = control_flow_tree.DepthFirstSearch(0);
  int cnt = 1;
  for (const auto& u : control_flow_tree.variants_) {
    outs << "Variant number " << cnt++ << ":\n";
    for (auto k : u) outs << k << '\n';
    puts("");
  }

  return absl::OkStatus();
}

static const std::pair<absl::string_view, SubcommandEntry> kCommands[] = {
    {"strip-comments",  //
     {&StripComments,   //
      R"(strip-comments file [replacement-char]

Inputs:
  'file' is a Verilog or SystemVerilog source file.
  Use '-' to read from stdin.

  'replacement-char' is a character to replace comments with.
  If not given, or given as a single space character, the comment contents and
  delimiters are replaced with spaces.
  If an empty string, the comment contents and delimiters are deleted. Newlines
  are not deleted.
  If a single character, the comment contents are replaced with the character.

Output: (stdout)
  Contents of original file with // and /**/ comments removed.
)"}},

    {"multiple-cu",  //
     {&MultipleCU,   //
      R"(multiple-cu file1 file2 ...

Input:
  'file's are Verilog or SystemVerilog source files.
  each one will be preprocessed in a separate compilation unit.
Output: (stdout)
  Contents of original file with compiler directives interpreted.
)"}},

    {"generate-variants",  //
     {&GenerateVariants,   //
      R"(bypass-conditionals file 

Input:
 'file' is Verilog or SystemVerilog source file.
Output: (stdout)
  Every possible source variants.
)"}},
};

int main(int argc, char* argv[]) {
  // Create a registry of subcommands (locally, rather than as a static global).
  verible::SubcommandRegistry commands;
  for (const auto& entry : kCommands) {
    const auto status = commands.RegisterCommand(entry.first, entry.second);
    if (!status.ok()) {
      std::cerr << status.message() << std::endl;
      return 2;  // fatal error
    }
  }

  const std::string usage = absl::StrCat("usage: ", argv[0],
                                         " command args...\n"
                                         "available commands:\n",
                                         commands.ListCommands());

  // Process invocation args.
  const auto args = verible::InitCommandLine(usage, &argc, &argv);
  if (args.size() == 1) {
    std::cerr << absl::ProgramUsageMessage() << std::endl;
    return 1;
  }
  // args[0] is the program name
  // args[1] is the subcommand
  // subcommand args start at [2]
  const SubcommandArgsRange command_args(args.cbegin() + 2, args.cend());

  const auto& sub = commands.GetSubcommandEntry(args[1]);
  // Run the subcommand.
  const auto status = sub.main(command_args, std::cin, std::cout, std::cerr);
  if (!status.ok()) {
    std::cerr << status.message() << std::endl;
    return 1;
  }
  return 0;
}
