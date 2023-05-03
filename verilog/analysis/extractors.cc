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

#include "verilog/analysis/extractors.h"

#include <string>

#include "verilog/CST/identifier.h"
#include "verilog/CST/module.h"
#include "verilog/analysis/verilog-analyzer.h"

namespace verilog {
namespace analysis {

// Find all modules and collect interface names
absl::Status CollectInterfaceNames(
    absl::string_view content, std::set<std::string>* if_names,
    const VerilogPreprocess::Config& preprocess_config) {
  VLOG(1) << __FUNCTION__;

  const auto analyzer = verilog::VerilogAnalyzer::AnalyzeAutomaticMode(
      content, "<file>", preprocess_config);
  auto lex_status = ABSL_DIE_IF_NULL(analyzer)->LexStatus();
  auto parse_status = analyzer->ParseStatus();

  if (!lex_status.ok()) return lex_status;
  if (!parse_status.ok()) return parse_status;

  // TODO: Having a syntax error may still result in a partial syntax tree
  // to work with.  Currently, this utility has zero tolerance on syntax error.

  const auto& syntax_tree = analyzer->SyntaxTree();
  const auto mod_headers =
      FindAllModuleHeaders(*ABSL_DIE_IF_NULL(syntax_tree).get());

  // For each module, collect all identifiers under the module
  // header tree into if_names.
  for (const auto& mod_header : mod_headers) {
    const auto if_leafs = FindAllSymbolIdentifierLeafs(*mod_header.match);
    for (const auto& if_leaf_match : if_leafs) {
      const auto& if_leaf = SymbolCastToLeaf(*if_leaf_match.match);
      absl::string_view if_name = if_leaf.get().text();
      if_names->insert(std::string(if_name));  // TODO: use absl::string_view
    }
  }
  return absl::OkStatus();
}

}  // namespace analysis
}  // namespace verilog
