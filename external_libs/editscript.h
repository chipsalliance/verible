// Copyright 2018 The diff-match-patch Authors.
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

// This code (especially GetBisectSplitPoints) derive in part from
// diff_match_patch.cpp under the Apache 2.0 license and attribution:
//
// Author: fraser@google.com (Neil Fraser)
// Author: mikeslemmer@gmail.com (Mike Slemmer)
//
// Diff Match and Patch
// https://github.com/google/diff-match-patch

// Returns the minimal number of edit operations (copy, delete, insert)
// needed to tranform one container of tokens into another (cf. `diff -d -e`).
// Requires random-access iterators of token streams.

#ifndef EDITSCRIPT_H_
#define EDITSCRIPT_H_

#include <algorithm>
#include <cstdint>
#include <iterator>
#include <tuple>
#include <utility>
#include <vector>

namespace diff {

enum class Operation { EQUALS, DELETE, INSERT };

/**
 * The data structure of edit operations to transform tokens1 into tokens2.
 * Indices for EQUALS and DELETE point into tokens1, INSERT into tokens2.
 * Concatenating EQUALS and DELETE tokens will return tokens1.
 * Concatenating EQUALS and INSERT tokens will return tokens2.
 * [start, end) is a semiopen interval.
 */
struct Edit {
  Operation operation;  // One of: EQUALS, DELETE, or INSERT.
  int64_t start;        // Start offset into tokens1 (tokens2 for INSERT).
  int64_t end;          // End   offset into tokens1 (tokens2 for INSERT).
  bool operator==(const Edit &rhs) const {
    return (std::tie(operation, start, end) ==
            std::tie(rhs.operation, rhs.start, rhs.end));
  }
  bool operator<(const Edit &rhs) const {
    return (std::tie(operation, start, end) <
            std::tie(rhs.operation, rhs.start, rhs.end));
  }
};
using Edits = std::vector<Edit>;

/**
 * Finds the differences between two vectors of tokens, returning edits
 * required to transform tokens1 into tokens2.
 * Every token in the combined document belongs to exactly one edit.
 *
 * For example, for tokens1 = "Hello world." and tokens2 = "Goodbye world.":
 * {Edit(Operation::DELETE, 0, 5),   // token1[0,5)   == "Hello"
 *  Edit(Operation::INSERT, 0, 7),   // token2[0,7)   == "Goodbye"
 *  Edit(Operation::EQUALS, 5, 12)}  // token1[5, 12) == " world."
 *
 * @param tokens1_begin Beginning index into tokens1.
 * @param tokens1_end   Ending index into tokens1.
 * @param tokens2_begin Beginning index into tokens2.
 * @param tokens2_end   Ending index into tokens2.
 * @return Cumulative edits to transform tokens1 into tokens2.
 */
template <typename TokenIter>
Edits GetTokenDiffs(TokenIter tokens1_begin, TokenIter tokens1_end,
                    TokenIter tokens2_begin, TokenIter tokens2_end);

//////////////////////////////////////////////////////////////////////////////
// IMPLEMENTATION
//////////////////////////////////////////////////////////////////////////////

namespace diff_impl {
/**
 * Appends an edit operation to the cumulative edits.
 * Fuses with previous edit if possible.
 * @param op    Edit operation.
 * @param start Start offset into tokens1 (tokens2 for INSERT).
 * @param end   End offset into tokens1 (tokens2 for INSERT).
 * @param edits Cumulative vector of edits (inout).
 */
inline void AppendEdit(Operation op, int64_t start, int64_t end, Edits *edits) {
  if (!edits->empty() && edits->back().operation == op &&
      edits->back().end == start) {
    // Merge into previous edit operation.
    edits->back().end = end;
  } else {
    // Add a new edit operation.
    edits->emplace_back(Edit{op, start, end});
  }
}

/**
 * Inserts an edit operation into the cumulative edits at given index.
 * Fuses with neighboring edit if possible.
 * @param index Index of insertion point.
 * @param op    Edit operation.
 * @param start Start offset into tokens1 (tokens2 for INSERT).
 * @param end   End offset into tokens1 (tokens2 for INSERT).
 * @param edits Cumulative vector of edits (inout).
 */
inline void InsertEditAt(int64_t index, Operation op, int64_t start,
                         int64_t end, Edits *edits) {
  if (index > 0) {
    Edit &prev_edit = (*edits)[index - 1];
    if (prev_edit.operation == op && prev_edit.end == start) {
      // Merge into previous edit operation.
      prev_edit.end = end;
      return;
    }
  }
  if (index < static_cast<int64_t>(edits->size())) {
    Edit &next_edit = (*edits)[index];
    if (next_edit.operation == op && next_edit.end == start) {
      // Merge into subsequent edit operation.
      next_edit.end = end;
      return;
    }
  }

  // Cannot merge into existing? Insert a new edit operation.
  edits->emplace(edits->begin() + index, Edit{op, start, end});
}

/**
 * Reverse direction of iterator (without moving its location).
 * Returned iter will now point to one elem to left of input iter.
 * @param iter Original bidirectional or random iterator.
 */
template <typename TokenIter>
inline std::reverse_iterator<TokenIter> Reverse(TokenIter iter) {
  return std::reverse_iterator<TokenIter>(iter);
}

/**
 * Determine the common affix of two tokens.
 * For common prefix, pass normal iterators.
 * For common suffix, pass reverse iterators.
 * @param span1_begin Beginning iterator into tokens1.
 * @param span1_end   Ending iterator into tokens1.
 * @param span2_begin Beginning iterator into tokens2.
 * @param span2_end   Ending iterator into tokens2.
 * @return The number of tokens common to the start of spans.
 */
template <typename TokenIter>
inline int64_t CommonAffix(TokenIter span1_begin, TokenIter span1_end,
                           TokenIter span2_begin, TokenIter span2_end) {
  // std::mismatch wants the shorter container first.
  if (span1_end - span1_begin > span2_end - span2_begin) {
    using std::swap;
    swap(span1_begin, span2_begin);
    swap(span1_end, span2_end);
  }

  auto affix_ends = std::mismatch(span1_begin, span1_end, span2_begin);
  const int64_t affix_length = affix_ends.first - span1_begin;
  return affix_length;
}

/**
 * Finds the differences between two vectors of tokens.
 * Invoke using GetTokenDiffs.
 */
template <typename TokenIter>
class Diff {
 private:
  friend Edits GetTokenDiffs<>(TokenIter tokens1_begin, TokenIter tokens1_end,
                               TokenIter tokens2_begin, TokenIter tokens2_end);

  /**
   * Finds the differences between two vectors of tokens, returning edits
   * required to transform tokens1 into tokens2.
   * Every token in the combined document belongs to exactly one edit.
   * @param tokens1_begin Iterator pointing to start of tokens1.
   * @param tokens2_begin Iterator pointing to start of tokens2.
   * @param edits Cumulative edits to transform tokens1 into tokens2 (inout).
   */
  Diff(TokenIter tokens1_begin, TokenIter tokens2_begin)
      : tokens1_begin_(tokens1_begin), tokens2_begin_(tokens2_begin) {}

  /**
   * Find the differences between two vectors of tokens.
   * @param b1 Beginning index into tokens1.
   * @param e1 Ending index into tokens1.
   * @param b2 Beginning index into tokens2.
   * @param e2 Ending index into tokens2.
   * @param edits Cumulative edits to transform tokens1 into tokens2 (inout).
   * @param edits Vector of Edit objects
   */
  void Generate(int64_t b1, int64_t e1, int64_t b2, int64_t e2,
                Edits *edits) const {
    // Avoid growing stack through recursion more than necessary.
    int64_t prefix_size = 0;
    int64_t suffix_size = 0;
    int64_t edits_size = 0;
    {
      // The diff strategy is to recursively:
      // - Peel off common prefix and suffix
      // - Split what's left in two parts
      // - Generate diffs for each and combine.
      // The diffs could be done in parallel (with a fresh edits) and
      // later combined if speed up is needed.
      auto span1_begin = tokens1_begin_ + b1;
      auto span2_begin = tokens2_begin_ + b2;
      auto span1_end = tokens1_begin_ + e1;
      auto span2_end = tokens2_begin_ + e2;

      // Check for equality (speedup).
      if ((e1 - b1) == (e2 - b2) &&
          std::equal(span1_begin, span1_end, span2_begin)) {
        if (span1_begin != span1_end) {
          AppendEdit(Operation::EQUALS, b1, e1, edits);
        }
        return;
      }

      // Find longest common prefix and suffix.
      prefix_size = CommonAffix(span1_begin, span1_end, span2_begin, span2_end);
      suffix_size =
          CommonAffix(Reverse(span1_end), Reverse(span1_begin + prefix_size),
                      Reverse(span2_end), Reverse(span2_begin + prefix_size));

      // Remember the current location so we can insert a common prefix.
      edits_size = edits->size();
    }

    // Compute the diff on the middle block.
    Compute(b1 + prefix_size, e1 - suffix_size, b2 + prefix_size,
            e2 - suffix_size, edits);

    // Restore the prefix and suffix.
    if (prefix_size != 0) {
      InsertEditAt(edits_size, Operation::EQUALS, b1, b1 + prefix_size, edits);
    }
    if (suffix_size != 0) {
      AppendEdit(Operation::EQUALS, e1 - suffix_size, e1, edits);
    }
  }

  /**
   * Find the differences between two vectors of tokens.
   * @pre Tokens are not equal nor share a common prefix or suffix.
   * @param b1 Beginning index into tokens1.
   * @param e1 Ending index into tokens1.
   * @param b2 Beginning index into tokens2.
   * @param e2 Ending index into tokens2.
   * @param edits Cumulative edits to transform tokens1 into tokens2 (inout).
   */
  void Compute(int64_t b1, int64_t e1, int64_t b2, int64_t e2,
               Edits *edits) const {
    // Try various speedups first.
    // Enclose in a scope to save stack space before recursing.
    {
      const int64_t length1 = e1 - b1;
      const int64_t length2 = e2 - b2;
      if (length1 == 0 && length2 != 0) {
        // tokens1 empty, but tokens2 isn't: Just insert tokens2
        AppendEdit(Operation::INSERT, b2, e2, edits);
        return;
      }
      if (length2 == 0 && length1 != 0) {
        // tokens2 empty, but tokens1 isn't: Just delete tokens1
        AppendEdit(Operation::DELETE, b1, e1, edits);
        return;
      }
      if (length1 > length2) {
        const int64_t offset =
            std::search(tokens1_begin_ + b1, tokens1_begin_ + e1,
                        tokens2_begin_ + b2, tokens2_begin_ + e2) -
            tokens1_begin_;
        if (offset != e1) {
          // tokens2 is a proper substring of tokens1: delete the rest.
          const int64_t offset_end = offset + length2;
          AppendEdit(Operation::DELETE, b1, offset, edits);
          AppendEdit(Operation::EQUALS, offset, offset_end, edits);
          AppendEdit(Operation::DELETE, offset_end, e1, edits);
          return;
        }
        if (length2 == 1) {
          // Single-token span.
          // After the previous speedup, the operation can't be EQUALS.
          AppendEdit(Operation::DELETE, b1, e1, edits);
          AppendEdit(Operation::INSERT, b2, e2, edits);
          return;
        }
      } else if (length2 > length1) {
        const int64_t offset =
            std::search(tokens2_begin_ + b2, tokens2_begin_ + e2,
                        tokens1_begin_ + b1, tokens1_begin_ + e1) -
            tokens2_begin_;
        if (offset != e2) {
          // tokens1 is a proper substring of tokens2: insert the rest.
          AppendEdit(Operation::INSERT, b2, offset, edits);
          AppendEdit(Operation::EQUALS, b1, e1, edits);  // Index into tokens1!
          AppendEdit(Operation::INSERT, offset + length1, e2, edits);
          return;
        }
        if (length1 == 1) {
          // Single-token span.
          // After the previous speedup, the operation can't be EQUALS.
          AppendEdit(Operation::DELETE, b1, e1, edits);
          AppendEdit(Operation::INSERT, b2, e2, edits);
          return;
        }
      }
    }

    // No speedups apply? Bisect and diff each half, then combine results.
    Bisect(b1, e1, b2, e2, edits);
  }

  /**
   * Find the 'middle snake' of a diff, returning split points.
   * See Myers 1986 paper: An O(ND) Difference Algorithm and Its Variations.
   * @param b1    Beginning index into tokens1.
   * @param e1    Ending index into tokens1.
   * @param b2    Beginning index into tokens2.
   * @param e2    Ending index into tokens2.
   * @return Index of split points in tokens1 and tokens2, respectively.
   *     Returns {-1, -1} if there is no need to bisect.
   */
  std::pair<int64_t, int64_t> GetBisectSplitPoints(int64_t b1, int64_t e1,
                                                   int64_t b2,
                                                   int64_t e2) const {
    std::pair<int64_t, int64_t> splits;
    int64_t &x1 = splits.first;
    int64_t &y1 = splits.second;

    const int64_t length1 = e1 - b1;
    const int64_t length2 = e2 - b2;
    const int64_t max_d = (length1 + length2 + 1) / 2;
    const int64_t v_offset = max_d;
    const int64_t v_size = 2 * max_d;
    const int64_t w_size = 2 * v_size;
    std::vector<int64_t> w(w_size + 4,
                           -1);  // interleave for cache friendliness

    w[2 * v_offset + 2] = 0;
    w[2 * v_offset + 3] = 0;
    const int64_t delta = length1 - length2;

    // If the total number of tokens is odd, then the front path will
    // collide with the reverse path.
    const bool front = (delta % 2 != 0);

    // Offsets for start and end of k loop.
    // Prevents mapping of space beyond the grid.
    int64_t k1start = 0;
    int64_t k1end = 0;
    int64_t k2start = 0;
    int64_t k2end = 0;
    for (int64_t d = 0; d < max_d; d++) {
      // Walk the front path one step.
      for (int64_t k1 = -d + k1start; k1 <= d - k1end; k1 += 2) {
        const int64_t k1_offset = v_offset + k1;
        x1 = 0;
        if (k1 == -d ||
            (k1 != d && w[2 * k1_offset - 2] < w[2 * k1_offset + 2])) {
          x1 = w[2 * k1_offset + 2];
        } else {
          x1 = w[2 * k1_offset - 2] + 1;
        }
        y1 = x1 - k1;
        while (x1 < length1 && y1 < length2 &&
               *(tokens1_begin_ + (b1 + x1)) == *(tokens2_begin_ + (b2 + y1))) {
          x1++;
          y1++;
        }
        w[2 * k1_offset] = x1;
        if (x1 > length1) {
          // Ran off the right of the graph.
          k1end += 2;
        } else if (y1 > length2) {
          // Ran off the bottom of the graph.
          k1start += 2;
        } else if (front) {
          int64_t k2_offset = v_offset + delta - k1;
          if (k2_offset >= 0 && k2_offset < v_size &&
              w[2 * k2_offset + 1] != -1) {
            // Mirror x2 onto top-left coordinate system.
            int64_t x2 = length1 - w[2 * k2_offset + 1];
            if (x1 >= x2) {
              // Overlap detected.
              return splits;
            }
          }
        }
      }

      // Walk the reverse path one step.
      for (int64_t k2 = -d + k2start; k2 <= d - k2end; k2 += 2) {
        const int64_t k2_offset = v_offset + k2;
        int64_t x2;
        if (k2 == -d ||
            (k2 != d && w[2 * k2_offset - 1] < w[2 * k2_offset + 3])) {
          x2 = w[2 * k2_offset + 3];
        } else {
          x2 = w[2 * k2_offset - 1] + 1;
        }
        int64_t y2 = x2 - k2;
        while (x2 < length1 && y2 < length2 &&
               *(tokens1_begin_ + (b1 + length1 - x2 - 1)) ==
                   *(tokens2_begin_ + (b2 + length2 - y2 - 1))) {
          x2++;
          y2++;
        }
        w[2 * k2_offset + 1] = x2;
        if (x2 > length1) {
          // Ran off the left of the graph.
          k2end += 2;
        } else if (y2 > length2) {
          // Ran off the top of the graph.
          k2start += 2;
        } else if (!front) {
          int64_t k1_offset = v_offset + delta - k2;
          if (k1_offset >= 0 && k1_offset < v_size && w[2 * k1_offset] != -1) {
            x1 = w[2 * k1_offset];
            y1 = v_offset + x1 - k1_offset;

            // Mirror x2 onto top-left coordinate system.
            x2 = length1 - x2;
            if (x1 >= x2) {
              // Overlap detected.
              return splits;
            }
          }
        }
      }
    }
    x1 = -1;
    y1 = -1;
    return splits;
  }

  /**
   * Bisect and recurse if necessary.
   * @param b1    Beginning index into tokens1.
   * @param e1    Ending index into tokens1.
   * @param b2    Beginning index into tokens2.
   * @param e2    Ending index into tokens2.
   * @param edits Cumulative edits to transform tokens1 into tokens2 (inout).
   */
  void Bisect(int64_t b1, int64_t e1, int64_t b2, int64_t e2,
              Edits *edits) const {
    int64_t x1, x2;
    std::tie(x1, x2) = GetBisectSplitPoints(b1, e1, b2, e2);
    if (x1 >= 0) {
      // Some commonality, so bisect and recurse.
      Generate(b1, b1 + x1, b2, b2 + x2, edits);
      Generate(b1 + x1, e1, b2 + x2, e2, edits);
    } else {
      // No commonality at all (number of edits equals number of tokens),
      // so just delete the old and insert the new.
      AppendEdit(Operation::DELETE, b1, e1, edits);
      AppendEdit(Operation::INSERT, b2, e2, edits);
    }
  }

 private:
  TokenIter tokens1_begin_;
  TokenIter tokens2_begin_;
};  // class Diff
}  // namespace diff_impl

template <typename TokenIter>
inline Edits GetTokenDiffs(TokenIter tokens1_begin, TokenIter tokens1_end,
                           TokenIter tokens2_begin, TokenIter tokens2_end) {
  Edits token_edits;
  diff_impl::Diff<TokenIter>(tokens1_begin, tokens2_begin)
      .Generate(0, std::distance(tokens1_begin, tokens1_end), 0,
                std::distance(tokens2_begin, tokens2_end), &token_edits);
  return token_edits;  // efficient: uses named return value optimization
}

}  // namespace diff

#endif  // EDITSCRIPT_H_
