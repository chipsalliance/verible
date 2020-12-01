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

#include "verilog/CST/verilog_tree_print.h"

#include <iostream>
#include <memory>
#include <string>

#include "absl/strings/str_cat.h"
#include "absl/strings/string_view.h"
#include "common/text/concrete_syntax_leaf.h"
#include "common/text/concrete_syntax_tree.h"
#include "common/text/symbol.h"
#include "common/text/token_info.h"
#include "common/util/value_saver.h"
#include "verilog/CST/verilog_nonterminals.h"  // for NodeEnumToString
#include "verilog/parser/verilog_parser.h"     // for verilog_symbol_name

#include "json/json.h"

namespace verilog {

VerilogPrettyPrinter::VerilogPrettyPrinter(std::ostream* output_stream,
                                           absl::string_view base)
    : verible::PrettyPrinter(
          output_stream,
          verible::TokenInfo::Context(base, [](std::ostream& stream, int e) {
            stream << verilog_symbol_name(e);
          })) {}

void VerilogPrettyPrinter::Visit(const verible::SyntaxTreeLeaf& leaf) {
  auto_indent() << "Leaf @" << child_rank_ << ' '
                << verible::TokenWithContext{leaf.get(), context_} << std::endl;
}

void VerilogPrettyPrinter::Visit(const verible::SyntaxTreeNode& node) {
  std::string tag_info = absl::StrCat(
      "(tag: ", NodeEnumToString(static_cast<NodeEnum>(node.Tag().tag)), ") ");

  auto_indent() << "Node @" << child_rank_ << ' ' << tag_info << "{"
                << std::endl;

  {
    const verible::ValueSaver<int> value_saver(&indent_, indent_ + 2);
    const verible::ValueSaver<int> rank_saver(&child_rank_, 0);
    for (const auto& child : node.children()) {
      // TODO(fangism): display nullptrs or child indices to show position.
      if (child) child->Accept(this);
      ++child_rank_;
    }
  }
  auto_indent() << "}" << std::endl;
}

void PrettyPrintVerilogTree(const verible::Symbol& root, absl::string_view base,
                            std::ostream* stream) {
  VerilogPrettyPrinter printer(stream, base);
  root.Accept(&printer);
}


VerilogTreeToJsonConverter::VerilogTreeToJsonConverter(absl::string_view base)
    : context_(verible::TokenInfo::Context(base)),
    json_(Json::objectValue), value_(&json_) {}

void VerilogTreeToJsonConverter::Visit(const verible::SyntaxTreeLeaf& leaf) {
  *value_ = Json::objectValue;
  (*value_)["symbol"] = verilog_symbol_name(leaf.get().token_enum());
  (*value_)["start"]  = leaf.get().left(context_.base);
  (*value_)["end"]    = leaf.get().right(context_.base);
}

void VerilogTreeToJsonConverter::Visit(const verible::SyntaxTreeNode& node) {
  *value_ = Json::objectValue;
  (*value_)["tag"] = NodeEnumToString(static_cast<NodeEnum>(node.Tag().tag));
  Json::Value &children = (*value_)["children"] = Json::arrayValue;
  children.resize(node.children().size());

  {
    const verible::ValueSaver<Json::Value*> value_saver(&value_, nullptr);
    unsigned child_rank = 0;
    for (const auto& child : node.children()) {
      value_ = &children[child_rank];
      if (child)
        child->Accept(this);
      ++child_rank;
    }
  }
}

Json::Value ConvertVerilogTreeToJson(const verible::Symbol& root, absl::string_view base) {
  VerilogTreeToJsonConverter converter(base);
  root.Accept(&converter);
  return std::move(converter.get_json());
}

}  // namespace verilog
