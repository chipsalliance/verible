# 'text' contains classes for structural representations of text/code,
# such as token streams, syntax trees.

package(
    default_applicable_licenses = ["//:license"],
    # library layering: common/ packages that may NOT depend on this one:
    #   strings/
    #   util/
    default_visibility = [
        "//common/analysis:__subpackages__",
        "//common/formatting:__subpackages__",
        "//common/lexer:__subpackages__",
        "//common/parser:__subpackages__",
        "//verilog:__subpackages__",
    ],
    features = ["layering_check"],
)

cc_library(
    name = "token-info",
    srcs = ["token-info.cc"],
    hdrs = ["token-info.h"],
    deps = [
        ":constants",
        "//common/strings:rebase",
        "//common/util:iterator-range",
        "//common/util:logging",
        "//common/util:range",
        "@com_google_absl//absl/strings",
        "@com_google_absl//absl/strings:string_view",
    ],
)

cc_library(
    name = "token-info-json",
    srcs = ["token-info-json.cc"],
    hdrs = ["token-info-json.h"],
    deps = [
        ":token-info",
        "@jsonhpp//:singleheader-json",
    ],
)

cc_library(
    name = "token-info-test-util",
    testonly = 1,
    srcs = ["token-info-test-util.cc"],
    hdrs = ["token-info-test-util.h"],
    deps = [
        ":token-info",
        "//common/util:logging",
        "@com_google_absl//absl/strings:string_view",
    ],
)

cc_library(
    name = "token-stream-view",
    srcs = ["token-stream-view.cc"],
    hdrs = ["token-stream-view.h"],
    deps = [
        ":token-info",
        "//common/util:iterator-range",
        "@com_google_absl//absl/strings:string_view",
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
        "//common/util:casts",
        "//common/util:logging",
    ],
)

cc_test(
    name = "concrete-syntax-leaf_test",
    srcs = ["concrete-syntax-leaf_test.cc"],
    deps = [
        ":concrete-syntax-leaf",
        ":token-info",
        "@com_google_absl//absl/strings:string_view",
        "@com_google_googletest//:gtest",
        "@com_google_googletest//:gtest_main",
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
        "//common/util:casts",
        "//common/util:iterator-range",
        "//common/util:logging",
        "@com_google_absl//absl/base:core_headers",
    ],
)

cc_library(
    name = "config-utils",
    srcs = ["config-utils.cc"],
    hdrs = ["config-utils.h"],
    deps = [
        "//common/util:logging",
        "@com_google_absl//absl/status",
        "@com_google_absl//absl/strings",
        "@com_google_absl//absl/strings:string_view",
        "@com_googlesource_code_re2//:re2",
    ],
)

cc_test(
    name = "config-utils_test",
    srcs = ["config-utils_test.cc"],
    deps = [
        ":config-utils",
        "@com_google_absl//absl/status",
        "@com_google_absl//absl/strings",
        "@com_google_absl//absl/strings:string_view",
        "@com_google_googletest//:gtest",
        "@com_google_googletest//:gtest_main",
        "@com_googlesource_code_re2//:re2",
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
        "//common/util:auto-pop-stack",
        "//common/util:iterator-adaptors",
        "//common/util:logging",
    ],
)

cc_library(
    name = "tree-compare",
    srcs = ["tree-compare.cc"],
    hdrs = ["tree-compare.h"],
    deps = [
        ":symbol",
        ":token-info",
        "@com_google_absl//absl/strings:string_view",
    ],
)

cc_test(
    name = "tree-compare_test",
    srcs = ["tree-compare_test.cc"],
    deps = [
        ":concrete-syntax-tree",
        ":tree-builder-test-util",
        ":tree-compare",
        "@com_google_absl//absl/strings:string_view",
        "@com_google_googletest//:gtest",
        "@com_google_googletest//:gtest_main",
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
        "//common/strings:display-utils",
        "//common/util:logging",
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
        "@com_google_absl//absl/strings:string_view",
        "@com_google_googletest//:gtest",
        "@com_google_googletest//:gtest_main",
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
        "//common/util:casts",
        "//common/util:iterator-adaptors",
        "//common/util:logging",
        "//common/util:spacer",
        "//common/util:type-traits",
        "//common/util:value-saver",
        "@com_google_absl//absl/log:check",
        "@com_google_absl//absl/strings",
        "@com_google_absl//absl/strings:string_view",
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
        "//common/util:logging",
        "@com_google_absl//absl/strings:string_view",
    ],
)

cc_test(
    name = "tree-builder-test-util_test",
    srcs = ["tree-builder-test-util_test.cc"],
    deps = [
        ":symbol-ptr",
        ":tree-builder-test-util",
        ":tree-utils",
        "@com_google_googletest//:gtest",
        "@com_google_googletest//:gtest_main",
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
        "//common/strings:line-column-map",
        "//common/strings:mem-block",
        "//common/util:iterator-range",
        "//common/util:logging",
        "//common/util:range",
        "//common/util:status-macros",
        "@com_google_absl//absl/status",
        "@com_google_absl//absl/strings",
        "@com_google_absl//absl/strings:string_view",
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
        "//common/util:logging",
        "@com_google_absl//absl/strings",
        "@com_google_absl//absl/strings:string_view",
    ],
)

cc_library(
    name = "macro-definition",
    srcs = ["macro-definition.cc"],
    hdrs = ["macro-definition.h"],
    deps = [
        ":token-info",
        "//common/util:container-util",
        "@com_google_absl//absl/status",
        "@com_google_absl//absl/strings",
        "@com_google_absl//absl/strings:string_view",
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
        "//common/analysis:__pkg__",
        "//common/formatting:__pkg__",
        "//common/lexer:__pkg__",
        "//verilog/CST:__pkg__",
        "//verilog/analysis:__pkg__",
        "//verilog/analysis/checkers:__pkg__",
        "//verilog/formatting:__pkg__",
        "//verilog/parser:__pkg__",
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
        "//common/util:casts",
        "//common/util:logging",
        "//common/util:range",
        "@com_google_absl//absl/strings:string_view",
        "@com_google_googletest//:gtest",
        "@com_google_googletest//:gtest_main",
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
        "//common/util:casts",
        "//common/util:logging",
        "@com_google_googletest//:gtest",
        "@com_google_googletest//:gtest_main",
    ],
)

cc_test(
    name = "token-info_test",
    srcs = ["token-info_test.cc"],
    deps = [
        ":constants",
        ":token-info",
        "//common/util:range",
        "@com_google_absl//absl/strings:string_view",
        "@com_google_googletest//:gtest",
        "@com_google_googletest//:gtest_main",
    ],
)

cc_test(
    name = "token-info-json_test",
    srcs = ["token-info-json_test.cc"],
    deps = [
        ":constants",
        ":token-info",
        ":token-info-json",
        "@com_google_absl//absl/strings:string_view",
        "@com_google_googletest//:gtest",
        "@com_google_googletest//:gtest_main",
        "@jsonhpp//:singleheader-json",
    ],
)

cc_test(
    name = "token-info-test-util_test",
    srcs = ["token-info-test-util_test.cc"],
    deps = [
        ":token-info",
        ":token-info-test-util",
        "@com_google_absl//absl/strings:string_view",
        "@com_google_googletest//:gtest",
        "@com_google_googletest//:gtest_main",
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
        "//common/util:iterator-range",
        "@com_google_absl//absl/strings:string_view",
        "@com_google_googletest//:gtest",
        "@com_google_googletest//:gtest_main",
    ],
)

cc_test(
    name = "syntax-tree-context_test",
    srcs = ["syntax-tree-context_test.cc"],
    deps = [
        ":concrete-syntax-tree",
        ":syntax-tree-context",
        "//common/util:iterator-range",
        "@com_google_googletest//:gtest",
        "@com_google_googletest//:gtest_main",
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
        "//common/strings:line-column-map",
        "//common/util:casts",
        "//common/util:iterator-range",
        "//common/util:logging",
        "//common/util:range",
        "//common/util:value-saver",
        "@com_google_absl//absl/status",
        "@com_google_absl//absl/strings:string_view",
        "@com_google_googletest//:gtest",
        "@com_google_googletest//:gtest_main",
    ],
)

cc_test(
    name = "macro-definition_test",
    srcs = ["macro-definition_test.cc"],
    deps = [
        ":macro-definition",
        ":token-info",
        "//common/util:container-util",
        "@com_google_absl//absl/status",
        "@com_google_googletest//:gtest",
        "@com_google_googletest//:gtest_main",
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
        "@com_google_absl//absl/strings:string_view",
        "@com_google_googletest//:gtest",
        "@com_google_googletest//:gtest_main",
    ],
)
