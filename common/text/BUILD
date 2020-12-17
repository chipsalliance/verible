# 'text' contains classes for structural representations of text/code,
# such as token streams, syntax trees.

licenses(["notice"])

package(
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
    name = "token_info",
    srcs = ["token_info.cc"],
    hdrs = ["token_info.h"],
    deps = [
        ":constants",
        "//common/strings:rebase",
        "//common/util:iterator_range",
        "//common/util:logging",
        "//common/util:range",
        "@com_google_absl//absl/strings",
    ],
)

cc_library(
    name = "token_info_json",
    srcs = ["token_info_json.cc"],
    hdrs = ["token_info_json.h"],
    deps = [
        ":token_info",
        "@jsoncpp_git//:jsoncpp",
    ],
)


cc_library(
    name = "token_info_test_util",
    testonly = 1,
    srcs = ["token_info_test_util.cc"],
    hdrs = ["token_info_test_util.h"],
    deps = [
        ":token_info",
        "//common/util:logging",
        "@com_google_absl//absl/strings",
    ],
)

cc_library(
    name = "token_stream_view",
    srcs = ["token_stream_view.cc"],
    hdrs = ["token_stream_view.h"],
    deps = [
        ":token_info",
        "//common/util:iterator_range",
    ],
)

cc_library(
    name = "symbol",
    srcs = ["symbol.cc"],
    hdrs = ["symbol.h"],
    deps = [
        ":token_info",
        ":visitors",
    ],
)

cc_library(
    name = "concrete_syntax_leaf",
    srcs = ["concrete_syntax_leaf.cc"],
    hdrs = ["concrete_syntax_leaf.h"],
    deps = [
        ":concrete_syntax_tree",
        ":symbol",
        ":token_info",
        ":tree_compare",
        ":visitors",
        "//common/util:logging",
    ],
)

cc_test(
    name = "concrete_syntax_leaf_test",
    srcs = ["concrete_syntax_leaf_test.cc"],
    deps = [
        ":concrete_syntax_leaf",
        ":token_info",
        "@com_google_googletest//:gtest_main",
    ],
)

cc_library(
    name = "concrete_syntax_tree",
    srcs = ["concrete_syntax_tree.cc"],
    hdrs = ["concrete_syntax_tree.h"],
    deps = [
        ":constants",
        ":symbol",
        ":tree_compare",
        ":visitors",
        "//common/util:casts",
        "//common/util:logging",
    ],
)

cc_library(
    name = "config_utils",
    srcs = ["config_utils.cc"],
    hdrs = ["config_utils.h"],
    deps = [
        "//common/util:logging",
        "@com_google_absl//absl/status",
        "@com_google_absl//absl/strings",
    ],
)

cc_test(
    name = "config_utils_test",
    srcs = ["config_utils_test.cc"],
    deps = [
        ":config_utils",
        "@com_google_absl//absl/status",
        "@com_google_absl//absl/strings",
        "@com_google_googletest//:gtest_main",
    ],
)

cc_library(
    name = "visitors",
    hdrs = ["visitors.h"],
)

cc_library(
    name = "syntax_tree_context",
    hdrs = ["syntax_tree_context.h"],
    deps = [
        ":concrete_syntax_tree",
        "//common/util:auto_pop_stack",
        "//common/util:iterator_adaptors",
        "//common/util:logging",
    ],
)

cc_library(
    name = "tree_compare",
    srcs = ["tree_compare.cc"],
    hdrs = ["tree_compare.h"],
    deps = [
        ":symbol",
        ":token_info",
        "@com_google_absl//absl/strings",
    ],
)

cc_test(
    name = "tree_compare_test",
    srcs = ["tree_compare_test.cc"],
    deps = [
        ":concrete_syntax_tree",
        ":tree_builder_test_util",
        ":tree_compare",
        "@com_google_googletest//:gtest_main",
    ],
)

cc_library(
    name = "tree_context_visitor",
    srcs = ["tree_context_visitor.cc"],
    hdrs = ["tree_context_visitor.h"],
    deps = [
        ":syntax_tree_context",
        ":visitors",
        "//common/strings:display_utils",
        "//common/util:logging",
    ],
)

cc_test(
    name = "tree_context_visitor_test",
    srcs = ["tree_context_visitor_test.cc"],
    deps = [
        ":tree_builder_test_util",
        ":tree_context_visitor",
        "@com_google_googletest//:gtest_main",
    ],
)

cc_library(
    name = "tree_utils",
    srcs = ["tree_utils.cc"],
    hdrs = ["tree_utils.h"],
    deps = [
        ":concrete_syntax_leaf",
        ":concrete_syntax_tree",
        ":symbol",
        ":token_info",
        ":visitors",
        "//common/util:iterator_adaptors",
        "//common/util:logging",
        "//common/util:spacer",
        "//common/util:type_traits",
        "//common/util:value_saver",
        "@com_google_absl//absl/strings",
    ],
)

cc_library(
    name = "tree_builder_test_util",
    testonly = 1,
    srcs = ["tree_builder_test_util.cc"],
    hdrs = ["tree_builder_test_util.h"],
    deps = [
        ":concrete_syntax_leaf",
        ":concrete_syntax_tree",
        ":symbol",
        ":tree_utils",
        "@com_google_absl//absl/strings",
    ],
)

cc_test(
    name = "tree_builder_test_util_test",
    srcs = ["tree_builder_test_util_test.cc"],
    deps = [
        ":tree_builder_test_util",
        ":tree_utils",
        "@com_google_googletest//:gtest_main",
    ],
)

cc_library(
    name = "text_structure",
    srcs = ["text_structure.cc"],
    hdrs = ["text_structure.h"],
    deps = [
        ":concrete_syntax_leaf",
        ":concrete_syntax_tree",
        ":symbol",
        ":token_info",
        ":token_stream_view",
        ":tree_utils",
        "//common/strings:line_column_map",
        "//common/util:iterator_range",
        "//common/util:logging",
        "//common/util:range",
        "//common/util:status_macros",
        "@com_google_absl//absl/status",
        "@com_google_absl//absl/strings",
    ],
)

cc_library(
    name = "text_structure_test_utils",
    testonly = 1,
    srcs = ["text_structure_test_utils.cc"],
    hdrs = ["text_structure_test_utils.h"],
    deps = [
        ":concrete_syntax_tree",
        ":text_structure",
        ":token_info",
        ":token_stream_view",
        ":tree_builder_test_util",
        "//common/util:logging",
        "@com_google_absl//absl/memory",
        "@com_google_absl//absl/strings",
    ],
)

cc_library(
    name = "macro_definition",
    srcs = ["macro_definition.cc"],
    hdrs = ["macro_definition.h"],
    deps = [
        ":token_info",
        "//common/util:container_util",
        "@com_google_absl//absl/status",
        "@com_google_absl//absl/strings",
    ],
)

cc_library(
    name = "parser_verifier",
    srcs = ["parser_verifier.cc"],
    hdrs = ["parser_verifier.h"],
    deps = [
        ":concrete_syntax_leaf",
        ":concrete_syntax_tree",
        ":symbol",
        ":token_info",
        ":token_stream_view",
        ":tree_compare",
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
    name = "tree_utils_test",
    srcs = ["tree_utils_test.cc"],
    deps = [
        ":concrete_syntax_leaf",
        ":concrete_syntax_tree",
        ":symbol",
        ":token_info",
        ":tree_builder_test_util",
        ":tree_compare",
        ":tree_utils",
        "//common/util:logging",
        "//common/util:range",
        "@com_google_googletest//:gtest_main",
    ],
)

cc_test(
    name = "concrete_syntax_tree_test",
    srcs = ["concrete_syntax_tree_test.cc"],
    deps = [
        ":concrete_syntax_leaf",
        ":concrete_syntax_tree",
        ":symbol",
        ":tree_builder_test_util",
        ":tree_compare",
        "//common/util:logging",
        "@com_google_googletest//:gtest_main",
    ],
)

cc_test(
    name = "token_info_test",
    srcs = ["token_info_test.cc"],
    deps = [
        ":constants",
        ":token_info",
        "//common/util:range",
        "@com_google_absl//absl/base:core_headers",
        "@com_google_absl//absl/strings",
        "@com_google_googletest//:gtest_main",
    ],
)

cc_test(
    name = "token_info_json_test",
    srcs = ["token_info_json_test.cc"],
    deps = [
        ":constants",
        ":token_info",
        ":token_info_json",
        "@com_google_absl//absl/strings",
        "@com_google_googletest//:gtest_main",
        "@jsoncpp_git//:jsoncpp",
    ],
)

cc_test(
    name = "token_info_test_util_test",
    srcs = ["token_info_test_util_test.cc"],
    deps = [
        ":token_info",
        ":token_info_test_util",
        "//common/util:logging",
        "@com_google_absl//absl/strings",
        "@com_google_googletest//:gtest_main",
    ],
)

cc_test(
    name = "token_stream_view_test",
    srcs = ["token_stream_view_test.cc"],
    deps = [
        ":text_structure",
        ":text_structure_test_utils",
        ":token_info",
        ":token_stream_view",
        "//common/util:iterator_range",
        "@com_google_googletest//:gtest_main",
    ],
)

cc_test(
    name = "syntax_tree_context_test",
    srcs = ["syntax_tree_context_test.cc"],
    deps = [
        ":concrete_syntax_tree",
        ":syntax_tree_context",
        "//common/util:iterator_range",
        "@com_google_googletest//:gtest_main",
    ],
)

cc_test(
    name = "text_structure_test",
    srcs = ["text_structure_test.cc"],
    deps = [
        ":concrete_syntax_tree",
        ":symbol",
        ":text_structure",
        ":text_structure_test_utils",
        ":token_info",
        ":token_stream_view",
        ":tree_builder_test_util",
        ":tree_compare",
        "//common/strings:line_column_map",
        "//common/util:iterator_range",
        "//common/util:logging",
        "//common/util:range",
        "//common/util:value_saver",
        "@com_google_absl//absl/memory",
        "@com_google_absl//absl/status",
        "@com_google_absl//absl/strings",
        "@com_google_googletest//:gtest_main",
    ],
)

cc_test(
    name = "macro_definition_test",
    srcs = ["macro_definition_test.cc"],
    deps = [
        ":macro_definition",
        ":token_info",
        "//common/util:container_util",
        "@com_google_absl//absl/status",
        "@com_google_googletest//:gtest_main",
    ],
)

cc_test(
    name = "parser_verifier_test",
    srcs = ["parser_verifier_test.cc"],
    deps = [
        ":constants",
        ":parser_verifier",
        ":token_info",
        ":token_stream_view",
        ":tree_builder_test_util",
        "@com_google_absl//absl/strings",
        "@com_google_googletest//:gtest_main",
    ],
)
