// Copyright 2017-2019 The Verible Authors.
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

#ifndef VERIBLE_VERILOG_FORMATTING_TOKEN_SCANNER_H_
#define VERIBLE_VERILOG_FORMATTING_TOKEN_SCANNER_H_

#include <iosfwd>

#include "verilog/parser/verilog_token_enum.h"

namespace verilog {
namespace formatter {

// This finite state machine class is used to determine the placement of
// non-whitespace and non-syntax-tree-node tokens such as comments in
// UnwrappedLines.  The input to this FSM is a Verilog-specific token enum.
// This class is an internal implementation detail of the TreeUnwrapper class.
// TODO(fangism): rename this InterLeafTokenScanner.
// TODO(fangism): handle attributes.
class TokenScanner {
 public:
  TokenScanner() = default;

  // Deleted standard interfaces:
  TokenScanner(const TokenScanner&) = delete;
  TokenScanner(TokenScanner&&) = delete;
  TokenScanner& operator=(const TokenScanner&) = delete;
  TokenScanner& operator=(TokenScanner&&) = delete;

  // Re-initializes state.
  void Reset();

  // Calls TransitionState using current_state_ and the tranition token_Type
  void UpdateState(yytokentype token_type);

  // Returns true if the state is currently kEnd
  bool EndState() const { return current_state_.state == State::kEnd; }

  // Returns true if the state is currently kRepeatNewline
  bool RepeatNewlineState() const {
    return current_state_.state == State::kRepeatNewline;
  }

 protected:
  // Represents a state for the TokenScanner
  enum State {
    kStart,
    kNewline,
    kRepeatNewline,
    kNewlineComment,
    kEnd,
  };

  // Used to maintain a state of the TokenScanner: A state and count of
  // consecutive newlines passed
  struct TokenScannerState {
    State state = State::kStart;

    // The number of consecutive newlines up to and including this
    // TokenScannerStates
    int newline_count = 0;

    bool operator==(const TokenScannerState& r) const {
      return (state == r.state && newline_count == r.newline_count);
    }

    bool operator!=(const TokenScannerState& r) const {
      return !(state == r.state && newline_count == r.newline_count);
    }

    std::ostream& Print(std::ostream&) const;
  };

  // The current state of the TokenScanner. Initializes to kStart.
  TokenScannerState current_state_;

  // Transitions the TokenScanner given a TokenState and a yytokentype
  // transition
  static TokenScannerState TransitionState(
      const TokenScannerState& scanner_state, yytokentype token_type);
};

}  // namespace formatter
}  // namespace verilog

#endif  // VERIBLE_VERILOG_FORMATTING_TOKEN_SCANNER_H_
