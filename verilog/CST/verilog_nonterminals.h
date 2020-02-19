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

// Enumerations for SystemVerilog concrete syntax tree nodes.
// Do not rely on the numeric values of these enums.
// Enumeration values are not guaranteed to be stable across versions;
// enumerations are subject to change/removal.

#ifndef VERIBLE_VERILOG_CST_VERILOG_NONTERMINALS_H_
#define VERIBLE_VERILOG_CST_VERILOG_NONTERMINALS_H_

#include <iosfwd>
#include <string>

#include "common/text/constants.h"

namespace verilog {

// Language-specific tags for various nonterminals should be nonzero.
enum class NodeEnum {
  // BEGIN GENERATE -- do not delete
  kUntagged = verible::kUntagged,
  kDescription,
  kDescriptionList,
  kPackageItemList,
  kPackageDeclaration,
  kClassDeclaration,
  kClassHeader,
  kClassItems,
  kClassConstructor,
  kClassConstructorPrototype,
  kFunctionDeclaration,
  kFunctionHeader,
  kFunctionPrototype,
  kTaskDeclaration,
  kTaskHeader,
  kTaskPrototype,
  kConfigDeclaration,
  kDesignStatement,
  kDesignStatementItems,
  kCellIdentifier,
  kConfigRuleStatementList,
  kConfigRuleStatement,
  kPreprocessorBalancedConfigRuleStatements,
  kInstClause,
  kCellClause,
  kLiblistClause,
  kUseClause,
  kExpression,
  kExpressionList,
  kWithGroup,
  kBinaryExpression,
  kTernaryExpression,
  kUnaryPrefixExpression,
  kReference,
  kFunctionCall,
  kReferenceCallBase,
  kMethodCallExtension,
  kHierarchyExtension,
  kRandomizeMethodCallExtension,
  kBuiltinArrayMethodCallExtension,
  kMacroCallExtension,
  kNewCall,
  kSelectVariableDimension,
  kSelectVariableDimensionList,
  kEventExpression,
  kEventExpressionList,
  kLocalRoot,
  kQualifiedId,
  kUnqualifiedId,
  kModuleDeclaration,
  kModuleHeader,
  kMacroModuleDeclaration,
  kProgramDeclaration,
  kInterfaceDeclaration,
  kBegin,
  kEnd,
  kStatement,
  kNullStatement,
  kLabeledStatement,
  kStatementList,
  kBlockItemStatementList,
  kVoidcast,
  kRandomizeFunctionCall,
  kAttribute,
  kBracketGroup,
  kBraceGroup,
  kParenGroup,
  kQualifierList,
  kForwardDeclaration,
  kNullDeclaration,
  kSystemTFCall,
  kExtendsList,
  kFormalParameterList,
  kActualParameterPositionalList,
  kActualParameterByNameList,
  kActualParameterList,
  kFormalParameterListDeclaration,
  kDeclarationDimensions,
  kDimensionRange,
  kDimensionScalar,
  kDimensionSlice,
  kDimensionAssociativeType,
  kDimensionAssociativeIntegral,
  kParamType,
  kParamByName,
  kTrailingAssign,
  kParameterAssign,
  kParameterAssignList,
  kTypeAssignment,
  kTypeAssignmentList,
  kBaseDigits,  // e.g. 'd 1234
  kNumber,
  kInterfaceType,
  kImplementsList,
  kVariableDeclarationAssignmentList,
  kVariableDeclarationAssignment,
  kEndNew,
  kLetPortList,
  kLetPortItem,
  kPortItem,
  kPortActualList,
  kPortList,
  kPort,
  kActualPositionalPort,
  kActualNamedPort,
  kPortReference,
  kPortReferenceList,
  kInstantiationBase,
  kInstantiationType,
  kGateInstanceRegisterVariableList,
  kRegisterVariable,
  kGateInstance,
  kGateInstanceList,
  kPrimitiveGateInstance,
  kPrimitiveGateInstanceList,
  kDataDeclaration,
  kPackageImportDeclaration,
  kPackageImportList,
  kPackageImportItem,
  kPackageImportItemList,
  kPackageExportDeclaration,
  kScopePrefix,
  kParamDeclaration,
  kTypeDeclaration,
  kTypeReference,
  kLetDeclaration,
  kDataType,
  kDataTypePrimitive,
  kDataTypeImplicitBasicId,
  kDataTypeImplicitBasicIdDimensions,
  kDataTypeImplicitIdDimensions,
  kAssignModifyStatement,
  kNonblockingAssignmentStatement,
  kBlockingAssignmentStatement,
  kContinuousAssignmentStatement,
  kAssignmentStatement,
  kCaseStatement,
  kConditionalStatement,
  kIfClause,
  kElseClause,
  kIfHeader,
  kIfBody,
  kElseBody,
  kDisableStatement,
  kEventTriggerStatement,
  kForLoopStatement,
  kLoopHeader,
  kJumpStatement,
  kForSpec,
  kForInitialization,
  kForInitializationList,
  kForCondition,
  kForStepList,
  kForeverLoopStatement,
  kRepeatLoopStatement,
  kWhileLoopStatement,
  kDoWhileLoopStatement,
  kForeachLoopStatement,
  kLPValue,
  kOpenRangeList,
  kValueRange,
  kStreamingConcatenation,
  kCaseItemList,
  kCaseItem,
  kDefaultItem,
  kCasePatternItemList,
  kCasePatternItem,
  kCaseInsideItemList,
  kCaseInsideItem,
  kPattern,
  kPatternList,
  kMemberPattern,
  kMemberPatternList,
  kIncrementDecrementExpression,
  kParBlock,
  kProceduralTimingControlStatement,
  kSeqBlock,
  kWaitStatement,
  kAssertionStatement,
  kActionBlock,
  kArgumentList,
  kEventControl,
  kHierarchySegment,
  kHierarchySegmentList,
  kPreprocessorIfdefClause,
  kPreprocessorIfndefClause,
  kPreprocessorElsifClause,
  kPreprocessorElseClause,
  kPreprocessorDefine,
  kPreprocessorUndef,
  kPreprocessorInclude,
  kPreprocessorBalancedStatements,
  kPreprocessorBalancedPortDeclarations,
  kPreprocessorBalancedClassItems,
  kPreprocessorBalancedPackageItems,
  kPreprocessorBalancedModuleItems,
  kPreprocessorBalancedGenerateItems,
  kPreprocessorBalancedDescriptionItems,
  kMacroArgList,
  kMacroCall,
  kAssignmentPattern,
  kPatternExpression,
  kMinTypMaxList,
  kConstRef,
  kInterfacePortHeader,
  kTFPortDeclaration,
  kTFVariableIdentifier,
  kTFVariableIdentifierList,
  kFunctionItemList,
  kStreamExpressionList,
  kCast,
  kFunctionEndlabel,
  kLabel,
  kArrayWithPredicate,
  kDynamicArrayNew,
  kDelay,
  kDelayValue,
  kDelayValueList,
  kClassNew,
  kPoundZero,
  kPackedDimensions,
  kUnpackedDimensions,
  kIdentifierList,
  kIdentifierUnpackedDimensions,
  kIdentifierUnpackedDimensionsList,
  kPortIdentifierList,
  kWithConstraints,
  kConstraintBlockItemList,
  kPreprocessorBalancedConstraintBlockItem,
  kConstraintExpression,
  kPreprocessorBalancedConstraintExpressions,
  kDistribution,
  kDistributionItemList,
  kDistributionItem,
  kConstraintExpressionList,
  kConstraintPrimaryList,
  kUniquenessConstraint,
  kConstraintDeclaration,
  kConstraintPrototype,
  kCycleRange,
  kCycleDelay,
  kCycleDelayRange,
  kCycleDelayConstRange,
  kModuleAttributeForeign,
  kPortDeclarationList,
  kPortDeclaration,
  kModuleItemList,
  kModuleBlock,
  kNetVariableDeclarationAssign,
  kNetDeclarationAssignment,
  kNetVariable,
  kNetAlias,
  kNetAliasLvalueList,
  kModulePortDeclaration,
  kGenerateRegion,
  kGenerateItemList,
  kParameterOverride,
  kDefParamAssignList,
  kGateInstantiation,
  kBlockIdentifier,
  kAssertionItem,
  kAnalogStatement,
  kBranchProbeExpression,
  kBindDirective,
  kBindTargetInstanceList,
  kBindTargetInstance,
  kContinuousAssign,
  kLoopGenerateConstruct,
  kConditionalGenerateConstruct,
  kCaseGenerateConstruct,
  kGenerateIf,
  kGenerateIfClause,
  kGenerateIfHeader,
  kGenerateIfBody,
  kGenerateElseClause,
  kGenerateElseBody,
  kGenerateCaseItemList,
  kGenerateCaseItem,
  kGenerateDefaultItem,
  kGenerateBlock,
  kSpecifyBlock,
  kSpecifyItem,
  kSpecifyItemList,
  kTimeunitsDeclaration,
  kTypeIdentifierId,
  kNetDeclaration,
  kAssignmentList,
  kDefaultClockingStatement,
  kDefaultDisableStatement,
  kClockingDeclaration,
  kClockingItemList,
  kClockingItem,
  kDefaultSkew,
  kClockingSkew,
  kClockingDirection,
  kClockingAssignList,
  kClockingAssign,
  kGenvarDeclaration,
  kDPIImportItem,
  kDPIExportItem,
  kPropertySpecDisableIff,
  kPropertyPrefixExpression,
  kPropertyExpressionIndex,
  kPropertyIfElse,
  kPropertySimpleSequenceExpression,
  kPropertyCaseItem,
  kPropertyDefaultItem,
  kPropertyCaseItemList,
  kPropertyCaseStatement,
  kPropertyImplicationList,
  kPropertySpec,
  kExpressionDistributionList,
  kTimescaleDirective,
  kTimeLiteral,
  kTopLevelDirective,
  kSpecParam,
  kSpecParamList,
  kSpecParamDeclaration,
  kSpecifyPathDeclaration,
  kSpecifyEdgePath,
  kSpecifySimplePath,
  kSpecifyReferenceEvent,
  kSpecifyNotifier,
  kSpecifyPathIdentifier,
  kEdgeDescriptorList,
  kAlwaysStatement,
  kInitialStatement,
  kFinalStatement,
  kModportDeclaration,
  kModportItem,
  kModportItemList,
  kModportPortList,
  kModportTFPortsDeclaration,
  kModportSimplePortsDeclaration,
  kModportClockingPortsDeclaration,
  kModportSimplePort,
  kEnumType,
  kEnumName,
  kEnumNameList,
  kPackedSigning,
  kUnionType,
  kStructType,
  kStructUnionMember,
  kStructUnionMemberList,
  kSequenceDeclaration,
  kSequenceDeclarationFinalExpr,
  kSequencePortList,
  kSequencePortItem,
  kSequencePortTypeId,
  kSequenceMatchItemList,
  kSequenceSpec,
  kPropertyDeclaration,
  kPropertyPortList,
  kPropertyPortItem,
  kPropertyPortModifierList,
  kPropertyActualArg,
  kAssertionVariableDeclarationList,
  kAssertionVariableDeclaration,
  kVarDataTypeImplicitBasicIdDimensions,
  kSequenceDelayRange,
  kSequenceDelayRepetition,
  kConsecutiveRepetition,
  kNonconsecutiveRepetition,
  kGotoRepetition,
  kSequenceRepetitionExpression,
  kCovergroupDeclaration,
  kCovergroupHeader,
  kCoverageSpecOptionList,
  kCoverageOption,
  kCoverageEvent,
  kCoverageBlockEventOrList,
  kCoverageBlockEventExpression,
  kCoverCross,
  kCrossBodyItemList,
  kCrossItemList,
  kBinsSelection,
  kSelectCondition,
  kCoverPoint,
  kIffExpression,
  kBinOptionList,
  kPreprocessorBalancedBinsOrOptions,
  kCoverageBin,
  kCoverageBinRhs,
  kSpecCoverDeclaration,
  kInterfaceClassDeclaration,
  kInterfaceClassItemList,
  kInterfaceClassMethod,
  kDeclarationExtendsList,
  kDisciplineDeclaration,
  kDisciplineItemList,
  kDisciplineDomainBinding,
  kDisciplinePotential,
  kDisciplineFlow,
  kUdpPortList,
  kUdpPortDeclarationList,
  kUdpPortDeclaration,
  kUdpInitial,
  kUdpBody,
  kUdpEntryList,
  kUdpCombEntry,
  kUdpSequenceEntry,
  kUdpInputList,
  kUdpInputDeclarationList,
  kUdpPrimitive,
  kRandSequenceStatement,
  kProductionList,
  kProduction,
  kRandSequenceRuleList,
  kRandSequenceRule,
  kRandJoin,
  kWeightSpecification,
  kRandSequenceProductionList,
  kProductionItemsList,
  kProductionItem,
  kRandSequenceConditional,
  kRandSequenceLoop,
  kRandSequenceCase,
  kRandSequenceCaseItemList,
  kRandSequenceCaseItem,
  kRandSequenceDefaultItem,
  kTypeInfo,
  // END GENERATE -- do not delete

  // kInvalidTag is used to mark past-the-end of the last valid enum,
  // so keep this in last position.  It is not intended for use
  // in tagging syntax tree nodes.
  kInvalidTag,
};

// Stringify's node_enum. If node_enum does not have a string definition,
// returns a string stating this.
// Note: this function should only be used for debugging output and
//       must be maintained manually as NodeEnum's are added
std::string NodeEnumToString(NodeEnum node_enum);

std::ostream& operator<<(std::ostream&, const NodeEnum&);

// Predicate which returns true if the node_enum is a preprocessing node
bool IsPreprocessingNode(NodeEnum node_enum);

}  // namespace verilog

#endif  // VERIBLE_VERILOG_CST_VERILOG_NONTERMINALS_H_
