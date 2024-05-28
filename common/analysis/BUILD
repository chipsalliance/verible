# This package contains functions and classes for analyzing text structures.

load("//bazel:flex.bzl", "genlex")

package(
    default_applicable_licenses = ["//:license"],
    default_visibility = [
        "//verilog/CST:__subpackages__",
        "//verilog/analysis:__subpackages__",
        "//verilog/tools/kythe:__pkg__",
        "//verilog/tools/lint:__subpackages__",
        "//verilog/tools/ls:__subpackages__",
    ],
    # Not yet enabled, lexer does not find FlexLexer.h
    #features = ["layering_check"],
)

cc_library(
    name = "citation",
    srcs = ["citation.cc"],
    hdrs = ["citation.h"],
    deps = [
        "@com_google_absl//absl/strings",
        "@com_google_absl//absl/strings:string_view",
    ],
)

cc_library(
    name = "lint-rule-status",
    srcs = ["lint_rule_status.cc"],
    hdrs = ["lint_rule_status.h"],
    deps = [
        ":citation",
        "//common/strings:line-column-map",
        "//common/text:concrete-syntax-leaf",
        "//common/text:symbol",
        "//common/text:syntax-tree-context",
        "//common/text:token-info",
        "//common/text:tree-utils",
        "//common/util:logging",
        "//common/util:spacer",
        "@com_google_absl//absl/strings",
        "@com_google_absl//absl/strings:string_view",
    ],
)

cc_library(
    name = "lint-rule",
    hdrs = ["lint_rule.h"],
    deps = [
        ":lint-rule-status",
        "@com_google_absl//absl/status",
        "@com_google_absl//absl/strings:string_view",
    ],
)

genlex(
    name = "command-file-lex",
    src = "command_file.lex",
    out = "command_file.yy.cc",
)

cc_library(
    name = "command-file-lexer",
    srcs = [
        "command_file.yy.cc",
        "command_file_lexer.cc",
    ],
    hdrs = [
        "command_file_lexer.h",
    ],
    copts = select({
        "@platforms//os:windows": [],
        "//conditions:default": ["-Wno-implicit-fallthrough"],
    }),
    deps = [
        "//bazel:flex",
        "//common/lexer:flex-lexer-adapter",
        "//common/lexer:token-stream-adapter",
        "//common/text:token-info",
        "//common/text:token-stream-view",
        "//common/util:iterator-range",
        "//common/util:logging",
        "@com_google_absl//absl/strings:string_view",
    ],
)

cc_library(
    name = "violation-handler",
    srcs = ["violation_handler.cc"],
    hdrs = ["violation_handler.h"],
    deps = [
        ":lint-rule-status",
        "//common/strings:diff",
        "//common/util:file-util",
        "//common/util:logging",
        "//common/util:user-interaction",
        "@com_google_absl//absl/status",
        "@com_google_absl//absl/strings",
        "@com_google_absl//absl/strings:string_view",
    ],
)

cc_test(
    name = "command-file-lexer_test",
    size = "small",
    srcs = ["command_file_lexer_test.cc"],
    deps = [
        ":command-file-lexer",
        "//common/lexer:lexer-test-util",
        "//common/text:constants",
        "//common/text:token-info",
        "//common/text:token-info-test-util",
        "@com_google_absl//absl/strings:string_view",
        "@com_google_googletest//:gtest",
        "@com_google_googletest//:gtest_main",
    ],
)

cc_library(
    name = "lint-waiver",
    srcs = ["lint_waiver.cc"],
    hdrs = ["lint_waiver.h"],
    copts = select({
        "@platforms//os:windows": [],
        "//conditions:default": ["-fexceptions"],
    }),
    features = ["-use_header_modules"],  # precompiled headers incompatible with -fexceptions.
    deps = [
        ":command-file-lexer",
        "//common/strings:comment-utils",
        "//common/strings:line-column-map",
        "//common/strings:position",
        "//common/text:text-structure",
        "//common/text:token-info",
        "//common/text:token-stream-view",
        "//common/util:container-iterator-range",
        "//common/util:container-util",
        "//common/util:file-util",
        "//common/util:iterator-range",
        "//common/util:logging",
        "@com_google_absl//absl/status",
        "@com_google_absl//absl/status:statusor",
        "@com_google_absl//absl/strings",
        "@com_google_absl//absl/strings:string_view",
    ],
)

cc_library(
    name = "file-analyzer",
    srcs = ["file_analyzer.cc"],
    hdrs = ["file_analyzer.h"],
    deps = [
        "//common/lexer",
        "//common/lexer:token-stream-adapter",
        "//common/parser:parse",
        "//common/strings:line-column-map",
        "//common/strings:mem-block",
        "//common/text:concrete-syntax-tree",
        "//common/text:text-structure",
        "//common/text:token-info",
        "//common/text:token-stream-view",
        "//common/util:logging",
        "//common/util:spacer",
        "@com_google_absl//absl/status",
        "@com_google_absl//absl/strings:string_view",
    ],
)

cc_library(
    name = "linter-test-utils",
    testonly = 1,
    srcs = ["linter_test_utils.cc"],
    hdrs = ["linter_test_utils.h"],
    deps = [
        ":lint-rule-status",
        "//common/text:token-info",
        "//common/text:token-info-test-util",
        "//common/util:algorithm",
        "//common/util:logging",
        "@com_google_absl//absl/status",
        "@com_google_absl//absl/strings",
        "@com_google_absl//absl/strings:string_view",
        "@com_google_googletest//:gtest",  # for library testonly
    ],
)

cc_library(
    name = "syntax-tree-search-test-utils",
    testonly = 1,
    srcs = ["syntax_tree_search_test_utils.cc"],
    hdrs = ["syntax_tree_search_test_utils.h"],
    deps = [
        ":syntax-tree-search",
        "//common/lexer:lexer-test-util",
        "//common/text:token-info",
        "//common/text:token-info-test-util",
        "//common/text:tree-utils",
        "//common/util:algorithm",
        "//common/util:logging",
        "@com_google_absl//absl/strings:string_view",
    ],
)

cc_library(
    name = "line-linter",
    srcs = ["line_linter.cc"],
    hdrs = ["line_linter.h"],
    deps = [
        ":line-lint-rule",
        ":lint-rule-status",
        "//common/util:logging",
        "@com_google_absl//absl/strings:string_view",
    ],
)

cc_library(
    name = "line-linter-test-utils",
    testonly = 1,
    hdrs = ["line_linter_test_utils.h"],
    deps = [
        ":line-lint-rule",
        ":line-linter",
        ":lint-rule-status",
        ":linter-test-utils",
        "//common/text:text-structure",
        "//common/util:logging",
        "@com_google_absl//absl/strings:string_view",
    ],
)

cc_library(
    name = "line-lint-rule",
    hdrs = ["line_lint_rule.h"],
    deps = [
        ":lint-rule",
        "@com_google_absl//absl/strings:string_view",
    ],
)

cc_library(
    name = "syntax-tree-linter",
    srcs = ["syntax_tree_linter.cc"],
    hdrs = ["syntax_tree_linter.h"],
    deps = [
        ":lint-rule-status",
        ":syntax-tree-lint-rule",
        "//common/text:concrete-syntax-leaf",
        "//common/text:concrete-syntax-tree",
        "//common/text:symbol",
        "//common/text:tree-context-visitor",
        "//common/util:logging",
    ],
)

cc_library(
    name = "syntax-tree-linter-test-utils",
    testonly = 1,
    hdrs = ["syntax_tree_linter_test_utils.h"],
    deps = [
        ":lint-rule-status",
        ":linter-test-utils",
        ":syntax-tree-lint-rule",
        ":syntax-tree-linter",
        "//common/text:text-structure",
        "//common/util:logging",
        "@com_google_absl//absl/strings:string_view",
    ],
)

cc_library(
    name = "syntax-tree-lint-rule",
    hdrs = ["syntax_tree_lint_rule.h"],
    deps = [
        ":lint-rule",
        "//common/text:concrete-syntax-leaf",
        "//common/text:concrete-syntax-tree",
        "//common/text:symbol",
        "//common/text:syntax-tree-context",
    ],
)

cc_library(
    name = "syntax-tree-search",
    srcs = ["syntax_tree_search.cc"],
    hdrs = ["syntax_tree_search.h"],
    deps = [
        "//common/analysis/matcher",
        "//common/analysis/matcher:bound-symbol-manager",
        "//common/text:concrete-syntax-leaf",
        "//common/text:concrete-syntax-tree",
        "//common/text:symbol",
        "//common/text:syntax-tree-context",
        "//common/text:tree-context-visitor",
    ],
)

cc_library(
    name = "text-structure-linter",
    srcs = ["text_structure_linter.cc"],
    hdrs = ["text_structure_linter.h"],
    deps = [
        ":lint-rule-status",
        ":text-structure-lint-rule",
        "//common/text:text-structure",
        "//common/util:logging",
        "@com_google_absl//absl/strings:string_view",
    ],
)

cc_library(
    name = "text-structure-linter-test-utils",
    testonly = 1,
    hdrs = ["text_structure_linter_test_utils.h"],
    deps = [
        ":lint-rule-status",
        ":linter-test-utils",
        ":text-structure-lint-rule",
        ":text-structure-linter",
        "//common/text:text-structure",
        "//common/util:logging",
        "@com_google_absl//absl/strings:string_view",
    ],
)

cc_library(
    name = "text-structure-lint-rule",
    hdrs = ["text_structure_lint_rule.h"],
    deps = [
        ":lint-rule",
        "//common/text:text-structure",
        "@com_google_absl//absl/strings:string_view",
    ],
)

cc_library(
    name = "token-stream-linter",
    srcs = ["token_stream_linter.cc"],
    hdrs = ["token_stream_linter.h"],
    deps = [
        ":lint-rule-status",
        ":token-stream-lint-rule",
        "//common/text:token-stream-view",
        "//common/util:logging",
    ],
)

cc_library(
    name = "token-stream-linter-test-utils",
    testonly = 1,
    hdrs = ["token_stream_linter_test_utils.h"],
    deps = [
        ":lint-rule-status",
        ":linter-test-utils",
        ":token-stream-lint-rule",
        ":token-stream-linter",
        "//common/text:text-structure",
        "//common/util:logging",
        "@com_google_absl//absl/strings:string_view",
    ],
)

cc_library(
    name = "token-stream-lint-rule",
    hdrs = ["token_stream_lint_rule.h"],
    deps = [
        ":lint-rule",
        "//common/text:token-info",
    ],
)

cc_test(
    name = "lint-rule-status_test",
    srcs = ["lint_rule_status_test.cc"],
    deps = [
        ":lint-rule-status",
        "//common/text:concrete-syntax-tree",
        "//common/text:token-info",
        "//common/text:tree-builder-test-util",
        "@com_google_absl//absl/strings",
        "@com_google_absl//absl/strings:string_view",
        "@com_google_googletest//:gtest",
        "@com_google_googletest//:gtest_main",
    ],
)

cc_test(
    name = "lint-waiver_test",
    srcs = ["lint_waiver_test.cc"],
    deps = [
        ":lint-waiver",
        "//common/strings:line-column-map",
        "//common/text:text-structure-test-utils",
        "//common/text:token-info",
        "//common/text:token-stream-view",
        "//common/util:iterator-range",
        "@com_google_absl//absl/strings:string_view",
        "@com_google_googletest//:gtest",
        "@com_google_googletest//:gtest_main",
    ],
)

cc_test(
    name = "file-analyzer_test",
    srcs = ["file_analyzer_test.cc"],
    deps = [
        ":file-analyzer",
        "//common/strings:line-column-map",
        "//common/text:text-structure",
        "//common/text:token-info",
        "@com_google_absl//absl/status",
        "@com_google_absl//absl/strings",
        "@com_google_absl//absl/strings:string_view",
        "@com_google_googletest//:gtest",
        "@com_google_googletest//:gtest_main",
    ],
)

cc_test(
    name = "linter-test-utils_test",
    srcs = ["linter_test_utils_test.cc"],
    deps = [
        ":lint-rule-status",
        ":linter-test-utils",
        "//common/util:range",
        "@com_google_absl//absl/strings",
        "@com_google_absl//absl/strings:string_view",
        "@com_google_googletest//:gtest",
        "@com_google_googletest//:gtest_main",
    ],
)

cc_test(
    name = "line-linter_test",
    srcs = ["line_linter_test.cc"],
    deps = [
        ":line-lint-rule",
        ":line-linter",
        ":lint-rule-status",
        "//common/text:token-info",
        "@com_google_absl//absl/strings:string_view",
        "@com_google_googletest//:gtest",
        "@com_google_googletest//:gtest_main",
    ],
)

cc_test(
    name = "syntax-tree-linter_test",
    srcs = ["syntax_tree_linter_test.cc"],
    deps = [
        ":lint-rule-status",
        ":syntax-tree-lint-rule",
        ":syntax-tree-linter",
        "//common/text:concrete-syntax-leaf",
        "//common/text:concrete-syntax-tree",
        "//common/text:symbol",
        "//common/text:syntax-tree-context",
        "//common/text:token-info",
        "//common/text:tree-builder-test-util",
        "//common/util:casts",
        "@com_google_absl//absl/strings:string_view",
        "@com_google_googletest//:gtest",
        "@com_google_googletest//:gtest_main",
    ],
)

cc_test(
    name = "syntax-tree-search_test",
    srcs = ["syntax_tree_search_test.cc"],
    deps = [
        ":syntax-tree-search",
        "//common/analysis/matcher:matcher-builders",
        "//common/text:symbol",
        "//common/text:syntax-tree-context",
        "//common/text:tree-builder-test-util",
        "//common/text:tree-utils",
        "@com_google_googletest//:gtest",
        "@com_google_googletest//:gtest_main",
    ],
)

cc_test(
    name = "syntax-tree-search-test-utils_test",
    srcs = ["syntax_tree_search_test_utils_test.cc"],
    deps = [
        ":syntax-tree-search",
        ":syntax-tree-search-test-utils",
        "//common/text:tree-builder-test-util",
        "//common/util:range",
        "@com_google_absl//absl/strings",
        "@com_google_absl//absl/strings:string_view",
        "@com_google_googletest//:gtest",
        "@com_google_googletest//:gtest_main",
    ],
)

cc_test(
    name = "text-structure-linter_test",
    srcs = ["text_structure_linter_test.cc"],
    deps = [
        ":lint-rule-status",
        ":text-structure-lint-rule",
        ":text-structure-linter",
        "//common/text:text-structure",
        "//common/text:token-info",
        "@com_google_absl//absl/strings",
        "@com_google_absl//absl/strings:string_view",
        "@com_google_googletest//:gtest",
        "@com_google_googletest//:gtest_main",
    ],
)

cc_test(
    name = "token-stream-linter_test",
    srcs = ["token_stream_linter_test.cc"],
    deps = [
        ":lint-rule-status",
        ":token-stream-lint-rule",
        ":token-stream-linter",
        "//common/text:token-info",
        "//common/text:token-stream-view",
        "@com_google_absl//absl/strings:string_view",
        "@com_google_googletest//:gtest",
        "@com_google_googletest//:gtest_main",
    ],
)
