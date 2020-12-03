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
#include <vector>

#include "verilog/analysis/verilog_project.h"
#include "verilog/tools/kythe/indexing_facts_tree.h"
#include "verilog/tools/kythe/indexing_facts_tree_context.h"
#include "verilog/tools/kythe/indexing_facts_tree_extractor.h"
#include "verilog/tools/kythe/kythe_facts.h"
#include "verilog/tools/kythe/scope_resolver.h"

namespace verilog {
namespace kythe {

// Streamable printing class for kythe facts.
// Usage: stream << KytheFactsPrinter(IndexingFactNode);
class KytheFactsPrinter {
 public:
  KytheFactsPrinter(const IndexingFactNode& file_list_facts_tree,
                    const VerilogProject& project, bool debug = false)
      : file_list_facts_tree_(file_list_facts_tree),
        project_(&project),
        debug_(debug) {}

  std::ostream& Print(std::ostream&) const;

 private:
  // The root of the indexing facts tree to extract kythe facts from.
  const IndexingFactNode& file_list_facts_tree_;

  // This project manages the opening and path resolution of referenced files.
  const VerilogProject* const project_;

  // When debugging is enabled, print human-readable un-encoded text.
  const bool debug_;
};

std::ostream& operator<<(std::ostream&, const KytheFactsPrinter&);

// Extracted Kythe indexing facts and edges.
struct KytheIndexingData {
  // Extracted Kythe indexing facts.
  std::set<Fact> facts;

  // Extracted Kythe edges.
  std::set<Edge> edges;
};

// KytheFactsExtractor processes indexing facts for a single file.
// Responsible for traversing IndexingFactsTree and processing its different
// nodes to produce kythe indexing facts.
// Iteratively extracts facts and keeps running until no new facts are found in
// the last iteration.
class KytheFactsExtractor {
 public:
  KytheFactsExtractor(const VerilogSourceFile& source,
                      ScopeResolver* previous_files_scopes)
      : source_(&source), scope_resolver_(previous_files_scopes) {}

  // Extract facts across an entire project.
  // Extracts node tagged with kFileList where it iterates over every child node
  // tagged with kFile from the begining and extracts the facts for each file.
  // Currently, the file_list must be dependency-ordered for best results, that
  // is, definitions of symbols should be encountered earlier in the file list
  // than references to those symbols.
  // TODO(fangism): make this the only public function in this header, and move
  // the entire class definition into the .cc as implementation detail.
  static KytheIndexingData ExtractKytheFacts(const IndexingFactNode& file_list,
                                             const VerilogProject& project);

 private:
  // Container with a stack of VNames to hold context of VNames during traversal
  // of an IndexingFactsTree.
  // This is used to generate to VNames inside the current scope.
  // e.g.
  // module foo();
  //  wire x; ==> "foo#x"
  // endmodule: foo
  //
  // module bar();
  //  wire x; ==> "bar#x"
  // endmodule: bar
  class VNameContext : public verible::AutoPopStack<const VName*> {
   public:
    typedef verible::AutoPopStack<const VName*> base_type;

    // member class to handle push and pop of stack safely
    using AutoPop = base_type::AutoPop;

    // returns the top VName of the stack
    const VName& top() const { return *ABSL_DIE_IF_NULL(base_type::top()); }
  };

  // Returns the path-resolved name of the current source file.
  absl::string_view FileName() const;

  // Returns the string_view that spans the source file's entire text.
  absl::string_view SourceText() const;

  // Extracts kythe facts from the given IndexingFactsTree root.
  KytheIndexingData ExtractFile(const IndexingFactNode&);

  // Resolves the tag of the given node and directs the flow to the appropriate
  // function to extract kythe facts for that node.
  void IndexingFactNodeTagResolver(const IndexingFactNode&);

  // Determines whether to create a scope for this node or not and visits the
  // children.
  void VisitAutoConstructScope(const IndexingFactNode& node,
                               const VName& vname);

  // Add the given VName to vnames_context (to be used in scope relative
  // signatures) and visits the children of the given node creating a new scope
  // for the given node.
  void VisitUsingVName(const IndexingFactNode& node, const VName&, Scope&);

  // Directs the flow to the children of the given node.
  void Visit(const IndexingFactNode& node);

  // Determines whether or not to add the definition to the current scope.
  void AddDefinitionToCurrentScope(IndexingFactType, const VName&);

  // Appends the extracted children vnames to the scope of the current node.
  void ConstructScope(const IndexingFactNode&, const VName&, Scope&);

  // Determines whether or not to create a child of edge between the current
  // node and the previous node.
  void CreateChildOfEdge(IndexingFactType, const VName&);

  //=================================================================
  // Declare* methods create facts (some edges) and may introduce new scopes.
  // Reference* methods only create edges, and may not modify scopes' contents.

  // Extracts kythe facts from file node and returns it VName.
  VName DeclareFile(const IndexingFactNode&);

  // Extracts kythe facts for a reference to some user defined data type like
  // class or module.
  void ReferenceDataType(const IndexingFactNode&);

  // Extracts kythe facts for a user defined type like enum or struct.
  VName ExtractTypeDefine(const IndexingFactNode&);

  // Extracts kythe facts for a constant like member in enums.
  VName DeclareConstant(const IndexingFactNode&);

  // Extracts kythe facts for structs or unions.
  VName DeclareStructOrUnion(const IndexingFactNode&);

  // Extracts kythe facts for a type declaration.
  VName DeclareTypedef(const IndexingFactNode&);

  // Extracts kythe facts from interface node and returns it VName.
  VName DeclareInterface(const IndexingFactNode& interface_fact_node);

  // Extracts kythe facts from program node and returns it VName.
  VName DeclareProgram(const IndexingFactNode& program_fact_node);

  // Extracts kythe facts from module named port node e.g("m(.in1(a))").
  void ReferenceModuleNamedPort(const IndexingFactNode&);

  // Extracts kythe facts from named param
  // e.g module_type #(.N(x)) extracts "N";
  void ReferenceNamedParam(const IndexingFactNode&);

  // Extracts kythe facts from module node and returns it VName.
  VName DeclareModule(const IndexingFactNode&);

  // Extracts kythe facts from class node and returns it VName.
  VName DeclareClass(const IndexingFactNode&);

  // Extracts kythe facts from class extends node.
  void ReferenceExtendsInheritance(const IndexingFactNode&);

  // Extracts kythe facts from module instance, class instance, variable
  // definition and param declaration nodes and returns its VName.
  VName DeclareVariable(const IndexingFactNode& node);

  // Extracts kythe facts from a module port reference node.
  void ReferenceVariable(const IndexingFactNode& node);

  // Creates a new anonymous scope for if conditions and loops.
  VName DeclareAnonymousScope(const IndexingFactNode& temp_scope);

  // Extracts kythe facts from a function or task node and returns its VName.
  VName DeclareFunctionOrTask(const IndexingFactNode& function_fact_node);

  // Extracts kythe facts from a function or task call node.
  void ReferenceFunctionOrTaskCall(
      const IndexingFactNode& function_call_fact_node);

  // Extracts kythe facts from a package declaration node and returns its VName.
  VName DeclarePackage(const IndexingFactNode& node);

  // Extracts kythe facts from package import node.
  void ReferencePackageImport(const IndexingFactNode& node);

  // Extracts kythe facts from a macro definition node and returns its VName.
  VName DeclareMacroDefinition(const IndexingFactNode& macro_definition_node);

  // Extracts kythe facts from a macro call node.
  void ReferenceMacroCall(const IndexingFactNode& macro_call_node);

  // Extracts kythe facts from a "`include" node.
  void ReferenceIncludeFile(const IndexingFactNode& include_node);

  // Extracts kythe facts from member reference statement.
  // e.g pkg::member or class::member or class.member
  // The names are treated as anchors e.g:
  // pkg::member => {Anchor(pkg), Anchor(member)}
  // pkg::class_name::var => {Anchor(pkg), Anchor(class_name), Anchor(var)}
  void ReferenceMember(const IndexingFactNode& member_reference_node);

  //============ end of Declare*, Reference* methods ===================

  // Create "ref" edges that point from the given anchors to the given
  // definitions in order.
  void CreateAnchorReferences(
      const std::vector<Anchor>& anchors,
      const std::vector<std::pair<const VName*, const Scope*>>& definitions);

  // Generates an anchor VName for kythe.
  VName CreateAnchor(const Anchor&);

  // Appends the signatures of previous containing scope vname to make
  // signatures unique relative to scopes.
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

  // The verilog source file from which facts are extracted.
  const VerilogSourceFile* const source_;

 private:  // data
  // Keeps track of VNames of ancestors as the visitor traverses the facts
  // tree.
  VNameContext vnames_context_;

  // Keeps track and saves the explored scopes with a <key, value> and maps
  // every signature to its scope.
  ScopeResolver* scope_resolver_;

  // Contains resulting kythe facts and edges to output.
  KytheIndexingData kythe_data_;
};

}  // namespace kythe
}  // namespace verilog

#endif  // VERIBLE_VERILOG_TOOLS_KYTHE_KYTHE_FACTS_EXTRACTOR_H_
