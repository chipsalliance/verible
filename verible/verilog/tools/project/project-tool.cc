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

#include <iostream>
#include <memory>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include "absl/flags/flag.h"
#include "absl/flags/usage.h"
#include "absl/status/status.h"
#include "absl/strings/str_cat.h"
#include "absl/strings/str_join.h"
#include "verible/common/util/init-command-line.h"
#include "verible/common/util/logging.h"
#include "verible/common/util/status-macros.h"
#include "verible/common/util/subcommand.h"
#include "verible/verilog/analysis/dependencies.h"
#include "verible/verilog/analysis/symbol-table.h"
#include "verible/verilog/analysis/verilog-filelist.h"
#include "verible/verilog/analysis/verilog-project.h"

// Note: These flags were copied over from
// verilog/tools/kythe/verilog_kythe_extractor.cc.
// TODO: standardize Verilog project flags across tools.
ABSL_FLAG(
    std::string, file_list_path, "",
    R"(The path to the file list which contains the names of SystemVerilog files.
    The files should be ordered by definition dependencies.)");

ABSL_FLAG(
    std::string, file_list_root, ".",
    R"(The absolute location which we prepend to the files in the file list (where listed files are relative to).)");

// TODO: support repeatable flag
ABSL_FLAG(
    std::vector<std::string>, include_dir_paths, {},
    R"(Comma separated paths of the directories used to look for included files.
Note: The order of the files here is important.
File search will stop at the the first found among the listed directories.
e.g --include_dir_paths directory1,directory2
if "A.sv" exists in both "directory1" and "directory2" the one in "directory1" is the one we will use.
)");

using verible::SubcommandArgsRange;
using verible::SubcommandEntry;

// Project configuration information expected to come from command-line
// invocation.
// TODO: refactor for re-use in verilog/tools/kythe/verilog_kythe_extractor.cc
struct VerilogProjectConfig {
  verilog::FileList file_list;
  // See --file_list_root above.
  std::string file_list_root;

  absl::Status LoadFromCommandline(const SubcommandArgsRange &args) {
    const std::vector<std::string_view> cmdline{args.begin(), args.end()};
    auto status = AppendFileListFromCommandline(cmdline, &file_list);
    if (!status.ok()) return status;

    // TODO(hzeller): phase out these flags but delegate things to
    // AppendFileListFromCommandline()
    auto include_paths = absl::GetFlag(FLAGS_include_dir_paths);
    file_list.preprocessing.include_dirs.insert(
        file_list.preprocessing.include_dirs.end(), include_paths.begin(),
        include_paths.end());

    file_list_root = absl::GetFlag(FLAGS_file_list_root);
    if (auto fl_path = absl::GetFlag(FLAGS_file_list_path); !fl_path.empty()) {
      return verilog::AppendFileListFromFile(fl_path, &file_list);
    }

    if (file_list.file_paths.empty()) {
      status = absl::InvalidArgumentError(
          "No files given or --file_list_path missing.");
    }
    return status;
  }
};

// Holds VerilogProject and SymbolTable together with proper object lifetime.
// TODO: refactor this for re-use with tools/kythe/verilog_kythe_extractor.cc.
struct ProjectSymbols {
  // From global absl flags.
  const VerilogProjectConfig &config;

  // This object must outlive 'symbol_table' (maintained by struct-ordering).
  std::unique_ptr<verilog::VerilogProject> project;

  // Unified symbol table.
  std::unique_ptr<verilog::SymbolTable> symbol_table;

  explicit ProjectSymbols(const VerilogProjectConfig &config)
      : config(config) {}

  // no copy, no move, no assign
  ProjectSymbols(const ProjectSymbols &) = delete;
  ProjectSymbols(ProjectSymbols &&) = delete;
  ProjectSymbols &operator=(const ProjectSymbols &) = delete;
  ProjectSymbols &operator=(ProjectSymbols &&) = delete;

  // Initializes a project, and opens listed files.
  absl::Status Load() {
    VLOG(1) << __FUNCTION__;
    // Load all source files first.
    // Error-out early if any files failed to open.
    project = std::make_unique<verilog::VerilogProject>(
        config.file_list_root, config.file_list.preprocessing.include_dirs);
    for (const auto &file : config.file_list.file_paths) {
      const auto open_status = project->OpenTranslationUnit(file);
      if (!open_status.ok()) return open_status.status();
    }

    // Initialize symbol table (empty).
    symbol_table = std::make_unique<verilog::SymbolTable>(project.get());
    return absl::OkStatus();
  }

  // Builds symbol table.
  void Build(std::vector<absl::Status> *build_statuses) {
    VLOG(1) << __FUNCTION__;
    // For now, ingest files in the order they were listed.
    // Without conflicting definitions in files, this order should not matter.
    for (const auto &file : config.file_list.file_paths) {
      symbol_table->BuildSingleTranslationUnit(file, build_statuses);
    }
  }

  // Resolves symbols.
  void Resolve(std::vector<absl::Status> *resolve_statuses) const {
    symbol_table->Resolve(resolve_statuses);
  }
};

static std::string JoinStatusMessages(
    const std::vector<absl::Status> &statuses) {
  return absl::StrCat(
      "[combined statuses]:\n",
      absl::StrJoin(
          statuses, "\n", [](std::string *out, const absl::Status &status) {
            out->append(status.message().begin(), status.message().end());
          }));
}

static absl::Status BuildAndShowSymbolTable(const SubcommandArgsRange &args,
                                            std::istream &ins,
                                            std::ostream &outs,
                                            std::ostream &errs) {
  VLOG(1) << __FUNCTION__;
  // Load configuration.
  VerilogProjectConfig config;
  RETURN_IF_ERROR(config.LoadFromCommandline(args));

  // Load project and files.
  ProjectSymbols project_symbols(config);
  RETURN_IF_ERROR(project_symbols.Load());

  // Build symbol table.
  std::vector<absl::Status> build_statuses;
  project_symbols.Build(&build_statuses);

  // Print.
  outs << "Symbol Table:" << std::endl;
  project_symbols.symbol_table->PrintSymbolDefinitions(outs) << std::endl;

  // Accumulate diagnostics.
  if (!build_statuses.empty()) {
    return absl::InvalidArgumentError(JoinStatusMessages(build_statuses));
  }

  return absl::OkStatus();
}

static absl::Status ResolveAndShowSymbolReferences(
    const SubcommandArgsRange &args, std::istream &ins, std::ostream &outs,
    std::ostream &errs) {
  VLOG(1) << __FUNCTION__;
  // Load configuration.
  VerilogProjectConfig config;
  RETURN_IF_ERROR(config.LoadFromCommandline(args));

  // Load project and files.
  ProjectSymbols project_symbols(config);
  RETURN_IF_ERROR(project_symbols.Load());

  // Build symbol table.
  std::vector<absl::Status> statuses;
  project_symbols.Build(&statuses);

  // Resolve symbols.
  project_symbols.Resolve(&statuses);

  // Print.
  outs << "Symbol References:" << std::endl;
  project_symbols.symbol_table->PrintSymbolReferences(outs) << std::endl;

  // Accumulate diagnostics.
  if (!statuses.empty()) {
    return absl::InvalidArgumentError(JoinStatusMessages(statuses));
  }

  return absl::OkStatus();
}

static absl::Status ShowFileDependencies(const SubcommandArgsRange &args,
                                         std::istream &ins, std::ostream &outs,
                                         std::ostream &errs) {
  VLOG(1) << __FUNCTION__;
  // Load configuration.
  VerilogProjectConfig config;
  RETURN_IF_ERROR(config.LoadFromCommandline(args));

  // Load project and files.
  ProjectSymbols project_symbols(config);
  RETURN_IF_ERROR(project_symbols.Load());

  // Build symbol table.
  std::vector<absl::Status> statuses;
  project_symbols.Build(&statuses);

  // Accumulate diagnostics.
  if (!statuses.empty()) {
    return absl::InvalidArgumentError(JoinStatusMessages(statuses));
  }

  // Partially resolve symbols.
  project_symbols.symbol_table->ResolveLocallyOnly();

  // Compute dependencies.
  const verilog::FileDependencies deps(*project_symbols.symbol_table);

  // Print.
  // TODO(hzeller): support various output options {human-readable,
  // machine-readable, etc.} using subcommand flags (b/164300992).
  // One variant should include tsort-consumable 2-column text.
  deps.PrintGraph(outs);
  return absl::OkStatus();
}

static const std::pair<std::string_view, SubcommandEntry> kCommands[] = {
    {"symbol-table-defs",        //
     {&BuildAndShowSymbolTable,  //
      R"(symbol-table-defs [project args]

Prints human-readable unified symbol table representation of all files.
This does not attempt to resolve symbols.

Input:
Project options, including source file list.
)"}},
    {"symbol-table-refs",               //
     {&ResolveAndShowSymbolReferences,  //
      R"(symbol-table-refs [project args]

Prints human-readable representation of symbol table references, after
attempting to resolve symbols.

Input:
Project options, including source file list.
)"}},
    {"file-deps",             //
     {&ShowFileDependencies,  //
      R"(file-deps [project args]

Prints human-readable representation of inter-file dependencies, e.g.

  "file1.sv" depends on "file2.sv" for symbols { X, Y, Z... }

Input:
Project options, including source file list.
)"}},
    // TODO: project-wide transformations like RenameSymbol()
    // TODO: symbol table name-completion demo
};

int main(int argc, char *argv[]) {
  // Create a registry of subcommands (locally, rather than as a static global).
  verible::SubcommandRegistry commands;
  for (const auto &entry : kCommands) {
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

  const auto &sub = commands.GetSubcommandEntry(args[1]);
  // Run the subcommand.
  const auto status = sub.main(command_args, std::cin, std::cout, std::cerr);
  if (!status.ok()) {
    std::cerr << status.message() << std::endl;
    return 1;
  }
  return 0;
}
