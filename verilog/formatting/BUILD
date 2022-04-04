# This package SystemVerilog-specific code formatting functions.

licenses(["notice"])

package(
    default_visibility = [
        "//verilog/tools/formatter:__pkg__",
    ],
)

# libraries

cc_library(
    name = "align",
    srcs = ["align.cc"],
    hdrs = ["align.h"],
    deps = [
        ":format_style",
        "//common/formatting:align",
        "//common/formatting:format_token",
        "//common/formatting:token_partition_tree",
        "//common/formatting:unwrapped_line",
        "//common/strings:position",
        "//common/text:concrete_syntax_leaf",
        "//common/text:concrete_syntax_tree",
        "//common/text:symbol",
        "//common/text:token_info",
        "//common/text:tree_utils",
        "//common/util:casts",
        "//common/util:logging",
        "//common/util:value_saver",
        "//verilog/CST:context_functions",
        "//verilog/CST:declaration",
        "//verilog/CST:verilog_nonterminals",
        "//verilog/parser:verilog_token_classifications",
        "//verilog/parser:verilog_token_enum",
        "@com_google_absl//absl/strings",
    ],
)

cc_library(
    name = "tree_unwrapper",
    srcs = ["tree_unwrapper.cc"],
    hdrs = ["tree_unwrapper.h"],
    deps = [
        ":format_style",
        ":verilog_token",
        "//common/formatting:basic_format_style",
        "//common/formatting:format_token",
        "//common/formatting:token_partition_tree",
        "//common/formatting:tree_unwrapper",
        "//common/formatting:unwrapped_line",
        "//common/strings:range",
        "//common/text:concrete_syntax_leaf",
        "//common/text:concrete_syntax_tree",
        "//common/text:constants",
        "//common/text:symbol",
        "//common/text:syntax_tree_context",
        "//common/text:text_structure",
        "//common/text:token_info",
        "//common/text:token_stream_view",
        "//common/text:tree_utils",
        "//common/util:container_iterator_range",
        "//common/util:enum_flags",
        "//common/util:logging",
        "//common/util:tree_operations",
        "//common/util:value_saver",
        "//common/util:vector_tree_iterators",
        "//verilog/CST:declaration",
        "//verilog/CST:functions",
        "//verilog/CST:macro",
        "//verilog/CST:statement",
        "//verilog/CST:verilog_nonterminals",
        "//verilog/parser:verilog_parser",  # only for verilog_symbol_name
        "//verilog/parser:verilog_token_classifications",
        "//verilog/parser:verilog_token_enum",
        "@com_google_absl//absl/base:core_headers",
        "@com_google_absl//absl/container:inlined_vector",
        "@com_google_absl//absl/strings",
    ],
)

cc_test(
    name = "tree_unwrapper_test",
    srcs = ["tree_unwrapper_test.cc"],
    deps = [
        ":tree_unwrapper",
        "//common/formatting:basic_format_style",
        "//common/formatting:format_token",
        "//common/formatting:token_partition_tree",
        "//common/formatting:unwrapped_line",
        "//common/text:text_structure",
        "//common/util:container_iterator_range",
        "//common/util:logging",
        "//common/util:spacer",
        "//common/util:tree_operations",
        "//common/util:vector_tree",
        "//verilog/analysis:verilog_analyzer",
        "@com_google_absl//absl/memory",
        "@com_google_absl//absl/status",
        "@com_google_absl//absl/strings",
        "@com_google_googletest//:gtest_main",
    ],
)

cc_library(
    name = "verilog_token",
    srcs = ["verilog_token.cc"],
    hdrs = ["verilog_token.h"],
    deps = [
        "//common/util:container_util",
        "//verilog/parser:verilog_token_enum",
        "@com_google_absl//absl/container:node_hash_map",
    ],
)

cc_test(
    name = "verilog_token_test",
    srcs = ["verilog_token_test.cc"],
    deps = [
        ":verilog_token",
        "//common/formatting:format_token",
        "//common/text:token_info",
        "//verilog/parser:verilog_token_enum",
        "@com_google_googletest//:gtest_main",
    ],
)

cc_library(
    name = "formatter",
    srcs = ["formatter.cc"],
    hdrs = [
        "formatter.h",
    ],
    visibility = ["//visibility:public"],
    deps = [
        ":align",
        ":comment_controls",
        ":format_style",
        ":token_annotator",
        ":tree_unwrapper",
        "//common/formatting:format_token",
        "//common/formatting:layout_optimizer",
        "//common/formatting:line_wrap_searcher",
        "//common/formatting:token_partition_tree",
        "//common/formatting:unwrapped_line",
        "//common/formatting:verification",
        "//common/strings:diff",
        "//common/strings:line_column_map",
        "//common/strings:position",
        "//common/strings:range",
        "//common/text:text_structure",
        "//common/text:token_info",
        "//common/text:tree_utils",
        "//common/util:expandable_tree_view",
        "//common/util:interval",
        "//common/util:iterator_range",
        "//common/util:logging",
        "//common/util:range",
        "//common/util:spacer",
        "//common/util:tree_operations",
        "//common/util:vector_tree",
        "//common/util:vector_tree_iterators",
        "//verilog/CST:declaration",
        "//verilog/CST:module",
        "//verilog/analysis:verilog_analyzer",
        "//verilog/analysis:verilog_equivalence",
        "//verilog/parser:verilog_token_enum",
        "//verilog/preprocessor:verilog_preprocess",
        "@com_google_absl//absl/status",
        "@com_google_absl//absl/status:statusor",
    ],
)

cc_test(
    name = "formatter_test",
    srcs = ["formatter_test.cc"],
    deps = [
        ":format_style",
        ":formatter",
        "//common/formatting:align",
        "//common/strings:position",
        "//common/text:text_structure",
        "//common/util:logging",
        "//verilog/analysis:verilog_analyzer",
        "@com_google_absl//absl/status",
        "@com_google_absl//absl/strings",
        "@com_google_googletest//:gtest_main",
    ],
)

cc_test(
    name = "formatter_tuning_test",
    srcs = ["formatter_tuning_test.cc"],
    deps = [
        ":format_style",
        ":formatter",
        "//common/strings:position",
        "//common/util:logging",
        "@com_google_absl//absl/status",
        "@com_google_absl//absl/strings",
        "@com_google_googletest//:gtest_main",
    ],
)

cc_library(
    name = "comment_controls",
    srcs = ["comment_controls.cc"],
    hdrs = ["comment_controls.h"],
    deps = [
        "//common/strings:comment_utils",
        "//common/strings:display_utils",
        "//common/strings:line_column_map",
        "//common/strings:position",
        "//common/text:token_stream_view",
        "//common/util:interval_set",
        "//common/util:logging",
        "//common/util:range",
        "//common/util:spacer",
        "//verilog/parser:verilog_parser",
        "//verilog/parser:verilog_token_classifications",
        "//verilog/parser:verilog_token_enum",
        "@com_google_absl//absl/strings",
    ],
)

cc_test(
    name = "comment_controls_test",
    srcs = ["comment_controls_test.cc"],
    deps = [
        ":comment_controls",
        "//common/strings:line_column_map",
        "//common/text:token_info_test_util",
        "//verilog/analysis:verilog_analyzer",
        "@com_google_absl//absl/strings",
        "@com_google_googletest//:gtest_main",
    ],
)

cc_library(
    name = "format_style",
    hdrs = ["format_style.h"],
    visibility = ["//visibility:public"],
    deps = [
        "//common/formatting:align",
        "//common/formatting:basic_format_style",
        "@com_google_absl//absl/strings",
    ],
)

cc_library(
    name = "format_style_init",
    srcs = ["format_style_init.cc"],
    hdrs = ["format_style_init.h"],
    visibility = ["//visibility:public"],
    deps = [
        ":format_style",
        "//common/formatting:basic_format_style_init",
        "@com_google_absl//absl/flags:flag",
    ],
)

cc_library(
    name = "token_annotator",
    srcs = ["token_annotator.cc"],
    hdrs = ["token_annotator.h"],
    deps = [
        ":format_style",
        ":verilog_token",
        "//common/formatting:format_token",
        "//common/formatting:tree_annotator",
        "//common/strings:range",
        "//common/text:symbol",
        "//common/text:syntax_tree_context",
        "//common/text:text_structure",
        "//common/text:token_info",
        "//common/util:iterator_range",
        "//common/util:logging",
        "//common/util:with_reason",
        "//verilog/CST:verilog_nonterminals",
        "//verilog/parser:verilog_parser",
        "//verilog/parser:verilog_token_classifications",
        "//verilog/parser:verilog_token_enum",
        "@com_google_absl//absl/strings",
    ],
)

cc_test(
    name = "token_annotator_test",
    srcs = ["token_annotator_test.cc"],
    deps = [
        ":format_style",
        ":token_annotator",
        ":verilog_token",
        "//common/formatting:format_token",
        "//common/formatting:unwrapped_line",
        "//common/formatting:unwrapped_line_test_utils",
        "//common/text:syntax_tree_context",
        "//common/text:token_info_test_util",
        "//common/text:tree_builder_test_util",
        "//common/util:casts",
        "//common/util:iterator_adaptors",
        "//verilog/CST:verilog_nonterminals",
        "//verilog/parser:verilog_parser",
        "//verilog/parser:verilog_token_enum",
        "@com_google_absl//absl/strings",
        "@com_google_googletest//:gtest_main",
    ],
)
