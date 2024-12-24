# 'text' contains classes for structural representations of text/code,
# such as token streams, syntax trees.

package(
    default_applicable_licenses = ["//:license"],
    # library layering: common/ packages that may NOT depend on this one:
    #   strings/
    #   util/
    default_visibility = [
        "//verible/common/analysis:__subpackages__",
        "//verible/common/formatting:__subpackages__",
        "//verible/common/lexer:__subpackages__",
        "//verible/common/parser:__subpackages__",
        "//verible/verilog:__subpackages__",
    ],
    features = ["layering_check"],
)

cc_library(
    name = "token-info",
    srcs = ["token-info.cc"],
    hdrs = ["token-info.h"],
    deps = [
        ":constants",
        "//verible/common/strings:rebase",
        "//verible/common/util:iterator-range",
        "//verible/common/util:logging",
        "//verible/common/util:range",
        "@abseil-cpp//absl/strings",
        "@abseil-cpp//absl/strings:string_view",
    ],
)

cc_library(
    name = "token-info-json",
    srcs = ["token-info-json.cc"],
    hdrs = ["token-info-json.h"],
    deps = [
        ":token-info",
        "@nlohmann_json//:singleheader-json",
    ],
)

cc_library(
    name = "token-info-test-util",
    testonly = 1,
    srcs = ["token-info-test-util.cc"],
    hdrs = ["token-info-test-util.h"],
    deps = [
        ":token-info",
        "//verible/common/util:logging",
        "@abseil-cpp//absl/strings:string_view",
    ],
)

cc_library(
    name = "token-stream-view",
    srcs = ["token-stream-view.cc"],
    hdrs = ["token-stream-view.h"],
    deps = [
        ":token-info",
        "//verible/common/util:iterator-range",
        "@abseil-cpp//absl/strings:string_view",
    ],
)

cc_library(
    name = "symbol-ptr",
    hdrs = ["symbol-ptr.h"],
)

cc_library(
    name = "symbol",
    srcs = ["symbol.cc"],
    hdrs = ["symbol.h"],
    deps = [
        ":symbol-ptr",
        ":token-info",
        ":visitors",
    ],
)

cc_library(
    name = "concrete-syntax-leaf",
    srcs = ["concrete-syntax-leaf.cc"],
    hdrs = ["concrete-syntax-leaf.h"],
    deps = [
        ":concrete-syntax-tree",
        ":symbol",
        ":token-info",
        ":tree-compare",
        ":visitors",
        "//verible/common/util:casts",
        "//verible/common/util:logging",
    ],
)

cc_test(
    name = "concrete-syntax-leaf_test",
    srcs = ["concrete-syntax-leaf_test.cc"],
    deps = [
        ":concrete-syntax-leaf",
        ":token-info",
        "@abseil-cpp//absl/strings:string_view",
        "@googletest//:gtest",
        "@googletest//:gtest_main",
    ],
)

cc_library(
    name = "concrete-syntax-tree",
    srcs = ["concrete-syntax-tree.cc"],
    hdrs = ["concrete-syntax-tree.h"],
    deps = [
        ":constants",
        ":symbol",
        ":symbol-ptr",
        ":tree-compare",
        ":visitors",
        "//verible/common/util:casts",
        "//verible/common/util:iterator-range",
        "//verible/common/util:logging",
        "@abseil-cpp//absl/base:core_headers",
    ],
)

cc_library(
    name = "config-utils",
    srcs = ["config-utils.cc"],
    hdrs = ["config-utils.h"],
    deps = [
        "//verible/common/util:logging",
        "@abseil-cpp//absl/status",
        "@abseil-cpp//absl/strings",
        "@abseil-cpp//absl/strings:string_view",
        "@re2",
    ],
)

cc_test(
    name = "config-utils_test",
    srcs = ["config-utils_test.cc"],
    deps = [
        ":config-utils",
        "@abseil-cpp//absl/status",
        "@abseil-cpp//absl/strings",
        "@abseil-cpp//absl/strings:string_view",
        "@googletest//:gtest",
        "@googletest//:gtest_main",
        "@re2",
    ],
)

cc_library(
    name = "visitors",
    hdrs = ["visitors.h"],
    deps = [
        ":symbol-ptr",
    ],
)

cc_library(
    name = "syntax-tree-context",
    hdrs = ["syntax-tree-context.h"],
    deps = [
        ":concrete-syntax-tree",
        "//verible/common/util:auto-pop-stack",
        "//verible/common/util:iterator-adaptors",
        "//verible/common/util:logging",
    ],
)

cc_library(
    name = "tree-compare",
    srcs = ["tree-compare.cc"],
    hdrs = ["tree-compare.h"],
    deps = [
        ":symbol",
        ":token-info",
    ],
)

cc_test(
    name = "tree-compare_test",
    srcs = ["tree-compare_test.cc"],
    deps = [
        ":concrete-syntax-tree",
        ":tree-builder-test-util",
        ":tree-compare",
        "@abseil-cpp//absl/strings:string_view",
        "@googletest//:gtest",
        "@googletest//:gtest_main",
    ],
)

cc_library(
    name = "tree-context-visitor",
    srcs = ["tree-context-visitor.cc"],
    hdrs = ["tree-context-visitor.h"],
    deps = [
        ":concrete-syntax-tree",
        ":syntax-tree-context",
        ":visitors",
        "//verible/common/strings:display-utils",
        "//verible/common/util:logging",
    ],
)

cc_test(
    name = "tree-context-visitor_test",
    srcs = ["tree-context-visitor_test.cc"],
    deps = [
        ":concrete-syntax-leaf",
        ":concrete-syntax-tree",
        ":symbol-ptr",
        ":syntax-tree-context",
        ":tree-builder-test-util",
        ":tree-context-visitor",
        "@abseil-cpp//absl/strings:string_view",
        "@googletest//:gtest",
        "@googletest//:gtest_main",
    ],
)

cc_library(
    name = "tree-utils",
    srcs = ["tree-utils.cc"],
    hdrs = ["tree-utils.h"],
    deps = [
        ":concrete-syntax-leaf",
        ":concrete-syntax-tree",
        ":symbol",
        ":token-info",
        ":visitors",
        "//verible/common/util:casts",
        "//verible/common/util:iterator-adaptors",
        "//verible/common/util:logging",
        "//verible/common/util:spacer",
        "//verible/common/util:type-traits",
        "//verible/common/util:value-saver",
        "@abseil-cpp//absl/log:check",
        "@abseil-cpp//absl/strings",
        "@abseil-cpp//absl/strings:string_view",
    ],
)

cc_library(
    name = "tree-builder-test-util",
    testonly = 1,
    srcs = ["tree-builder-test-util.cc"],
    hdrs = ["tree-builder-test-util.h"],
    deps = [
        ":concrete-syntax-leaf",
        ":concrete-syntax-tree",
        ":symbol",
        ":tree-utils",
        "//verible/common/util:logging",
        "@abseil-cpp//absl/strings:string_view",
    ],
)

cc_test(
    name = "tree-builder-test-util_test",
    srcs = ["tree-builder-test-util_test.cc"],
    deps = [
        ":symbol-ptr",
        ":tree-builder-test-util",
        ":tree-utils",
        "@googletest//:gtest",
        "@googletest//:gtest_main",
    ],
)

cc_library(
    name = "text-structure",
    srcs = ["text-structure.cc"],
    hdrs = ["text-structure.h"],
    deps = [
        ":concrete-syntax-leaf",
        ":concrete-syntax-tree",
        ":symbol",
        ":token-info",
        ":token-stream-view",
        ":tree-utils",
        "//verible/common/strings:line-column-map",
        "//verible/common/strings:mem-block",
        "//verible/common/util:iterator-range",
        "//verible/common/util:logging",
        "//verible/common/util:range",
        "//verible/common/util:status-macros",
        "@abseil-cpp//absl/status",
        "@abseil-cpp//absl/strings",
        "@abseil-cpp//absl/strings:string_view",
    ],
)

cc_library(
    name = "text-structure-test-utils",
    testonly = 1,
    srcs = ["text-structure-test-utils.cc"],
    hdrs = ["text-structure-test-utils.h"],
    deps = [
        ":concrete-syntax-tree",
        ":text-structure",
        ":token-info",
        ":token-stream-view",
        ":tree-builder-test-util",
        "//verible/common/util:logging",
        "@abseil-cpp//absl/strings",
        "@abseil-cpp//absl/strings:string_view",
    ],
)

cc_library(
    name = "macro-definition",
    srcs = ["macro-definition.cc"],
    hdrs = ["macro-definition.h"],
    deps = [
        ":token-info",
        "//verible/common/util:container-util",
        "@abseil-cpp//absl/status",
        "@abseil-cpp//absl/strings",
        "@abseil-cpp//absl/strings:string_view",
    ],
)

cc_library(
    name = "parser-verifier",
    srcs = ["parser-verifier.cc"],
    hdrs = ["parser-verifier.h"],
    deps = [
        ":concrete-syntax-leaf",
        ":concrete-syntax-tree",
        ":symbol",
        ":token-info",
        ":token-stream-view",
        ":tree-compare",
        ":visitors",
    ],
)

cc_library(
    name = "constants",
    hdrs = ["constants.h"],
    # TODO(fangism): minimize direct exposure of constants
    visibility = [
        "//verible/common/analysis:__pkg__",
        "//verible/common/formatting:__pkg__",
        "//verible/common/lexer:__pkg__",
        "//verible/verilog/CST:__pkg__",
        "//verible/verilog/analysis:__pkg__",
        "//verible/verilog/analysis/checkers:__pkg__",
        "//verible/verilog/formatting:__pkg__",
        "//verible/verilog/parser:__pkg__",
    ],
)

cc_test(
    name = "tree-utils_test",
    srcs = ["tree-utils_test.cc"],
    deps = [
        ":concrete-syntax-leaf",
        ":concrete-syntax-tree",
        ":symbol",
        ":token-info",
        ":tree-builder-test-util",
        ":tree-compare",
        ":tree-utils",
        "//verible/common/util:casts",
        "//verible/common/util:logging",
        "//verible/common/util:range",
        "@abseil-cpp//absl/strings:string_view",
        "@googletest//:gtest",
        "@googletest//:gtest_main",
    ],
)

cc_test(
    name = "concrete-syntax-tree_test",
    srcs = ["concrete-syntax-tree_test.cc"],
    deps = [
        ":concrete-syntax-leaf",
        ":concrete-syntax-tree",
        ":symbol",
        ":tree-builder-test-util",
        ":tree-compare",
        "//verible/common/util:casts",
        "//verible/common/util:logging",
        "@googletest//:gtest",
        "@googletest//:gtest_main",
    ],
)

cc_test(
    name = "token-info_test",
    srcs = ["token-info_test.cc"],
    deps = [
        ":constants",
        ":token-info",
        "//verible/common/util:range",
        "@abseil-cpp//absl/strings:string_view",
        "@googletest//:gtest",
        "@googletest//:gtest_main",
    ],
)

cc_test(
    name = "token-info-json_test",
    srcs = ["token-info-json_test.cc"],
    deps = [
        ":constants",
        ":token-info",
        ":token-info-json",
        "@abseil-cpp//absl/strings:string_view",
        "@googletest//:gtest",
        "@googletest//:gtest_main",
        "@nlohmann_json//:singleheader-json",
    ],
)

cc_test(
    name = "token-info-test-util_test",
    srcs = ["token-info-test-util_test.cc"],
    deps = [
        ":token-info",
        ":token-info-test-util",
        "@abseil-cpp//absl/strings:string_view",
        "@googletest//:gtest",
        "@googletest//:gtest_main",
    ],
)

cc_test(
    name = "token-stream-view_test",
    srcs = ["token-stream-view_test.cc"],
    deps = [
        ":text-structure",
        ":text-structure-test-utils",
        ":token-info",
        ":token-stream-view",
        "//verible/common/util:iterator-range",
        "@abseil-cpp//absl/strings:string_view",
        "@googletest//:gtest",
        "@googletest//:gtest_main",
    ],
)

cc_test(
    name = "syntax-tree-context_test",
    srcs = ["syntax-tree-context_test.cc"],
    deps = [
        ":concrete-syntax-tree",
        ":syntax-tree-context",
        "//verible/common/util:iterator-range",
        "@googletest//:gtest",
        "@googletest//:gtest_main",
    ],
)

cc_test(
    name = "text-structure_test",
    srcs = ["text-structure_test.cc"],
    deps = [
        ":concrete-syntax-tree",
        ":constants",
        ":symbol",
        ":text-structure",
        ":text-structure-test-utils",
        ":token-info",
        ":token-stream-view",
        ":tree-builder-test-util",
        ":tree-compare",
        "//verible/common/strings:line-column-map",
        "//verible/common/util:casts",
        "//verible/common/util:iterator-range",
        "//verible/common/util:logging",
        "//verible/common/util:range",
        "//verible/common/util:value-saver",
        "@abseil-cpp//absl/status",
        "@abseil-cpp//absl/strings:string_view",
        "@googletest//:gtest",
        "@googletest//:gtest_main",
    ],
)

cc_test(
    name = "macro-definition_test",
    srcs = ["macro-definition_test.cc"],
    deps = [
        ":macro-definition",
        ":token-info",
        "//verible/common/util:container-util",
        "@abseil-cpp//absl/status",
        "@googletest//:gtest",
        "@googletest//:gtest_main",
    ],
)

cc_test(
    name = "parser-verifier_test",
    srcs = ["parser-verifier_test.cc"],
    deps = [
        ":constants",
        ":parser-verifier",
        ":token-info",
        ":token-stream-view",
        ":tree-builder-test-util",
        "@abseil-cpp//absl/strings:string_view",
        "@googletest//:gtest",
        "@googletest//:gtest_main",
    ],
)
