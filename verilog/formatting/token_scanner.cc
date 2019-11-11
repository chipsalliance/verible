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

#include "verilog/formatting/token_scanner.h"

#include <iostream>

#include "verilog/formatting/verilog_token.h"
#include "verilog/parser/verilog_token_enum.h"

namespace verilog {
namespace formatter {

std::ostream& TokenScanner::TokenScannerState::Print(
    std::ostream& stream) const {
  stream << "TokenScannerState with state: " << state << " newline count "
         << newline_count << std::endl;
  return stream;
}

void TokenScanner::Reset() { current_state_ = TokenScannerState(); }

void TokenScanner::UpdateState(yytokentype token_type) {
  current_state_ = TransitionState(current_state_, token_type);
}

/*
 * TransitionState computes the next state in the state machine given
 * a current TokenScannerState and token_type transition.
 *
 * The transitions are as follows:
 *
 * Start is an arbitrary state which only changes if a newline is encountered
 * kStart -> newline = kNewline
 *
 * Once a newline is encountered, encountering one or more comments will put
 * it into kNewlineComment state.
 * kNewline -> comment = kNewlineComment
 *
 * If a kNewlineComment encounters a newline, this represents an isolated
 * comment which occupies its own line entirely. This warrants a new
 * UnwrappedLine which is the kEnd state. If a token other than a newline or
 * comment is encountered, it will restart the cycle from the kStart state.
 *
 * kNewlineComment -> comment = kNewlineComment
 * kNewlineComment -> newline = kEnd
 *
 * The kEnd state can transition to a kRepeatNewline, since kEnd represents a
 * newline. A comment can also start a new cycle for a newline_commentu
 * kEnd -> newline = kRepeatNewline
 * kEnd -> comment = kNewlineComment
 *
 * All other tokens, including TK_SPACE and TK_BLOCK_COMMENT, are ignored,
 * and do not change state.
 */
TokenScanner::TokenScannerState TokenScanner::TransitionState(
    const TokenScannerState& scanner_state, yytokentype token_type) {
  // Returns true if a new UnwrappedLine should be created given a token_type.
  TokenScannerState new_state;
  switch (scanner_state.state) {
    case kStart: {
      if (IsComment(token_type)) {
        new_state.state = kNewlineComment;
        new_state.newline_count = 0;
      } else if (token_type == yytokentype::TK_NEWLINE) {
        new_state.state = kNewline;
        new_state.newline_count = 1;
      }
      break;
    }
    case kNewline: {
      if (token_type == yytokentype::TK_EOL_COMMENT) {
        new_state.state = kNewlineComment;
        new_state.newline_count = 0;
      } else if (token_type == yytokentype::TK_NEWLINE) {
        new_state.state = kRepeatNewline;
        new_state.newline_count = 2;
      }
      break;
    }
    case kRepeatNewline: {
      if (IsComment(token_type)) {
        new_state.state = kNewlineComment;
        new_state.newline_count = 0;
      } else if (token_type == yytokentype::TK_NEWLINE) {
        new_state.state = kRepeatNewline;
        new_state.newline_count = scanner_state.newline_count + 1;
      }
      break;
    }
    case kNewlineComment: {
      if (IsComment(token_type)) {
        new_state.state = kNewlineComment;
        new_state.newline_count = 0;
      } else if (token_type == yytokentype::TK_NEWLINE) {
        new_state.state = kEnd;
        new_state.newline_count = 1;
      }
      break;
    }
    case kEnd: {
      if (IsComment(token_type)) {
        new_state.state = kNewlineComment;
        new_state.newline_count = 0;
      } else if (token_type == yytokentype::TK_NEWLINE) {
        new_state.state = kRepeatNewline;
        new_state.newline_count = 2;
      }
      break;
    }
    default:
      break;
  }

  return new_state;
}

}  // namespace formatter
}  // namespace verilog
