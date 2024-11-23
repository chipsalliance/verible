// Copyright 2021 The Verible Authors.
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

#ifndef VERIBLE_COMMON_LSP_LSP_PROTOCOL_OPERATORS_H
#define VERIBLE_COMMON_LSP_LSP_PROTOCOL_OPERATORS_H

// Some operators defined for the generated structs in lsp-protocol

#include "verible/common/lsp/lsp-protocol.h"

namespace verible {
namespace lsp {

// Less-than ordering of positions
constexpr bool operator<(const Position &a, const Position &b) {
  if (a.line > b.line) return false;
  if (a.line < b.line) return true;
  return a.character < b.character;
}
constexpr bool operator>=(const Position &a, const Position &b) {
  return !(a < b);
}
constexpr bool operator==(const Position &a, const Position &b) {
  return a.line == b.line && a.character == b.character;
}

// Ranges overlap if some part of one is inside the other range.
// Also empty ranges are considered overlapping if their start point is within
// the other range.
// rangerOverlap() is commutative.
constexpr bool rangeOverlap(const Range &a, const Range &b) {
  return !(a.start >= b.end || b.start >= a.end) || (a.start == b.start);
}
}  // namespace lsp
}  // namespace verible

#endif  // VERIBLE_COMMON_LSP_LSP_PROTOCOL_OPERATORS_H
