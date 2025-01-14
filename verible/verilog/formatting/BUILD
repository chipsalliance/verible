# This package SystemVerilog-specific code formatting functions.

default_visibility = [
    "//verible/verilog/tools/formatter:__pkg__",
    "//verible/verilog/tools/ls:__pkg__",
]

package(
    default_applicable_licenses = ["//:license"],
    default_visibility = default_visibility,
    features = ["layering_check"],
)

# libraries

cc_library(
    name = "align",
    srcs = ["align.cc"],
    hdrs = ["align.h"],
    deps = [
        ":format-style",
        "//verible/common/formatting:align",
        "//verible/common/formatting:format-token",
        "//verible/common/formatting:token-partition-tree",
        "//verible/common/formatting:unwrapped-line",
        "//verible/common/strings:position",
        "//verible/common/text:concrete-syntax-leaf",
        "//verible/common/text:concrete-syntax-tree",
        "//verible/common/text:symbol",
        "//verible/common/text:token-info",
        "//verible/common/text:tree-context-visitor",
        "//verible/common/text:tree-utils",
        "//verible/common/util:casts",
        "//verible/common/util:logging",
        "//verible/common/util:value-saver",
        "//verible/verilog/CST:context-functions",
        "//verible/verilog/CST:declaration",
        "//verible/verilog/CST:verilog-nonterminals",
        "//verible/verilog/parser:verilog-token-classifications",
        "//verible/verilog/parser:verilog-token-enum",
    ],
)

cc_library(
    name = "tree-unwrapper",
    srcs = ["tree-unwrapper.cc"],
    hdrs = ["tree-unwrapper.h"],
    deps = [
        ":format-style",
        ":verilog-token",
        "//verible/common/formatting:format-token",
        "//verible/common/formatting:token-partition-tree",
        "//verible/common/formatting:tree-unwrapper",
        "//verible/common/formatting:unwrapped-line",
        "//verible/common/strings:display-utils",
        "//verible/common/strings:range",
        "//verible/common/text:concrete-syntax-leaf",
        "//verible/common/text:concrete-syntax-tree",
        "//verible/common/text:constants",
        "//verible/common/text:symbol",
        "//verible/common/text:syntax-tree-context",
        "//verible/common/text:text-structure",
        "//verible/common/text:token-info",
        "//verible/common/text:token-stream-view",
        "//verible/common/text:tree-utils",
        "//verible/common/util:container-iterator-range",
        "//verible/common/util:enum-flags",
        "//verible/common/util:iterator-range",
        "//verible/common/util:logging",
        "//verible/common/util:tree-operations",
        "//verible/verilog/CST:declaration",
        "//verible/verilog/CST:macro",
        "//verible/verilog/CST:statement",
        "//verible/verilog/CST:verilog-nonterminals",
        "//verible/verilog/parser:verilog-parser",  # only for verilog_symbol_name
        "//verible/verilog/parser:verilog-token-classifications",
        "//verible/verilog/parser:verilog-token-enum",
        "@abseil-cpp//absl/base:core_headers",
        "@abseil-cpp//absl/strings",
    ],
)

cc_test(
    name = "tree-unwrapper_test",
    srcs = ["tree-unwrapper_test.cc"],
    deps = [
        ":format-style",
        ":tree-unwrapper",
        "//verible/common/formatting:format-token",
        "//verible/common/formatting:token-partition-tree",
        "//verible/common/formatting:unwrapped-line",
        "//verible/common/text:text-structure",
        "//verible/common/util:container-iterator-range",
        "//verible/common/util:logging",
        "//verible/common/util:spacer",
        "//verible/common/util:tree-operations",
        "//verible/common/util:vector-tree",
        "//verible/verilog/analysis:verilog-analyzer",
        "@abseil-cpp//absl/status",
        "@abseil-cpp//absl/strings",
        "@googletest//:gtest",
        "@googletest//:gtest_main",
    ],
)

cc_library(
    name = "verilog-token",
    srcs = ["verilog-token.cc"],
    hdrs = ["verilog-token.h"],
    deps = [
        "//verible/common/util:container-util",
        "//verible/verilog/parser:verilog-token-enum",
        "@abseil-cpp//absl/container:node_hash_map",
    ],
)

cc_test(
    name = "verilog-token_test",
    srcs = ["verilog-token_test.cc"],
    deps = [
        ":verilog-token",
        "//verible/common/formatting:format-token",
        "//verible/common/text:token-info",
        "//verible/verilog/parser:verilog-token-enum",
        "@googletest//:gtest",
        "@googletest//:gtest_main",
    ],
)

cc_library(
    name = "formatter",
    srcs = ["formatter.cc"],
    hdrs = [
        "formatter.h",
    ],
    deps = [
        ":align",
        ":comment-controls",
        ":format-style",
        ":token-annotator",
        ":tree-unwrapper",
        "//verible/common/formatting:format-token",
        "//verible/common/formatting:layout-optimizer",
        "//verible/common/formatting:line-wrap-searcher",
        "//verible/common/formatting:token-partition-tree",
        "//verible/common/formatting:unwrapped-line",
        "//verible/common/formatting:verification",
        "//verible/common/strings:diff",
        "//verible/common/strings:line-column-map",
        "//verible/common/strings:position",
        "//verible/common/strings:range",
        "//verible/common/text:symbol",
        "//verible/common/text:text-structure",
        "//verible/common/text:token-info",
        "//verible/common/text:tree-utils",
        "//verible/common/util:expandable-tree-view",
        "//verible/common/util:interval",
        "//verible/common/util:interval-set",
        "//verible/common/util:iterator-range",
        "//verible/common/util:logging",
        "//verible/common/util:spacer",
        "//verible/common/util:tree-operations",
        "//verible/common/util:vector-tree",
        "//verible/common/util:vector-tree-iterators",
        "//verible/verilog/CST:declaration",
        "//verible/verilog/CST:verilog-nonterminals",
        "//verible/verilog/analysis:verilog-analyzer",
        "//verible/verilog/analysis:verilog-equivalence",
        "//verible/verilog/parser:verilog-token-enum",
        "//verible/verilog/preprocessor:verilog-preprocess",
        "@abseil-cpp//absl/base:core_headers",
        "@abseil-cpp//absl/status",
        "@abseil-cpp//absl/status:statusor",
        "@abseil-cpp//absl/strings",
    ],
)

cc_test(
    name = "formatter_test",
    srcs = ["formatter_test.cc"],
    deps = [
        ":format-style",
        ":formatter",
        "//verible/common/formatting:align",
        "//verible/common/formatting:basic-format-style",
        "//verible/common/strings:display-utils",
        "//verible/common/strings:position",
        "//verible/common/text:text-structure",
        "//verible/common/util:interval",
        "//verible/common/util:logging",
        "//verible/verilog/analysis:verilog-analyzer",
        "@abseil-cpp//absl/status",
        "@abseil-cpp//absl/strings",
        "@googletest//:gtest",
        "@googletest//:gtest_main",
    ],
)

cc_test(
    name = "formatter-tuning_test",
    srcs = ["formatter-tuning_test.cc"],
    deps = [
        ":format-style",
        ":formatter",
        "//verible/common/strings:position",
        "//verible/common/util:logging",
        "@abseil-cpp//absl/status",
        "@googletest//:gtest",
        "@googletest//:gtest_main",
    ],
)

cc_library(
    name = "comment-controls",
    srcs = ["comment-controls.cc"],
    hdrs = ["comment-controls.h"],
    deps = [
        "//verible/common/strings:comment-utils",
        "//verible/common/strings:display-utils",
        "//verible/common/strings:line-column-map",
        "//verible/common/strings:position",
        "//verible/common/text:token-info",
        "//verible/common/text:token-stream-view",
        "//verible/common/util:logging",
        "//verible/common/util:range",
        "//verible/common/util:spacer",
        "//verible/verilog/parser:verilog-parser",
        "//verible/verilog/parser:verilog-token-classifications",
        "//verible/verilog/parser:verilog-token-enum",
        "@abseil-cpp//absl/strings",
    ],
)

cc_test(
    name = "comment-controls_test",
    srcs = ["comment-controls_test.cc"],
    deps = [
        ":comment-controls",
        "//verible/common/strings:line-column-map",
        "//verible/common/strings:position",
        "//verible/common/text:token-info-test-util",
        "//verible/verilog/analysis:verilog-analyzer",
        "@abseil-cpp//absl/strings",
        "@googletest//:gtest",
        "@googletest//:gtest_main",
    ],
)

cc_library(
    name = "format-style",
    hdrs = ["format-style.h"],
    deps = [
        "//verible/common/formatting:align",
        "//verible/common/formatting:basic-format-style",
    ],
)

cc_library(
    name = "format-style-init",
    srcs = ["format-style-init.cc"],
    hdrs = ["format-style-init.h"],
    deps = [
        ":format-style",
        "//verible/common/formatting:align",
        "//verible/common/formatting:basic-format-style",
        "//verible/common/formatting:basic-format-style-init",
        "@abseil-cpp//absl/flags:flag",
    ],
)

cc_library(
    name = "token-annotator",
    srcs = ["token-annotator.cc"],
    hdrs = ["token-annotator.h"],
    deps = [
        ":format-style",
        ":verilog-token",
        "//verible/common/formatting:format-token",
        "//verible/common/formatting:tree-annotator",
        "//verible/common/strings:range",
        "//verible/common/text:symbol",
        "//verible/common/text:syntax-tree-context",
        "//verible/common/text:text-structure",
        "//verible/common/text:token-info",
        "//verible/common/util:logging",
        "//verible/common/util:with-reason",
        "//verible/verilog/CST:verilog-nonterminals",
        "//verible/verilog/parser:verilog-parser",
        "//verible/verilog/parser:verilog-token-classifications",
        "//verible/verilog/parser:verilog-token-enum",
    ],
)

cc_test(
    name = "token-annotator_test",
    srcs = ["token-annotator_test.cc"],
    deps = [
        ":format-style",
        ":token-annotator",
        ":verilog-token",
        "//verible/common/formatting:format-token",
        "//verible/common/formatting:unwrapped-line",
        "//verible/common/formatting:unwrapped-line-test-utils",
        "//verible/common/text:concrete-syntax-tree",
        "//verible/common/text:symbol-ptr",
        "//verible/common/text:syntax-tree-context",
        "//verible/common/text:token-info",
        "//verible/common/text:token-info-test-util",
        "//verible/common/util:casts",
        "//verible/common/util:iterator-adaptors",
        "//verible/common/util:iterator-range",
        "//verible/common/util:logging",
        "//verible/verilog/CST:verilog-nonterminals",
        "//verible/verilog/parser:verilog-token-enum",
        "@googletest//:gtest",
        "@googletest//:gtest_main",
    ],
)
