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

#include "verilog/CST/verilog_treebuilder_utils.h"

#include <memory>
#include <string>

#include "absl/strings/str_cat.h"
#include "absl/strings/string_view.h"
#include "common/text/concrete_syntax_leaf.h"
#include "common/text/concrete_syntax_tree.h"
#include "common/text/symbol.h"
#include "common/text/token_info.h"
#include "common/util/logging.h"
#include "verilog/CST/verilog_nonterminals.h"

namespace verilog {

using verible::down_cast;

// Set of utility functions for embedded a statement into a certain context.
std::string EmbedInClass(absl::string_view text) {
  return absl::StrCat("class test_class;\n", text, "\nendclass\n");
}

std::string EmbedInModule(absl::string_view text) {
  return absl::StrCat("module test_module;\n", text, "\nendmodule\n");
}

std::string EmbedInFunction(absl::string_view text) {
  return absl::StrCat("function integer test_function;\n", text,
                      "\nendfunction\n");
}

std::string EmbedInClassMethod(absl::string_view text) {
  return EmbedInClass(EmbedInFunction(text));
}

void ExpectString(const verible::SymbolPtr& symbol,
                  absl::string_view expected) {
  const auto* leaf = down_cast<const verible::SyntaxTreeLeaf*>(symbol.get());
  CHECK_NE(leaf, nullptr) << "expected: " << expected;
  CHECK_EQ(leaf->get().text(), expected);
}

}  // namespace verilog
