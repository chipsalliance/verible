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

#ifndef VERIBLE_VERILOG_PARSER_VERILOG_LEXICAL_CONTEXT_H_
#define VERIBLE_VERILOG_PARSER_VERILOG_LEXICAL_CONTEXT_H_

#include <iosfwd>
#include <stack>
#include <vector>

#include "verible/common/text/token-info.h"
#include "verible/common/text/token-stream-view.h"
#include "verible/common/util/with-reason.h"

namespace verilog {
namespace internal {

// Helper state machine to parse optional labels after certain keywords.
class KeywordLabelStateMachine {
 public:
  // Updates the state machine, by looking ahead at the next token's enum.
  void UpdateState(int);

  // Returns true if a statement or item could start in this state.
  bool ItemMayStart() const {
    return state_ == kItemStart || state_ == kGotLabelableKeyword;
  }

 private:
  enum State {
    kItemStart,            // Could be the start of an item.
    kItemMiddle,           // After the start of an item.
    kGotLabelableKeyword,  // Seen a keyword that can accept a label.
    kGotColonExpectingLabel,
  };

  State state_ = kItemStart;
};

// Helper state machine for tracking constraint_block and constraint_set in the
// grammar.
class ConstraintBlockStateMachine {
 public:
  ConstraintBlockStateMachine() = default;

  bool IsActive() const { return !states_.empty(); }

  // Updates the state machine, by looking ahead at the next token's enum.
  void UpdateState(int);

  // Returns disambiguated enum for '->' token.
  int InterpretToken(int token_enum) const;

  // Show representation (for debugging).
  std::ostream &Dump(std::ostream &) const;

 private:
  void DeferInvalidToken(int token_enum);

  // See grammar for constraint_block_item and constraint_expression.
  enum State {
    kBeginningOfBlockItemOrExpression,  // list item (home state)

    // kIgnoreUntilSemicolon is applicable to:
    // "soft ...;"
    // "unique { ... };"
    // "disable soft ...;"
    // "solve ... before ...;" (from constraint_block_item)
    kIgnoreUntilSemicolon,

    // constraint_expression
    //   : expression_or_dist ;
    //   | expression -> constraint_set
    kExpectingExpressionOrImplication,

    kGotIf,       // if ...
    kGotForeach,  // foreach ...

    // constraint_set
    //   : constraint_expression
    //   | '{' { constraint_expression , }** '}'
    //
    // This is the final nonterminal for: if-clause, else-clause, foreach-body,
    // and RHS of expression -> constraint_set (constraint-implication)
    kExpectingConstraintSet,

    kInParenExpression,  // balance until ')'
    kInBraceExpression,  // balance until '}'
  };

  // Constraint sets are nestable, so we need a stack.
  // Each level of this stack represents a level of constraint block or
  // constraint set, both of which are wrapped in { }.
  std::stack<State> states_;
};

inline std::ostream &operator<<(std::ostream &os,
                                const ConstraintBlockStateMachine &s) {
  return s.Dump(os);
}

// Helper state machine to parse randomize calls.
class RandomizeCallStateMachine {
 public:
  bool IsActive() const { return state_ != kNone; }

  // Updates the state machine, by looking ahead at the next token's enum.
  void UpdateState(int);

  int InterpretToken(int) const;

 private:
  enum State {
    kNone,  // Not in a andomize call.
    kGotRandomizeKeyword,
    kOpenedVariableList,
    kClosedVariableList,
    kGotWithKeyword,
    kInsideWithIdentifierList,
    kExpectConstraintBlock,
    kInsideConstraintBlock,
  };

  // TODO(fangism): do we need a stack?  can randomize appear inside a
  // randomize_call?
  State state_ = kNone;

  // Nested state machine.
  ConstraintBlockStateMachine constraint_block_tracker_;
};

// Helper state machine to parse (non-extern) constraint declarations.
class ConstraintDeclarationStateMachine {
 public:
  bool IsActive() const { return state_ != kNone; }

  // Updates the state machine, by looking ahead at the next token's enum.
  void UpdateState(int);

  int InterpretToken(int) const;

 private:
  enum State {
    kNone,
    kGotConstraintKeyword,
    kGotConstraintIdentifier,
    // TODO(fangism): handle out-of-line definitions: constraint foo::bar ...
    kInsideConstraintBlock,
  };

  State state_ = kNone;

  // Nested state machine.
  ConstraintBlockStateMachine constraint_block_tracker_;
};

// This state machine keeps track of semicolons in a range enclosed by
// a pair of (keyword) tokens.  This is useful in disambiguating between
// grammatic constructs that can conflict due to optionality of a former
// list.  See the definition bodies of property_declaration and
// sequence_declaration for examples.
// For additional fun, both declarations accept an optional ';' right before
// the terminating keyword, but that one should *not* count as the 'last'.
class LastSemicolonStateMachine {
 public:
  LastSemicolonStateMachine(int trigger, int stop, int replacement)
      : trigger_token_enum_(trigger),
        finish_token_enum_(stop),
        semicolon_replacement_(replacement) {}

  void UpdateState(verible::TokenInfo *);

 protected:
  enum State {
    kNone,
    kActive,  // in betwen two keywords
  };

  // This is the token_enum that activates this state machine.
  const int trigger_token_enum_;
  // This is the token_enum that de-activates this state machine.
  const int finish_token_enum_;
  // This is the token_enum that should replace the last ';'.
  const int semicolon_replacement_;

  State state_ = kNone;

  // Keeps track of the last semicolons.  Upon de-activation, the last
  // semicolon will be replaced.  Technically, we only need a two-slot queue,
  // but a CircularBuffer is overkill.
  std::stack<verible::TokenInfo *> semicolons_;

  // One token look-back.
  verible::TokenInfo *previous_token_ = nullptr;
};
}  // namespace internal
   //
// A structure for tracking context needed to disambiguate tokens.
// The main input is a token stream coming from a lexer, and the main consumer
// is a parser that accepts a token stream.
// The vast majority of tokens should pass through unchanged.
// The ones that are changed are those that require context-based
// disambiguation.
// This should be designed in a manner that is forgiving of invalid inputs,
// i.e. improperly balanced code should never cause fatal errors.
// This class should maintain just enough state to correctly
// transform token enums on *valid* lexical streams.
//
// Design philosophy: This class itself is a state machine while employing
// smaller, simpler, concurrent state machines.
// The constituent state machines also scan the input token stream and
// update their states accordingly.
// The smaller state machines will be inactive most of the time, and activated
// on certain keywords in certain states.
class LexicalContext {
 public:
  LexicalContext();
  ~LexicalContext() = default;

  // Not copy-able.
  LexicalContext(const LexicalContext &) = delete;
  LexicalContext &operator=(const LexicalContext &) = delete;

  // Re-writes some token enums in-place using context-sensitivity.
  // This function must re-tag tokens enumerated (_TK_*), see verilog.y and
  // verilog.lex for all such enumerations.
  // This function must accept both valid and invalid inputs, but is only
  // required to operate correctly on valid inputs.
  // Postcondition: tokens_view's tokens must not be tagged with (_TK_*)
  // enumerations.
  void TransformVerilogSymbols(
      const verible::TokenStreamReferenceView &tokens_view) {
    // TODO(fangism): Using a stream interface would further decouple the input
    // iteration from output iteration.
    for (auto iter : tokens_view) {
      AdvanceToken(&*iter);
    }
  }

 protected:  // Allow direct testing of some methods.
  // Reads a single token, and may alter it depending on internal state.
  void AdvanceToken(verible::TokenInfo *);

  // Changes the enum of a token where disambiguation is needed.
  int InterpretToken(int token_enum) const;

  // Changes the enum of a token (in-place) without changing internal state.
  void MutateToken(verible::TokenInfo *token) const {
    token->set_token_enum(InterpretToken(token->token_enum()));
  }

  // Updates the internally tracked state without touching the token.
  void UpdateState(const verible::TokenInfo &token);

  // State functions:

  bool ExpectingStatement() const;
  verible::WithReason<bool> ExpectingBodyItemStart() const;

  bool InFlowControlHeader() const;
  bool InModuleDeclarationHeader() const {
    return in_module_declaration_ && !in_module_body_;
  }
  bool InFunctionDeclarationHeader() const {
    return in_function_declaration_ && !in_function_body_;
  }
  bool InTaskDeclarationHeader() const {
    return in_task_declaration_ && !in_task_body_;
  }
  bool InAnyDeclaration() const;
  bool InAnyDeclarationHeader() const;

  bool InStatementContext() const {
    return in_function_body_ || in_task_body_ ||
           in_initial_always_final_construct_;
  }

  const verible::TokenInfo *previous_token_ = nullptr;

  // Non-nestable states can be represented without a stack.
  // Do not bother trying to accommodate malformed input token sequences.
  bool in_module_declaration_ = false;
  bool in_module_body_ = false;

  bool in_initial_always_final_construct_ = false;

  bool seen_delay_value_in_initial_always_final_construct_context_ = false;

  bool in_function_declaration_ = false;
  bool in_function_body_ = false;

  bool in_task_declaration_ = false;
  bool in_task_body_ = false;

  // TODO(fangism): class_declaration, interface_declaration, udp_declaration...

  // Extern declarations cannot be nested, so a single bool suffices.
  bool in_extern_declaration_ = false;

  bool previous_token_finished_header_ = true;

  // Nestable states need to be tracked with a stack.

  // Tracks if, for, case blocks.
  struct FlowControlState {
    const verible::TokenInfo *start;
    // When this is false, the state is still in the header, which is:
    //   if (...)
    //   for (...)
    //   case (...)  (including other case-variants)
    bool in_body = false;  // starts in header state

    explicit FlowControlState(const verible::TokenInfo *token) : start(token) {}
  };
  std::vector<FlowControlState> flow_control_stack_;

  // Tracks optional labels after certain keywords.
  internal::KeywordLabelStateMachine keyword_label_tracker_;

  // Tracks parsing state inside randomize_call.
  internal::RandomizeCallStateMachine randomize_call_tracker_;

  // Tracks parsing state inside randomize_call.
  internal::ConstraintDeclarationStateMachine constraint_declaration_tracker_;

  // Tracks last semicolon in property_declarations so that it can be
  // re-enumerated to help disambiguate.
  internal::LastSemicolonStateMachine property_declaration_tracker_;

  // Tracks last semicolon in sequence_declarations so that it can be
  // re-enumerated to help disambiguate.
  internal::LastSemicolonStateMachine sequence_declaration_tracker_;

  // Tracks begin-end paired sequence blocks in all contexts (generate blocks,
  // function/task statements, flow-control constructs...).
  // Every 'begin' token will be pushed onto this stack.
  // Every 'end' token will pop the stack (safely).
  // Accepts invalid input, which does not guarantee begin-end balancing.
  // Does not care about optional labels after these keywords.
  //
  // e.g.
  //   ...     // stack initially empty
  //   begin   // pushes onto this stack
  //     ...
  //     begin  // pushes onto this stack
  //       ...
  //     end  // pops off of this stack
  //     ...
  //   end  // pops off of this stack
  //
  std::vector<const verible::TokenInfo *> block_stack_;

  // Tracks open-close paired tokens like parentheses and brackets and braces.
  std::vector<const verible::TokenInfo *> balance_stack_;
};

}  // namespace verilog

#endif  // VERIBLE_VERILOG_PARSER_VERILOG_LEXICAL_CONTEXT_H_
