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

// VerilogPrettyPrinter is a specialized printer for Verilog syntax trees.

#ifndef VERIBLE_VERILOG_CST_VERILOG_TREE_PRINT_H_
#define VERIBLE_VERILOG_CST_VERILOG_TREE_PRINT_H_

#include <iosfwd>

#include "absl/strings/string_view.h"
#include "verible/common/text/concrete-syntax-leaf.h"
#include "verible/common/text/concrete-syntax-tree.h"
#include "verible/common/text/symbol.h"
#include "verible/common/text/tree-utils.h"

namespace verilog {

class VerilogPrettyPrinter : public verible::PrettyPrinter {
 public:
  explicit VerilogPrettyPrinter(std::ostream *output_stream,
                                absl::string_view base);

  void Visit(const verible::SyntaxTreeLeaf &) final;
  void Visit(const verible::SyntaxTreeNode &) final;

  // void Visit(verible::SyntaxTreeNode*) final;
};

// Prints tree contained at root to stream
void PrettyPrintVerilogTree(const verible::Symbol &root, absl::string_view base,
                            std::ostream *stream);

}  // namespace verilog

#endif  // VERIBLE_VERILOG_CST_VERILOG_TREE_PRINT_H_
