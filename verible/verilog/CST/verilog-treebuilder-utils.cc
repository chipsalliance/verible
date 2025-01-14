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

#include "verible/verilog/CST/verilog-treebuilder-utils.h"

#include <string>
#include <string_view>

#include "absl/strings/str_cat.h"
#include "verible/common/text/concrete-syntax-leaf.h"
#include "verible/common/text/concrete-syntax-tree.h"
#include "verible/common/text/symbol.h"
#include "verible/common/text/token-info.h"
#include "verible/common/util/logging.h"

namespace verilog {

using verible::down_cast;

// Set of utility functions for embedded a statement into a certain context.
std::string EmbedInClass(std::string_view text) {
  return absl::StrCat("class test_class;\n", text, "\nendclass\n");
}

std::string EmbedInModule(std::string_view text) {
  return absl::StrCat("module test_module;\n", text, "\nendmodule\n");
}

std::string EmbedInFunction(std::string_view text) {
  return absl::StrCat("function integer test_function;\n", text,
                      "\nendfunction\n");
}

std::string EmbedInClassMethod(std::string_view text) {
  return EmbedInClass(EmbedInFunction(text));
}

void ExpectString(const verible::SymbolPtr &symbol, std::string_view expected) {
  const auto *leaf = down_cast<const verible::SyntaxTreeLeaf *>(symbol.get());
  CHECK(leaf != nullptr) << "expected: " << expected;
  CHECK_EQ(leaf->get().text(), expected);
}

}  // namespace verilog
