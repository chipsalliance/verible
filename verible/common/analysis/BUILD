# This package contains functions and classes for analyzing text structures.

load("//bazel:flex.bzl", "genlex")

package(
    default_applicable_licenses = ["//:license"],
    default_visibility = [
        "//verible/verilog/CST:__subpackages__",
        "//verible/verilog/analysis:__subpackages__",
        "//verible/verilog/tools/kythe:__pkg__",
        "//verible/verilog/tools/lint:__subpackages__",
        "//verible/verilog/tools/ls:__subpackages__",
    ],
    # Not yet enabled, lexer does not find FlexLexer.h
    #features = ["layering_check"],
)

cc_library(
    name = "citation",
    srcs = ["citation.cc"],
    hdrs = ["citation.h"],
    deps = ["@abseil-cpp//absl/strings"],
)

cc_library(
    name = "lint-rule-status",
    srcs = ["lint-rule-status.cc"],
    hdrs = ["lint-rule-status.h"],
    deps = [
        ":citation",
        "//verible/common/strings:line-column-map",
        "//verible/common/text:concrete-syntax-leaf",
        "//verible/common/text:symbol",
        "//verible/common/text:syntax-tree-context",
        "//verible/common/text:token-info",
        "//verible/common/text:tree-utils",
        "//verible/common/util:logging",
        "//verible/common/util:spacer",
        "@abseil-cpp//absl/strings",
    ],
)

cc_library(
    name = "lint-rule",
    hdrs = ["lint-rule.h"],
    deps = [
        ":lint-rule-status",
        "@abseil-cpp//absl/status",
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
        "//verible/common/lexer:flex-lexer-adapter",
        "//verible/common/lexer:token-stream-adapter",
        "//verible/common/text:token-info",
        "//verible/common/text:token-stream-view",
        "//verible/common/util:iterator-range",
        "//verible/common/util:logging",
    ],
)

cc_library(
    name = "violation-handler",
    srcs = ["violation-handler.cc"],
    hdrs = ["violation-handler.h"],
    deps = [
        ":lint-rule-status",
        "//verible/common/strings:diff",
        "//verible/common/util:file-util",
        "//verible/common/util:logging",
        "//verible/common/util:user-interaction",
        "@abseil-cpp//absl/status",
        "@abseil-cpp//absl/strings",
    ],
)

cc_test(
    name = "command-file-lexer_test",
    size = "small",
    srcs = ["command-file-lexer_test.cc"],
    deps = [
        ":command-file-lexer",
        "//verible/common/lexer:lexer-test-util",
        "//verible/common/text:constants",
        "//verible/common/text:token-info",
        "//verible/common/text:token-info-test-util",
        "@googletest//:gtest",
        "@googletest//:gtest_main",
    ],
)

cc_library(
    name = "lint-waiver",
    srcs = ["lint-waiver.cc"],
    hdrs = ["lint-waiver.h"],
    deps = [
        ":command-file-lexer",
        "//verible/common/strings:comment-utils",
        "//verible/common/strings:line-column-map",
        "//verible/common/strings:position",
        "//verible/common/text:text-structure",
        "//verible/common/text:token-info",
        "//verible/common/text:token-stream-view",
        "//verible/common/util:container-iterator-range",
        "//verible/common/util:container-util",
        "//verible/common/util:file-util",
        "//verible/common/util:iterator-range",
        "//verible/common/util:logging",
        "@abseil-cpp//absl/container:flat_hash_map",
        "@abseil-cpp//absl/status",
        "@abseil-cpp//absl/status:statusor",
        "@abseil-cpp//absl/strings",
        "@re2",
    ],
)

cc_library(
    name = "file-analyzer",
    srcs = ["file-analyzer.cc"],
    hdrs = ["file-analyzer.h"],
    deps = [
        "//verible/common/lexer",
        "//verible/common/lexer:token-stream-adapter",
        "//verible/common/parser:parse",
        "//verible/common/strings:line-column-map",
        "//verible/common/strings:mem-block",
        "//verible/common/text:concrete-syntax-tree",
        "//verible/common/text:text-structure",
        "//verible/common/text:token-info",
        "//verible/common/text:token-stream-view",
        "//verible/common/util:logging",
        "//verible/common/util:spacer",
        "@abseil-cpp//absl/status",
    ],
)

cc_library(
    name = "linter-test-utils",
    testonly = 1,
    srcs = ["linter-test-utils.cc"],
    hdrs = ["linter-test-utils.h"],
    deps = [
        ":lint-rule-status",
        "//verible/common/text:token-info",
        "//verible/common/text:token-info-test-util",
        "//verible/common/util:algorithm",
        "//verible/common/util:logging",
        "@abseil-cpp//absl/status",
        "@abseil-cpp//absl/strings",
        "@googletest//:gtest",  # for library testonly
    ],
)

cc_library(
    name = "syntax-tree-search-test-utils",
    testonly = 1,
    srcs = ["syntax-tree-search-test-utils.cc"],
    hdrs = ["syntax-tree-search-test-utils.h"],
    deps = [
        ":syntax-tree-search",
        "//verible/common/lexer:lexer-test-util",
        "//verible/common/text:token-info",
        "//verible/common/text:token-info-test-util",
        "//verible/common/text:tree-utils",
        "//verible/common/util:algorithm",
        "//verible/common/util:logging",
    ],
)

cc_library(
    name = "line-linter",
    srcs = ["line-linter.cc"],
    hdrs = ["line-linter.h"],
    deps = [
        ":line-lint-rule",
        ":lint-rule-status",
        "//verible/common/util:logging",
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
        "//verible/common/text:text-structure",
        "//verible/common/util:logging",
    ],
)

cc_library(
    name = "line-lint-rule",
    hdrs = ["line-lint-rule.h"],
    deps = [":lint-rule"],
)

cc_library(
    name = "syntax-tree-linter",
    srcs = ["syntax-tree-linter.cc"],
    hdrs = ["syntax-tree-linter.h"],
    deps = [
        ":lint-rule-status",
        ":syntax-tree-lint-rule",
        "//verible/common/text:concrete-syntax-leaf",
        "//verible/common/text:concrete-syntax-tree",
        "//verible/common/text:symbol",
        "//verible/common/text:tree-context-visitor",
        "//verible/common/util:logging",
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
        "//verible/common/text:text-structure",
        "//verible/common/util:logging",
    ],
)

cc_library(
    name = "syntax-tree-lint-rule",
    hdrs = ["syntax-tree-lint-rule.h"],
    deps = [
        ":lint-rule",
        "//verible/common/text:concrete-syntax-leaf",
        "//verible/common/text:concrete-syntax-tree",
        "//verible/common/text:symbol",
        "//verible/common/text:syntax-tree-context",
    ],
)

cc_library(
    name = "syntax-tree-search",
    srcs = ["syntax-tree-search.cc"],
    hdrs = ["syntax-tree-search.h"],
    deps = [
        "//verible/common/analysis/matcher",
        "//verible/common/analysis/matcher:bound-symbol-manager",
        "//verible/common/text:concrete-syntax-leaf",
        "//verible/common/text:concrete-syntax-tree",
        "//verible/common/text:symbol",
        "//verible/common/text:syntax-tree-context",
        "//verible/common/text:tree-context-visitor",
    ],
)

cc_library(
    name = "text-structure-linter",
    srcs = ["text-structure-linter.cc"],
    hdrs = ["text-structure-linter.h"],
    deps = [
        ":lint-rule-status",
        ":text-structure-lint-rule",
        "//verible/common/text:text-structure",
        "//verible/common/util:logging",
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
        "//verible/common/text:text-structure",
        "//verible/common/util:logging",
    ],
)

cc_library(
    name = "text-structure-lint-rule",
    hdrs = ["text-structure-lint-rule.h"],
    deps = [
        ":lint-rule",
        "//verible/common/text:text-structure",
    ],
)

cc_library(
    name = "token-stream-linter",
    srcs = ["token-stream-linter.cc"],
    hdrs = ["token-stream-linter.h"],
    deps = [
        ":lint-rule-status",
        ":token-stream-lint-rule",
        "//verible/common/text:token-stream-view",
        "//verible/common/util:logging",
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
        "//verible/common/text:text-structure",
        "//verible/common/util:logging",
    ],
)

cc_library(
    name = "token-stream-lint-rule",
    hdrs = ["token-stream-lint-rule.h"],
    deps = [
        ":lint-rule",
        "//verible/common/text:token-info",
    ],
)

cc_test(
    name = "lint-rule-status_test",
    srcs = ["lint-rule-status_test.cc"],
    deps = [
        ":lint-rule-status",
        "//verible/common/text:concrete-syntax-tree",
        "//verible/common/text:token-info",
        "//verible/common/text:tree-builder-test-util",
        "@abseil-cpp//absl/strings",
        "@googletest//:gtest",
        "@googletest//:gtest_main",
    ],
)

cc_test(
    name = "lint-waiver_test",
    srcs = ["lint-waiver_test.cc"],
    deps = [
        ":lint-waiver",
        "//verible/common/strings:line-column-map",
        "//verible/common/text:text-structure-test-utils",
        "//verible/common/text:token-info",
        "//verible/common/text:token-stream-view",
        "//verible/common/util:iterator-range",
        "@googletest//:gtest",
        "@googletest//:gtest_main",
    ],
)

cc_test(
    name = "file-analyzer_test",
    srcs = ["file-analyzer_test.cc"],
    deps = [
        ":file-analyzer",
        "//verible/common/strings:line-column-map",
        "//verible/common/text:text-structure",
        "//verible/common/text:token-info",
        "@abseil-cpp//absl/status",
        "@abseil-cpp//absl/strings",
        "@googletest//:gtest",
        "@googletest//:gtest_main",
    ],
)

cc_test(
    name = "linter-test-utils_test",
    srcs = ["linter-test-utils_test.cc"],
    deps = [
        ":lint-rule-status",
        ":linter-test-utils",
        "//verible/common/util:range",
        "@abseil-cpp//absl/strings",
        "@googletest//:gtest",
        "@googletest//:gtest_main",
    ],
)

cc_test(
    name = "line-linter_test",
    srcs = ["line-linter_test.cc"],
    deps = [
        ":line-lint-rule",
        ":line-linter",
        ":lint-rule-status",
        "//verible/common/text:token-info",
        "@googletest//:gtest",
        "@googletest//:gtest_main",
    ],
)

cc_test(
    name = "syntax-tree-linter_test",
    srcs = ["syntax-tree-linter_test.cc"],
    deps = [
        ":lint-rule-status",
        ":syntax-tree-lint-rule",
        ":syntax-tree-linter",
        "//verible/common/text:concrete-syntax-leaf",
        "//verible/common/text:concrete-syntax-tree",
        "//verible/common/text:symbol",
        "//verible/common/text:syntax-tree-context",
        "//verible/common/text:token-info",
        "//verible/common/text:tree-builder-test-util",
        "//verible/common/util:casts",
        "@googletest//:gtest",
        "@googletest//:gtest_main",
    ],
)

cc_test(
    name = "syntax-tree-search_test",
    srcs = ["syntax-tree-search_test.cc"],
    deps = [
        ":syntax-tree-search",
        "//verible/common/analysis/matcher:matcher-builders",
        "//verible/common/text:symbol",
        "//verible/common/text:syntax-tree-context",
        "//verible/common/text:tree-builder-test-util",
        "//verible/common/text:tree-utils",
        "@googletest//:gtest",
        "@googletest//:gtest_main",
    ],
)

cc_test(
    name = "syntax-tree-search-test-utils_test",
    srcs = ["syntax-tree-search-test-utils_test.cc"],
    deps = [
        ":syntax-tree-search",
        ":syntax-tree-search-test-utils",
        "//verible/common/text:tree-builder-test-util",
        "//verible/common/util:range",
        "@abseil-cpp//absl/strings",
        "@googletest//:gtest",
        "@googletest//:gtest_main",
    ],
)

cc_test(
    name = "text-structure-linter_test",
    srcs = ["text-structure-linter_test.cc"],
    deps = [
        ":lint-rule-status",
        ":text-structure-lint-rule",
        ":text-structure-linter",
        "//verible/common/text:text-structure",
        "//verible/common/text:token-info",
        "@abseil-cpp//absl/strings",
        "@googletest//:gtest",
        "@googletest//:gtest_main",
    ],
)

cc_test(
    name = "token-stream-linter_test",
    srcs = ["token-stream-linter_test.cc"],
    deps = [
        ":lint-rule-status",
        ":token-stream-lint-rule",
        ":token-stream-linter",
        "//verible/common/text:token-info",
        "//verible/common/text:token-stream-view",
        "@googletest//:gtest",
        "@googletest//:gtest_main",
    ],
)
