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

#include "absl/log/die_if_null.h"
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
    {.code = "", .expect_packed_matches = 0, .expect_unpacked_matches = 0},
    // package_or_generate_item_declaration level tests
    {.code = "wire w;",
     .expect_packed_matches = 0,
     .expect_unpacked_matches = 0},
    {.code = "wire [1:0] w;",
     .expect_packed_matches = 1,
     .expect_unpacked_matches = 0},
    {.code = "wire [9:0] w;",
     .expect_packed_matches = 1,
     .expect_unpacked_matches = 0},
    {.code = "wire [0:4] w;",
     .expect_packed_matches = 1,
     .expect_unpacked_matches = 0},
    {.code = "wire [1:0][3:0] w;",
     .expect_packed_matches = 1,
     .expect_unpacked_matches = 0},  // 2 dimensional, but 1 set of packed
    {.code = "wire w [0:1];",
     .expect_packed_matches = 0,
     .expect_unpacked_matches = 1},
    {.code = "wire w [1:0];",
     .expect_packed_matches = 0,
     .expect_unpacked_matches = 1},
    {.code = "wire w [1:0][7:0];",
     .expect_packed_matches = 0,
     .expect_unpacked_matches = 1},  // 2 dimensional, but 1 set of unpacked
    {.code = "wire [2:0] w [2];",
     .expect_packed_matches = 1,
     .expect_unpacked_matches = 1},
    {.code = "wire [2:0][1:0] w [4][2];",
     .expect_packed_matches = 1,
     .expect_unpacked_matches = 1},
    {.code = "wire [1:0] w; wire [1:0] x;",
     .expect_packed_matches = 2,
     .expect_unpacked_matches = 0},  // separate declarations
    {.code = "wire w [1:0]; wire x [1:0];",
     .expect_packed_matches = 0,
     .expect_unpacked_matches = 2},  // separate declarations

    // Test different data_declaration types.
    {.code = "logic l;",
     .expect_packed_matches = 0,
     .expect_unpacked_matches = 0},
    {.code = "logic [1:0] l;",
     .expect_packed_matches = 1,
     .expect_unpacked_matches = 0},
    {.code = "logic l [1:0];",
     .expect_packed_matches = 0,
     .expect_unpacked_matches = 1},
    {.code = "bit b;",
     .expect_packed_matches = 0,
     .expect_unpacked_matches = 0},
    {.code = "bit [1:0] b;",
     .expect_packed_matches = 1,
     .expect_unpacked_matches = 0},
    {.code = "bit b [1:0];",
     .expect_packed_matches = 0,
     .expect_unpacked_matches = 1},
    {.code = "reg r;",
     .expect_packed_matches = 0,
     .expect_unpacked_matches = 0},
    {.code = "reg [1:0] r;",
     .expect_packed_matches = 1,
     .expect_unpacked_matches = 0},
    {.code = "reg r [1:0];",
     .expect_packed_matches = 0,
     .expect_unpacked_matches = 1},
    {.code = "mytype m;",
     .expect_packed_matches = 0,
     .expect_unpacked_matches = 0},
    {.code = "mytype [1:0] m;",
     .expect_packed_matches = 1,
     .expect_unpacked_matches = 0},
    {.code = "mytype m [1:0];",
     .expect_packed_matches = 0,
     .expect_unpacked_matches = 1},
    {.code = "mypkg::mytype m;",
     .expect_packed_matches = 0,
     .expect_unpacked_matches = 0},
    {.code = "mypkg::mytype [1:0] m;",
     .expect_packed_matches = 1,
     .expect_unpacked_matches = 0},
    {.code = "mypkg::mytype m [1:0];",
     .expect_packed_matches = 0,
     .expect_unpacked_matches = 1},
    {.code = "signed m;",
     .expect_packed_matches = 0,
     .expect_unpacked_matches = 0},
    {.code = "signed [1:0] m;",
     .expect_packed_matches = 1,
     .expect_unpacked_matches = 0},
    {.code = "signed m [1:0];",
     .expect_packed_matches = 0,
     .expect_unpacked_matches = 1},
    {.code = "unsigned m;",
     .expect_packed_matches = 0,
     .expect_unpacked_matches = 0},
    {.code = "unsigned [1:0] m;",
     .expect_packed_matches = 1,
     .expect_unpacked_matches = 0},
    {.code = "unsigned m [1:0];",
     .expect_packed_matches = 0,
     .expect_unpacked_matches = 1},
    {.code = "event e;",
     .expect_packed_matches = 0,
     .expect_unpacked_matches = 0},
    {.code = "event [1:0] e;",
     .expect_packed_matches = 1,
     .expect_unpacked_matches = 0},
    {.code = "event e [1:0];",
     .expect_packed_matches = 0,
     .expect_unpacked_matches = 1},

    // Test unnamed struct members
    {.code = "struct { logic l; } s;",
     .expect_packed_matches = 0,
     .expect_unpacked_matches = 0},
    {.code = "struct { logic [2:0] l; } s;",
     .expect_packed_matches = 1,
     .expect_unpacked_matches = 0},
    {.code = "struct { logic l [2:0]; } s;",
     .expect_packed_matches = 0,
     .expect_unpacked_matches = 1},
    // Test typedef struct members
    {.code = "typedef struct { logic l; } s_s;",
     .expect_packed_matches = 0,
     .expect_unpacked_matches = 0},
    {.code = "typedef struct { logic [2:0] l; } s_s;",
     .expect_packed_matches = 1,
     .expect_unpacked_matches = 0},
    {.code = "typedef struct { logic l [2:0]; } s_s;",
     .expect_packed_matches = 0,
     .expect_unpacked_matches = 1},

    // Test class fields
    {.code = "class c; bit b; endclass",
     .expect_packed_matches = 0,
     .expect_unpacked_matches = 0},
    {.code = "class c; bit [1:0] b; endclass",
     .expect_packed_matches = 1,
     .expect_unpacked_matches = 0},
    {.code = "class c; bit b [0:1]; endclass",
     .expect_packed_matches = 0,
     .expect_unpacked_matches = 1},
    {.code = "class c; bit [2:0] b [0:1]; endclass",
     .expect_packed_matches = 1,
     .expect_unpacked_matches = 1},

    // Test module ports
    {.code = "module m(input wire foo); endmodule",
     .expect_packed_matches = 0,
     .expect_unpacked_matches = 0},
    {.code = "module m(input wire [2:0] foo); endmodule",
     .expect_packed_matches = 1,
     .expect_unpacked_matches = 0},
    {.code = "module m(input wire foo [2:0]); endmodule",
     .expect_packed_matches = 0,
     .expect_unpacked_matches = 1},
    {.code = "module m(input wire [2:0] foo [2:0]); endmodule",
     .expect_packed_matches = 1,
     .expect_unpacked_matches = 1},
    {.code = "module m(output reg foo); endmodule",
     .expect_packed_matches = 0,
     .expect_unpacked_matches = 0},
    {.code = "module m(output reg [2:0] foo); endmodule",
     .expect_packed_matches = 1,
     .expect_unpacked_matches = 0},
    {.code = "module m(output reg foo [2:0]); endmodule",
     .expect_packed_matches = 0,
     .expect_unpacked_matches = 1},
    {.code = "module m(output reg [2:0] foo [2:0]); endmodule",
     .expect_packed_matches = 1,
     .expect_unpacked_matches = 1},

    // Test module local declarations
    {.code = "module m; wire foo; endmodule",
     .expect_packed_matches = 0,
     .expect_unpacked_matches = 0},
    {.code = "module m; wire [4:0] foo; endmodule",
     .expect_packed_matches = 1,
     .expect_unpacked_matches = 0},
    {.code = "module m; wire foo[5]; endmodule",
     .expect_packed_matches = 0,
     .expect_unpacked_matches = 1},
    {.code = "module m; wire [4:0] foo[5]; endmodule",
     .expect_packed_matches = 1,
     .expect_unpacked_matches = 1},
    {.code = "module m; submod foo; endmodule",
     .expect_packed_matches = 0,
     .expect_unpacked_matches = 0},
    {.code = "module m; submod foo[5]; endmodule",
     .expect_packed_matches = 0,
     .expect_unpacked_matches = 1},

    // Test function ports
    {.code = "function void f(bit foo); endfunction",
     .expect_packed_matches = 0,
     .expect_unpacked_matches = 0},
    {.code = "function void f(bit [2:0] foo); endfunction",
     .expect_packed_matches = 1,
     .expect_unpacked_matches = 0},
    {.code = "function void f(bit foo [2:0]); endfunction",
     .expect_packed_matches = 0,
     .expect_unpacked_matches = 1},

    // Test function locals
    {.code = "function void f; bit foo; endfunction",
     .expect_packed_matches = 0,
     .expect_unpacked_matches = 0},
    {.code = "function void f; bit [3:0] foo; endfunction",
     .expect_packed_matches = 1,
     .expect_unpacked_matches = 0},
    {.code = "function void f; bit foo [3:0]; endfunction",
     .expect_packed_matches = 0,
     .expect_unpacked_matches = 1},

    // Test function return types
    {.code = "function bit foo; endfunction",
     .expect_packed_matches = 0,
     .expect_unpacked_matches = 0},
    {.code = "function bit [2:0] foo; endfunction",
     .expect_packed_matches = 1,
     .expect_unpacked_matches = 0},

    // Test task ports
    {.code = "task automatic t(bit foo); endtask",
     .expect_packed_matches = 0,
     .expect_unpacked_matches = 0},
    {.code = "task automatic t(bit [2:0] foo); endtask",
     .expect_packed_matches = 1,
     .expect_unpacked_matches = 0},
    {.code = "task automatic t(bit foo [2:0]); endtask",
     .expect_packed_matches = 0,
     .expect_unpacked_matches = 1},

    // Test task locals
    {.code = "task automatic t; bit foo; endtask",
     .expect_packed_matches = 0,
     .expect_unpacked_matches = 0},
    {.code = "task automatic t; bit [2:0] foo; endtask",
     .expect_packed_matches = 1,
     .expect_unpacked_matches = 0},
    {.code = "task automatic t; bit foo [2:0]; endtask",
     .expect_packed_matches = 0,
     .expect_unpacked_matches = 1},

    // Test parameters
    {.code = "parameter int p = 0;",
     .expect_packed_matches = 0,
     .expect_unpacked_matches = 0},
    {.code = "parameter int [3:0] p = 0;",
     .expect_packed_matches = 1,
     .expect_unpacked_matches = 0},
    {.code = "parameter int p [3:0] = 0;",
     .expect_packed_matches = 0,
     .expect_unpacked_matches = 1},
    {.code = "localparam int p = 0;",
     .expect_packed_matches = 0,
     .expect_unpacked_matches = 0},
    {.code = "localparam int [3:0] p = 0;",
     .expect_packed_matches = 1,
     .expect_unpacked_matches = 0},
    {.code = "localparam int p [3:0] = 0;",
     .expect_packed_matches = 0,
     .expect_unpacked_matches = 1},
    {.code = "parameter int p = q[0];",
     .expect_packed_matches = 0,
     .expect_unpacked_matches = 0},  // selection is not declaration
    {.code = "parameter int p = q[1:0];",
     .expect_packed_matches = 0,
     .expect_unpacked_matches = 0},  // selection is not declaration
    {.code = "parameter int p = 0, q = 1;",
     .expect_packed_matches = 0,
     .expect_unpacked_matches = 0},  // multiple assignments
    {.code = "parameter int [1:0] p = 0, q = 1;",
     .expect_packed_matches = 1,
     .expect_unpacked_matches = 0},  // multiple assignments
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
      {.code = "wire w;", .expect_dimensions = 0},
      {.code = "wire [] w;", .expect_dimensions = 1},
      {.code = "wire [1:0] w;", .expect_dimensions = 1},
      {.code = "wire [1:0][1:0] w;", .expect_dimensions = 2},
      {.code = "wire w [0:1];", .expect_dimensions = 1},
      {.code = "wire w [0:1][0:3];", .expect_dimensions = 2},
      {.code = "wire w [2];", .expect_dimensions = 1},
      {.code = "wire w [3][5];", .expect_dimensions = 2},
      {.code = "wire w [];", .expect_dimensions = 1},
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
    {.code = "wire [a:b] w;", .expect_left = "a", .expect_right = "b"},
    {.code = "wire w [c:d];", .expect_left = "c", .expect_right = "d"},
    {.code = "wire w [c1:d1][e];", .expect_left = "c1", .expect_right = "d1"},
    {.code = "wire w [f][c2:d2];", .expect_left = "c2", .expect_right = "d2"},
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
