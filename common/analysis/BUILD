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
    srcs = ["lint-rule-status.cc"],
    hdrs = ["lint-rule-status.h"],
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
    hdrs = ["lint-rule.h"],
    deps = [
        ":lint-rule-status",
        "@com_google_absl//absl/status",
        "@com_google_absl//absl/strings:string_view",
    ],
)

genlex(
    name = "command-file-lex",
    src = "command_file.lex",
    out = "command-file.yy.cc",
)

cc_library(
    name = "command-file-lexer",
    srcs = [
        "command-file.yy.cc",
        "command-file-lexer.cc",
    ],
    hdrs = [
        "command-file-lexer.h",
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
    srcs = ["violation-handler.cc"],
    hdrs = ["violation-handler.h"],
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
    srcs = ["command-file-lexer_test.cc"],
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
    srcs = ["lint-waiver.cc"],
    hdrs = ["lint-waiver.h"],
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
        "@com_google_absl//absl/container:flat_hash_map",
        "@com_google_absl//absl/status",
        "@com_google_absl//absl/status:statusor",
        "@com_google_absl//absl/strings",
        "@com_google_absl//absl/strings:string_view",
        "@com_googlesource_code_re2//:re2",
    ],
)

cc_library(
    name = "file-analyzer",
    srcs = ["file-analyzer.cc"],
    hdrs = ["file-analyzer.h"],
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
    srcs = ["linter-test-utils.cc"],
    hdrs = ["linter-test-utils.h"],
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
    srcs = ["syntax-tree-search-test-utils.cc"],
    hdrs = ["syntax-tree-search-test-utils.h"],
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
    srcs = ["line-linter.cc"],
    hdrs = ["line-linter.h"],
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
    hdrs = ["line-linter-test-utils.h"],
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
    hdrs = ["line-lint-rule.h"],
    deps = [
        ":lint-rule",
        "@com_google_absl//absl/strings:string_view",
    ],
)

cc_library(
    name = "syntax-tree-linter",
    srcs = ["syntax-tree-linter.cc"],
    hdrs = ["syntax-tree-linter.h"],
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
    hdrs = ["syntax-tree-linter-test-utils.h"],
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
    hdrs = ["syntax-tree-lint-rule.h"],
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
    srcs = ["syntax-tree-search.cc"],
    hdrs = ["syntax-tree-search.h"],
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
    srcs = ["text-structure-linter.cc"],
    hdrs = ["text-structure-linter.h"],
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
    hdrs = ["text-structure-linter-test-utils.h"],
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
    hdrs = ["text-structure-lint-rule.h"],
    deps = [
        ":lint-rule",
        "//common/text:text-structure",
        "@com_google_absl//absl/strings:string_view",
    ],
)

cc_library(
    name = "token-stream-linter",
    srcs = ["token-stream-linter.cc"],
    hdrs = ["token-stream-linter.h"],
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
    hdrs = ["token-stream-linter-test-utils.h"],
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
    hdrs = ["token-stream-lint-rule.h"],
    deps = [
        ":lint-rule",
        "//common/text:token-info",
    ],
)

cc_test(
    name = "lint-rule-status_test",
    srcs = ["lint-rule-status_test.cc"],
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
    srcs = ["lint-waiver_test.cc"],
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
    srcs = ["file-analyzer_test.cc"],
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
    srcs = ["linter-test-utils_test.cc"],
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
    srcs = ["line-linter_test.cc"],
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
    srcs = ["syntax-tree-linter_test.cc"],
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
    srcs = ["syntax-tree-search_test.cc"],
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
    srcs = ["syntax-tree-search-test-utils_test.cc"],
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
    srcs = ["text-structure-linter_test.cc"],
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
    srcs = ["token-stream-linter_test.cc"],
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
