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

// Implementation of VerilogPrettyPrinter

#include "verible/verilog/CST/verilog-tree-print.h"

#include <iostream>
#include <memory>
#include <string>

#include "absl/strings/str_cat.h"
#include "absl/strings/string_view.h"
#include "verible/common/text/concrete-syntax-leaf.h"
#include "verible/common/text/concrete-syntax-tree.h"
#include "verible/common/text/symbol.h"
#include "verible/common/text/token-info.h"
#include "verible/common/text/tree-utils.h"
#include "verible/common/util/value-saver.h"
#include "verible/verilog/CST/verilog-nonterminals.h"  // for NodeEnumToString
#include "verible/verilog/parser/verilog-parser.h"  // for verilog_symbol_name

namespace verilog {

VerilogPrettyPrinter::VerilogPrettyPrinter(std::ostream *output_stream,
                                           absl::string_view base)
    : verible::PrettyPrinter(
          output_stream,
          verible::TokenInfo::Context(base, [](std::ostream &stream, int e) {
            stream << verilog_symbol_name(e);
          })) {}

void VerilogPrettyPrinter::Visit(const verible::SyntaxTreeLeaf &leaf) {
  auto_indent() << "Leaf @" << child_rank_ << ' '
                << verible::TokenWithContext{leaf.get(), context_} << std::endl;
}

void VerilogPrettyPrinter::Visit(const verible::SyntaxTreeNode &node) {
  std::string tag_info = absl::StrCat(
      "(tag: ", NodeEnumToString(static_cast<NodeEnum>(node.Tag().tag)), ") ");

  auto_indent() << "Node @" << child_rank_ << ' ' << tag_info << "{"
                << std::endl;

  {
    const verible::ValueSaver<int> value_saver(&indent_, indent_ + 2);
    const verible::ValueSaver<int> rank_saver(&child_rank_, 0);
    for (const auto &child : node.children()) {
      // TODO(fangism): display nullptrs or child indices to show position.
      if (child) child->Accept(this);
      ++child_rank_;
    }
  }
  auto_indent() << "}" << std::endl;
}

void PrettyPrintVerilogTree(const verible::Symbol &root, absl::string_view base,
                            std::ostream *stream) {
  VerilogPrettyPrinter printer(stream, base);
  root.Accept(&printer);
}

}  // namespace verilog
