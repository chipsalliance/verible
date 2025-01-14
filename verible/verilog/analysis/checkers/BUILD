# A set of linter rules to be used in analysis of verilog code.

default_visibility = [
    "//verible/verilog/analysis:__subpackages__",
    "//verible/verilog/tools:__subpackages__",
]

package(
    default_applicable_licenses = ["//:license"],
    default_visibility = default_visibility,
    features = ["layering_check"],
)

cc_library(
    name = "verilog-lint-rules",
    deps = [
        ":always-comb-blocking-rule",
        ":always-comb-rule",
        ":always-ff-non-blocking-rule",
        ":banned-declared-name-patterns-rule",
        ":case-missing-default-rule",
        ":constraint-name-style-rule",
        ":create-object-name-match-rule",
        ":dff-name-style-rule",
        ":disable-statement-rule",
        ":endif-comment-rule",
        ":enum-name-style-rule",
        ":explicit-begin-rule",
        ":explicit-function-lifetime-rule",
        ":explicit-function-task-parameter-type-rule",
        ":explicit-parameter-storage-type-rule",
        ":explicit-task-lifetime-rule",
        ":forbid-consecutive-null-statements-rule",
        ":forbid-defparam-rule",
        ":forbid-line-continuations-rule",
        ":forbid-negative-array-dim",
        ":forbidden-anonymous-enums-rule",
        ":forbidden-anonymous-structs-unions-rule",
        ":forbidden-macro-rule",
        ":forbidden-symbol-rule",
        ":generate-label-prefix-rule",
        ":generate-label-rule",
        ":instance-shadow-rule",
        ":interface-name-style-rule",
        ":legacy-generate-region-rule",
        ":legacy-genvar-declaration-rule",
        ":line-length-rule",
        ":macro-name-style-rule",
        ":macro-string-concatenation-rule",
        ":mismatched-labels-rule",
        ":module-begin-block-rule",
        ":module-filename-rule",
        ":module-instantiation-rules",
        ":no-tabs-rule",
        ":no-trailing-spaces-rule",
        ":numeric-format-string-style-rule",
        ":one-module-per-file-rule",
        ":package-filename-rule",
        ":packed-dimensions-rule",
        ":parameter-name-style-rule",
        ":parameter-type-name-style-rule",
        ":plusarg-assignment-rule",
        ":port-name-suffix-rule",
        ":positive-meaning-parameter-name-rule",
        ":posix-eof-rule",
        ":proper-parameter-declaration-rule",
        ":signal-name-style-rule",
        ":struct-union-name-style-rule",
        ":suggest-parentheses-rule",
        ":suspicious-semicolon-rule",
        ":truncated-numeric-literal-rule",
        ":undersized-binary-literal-rule",
        ":unpacked-dimensions-rule",
        ":uvm-macro-semicolon-rule",
        ":v2001-generate-begin-rule",
        ":void-cast-rule",
    ],
    alwayslink = 1,
)

cc_library(
    name = "banned-declared-name-patterns-rule",
    srcs = ["banned-declared-name-patterns-rule.cc"],
    hdrs = ["banned-declared-name-patterns-rule.h"],
    deps = [
        "//verible/common/analysis:lint-rule-status",
        "//verible/common/analysis:syntax-tree-lint-rule",
        "//verible/common/text:concrete-syntax-tree",
        "//verible/common/text:symbol",
        "//verible/common/text:syntax-tree-context",
        "//verible/common/text:token-info",
        "//verible/verilog/CST:module",
        "//verible/verilog/CST:package",
        "//verible/verilog/CST:verilog-matchers",
        "//verible/verilog/CST:verilog-nonterminals",
        "//verible/verilog/analysis:descriptions",
        "//verible/verilog/analysis:lint-rule-registry",
        "@abseil-cpp//absl/strings",
    ],
    alwayslink = 1,
)

cc_test(
    name = "banned-declared-name-patterns-rule_test",
    srcs = ["banned-declared-name-patterns-rule_test.cc"],
    deps = [
        ":banned-declared-name-patterns-rule",
        "//verible/common/analysis:linter-test-utils",
        "//verible/common/analysis:syntax-tree-linter-test-utils",
        "//verible/verilog/analysis:verilog-analyzer",
        "//verible/verilog/parser:verilog-token-enum",
        "@googletest//:gtest",
        "@googletest//:gtest_main",
    ],
)

cc_library(
    name = "no-tabs-rule",
    srcs = ["no-tabs-rule.cc"],
    hdrs = ["no-tabs-rule.h"],
    deps = [
        "//verible/common/analysis:line-lint-rule",
        "//verible/common/analysis:lint-rule-status",
        "//verible/common/text:token-info",
        "//verible/verilog/analysis:descriptions",
        "//verible/verilog/analysis:lint-rule-registry",
        "//verible/verilog/parser:verilog-token-enum",
    ],
    alwayslink = 1,
)

cc_test(
    name = "no-tabs-rule_test",
    srcs = ["no-tabs-rule_test.cc"],
    deps = [
        ":no-tabs-rule",
        "//verible/common/analysis:line-linter-test-utils",
        "//verible/common/analysis:linter-test-utils",
        "//verible/verilog/analysis:verilog-analyzer",
        "//verible/verilog/parser:verilog-token-enum",
        "@googletest//:gtest",
        "@googletest//:gtest_main",
    ],
)

cc_library(
    name = "no-trailing-spaces-rule",
    srcs = ["no-trailing-spaces-rule.cc"],
    hdrs = ["no-trailing-spaces-rule.h"],
    deps = [
        "//verible/common/analysis:line-lint-rule",
        "//verible/common/analysis:lint-rule-status",
        "//verible/common/text:token-info",
        "//verible/verilog/analysis:descriptions",
        "//verible/verilog/analysis:lint-rule-registry",
        "//verible/verilog/parser:verilog-token-enum",
        "@abseil-cpp//absl/strings",
    ],
    alwayslink = 1,
)

cc_test(
    name = "no-trailing-spaces-rule_test",
    srcs = ["no-trailing-spaces-rule_test.cc"],
    deps = [
        ":no-trailing-spaces-rule",
        "//verible/common/analysis:line-linter-test-utils",
        "//verible/common/analysis:linter-test-utils",
        "//verible/verilog/analysis:verilog-analyzer",
        "//verible/verilog/parser:verilog-token-enum",
        "@googletest//:gtest",
        "@googletest//:gtest_main",
    ],
)

cc_library(
    name = "positive-meaning-parameter-name-rule",
    srcs = ["positive-meaning-parameter-name-rule.cc"],
    hdrs = ["positive-meaning-parameter-name-rule.h"],
    deps = [
        "//verible/common/analysis:lint-rule-status",
        "//verible/common/analysis:syntax-tree-lint-rule",
        "//verible/common/analysis/matcher",
        "//verible/common/analysis/matcher:bound-symbol-manager",
        "//verible/common/text:symbol",
        "//verible/common/text:syntax-tree-context",
        "//verible/common/text:token-info",
        "//verible/verilog/CST:parameters",
        "//verible/verilog/CST:verilog-matchers",
        "//verible/verilog/analysis:descriptions",
        "//verible/verilog/analysis:lint-rule-registry",
        "@abseil-cpp//absl/strings",
    ],
    alwayslink = 1,
)

cc_test(
    name = "positive-meaning-parameter-name-rule_test",
    srcs = ["positive-meaning-parameter-name-rule_test.cc"],
    deps = [
        ":positive-meaning-parameter-name-rule",
        "//verible/common/analysis:linter-test-utils",
        "//verible/common/analysis:syntax-tree-linter-test-utils",
        "//verible/verilog/analysis:verilog-analyzer",
        "//verible/verilog/parser:verilog-token-enum",
        "@googletest//:gtest",
        "@googletest//:gtest_main",
    ],
)

cc_library(
    name = "posix-eof-rule",
    srcs = ["posix-eof-rule.cc"],
    hdrs = ["posix-eof-rule.h"],
    deps = [
        "//verible/common/analysis:lint-rule-status",
        "//verible/common/analysis:text-structure-lint-rule",
        "//verible/common/text:text-structure",
        "//verible/common/text:token-info",
        "//verible/verilog/analysis:descriptions",
        "//verible/verilog/analysis:lint-rule-registry",
        "//verible/verilog/parser:verilog-token-enum",
    ],
    alwayslink = 1,
)

cc_test(
    name = "posix-eof-rule_test",
    srcs = ["posix-eof-rule_test.cc"],
    deps = [
        ":posix-eof-rule",
        "//verible/common/analysis:linter-test-utils",
        "//verible/common/analysis:text-structure-linter-test-utils",
        "//verible/verilog/analysis:verilog-analyzer",
        "//verible/verilog/parser:verilog-token-enum",
        "@googletest//:gtest",
        "@googletest//:gtest_main",
    ],
)

cc_library(
    name = "line-length-rule",
    srcs = ["line-length-rule.cc"],
    hdrs = ["line-length-rule.h"],
    deps = [
        "//verible/common/analysis:lint-rule-status",
        "//verible/common/analysis:text-structure-lint-rule",
        "//verible/common/strings:comment-utils",
        "//verible/common/strings:utf8",
        "//verible/common/text:config-utils",
        "//verible/common/text:constants",
        "//verible/common/text:text-structure",
        "//verible/common/text:token-info",
        "//verible/common/text:token-stream-view",
        "//verible/common/util:iterator-range",
        "//verible/verilog/analysis:descriptions",
        "//verible/verilog/analysis:lint-rule-registry",
        "//verible/verilog/analysis:verilog-linter-constants",
        "//verible/verilog/parser:verilog-token-classifications",
        "//verible/verilog/parser:verilog-token-enum",
        "@abseil-cpp//absl/status",
        "@abseil-cpp//absl/strings",
    ],
    alwayslink = 1,
)

cc_test(
    name = "line-length-rule_test",
    srcs = ["line-length-rule_test.cc"],
    deps = [
        ":line-length-rule",
        "//verible/common/analysis:linter-test-utils",
        "//verible/common/analysis:text-structure-linter-test-utils",
        "//verible/common/util:logging",
        "//verible/verilog/analysis:verilog-analyzer",
        "//verible/verilog/parser:verilog-token-enum",
        "@abseil-cpp//absl/status",
        "@abseil-cpp//absl/strings",
        "@googletest//:gtest",
        "@googletest//:gtest_main",
    ],
)

cc_library(
    name = "forbid-consecutive-null-statements-rule",
    srcs = ["forbid-consecutive-null-statements-rule.cc"],
    hdrs = ["forbid-consecutive-null-statements-rule.h"],
    deps = [
        "//verible/common/analysis:lint-rule-status",
        "//verible/common/analysis:syntax-tree-lint-rule",
        "//verible/common/text:concrete-syntax-leaf",
        "//verible/common/text:symbol",
        "//verible/common/text:syntax-tree-context",
        "//verible/verilog/CST:verilog-matchers",  # fixdeps: keep
        "//verible/verilog/CST:verilog-nonterminals",
        "//verible/verilog/analysis:descriptions",
        "//verible/verilog/analysis:lint-rule-registry",
    ],
    alwayslink = 1,
)

cc_test(
    name = "forbid-consecutive-null-statements-rule_test",
    srcs = ["forbid-consecutive-null-statements-rule_test.cc"],
    deps = [
        ":forbid-consecutive-null-statements-rule",
        "//verible/common/analysis:linter-test-utils",
        "//verible/common/analysis:syntax-tree-linter-test-utils",
        "//verible/verilog/analysis:verilog-analyzer",
        "@googletest//:gtest",
        "@googletest//:gtest_main",
    ],
)

cc_library(
    name = "forbid-defparam-rule",
    srcs = ["forbid-defparam-rule.cc"],
    hdrs = ["forbid-defparam-rule.h"],
    deps = [
        "//verible/common/analysis:lint-rule-status",
        "//verible/common/analysis:syntax-tree-lint-rule",
        "//verible/common/analysis/matcher",
        "//verible/common/analysis/matcher:bound-symbol-manager",
        "//verible/common/text:concrete-syntax-leaf",
        "//verible/common/text:symbol",
        "//verible/common/text:syntax-tree-context",
        "//verible/common/text:tree-utils",
        "//verible/common/util:logging",
        "//verible/verilog/CST:verilog-matchers",  # fixdeps: keep
        "//verible/verilog/CST:verilog-nonterminals",
        "//verible/verilog/analysis:descriptions",
        "//verible/verilog/analysis:lint-rule-registry",
        "//verible/verilog/parser:verilog-token-enum",
    ],
    alwayslink = 1,
)

cc_test(
    name = "forbid-defparam-rule_test",
    srcs = ["forbid-defparam-rule_test.cc"],
    deps = [
        ":forbid-defparam-rule",
        "//verible/common/analysis:linter-test-utils",
        "//verible/common/analysis:syntax-tree-linter-test-utils",
        "//verible/verilog/analysis:verilog-analyzer",
        "//verible/verilog/parser:verilog-token-enum",
        "@googletest//:gtest",
        "@googletest//:gtest_main",
    ],
)

cc_library(
    name = "mismatched-labels-rule",
    srcs = ["mismatched-labels-rule.cc"],
    hdrs = ["mismatched-labels-rule.h"],
    deps = [
        "//verible/common/analysis:lint-rule-status",
        "//verible/common/analysis:syntax-tree-lint-rule",
        "//verible/common/analysis/matcher",
        "//verible/common/analysis/matcher:bound-symbol-manager",
        "//verible/common/text:concrete-syntax-tree",
        "//verible/common/text:symbol",
        "//verible/common/text:syntax-tree-context",
        "//verible/verilog/CST:seq-block",
        "//verible/verilog/CST:verilog-matchers",  # fixdeps: keep
        "//verible/verilog/analysis:descriptions",
        "//verible/verilog/analysis:lint-rule-registry",
    ],
    alwayslink = 1,
)

cc_test(
    name = "mismatched-labels-rule_test",
    srcs = ["mismatched-labels-rule_test.cc"],
    deps = [
        ":mismatched-labels-rule",
        "//verible/common/analysis:linter-test-utils",
        "//verible/common/analysis:syntax-tree-linter-test-utils",
        "//verible/verilog/analysis:verilog-analyzer",
        "//verible/verilog/parser:verilog-token-enum",
        "@googletest//:gtest",
        "@googletest//:gtest_main",
    ],
)

cc_library(
    name = "module-begin-block-rule",
    srcs = ["module-begin-block-rule.cc"],
    hdrs = ["module-begin-block-rule.h"],
    deps = [
        "//verible/common/analysis:lint-rule-status",
        "//verible/common/analysis:syntax-tree-lint-rule",
        "//verible/common/analysis/matcher",
        "//verible/common/analysis/matcher:bound-symbol-manager",
        "//verible/common/text:symbol",
        "//verible/common/text:syntax-tree-context",
        "//verible/verilog/CST:verilog-matchers",  # fixdeps: keep
        "//verible/verilog/analysis:descriptions",
        "//verible/verilog/analysis:lint-rule-registry",
    ],
    alwayslink = 1,
)

cc_test(
    name = "module-begin-block-rule_test",
    srcs = ["module-begin-block-rule_test.cc"],
    deps = [
        ":module-begin-block-rule",
        "//verible/common/analysis:linter-test-utils",
        "//verible/common/analysis:syntax-tree-linter-test-utils",
        "//verible/verilog/analysis:verilog-analyzer",
        "//verible/verilog/parser:verilog-token-enum",
        "@googletest//:gtest",
        "@googletest//:gtest_main",
    ],
)

cc_library(
    name = "module-filename-rule",
    srcs = ["module-filename-rule.cc"],
    hdrs = ["module-filename-rule.h"],
    deps = [
        "//verible/common/analysis:lint-rule-status",
        "//verible/common/analysis:syntax-tree-search",
        "//verible/common/analysis:text-structure-lint-rule",
        "//verible/common/text:concrete-syntax-leaf",
        "//verible/common/text:config-utils",
        "//verible/common/text:symbol",
        "//verible/common/text:text-structure",
        "//verible/common/text:token-info",
        "//verible/common/util:file-util",
        "//verible/common/util:logging",
        "//verible/verilog/CST:module",
        "//verible/verilog/CST:verilog-nonterminals",
        "//verible/verilog/analysis:descriptions",
        "//verible/verilog/analysis:lint-rule-registry",
        "@abseil-cpp//absl/status",
        "@abseil-cpp//absl/strings",
    ],
    alwayslink = 1,
)

cc_test(
    name = "module-filename-rule_test",
    srcs = ["module-filename-rule_test.cc"],
    deps = [
        ":module-filename-rule",
        "//verible/common/analysis:linter-test-utils",
        "//verible/common/analysis:text-structure-linter-test-utils",
        "//verible/verilog/analysis:verilog-analyzer",
        "//verible/verilog/parser:verilog-token-enum",
        "@googletest//:gtest",
        "@googletest//:gtest_main",
    ],
)

cc_library(
    name = "package-filename-rule",
    srcs = ["package-filename-rule.cc"],
    hdrs = ["package-filename-rule.h"],
    deps = [
        "//verible/common/analysis:lint-rule-status",
        "//verible/common/analysis:syntax-tree-search",
        "//verible/common/analysis:text-structure-lint-rule",
        "//verible/common/text:config-utils",
        "//verible/common/text:text-structure",
        "//verible/common/text:token-info",
        "//verible/common/util:file-util",
        "//verible/verilog/CST:package",
        "//verible/verilog/analysis:descriptions",
        "//verible/verilog/analysis:lint-rule-registry",
        "@abseil-cpp//absl/status",
        "@abseil-cpp//absl/strings",
    ],
    alwayslink = 1,
)

cc_test(
    name = "package-filename-rule_test",
    srcs = ["package-filename-rule_test.cc"],
    deps = [
        ":package-filename-rule",
        "//verible/common/analysis:linter-test-utils",
        "//verible/common/analysis:text-structure-linter-test-utils",
        "//verible/verilog/analysis:verilog-analyzer",
        "//verible/verilog/parser:verilog-token-enum",
        "@googletest//:gtest",
        "@googletest//:gtest_main",
    ],
)

cc_library(
    name = "module-instantiation-rules",
    srcs = ["module-instantiation-rules.cc"],
    hdrs = ["module-instantiation-rules.h"],
    deps = [
        "//verible/common/analysis:lint-rule-status",
        "//verible/common/analysis:syntax-tree-lint-rule",
        "//verible/common/analysis/matcher",
        "//verible/common/analysis/matcher:bound-symbol-manager",
        "//verible/common/text:concrete-syntax-leaf",
        "//verible/common/text:concrete-syntax-tree",
        "//verible/common/text:symbol",
        "//verible/common/text:syntax-tree-context",
        "//verible/common/text:token-info",
        "//verible/common/text:tree-utils",
        "//verible/common/util:casts",
        "//verible/common/util:logging",
        "//verible/verilog/CST:context-functions",
        "//verible/verilog/CST:verilog-matchers",
        "//verible/verilog/CST:verilog-nonterminals",
        "//verible/verilog/analysis:descriptions",
        "//verible/verilog/analysis:lint-rule-registry",
    ],
    alwayslink = 1,
)

cc_test(
    name = "module-instantiation-rules_test",
    srcs = ["module-instantiation-rules_test.cc"],
    deps = [
        ":module-instantiation-rules",
        "//verible/common/analysis:linter-test-utils",
        "//verible/common/analysis:syntax-tree-linter-test-utils",
        "//verible/verilog/analysis:verilog-analyzer",
        "//verible/verilog/parser:verilog-token-enum",
        "@googletest//:gtest",
        "@googletest//:gtest_main",
    ],
)

cc_library(
    name = "one-module-per-file-rule",
    srcs = ["one-module-per-file-rule.cc"],
    hdrs = ["one-module-per-file-rule.h"],
    deps = [
        "//verible/common/analysis:lint-rule-status",
        "//verible/common/analysis:syntax-tree-search",
        "//verible/common/analysis:text-structure-lint-rule",
        "//verible/common/text:text-structure",
        "//verible/common/util:logging",
        "//verible/verilog/CST:module",
        "//verible/verilog/CST:verilog-nonterminals",
        "//verible/verilog/analysis:descriptions",
        "//verible/verilog/analysis:lint-rule-registry",
        "@abseil-cpp//absl/strings",
    ],
    alwayslink = 1,
)

cc_test(
    name = "one-module-per-file-rule_test",
    srcs = ["one-module-per-file-rule_test.cc"],
    deps = [
        ":one-module-per-file-rule",
        "//verible/common/analysis:linter-test-utils",
        "//verible/common/analysis:text-structure-linter-test-utils",
        "//verible/verilog/analysis:verilog-analyzer",
        "//verible/verilog/parser:verilog-token-enum",
        "@googletest//:gtest",
        "@googletest//:gtest_main",
    ],
)

cc_library(
    name = "endif-comment-rule",
    srcs = ["endif-comment-rule.cc"],
    hdrs = ["endif-comment-rule.h"],
    deps = [
        "//verible/common/analysis:lint-rule-status",
        "//verible/common/analysis:token-stream-lint-rule",
        "//verible/common/strings:comment-utils",
        "//verible/common/text:token-info",
        "//verible/verilog/analysis:descriptions",
        "//verible/verilog/analysis:lint-rule-registry",
        "//verible/verilog/parser:verilog-token-enum",
        "@abseil-cpp//absl/strings",
    ],
    alwayslink = 1,
)

cc_test(
    name = "endif-comment-rule_test",
    srcs = ["endif-comment-rule_test.cc"],
    deps = [
        ":endif-comment-rule",
        "//verible/common/analysis:linter-test-utils",
        "//verible/common/analysis:token-stream-linter-test-utils",
        "//verible/verilog/analysis:verilog-analyzer",
        "//verible/verilog/parser:verilog-token-enum",
        "@googletest//:gtest",
        "@googletest//:gtest_main",
    ],
)

cc_library(
    name = "forbidden-anonymous-enums-rule",
    srcs = ["forbidden-anonymous-enums-rule.cc"],
    hdrs = ["forbidden-anonymous-enums-rule.h"],
    deps = [
        "//verible/common/analysis:lint-rule-status",
        "//verible/common/analysis:syntax-tree-lint-rule",
        "//verible/common/analysis/matcher",
        "//verible/common/analysis/matcher:bound-symbol-manager",
        "//verible/common/text:symbol",
        "//verible/common/text:syntax-tree-context",
        "//verible/verilog/CST:verilog-matchers",
        "//verible/verilog/CST:verilog-nonterminals",
        "//verible/verilog/analysis:descriptions",
        "//verible/verilog/analysis:lint-rule-registry",
    ],
    alwayslink = 1,
)

cc_test(
    name = "forbidden-anonymous-enums-rule_test",
    srcs = ["forbidden-anonymous-enums-rule_test.cc"],
    deps = [
        ":forbidden-anonymous-enums-rule",
        "//verible/common/analysis:linter-test-utils",
        "//verible/common/analysis:syntax-tree-linter-test-utils",
        "//verible/verilog/analysis:verilog-analyzer",
        "//verible/verilog/parser:verilog-token-enum",
        "@googletest//:gtest",
        "@googletest//:gtest_main",
    ],
)

cc_library(
    name = "enum-name-style-rule",
    srcs = ["enum-name-style-rule.cc"],
    hdrs = ["enum-name-style-rule.h"],
    deps = [
        "//verible/common/analysis:lint-rule-status",
        "//verible/common/analysis:syntax-tree-lint-rule",
        "//verible/common/analysis/matcher",
        "//verible/common/analysis/matcher:bound-symbol-manager",
        "//verible/common/text:config-utils",
        "//verible/common/text:symbol",
        "//verible/common/text:syntax-tree-context",
        "//verible/common/util:logging",
        "//verible/verilog/CST:type",
        "//verible/verilog/CST:verilog-matchers",
        "//verible/verilog/analysis:descriptions",
        "//verible/verilog/analysis:lint-rule-registry",
        "@abseil-cpp//absl/status",
        "@abseil-cpp//absl/strings",
        "@re2",
    ],
    alwayslink = 1,
)

cc_test(
    name = "enum-name-style-rule_test",
    srcs = ["enum-name-style-rule_test.cc"],
    deps = [
        ":enum-name-style-rule",
        "//verible/common/analysis:linter-test-utils",
        "//verible/common/analysis:syntax-tree-linter-test-utils",
        "//verible/verilog/analysis:verilog-analyzer",
        "//verible/verilog/parser:verilog-token-enum",
        "@googletest//:gtest",
        "@googletest//:gtest_main",
    ],
)

cc_library(
    name = "forbidden-macro-rule",
    srcs = ["forbidden-macro-rule.cc"],
    hdrs = ["forbidden-macro-rule.h"],
    deps = [
        "//verible/common/analysis:citation",
        "//verible/common/analysis:lint-rule-status",
        "//verible/common/analysis:syntax-tree-lint-rule",
        "//verible/common/analysis/matcher",
        "//verible/common/analysis/matcher:bound-symbol-manager",
        "//verible/common/text:concrete-syntax-leaf",
        "//verible/common/text:symbol",
        "//verible/common/text:syntax-tree-context",
        "//verible/common/text:token-info",
        "//verible/common/util:container-util",
        "//verible/verilog/CST:verilog-matchers",
        "//verible/verilog/analysis:descriptions",
        "//verible/verilog/analysis:lint-rule-registry",
    ],
    alwayslink = 1,
)

cc_library(
    name = "forbidden-anonymous-structs-unions-rule",
    srcs = ["forbidden-anonymous-structs-unions-rule.cc"],
    hdrs = ["forbidden-anonymous-structs-unions-rule.h"],
    deps = [
        "//verible/common/analysis:lint-rule-status",
        "//verible/common/analysis:syntax-tree-lint-rule",
        "//verible/common/analysis/matcher",
        "//verible/common/analysis/matcher:bound-symbol-manager",
        "//verible/common/text:config-utils",
        "//verible/common/text:symbol",
        "//verible/common/text:syntax-tree-context",
        "//verible/verilog/CST:verilog-matchers",
        "//verible/verilog/CST:verilog-nonterminals",
        "//verible/verilog/analysis:descriptions",
        "//verible/verilog/analysis:lint-rule-registry",
        "@abseil-cpp//absl/status",
    ],
    alwayslink = 1,
)

cc_test(
    name = "forbidden-anonymous-structs-unions-rule_test",
    srcs = ["forbidden-anonymous-structs-unions-rule_test.cc"],
    deps = [
        ":forbidden-anonymous-structs-unions-rule",
        "//verible/common/analysis:linter-test-utils",
        "//verible/common/analysis:syntax-tree-linter-test-utils",
        "//verible/verilog/analysis:verilog-analyzer",
        "//verible/verilog/parser:verilog-token-enum",
        "@abseil-cpp//absl/status",
        "@abseil-cpp//absl/strings",
        "@googletest//:gtest",
        "@googletest//:gtest_main",
    ],
)

cc_test(
    name = "forbidden-macro-rule_test",
    srcs = ["forbidden-macro-rule_test.cc"],
    deps = [
        ":forbidden-macro-rule",
        "//verible/common/analysis:linter-test-utils",
        "//verible/common/analysis:syntax-tree-linter-test-utils",
        "//verible/verilog/analysis:verilog-analyzer",
        "//verible/verilog/parser:verilog-token-enum",
        "@googletest//:gtest",
        "@googletest//:gtest_main",
    ],
)

cc_library(
    name = "forbidden-symbol-rule",
    srcs = ["forbidden-symbol-rule.cc"],
    hdrs = ["forbidden-symbol-rule.h"],
    deps = [
        "//verible/common/analysis:lint-rule-status",
        "//verible/common/analysis:syntax-tree-lint-rule",
        "//verible/common/analysis/matcher",
        "//verible/common/analysis/matcher:bound-symbol-manager",
        "//verible/common/text:concrete-syntax-leaf",
        "//verible/common/text:symbol",
        "//verible/common/text:syntax-tree-context",
        "//verible/common/text:token-info",
        "//verible/common/util:container-util",
        "//verible/verilog/CST:verilog-matchers",
        "//verible/verilog/analysis:descriptions",
        "//verible/verilog/analysis:lint-rule-registry",
    ],
    alwayslink = 1,
)

cc_test(
    name = "forbidden-symbol-rule_test",
    srcs = ["forbidden-symbol-rule_test.cc"],
    deps = [
        ":forbidden-symbol-rule",
        "//verible/common/analysis:linter-test-utils",
        "//verible/common/analysis:syntax-tree-linter-test-utils",
        "//verible/verilog/analysis:verilog-analyzer",
        "//verible/verilog/parser:verilog-token-enum",
        "@googletest//:gtest",
        "@googletest//:gtest_main",
    ],
)

cc_library(
    name = "generate-label-rule",
    srcs = ["generate-label-rule.cc"],
    hdrs = ["generate-label-rule.h"],
    deps = [
        "//verible/common/analysis:lint-rule-status",
        "//verible/common/analysis:syntax-tree-lint-rule",
        "//verible/common/analysis/matcher",
        "//verible/common/analysis/matcher:bound-symbol-manager",
        "//verible/common/analysis/matcher:core-matchers",
        "//verible/common/text:symbol",
        "//verible/common/text:syntax-tree-context",
        "//verible/verilog/CST:verilog-matchers",
        "//verible/verilog/analysis:descriptions",
        "//verible/verilog/analysis:lint-rule-registry",
    ],
    alwayslink = 1,
)

cc_test(
    name = "generate-label-rule_test",
    srcs = ["generate-label-rule_test.cc"],
    deps = [
        ":generate-label-rule",
        "//verible/common/analysis:linter-test-utils",
        "//verible/common/analysis:syntax-tree-linter-test-utils",
        "//verible/verilog/analysis:verilog-analyzer",
        "//verible/verilog/parser:verilog-token-enum",
        "@googletest//:gtest",
        "@googletest//:gtest_main",
    ],
)

cc_library(
    name = "generate-label-prefix-rule",
    srcs = ["generate-label-prefix-rule.cc"],
    hdrs = ["generate-label-prefix-rule.h"],
    deps = [
        "//verible/common/analysis:lint-rule-status",
        "//verible/common/analysis:syntax-tree-lint-rule",
        "//verible/common/analysis/matcher",
        "//verible/common/analysis/matcher:bound-symbol-manager",
        "//verible/common/text:symbol",
        "//verible/common/text:syntax-tree-context",
        "//verible/common/text:token-info",
        "//verible/common/text:tree-utils",
        "//verible/verilog/CST:seq-block",
        "//verible/verilog/CST:verilog-matchers",
        "//verible/verilog/CST:verilog-nonterminals",
        "//verible/verilog/analysis:descriptions",
        "//verible/verilog/analysis:lint-rule-registry",
        "@abseil-cpp//absl/strings",
    ],
    alwayslink = 1,
)

cc_test(
    name = "generate-label-prefix-rule_test",
    srcs = ["generate-label-prefix-rule_test.cc"],
    deps = [
        ":generate-label-prefix-rule",
        "//verible/common/analysis:linter-test-utils",
        "//verible/common/analysis:syntax-tree-linter-test-utils",
        "//verible/verilog/analysis:verilog-analyzer",
        "//verible/verilog/parser:verilog-token-enum",
        "@googletest//:gtest",
        "@googletest//:gtest_main",
    ],
)

cc_library(
    name = "v2001-generate-begin-rule",
    srcs = ["v2001-generate-begin-rule.cc"],
    hdrs = ["v2001-generate-begin-rule.h"],
    deps = [
        "//verible/common/analysis:lint-rule-status",
        "//verible/common/analysis:syntax-tree-lint-rule",
        "//verible/common/analysis/matcher",
        "//verible/common/analysis/matcher:bound-symbol-manager",
        "//verible/common/text:concrete-syntax-leaf",
        "//verible/common/text:concrete-syntax-tree",
        "//verible/common/text:symbol",
        "//verible/common/text:syntax-tree-context",
        "//verible/common/text:tree-utils",
        "//verible/verilog/CST:verilog-matchers",
        "//verible/verilog/analysis:descriptions",
        "//verible/verilog/analysis:lint-rule-registry",
    ],
    alwayslink = 1,
)

cc_test(
    name = "v2001-generate-begin-rule_test",
    srcs = ["v2001-generate-begin-rule_test.cc"],
    deps = [
        ":forbidden-symbol-rule",
        ":v2001-generate-begin-rule",
        "//verible/common/analysis:linter-test-utils",
        "//verible/common/analysis:syntax-tree-linter-test-utils",
        "//verible/verilog/analysis:verilog-analyzer",
        "//verible/verilog/parser:verilog-token-enum",
        "@googletest//:gtest",
        "@googletest//:gtest_main",
    ],
)

cc_library(
    name = "void-cast-rule",
    srcs = ["void-cast-rule.cc"],
    hdrs = ["void-cast-rule.h"],
    deps = [
        "//verible/common/analysis:lint-rule-status",
        "//verible/common/analysis:syntax-tree-lint-rule",
        "//verible/common/analysis/matcher",
        "//verible/common/analysis/matcher:bound-symbol-manager",
        "//verible/common/analysis/matcher:core-matchers",
        "//verible/common/text:concrete-syntax-leaf",
        "//verible/common/text:concrete-syntax-tree",
        "//verible/common/text:symbol",
        "//verible/common/text:syntax-tree-context",
        "//verible/common/text:token-info",
        "//verible/common/text:tree-utils",
        "//verible/common/util:logging",
        "//verible/verilog/CST:verilog-matchers",
        "//verible/verilog/analysis:descriptions",
        "//verible/verilog/analysis:lint-rule-registry",
    ],
    alwayslink = 1,
)

cc_test(
    name = "void-cast-rule_test",
    srcs = ["void-cast-rule_test.cc"],
    deps = [
        ":void-cast-rule",
        "//verible/common/analysis:linter-test-utils",
        "//verible/common/analysis:syntax-tree-linter-test-utils",
        "//verible/verilog/analysis:verilog-analyzer",
        "//verible/verilog/parser:verilog-token-enum",
        "@googletest//:gtest",
        "@googletest//:gtest_main",
    ],
)

cc_library(
    name = "undersized-binary-literal-rule",
    srcs = ["undersized-binary-literal-rule.cc"],
    hdrs = ["undersized-binary-literal-rule.h"],
    deps = [
        "//verible/common/analysis:lint-rule-status",
        "//verible/common/analysis:syntax-tree-lint-rule",
        "//verible/common/analysis/matcher",
        "//verible/common/analysis/matcher:bound-symbol-manager",
        "//verible/common/text:concrete-syntax-leaf",
        "//verible/common/text:concrete-syntax-tree",
        "//verible/common/text:config-utils",
        "//verible/common/text:symbol",
        "//verible/common/text:syntax-tree-context",
        "//verible/common/text:token-info",
        "//verible/common/util:logging",
        "//verible/verilog/CST:numbers",
        "//verible/verilog/CST:verilog-matchers",
        "//verible/verilog/analysis:descriptions",
        "//verible/verilog/analysis:lint-rule-registry",
        "@abseil-cpp//absl/status",
        "@abseil-cpp//absl/strings",
    ],
    alwayslink = 1,
)

cc_test(
    name = "undersized-binary-literal-rule_test",
    srcs = ["undersized-binary-literal-rule_test.cc"],
    deps = [
        ":undersized-binary-literal-rule",
        "//verible/common/analysis:linter-test-utils",
        "//verible/common/analysis:syntax-tree-linter-test-utils",
        "//verible/verilog/analysis:verilog-analyzer",
        "//verible/verilog/parser:verilog-token-enum",
        "@abseil-cpp//absl/status",
        "@googletest//:gtest",
        "@googletest//:gtest_main",
    ],
)

cc_library(
    name = "truncated-numeric-literal-rule",
    srcs = ["truncated-numeric-literal-rule.cc"],
    hdrs = ["truncated-numeric-literal-rule.h"],
    deps = [
        "//verible/common/analysis:lint-rule-status",
        "//verible/common/analysis:syntax-tree-lint-rule",
        "//verible/common/analysis/matcher",
        "//verible/common/analysis/matcher:bound-symbol-manager",
        "//verible/common/text:concrete-syntax-leaf",
        "//verible/common/text:concrete-syntax-tree",
        "//verible/common/text:symbol",
        "//verible/common/text:syntax-tree-context",
        "//verible/common/text:token-info",
        "//verible/verilog/CST:numbers",
        "//verible/verilog/CST:verilog-matchers",
        "//verible/verilog/analysis:descriptions",
        "//verible/verilog/analysis:lint-rule-registry",
        "@abseil-cpp//absl/numeric:int128",
        "@abseil-cpp//absl/strings",
    ],
    alwayslink = 1,
)

cc_test(
    name = "truncated-numeric-literal-rule_test",
    srcs = ["truncated-numeric-literal-rule_test.cc"],
    deps = [
        ":truncated-numeric-literal-rule",
        "//verible/common/analysis:linter-test-utils",
        "//verible/common/analysis:syntax-tree-linter-test-utils",
        "//verible/verilog/analysis:verilog-analyzer",
        "//verible/verilog/parser:verilog-token-enum",
        "@abseil-cpp//absl/strings",
        "@googletest//:gtest",
        "@googletest//:gtest_main",
    ],
)

cc_library(
    name = "create-object-name-match-rule",
    srcs = ["create-object-name-match-rule.cc"],
    hdrs = ["create-object-name-match-rule.h"],
    deps = [
        "//verible/common/analysis:lint-rule-status",
        "//verible/common/analysis:syntax-tree-lint-rule",
        "//verible/common/analysis/matcher",
        "//verible/common/analysis/matcher:bound-symbol-manager",
        "//verible/common/text:concrete-syntax-leaf",
        "//verible/common/text:concrete-syntax-tree",
        "//verible/common/text:symbol",
        "//verible/common/text:syntax-tree-context",
        "//verible/common/text:token-info",
        "//verible/common/util:casts",
        "//verible/verilog/CST:expression",
        "//verible/verilog/CST:verilog-matchers",
        "//verible/verilog/CST:verilog-nonterminals",
        "//verible/verilog/analysis:descriptions",
        "//verible/verilog/analysis:lint-rule-registry",
        "//verible/verilog/parser:verilog-token-enum",
        "@abseil-cpp//absl/strings",
    ],
    alwayslink = 1,
)

cc_test(
    name = "create-object-name-match-rule_test",
    srcs = ["create-object-name-match-rule_test.cc"],
    deps = [
        ":create-object-name-match-rule",
        "//verible/common/analysis:linter-test-utils",
        "//verible/common/analysis:syntax-tree-linter-test-utils",
        "//verible/verilog/analysis:verilog-analyzer",
        "//verible/verilog/parser:verilog-token-enum",
        "@googletest//:gtest",
        "@googletest//:gtest_main",
    ],
)

cc_library(
    name = "always-comb-rule",
    srcs = ["always-comb-rule.cc"],
    hdrs = ["always-comb-rule.h"],
    deps = [
        "//verible/common/analysis:lint-rule-status",
        "//verible/common/analysis:syntax-tree-lint-rule",
        "//verible/common/analysis/matcher",
        "//verible/common/analysis/matcher:bound-symbol-manager",
        "//verible/common/text:concrete-syntax-leaf",
        "//verible/common/text:concrete-syntax-tree",
        "//verible/common/text:symbol",
        "//verible/common/text:syntax-tree-context",
        "//verible/common/text:tree-utils",
        "//verible/verilog/CST:statement",
        "//verible/verilog/CST:verilog-matchers",
        "//verible/verilog/analysis:descriptions",
        "//verible/verilog/analysis:lint-rule-registry",
    ],
    alwayslink = 1,
)

cc_test(
    name = "always-comb-rule_test",
    srcs = ["always-comb-rule_test.cc"],
    deps = [
        ":always-comb-rule",
        "//verible/common/analysis:linter-test-utils",
        "//verible/common/analysis:syntax-tree-linter-test-utils",
        "//verible/verilog/analysis:verilog-analyzer",
        "//verible/verilog/parser:verilog-token-enum",
        "@googletest//:gtest",
        "@googletest//:gtest_main",
    ],
)

cc_library(
    name = "forbid-line-continuations-rule",
    srcs = ["forbid-line-continuations-rule.cc"],
    hdrs = ["forbid-line-continuations-rule.h"],
    deps = [
        "//verible/common/analysis:lint-rule-status",
        "//verible/common/analysis:syntax-tree-lint-rule",
        "//verible/common/analysis/matcher",
        "//verible/common/analysis/matcher:bound-symbol-manager",
        "//verible/common/text:symbol",
        "//verible/common/text:syntax-tree-context",
        "//verible/common/text:tree-utils",
        "//verible/verilog/CST:verilog-matchers",
        "//verible/verilog/analysis:descriptions",
        "//verible/verilog/analysis:lint-rule-registry",
        "//verible/verilog/parser:verilog-token-enum",
        "@abseil-cpp//absl/strings",
    ],
    alwayslink = 1,
)

cc_test(
    name = "forbid-line-continuations-rule_test",
    srcs = ["forbid-line-continuations-rule_test.cc"],
    deps = [
        ":forbid-line-continuations-rule",
        "//verible/common/analysis:linter-test-utils",
        "//verible/common/analysis:syntax-tree-linter-test-utils",
        "//verible/verilog/analysis:verilog-analyzer",
        "//verible/verilog/parser:verilog-token-enum",
        "@googletest//:gtest",
        "@googletest//:gtest_main",
    ],
)

cc_library(
    name = "suggest-parentheses-rule",
    srcs = ["suggest-parentheses-rule.cc"],
    hdrs = ["suggest-parentheses-rule.h"],
    deps = [
        "//verible/common/analysis:lint-rule-status",
        "//verible/common/analysis:syntax-tree-lint-rule",
        "//verible/common/text:concrete-syntax-tree",
        "//verible/common/text:symbol",
        "//verible/common/text:syntax-tree-context",
        "//verible/common/text:token-info",
        "//verible/common/text:tree-utils",
        "//verible/verilog/CST:expression",
        "//verible/verilog/CST:verilog-nonterminals",
        "//verible/verilog/analysis:descriptions",
        "//verible/verilog/analysis:lint-rule-registry",
        "//verible/verilog/parser:verilog-token-enum",
    ],
    alwayslink = 1,
)

cc_test(
    name = "suggest-parentheses-rule_test",
    srcs = ["suggest-parentheses-rule_test.cc"],
    deps = [
        ":suggest-parentheses-rule",
        "//verible/common/analysis:linter-test-utils",
        "//verible/common/analysis:syntax-tree-linter-test-utils",
        "//verible/verilog/analysis:verilog-analyzer",
        "@googletest//:gtest",
        "@googletest//:gtest_main",
    ],
)

cc_library(
    name = "always-comb-blocking-rule",
    srcs = ["always-comb-blocking-rule.cc"],
    hdrs = ["always-comb-blocking-rule.h"],
    deps = [
        "//verible/common/analysis:lint-rule-status",
        "//verible/common/analysis:syntax-tree-lint-rule",
        "//verible/common/analysis:syntax-tree-search",
        "//verible/common/analysis/matcher",
        "//verible/common/analysis/matcher:bound-symbol-manager",
        "//verible/common/text:concrete-syntax-leaf",
        "//verible/common/text:concrete-syntax-tree",
        "//verible/common/text:symbol",
        "//verible/common/text:syntax-tree-context",
        "//verible/common/text:tree-utils",
        "//verible/common/util:casts",
        "//verible/verilog/CST:verilog-matchers",
        "//verible/verilog/CST:verilog-nonterminals",
        "//verible/verilog/analysis:descriptions",
        "//verible/verilog/analysis:lint-rule-registry",
        "//verible/verilog/parser:verilog-token-enum",
    ],
    alwayslink = 1,
)

cc_test(
    name = "always-comb-blocking-rule_test",
    srcs = ["always-comb-blocking-rule_test.cc"],
    deps = [
        ":always-comb-blocking-rule",
        "//verible/common/analysis:linter-test-utils",
        "//verible/common/analysis:syntax-tree-linter-test-utils",
        "//verible/verilog/analysis:verilog-analyzer",
        "//verible/verilog/parser:verilog-token-enum",
        "@googletest//:gtest",
        "@googletest//:gtest_main",
    ],
)

cc_library(
    name = "always-ff-non-blocking-rule",
    srcs = ["always-ff-non-blocking-rule.cc"],
    hdrs = ["always-ff-non-blocking-rule.h"],
    deps = [
        "//verible/common/analysis:lint-rule-status",
        "//verible/common/analysis:syntax-tree-lint-rule",
        "//verible/common/analysis:syntax-tree-search",
        "//verible/common/analysis/matcher",
        "//verible/common/analysis/matcher:bound-symbol-manager",
        "//verible/common/text:concrete-syntax-leaf",
        "//verible/common/text:concrete-syntax-tree",
        "//verible/common/text:config-utils",
        "//verible/common/text:symbol",
        "//verible/common/text:syntax-tree-context",
        "//verible/common/util:casts",
        "//verible/common/util:logging",
        "//verible/verilog/CST:verilog-matchers",
        "//verible/verilog/CST:verilog-nonterminals",
        "//verible/verilog/analysis:descriptions",
        "//verible/verilog/analysis:lint-rule-registry",
        "@abseil-cpp//absl/status",
    ],
    alwayslink = 1,
)

cc_test(
    name = "always-ff-non-blocking-rule_test",
    srcs = ["always-ff-non-blocking-rule_test.cc"],
    deps = [
        ":always-ff-non-blocking-rule",
        "//verible/common/analysis:linter-test-utils",
        "//verible/common/analysis:syntax-tree-linter-test-utils",
        "//verible/verilog/analysis:verilog-analyzer",
        "//verible/verilog/parser:verilog-token-enum",
        "@googletest//:gtest",
        "@googletest//:gtest_main",
    ],
)

cc_library(
    name = "explicit-begin-rule",
    srcs = ["explicit-begin-rule.cc"],
    hdrs = ["explicit-begin-rule.h"],
    deps = [
        "//verible/common/analysis:lint-rule-status",
        "//verible/common/analysis:token-stream-lint-rule",
        "//verible/common/text:config-utils",
        "//verible/common/text:token-info",
        "//verible/verilog/analysis:descriptions",
        "//verible/verilog/analysis:lint-rule-registry",
        "//verible/verilog/parser:verilog-token-enum",
        "@abseil-cpp//absl/base:core_headers",
        "@abseil-cpp//absl/status",
        "@abseil-cpp//absl/strings",
    ],
    alwayslink = 1,
)

cc_test(
    name = "explicit-begin-rule_test",
    srcs = ["explicit-begin-rule_test.cc"],
    deps = [
        ":explicit-begin-rule",
        "//verible/common/analysis:linter-test-utils",
        "//verible/common/analysis:token-stream-linter-test-utils",
        "//verible/verilog/analysis:verilog-analyzer",
        "//verible/verilog/parser:verilog-token-enum",
        "@googletest//:gtest",
        "@googletest//:gtest_main",
    ],
)

cc_library(
    name = "explicit-function-lifetime-rule",
    srcs = ["explicit-function-lifetime-rule.cc"],
    hdrs = ["explicit-function-lifetime-rule.h"],
    deps = [
        "//verible/common/analysis:lint-rule-status",
        "//verible/common/analysis:syntax-tree-lint-rule",
        "//verible/common/analysis/matcher",
        "//verible/common/analysis/matcher:bound-symbol-manager",
        "//verible/common/text:symbol",
        "//verible/common/text:syntax-tree-context",
        "//verible/common/text:token-info",
        "//verible/common/text:tree-utils",
        "//verible/common/util:logging",
        "//verible/verilog/CST:context-functions",
        "//verible/verilog/CST:functions",
        "//verible/verilog/CST:identifier",
        "//verible/verilog/CST:verilog-matchers",
        "//verible/verilog/analysis:descriptions",
        "//verible/verilog/analysis:lint-rule-registry",
        "//verible/verilog/parser:verilog-token-enum",
    ],
    alwayslink = 1,
)

cc_test(
    name = "explicit-function-lifetime-rule_test",
    srcs = ["explicit-function-lifetime-rule_test.cc"],
    deps = [
        ":explicit-function-lifetime-rule",
        "//verible/common/analysis:linter-test-utils",
        "//verible/common/analysis:syntax-tree-linter-test-utils",
        "//verible/verilog/analysis:verilog-analyzer",
        "//verible/verilog/parser:verilog-token-enum",
        "@googletest//:gtest",
        "@googletest//:gtest_main",
    ],
)

cc_library(
    name = "explicit-task-lifetime-rule",
    srcs = ["explicit-task-lifetime-rule.cc"],
    hdrs = ["explicit-task-lifetime-rule.h"],
    deps = [
        "//verible/common/analysis:lint-rule-status",
        "//verible/common/analysis:syntax-tree-lint-rule",
        "//verible/common/analysis/matcher",
        "//verible/common/analysis/matcher:bound-symbol-manager",
        "//verible/common/text:symbol",
        "//verible/common/text:syntax-tree-context",
        "//verible/common/text:token-info",
        "//verible/common/text:tree-utils",
        "//verible/verilog/CST:context-functions",
        "//verible/verilog/CST:identifier",
        "//verible/verilog/CST:tasks",
        "//verible/verilog/CST:verilog-matchers",
        "//verible/verilog/analysis:descriptions",
        "//verible/verilog/analysis:lint-rule-registry",
        "//verible/verilog/parser:verilog-token-enum",
    ],
    alwayslink = 1,
)

cc_test(
    name = "explicit-task-lifetime-rule_test",
    srcs = ["explicit-task-lifetime-rule_test.cc"],
    deps = [
        ":explicit-task-lifetime-rule",
        "//verible/common/analysis:linter-test-utils",
        "//verible/common/analysis:syntax-tree-linter-test-utils",
        "//verible/verilog/analysis:verilog-analyzer",
        "//verible/verilog/parser:verilog-token-enum",
        "@googletest//:gtest",
        "@googletest//:gtest_main",
    ],
)

cc_library(
    name = "packed-dimensions-rule",
    srcs = ["packed-dimensions-rule.cc"],
    hdrs = ["packed-dimensions-rule.h"],
    deps = [
        "//verible/common/analysis:lint-rule-status",
        "//verible/common/analysis:syntax-tree-lint-rule",
        "//verible/common/analysis/matcher",
        "//verible/common/analysis/matcher:bound-symbol-manager",
        "//verible/common/text:symbol",
        "//verible/common/text:syntax-tree-context",
        "//verible/common/text:token-info",
        "//verible/common/text:tree-utils",
        "//verible/common/util:logging",
        "//verible/verilog/CST:context-functions",
        "//verible/verilog/CST:dimensions",
        "//verible/verilog/CST:expression",
        "//verible/verilog/CST:verilog-matchers",
        "//verible/verilog/analysis:descriptions",
        "//verible/verilog/analysis:lint-rule-registry",
        "//verible/verilog/parser:verilog-token-enum",
    ],
    alwayslink = 1,
)

cc_test(
    name = "packed-dimensions-rule_test",
    srcs = ["packed-dimensions-rule_test.cc"],
    deps = [
        ":packed-dimensions-rule",
        "//verible/common/analysis:linter-test-utils",
        "//verible/common/analysis:syntax-tree-linter-test-utils",
        "//verible/verilog/analysis:verilog-analyzer",
        "//verible/verilog/parser:verilog-token-enum",
        "@googletest//:gtest",
        "@googletest//:gtest_main",
    ],
)

cc_library(
    name = "unpacked-dimensions-rule",
    srcs = ["unpacked-dimensions-rule.cc"],
    hdrs = ["unpacked-dimensions-rule.h"],
    deps = [
        "//verible/common/analysis:lint-rule-status",
        "//verible/common/analysis:syntax-tree-lint-rule",
        "//verible/common/analysis/matcher",
        "//verible/common/analysis/matcher:bound-symbol-manager",
        "//verible/common/text:symbol",
        "//verible/common/text:syntax-tree-context",
        "//verible/common/text:token-info",
        "//verible/common/text:tree-utils",
        "//verible/common/util:logging",
        "//verible/verilog/CST:context-functions",
        "//verible/verilog/CST:dimensions",
        "//verible/verilog/CST:expression",
        "//verible/verilog/CST:verilog-matchers",
        "//verible/verilog/CST:verilog-nonterminals",
        "//verible/verilog/analysis:descriptions",
        "//verible/verilog/analysis:lint-rule-registry",
        "//verible/verilog/parser:verilog-token-enum",
    ],
    alwayslink = 1,
)

cc_test(
    name = "unpacked-dimensions-rule_test",
    srcs = ["unpacked-dimensions-rule_test.cc"],
    deps = [
        ":unpacked-dimensions-rule",
        "//verible/common/analysis:linter-test-utils",
        "//verible/common/analysis:syntax-tree-linter-test-utils",
        "//verible/verilog/analysis:verilog-analyzer",
        "//verible/verilog/parser:verilog-token-enum",
        "@googletest//:gtest",
        "@googletest//:gtest_main",
    ],
)

cc_library(
    name = "forbid-negative-array-dim",
    srcs = ["forbid-negative-array-dim.cc"],
    hdrs = ["forbid-negative-array-dim.h"],
    deps = [
        "//verible/common/analysis:lint-rule-status",
        "//verible/common/analysis:syntax-tree-lint-rule",
        "//verible/common/analysis/matcher",
        "//verible/common/analysis/matcher:bound-symbol-manager",
        "//verible/common/text:symbol",
        "//verible/common/text:syntax-tree-context",
        "//verible/common/text:token-info",
        "//verible/common/text:tree-utils",
        "//verible/verilog/CST:expression",
        "//verible/verilog/CST:verilog-matchers",
        "//verible/verilog/CST:verilog-nonterminals",
        "//verible/verilog/analysis:descriptions",
        "//verible/verilog/analysis:lint-rule-registry",
        "//verible/verilog/parser:verilog-token-enum",
    ],
    alwayslink = 1,
)

cc_test(
    name = "forbid-negative-array-dim_test",
    srcs = ["forbid-negative-array-dim_test.cc"],
    deps = [
        ":forbid-negative-array-dim",
        "//verible/common/analysis:linter-test-utils",
        "//verible/common/analysis:syntax-tree-linter-test-utils",
        "//verible/verilog/analysis:verilog-analyzer",
        "//verible/verilog/parser:verilog-token-enum",
        "@googletest//:gtest",
        "@googletest//:gtest_main",
    ],
)

cc_library(
    name = "case-missing-default-rule",
    srcs = ["case-missing-default-rule.cc"],
    hdrs = ["case-missing-default-rule.h"],
    deps = [
        "//verible/common/analysis:lint-rule-status",
        "//verible/common/analysis:syntax-tree-lint-rule",
        "//verible/common/analysis/matcher",
        "//verible/common/analysis/matcher:bound-symbol-manager",
        "//verible/common/analysis/matcher:matcher-builders",
        "//verible/common/text:concrete-syntax-tree",
        "//verible/common/text:symbol",
        "//verible/common/text:syntax-tree-context",
        "//verible/common/text:tree-utils",
        "//verible/verilog/CST:verilog-matchers",
        "//verible/verilog/CST:verilog-nonterminals",
        "//verible/verilog/analysis:descriptions",
        "//verible/verilog/analysis:lint-rule-registry",
    ],
    alwayslink = 1,
)

cc_test(
    name = "case-missing-default-rule_test",
    srcs = ["case-missing-default-rule_test.cc"],
    deps = [
        ":case-missing-default-rule",
        "//verible/common/analysis:linter-test-utils",
        "//verible/common/analysis:syntax-tree-linter-test-utils",
        "//verible/verilog/analysis:verilog-analyzer",
        "//verible/verilog/parser:verilog-token-enum",
        "@googletest//:gtest",
        "@googletest//:gtest_main",
    ],
)

cc_library(
    name = "constraint-name-style-rule",
    srcs = ["constraint-name-style-rule.cc"],
    hdrs = ["constraint-name-style-rule.h"],
    deps = [
        "//verible/common/analysis:lint-rule-status",
        "//verible/common/analysis:syntax-tree-lint-rule",
        "//verible/common/analysis/matcher",
        "//verible/common/analysis/matcher:bound-symbol-manager",
        "//verible/common/text:config-utils",
        "//verible/common/text:symbol",
        "//verible/common/text:syntax-tree-context",
        "//verible/common/text:token-info",
        "//verible/verilog/CST:constraints",
        "//verible/verilog/CST:verilog-matchers",
        "//verible/verilog/analysis:descriptions",
        "//verible/verilog/analysis:lint-rule-registry",
        "@abseil-cpp//absl/status",
        "@abseil-cpp//absl/strings",
        "@re2",
    ],
    alwayslink = 1,
)

cc_test(
    name = "constraint-name-style-rule_test",
    srcs = ["constraint-name-style-rule_test.cc"],
    deps = [
        ":constraint-name-style-rule",
        "//verible/common/analysis:linter-test-utils",
        "//verible/common/analysis:syntax-tree-linter-test-utils",
        "//verible/verilog/analysis:verilog-analyzer",
        "//verible/verilog/parser:verilog-token-enum",
        "@googletest//:gtest",
        "@googletest//:gtest_main",
    ],
)

cc_library(
    name = "disable-statement-rule",
    srcs = ["disable-statement-rule.cc"],
    hdrs = ["disable-statement-rule.h"],
    deps = [
        "//verible/common/analysis:lint-rule-status",
        "//verible/common/analysis:syntax-tree-lint-rule",
        "//verible/common/analysis/matcher",
        "//verible/common/analysis/matcher:bound-symbol-manager",
        "//verible/common/text:symbol",
        "//verible/common/text:syntax-tree-context",
        "//verible/common/text:tree-utils",
        "//verible/common/util:iterator-adaptors",
        "//verible/verilog/CST:identifier",
        "//verible/verilog/CST:verilog-matchers",
        "//verible/verilog/CST:verilog-nonterminals",
        "//verible/verilog/analysis:descriptions",
        "//verible/verilog/analysis:lint-rule-registry",
    ],
    alwayslink = 1,
)

cc_test(
    name = "disable-statement-rule_test",
    srcs = ["disable-statement-rule_test.cc"],
    deps = [
        ":disable-statement-rule",
        "//verible/common/analysis:linter-test-utils",
        "//verible/common/analysis:syntax-tree-linter-test-utils",
        "//verible/verilog/analysis:verilog-analyzer",
        "//verible/verilog/parser:verilog-token-enum",
        "@googletest//:gtest",
        "@googletest//:gtest_main",
    ],
)

cc_library(
    name = "macro-name-style-rule",
    srcs = ["macro-name-style-rule.cc"],
    hdrs = ["macro-name-style-rule.h"],
    deps = [
        "//verible/common/analysis:lint-rule-status",
        "//verible/common/analysis:token-stream-lint-rule",
        "//verible/common/text:config-utils",
        "//verible/common/text:token-info",
        "//verible/verilog/analysis:descriptions",
        "//verible/verilog/analysis:lint-rule-registry",
        "//verible/verilog/parser:verilog-lexer",
        "//verible/verilog/parser:verilog-token-classifications",
        "//verible/verilog/parser:verilog-token-enum",
        "@abseil-cpp//absl/status",
        "@abseil-cpp//absl/strings",
        "@re2",
    ],
    alwayslink = 1,
)

cc_test(
    name = "macro-name-style-rule_test",
    srcs = ["macro-name-style-rule_test.cc"],
    deps = [
        ":macro-name-style-rule",
        "//verible/common/analysis:linter-test-utils",
        "//verible/common/analysis:token-stream-linter-test-utils",
        "//verible/verilog/analysis:verilog-analyzer",
        "//verible/verilog/parser:verilog-token-enum",
        "@googletest//:gtest",
        "@googletest//:gtest_main",
    ],
)

cc_library(
    name = "macro-string-concatenation-rule",
    srcs = ["macro-string-concatenation-rule.cc"],
    hdrs = ["macro-string-concatenation-rule.h"],
    deps = [
        "//verible/common/analysis:lint-rule-status",
        "//verible/common/analysis:token-stream-lint-rule",
        "//verible/common/text:token-info",
        "//verible/common/util:value-saver",
        "//verible/verilog/analysis:descriptions",
        "//verible/verilog/analysis:lint-rule-registry",
        "//verible/verilog/parser:verilog-lexer",
        "//verible/verilog/parser:verilog-token-classifications",
        "//verible/verilog/parser:verilog-token-enum",
    ],
    alwayslink = 1,
)

cc_test(
    name = "macro-string-concatenation-rule_test",
    srcs = ["macro-string-concatenation-rule_test.cc"],
    deps = [
        ":macro-string-concatenation-rule",
        "//verible/common/analysis:linter-test-utils",
        "//verible/common/analysis:token-stream-linter-test-utils",
        "//verible/verilog/analysis:verilog-analyzer",
        "//verible/verilog/parser:verilog-token-enum",
        "@googletest//:gtest",
        "@googletest//:gtest_main",
    ],
)

cc_library(
    name = "numeric-format-string-style-rule",
    srcs = ["numeric-format-string-style-rule.cc"],
    hdrs = ["numeric-format-string-style-rule.h"],
    deps = [
        "//verible/common/analysis:lint-rule-status",
        "//verible/common/analysis:token-stream-lint-rule",
        "//verible/common/text:token-info",
        "//verible/verilog/analysis:descriptions",
        "//verible/verilog/analysis:lint-rule-registry",
        "//verible/verilog/parser:verilog-lexer",
        "//verible/verilog/parser:verilog-token-classifications",
        "//verible/verilog/parser:verilog-token-enum",
        "@abseil-cpp//absl/strings",
    ],
    alwayslink = 1,
)

cc_test(
    name = "numeric-format-string-style-rule_test",
    srcs = ["numeric-format-string-style-rule_test.cc"],
    deps = [
        ":numeric-format-string-style-rule",
        "//verible/common/analysis:linter-test-utils",
        "//verible/common/analysis:token-stream-linter-test-utils",
        "//verible/verilog/analysis:verilog-analyzer",
        "//verible/verilog/parser:verilog-token-enum",
        "@googletest//:gtest",
        "@googletest//:gtest_main",
    ],
)

cc_library(
    name = "parameter-name-style-rule",
    srcs = ["parameter-name-style-rule.cc"],
    hdrs = ["parameter-name-style-rule.h"],
    deps = [
        "//verible/common/analysis:lint-rule-status",
        "//verible/common/analysis:syntax-tree-lint-rule",
        "//verible/common/analysis/matcher",
        "//verible/common/analysis/matcher:bound-symbol-manager",
        "//verible/common/text:config-utils",
        "//verible/common/text:symbol",
        "//verible/common/text:syntax-tree-context",
        "//verible/common/text:token-info",
        "//verible/verilog/CST:parameters",
        "//verible/verilog/CST:verilog-matchers",
        "//verible/verilog/analysis:descriptions",
        "//verible/verilog/analysis:lint-rule-registry",
        "//verible/verilog/parser:verilog-token-enum",
        "@abseil-cpp//absl/status",
        "@abseil-cpp//absl/strings",
        "@re2",
    ],
    alwayslink = 1,
)

cc_test(
    name = "parameter-name-style-rule_test",
    srcs = ["parameter-name-style-rule_test.cc"],
    deps = [
        ":parameter-name-style-rule",
        "//verible/common/analysis:linter-test-utils",
        "//verible/common/analysis:syntax-tree-linter-test-utils",
        "//verible/verilog/analysis:verilog-analyzer",
        "//verible/verilog/parser:verilog-token-enum",
        "@abseil-cpp//absl/status",
        "@abseil-cpp//absl/strings",
        "@googletest//:gtest",
        "@googletest//:gtest_main",
    ],
)

cc_library(
    name = "parameter-type-name-style-rule",
    srcs = ["parameter-type-name-style-rule.cc"],
    hdrs = ["parameter-type-name-style-rule.h"],
    deps = [
        "//verible/common/analysis:lint-rule-status",
        "//verible/common/analysis:syntax-tree-lint-rule",
        "//verible/common/analysis/matcher",
        "//verible/common/analysis/matcher:bound-symbol-manager",
        "//verible/common/strings:naming-utils",
        "//verible/common/text:symbol",
        "//verible/common/text:syntax-tree-context",
        "//verible/common/text:token-info",
        "//verible/verilog/CST:parameters",
        "//verible/verilog/CST:verilog-matchers",
        "//verible/verilog/analysis:descriptions",
        "//verible/verilog/analysis:lint-rule-registry",
        "@abseil-cpp//absl/strings",
    ],
    alwayslink = 1,
)

cc_test(
    name = "parameter-type-name-style-rule_test",
    srcs = ["parameter-type-name-style-rule_test.cc"],
    deps = [
        ":parameter-type-name-style-rule",
        "//verible/common/analysis:linter-test-utils",
        "//verible/common/analysis:syntax-tree-linter-test-utils",
        "//verible/verilog/analysis:verilog-analyzer",
        "//verible/verilog/parser:verilog-token-enum",
        "@googletest//:gtest",
        "@googletest//:gtest_main",
    ],
)

cc_library(
    name = "proper-parameter-declaration-rule",
    srcs = ["proper-parameter-declaration-rule.cc"],
    hdrs = ["proper-parameter-declaration-rule.h"],
    deps = [
        "//verible/common/analysis:lint-rule-status",
        "//verible/common/analysis:syntax-tree-lint-rule",
        "//verible/common/analysis/matcher",
        "//verible/common/analysis/matcher:bound-symbol-manager",
        "//verible/common/text:config-utils",
        "//verible/common/text:symbol",
        "//verible/common/text:syntax-tree-context",
        "//verible/common/text:token-info",
        "//verible/verilog/CST:context-functions",
        "//verible/verilog/CST:parameters",
        "//verible/verilog/CST:verilog-matchers",
        "//verible/verilog/analysis:descriptions",
        "//verible/verilog/analysis:lint-rule-registry",
        "//verible/verilog/parser:verilog-token-enum",
        "@abseil-cpp//absl/status",
    ],
    alwayslink = 1,
)

cc_test(
    name = "proper-parameter-declaration-rule_test",
    srcs = ["proper-parameter-declaration-rule_test.cc"],
    deps = [
        ":proper-parameter-declaration-rule",
        "//verible/common/analysis:linter-test-utils",
        "//verible/common/analysis:syntax-tree-linter-test-utils",
        "//verible/verilog/analysis:verilog-analyzer",
        "//verible/verilog/parser:verilog-token-enum",
        "@googletest//:gtest",
        "@googletest//:gtest_main",
    ],
)

cc_library(
    name = "plusarg-assignment-rule",
    srcs = ["plusarg-assignment-rule.cc"],
    hdrs = ["plusarg-assignment-rule.h"],
    deps = [
        "//verible/common/analysis:lint-rule-status",
        "//verible/common/analysis:syntax-tree-lint-rule",
        "//verible/common/analysis/matcher",
        "//verible/common/analysis/matcher:bound-symbol-manager",
        "//verible/common/text:concrete-syntax-leaf",
        "//verible/common/text:symbol",
        "//verible/common/text:syntax-tree-context",
        "//verible/common/text:token-info",
        "//verible/verilog/CST:verilog-matchers",
        "//verible/verilog/analysis:descriptions",
        "//verible/verilog/analysis:lint-rule-registry",
        "@abseil-cpp//absl/strings",
    ],
    alwayslink = 1,
)

cc_test(
    name = "plusarg-assignment-rule_test",
    srcs = ["plusarg-assignment-rule_test.cc"],
    deps = [
        ":plusarg-assignment-rule",
        "//verible/common/analysis:linter-test-utils",
        "//verible/common/analysis:syntax-tree-linter-test-utils",
        "//verible/verilog/analysis:verilog-analyzer",
        "//verible/verilog/parser:verilog-token-enum",
        "@googletest//:gtest",
        "@googletest//:gtest_main",
    ],
)

cc_library(
    name = "port-name-suffix-rule",
    srcs = ["port-name-suffix-rule.cc"],
    hdrs = ["port-name-suffix-rule.h"],
    deps = [
        "//verible/common/analysis:lint-rule-status",
        "//verible/common/analysis:syntax-tree-lint-rule",
        "//verible/common/analysis/matcher",
        "//verible/common/analysis/matcher:bound-symbol-manager",
        "//verible/common/text:symbol",
        "//verible/common/text:syntax-tree-context",
        "//verible/common/text:token-info",
        "//verible/common/util:logging",
        "//verible/verilog/CST:port",
        "//verible/verilog/CST:verilog-matchers",
        "//verible/verilog/analysis:descriptions",
        "//verible/verilog/analysis:lint-rule-registry",
        "@abseil-cpp//absl/strings",
    ],
    alwayslink = 1,
)

cc_test(
    name = "port-name-suffix-rule_test",
    srcs = ["port-name-suffix-rule_test.cc"],
    deps = [
        ":port-name-suffix-rule",
        "//verible/common/analysis:linter-test-utils",
        "//verible/common/analysis:syntax-tree-linter-test-utils",
        "//verible/verilog/analysis:verilog-analyzer",
        "//verible/verilog/parser:verilog-token-enum",
        "@googletest//:gtest",
        "@googletest//:gtest_main",
    ],
)

cc_library(
    name = "explicit-parameter-storage-type-rule",
    srcs = ["explicit-parameter-storage-type-rule.cc"],
    hdrs = ["explicit-parameter-storage-type-rule.h"],
    deps = [
        "//verible/common/analysis:lint-rule-status",
        "//verible/common/analysis:syntax-tree-lint-rule",
        "//verible/common/analysis/matcher",
        "//verible/common/analysis/matcher:bound-symbol-manager",
        "//verible/common/text:config-utils",
        "//verible/common/text:symbol",
        "//verible/common/text:syntax-tree-context",
        "//verible/common/text:token-info",
        "//verible/common/text:tree-utils",
        "//verible/common/util:logging",
        "//verible/verilog/CST:parameters",
        "//verible/verilog/CST:verilog-matchers",  # fixdeps: keep
        "//verible/verilog/analysis:descriptions",
        "//verible/verilog/analysis:lint-rule-registry",
        "//verible/verilog/parser:verilog-token-enum",
        "@abseil-cpp//absl/status",
        "@abseil-cpp//absl/strings",
    ],
    alwayslink = 1,
)

cc_test(
    name = "explicit-parameter-storage-type-rule_test",
    srcs = ["explicit-parameter-storage-type-rule_test.cc"],
    deps = [
        ":explicit-parameter-storage-type-rule",
        "//verible/common/analysis:linter-test-utils",
        "//verible/common/analysis:syntax-tree-linter-test-utils",
        "//verible/verilog/analysis:verilog-analyzer",
        "//verible/verilog/parser:verilog-token-enum",
        "@abseil-cpp//absl/status",
        "@abseil-cpp//absl/strings",
        "@googletest//:gtest",
        "@googletest//:gtest_main",
    ],
)

cc_library(
    name = "explicit-function-task-parameter-type-rule",
    srcs = ["explicit-function-task-parameter-type-rule.cc"],
    hdrs = ["explicit-function-task-parameter-type-rule.h"],
    deps = [
        "//verible/common/analysis:lint-rule-status",
        "//verible/common/analysis:syntax-tree-lint-rule",
        "//verible/common/analysis/matcher",
        "//verible/common/analysis/matcher:bound-symbol-manager",
        "//verible/common/text:symbol",
        "//verible/common/text:syntax-tree-context",
        "//verible/common/util:logging",
        "//verible/verilog/CST:port",
        "//verible/verilog/CST:type",
        "//verible/verilog/CST:verilog-matchers",
        "//verible/verilog/analysis:descriptions",
        "//verible/verilog/analysis:lint-rule-registry",
    ],
    alwayslink = 1,
)

cc_test(
    name = "explicit-function-task-parameter-type-rule_test",
    srcs = ["explicit-function-task-parameter-type-rule_test.cc"],
    deps = [
        ":explicit-function-task-parameter-type-rule",
        "//verible/common/analysis:linter-test-utils",
        "//verible/common/analysis:syntax-tree-linter-test-utils",
        "//verible/verilog/analysis:verilog-analyzer",
        "//verible/verilog/parser:verilog-token-enum",
        "@googletest//:gtest",
        "@googletest//:gtest_main",
    ],
)

cc_library(
    name = "signal-name-style-rule",
    srcs = ["signal-name-style-rule.cc"],
    hdrs = ["signal-name-style-rule.h"],
    deps = [
        "//verible/common/analysis:lint-rule-status",
        "//verible/common/analysis:syntax-tree-lint-rule",
        "//verible/common/analysis/matcher",
        "//verible/common/analysis/matcher:bound-symbol-manager",
        "//verible/common/text:concrete-syntax-leaf",
        "//verible/common/text:config-utils",
        "//verible/common/text:symbol",
        "//verible/common/text:syntax-tree-context",
        "//verible/common/text:token-info",
        "//verible/common/util:logging",
        "//verible/verilog/CST:data",
        "//verible/verilog/CST:net",
        "//verible/verilog/CST:port",
        "//verible/verilog/CST:verilog-matchers",
        "//verible/verilog/analysis:descriptions",
        "//verible/verilog/analysis:lint-rule-registry",
        "@abseil-cpp//absl/status",
        "@abseil-cpp//absl/strings",
        "@re2",
    ],
    alwayslink = 1,
)

cc_test(
    name = "signal-name-style-rule_test",
    srcs = ["signal-name-style-rule_test.cc"],
    deps = [
        ":signal-name-style-rule",
        "//verible/common/analysis:linter-test-utils",
        "//verible/common/analysis:syntax-tree-linter-test-utils",
        "//verible/verilog/analysis:verilog-analyzer",
        "//verible/verilog/parser:verilog-token-enum",
        "@googletest//:gtest",
        "@googletest//:gtest_main",
    ],
)

cc_library(
    name = "struct-union-name-style-rule",
    srcs = ["struct-union-name-style-rule.cc"],
    hdrs = ["struct-union-name-style-rule.h"],
    deps = [
        "//verible/common/analysis:lint-rule-status",
        "//verible/common/analysis:syntax-tree-lint-rule",
        "//verible/common/analysis/matcher",
        "//verible/common/analysis/matcher:bound-symbol-manager",
        "//verible/common/text:config-utils",
        "//verible/common/text:symbol",
        "//verible/common/text:syntax-tree-context",
        "//verible/common/util:logging",
        "//verible/verilog/CST:type",
        "//verible/verilog/CST:verilog-matchers",
        "//verible/verilog/analysis:descriptions",
        "//verible/verilog/analysis:lint-rule-registry",
        "@abseil-cpp//absl/status",
        "@abseil-cpp//absl/strings",
    ],
    alwayslink = 1,
)

cc_test(
    name = "struct-union-name-style-rule_test",
    srcs = ["struct-union-name-style-rule_test.cc"],
    deps = [
        ":struct-union-name-style-rule",
        "//verible/common/analysis:linter-test-utils",
        "//verible/common/analysis:syntax-tree-linter-test-utils",
        "//verible/verilog/analysis:verilog-analyzer",
        "//verible/verilog/parser:verilog-token-enum",
        "@abseil-cpp//absl/status",
        "@abseil-cpp//absl/strings",
        "@googletest//:gtest",
        "@googletest//:gtest_main",
    ],
)

cc_library(
    name = "interface-name-style-rule",
    srcs = ["interface-name-style-rule.cc"],
    hdrs = ["interface-name-style-rule.h"],
    deps = [
        "//verible/common/analysis:lint-rule-status",
        "//verible/common/analysis:syntax-tree-lint-rule",
        "//verible/common/analysis/matcher",
        "//verible/common/analysis/matcher:bound-symbol-manager",
        "//verible/common/text:config-utils",
        "//verible/common/text:symbol",
        "//verible/common/text:syntax-tree-context",
        "//verible/common/text:token-info",
        "//verible/verilog/CST:module",
        "//verible/verilog/CST:type",
        "//verible/verilog/CST:verilog-matchers",
        "//verible/verilog/analysis:descriptions",
        "//verible/verilog/analysis:lint-rule-registry",
        "@abseil-cpp//absl/status",
        "@abseil-cpp//absl/strings",
        "@re2",
    ],
    alwayslink = 1,
)

cc_test(
    name = "interface-name-style-rule_test",
    srcs = ["interface-name-style-rule_test.cc"],
    deps = [
        ":interface-name-style-rule",
        "//verible/common/analysis:linter-test-utils",
        "//verible/common/analysis:syntax-tree-linter-test-utils",
        "//verible/verilog/analysis:verilog-analyzer",
        "//verible/verilog/parser:verilog-token-enum",
        "@googletest//:gtest",
        "@googletest//:gtest_main",
    ],
)

cc_library(
    name = "instance-shadow-rule",
    srcs = ["instance-shadow-rule.cc"],
    hdrs = ["instance-shadow-rule.h"],
    deps = [
        "//verible/common/analysis:citation",
        "//verible/common/analysis:lint-rule-status",
        "//verible/common/analysis:syntax-tree-lint-rule",
        "//verible/common/analysis:syntax-tree-search",
        "//verible/common/analysis/matcher",
        "//verible/common/analysis/matcher:bound-symbol-manager",
        "//verible/common/text:symbol",
        "//verible/common/text:syntax-tree-context",
        "//verible/common/text:tree-utils",
        "//verible/common/util:iterator-adaptors",
        "//verible/verilog/CST:identifier",
        "//verible/verilog/CST:verilog-matchers",
        "//verible/verilog/CST:verilog-nonterminals",
        "//verible/verilog/analysis:descriptions",
        "//verible/verilog/analysis:lint-rule-registry",
    ],
    alwayslink = 1,
)

cc_test(
    name = "instance-shadow-rule-test",
    srcs = ["instance-shadow-rule_test.cc"],
    deps = [
        ":instance-shadow-rule",
        "//verible/common/analysis:linter-test-utils",
        "//verible/common/analysis:syntax-tree-linter",
        "//verible/common/analysis:syntax-tree-linter-test-utils",
        "//verible/common/util:logging",
        "//verible/verilog/analysis:verilog-analyzer",
        "//verible/verilog/parser:verilog-token-enum",
        "@abseil-cpp//absl/status",
        "@googletest//:gtest",
        "@googletest//:gtest_main",
    ],
)

cc_library(
    name = "legacy-genvar-declaration-rule",
    srcs = ["legacy-genvar-declaration-rule.cc"],
    hdrs = ["legacy-genvar-declaration-rule.h"],
    deps = [
        "//verible/common/analysis:lint-rule-status",
        "//verible/common/analysis:syntax-tree-lint-rule",
        "//verible/common/text:concrete-syntax-tree",
        "//verible/common/text:symbol",
        "//verible/common/text:syntax-tree-context",
        "//verible/common/text:tree-utils",
        "//verible/verilog/CST:identifier",
        "//verible/verilog/CST:verilog-nonterminals",
        "//verible/verilog/analysis:descriptions",
        "//verible/verilog/analysis:lint-rule-registry",
    ],
    alwayslink = 1,
)

cc_test(
    name = "legacy-genvar-declaration-rule_test",
    srcs = ["legacy-genvar-declaration-rule_test.cc"],
    deps = [
        ":legacy-genvar-declaration-rule",
        "//verible/common/analysis:linter-test-utils",
        "//verible/common/analysis:syntax-tree-linter-test-utils",
        "//verible/verilog/analysis:verilog-analyzer",
        "//verible/verilog/parser:verilog-token-enum",
        "@googletest//:gtest",
        "@googletest//:gtest_main",
    ],
)

cc_library(
    name = "legacy-generate-region-rule",
    srcs = ["legacy-generate-region-rule.cc"],
    hdrs = ["legacy-generate-region-rule.h"],
    deps = [
        "//verible/common/analysis:lint-rule-status",
        "//verible/common/analysis:syntax-tree-lint-rule",
        "//verible/common/analysis/matcher:matcher-builders",
        "//verible/common/text:concrete-syntax-tree",
        "//verible/common/text:symbol",
        "//verible/common/text:syntax-tree-context",
        "//verible/common/text:tree-utils",
        "//verible/common/util:logging",
        "//verible/verilog/CST:verilog-nonterminals",
        "//verible/verilog/analysis:descriptions",
        "//verible/verilog/analysis:lint-rule-registry",
        "//verible/verilog/parser:verilog-token-enum",
    ],
    alwayslink = 1,
)

cc_test(
    name = "legacy-generate-region-rule_test",
    srcs = ["legacy-generate-region-rule_test.cc"],
    deps = [
        ":legacy-generate-region-rule",
        "//verible/common/analysis:linter-test-utils",
        "//verible/common/analysis:syntax-tree-linter-test-utils",
        "//verible/verilog/analysis:verilog-analyzer",
        "//verible/verilog/parser:verilog-token-enum",
        "@googletest//:gtest",
        "@googletest//:gtest_main",
    ],
)

cc_library(
    name = "uvm-macro-semicolon-rule",
    srcs = ["uvm-macro-semicolon-rule.cc"],
    hdrs = ["uvm-macro-semicolon-rule.h"],
    deps = [
        "//verible/common/analysis:lint-rule-status",
        "//verible/common/analysis:syntax-tree-lint-rule",
        "//verible/common/text:concrete-syntax-leaf",
        "//verible/common/text:symbol",
        "//verible/common/text:syntax-tree-context",
        "//verible/common/text:token-info",
        "//verible/verilog/CST:context-functions",
        "//verible/verilog/CST:macro",
        "//verible/verilog/CST:verilog-nonterminals",
        "//verible/verilog/analysis:descriptions",
        "//verible/verilog/analysis:lint-rule-registry",
        "//verible/verilog/parser:verilog-token-enum",
        "@abseil-cpp//absl/strings",
    ],
    alwayslink = 1,
)

cc_test(
    name = "uvm-macro-semicolon-rule_test",
    srcs = ["uvm-macro-semicolon-rule_test.cc"],
    deps = [
        ":uvm-macro-semicolon-rule",
        "//verible/common/analysis:linter-test-utils",
        "//verible/common/analysis:syntax-tree-linter-test-utils",
        "//verible/verilog/analysis:verilog-analyzer",
        "@googletest//:gtest",
        "@googletest//:gtest_main",
    ],
)

cc_library(
    name = "suspicious-semicolon-rule",
    srcs = ["suspicious-semicolon-rule.cc"],
    hdrs = ["suspicious-semicolon-rule.h"],
    deps = [
        "//verible/common/analysis:lint-rule-status",
        "//verible/common/analysis:syntax-tree-lint-rule",
        "//verible/common/analysis/matcher",
        "//verible/common/analysis/matcher:bound-symbol-manager",
        "//verible/common/text:concrete-syntax-tree",
        "//verible/common/text:syntax-tree-context",
        "//verible/common/text:tree-utils",
        "//verible/verilog/CST:verilog-matchers",
        "//verible/verilog/CST:verilog-nonterminals",
        "//verible/verilog/analysis:descriptions",
        "//verible/verilog/analysis:lint-rule-registry",
    ],
    alwayslink = 1,
)

cc_test(
    name = "suspicious-semicolon-rule_test",
    srcs = ["suspicious-semicolon-rule_test.cc"],
    deps = [
        ":suspicious-semicolon-rule",
        "//verible/common/analysis:linter-test-utils",
        "//verible/common/analysis:syntax-tree-linter-test-utils",
        "//verible/verilog/analysis:verilog-analyzer",
        "@googletest//:gtest",
        "@googletest//:gtest_main",
    ],
)

cc_library(
    name = "dff-name-style-rule",
    srcs = ["dff-name-style-rule.cc"],
    hdrs = ["dff-name-style-rule.h"],
    deps = [
        "//verible/common/analysis:lint-rule-status",
        "//verible/common/analysis:syntax-tree-lint-rule",
        "//verible/common/analysis/matcher",
        "//verible/common/analysis/matcher:bound-symbol-manager",
        "//verible/common/analysis/matcher:core-matchers",
        "//verible/common/text:concrete-syntax-leaf",
        "//verible/common/text:concrete-syntax-tree",
        "//verible/common/text:config-utils",
        "//verible/common/text:symbol",
        "//verible/common/text:syntax-tree-context",
        "//verible/common/text:token-info",
        "//verible/common/text:tree-utils",
        "//verible/verilog/CST:expression",
        "//verible/verilog/CST:statement",
        "//verible/verilog/CST:verilog-matchers",
        "//verible/verilog/CST:verilog-nonterminals",
        "//verible/verilog/analysis:descriptions",
        "//verible/verilog/analysis:lint-rule-registry",
        "//verible/verilog/parser:verilog-token-enum",
        "@abseil-cpp//absl/status",
        "@abseil-cpp//absl/strings",
        "@re2",
    ],
    alwayslink = 1,
)

cc_test(
    name = "dff-name-style-rule_test",
    srcs = ["dff-name-style-rule_test.cc"],
    deps = [
        ":dff-name-style-rule",
        "//verible/common/analysis:linter-test-utils",
        "//verible/common/analysis:syntax-tree-linter-test-utils",
        "//verible/verilog/analysis:verilog-analyzer",
        "//verible/verilog/parser:verilog-token-enum",
        "@abseil-cpp//absl/log:check",
        "@googletest//:gtest",
        "@googletest//:gtest_main",
    ],
)
