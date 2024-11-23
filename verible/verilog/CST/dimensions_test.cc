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

#include "verible/verilog/CST/dimensions.h"

#include <algorithm>
#include <cstddef>
#include <memory>
#include <vector>

#include "gtest/gtest.h"
#include "verible/common/analysis/syntax-tree-search.h"
#include "verible/common/text/concrete-syntax-leaf.h"
#include "verible/common/text/concrete-syntax-tree.h"
#include "verible/common/text/symbol.h"
#include "verible/common/text/text-structure.h"
#include "verible/common/text/token-info.h"
#include "verible/common/text/tree-utils.h"
#include "verible/common/util/casts.h"
#include "verible/common/util/logging.h"
#include "verible/verilog/CST/verilog-matchers.h"  // IWYU pragma: keep
#include "verible/verilog/analysis/verilog-analyzer.h"

#undef ASSERT_OK
#define ASSERT_OK(value) ASSERT_TRUE((value).ok())

namespace verilog {
namespace {

using verible::down_cast;
using verible::SyntaxTreeLeaf;

struct MatchTestCase {
  const char *code;
  int expect_packed_matches;
  int expect_unpacked_matches;
};

// These test cases check for the correct number of occurences of
// packed and unpacked dimensions.
static const MatchTestCase kMatchTestCases[] = {
    {"", 0, 0},
    // package_or_generate_item_declaration level tests
    {"wire w;", 0, 0},
    {"wire [1:0] w;", 1, 0},
    {"wire [9:0] w;", 1, 0},
    {"wire [0:4] w;", 1, 0},
    {"wire [1:0][3:0] w;", 1, 0},  // 2 dimensional, but 1 set of packed
    {"wire w [0:1];", 0, 1},
    {"wire w [1:0];", 0, 1},
    {"wire w [1:0][7:0];", 0, 1},  // 2 dimensional, but 1 set of unpacked
    {"wire [2:0] w [2];", 1, 1},
    {"wire [2:0][1:0] w [4][2];", 1, 1},
    {"wire [1:0] w; wire [1:0] x;", 2, 0},  // separate declarations
    {"wire w [1:0]; wire x [1:0];", 0, 2},  // separate declarations

    // Test different data_declaration types.
    {"logic l;", 0, 0},
    {"logic [1:0] l;", 1, 0},
    {"logic l [1:0];", 0, 1},
    {"bit b;", 0, 0},
    {"bit [1:0] b;", 1, 0},
    {"bit b [1:0];", 0, 1},
    {"reg r;", 0, 0},
    {"reg [1:0] r;", 1, 0},
    {"reg r [1:0];", 0, 1},
    {"mytype m;", 0, 0},
    {"mytype [1:0] m;", 1, 0},
    {"mytype m [1:0];", 0, 1},
    {"mypkg::mytype m;", 0, 0},
    {"mypkg::mytype [1:0] m;", 1, 0},
    {"mypkg::mytype m [1:0];", 0, 1},
    {"signed m;", 0, 0},
    {"signed [1:0] m;", 1, 0},
    {"signed m [1:0];", 0, 1},
    {"unsigned m;", 0, 0},
    {"unsigned [1:0] m;", 1, 0},
    {"unsigned m [1:0];", 0, 1},
    {"event e;", 0, 0},
    {"event [1:0] e;", 1, 0},
    {"event e [1:0];", 0, 1},

    // Test unnamed struct members
    {"struct { logic l; } s;", 0, 0},
    {"struct { logic [2:0] l; } s;", 1, 0},
    {"struct { logic l [2:0]; } s;", 0, 1},
    // Test typedef struct members
    {"typedef struct { logic l; } s_s;", 0, 0},
    {"typedef struct { logic [2:0] l; } s_s;", 1, 0},
    {"typedef struct { logic l [2:0]; } s_s;", 0, 1},

    // Test class fields
    {"class c; bit b; endclass", 0, 0},
    {"class c; bit [1:0] b; endclass", 1, 0},
    {"class c; bit b [0:1]; endclass", 0, 1},
    {"class c; bit [2:0] b [0:1]; endclass", 1, 1},

    // Test module ports
    {"module m(input wire foo); endmodule", 0, 0},
    {"module m(input wire [2:0] foo); endmodule", 1, 0},
    {"module m(input wire foo [2:0]); endmodule", 0, 1},
    {"module m(input wire [2:0] foo [2:0]); endmodule", 1, 1},
    {"module m(output reg foo); endmodule", 0, 0},
    {"module m(output reg [2:0] foo); endmodule", 1, 0},
    {"module m(output reg foo [2:0]); endmodule", 0, 1},
    {"module m(output reg [2:0] foo [2:0]); endmodule", 1, 1},

    // Test module local declarations
    {"module m; wire foo; endmodule", 0, 0},
    {"module m; wire [4:0] foo; endmodule", 1, 0},
    {"module m; wire foo[5]; endmodule", 0, 1},
    {"module m; wire [4:0] foo[5]; endmodule", 1, 1},
    {"module m; submod foo; endmodule", 0, 0},
    {"module m; submod foo[5]; endmodule", 0, 1},

    // Test function ports
    {"function void f(bit foo); endfunction", 0, 0},
    {"function void f(bit [2:0] foo); endfunction", 1, 0},
    {"function void f(bit foo [2:0]); endfunction", 0, 1},

    // Test function locals
    {"function void f; bit foo; endfunction", 0, 0},
    {"function void f; bit [3:0] foo; endfunction", 1, 0},
    {"function void f; bit foo [3:0]; endfunction", 0, 1},

    // Test function return types
    {"function bit foo; endfunction", 0, 0},
    {"function bit [2:0] foo; endfunction", 1, 0},

    // Test task ports
    {"task automatic t(bit foo); endtask", 0, 0},
    {"task automatic t(bit [2:0] foo); endtask", 1, 0},
    {"task automatic t(bit foo [2:0]); endtask", 0, 1},

    // Test task locals
    {"task automatic t; bit foo; endtask", 0, 0},
    {"task automatic t; bit [2:0] foo; endtask", 1, 0},
    {"task automatic t; bit foo [2:0]; endtask", 0, 1},

    // Test parameters
    {"parameter int p = 0;", 0, 0},
    {"parameter int [3:0] p = 0;", 1, 0},
    {"parameter int p [3:0] = 0;", 0, 1},
    {"localparam int p = 0;", 0, 0},
    {"localparam int [3:0] p = 0;", 1, 0},
    {"localparam int p [3:0] = 0;", 0, 1},
    {"parameter int p = q[0];", 0, 0},      // selection is not declaration
    {"parameter int p = q[1:0];", 0, 0},    // selection is not declaration
    {"parameter int p = 0, q = 1;", 0, 0},  // multiple assignments
    {"parameter int [1:0] p = 0, q = 1;", 1, 0},  // multiple assignments
    // TODO(b/132818394): parse unpacked dimensions in subsequent initializers
    // {"parameter int p[0:1] = {0,2}, q[0:1] = {1,3};", 0, 2},
};

static size_t ExtractNumDimensions(const verible::Symbol *root) {
  if (root == nullptr) return 0;
  const auto matches = FindAllDeclarationDimensions(*root);
  if (matches.empty()) return 0;
  // Only extract from the first match.
  if (matches[0].match == nullptr) return 0;
  const auto &s = *matches[0].match;
  return down_cast<const verible::SyntaxTreeNode &>(s).size();
}

// Test that number of sets of packed dimensions found is correct.
TEST(FindAllPackedDimensionsTest, MatchCounts) {
  for (const auto &test : kMatchTestCases) {
    VerilogAnalyzer analyzer(test.code, "");
    ASSERT_OK(analyzer.Analyze()) << "Failed test code: " << test.code;
    const auto &root = analyzer.Data().SyntaxTree();
    const auto packed_dimensions =
        FindAllPackedDimensions(*ABSL_DIE_IF_NULL(root));
    const int nonempty_packed_dimensions =
        std::count_if(packed_dimensions.begin(), packed_dimensions.end(),
                      [](const verible::TreeSearchMatch &m) {
                        return ExtractNumDimensions(m.match) > 0;
                      });
    EXPECT_EQ(nonempty_packed_dimensions, test.expect_packed_matches)
        << "Failed test code: " << test.code;
  }
}

// Test that number of sets of unpacked dimensions found is correct.
TEST(FindAllUnpackedDimensionsTest, MatchCounts) {
  for (const auto &test : kMatchTestCases) {
    VerilogAnalyzer analyzer(test.code, "");
    ASSERT_OK(analyzer.Analyze()) << "Failed test code: " << test.code;
    const auto &root = analyzer.Data().SyntaxTree();
    const auto unpacked_dimensions =
        FindAllUnpackedDimensions(*ABSL_DIE_IF_NULL(root));
    const int nonempty_unpacked_dimensions =
        std::count_if(unpacked_dimensions.begin(), unpacked_dimensions.end(),
                      [](const verible::TreeSearchMatch &m) {
                        return ExtractNumDimensions(m.match) > 0;
                      });
    EXPECT_EQ(nonempty_unpacked_dimensions, test.expect_unpacked_matches)
        << "Failed test code: " << test.code;
  }
}

struct DimensionTestCase {
  const char *code;
  int expect_dimensions;
};

// Test that dimensionality counts are correct.
TEST(ExtractNumDimensionsTest, DimensionCounts) {
  // In each of these cases, there should be exactly one set of dimensions.
  const DimensionTestCase kDimensionTestCases[] = {
      {"wire w;", 0},       {"wire [] w;", 1},
      {"wire [1:0] w;", 1}, {"wire [1:0][1:0] w;", 2},
      {"wire w [0:1];", 1}, {"wire w [0:1][0:3];", 2},
      {"wire w [2];", 1},   {"wire w [3][5];", 2},
      {"wire w [];", 1},
  };
  for (const auto &test : kDimensionTestCases) {
    VerilogAnalyzer analyzer(test.code, "");
    ASSERT_OK(analyzer.Analyze()) << "Failed test code: " << test.code;
    const auto &root = analyzer.Data().SyntaxTree();
    EXPECT_EQ(ExtractNumDimensions(root.get()), test.expect_dimensions)
        << "Failed test code: " << test.code;
  }
}

struct RangeTestCase {
  const char *code;
  const char *expect_left;
  const char *expect_right;
};

// Each of these test cases should have exactly one ranged-dimension.
static const RangeTestCase kRangeTestCases[] = {
    {"wire [a:b] w;", "a", "b"},
    {"wire w [c:d];", "c", "d"},
    {"wire w [c1:d1][e];", "c1", "d1"},
    {"wire w [f][c2:d2];", "c2", "d2"},
};

// Test that left-expression of dimension range is extracted correctly.
TEST(GetDimensionRangeLeftBoundTest, CheckBounds) {
  for (const auto &test : kRangeTestCases) {
    VerilogAnalyzer analyzer(test.code, "");
    ASSERT_OK(analyzer.Analyze()) << "Failed test code: " << test.code;
    const auto &root = analyzer.Data().SyntaxTree();
    const auto range_matches =
        SearchSyntaxTree(*ABSL_DIE_IF_NULL(root), NodekDimensionRange());
    ASSERT_EQ(range_matches.size(), 1);
    const auto *left = GetDimensionRangeLeftBound(*range_matches.front().match);
    const SyntaxTreeLeaf *left_leaf = verible::GetLeftmostLeaf(*left);
    EXPECT_EQ(ABSL_DIE_IF_NULL(left_leaf)->get().text(), test.expect_left);
  }
}

// Test that right-expression of dimension range is extracted correctly.
TEST(GetDimensionRangeRightBoundTest, CheckBounds) {
  for (const auto &test : kRangeTestCases) {
    VerilogAnalyzer analyzer(test.code, "");
    ASSERT_OK(analyzer.Analyze()) << "Failed test code: " << test.code;
    const auto &root = analyzer.Data().SyntaxTree();
    const auto range_matches =
        SearchSyntaxTree(*ABSL_DIE_IF_NULL(root), NodekDimensionRange());
    ASSERT_EQ(range_matches.size(), 1);
    const auto *right =
        GetDimensionRangeRightBound(*range_matches.front().match);
    const SyntaxTreeLeaf *right_leaf = verible::GetLeftmostLeaf(*right);
    EXPECT_EQ(ABSL_DIE_IF_NULL(right_leaf)->get().text(), test.expect_right);
  }
}

}  // namespace
}  // namespace verilog
