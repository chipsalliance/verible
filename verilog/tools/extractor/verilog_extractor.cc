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

// verilog_extractor is a simple command-line utility to extract indexing facts
// from the given file.
//
// Example usage:
// verilog_extractor files...

#include <algorithm>
#include <iostream>
#include <iterator>
#include <map>
#include <memory>
#include <sstream>  // IWYU pragma: keep  // for ostringstream
#include <string>   // for string, allocator, etc
#include <utility>
#include <vector>

#include "absl/flags/flag.h"
#include "absl/status/status.h"
#include "absl/strings/str_cat.h"
#include "absl/strings/string_view.h"
#include "absl/types/span.h"  // for MakeArraySlice
#include "common/text/concrete_syntax_tree.h"
#include "common/text/symbol.h"
#include "common/text/parser_verifier.h"
#include "common/text/text_structure.h"
#include "common/text/token_info.h"
#include "common/util/file_util.h"
#include "common/util/init_command_line.h"
#include "common/util/logging.h"  // for operator<<, LOG, LogMessage, etc
#include "verilog/CST/verilog_tree_print.h"
#include "verilog/analysis/verilog_analyzer.h"
#include "verilog/parser/verilog_parser.h"
#include "verilog/CST/verilog_nonterminals.h"
#include "common/text/concrete_syntax_leaf.h"


ABSL_FLAG(bool, printextraction,

          false, "Whether or not to print the extracted facts");

enum class Type {
    File,
    Module,
    Input,
    Output,
    VariableName,
};

class Anchor {
public:
    Anchor(int startLocation, int endLocation, absl::string_view value) : startLocation_(startLocation),
                                                                          endLocation_(endLocation),
                                                                          value_(value) {};

    void print() {
        std::cout <<
                  "Anchor: {\n"
                  "StartLocation: " << startLocation_ << ",\n" <<
                  "EndLocation: " << endLocation_ << ",\n" <<
                  "Value: " << value_ << ",\n" <<
                  "}"
                  << std::endl;
    }

private:
    int startLocation_, endLocation_;
    absl::string_view value_;
};

class Block {
public:
    Block(Anchor *anchor, Type dataType) :
            anchor_(anchor), dataType_(dataType) {}

    void print() {
        std::cout << "{\n" << "Type: " << static_cast<int>(dataType_) << ",\n";
        if (anchor_ != nullptr) anchor_->print();

        for (auto child : children_) {
            child.print();
        }

        std::cout << "}\n";
    }

    std::vector<Block> Children() { return children_; };

    void AppendChild(Block entry) { children_.push_back(std::move(entry)); };

private:
    Anchor *anchor_;

    Type dataType_;

    std::vector<Block> children_;
};

using TagResolver = std::function<void(const verible::SyntaxTreeNode &, absl::string_view)>;
std::map<int, TagResolver> tagResolver;

std::vector<Block> out;

void InitializeTagResolver() {
    tagResolver[static_cast<int>(verilog::NodeEnum::kDescriptionList)] = [=](const verible::Symbol &node,
                                                                             absl::string_view base) {
        std::cout << "DesList" << std::endl;
    };

    tagResolver[static_cast<int>(verilog::NodeEnum::kModuleHeader)] = [=](
            const verible::SyntaxTreeNode &node, absl::string_view base) {

        std::cout << "In" << node.children()[0].get()->Tag().tag << " " << std::endl;

        for (const auto &child : node.children()) {
            if (child) {
                std::cout << "NOT NULL child" << std::endl;
                std::cout << child->Tag().kind << " " << child->Tag().tag << std::endl;

                if (child->Tag().tag == static_cast<int>(verilog::NodeEnum::kTimeLiteral)) {
                    auto module = dynamic_cast<verible::SyntaxTreeLeaf *>(child.get());

                    Anchor moduleAnchor = Anchor(module->get().left(base), module->get().right(base),
                                                 module->get().text());

                    Block moduleEntry = Block(&moduleAnchor, Type::Module);
                    out.push_back(moduleEntry);
                } else if (child->Tag().tag ==
                           static_cast<int>(verilog::NodeEnum::kNetVariableDeclarationAssign)) {
                    auto moduleName = dynamic_cast<verible::SyntaxTreeLeaf *>(child.get());

                    Anchor nameAnchor = Anchor(moduleName->get().left(base), moduleName->get().right(base),
                                               moduleName->get().text());
                    Block nameEntry = Block(&nameAnchor, Type::VariableName);
                    out.emplace_back(nameEntry);
                }
            } else {
                std::cout << "NULL child" << std::endl;
            }
        }

        std::cout << "Out" << std::endl;
    };

    std::cout << "Map Content" << std::endl;
    for (auto x : tagResolver) {
        std::cout << x.first << std::endl;
    }
}

void Extract(const verible::SyntaxTreeLeaf &node, absl::string_view base) {
    std::cout << "Start Leaf" << std::endl;
    std::cout << verilog::NodeEnumToString(static_cast<verilog::NodeEnum>(node.Tag().tag)) << " <<>> " << node.Tag().tag
              << " " << node.get()
              << std::endl;
    std::cout << "End Leaf" << std::endl;
    std::cout << std::endl;
}

void Extract(const verible::SyntaxTreeNode &node, absl::string_view base) {
    std::cout << "Start Node" << std::endl;
    std::cout << verilog::NodeEnumToString(static_cast<verilog::NodeEnum>(node.Tag().tag)) << "  ";
    std::cout << node.children().size() << std::endl;

    auto resolver = tagResolver.find(node.Tag().tag);
    if (resolver != tagResolver.end()) {
        resolver->second(node, base);
    }

    for (const auto &child : node.children()) {
        if (child) {
            if (child->Kind() == verible::SymbolKind::kNode) {
                Extract(verible::SymbolCastToNode(*child), base);
            } else {
                Extract(verible::SymbolCastToLeaf(*child), base);
            }
        }
    }

    std::cout << "End Node" << std::endl << std::endl;
}

static int ExtractOneFile(absl::string_view content,
                          absl::string_view filename) {
    int exit_status = 0;
    const auto analyzer =
            verilog::VerilogAnalyzer::AnalyzeAutomaticMode(content, filename);
    const auto lex_status = ABSL_DIE_IF_NULL(analyzer)->LexStatus();
    const auto parse_status = analyzer->ParseStatus();
    if (!lex_status.ok() || !parse_status.ok()) {
        const std::vector<std::string> syntax_error_messages(
                analyzer->LinterTokenErrorMessages());
        for (const auto &message : syntax_error_messages) {
            std::cout << message << std::endl;
        }
        exit_status = 1;
    }
    const bool parse_ok = parse_status.ok();

    const auto &text_structure = analyzer->Data();
    const auto &syntax_tree = text_structure.SyntaxTree();

    // check for printextraction flag, and print extraction if on
    if (absl::GetFlag(FLAGS_printextraction) && syntax_tree != nullptr) {
        std::cout << std::endl
                  << (!parse_ok ? " (incomplete due to syntax errors): " : "")
                  << std::endl;

        Extract(verible::SymbolCastToNode(*syntax_tree), analyzer->Data().Contents());

        std::cout << "===================================" << std::endl;
        for (auto entry : out) {
            entry.print();
            std::cout << '\n';
        }
    }

    return exit_status;
}

int main(int argc, char **argv) {
    const auto usage =
            absl::StrCat("usage: ", argv[0], " [options] <file> [<file>...]");
    const auto args = verible::InitCommandLine(usage, &argc, &argv);

    InitializeTagResolver();

    int exit_status = 0;
    // All positional arguments are file names.  Exclude program name.
    for (const auto filename :
            verible::make_range(args.begin() + 1, args.end())) {
        std::string content;
        if (!verible::file::GetContents(filename, &content).ok()) {
            exit_status = 1;
            continue;
        }

        int file_status = ExtractOneFile(content, filename);
        exit_status = std::max(exit_status, file_status);
    }
    return exit_status;
}
