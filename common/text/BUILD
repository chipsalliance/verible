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
)

cc_library(
    name = "token-info",
    srcs = ["token_info.cc"],
    hdrs = ["token_info.h"],
    deps = [
        ":constants",
        "//common/strings:rebase",
        "//common/util:iterator-range",
        "//common/util:logging",
        "//common/util:range",
        "@com_google_absl//absl/strings",
    ],
)

cc_library(
    name = "token-info-json",
    srcs = ["token_info_json.cc"],
    hdrs = ["token_info_json.h"],
    deps = [
        ":token-info",
        "@jsonhpp",
    ],
)

cc_library(
    name = "token-info-test-util",
    testonly = 1,
    srcs = ["token_info_test_util.cc"],
    hdrs = ["token_info_test_util.h"],
    deps = [
        ":token-info",
        "//common/util:logging",
        "@com_google_absl//absl/strings",
    ],
)

cc_library(
    name = "token-stream-view",
    srcs = ["token_stream_view.cc"],
    hdrs = ["token_stream_view.h"],
    deps = [
        ":token-info",
        "//common/util:iterator-range",
    ],
)

cc_library(
    name = "symbol-ptr",
    hdrs = ["symbol_ptr.h"],
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
    srcs = ["concrete_syntax_leaf.cc"],
    hdrs = ["concrete_syntax_leaf.h"],
    deps = [
        ":concrete-syntax-tree",
        ":symbol",
        ":token-info",
        ":tree-compare",
        ":visitors",
        "//common/util:logging",
    ],
)

cc_test(
    name = "concrete-syntax-leaf_test",
    srcs = ["concrete_syntax_leaf_test.cc"],
    deps = [
        ":concrete-syntax-leaf",
        ":token-info",
        "@com_google_absl//absl/strings",
        "@com_google_googletest//:gtest_main",
    ],
)

cc_library(
    name = "concrete-syntax-tree",
    srcs = ["concrete_syntax_tree.cc"],
    hdrs = ["concrete_syntax_tree.h"],
    deps = [
        ":constants",
        ":symbol",
        ":tree-compare",
        ":visitors",
        "//common/util:casts",
        "//common/util:iterator-range",
        "//common/util:logging",
    ],
)

cc_library(
    name = "config-utils",
    srcs = ["config_utils.cc"],
    hdrs = ["config_utils.h"],
    deps = [
        "//common/util:logging",
        "@com_google_absl//absl/status",
        "@com_google_absl//absl/strings",
    ],
)

cc_test(
    name = "config-utils_test",
    srcs = ["config_utils_test.cc"],
    deps = [
        ":config-utils",
        "@com_google_absl//absl/status",
        "@com_google_absl//absl/strings",
        "@com_google_googletest//:gtest_main",
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
    hdrs = ["syntax_tree_context.h"],
    deps = [
        ":concrete-syntax-tree",
        "//common/util:auto-pop-stack",
        "//common/util:iterator-adaptors",
        "//common/util:logging",
    ],
)

cc_library(
    name = "tree-compare",
    srcs = ["tree_compare.cc"],
    hdrs = ["tree_compare.h"],
    deps = [
        ":symbol",
        ":token-info",
        "@com_google_absl//absl/strings",
    ],
)

cc_test(
    name = "tree-compare_test",
    srcs = ["tree_compare_test.cc"],
    deps = [
        ":concrete-syntax-tree",
        ":tree-builder-test-util",
        ":tree-compare",
        "@com_google_googletest//:gtest_main",
    ],
)

cc_library(
    name = "tree-context-visitor",
    srcs = ["tree_context_visitor.cc"],
    hdrs = ["tree_context_visitor.h"],
    deps = [
        ":syntax-tree-context",
        ":visitors",
        "//common/strings:display-utils",
    ],
)

cc_test(
    name = "tree-context-visitor_test",
    srcs = ["tree_context_visitor_test.cc"],
    deps = [
        ":tree-builder-test-util",
        ":tree-context-visitor",
        "@com_google_googletest//:gtest_main",
    ],
)

cc_library(
    name = "tree-utils",
    srcs = ["tree_utils.cc"],
    hdrs = ["tree_utils.h"],
    deps = [
        ":concrete-syntax-leaf",
        ":concrete-syntax-tree",
        ":symbol",
        ":token-info",
        ":visitors",
        "//common/util:iterator-adaptors",
        "//common/util:logging",
        "//common/util:spacer",
        "//common/util:type-traits",
        "//common/util:value-saver",
        "@com_google_absl//absl/strings",
    ],
)

cc_library(
    name = "tree-builder-test-util",
    testonly = 1,
    srcs = ["tree_builder_test_util.cc"],
    hdrs = ["tree_builder_test_util.h"],
    deps = [
        ":concrete-syntax-leaf",
        ":concrete-syntax-tree",
        ":symbol",
        ":tree-utils",
        "@com_google_absl//absl/strings",
    ],
)

cc_test(
    name = "tree-builder-test-util_test",
    srcs = ["tree_builder_test_util_test.cc"],
    deps = [
        ":tree-builder-test-util",
        ":tree-utils",
        "@com_google_googletest//:gtest_main",
    ],
)

cc_library(
    name = "text-structure",
    srcs = ["text_structure.cc"],
    hdrs = ["text_structure.h"],
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
    ],
)

cc_library(
    name = "text-structure-test-utils",
    testonly = 1,
    srcs = ["text_structure_test_utils.cc"],
    hdrs = ["text_structure_test_utils.h"],
    deps = [
        ":concrete-syntax-tree",
        ":text-structure",
        ":token-info",
        ":token-stream-view",
        ":tree-builder-test-util",
        "//common/util:logging",
        "@com_google_absl//absl/memory",
        "@com_google_absl//absl/strings",
    ],
)

cc_library(
    name = "macro-definition",
    srcs = ["macro_definition.cc"],
    hdrs = ["macro_definition.h"],
    deps = [
        ":token-info",
        "//common/util:container-util",
        "@com_google_absl//absl/status",
        "@com_google_absl//absl/strings",
    ],
)

cc_library(
    name = "parser-verifier",
    srcs = ["parser_verifier.cc"],
    hdrs = ["parser_verifier.h"],
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
    srcs = ["tree_utils_test.cc"],
    deps = [
        ":concrete-syntax-leaf",
        ":concrete-syntax-tree",
        ":symbol",
        ":token-info",
        ":tree-builder-test-util",
        ":tree-compare",
        ":tree-utils",
        "//common/util:logging",
        "//common/util:range",
        "@com_google_googletest//:gtest_main",
    ],
)

cc_test(
    name = "concrete-syntax-tree_test",
    srcs = ["concrete_syntax_tree_test.cc"],
    deps = [
        ":concrete-syntax-leaf",
        ":concrete-syntax-tree",
        ":symbol",
        ":tree-builder-test-util",
        ":tree-compare",
        "//common/util:logging",
        "@com_google_googletest//:gtest_main",
    ],
)

cc_test(
    name = "token-info_test",
    srcs = ["token_info_test.cc"],
    deps = [
        ":constants",
        ":token-info",
        "//common/util:range",
        "@com_google_absl//absl/base:core_headers",
        "@com_google_absl//absl/strings",
        "@com_google_googletest//:gtest_main",
    ],
)

cc_test(
    name = "token-info-json_test",
    srcs = ["token_info_json_test.cc"],
    deps = [
        ":constants",
        ":token-info",
        ":token-info-json",
        "@com_google_absl//absl/strings",
        "@com_google_googletest//:gtest_main",
    ],
)

cc_test(
    name = "token-info-test-util_test",
    srcs = ["token_info_test_util_test.cc"],
    deps = [
        ":token-info",
        ":token-info-test-util",
        "//common/util:logging",
        "@com_google_absl//absl/strings",
        "@com_google_googletest//:gtest_main",
    ],
)

cc_test(
    name = "token-stream-view_test",
    srcs = ["token_stream_view_test.cc"],
    deps = [
        ":text-structure",
        ":text-structure-test-utils",
        ":token-info",
        ":token-stream-view",
        "//common/util:iterator-range",
        "@com_google_googletest//:gtest_main",
    ],
)

cc_test(
    name = "syntax-tree-context_test",
    srcs = ["syntax_tree_context_test.cc"],
    deps = [
        ":concrete-syntax-tree",
        ":syntax-tree-context",
        "//common/util:iterator-range",
        "@com_google_googletest//:gtest_main",
    ],
)

cc_test(
    name = "text-structure_test",
    srcs = ["text_structure_test.cc"],
    deps = [
        ":concrete-syntax-tree",
        ":symbol",
        ":text-structure",
        ":text-structure-test-utils",
        ":token-info",
        ":token-stream-view",
        ":tree-builder-test-util",
        ":tree-compare",
        "//common/strings:line-column-map",
        "//common/util:iterator-range",
        "//common/util:logging",
        "//common/util:range",
        "//common/util:value-saver",
        "@com_google_absl//absl/memory",
        "@com_google_absl//absl/status",
        "@com_google_absl//absl/strings",
        "@com_google_googletest//:gtest_main",
    ],
)

cc_test(
    name = "macro-definition_test",
    srcs = ["macro_definition_test.cc"],
    deps = [
        ":macro-definition",
        ":token-info",
        "//common/util:container-util",
        "@com_google_absl//absl/status",
        "@com_google_googletest//:gtest_main",
    ],
)

cc_test(
    name = "parser-verifier_test",
    srcs = ["parser_verifier_test.cc"],
    deps = [
        ":constants",
        ":parser-verifier",
        ":token-info",
        ":token-stream-view",
        ":tree-builder-test-util",
        "@com_google_absl//absl/strings",
        "@com_google_googletest//:gtest_main",
    ],
)
