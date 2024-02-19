# 'formatting' contains functionality for language-agnostic formatting

package(
    default_applicable_licenses = ["//:license"],
    default_visibility = [
        "//verilog/formatting:__subpackages__",
    ],
    features = ["layering_check"],
)

cc_library(
    name = "align",
    srcs = ["align.cc"],
    hdrs = ["align.h"],
    deps = [
        ":format-token",
        ":token-partition-tree",
        ":unwrapped-line",
        "//common/strings:display-utils",
        "//common/strings:position",
        "//common/strings:range",
        "//common/text:concrete-syntax-leaf",
        "//common/text:concrete-syntax-tree",
        "//common/text:token-info",
        "//common/text:token-stream-view",
        "//common/text:tree-context-visitor",
        "//common/text:tree-utils",
        "//common/util:algorithm",
        "//common/util:container-iterator-range",
        "//common/util:enum-flags",
        "//common/util:iterator-range",
        "//common/util:logging",
        "//common/util:tree-operations",
        "//common/util:vector-tree",
        "//common/util:vector-tree-iterators",
        "@com_google_absl//absl/strings",
    ],
)

cc_test(
    name = "align_test",
    srcs = ["align_test.cc"],
    deps = [
        ":align",
        ":format-token",
        ":token-partition-tree",
        ":token-partition-tree-test-utils",
        ":unwrapped-line-test-utils",
        "//common/text:tree-builder-test-util",
        "//common/util:logging",
        "//common/util:range",
        "//common/util:spacer",
        "//common/util:value-saver",
        "@com_google_absl//absl/strings",
        "@com_google_googletest//:gtest",
        "@com_google_googletest//:gtest_main",
    ],
)

cc_library(
    name = "basic-format-style",
    srcs = ["basic_format_style.cc"],
    hdrs = ["basic_format_style.h"],
    deps = [
        "//common/util:enum-flags",
        "@com_google_absl//absl/strings",
    ],
)

cc_library(
    name = "basic-format-style-init",
    srcs = ["basic_format_style_init.cc"],
    hdrs = ["basic_format_style_init.h"],
    deps = [
        ":basic-format-style",
        "@com_google_absl//absl/flags:flag",
    ],
)

cc_test(
    name = "basic-format-style_test",
    srcs = ["basic_format_style_test.cc"],
    deps = [
        ":basic-format-style",
        "@com_google_googletest//:gtest",
        "@com_google_googletest//:gtest_main",
    ],
)

cc_library(
    name = "format-token",
    srcs = ["format_token.cc"],
    hdrs = ["format_token.h"],
    deps = [
        "//common/strings:display-utils",
        "//common/strings:position",
        "//common/strings:range",
        "//common/text:concrete-syntax-leaf",
        "//common/text:token-info",
        "//common/util:container-iterator-range",
        "//common/util:iterator-adaptors",
        "//common/util:logging",
        "//common/util:spacer",
        "@com_google_absl//absl/base:core_headers",
        "@com_google_absl//absl/strings",
    ],
)

cc_test(
    name = "format-token_test",
    srcs = ["format_token_test.cc"],
    deps = [
        ":format-token",
        ":unwrapped-line",
        ":unwrapped-line-test-utils",
        "//common/text:token-info",
        "//common/util:range",
        "@com_google_absl//absl/strings",
        "@com_google_googletest//:gtest",
        "@com_google_googletest//:gtest_main",
    ],
)

cc_library(
    name = "layout-optimizer",
    srcs = [
        "layout_optimizer.cc",
        "layout_optimizer_internal.h",
    ],
    hdrs = ["layout_optimizer.h"],
    deps = [
        ":basic-format-style",
        ":line-wrap-searcher",
        ":token-partition-tree",
        ":unwrapped-line",
        "//common/util:container-iterator-range",
        "//common/util:logging",
        "//common/util:tree-operations",
        "//common/util:value-saver",
        "//common/util:vector-tree",
        "@com_google_absl//absl/container:fixed_array",
        "@com_google_absl//absl/strings",
    ],
)

cc_test(
    name = "layout-optimizer_test",
    srcs = [
        "layout_optimizer_internal.h",
        "layout_optimizer_test.cc",
    ],
    deps = [
        ":basic-format-style",
        ":layout-optimizer",
        ":token-partition-tree",
        ":token-partition-tree-test-utils",
        ":unwrapped-line",
        ":unwrapped-line-test-utils",
        "//common/strings:split",
        "//common/util:logging",
        "//common/util:spacer",
        "//common/util:tree-operations",
        "//common/util:vector-tree",
        "@com_google_absl//absl/container:fixed_array",
        "@com_google_absl//absl/strings",
        "@com_google_googletest//:gtest",
        "@com_google_googletest//:gtest_main",
    ],
)

cc_library(
    name = "token-partition-tree",
    srcs = ["token_partition_tree.cc"],
    hdrs = [
        "token_partition_tree.h",
    ],
    deps = [
        ":basic-format-style",
        ":format-token",
        ":line-wrap-searcher",
        ":unwrapped-line",
        "//common/strings:display-utils",
        "//common/strings:position",
        "//common/strings:range",
        "//common/text:tree-utils",
        "//common/util:algorithm",
        "//common/util:container-iterator-range",
        "//common/util:iterator-adaptors",
        "//common/util:logging",
        "//common/util:spacer",
        "//common/util:top-n",
        "//common/util:tree-operations",
        "//common/util:vector-tree",
        "@com_google_absl//absl/strings:string_view",
    ],
)

cc_library(
    name = "token-partition-tree-test-utils",
    testonly = 1,
    srcs = ["token_partition_tree_test_utils.cc"],
    hdrs = ["token_partition_tree_test_utils.h"],
    deps = [
        ":format-token",
        ":token-partition-tree",
        ":unwrapped-line",
        "//common/util:logging",
        "//common/util:tree-operations",
        "@com_google_googletest//:gtest",
        "@com_google_googletest//:gtest_main",
    ],
)

cc_test(
    name = "token-partition-tree_test",
    srcs = ["token_partition_tree_test.cc"],
    deps = [
        ":format-token",
        ":token-partition-tree",
        ":token-partition-tree-test-utils",
        ":unwrapped-line",
        ":unwrapped-line-test-utils",
        "//common/util:container-iterator-range",
        "//common/util:logging",
        "//common/util:tree-operations",
        "//common/util:vector-tree",
        "@com_google_absl//absl/strings",
        "@com_google_googletest//:gtest",
        "@com_google_googletest//:gtest_main",
    ],
)

cc_library(
    name = "unwrapped-line",
    srcs = ["unwrapped_line.cc"],
    hdrs = [
        "unwrapped_line.h",
    ],
    deps = [
        ":format-token",
        "//common/strings:display-utils",
        "//common/text:symbol",
        "//common/text:tree-utils",
        "//common/util:container-iterator-range",
        "//common/util:logging",
        "//common/util:spacer",
        "@com_google_absl//absl/strings",
    ],
)

cc_library(
    name = "unwrapped-line-test-utils",
    testonly = 1,
    srcs = ["unwrapped_line_test_utils.cc"],
    hdrs = ["unwrapped_line_test_utils.h"],
    deps = [
        ":format-token",
        ":unwrapped-line",
        "//common/text:token-info",
        "@com_google_absl//absl/strings",
    ],
)

cc_test(
    name = "unwrapped-line_test",
    srcs = ["unwrapped_line_test.cc"],
    deps = [
        ":format-token",
        ":unwrapped-line",
        ":unwrapped-line-test-utils",
        "//common/text:tree-builder-test-util",
        "//common/util:container-iterator-range",
        "@com_google_absl//absl/strings:string_view",
        "@com_google_googletest//:gtest",
        "@com_google_googletest//:gtest_main",
    ],
)

cc_library(
    name = "tree-annotator",
    srcs = ["tree_annotator.cc"],
    hdrs = [
        "tree_annotator.h",
    ],
    deps = [
        ":format-token",
        "//common/text:concrete-syntax-leaf",
        "//common/text:symbol",
        "//common/text:syntax-tree-context",
        "//common/text:token-info",
        "//common/text:token-stream-view",
        "//common/text:tree-context-visitor",
    ],
)

cc_test(
    name = "tree-annotator_test",
    srcs = ["tree_annotator_test.cc"],
    deps = [
        ":format-token",
        ":tree-annotator",
        "//common/text:constants",
        "//common/text:token-info",
        "//common/text:tree-builder-test-util",
        "//common/util:iterator-range",
        "//common/util:logging",
        "@com_google_absl//absl/strings:string_view",
        "@com_google_googletest//:gtest",
        "@com_google_googletest//:gtest_main",
    ],
)

cc_library(
    name = "tree-unwrapper",
    srcs = ["tree_unwrapper.cc"],
    hdrs = [
        "tree_unwrapper.h",
    ],
    deps = [
        ":format-token",
        ":token-partition-tree",
        ":unwrapped-line",
        "//common/text:concrete-syntax-tree",
        "//common/text:symbol",
        "//common/text:syntax-tree-context",
        "//common/text:text-structure",
        "//common/text:token-info",
        "//common/text:token-stream-view",
        "//common/text:tree-context-visitor",
        "//common/util:logging",
        "//common/util:tree-operations",
        "//common/util:value-saver",
        "//common/util:vector-tree",
        "@com_google_absl//absl/strings:string_view",
    ],
)

cc_test(
    name = "tree-unwrapper_test",
    srcs = ["tree_unwrapper_test.cc"],
    deps = [
        ":format-token",
        ":tree-unwrapper",
        ":unwrapped-line",
        "//common/text:concrete-syntax-leaf",
        "//common/text:concrete-syntax-tree",
        "//common/text:text-structure",
        "//common/text:text-structure-test-utils",
        "//common/text:token-info",
        "//common/text:token-stream-view",
        "//common/util:container-iterator-range",
        "//common/util:range",
        "@com_google_absl//absl/strings",
        "@com_google_googletest//:gtest",
        "@com_google_googletest//:gtest_main",
    ],
)

cc_library(
    name = "state-node",
    srcs = ["state_node.cc"],
    hdrs = [
        "state_node.h",
    ],
    deps = [
        ":basic-format-style",
        ":format-token",
        ":unwrapped-line",
        "//common/strings:position",
        "//common/strings:range",
        "//common/text:token-info",
        "//common/util:container-iterator-range",
        "//common/util:iterator-adaptors",
        "//common/util:iterator-range",
        "//common/util:logging",
        "@com_google_absl//absl/strings",
    ],
)

cc_library(
    name = "line-wrap-searcher",
    srcs = ["line_wrap_searcher.cc"],
    hdrs = ["line_wrap_searcher.h"],
    deps = [
        ":basic-format-style",
        ":format-token",
        ":state-node",
        ":unwrapped-line",
        "//common/text:token-info",
        "//common/util:logging",
        "//common/util:spacer",
        "@com_google_absl//absl/strings",
    ],
)

cc_test(
    name = "state-node_test",
    srcs = ["state_node_test.cc"],
    deps = [
        ":basic-format-style",
        ":format-token",
        ":state-node",
        ":unwrapped-line",
        ":unwrapped-line-test-utils",
        "//common/util:logging",
        "@com_google_absl//absl/memory",
        "@com_google_absl//absl/strings",
        "@com_google_googletest//:gtest",
        "@com_google_googletest//:gtest_main",
    ],
)

cc_test(
    name = "line-wrap-searcher_test",
    srcs = ["line_wrap_searcher_test.cc"],
    deps = [
        ":basic-format-style",
        ":format-token",
        ":line-wrap-searcher",
        ":unwrapped-line",
        ":unwrapped-line-test-utils",
        "@com_google_absl//absl/strings",
        "@com_google_googletest//:gtest",
        "@com_google_googletest//:gtest_main",
    ],
)

cc_library(
    name = "verification",
    srcs = ["verification.cc"],
    hdrs = ["verification.h"],
    deps = [
        "//common/strings:diff",
        "//common/strings:position",
        "@com_google_absl//absl/status",
        "@com_google_absl//absl/strings",
    ],
)

cc_test(
    name = "verification_test",
    srcs = ["verification_test.cc"],
    deps = [
        ":verification",
        "@com_google_absl//absl/strings",
        "@com_google_googletest//:gtest",
        "@com_google_googletest//:gtest_main",
    ],
)
