# 'formatting' contains functionality for language-agnostic formatting

package(
    default_applicable_licenses = ["//:license"],
    default_visibility = [
        "//verible/verilog/formatting:__subpackages__",
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
        "//verible/common/strings:display-utils",
        "//verible/common/strings:position",
        "//verible/common/text:concrete-syntax-leaf",
        "//verible/common/text:concrete-syntax-tree",
        "//verible/common/text:symbol",
        "//verible/common/text:token-info",
        "//verible/common/text:tree-context-visitor",
        "//verible/common/text:tree-utils",
        "//verible/common/util:container-iterator-range",
        "//verible/common/util:enum-flags",
        "//verible/common/util:iterator-adaptors",
        "//verible/common/util:iterator-range",
        "//verible/common/util:logging",
        "//verible/common/util:range",
        "//verible/common/util:tree-operations",
        "//verible/common/util:vector-tree",
        "//verible/common/util:vector-tree-iterators",
        "@abseil-cpp//absl/strings",
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
        ":unwrapped-line",
        ":unwrapped-line-test-utils",
        "//verible/common/strings:position",
        "//verible/common/text:concrete-syntax-leaf",
        "//verible/common/text:concrete-syntax-tree",
        "//verible/common/text:symbol-ptr",
        "//verible/common/text:token-info",
        "//verible/common/text:tree-builder-test-util",
        "//verible/common/text:tree-utils",
        "//verible/common/util:logging",
        "//verible/common/util:range",
        "//verible/common/util:spacer",
        "//verible/common/util:value-saver",
        "@abseil-cpp//absl/strings",
        "@googletest//:gtest",
        "@googletest//:gtest_main",
    ],
)

cc_library(
    name = "basic-format-style",
    srcs = ["basic-format-style.cc"],
    hdrs = ["basic-format-style.h"],
    deps = ["//verible/common/util:enum-flags"],
)

cc_library(
    name = "basic-format-style-init",
    srcs = ["basic-format-style-init.cc"],
    hdrs = ["basic-format-style-init.h"],
    deps = [
        ":basic-format-style",
        "@abseil-cpp//absl/flags:flag",
    ],
)

cc_test(
    name = "basic-format-style_test",
    srcs = ["basic-format-style_test.cc"],
    deps = [
        ":basic-format-style",
        "@googletest//:gtest",
        "@googletest//:gtest_main",
    ],
)

cc_library(
    name = "format-token",
    srcs = ["format-token.cc"],
    hdrs = ["format-token.h"],
    deps = [
        "//verible/common/strings:display-utils",
        "//verible/common/strings:position",
        "//verible/common/strings:range",
        "//verible/common/text:concrete-syntax-leaf",
        "//verible/common/text:token-info",
        "//verible/common/util:container-iterator-range",
        "//verible/common/util:interval",
        "//verible/common/util:iterator-range",
        "//verible/common/util:logging",
        "//verible/common/util:range",
        "//verible/common/util:spacer",
        "@abseil-cpp//absl/base:core_headers",
        "@abseil-cpp//absl/strings",
    ],
)

cc_test(
    name = "format-token_test",
    srcs = ["format-token_test.cc"],
    deps = [
        ":format-token",
        ":unwrapped-line-test-utils",
        "//verible/common/strings:position",
        "//verible/common/text:token-info",
        "//verible/common/util:range",
        "@googletest//:gtest",
        "@googletest//:gtest_main",
    ],
)

cc_library(
    name = "layout-optimizer",
    srcs = [
        "layout-optimizer.cc",
        "layout-optimizer-internal.h",
    ],
    hdrs = ["layout-optimizer.h"],
    # Can only add @abseil-cpp//absl/log:vlog_is_on after > absl 20240116
    # https://github.com/chipsalliance/verible/issues/2336
    features = ["-layering_check"],
    deps = [
        ":basic-format-style",
        ":format-token",
        ":token-partition-tree",
        ":unwrapped-line",
        "//verible/common/util:container-iterator-range",
        "//verible/common/util:iterator-range",
        "//verible/common/util:logging",
        "//verible/common/util:tree-operations",
        "//verible/common/util:value-saver",
        "//verible/common/util:vector-tree",
        "@abseil-cpp//absl/base:config",
        "@abseil-cpp//absl/container:fixed_array",
        "@abseil-cpp//absl/log",
        "@abseil-cpp//absl/strings",
    ],
)

cc_test(
    name = "layout-optimizer_test",
    srcs = [
        "layout-optimizer-internal.h",
        "layout-optimizer_test.cc",
    ],
    deps = [
        ":basic-format-style",
        ":format-token",
        ":layout-optimizer",
        ":token-partition-tree",
        ":token-partition-tree-test-utils",
        ":unwrapped-line",
        ":unwrapped-line-test-utils",
        "//verible/common/strings:split",
        "//verible/common/text:token-info",
        "//verible/common/util:iterator-range",
        "//verible/common/util:logging",
        "//verible/common/util:spacer",
        "//verible/common/util:tree-operations",
        "//verible/common/util:vector-tree",
        "@abseil-cpp//absl/container:fixed_array",
        "@abseil-cpp//absl/strings",
        "@googletest//:gtest",
        "@googletest//:gtest_main",
    ],
)

cc_library(
    name = "token-partition-tree",
    srcs = ["token-partition-tree.cc"],
    hdrs = [
        "token-partition-tree.h",
    ],
    deps = [
        ":basic-format-style",
        ":format-token",
        ":line-wrap-searcher",
        ":unwrapped-line",
        "//verible/common/strings:display-utils",
        "//verible/common/strings:position",
        "//verible/common/strings:range",
        "//verible/common/util:algorithm",
        "//verible/common/util:container-iterator-range",
        "//verible/common/util:iterator-adaptors",
        "//verible/common/util:iterator-range",
        "//verible/common/util:logging",
        "//verible/common/util:spacer",
        "//verible/common/util:top-n",
        "//verible/common/util:tree-operations",
        "//verible/common/util:vector-tree",
    ],
)

cc_library(
    name = "token-partition-tree-test-utils",
    testonly = 1,
    srcs = ["token-partition-tree-test-utils.cc"],
    hdrs = ["token-partition-tree-test-utils.h"],
    deps = [
        ":format-token",
        ":token-partition-tree",
        ":unwrapped-line",
        "//verible/common/util:logging",
        "//verible/common/util:tree-operations",
        "@googletest//:gtest",
        "@googletest//:gtest_main",
    ],
)

cc_test(
    name = "token-partition-tree_test",
    srcs = ["token-partition-tree_test.cc"],
    deps = [
        ":basic-format-style",
        ":format-token",
        ":token-partition-tree",
        ":token-partition-tree-test-utils",
        ":unwrapped-line",
        ":unwrapped-line-test-utils",
        "//verible/common/strings:position",
        "//verible/common/text:token-info",
        "//verible/common/util:container-iterator-range",
        "//verible/common/util:logging",
        "//verible/common/util:tree-operations",
        "//verible/common/util:vector-tree",
        "@abseil-cpp//absl/strings",
        "@googletest//:gtest",
        "@googletest//:gtest_main",
    ],
)

cc_library(
    name = "unwrapped-line",
    srcs = ["unwrapped-line.cc"],
    hdrs = [
        "unwrapped-line.h",
    ],
    deps = [
        ":format-token",
        "//verible/common/strings:display-utils",
        "//verible/common/text:symbol",
        "//verible/common/text:token-info",
        "//verible/common/text:tree-utils",
        "//verible/common/util:container-iterator-range",
        "//verible/common/util:iterator-range",
        "//verible/common/util:logging",
        "//verible/common/util:spacer",
        "@abseil-cpp//absl/strings",
    ],
)

cc_library(
    name = "unwrapped-line-test-utils",
    testonly = 1,
    srcs = ["unwrapped-line-test-utils.cc"],
    hdrs = ["unwrapped-line-test-utils.h"],
    deps = [
        ":format-token",
        ":unwrapped-line",
        "//verible/common/text:constants",
        "//verible/common/text:token-info",
    ],
)

cc_test(
    name = "unwrapped-line_test",
    srcs = ["unwrapped-line_test.cc"],
    deps = [
        ":format-token",
        ":unwrapped-line",
        ":unwrapped-line-test-utils",
        "//verible/common/text:symbol",
        "//verible/common/text:token-info",
        "//verible/common/text:tree-builder-test-util",
        "//verible/common/util:container-iterator-range",
        "@googletest//:gtest",
        "@googletest//:gtest_main",
    ],
)

cc_library(
    name = "tree-annotator",
    srcs = ["tree-annotator.cc"],
    hdrs = [
        "tree-annotator.h",
    ],
    deps = [
        ":format-token",
        "//verible/common/text:concrete-syntax-leaf",
        "//verible/common/text:symbol",
        "//verible/common/text:syntax-tree-context",
        "//verible/common/text:token-info",
        "//verible/common/text:tree-context-visitor",
    ],
)

cc_test(
    name = "tree-annotator_test",
    srcs = ["tree-annotator_test.cc"],
    deps = [
        ":format-token",
        ":tree-annotator",
        "//verible/common/text:constants",
        "//verible/common/text:syntax-tree-context",
        "//verible/common/text:token-info",
        "//verible/common/text:tree-builder-test-util",
        "//verible/common/util:iterator-range",
        "//verible/common/util:logging",
        "@googletest//:gtest",
        "@googletest//:gtest_main",
    ],
)

cc_library(
    name = "tree-unwrapper",
    srcs = ["tree-unwrapper.cc"],
    hdrs = [
        "tree-unwrapper.h",
    ],
    deps = [
        ":format-token",
        ":token-partition-tree",
        ":unwrapped-line",
        "//verible/common/text:concrete-syntax-tree",
        "//verible/common/text:symbol",
        "//verible/common/text:syntax-tree-context",
        "//verible/common/text:text-structure",
        "//verible/common/text:token-info",
        "//verible/common/text:token-stream-view",
        "//verible/common/text:tree-context-visitor",
        "//verible/common/util:logging",
        "//verible/common/util:tree-operations",
        "//verible/common/util:value-saver",
        "//verible/common/util:vector-tree",
    ],
)

cc_test(
    name = "tree-unwrapper_test",
    srcs = ["tree-unwrapper_test.cc"],
    deps = [
        ":format-token",
        ":tree-unwrapper",
        ":unwrapped-line",
        "//verible/common/text:concrete-syntax-leaf",
        "//verible/common/text:concrete-syntax-tree",
        "//verible/common/text:text-structure",
        "//verible/common/text:text-structure-test-utils",
        "//verible/common/text:token-info",
        "//verible/common/text:token-stream-view",
        "//verible/common/util:container-iterator-range",
        "//verible/common/util:range",
        "@abseil-cpp//absl/strings",
        "@googletest//:gtest",
        "@googletest//:gtest_main",
    ],
)

cc_library(
    name = "state-node",
    srcs = ["state-node.cc"],
    hdrs = [
        "state-node.h",
    ],
    deps = [
        ":basic-format-style",
        ":format-token",
        ":unwrapped-line",
        "//verible/common/strings:position",
        "//verible/common/text:token-info",
        "//verible/common/util:container-iterator-range",
        "//verible/common/util:iterator-adaptors",
        "//verible/common/util:iterator-range",
        "//verible/common/util:logging",
    ],
)

cc_library(
    name = "line-wrap-searcher",
    srcs = ["line-wrap-searcher.cc"],
    hdrs = ["line-wrap-searcher.h"],
    deps = [
        ":basic-format-style",
        ":format-token",
        ":state-node",
        ":unwrapped-line",
        "//verible/common/text:token-info",
        "//verible/common/util:logging",
        "//verible/common/util:spacer",
    ],
)

cc_test(
    name = "state-node_test",
    srcs = ["state-node_test.cc"],
    deps = [
        ":basic-format-style",
        ":format-token",
        ":state-node",
        ":unwrapped-line",
        ":unwrapped-line-test-utils",
        "//verible/common/text:token-info",
        "//verible/common/util:logging",
        "@googletest//:gtest",
        "@googletest//:gtest_main",
    ],
)

cc_test(
    name = "line-wrap-searcher_test",
    srcs = ["line-wrap-searcher_test.cc"],
    deps = [
        ":basic-format-style",
        ":format-token",
        ":line-wrap-searcher",
        ":unwrapped-line",
        ":unwrapped-line-test-utils",
        "//verible/common/text:token-info",
        "@abseil-cpp//absl/strings",
        "@googletest//:gtest",
        "@googletest//:gtest_main",
    ],
)

cc_library(
    name = "verification",
    srcs = ["verification.cc"],
    hdrs = ["verification.h"],
    deps = [
        "//verible/common/strings:diff",
        "//verible/common/strings:position",
        "@abseil-cpp//absl/status",
        "@abseil-cpp//absl/strings",
    ],
)

cc_test(
    name = "verification_test",
    srcs = ["verification_test.cc"],
    deps = [
        ":verification",
        "//verible/common/strings:position",
        "@abseil-cpp//absl/status",
        "@abseil-cpp//absl/strings",
        "@googletest//:gtest",
        "@googletest//:gtest_main",
    ],
)
