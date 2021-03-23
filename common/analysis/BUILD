# This package contains functions and classes for analyzing text structures.

load("//bazel:flex.bzl", "genlex")

licenses(["notice"])

package(
    default_visibility = [
        "//verilog/CST:__subpackages__",
        "//verilog/analysis:__subpackages__",
        "//verilog/tools/kythe:__pkg__",
    ],
)

cc_library(
    name = "citation",
    srcs = ["citation.cc"],
    hdrs = ["citation.h"],
    deps = [
        "@com_google_absl//absl/strings",
    ],
)

cc_library(
    name = "lint_rule_status",
    srcs = ["lint_rule_status.cc"],
    hdrs = ["lint_rule_status.h"],
    deps = [
        "//common/strings:line_column_map",
        "//common/text:concrete_syntax_leaf",
        "//common/text:symbol",
        "//common/text:syntax_tree_context",
        "//common/text:token_info",
        "//common/text:tree_utils",
        "//common/util:spacer",
        "@com_google_absl//absl/strings",
    ],
)

cc_library(
    name = "lint_rule",
    hdrs = ["lint_rule.h"],
    deps = [
        ":lint_rule_status",
        "@com_google_absl//absl/status",
        "@com_google_absl//absl/strings",
    ],
)

genlex(
    name = "command_file_lex",
    src = "command_file.lex",
    out = "command_file.yy.cc",
)

cc_library(
    name = "command_file_lexer",
    srcs = [
        "command_file.yy.cc",
        "command_file_lexer.cc",
    ],
    hdrs = [
        "command_file_lexer.h",
        "lint_waiver.h",
    ],
    copts = ["-Wno-implicit-fallthrough"],
    deps = [
        "//bazel:flex",
        "//common/lexer:flex_lexer_adapter",
        "//common/lexer:token_stream_adapter",
        "//common/strings:position",
        "//common/text:text_structure",
        "//common/text:token_info",
        "//common/text:token_stream_view",
        "//common/util:container_util",
        "//common/util:interval_set",
        "//common/util:logging",
        "@com_google_absl//absl/strings",
    ],
)

cc_test(
    name = "command_file_lexer_test",
    size = "small",
    srcs = ["command_file_lexer_test.cc"],
    deps = [
        ":command_file_lexer",
        "//common/lexer:lexer_test_util",
        "//common/text:token_info",
        "@com_google_absl//absl/strings",
        "@com_google_googletest//:gtest_main",
    ],
)

cc_library(
    name = "lint_waiver",
    srcs = ["lint_waiver.cc"],
    hdrs = ["lint_waiver.h"],
    copts = ["-fexceptions"],
    features = ["-use_header_modules"],  # precompiled headers incompatible with -fexceptions.
    deps = [
        ":command_file_lexer",
        "//common/strings:comment_utils",
        "//common/strings:line_column_map",
        "//common/strings:position",
        "//common/text:text_structure",
        "//common/text:token_info",
        "//common/text:token_stream_view",
        "//common/util:container_iterator_range",
        "//common/util:container_util",
        "//common/util:file_util",
        "//common/util:interval_set",
        "//common/util:iterator_range",
        "//common/util:logging",
        "@com_google_absl//absl/strings",
    ],
)

cc_library(
    name = "file_analyzer",
    srcs = ["file_analyzer.cc"],
    hdrs = ["file_analyzer.h"],
    deps = [
        "//common/lexer",
        "//common/lexer:token_stream_adapter",
        "//common/parser:parse",
        "//common/strings:line_column_map",
        "//common/text:concrete_syntax_tree",
        "//common/text:text_structure",
        "//common/text:token_info",
        "//common/text:token_stream_view",
        "//common/util:spacer",
        "@com_google_absl//absl/status",
        "@com_google_absl//absl/strings",
    ],
)

cc_library(
    name = "linter_test_utils",
    testonly = 1,
    srcs = ["linter_test_utils.cc"],
    hdrs = ["linter_test_utils.h"],
    deps = [
        ":lint_rule_status",
        "//common/text:token_info",
        "//common/text:token_info_test_util",
        "//common/util:algorithm",
        "@com_google_absl//absl/status",
        "@com_google_absl//absl/strings",
        "@com_google_googletest//:gtest",  # for library testonly
    ],
)

cc_library(
    name = "syntax_tree_search_test_utils",
    testonly = 1,
    srcs = ["syntax_tree_search_test_utils.cc"],
    hdrs = ["syntax_tree_search_test_utils.h"],
    deps = [
        ":syntax_tree_search",
        "//common/lexer:lexer_test_util",
        "//common/text:token_info",
        "//common/text:tree_utils",
        "//common/util:algorithm",
        "//common/util:logging",
        "@com_google_absl//absl/strings",
        "@com_google_googletest//:gtest",  # for library testonly
    ],
)

cc_library(
    name = "line_linter",
    srcs = ["line_linter.cc"],
    hdrs = ["line_linter.h"],
    deps = [
        ":line_lint_rule",
        ":lint_rule_status",
        "//common/util:logging",
        "@com_google_absl//absl/strings",
    ],
)

cc_library(
    name = "line_linter_test_utils",
    testonly = 1,
    hdrs = ["line_linter_test_utils.h"],
    deps = [
        ":line_lint_rule",
        ":line_linter",
        ":lint_rule_status",
        ":linter_test_utils",
        "//common/text:text_structure",
        "@com_google_absl//absl/strings",
    ],
)

cc_library(
    name = "line_lint_rule",
    hdrs = ["line_lint_rule.h"],
    deps = [
        ":lint_rule",
        "@com_google_absl//absl/strings",
    ],
)

cc_library(
    name = "syntax_tree_linter",
    srcs = ["syntax_tree_linter.cc"],
    hdrs = ["syntax_tree_linter.h"],
    deps = [
        ":lint_rule_status",
        ":syntax_tree_lint_rule",
        "//common/text:concrete_syntax_leaf",
        "//common/text:concrete_syntax_tree",
        "//common/text:symbol",
        "//common/text:syntax_tree_context",
        "//common/text:tree_context_visitor",
        "//common/util:logging",
    ],
)

cc_library(
    name = "syntax_tree_linter_test_utils",
    testonly = 1,
    hdrs = ["syntax_tree_linter_test_utils.h"],
    deps = [
        ":lint_rule_status",
        ":linter_test_utils",
        ":syntax_tree_lint_rule",
        ":syntax_tree_linter",
        "//common/text:text_structure",
        "//common/util:logging",
        "@com_google_absl//absl/strings",
        "@com_google_googletest//:gtest",  # for library testonly
    ],
)

cc_library(
    name = "syntax_tree_lint_rule",
    hdrs = ["syntax_tree_lint_rule.h"],
    deps = [
        ":lint_rule",
        "//common/text:concrete_syntax_leaf",
        "//common/text:concrete_syntax_tree",
        "//common/text:symbol",
        "//common/text:syntax_tree_context",
    ],
)

cc_library(
    name = "syntax_tree_search",
    srcs = ["syntax_tree_search.cc"],
    hdrs = ["syntax_tree_search.h"],
    deps = [
        "//common/analysis/matcher",
        "//common/analysis/matcher:bound_symbol_manager",
        "//common/text:concrete_syntax_leaf",
        "//common/text:concrete_syntax_tree",
        "//common/text:symbol",
        "//common/text:syntax_tree_context",
        "//common/text:tree_context_visitor",
    ],
)

cc_library(
    name = "text_structure_linter",
    srcs = ["text_structure_linter.cc"],
    hdrs = ["text_structure_linter.h"],
    deps = [
        ":lint_rule_status",
        ":text_structure_lint_rule",
        "//common/text:text_structure",
        "//common/util:logging",
        "@com_google_absl//absl/strings",
    ],
)

cc_library(
    name = "text_structure_linter_test_utils",
    testonly = 1,
    hdrs = ["text_structure_linter_test_utils.h"],
    deps = [
        ":lint_rule_status",
        ":linter_test_utils",
        ":text_structure_linter",
        "//common/text:symbol",
        "//common/util:logging",
        "@com_google_absl//absl/status",
        "@com_google_googletest//:gtest",  # for library testonly
    ],
)

cc_library(
    name = "text_structure_lint_rule",
    hdrs = ["text_structure_lint_rule.h"],
    deps = [
        ":lint_rule",
        "//common/text:text_structure",
        "@com_google_absl//absl/strings",
    ],
)

cc_library(
    name = "token_stream_linter",
    srcs = ["token_stream_linter.cc"],
    hdrs = ["token_stream_linter.h"],
    deps = [
        ":lint_rule_status",
        ":token_stream_lint_rule",
        "//common/text:token_stream_view",
        "//common/util:logging",
    ],
)

cc_library(
    name = "token_stream_linter_test_utils",
    testonly = 1,
    hdrs = ["token_stream_linter_test_utils.h"],
    deps = [
        ":lint_rule_status",
        ":linter_test_utils",
        ":token_stream_lint_rule",
        ":token_stream_linter",
        "//common/text:text_structure",
        "//common/util:logging",
        "@com_google_absl//absl/strings",
    ],
)

cc_library(
    name = "token_stream_lint_rule",
    hdrs = ["token_stream_lint_rule.h"],
    deps = [
        ":lint_rule",
        "//common/text:token_info",
    ],
)

cc_test(
    name = "lint_rule_status_test",
    srcs = ["lint_rule_status_test.cc"],
    deps = [
        ":lint_rule_status",
        "//common/text:concrete_syntax_tree",
        "//common/text:token_info",
        "//common/text:tree_builder_test_util",
        "@com_google_absl//absl/strings",
        "@com_google_googletest//:gtest_main",
    ],
)

cc_test(
    name = "lint_waiver_test",
    srcs = ["lint_waiver_test.cc"],
    deps = [
        ":lint_waiver",
        "//common/strings:line_column_map",
        "//common/text:text_structure_test_utils",
        "//common/text:token_info",
        "//common/text:token_stream_view",
        "//common/util:iterator_range",
        "@com_google_googletest//:gtest_main",
    ],
)

cc_test(
    name = "file_analyzer_test",
    srcs = ["file_analyzer_test.cc"],
    deps = [
        ":file_analyzer",
        "//common/text:text_structure",
        "//common/text:token_info",
        "@com_google_absl//absl/status",
        "@com_google_absl//absl/strings",
        "@com_google_googletest//:gtest_main",
    ],
)

cc_test(
    name = "linter_test_utils_test",
    srcs = ["linter_test_utils_test.cc"],
    deps = [
        ":lint_rule_status",
        ":linter_test_utils",
        "//common/util:range",
        "@com_google_absl//absl/strings",
        "@com_google_googletest//:gtest_main",
    ],
)

cc_test(
    name = "line_linter_test",
    srcs = ["line_linter_test.cc"],
    deps = [
        ":line_lint_rule",
        ":line_linter",
        ":lint_rule_status",
        "//common/text:token_info",
        "@com_google_absl//absl/strings",
        "@com_google_googletest//:gtest_main",
    ],
)

cc_test(
    name = "syntax_tree_linter_test",
    srcs = ["syntax_tree_linter_test.cc"],
    deps = [
        ":lint_rule_status",
        ":syntax_tree_lint_rule",
        ":syntax_tree_linter",
        "//common/text:concrete_syntax_leaf",
        "//common/text:concrete_syntax_tree",
        "//common/text:symbol",
        "//common/text:syntax_tree_context",
        "//common/text:token_info",
        "//common/text:tree_builder_test_util",
        "@com_google_googletest//:gtest_main",
    ],
)

cc_test(
    name = "syntax_tree_search_test",
    srcs = ["syntax_tree_search_test.cc"],
    deps = [
        ":syntax_tree_search",
        "//common/analysis/matcher",
        "//common/analysis/matcher:matcher_builders",
        "//common/text:symbol",
        "//common/text:syntax_tree_context",
        "//common/text:tree_builder_test_util",
        "//common/text:tree_utils",
        "@com_google_googletest//:gtest_main",
    ],
)

cc_test(
    name = "syntax_tree_search_test_utils_test",
    srcs = ["syntax_tree_search_test_utils_test.cc"],
    deps = [
        ":syntax_tree_search_test_utils",
        "//common/text:tree_builder_test_util",
        "//common/util:range",
        "@com_google_absl//absl/strings",
        "@com_google_googletest//:gtest_main",
    ],
)

cc_test(
    name = "text_structure_linter_test",
    srcs = ["text_structure_linter_test.cc"],
    deps = [
        ":lint_rule_status",
        ":text_structure_lint_rule",
        ":text_structure_linter",
        "//common/text:text_structure",
        "//common/text:token_info",
        "@com_google_absl//absl/strings",
        "@com_google_googletest//:gtest_main",
    ],
)

cc_test(
    name = "token_stream_linter_test",
    srcs = ["token_stream_linter_test.cc"],
    deps = [
        ":lint_rule_status",
        ":token_stream_lint_rule",
        ":token_stream_linter",
        "//common/text:token_info",
        "//common/text:token_stream_view",
        "@com_google_absl//absl/strings",
        "@com_google_googletest//:gtest_main",
    ],
)
