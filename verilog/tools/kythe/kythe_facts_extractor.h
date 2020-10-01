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

#ifndef VERIBLE_VERILOG_TOOLS_KYTHE_KYTHE_FACTS_EXTRACTOR_H_
#define VERIBLE_VERILOG_TOOLS_KYTHE_KYTHE_FACTS_EXTRACTOR_H_

#include <map>
#include <set>
#include <string>
#include <utility>

#include "absl/strings/match.h"
#include "verilog/tools/kythe/indexing_facts_tree.h"
#include "verilog/tools/kythe/indexing_facts_tree_context.h"
#include "verilog/tools/kythe/indexing_facts_tree_extractor.h"
#include "verilog/tools/kythe/kythe_facts.h"
#include "verilog/tools/kythe/scope_resolver.h"

namespace verilog {
namespace kythe {

// Streamable printing class for kythe facts.
// Usage: stream << KytheFactsPrinter(*tree_root);
class KytheFactsPrinter {
 public:
  explicit KytheFactsPrinter(const IndexingFactNode& root) : root_(root) {}

  std::ostream& Print(std::ostream&) const;

 private:
  // The root of the indexing facts tree to extract kythe facts from.
  const IndexingFactNode& root_;
};

std::ostream& operator<<(std::ostream&, const KytheFactsPrinter&);

// Responsible for traversing IndexingFactsTree and processing its different
// nodes to produce kythe indexing facts.
// Iteratively extracts facts and keeps running until no new facts are found in
// the last iteration.
class KytheFactsExtractor {
 public:
  explicit KytheFactsExtractor(absl::string_view file_path,
                               std::ostream* stream)
      : file_path_(file_path), stream_(stream) {}

  // Extracts kythe facts from the given IndexingFactsTree root.
  void ExtractKytheFacts(const IndexingFactNode&);

  const FlattenedScopeResolver GetFlattenedScopeResolver() const {
    return flattened_scope_resolver_;
  }

 private:
  // Container with a stack of VNames to hold context of VNames during traversal
  // of an IndexingFactsTree.
  // This is used to generate to VNames inside the current scope.
  // e.g.
  // module foo();
  //  wire x; ==> x#variable#foo#module
  // endmodule: foo
  //
  // module bar();
  //  wire x; ==> x#variable#bar#module
  // endmodule: bar
  class VNameContext : public verible::AutoPopStack<const VName*> {
   public:
    typedef verible::AutoPopStack<const VName*> base_type;

    // member class to handle push and pop of stack safely
    using AutoPop = base_type::AutoPop;

    // returns the top VName of the stack
    const VName& top() const { return *ABSL_DIE_IF_NULL(base_type::top()); }
  };

  // Resolves the tag of the given node and directs the flow to the appropriate
  // function to extract kythe facts for that node.
  void IndexingFactNodeTagResolver(const IndexingFactNode&);

  // Determines whether to create a scope for this node or not and visits the
  // children.
  void Visit(const IndexingFactNode& node, const VName& vname);

  // Add the given VName to vnames_context (to be used in scope relative
  // signatures) and visits the children of the given node creating a new scope
  // for the given node.
  void Visit(const IndexingFactNode& node, const VName&, Scope&);

  // Directs the flow to the children of the given node.
  void Visit(const IndexingFactNode& node);

  // Determines whether or not to add the VName.
  void AddVNameToVerticalScope(IndexingFactType, const VName&);

  // Appends the extracted children vnames to the scope of the current node.
  void ConstructFlattenedScope(const IndexingFactNode&, const VName&,
                               const Scope&);

  // Determines whether or not to create a child of edge between the current
  // node and the previous node.
  void CreateChildOfEdge(IndexingFactType, const VName&);

  // Extracts kythe facts from file node and returns it VName.
  VName ExtractFileFact(const IndexingFactNode&);

  // Extracts kythe facts a reference to a user defined data type like class or
  // module.
  void ExtractDataTypeReference(const IndexingFactNode&);

  // Extracts kythe facts from module instance node and returns it VName.
  VName ExtractModuleInstance(const IndexingFactNode&);

  // Extracts kythe facts from module named port node e.g("m(.in1(a))").
  void ExtractModuleNamedPort(const IndexingFactNode&);

  // Extracts kythe facts from named param
  // e.g module_type #(.N(x)) extracts "N";
  void ExtractNamedParam(const IndexingFactNode&);

  // Extracts kythe facts from module node and returns it VName.
  VName ExtractModuleFact(const IndexingFactNode&);

  // Extracts kythe facts from class node and returns it VName.
  VName ExtractClass(const IndexingFactNode&);

  // Extracts kythe facts from module port node and returns its VName.
  VName ExtractVariableDefinition(const IndexingFactNode& node);

  // Extracts kythe facts from a module port reference node.
  void ExtractVariableReference(const IndexingFactNode& node);

  // Extracts Kythe facts from class instance node and return its VName.
  VName ExtractClassInstances(const IndexingFactNode& class_instance_fact_node);

  // Extracts kythe facts from a function or task node and returns its VName.
  VName ExtractFunctionOrTask(const IndexingFactNode& function_fact_node);

  // Extracts kythe facts from a function or task call node.
  void ExtractFunctionOrTaskCall(
      const IndexingFactNode& function_call_fact_node);

  // Extracts kythe facts from a package declaration node and returns its VName.
  VName ExtractPackageDeclaration(const IndexingFactNode& node);

  // Extracts kythe facts from package import node.
  void ExtractPackageImport(const IndexingFactNode& node);

  // Extracts kythe facts from a macro definition node and returns its VName.
  VName ExtractMacroDefinition(const IndexingFactNode& macro_definition_node);

  // Extracts kythe facts from a macro call node.
  void ExtractMacroCall(const IndexingFactNode& macro_call_node);

  // Extracts kythe facts from member reference statement.
  // e.g pkg::member or class::member or class.member
  // The names are treated as anchors e.g:
  // pkg::member => {Anchor(pkg), Anchor(member)}
  // pkg::class_name::var => {Anchor(pkg), Anchor(class_name), Anchor(var)}
  //
  // is_function_call determines whether this member reference is function call
  // or not e.g pkg::class1::function_x().
  void ExtractMemberReference(const IndexingFactNode& member_reference_node,
                              bool is_function_call);

  // Extracts kythe facts from param declaration node.
  VName ExtractParamDeclaration(const IndexingFactNode& param_declaration_node);

  // Generates an anchor VName for kythe.
  VName CreateAnchor(const Anchor&);

  // Appends the signatures of given parent scope vname to make
  // signatures are unique relative to scopes.
  Signature CreateScopeRelativeSignature(absl::string_view,
                                         const Signature&) const;

  // Appends the signatures of previous containing scope vname to make
  // signatures are unique relative to scopes.
  Signature CreateScopeRelativeSignature(absl::string_view) const;

  // Generates fact strings for Kythe facts.
  // Schema for this fact can be found here:
  // https://kythe.io/docs/schema/writing-an-indexer.html
  void CreateFact(const VName& vname, absl::string_view name,
                  absl::string_view value);

  // Generates edge strings for Kythe edges.
  // Schema for this edge can be found here:
  // https://kythe.io/docs/schema/writing-an-indexer.html
  void CreateEdge(const VName& source, absl::string_view name,
                  const VName& target);

  // The verilog file name which the facts are extracted from.
  std::string file_path_;

  // Keeps track of VNames of ancestors as the visitor traverses the facts
  // tree.
  VNameContext vnames_context_;

  // Keeps track of scopes and definitions inside the scopes of ancestors as
  // the visitor traverses the facts tree.
  VerticalScopeResolver vertical_scope_resolver_;

  // Keeps track and saves the explored scopes with a <key, value> and maps
  // every signature to its scope.
  FlattenedScopeResolver flattened_scope_resolver_;

  // Output stream for capturing, redirecting, testing and verifying the
  // output.
  std::ostream* stream_;

  // Used to save all the generated facts Uniquely.
  std::set<Fact> facts_;

  // Used to save all the generated edges Uniquely.
  std::set<Edge> edges_;
};

// Returns the file path which this tree contains facts about.
std::string GetFilePathFromRoot(const IndexingFactNode& root);

}  // namespace kythe
}  // namespace verilog

#endif  // VERIBLE_VERILOG_TOOLS_KYTHE_KYTHE_FACTS_EXTRACTOR_H_
