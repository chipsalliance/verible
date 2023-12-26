# This package SystemVerilog-specific code formatting functions.

default_visibility = [
    "//verilog/tools/formatter:__pkg__",
    "//verilog/tools/ls:__pkg__",
]

package(
    default_applicable_licenses = ["//:license"],
    default_visibility = default_visibility,
)

# libraries

cc_library(
    name = "align",
    srcs = ["align.cc"],
    hdrs = ["align.h"],
    deps = [
        ":format-style",
        "//common/formatting:align",
        "//common/formatting:format-token",
        "//common/formatting:token-partition-tree",
        "//common/formatting:unwrapped-line",
        "//common/strings:position",
        "//common/text:concrete-syntax-leaf",
        "//common/text:concrete-syntax-tree",
        "//common/text:symbol",
        "//common/text:token-info",
        "//common/text:tree-utils",
        "//common/util:casts",
        "//common/util:logging",
        "//common/util:value-saver",
        "//verilog/CST:context-functions",
        "//verilog/CST:declaration",
        "//verilog/CST:verilog-nonterminals",
        "//verilog/parser:verilog-token-classifications",
        "//verilog/parser:verilog-token-enum",
        "@com_google_absl//absl/strings",
    ],
)

cc_library(
    name = "tree-unwrapper",
    srcs = ["tree_unwrapper.cc"],
    hdrs = ["tree_unwrapper.h"],
    deps = [
        ":format-style",
        ":verilog-token",
        "//common/formatting:basic-format-style",
        "//common/formatting:format-token",
        "//common/formatting:token-partition-tree",
        "//common/formatting:tree-unwrapper",
        "//common/formatting:unwrapped-line",
        "//common/strings:range",
        "//common/text:concrete-syntax-leaf",
        "//common/text:concrete-syntax-tree",
        "//common/text:constants",
        "//common/text:symbol",
        "//common/text:syntax-tree-context",
        "//common/text:text-structure",
        "//common/text:token-info",
        "//common/text:token-stream-view",
        "//common/text:tree-utils",
        "//common/util:container-iterator-range",
        "//common/util:enum-flags",
        "//common/util:logging",
        "//common/util:tree-operations",
        "//common/util:value-saver",
        "//common/util:vector-tree-iterators",
        "//verilog/CST:declaration",
        "//verilog/CST:functions",
        "//verilog/CST:macro",
        "//verilog/CST:statement",
        "//verilog/CST:verilog-nonterminals",
        "//verilog/parser:verilog-parser",  # only for verilog_symbol_name
        "//verilog/parser:verilog-token-classifications",
        "//verilog/parser:verilog-token-enum",
        "@com_google_absl//absl/base:core_headers",
        "@com_google_absl//absl/container:inlined_vector",
        "@com_google_absl//absl/strings",
    ],
)

cc_test(
    name = "tree-unwrapper_test",
    srcs = ["tree_unwrapper_test.cc"],
    deps = [
        ":tree-unwrapper",
        "//common/formatting:basic-format-style",
        "//common/formatting:format-token",
        "//common/formatting:token-partition-tree",
        "//common/formatting:unwrapped-line",
        "//common/text:text-structure",
        "//common/util:container-iterator-range",
        "//common/util:logging",
        "//common/util:spacer",
        "//common/util:tree-operations",
        "//common/util:vector-tree",
        "//verilog/analysis:verilog-analyzer",
        "@com_google_absl//absl/status",
        "@com_google_absl//absl/strings",
        "@com_google_googletest//:gtest_main",
    ],
)

cc_library(
    name = "verilog-token",
    srcs = ["verilog_token.cc"],
    hdrs = ["verilog_token.h"],
    deps = [
        "//common/util:container-util",
        "//verilog/parser:verilog-token-enum",
        "@com_google_absl//absl/container:node_hash_map",
    ],
)

cc_test(
    name = "verilog-token_test",
    srcs = ["verilog_token_test.cc"],
    deps = [
        ":verilog-token",
        "//common/formatting:format-token",
        "//common/text:token-info",
        "//verilog/parser:verilog-token-enum",
        "@com_google_googletest//:gtest_main",
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
        "//common/formatting:format-token",
        "//common/formatting:layout-optimizer",
        "//common/formatting:line-wrap-searcher",
        "//common/formatting:token-partition-tree",
        "//common/formatting:unwrapped-line",
        "//common/formatting:verification",
        "//common/strings:diff",
        "//common/strings:line-column-map",
        "//common/strings:position",
        "//common/strings:range",
        "//common/text:text-structure",
        "//common/text:token-info",
        "//common/text:tree-utils",
        "//common/util:expandable-tree-view",
        "//common/util:interval",
        "//common/util:iterator-range",
        "//common/util:logging",
        "//common/util:range",
        "//common/util:spacer",
        "//common/util:tree-operations",
        "//common/util:vector-tree",
        "//common/util:vector-tree-iterators",
        "//verilog/CST:declaration",
        "//verilog/CST:module",
        "//verilog/analysis:flow-tree",
        "//verilog/analysis:verilog-analyzer",
        "//verilog/analysis:verilog-equivalence",
        "//verilog/parser:verilog-token-enum",
        "//verilog/preprocessor:verilog-preprocess",
        "@com_google_absl//absl/status",
        "@com_google_absl//absl/status:statusor",
    ],
)

cc_test(
    name = "formatter_test",
    srcs = ["formatter_test.cc"],
    deps = [
        ":format-style",
        ":formatter",
        "//common/formatting:align",
        "//common/strings:display-utils",
        "//common/strings:position",
        "//common/text:text-structure",
        "//common/util:logging",
        "//verilog/analysis:verilog-analyzer",
        "@com_google_absl//absl/status",
        "@com_google_absl//absl/strings",
        "@com_google_googletest//:gtest_main",
    ],
)

cc_test(
    name = "formatter-tuning_test",
    srcs = ["formatter_tuning_test.cc"],
    deps = [
        ":format-style",
        ":formatter",
        "//common/strings:position",
        "//common/util:logging",
        "@com_google_absl//absl/status",
        "@com_google_absl//absl/strings",
        "@com_google_googletest//:gtest_main",
    ],
)

cc_library(
    name = "comment-controls",
    srcs = ["comment_controls.cc"],
    hdrs = ["comment_controls.h"],
    deps = [
        "//common/strings:comment-utils",
        "//common/strings:display-utils",
        "//common/strings:line-column-map",
        "//common/strings:position",
        "//common/text:token-stream-view",
        "//common/util:interval-set",
        "//common/util:logging",
        "//common/util:range",
        "//common/util:spacer",
        "//verilog/parser:verilog-parser",
        "//verilog/parser:verilog-token-classifications",
        "//verilog/parser:verilog-token-enum",
        "@com_google_absl//absl/strings",
    ],
)

cc_test(
    name = "comment-controls_test",
    srcs = ["comment_controls_test.cc"],
    deps = [
        ":comment-controls",
        "//common/strings:line-column-map",
        "//common/text:token-info-test-util",
        "//verilog/analysis:verilog-analyzer",
        "@com_google_absl//absl/strings",
        "@com_google_googletest//:gtest_main",
    ],
)

cc_library(
    name = "format-style",
    hdrs = ["format_style.h"],
    deps = [
        "//common/formatting:align",
        "//common/formatting:basic-format-style",
        "@com_google_absl//absl/strings",
    ],
)

cc_library(
    name = "format-style-init",
    srcs = ["format_style_init.cc"],
    hdrs = ["format_style_init.h"],
    deps = [
        ":format-style",
        "//common/formatting:basic-format-style-init",
        "@com_google_absl//absl/flags:flag",
    ],
)

cc_library(
    name = "token-annotator",
    srcs = ["token_annotator.cc"],
    hdrs = ["token_annotator.h"],
    deps = [
        ":format-style",
        ":verilog-token",
        "//common/formatting:format-token",
        "//common/formatting:tree-annotator",
        "//common/strings:range",
        "//common/text:symbol",
        "//common/text:syntax-tree-context",
        "//common/text:text-structure",
        "//common/text:token-info",
        "//common/util:iterator-range",
        "//common/util:logging",
        "//common/util:with-reason",
        "//verilog/CST:verilog-nonterminals",
        "//verilog/parser:verilog-parser",
        "//verilog/parser:verilog-token-classifications",
        "//verilog/parser:verilog-token-enum",
        "@com_google_absl//absl/strings",
    ],
)

cc_test(
    name = "token-annotator_test",
    srcs = ["token_annotator_test.cc"],
    deps = [
        ":format-style",
        ":token-annotator",
        ":verilog-token",
        "//common/formatting:format-token",
        "//common/formatting:unwrapped-line",
        "//common/formatting:unwrapped-line-test-utils",
        "//common/text:syntax-tree-context",
        "//common/text:token-info-test-util",
        "//common/text:tree-builder-test-util",
        "//common/util:casts",
        "//common/util:iterator-adaptors",
        "//verilog/CST:verilog-nonterminals",
        "//verilog/parser:verilog-parser",
        "//verilog/parser:verilog-token-enum",
        "@com_google_absl//absl/strings",
        "@com_google_googletest//:gtest_main",
    ],
)
