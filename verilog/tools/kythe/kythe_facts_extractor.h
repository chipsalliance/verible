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
#include <string>
#include <utility>

#include "absl/strings/match.h"
#include "verilog/tools/kythe/indexing_facts_tree.h"
#include "verilog/tools/kythe/indexing_facts_tree_context.h"
#include "verilog/tools/kythe/indexing_facts_tree_extractor.h"
#include "verilog/tools/kythe/kythe_facts.h"

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

// Responsible for traversing IndexingFactsTree and processing its different
// nodes to produce kythe indexing facts.
class KytheFactsExtractor {
 public:
  explicit KytheFactsExtractor(absl::string_view file_path,
                               std::ostream* stream)
      : file_path_(file_path), stream_(stream) {}

  // Extracts kythe facts from the given IndexingFactsTree root.
  void ExtractKytheFacts(const IndexingFactNode&);

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

  // Container with a stack of Scopes to hold the accessible scopes during
  // traversing an Indexing Facts Tree.
  // This is used to get the definitions of some reference.
  //
  // This is modified during tree traversal because in case of entering new
  // scope the new scope is resolved first and after that it's added to the
  // containing scope and the next scope is being analyzed.
  class ScopeContext : public verible::AutoPopStack<std::vector<VName>*> {
   public:
    typedef verible::AutoPopStack<std::vector<VName>*> base_type;

    // member class to handle push and pop of stack safely
    using AutoPop = base_type::AutoPop;

    // returns the top VName of the stack
    std::vector<VName>& top() { return *ABSL_DIE_IF_NULL(base_type::top()); }

    // TODO(minatoma): improve performance and memory for this function.
    //
    // This function uses string matching to find the definition of some
    // variable in reverse order of the current scopes.
    //
    // Improvement can be replacing the string matching to comparison based on
    // integers or enums and reshaping the scope to be one vector instead of
    // vector of vectors.
    //
    // TODO(minatoma): consider using vector<pair<name, type>> for signature.
    //
    // Search function to get the VName of a definitions of some reference.
    // It loops over the scopes in reverse order and loops over every scope in
    // reverse order to find a definition for the variable with given prefix
    // signature.
    // e.g
    // {
    //    bar#module,
    //    foo#module,
    // }
    // {
    //    other scope,
    // }
    // Given bar#module it return the whole VName of that definition.
    // And if more than one match is found the first would be returned.
    const VName* SearchForDefinition(std::string prefix) {
      for (const auto& scope : verible::make_range(rbegin(), rend())) {
        for (const VName& vname :
             verible::make_range(scope->rbegin(), scope->rend())) {
          if (absl::StartsWith(vname.signature, prefix)) {
            return &vname;
          }
        }
      }
      return nullptr;
    }
  };

  // Searches for a definition suitable for the given reference within package
  // scope context of the given package name.
  const VName* SearchForDefinitionVNameInPackage(
      absl::string_view package_name, absl::string_view reference_name) const;

  // Resolves the tag of the given node and directs the flow to the appropriate
  // function to extract kythe facts for that node.
  void IndexingFactNodeTagResolver(const IndexingFactNode&);

  // Add the given VName to vnames_context (to be used in scope relative
  // signatures) and visits the children of the given node.
  void Visit(const IndexingFactNode& node, const VName& vname,
             std::vector<VName>& current_scope);

  // Directs the flow to the children of the given node creating new scope for
  // that node.
  void Visit(const IndexingFactNode& node, std::vector<VName>& current_scope);

  // Extracts Packages and saves its scope to package_scope_context to be used
  // for definition searching.
  void CreatePackageScopes(const IndexingFactNode&);

  // Extracts kythe facts from file node and returns it VName.
  VName ExtractFileFact(const IndexingFactNode&);

  // Extracts kythe facts from module instance node and returns it VName.
  VName ExtractModuleInstanceFact(const IndexingFactNode&);

  // Extracts kythe facts from module node and returns it VName.
  VName ExtractModuleFact(const IndexingFactNode&);

  // Extracts kythe facts from class node and returns it VName.
  VName ExtractClassFact(const IndexingFactNode&);

  // Extracts kythe facts from module port node and returns its VName.
  VName ExtractVariableDefinitionFact(const IndexingFactNode& node);

  // Extracts kythe facts from a module port reference node and returns its
  // VName.
  VName ExtractVariableReferenceFact(const IndexingFactNode& node);

  // Extracts Kythe facts from class instance node and return its VName.
  VName ExtractClassInstances(const IndexingFactNode& class_instance_fact_node);

  // Extracts kythe facts from a function or task node and returns its VName.
  VName ExtractFunctionOrTask(const IndexingFactNode& function_fact_node);

  // Extracts kythe facts from a function or task call node and returns its
  // VName.
  VName ExtractFunctionOrTaskCall(
      const IndexingFactNode& function_call_fact_node);

  // Extracts kythe facts from a package declaration node and returns its VName.
  VName ExtractPackageDeclaration(const IndexingFactNode& node);

  // Extracts kythe facts from package import node and returns its VName.
  VName ExtractPackageImport(const IndexingFactNode& node);

  // Extracts kythe facts from a macro definition node and returns its VName.
  VName ExtractMacroDefinition(const IndexingFactNode& macro_definition_node);

  // Extracts kythe facts from a macro call node and returns its VName.
  VName ExtractMacroCall(const IndexingFactNode& macro_call_node);

  // Extracts kythe facts from member reference statement and return its vname.
  // e.g pkg::member or class::member or class.member
  // The names are treated as anchors e.g:
  // pkg::member => {Anchor(pkg), Anchor(member)}
  // pkg::class_name::var => {Anchor(pkg), Anchor(class_name), Anchor(var)}
  VName ExtractMemberReference(const IndexingFactNode& member_reference_node);

  // Generates an anchor VName for kythe.
  VName PrintAnchorVName(const Anchor&);

  // Appends the signatures of given parent scope vname to make
  // signatures are unique relative to scopes.
  std::string CreateScopeRelativeSignature(absl::string_view,
                                           absl::string_view) const;

  // Appends the signatures of previous containing scope vname to make
  // signatures are unique relative to scopes.
  std::string CreateScopeRelativeSignature(absl::string_view) const;

  // Generates fact strings for Kythe facts.
  // Schema for this fact can be found here:
  // https://kythe.io/docs/schema/writing-an-indexer.html
  void GenerateFactString(const VName& vname, absl::string_view name,
                          absl::string_view value) const;

  // Generates edge strings for Kythe edges.
  // Schema for this edge can be found here:
  // https://kythe.io/docs/schema/writing-an-indexer.html
  void GenerateEdgeString(const VName& source, absl::string_view name,
                          const VName& target) const;

  // The verilog file name which the facts are extracted from.
  std::string file_path_;

  // Keeps track of VNames of ancestors as the visitor traverses the facts
  // tree.
  VNameContext vnames_context_;

  // Keeps track of scopes and definitions inside the scopes of ancestors as
  // the visitor traverses the facts tree.
  ScopeContext scope_context_;

  // Saved packages signatures alongside with their inner members.
  // This is used for resolving references to some variables after using import
  // pkg::*.
  // e.g
  // package pkg1;
  //   function my_fun(); endfunction
  //   class my_class; endclass
  // endpackage
  //
  // package pkg2;
  //   function my_fun(); endfunction
  //   class my_class; endclass
  // endpackage
  //
  // Creates the following:
  // {
  //   "pkg1": ["my_fun", "my_class"],
  //   "pkg2": ["my_fun", "my_class"]
  // }
  std::map<std::string, std::vector<VName>> package_scope_context_;

  // Output stream for capturing, redirecting, testing and verifying the
  // output.
  std::ostream* stream_;
};

std::ostream& operator<<(std::ostream&, const KytheFactsPrinter&);

}  // namespace kythe
}  // namespace verilog

#endif  // VERIBLE_VERILOG_TOOLS_KYTHE_KYTHE_FACTS_EXTRACTOR_H_
