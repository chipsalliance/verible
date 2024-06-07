# A set of linter rules to be used in analysis of verilog code.

default_visibility = [
    "//verilog/analysis:__subpackages__",
    "//verilog/tools:__subpackages__",
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
        ":disable-statement-rule",
        ":endif-comment-rule",
        ":enum-name-style-rule",
        ":explicit-function-lifetime-rule",
        ":explicit-function-task-parameter-type-rule",
        ":explicit-parameter-storage-type-rule",
        ":explicit-task-lifetime-rule",
        ":forbid-consecutive-null-statements-rule",
        ":forbid-defparam-rule",
        ":forbid-negative-array-dim",
        ":forbidden-anonymous-enums-rule",
        ":forbidden-anonymous-structs-unions-rule",
        ":forbidden-macro-rule",
        ":forbidden-symbol-rule",
        ":generate-label-prefix-rule",
        ":generate-label-rule",
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
        ":token-stream-lint-rule",
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
    srcs = ["banned_declared_name_patterns_rule.cc"],
    hdrs = ["banned_declared_name_patterns_rule.h"],
    deps = [
        "//common/analysis:lint-rule-status",
        "//common/analysis:syntax-tree-lint-rule",
        "//common/text:concrete-syntax-tree",
        "//common/text:symbol",
        "//common/text:syntax-tree-context",
        "//common/text:token-info",
        "//verilog/CST:module",
        "//verilog/CST:package",
        "//verilog/CST:verilog-matchers",
        "//verilog/CST:verilog-nonterminals",
        "//verilog/analysis:descriptions",
        "//verilog/analysis:lint-rule-registry",
        "@com_google_absl//absl/strings",
        "@com_google_absl//absl/strings:string_view",
    ],
    alwayslink = 1,
)

cc_test(
    name = "banned-declared-name-patterns-rule_test",
    srcs = ["banned_declared_name_patterns_rule_test.cc"],
    deps = [
        ":banned-declared-name-patterns-rule",
        "//common/analysis:linter-test-utils",
        "//common/analysis:syntax-tree-linter-test-utils",
        "//verilog/analysis:verilog-analyzer",
        "//verilog/parser:verilog-token-enum",
        "@com_google_googletest//:gtest",
        "@com_google_googletest//:gtest_main",
    ],
)

cc_library(
    name = "no-tabs-rule",
    srcs = ["no_tabs_rule.cc"],
    hdrs = ["no_tabs_rule.h"],
    deps = [
        "//common/analysis:line-lint-rule",
        "//common/analysis:lint-rule-status",
        "//common/text:token-info",
        "//verilog/analysis:descriptions",
        "//verilog/analysis:lint-rule-registry",
        "//verilog/parser:verilog-token-enum",
        "@com_google_absl//absl/strings:string_view",
    ],
    alwayslink = 1,
)

cc_test(
    name = "no-tabs-rule_test",
    srcs = ["no_tabs_rule_test.cc"],
    deps = [
        ":no-tabs-rule",
        "//common/analysis:line-linter-test-utils",
        "//common/analysis:linter-test-utils",
        "//verilog/analysis:verilog-analyzer",
        "//verilog/parser:verilog-token-enum",
        "@com_google_googletest//:gtest",
        "@com_google_googletest//:gtest_main",
    ],
)

cc_library(
    name = "no-trailing-spaces-rule",
    srcs = ["no_trailing_spaces_rule.cc"],
    hdrs = ["no_trailing_spaces_rule.h"],
    deps = [
        "//common/analysis:line-lint-rule",
        "//common/analysis:lint-rule-status",
        "//common/text:token-info",
        "//verilog/analysis:descriptions",
        "//verilog/analysis:lint-rule-registry",
        "//verilog/parser:verilog-token-enum",
        "@com_google_absl//absl/strings",
        "@com_google_absl//absl/strings:string_view",
    ],
    alwayslink = 1,
)

cc_test(
    name = "no-trailing-spaces-rule_test",
    srcs = ["no_trailing_spaces_rule_test.cc"],
    deps = [
        ":no-trailing-spaces-rule",
        "//common/analysis:line-linter-test-utils",
        "//common/analysis:linter-test-utils",
        "//verilog/analysis:verilog-analyzer",
        "//verilog/parser:verilog-token-enum",
        "@com_google_googletest//:gtest",
        "@com_google_googletest//:gtest_main",
    ],
)

cc_library(
    name = "positive-meaning-parameter-name-rule",
    srcs = ["positive_meaning_parameter_name_rule.cc"],
    hdrs = ["positive_meaning_parameter_name_rule.h"],
    deps = [
        "//common/analysis:lint-rule-status",
        "//common/analysis:syntax-tree-lint-rule",
        "//common/analysis/matcher",
        "//common/analysis/matcher:bound-symbol-manager",
        "//common/text:symbol",
        "//common/text:syntax-tree-context",
        "//common/text:token-info",
        "//verilog/CST:parameters",
        "//verilog/CST:verilog-matchers",
        "//verilog/analysis:descriptions",
        "//verilog/analysis:lint-rule-registry",
        "@com_google_absl//absl/strings",
        "@com_google_absl//absl/strings:string_view",
    ],
    alwayslink = 1,
)

cc_test(
    name = "positive-meaning-parameter-name-rule_test",
    srcs = ["positive_meaning_parameter_name_rule_test.cc"],
    deps = [
        ":positive-meaning-parameter-name-rule",
        "//common/analysis:linter-test-utils",
        "//common/analysis:syntax-tree-linter-test-utils",
        "//verilog/analysis:verilog-analyzer",
        "//verilog/parser:verilog-token-enum",
        "@com_google_googletest//:gtest",
        "@com_google_googletest//:gtest_main",
    ],
)

cc_library(
    name = "posix-eof-rule",
    srcs = ["posix_eof_rule.cc"],
    hdrs = ["posix_eof_rule.h"],
    deps = [
        "//common/analysis:lint-rule-status",
        "//common/analysis:text-structure-lint-rule",
        "//common/text:text-structure",
        "//common/text:token-info",
        "//verilog/analysis:descriptions",
        "//verilog/analysis:lint-rule-registry",
        "//verilog/parser:verilog-token-enum",
        "@com_google_absl//absl/strings:string_view",
    ],
    alwayslink = 1,
)

cc_test(
    name = "posix-eof-rule_test",
    srcs = ["posix_eof_rule_test.cc"],
    deps = [
        ":posix-eof-rule",
        "//common/analysis:linter-test-utils",
        "//common/analysis:text-structure-linter-test-utils",
        "//verilog/analysis:verilog-analyzer",
        "//verilog/parser:verilog-token-enum",
        "@com_google_googletest//:gtest",
        "@com_google_googletest//:gtest_main",
    ],
)

cc_library(
    name = "line-length-rule",
    srcs = ["line_length_rule.cc"],
    hdrs = ["line_length_rule.h"],
    deps = [
        "//common/analysis:lint-rule-status",
        "//common/analysis:text-structure-lint-rule",
        "//common/strings:comment-utils",
        "//common/strings:utf8",
        "//common/text:config-utils",
        "//common/text:constants",
        "//common/text:text-structure",
        "//common/text:token-info",
        "//common/text:token-stream-view",
        "//common/util:iterator-range",
        "//verilog/analysis:descriptions",
        "//verilog/analysis:lint-rule-registry",
        "//verilog/analysis:verilog-linter-constants",
        "//verilog/parser:verilog-token-classifications",
        "//verilog/parser:verilog-token-enum",
        "@com_google_absl//absl/status",
        "@com_google_absl//absl/strings",
        "@com_google_absl//absl/strings:string_view",
    ],
    alwayslink = 1,
)

cc_test(
    name = "line-length-rule_test",
    srcs = ["line_length_rule_test.cc"],
    deps = [
        ":line-length-rule",
        "//common/analysis:linter-test-utils",
        "//common/analysis:text-structure-linter-test-utils",
        "//common/util:logging",
        "//verilog/analysis:verilog-analyzer",
        "//verilog/parser:verilog-token-enum",
        "@com_google_absl//absl/status",
        "@com_google_absl//absl/strings",
        "@com_google_googletest//:gtest",
        "@com_google_googletest//:gtest_main",
    ],
)

cc_library(
    name = "forbid-consecutive-null-statements-rule",
    srcs = ["forbid_consecutive_null_statements_rule.cc"],
    hdrs = ["forbid_consecutive_null_statements_rule.h"],
    deps = [
        "//common/analysis:lint-rule-status",
        "//common/analysis:syntax-tree-lint-rule",
        "//common/text:concrete-syntax-leaf",
        "//common/text:symbol",
        "//common/text:syntax-tree-context",
        "//verilog/CST:verilog-matchers",  # fixdeps: keep
        "//verilog/CST:verilog-nonterminals",
        "//verilog/analysis:descriptions",
        "//verilog/analysis:lint-rule-registry",
        "@com_google_absl//absl/strings:string_view",
    ],
    alwayslink = 1,
)

cc_test(
    name = "forbid-consecutive-null-statements-rule_test",
    srcs = ["forbid_consecutive_null_statements_rule_test.cc"],
    deps = [
        ":forbid-consecutive-null-statements-rule",
        "//common/analysis:linter-test-utils",
        "//common/analysis:syntax-tree-linter-test-utils",
        "//verilog/analysis:verilog-analyzer",
        "@com_google_googletest//:gtest",
        "@com_google_googletest//:gtest_main",
    ],
)

cc_library(
    name = "forbid-defparam-rule",
    srcs = ["forbid_defparam_rule.cc"],
    hdrs = ["forbid_defparam_rule.h"],
    deps = [
        "//common/analysis:lint-rule-status",
        "//common/analysis:syntax-tree-lint-rule",
        "//common/analysis/matcher",
        "//common/analysis/matcher:bound-symbol-manager",
        "//common/text:concrete-syntax-leaf",
        "//common/text:symbol",
        "//common/text:syntax-tree-context",
        "//common/text:tree-utils",
        "//common/util:logging",
        "//verilog/CST:verilog-matchers",  # fixdeps: keep
        "//verilog/CST:verilog-nonterminals",
        "//verilog/analysis:descriptions",
        "//verilog/analysis:lint-rule-registry",
        "//verilog/parser:verilog-token-enum",
        "@com_google_absl//absl/strings:string_view",
    ],
    alwayslink = 1,
)

cc_test(
    name = "forbid-defparam-rule_test",
    srcs = ["forbid_defparam_rule_test.cc"],
    deps = [
        ":forbid-defparam-rule",
        "//common/analysis:linter-test-utils",
        "//common/analysis:syntax-tree-linter-test-utils",
        "//verilog/analysis:verilog-analyzer",
        "//verilog/parser:verilog-token-enum",
        "@com_google_googletest//:gtest",
        "@com_google_googletest//:gtest_main",
    ],
)

cc_library(
    name = "mismatched-labels-rule",
    srcs = ["mismatched_labels_rule.cc"],
    hdrs = ["mismatched_labels_rule.h"],
    deps = [
        "//common/analysis:lint-rule-status",
        "//common/analysis:syntax-tree-lint-rule",
        "//common/analysis/matcher",
        "//common/analysis/matcher:bound-symbol-manager",
        "//common/text:concrete-syntax-tree",
        "//common/text:symbol",
        "//common/text:syntax-tree-context",
        "//verilog/CST:seq-block",
        "//verilog/CST:verilog-matchers",  # fixdeps: keep
        "//verilog/analysis:descriptions",
        "//verilog/analysis:lint-rule-registry",
        "@com_google_absl//absl/strings:string_view",
    ],
    alwayslink = 1,
)

cc_test(
    name = "mismatched-labels-rule_test",
    srcs = ["mismatched_labels_rule_test.cc"],
    deps = [
        ":mismatched-labels-rule",
        "//common/analysis:linter-test-utils",
        "//common/analysis:syntax-tree-linter-test-utils",
        "//verilog/analysis:verilog-analyzer",
        "//verilog/parser:verilog-token-enum",
        "@com_google_googletest//:gtest",
        "@com_google_googletest//:gtest_main",
    ],
)

cc_library(
    name = "module-begin-block-rule",
    srcs = ["module_begin_block_rule.cc"],
    hdrs = ["module_begin_block_rule.h"],
    deps = [
        "//common/analysis:lint-rule-status",
        "//common/analysis:syntax-tree-lint-rule",
        "//common/analysis/matcher",
        "//common/analysis/matcher:bound-symbol-manager",
        "//common/text:symbol",
        "//common/text:syntax-tree-context",
        "//verilog/CST:verilog-matchers",  # fixdeps: keep
        "//verilog/analysis:descriptions",
        "//verilog/analysis:lint-rule-registry",
        "@com_google_absl//absl/strings:string_view",
    ],
    alwayslink = 1,
)

cc_test(
    name = "module-begin-block-rule_test",
    srcs = ["module_begin_block_rule_test.cc"],
    deps = [
        ":module-begin-block-rule",
        "//common/analysis:linter-test-utils",
        "//common/analysis:syntax-tree-linter-test-utils",
        "//verilog/analysis:verilog-analyzer",
        "//verilog/parser:verilog-token-enum",
        "@com_google_googletest//:gtest",
        "@com_google_googletest//:gtest_main",
    ],
)

cc_library(
    name = "module-filename-rule",
    srcs = ["module_filename_rule.cc"],
    hdrs = ["module_filename_rule.h"],
    deps = [
        "//common/analysis:lint-rule-status",
        "//common/analysis:syntax-tree-search",
        "//common/analysis:text-structure-lint-rule",
        "//common/text:concrete-syntax-leaf",
        "//common/text:config-utils",
        "//common/text:symbol",
        "//common/text:text-structure",
        "//common/text:token-info",
        "//common/util:file-util",
        "//common/util:logging",
        "//verilog/CST:module",
        "//verilog/CST:verilog-nonterminals",
        "//verilog/analysis:descriptions",
        "//verilog/analysis:lint-rule-registry",
        "@com_google_absl//absl/status",
        "@com_google_absl//absl/strings",
        "@com_google_absl//absl/strings:string_view",
    ],
    alwayslink = 1,
)

cc_test(
    name = "module-filename-rule_test",
    srcs = ["module_filename_rule_test.cc"],
    deps = [
        ":module-filename-rule",
        "//common/analysis:linter-test-utils",
        "//common/analysis:text-structure-linter-test-utils",
        "//verilog/analysis:verilog-analyzer",
        "//verilog/parser:verilog-token-enum",
        "@com_google_absl//absl/strings:string_view",
        "@com_google_googletest//:gtest",
        "@com_google_googletest//:gtest_main",
    ],
)

cc_library(
    name = "package-filename-rule",
    srcs = ["package_filename_rule.cc"],
    hdrs = ["package_filename_rule.h"],
    deps = [
        "//common/analysis:lint-rule-status",
        "//common/analysis:syntax-tree-search",
        "//common/analysis:text-structure-lint-rule",
        "//common/text:config-utils",
        "//common/text:text-structure",
        "//common/text:token-info",
        "//common/util:file-util",
        "//verilog/CST:package",
        "//verilog/analysis:descriptions",
        "//verilog/analysis:lint-rule-registry",
        "@com_google_absl//absl/status",
        "@com_google_absl//absl/strings",
        "@com_google_absl//absl/strings:string_view",
    ],
    alwayslink = 1,
)

cc_test(
    name = "package-filename-rule_test",
    srcs = ["package_filename_rule_test.cc"],
    deps = [
        ":package-filename-rule",
        "//common/analysis:linter-test-utils",
        "//common/analysis:text-structure-linter-test-utils",
        "//verilog/analysis:verilog-analyzer",
        "//verilog/parser:verilog-token-enum",
        "@com_google_absl//absl/strings:string_view",
        "@com_google_googletest//:gtest",
        "@com_google_googletest//:gtest_main",
    ],
)

cc_library(
    name = "module-instantiation-rules",
    srcs = ["module_instantiation_rules.cc"],
    hdrs = ["module_instantiation_rules.h"],
    deps = [
        "//common/analysis:lint-rule-status",
        "//common/analysis:syntax-tree-lint-rule",
        "//common/analysis/matcher",
        "//common/analysis/matcher:bound-symbol-manager",
        "//common/text:concrete-syntax-leaf",
        "//common/text:concrete-syntax-tree",
        "//common/text:symbol",
        "//common/text:syntax-tree-context",
        "//common/text:token-info",
        "//common/text:tree-utils",
        "//common/util:casts",
        "//common/util:logging",
        "//verilog/CST:context-functions",
        "//verilog/CST:verilog-matchers",
        "//verilog/CST:verilog-nonterminals",
        "//verilog/analysis:descriptions",
        "//verilog/analysis:lint-rule-registry",
        "@com_google_absl//absl/strings:string_view",
    ],
    alwayslink = 1,
)

cc_test(
    name = "module-instantiation-rules_test",
    srcs = ["module_instantiation_rules_test.cc"],
    deps = [
        ":module-instantiation-rules",
        "//common/analysis:linter-test-utils",
        "//common/analysis:syntax-tree-linter-test-utils",
        "//verilog/analysis:verilog-analyzer",
        "//verilog/parser:verilog-token-enum",
        "@com_google_googletest//:gtest",
        "@com_google_googletest//:gtest_main",
    ],
)

cc_library(
    name = "one-module-per-file-rule",
    srcs = ["one_module_per_file_rule.cc"],
    hdrs = ["one_module_per_file_rule.h"],
    deps = [
        "//common/analysis:lint-rule-status",
        "//common/analysis:syntax-tree-search",
        "//common/analysis:text-structure-lint-rule",
        "//common/text:text-structure",
        "//common/util:logging",
        "//verilog/CST:module",
        "//verilog/CST:verilog-nonterminals",
        "//verilog/analysis:descriptions",
        "//verilog/analysis:lint-rule-registry",
        "@com_google_absl//absl/strings",
        "@com_google_absl//absl/strings:string_view",
    ],
    alwayslink = 1,
)

cc_test(
    name = "one-module-per-file-rule_test",
    srcs = ["one_module_per_file_rule_test.cc"],
    deps = [
        ":one-module-per-file-rule",
        "//common/analysis:linter-test-utils",
        "//common/analysis:text-structure-linter-test-utils",
        "//verilog/analysis:verilog-analyzer",
        "//verilog/parser:verilog-token-enum",
        "@com_google_googletest//:gtest",
        "@com_google_googletest//:gtest_main",
    ],
)

cc_library(
    name = "endif-comment-rule",
    srcs = ["endif_comment_rule.cc"],
    hdrs = ["endif_comment_rule.h"],
    deps = [
        "//common/analysis:lint-rule-status",
        "//common/analysis:token-stream-lint-rule",
        "//common/strings:comment-utils",
        "//common/text:token-info",
        "//verilog/analysis:descriptions",
        "//verilog/analysis:lint-rule-registry",
        "//verilog/parser:verilog-token-enum",
        "@com_google_absl//absl/strings",
        "@com_google_absl//absl/strings:string_view",
    ],
    alwayslink = 1,
)

cc_test(
    name = "endif-comment-rule_test",
    srcs = ["endif_comment_rule_test.cc"],
    deps = [
        ":endif-comment-rule",
        "//common/analysis:linter-test-utils",
        "//common/analysis:token-stream-linter-test-utils",
        "//verilog/analysis:verilog-analyzer",
        "//verilog/parser:verilog-token-enum",
        "@com_google_googletest//:gtest",
        "@com_google_googletest//:gtest_main",
    ],
)

cc_library(
    name = "forbidden-anonymous-enums-rule",
    srcs = ["forbidden_anonymous_enums_rule.cc"],
    hdrs = ["forbidden_anonymous_enums_rule.h"],
    deps = [
        "//common/analysis:lint-rule-status",
        "//common/analysis:syntax-tree-lint-rule",
        "//common/analysis/matcher",
        "//common/analysis/matcher:bound-symbol-manager",
        "//common/text:symbol",
        "//common/text:syntax-tree-context",
        "//verilog/CST:verilog-matchers",
        "//verilog/CST:verilog-nonterminals",
        "//verilog/analysis:descriptions",
        "//verilog/analysis:lint-rule-registry",
        "@com_google_absl//absl/strings:string_view",
    ],
    alwayslink = 1,
)

cc_test(
    name = "forbidden-anonymous-enums-rule_test",
    srcs = ["forbidden_anonymous_enums_rule_test.cc"],
    deps = [
        ":forbidden-anonymous-enums-rule",
        "//common/analysis:linter-test-utils",
        "//common/analysis:syntax-tree-linter-test-utils",
        "//verilog/analysis:verilog-analyzer",
        "//verilog/parser:verilog-token-enum",
        "@com_google_googletest//:gtest",
        "@com_google_googletest//:gtest_main",
    ],
)

cc_library(
    name = "enum-name-style-rule",
    srcs = ["enum_name_style_rule.cc"],
    hdrs = ["enum_name_style_rule.h"],
    deps = [
        "//common/analysis:lint-rule-status",
        "//common/analysis:syntax-tree-lint-rule",
        "//common/analysis/matcher",
        "//common/analysis/matcher:bound-symbol-manager",
        "//common/strings:naming-utils",
        "//common/text:symbol",
        "//common/text:syntax-tree-context",
        "//common/util:logging",
        "//verilog/CST:type",
        "//verilog/CST:verilog-matchers",
        "//verilog/analysis:descriptions",
        "//verilog/analysis:lint-rule-registry",
        "@com_google_absl//absl/strings",
        "@com_google_absl//absl/strings:string_view",
    ],
    alwayslink = 1,
)

cc_test(
    name = "enum-name-style-rule_test",
    srcs = ["enum_name_style_rule_test.cc"],
    deps = [
        ":enum-name-style-rule",
        "//common/analysis:linter-test-utils",
        "//common/analysis:syntax-tree-linter-test-utils",
        "//verilog/analysis:verilog-analyzer",
        "//verilog/parser:verilog-token-enum",
        "@com_google_googletest//:gtest",
        "@com_google_googletest//:gtest_main",
    ],
)

cc_library(
    name = "forbidden-macro-rule",
    srcs = ["forbidden_macro_rule.cc"],
    hdrs = ["forbidden_macro_rule.h"],
    deps = [
        "//common/analysis:citation",
        "//common/analysis:lint-rule-status",
        "//common/analysis:syntax-tree-lint-rule",
        "//common/analysis/matcher",
        "//common/analysis/matcher:bound-symbol-manager",
        "//common/text:concrete-syntax-leaf",
        "//common/text:symbol",
        "//common/text:syntax-tree-context",
        "//common/text:token-info",
        "//common/util:container-util",
        "//verilog/CST:verilog-matchers",
        "//verilog/analysis:descriptions",
        "//verilog/analysis:lint-rule-registry",
        "@com_google_absl//absl/strings:string_view",
    ],
    alwayslink = 1,
)

cc_library(
    name = "forbidden-anonymous-structs-unions-rule",
    srcs = ["forbidden_anonymous_structs_unions_rule.cc"],
    hdrs = ["forbidden_anonymous_structs_unions_rule.h"],
    deps = [
        "//common/analysis:lint-rule-status",
        "//common/analysis:syntax-tree-lint-rule",
        "//common/analysis/matcher",
        "//common/analysis/matcher:bound-symbol-manager",
        "//common/text:config-utils",
        "//common/text:symbol",
        "//common/text:syntax-tree-context",
        "//verilog/CST:verilog-matchers",
        "//verilog/CST:verilog-nonterminals",
        "//verilog/analysis:descriptions",
        "//verilog/analysis:lint-rule-registry",
        "@com_google_absl//absl/status",
        "@com_google_absl//absl/strings:string_view",
    ],
    alwayslink = 1,
)

cc_test(
    name = "forbidden-anonymous-structs-unions-rule_test",
    srcs = ["forbidden_anonymous_structs_unions_rule_test.cc"],
    deps = [
        ":forbidden-anonymous-structs-unions-rule",
        "//common/analysis:linter-test-utils",
        "//common/analysis:syntax-tree-linter-test-utils",
        "//verilog/analysis:verilog-analyzer",
        "//verilog/parser:verilog-token-enum",
        "@com_google_absl//absl/status",
        "@com_google_absl//absl/strings",
        "@com_google_googletest//:gtest",
        "@com_google_googletest//:gtest_main",
    ],
)

cc_test(
    name = "forbidden-macro-rule_test",
    srcs = ["forbidden_macro_rule_test.cc"],
    deps = [
        ":forbidden-macro-rule",
        "//common/analysis:linter-test-utils",
        "//common/analysis:syntax-tree-linter-test-utils",
        "//verilog/analysis:verilog-analyzer",
        "//verilog/parser:verilog-token-enum",
        "@com_google_googletest//:gtest",
        "@com_google_googletest//:gtest_main",
    ],
)

cc_library(
    name = "forbidden-symbol-rule",
    srcs = ["forbidden_symbol_rule.cc"],
    hdrs = ["forbidden_symbol_rule.h"],
    deps = [
        "//common/analysis:lint-rule-status",
        "//common/analysis:syntax-tree-lint-rule",
        "//common/analysis/matcher",
        "//common/analysis/matcher:bound-symbol-manager",
        "//common/text:concrete-syntax-leaf",
        "//common/text:symbol",
        "//common/text:syntax-tree-context",
        "//common/text:token-info",
        "//common/util:container-util",
        "//verilog/CST:verilog-matchers",
        "//verilog/analysis:descriptions",
        "//verilog/analysis:lint-rule-registry",
        "@com_google_absl//absl/strings:string_view",
    ],
    alwayslink = 1,
)

cc_test(
    name = "forbidden-symbol-rule_test",
    srcs = ["forbidden_symbol_rule_test.cc"],
    deps = [
        ":forbidden-symbol-rule",
        "//common/analysis:linter-test-utils",
        "//common/analysis:syntax-tree-linter-test-utils",
        "//verilog/analysis:verilog-analyzer",
        "//verilog/parser:verilog-token-enum",
        "@com_google_googletest//:gtest",
        "@com_google_googletest//:gtest_main",
    ],
)

cc_library(
    name = "generate-label-rule",
    srcs = ["generate_label_rule.cc"],
    hdrs = ["generate_label_rule.h"],
    deps = [
        "//common/analysis:lint-rule-status",
        "//common/analysis:syntax-tree-lint-rule",
        "//common/analysis/matcher",
        "//common/analysis/matcher:bound-symbol-manager",
        "//common/analysis/matcher:core-matchers",
        "//common/text:symbol",
        "//common/text:syntax-tree-context",
        "//verilog/CST:verilog-matchers",
        "//verilog/analysis:descriptions",
        "//verilog/analysis:lint-rule-registry",
        "@com_google_absl//absl/strings:string_view",
    ],
    alwayslink = 1,
)

cc_test(
    name = "generate-label-rule_test",
    srcs = ["generate_label_rule_test.cc"],
    deps = [
        ":generate-label-rule",
        "//common/analysis:linter-test-utils",
        "//common/analysis:syntax-tree-linter-test-utils",
        "//verilog/analysis:verilog-analyzer",
        "//verilog/parser:verilog-token-enum",
        "@com_google_googletest//:gtest",
        "@com_google_googletest//:gtest_main",
    ],
)

cc_library(
    name = "generate-label-prefix-rule",
    srcs = ["generate_label_prefix_rule.cc"],
    hdrs = ["generate_label_prefix_rule.h"],
    deps = [
        "//common/analysis:lint-rule-status",
        "//common/analysis:syntax-tree-lint-rule",
        "//common/analysis/matcher",
        "//common/analysis/matcher:bound-symbol-manager",
        "//common/text:symbol",
        "//common/text:syntax-tree-context",
        "//common/text:token-info",
        "//common/text:tree-utils",
        "//verilog/CST:seq-block",
        "//verilog/CST:verilog-matchers",
        "//verilog/CST:verilog-nonterminals",
        "//verilog/analysis:descriptions",
        "//verilog/analysis:lint-rule-registry",
        "@com_google_absl//absl/strings",
        "@com_google_absl//absl/strings:string_view",
    ],
    alwayslink = 1,
)

cc_test(
    name = "generate-label-prefix-rule_test",
    srcs = ["generate_label_prefix_rule_test.cc"],
    deps = [
        ":generate-label-prefix-rule",
        "//common/analysis:linter-test-utils",
        "//common/analysis:syntax-tree-linter-test-utils",
        "//verilog/analysis:verilog-analyzer",
        "//verilog/parser:verilog-token-enum",
        "@com_google_googletest//:gtest",
        "@com_google_googletest//:gtest_main",
    ],
)

cc_library(
    name = "v2001-generate-begin-rule",
    srcs = ["v2001_generate_begin_rule.cc"],
    hdrs = ["v2001_generate_begin_rule.h"],
    deps = [
        "//common/analysis:lint-rule-status",
        "//common/analysis:syntax-tree-lint-rule",
        "//common/analysis/matcher",
        "//common/analysis/matcher:bound-symbol-manager",
        "//common/text:concrete-syntax-leaf",
        "//common/text:concrete-syntax-tree",
        "//common/text:symbol",
        "//common/text:syntax-tree-context",
        "//common/text:tree-utils",
        "//verilog/CST:verilog-matchers",
        "//verilog/analysis:descriptions",
        "//verilog/analysis:lint-rule-registry",
        "@com_google_absl//absl/strings:string_view",
    ],
    alwayslink = 1,
)

cc_test(
    name = "v2001-generate-begin-rule_test",
    srcs = ["v2001_generate_begin_rule_test.cc"],
    deps = [
        ":forbidden-symbol-rule",
        ":v2001-generate-begin-rule",
        "//common/analysis:linter-test-utils",
        "//common/analysis:syntax-tree-linter-test-utils",
        "//verilog/analysis:verilog-analyzer",
        "//verilog/parser:verilog-token-enum",
        "@com_google_googletest//:gtest",
        "@com_google_googletest//:gtest_main",
    ],
)

cc_library(
    name = "void-cast-rule",
    srcs = ["void_cast_rule.cc"],
    hdrs = ["void_cast_rule.h"],
    deps = [
        "//common/analysis:lint-rule-status",
        "//common/analysis:syntax-tree-lint-rule",
        "//common/analysis/matcher",
        "//common/analysis/matcher:bound-symbol-manager",
        "//common/analysis/matcher:core-matchers",
        "//common/text:concrete-syntax-leaf",
        "//common/text:concrete-syntax-tree",
        "//common/text:symbol",
        "//common/text:syntax-tree-context",
        "//common/text:token-info",
        "//common/text:tree-utils",
        "//common/util:logging",
        "//verilog/CST:verilog-matchers",
        "//verilog/analysis:descriptions",
        "//verilog/analysis:lint-rule-registry",
        "@com_google_absl//absl/strings:string_view",
    ],
    alwayslink = 1,
)

cc_test(
    name = "void-cast-rule_test",
    srcs = ["void_cast_rule_test.cc"],
    deps = [
        ":void-cast-rule",
        "//common/analysis:linter-test-utils",
        "//common/analysis:syntax-tree-linter-test-utils",
        "//verilog/analysis:verilog-analyzer",
        "//verilog/parser:verilog-token-enum",
        "@com_google_googletest//:gtest",
        "@com_google_googletest//:gtest_main",
    ],
)

cc_library(
    name = "undersized-binary-literal-rule",
    srcs = ["undersized_binary_literal_rule.cc"],
    hdrs = ["undersized_binary_literal_rule.h"],
    deps = [
        "//common/analysis:lint-rule-status",
        "//common/analysis:syntax-tree-lint-rule",
        "//common/analysis/matcher",
        "//common/analysis/matcher:bound-symbol-manager",
        "//common/text:concrete-syntax-leaf",
        "//common/text:concrete-syntax-tree",
        "//common/text:config-utils",
        "//common/text:symbol",
        "//common/text:syntax-tree-context",
        "//common/text:token-info",
        "//common/util:logging",
        "//verilog/CST:numbers",
        "//verilog/CST:verilog-matchers",
        "//verilog/analysis:descriptions",
        "//verilog/analysis:lint-rule-registry",
        "@com_google_absl//absl/status",
        "@com_google_absl//absl/strings",
        "@com_google_absl//absl/strings:string_view",
    ],
    alwayslink = 1,
)

cc_test(
    name = "undersized-binary-literal-rule_test",
    srcs = ["undersized_binary_literal_rule_test.cc"],
    deps = [
        ":undersized-binary-literal-rule",
        "//common/analysis:linter-test-utils",
        "//common/analysis:syntax-tree-linter-test-utils",
        "//verilog/analysis:verilog-analyzer",
        "//verilog/parser:verilog-token-enum",
        "@com_google_absl//absl/status",
        "@com_google_googletest//:gtest",
        "@com_google_googletest//:gtest_main",
    ],
)

cc_library(
    name = "truncated-numeric-literal-rule",
    srcs = ["truncated_numeric_literal_rule.cc"],
    hdrs = ["truncated_numeric_literal_rule.h"],
    deps = [
        "//common/analysis:lint-rule-status",
        "//common/analysis:syntax-tree-lint-rule",
        "//common/analysis/matcher",
        "//common/analysis/matcher:bound-symbol-manager",
        "//common/text:concrete-syntax-leaf",
        "//common/text:concrete-syntax-tree",
        "//common/text:symbol",
        "//common/text:syntax-tree-context",
        "//common/text:token-info",
        "//verilog/CST:numbers",
        "//verilog/CST:verilog-matchers",
        "//verilog/analysis:descriptions",
        "//verilog/analysis:lint-rule-registry",
        "@com_google_absl//absl/numeric:int128",
        "@com_google_absl//absl/strings",
        "@com_google_absl//absl/strings:string_view",
    ],
    alwayslink = 1,
)

cc_test(
    name = "truncated-numeric-literal-rule_test",
    srcs = ["truncated_numeric_literal_rule_test.cc"],
    deps = [
        ":truncated-numeric-literal-rule",
        "//common/analysis:linter-test-utils",
        "//common/analysis:syntax-tree-linter-test-utils",
        "//verilog/analysis:verilog-analyzer",
        "//verilog/parser:verilog-token-enum",
        "@com_google_absl//absl/strings",
        "@com_google_absl//absl/strings:string_view",
        "@com_google_googletest//:gtest",
        "@com_google_googletest//:gtest_main",
    ],
)

cc_library(
    name = "create-object-name-match-rule",
    srcs = ["create_object_name_match_rule.cc"],
    hdrs = ["create_object_name_match_rule.h"],
    deps = [
        "//common/analysis:lint-rule-status",
        "//common/analysis:syntax-tree-lint-rule",
        "//common/analysis/matcher",
        "//common/analysis/matcher:bound-symbol-manager",
        "//common/text:concrete-syntax-leaf",
        "//common/text:concrete-syntax-tree",
        "//common/text:symbol",
        "//common/text:syntax-tree-context",
        "//common/text:token-info",
        "//common/util:casts",
        "//verilog/CST:expression",
        "//verilog/CST:verilog-matchers",
        "//verilog/CST:verilog-nonterminals",
        "//verilog/analysis:descriptions",
        "//verilog/analysis:lint-rule-registry",
        "//verilog/parser:verilog-token-enum",
        "@com_google_absl//absl/strings",
        "@com_google_absl//absl/strings:string_view",
    ],
    alwayslink = 1,
)

cc_test(
    name = "create-object-name-match-rule_test",
    srcs = ["create_object_name_match_rule_test.cc"],
    deps = [
        ":create-object-name-match-rule",
        "//common/analysis:linter-test-utils",
        "//common/analysis:syntax-tree-linter-test-utils",
        "//verilog/analysis:verilog-analyzer",
        "//verilog/parser:verilog-token-enum",
        "@com_google_googletest//:gtest",
        "@com_google_googletest//:gtest_main",
    ],
)

cc_library(
    name = "always-comb-rule",
    srcs = ["always_comb_rule.cc"],
    hdrs = ["always_comb_rule.h"],
    deps = [
        "//common/analysis:lint-rule-status",
        "//common/analysis:syntax-tree-lint-rule",
        "//common/analysis/matcher",
        "//common/analysis/matcher:bound-symbol-manager",
        "//common/text:concrete-syntax-leaf",
        "//common/text:concrete-syntax-tree",
        "//common/text:symbol",
        "//common/text:syntax-tree-context",
        "//common/text:tree-utils",
        "//verilog/CST:statement",
        "//verilog/CST:verilog-matchers",
        "//verilog/analysis:descriptions",
        "//verilog/analysis:lint-rule-registry",
        "@com_google_absl//absl/strings:string_view",
    ],
    alwayslink = 1,
)

cc_test(
    name = "always-comb-rule_test",
    srcs = ["always_comb_rule_test.cc"],
    deps = [
        ":always-comb-rule",
        "//common/analysis:linter-test-utils",
        "//common/analysis:syntax-tree-linter-test-utils",
        "//verilog/analysis:verilog-analyzer",
        "//verilog/parser:verilog-token-enum",
        "@com_google_googletest//:gtest",
        "@com_google_googletest//:gtest_main",
    ],
)

cc_library(
    name = "token-stream-lint-rule",
    srcs = ["token_stream_lint_rule.cc"],
    hdrs = ["token_stream_lint_rule.h"],
    deps = [
        "//common/analysis:lint-rule-status",
        "//common/analysis:syntax-tree-lint-rule",
        "//common/analysis/matcher",
        "//common/analysis/matcher:bound-symbol-manager",
        "//common/text:symbol",
        "//common/text:syntax-tree-context",
        "//common/text:tree-utils",
        "//verilog/CST:verilog-matchers",
        "//verilog/analysis:descriptions",
        "//verilog/analysis:lint-rule-registry",
        "//verilog/parser:verilog-token-enum",
        "@com_google_absl//absl/strings",
        "@com_google_absl//absl/strings:string_view",
    ],
    alwayslink = 1,
)

cc_test(
    name = "token-stream-lint-rule_test",
    srcs = ["token_stream_lint_rule_test.cc"],
    deps = [
        ":token-stream-lint-rule",
        "//common/analysis:linter-test-utils",
        "//common/analysis:syntax-tree-linter-test-utils",
        "//verilog/analysis:verilog-analyzer",
        "//verilog/parser:verilog-token-enum",
        "@com_google_googletest//:gtest",
        "@com_google_googletest//:gtest_main",
    ],
)

cc_library(
    name = "suggest-parentheses-rule",
    srcs = ["suggest_parentheses_rule.cc"],
    hdrs = ["suggest_parentheses_rule.h"],
    deps = [
        "//common/analysis:lint-rule-status",
        "//common/analysis:syntax-tree-lint-rule",
        "//common/text:concrete-syntax-tree",
        "//common/text:symbol",
        "//common/text:syntax-tree-context",
        "//common/text:token-info",
        "//common/text:tree-utils",
        "//verilog/CST:expression",
        "//verilog/CST:verilog-nonterminals",
        "//verilog/analysis:descriptions",
        "//verilog/analysis:lint-rule-registry",
        "//verilog/parser:verilog-token-enum",
        "@com_google_absl//absl/strings:string_view",
    ],
    alwayslink = 1,
)

cc_test(
    name = "suggest-parentheses-rule_test",
    srcs = ["suggest_parentheses_rule_test.cc"],
    deps = [
        ":suggest-parentheses-rule",
        "//common/analysis:linter-test-utils",
        "//common/analysis:syntax-tree-linter-test-utils",
        "//verilog/analysis:verilog-analyzer",
        "@com_google_googletest//:gtest",
        "@com_google_googletest//:gtest_main",
    ],
)

cc_library(
    name = "always-comb-blocking-rule",
    srcs = ["always_comb_blocking_rule.cc"],
    hdrs = ["always_comb_blocking_rule.h"],
    deps = [
        "//common/analysis:lint-rule-status",
        "//common/analysis:syntax-tree-lint-rule",
        "//common/analysis:syntax-tree-search",
        "//common/analysis/matcher",
        "//common/analysis/matcher:bound-symbol-manager",
        "//common/text:concrete-syntax-leaf",
        "//common/text:concrete-syntax-tree",
        "//common/text:symbol",
        "//common/text:syntax-tree-context",
        "//common/text:tree-utils",
        "//common/util:casts",
        "//verilog/CST:verilog-matchers",
        "//verilog/CST:verilog-nonterminals",
        "//verilog/analysis:descriptions",
        "//verilog/analysis:lint-rule-registry",
        "//verilog/parser:verilog-token-enum",
        "@com_google_absl//absl/strings:string_view",
    ],
    alwayslink = 1,
)

cc_test(
    name = "always-comb-blocking-rule_test",
    srcs = ["always_comb_blocking_rule_test.cc"],
    deps = [
        ":always-comb-blocking-rule",
        "//common/analysis:linter-test-utils",
        "//common/analysis:syntax-tree-linter-test-utils",
        "//verilog/analysis:verilog-analyzer",
        "//verilog/parser:verilog-token-enum",
        "@com_google_googletest//:gtest",
        "@com_google_googletest//:gtest_main",
    ],
)

cc_library(
    name = "always-ff-non-blocking-rule",
    srcs = ["always_ff_non_blocking_rule.cc"],
    hdrs = ["always_ff_non_blocking_rule.h"],
    deps = [
        "//common/analysis:lint-rule-status",
        "//common/analysis:syntax-tree-lint-rule",
        "//common/analysis:syntax-tree-search",
        "//common/analysis/matcher",
        "//common/analysis/matcher:bound-symbol-manager",
        "//common/text:concrete-syntax-leaf",
        "//common/text:concrete-syntax-tree",
        "//common/text:config-utils",
        "//common/text:symbol",
        "//common/text:syntax-tree-context",
        "//common/util:casts",
        "//common/util:logging",
        "//verilog/CST:verilog-matchers",
        "//verilog/CST:verilog-nonterminals",
        "//verilog/analysis:descriptions",
        "//verilog/analysis:lint-rule-registry",
        "@com_google_absl//absl/status",
        "@com_google_absl//absl/strings:string_view",
    ],
    alwayslink = 1,
)

cc_test(
    name = "always-ff-non-blocking-rule_test",
    srcs = ["always_ff_non_blocking_rule_test.cc"],
    deps = [
        ":always-ff-non-blocking-rule",
        "//common/analysis:linter-test-utils",
        "//common/analysis:syntax-tree-linter-test-utils",
        "//verilog/analysis:verilog-analyzer",
        "//verilog/parser:verilog-token-enum",
        "@com_google_googletest//:gtest",
        "@com_google_googletest//:gtest_main",
    ],
)

cc_library(
    name = "explicit-function-lifetime-rule",
    srcs = ["explicit_function_lifetime_rule.cc"],
    hdrs = ["explicit_function_lifetime_rule.h"],
    deps = [
        "//common/analysis:lint-rule-status",
        "//common/analysis:syntax-tree-lint-rule",
        "//common/analysis/matcher",
        "//common/analysis/matcher:bound-symbol-manager",
        "//common/text:symbol",
        "//common/text:syntax-tree-context",
        "//common/text:token-info",
        "//common/text:tree-utils",
        "//common/util:logging",
        "//verilog/CST:context-functions",
        "//verilog/CST:functions",
        "//verilog/CST:identifier",
        "//verilog/CST:verilog-matchers",
        "//verilog/analysis:descriptions",
        "//verilog/analysis:lint-rule-registry",
        "//verilog/parser:verilog-token-enum",
        "@com_google_absl//absl/strings:string_view",
    ],
    alwayslink = 1,
)

cc_test(
    name = "explicit-function-lifetime-rule_test",
    srcs = ["explicit_function_lifetime_rule_test.cc"],
    deps = [
        ":explicit-function-lifetime-rule",
        "//common/analysis:linter-test-utils",
        "//common/analysis:syntax-tree-linter-test-utils",
        "//verilog/analysis:verilog-analyzer",
        "//verilog/parser:verilog-token-enum",
        "@com_google_googletest//:gtest",
        "@com_google_googletest//:gtest_main",
    ],
)

cc_library(
    name = "explicit-task-lifetime-rule",
    srcs = ["explicit_task_lifetime_rule.cc"],
    hdrs = ["explicit_task_lifetime_rule.h"],
    deps = [
        "//common/analysis:lint-rule-status",
        "//common/analysis:syntax-tree-lint-rule",
        "//common/analysis/matcher",
        "//common/analysis/matcher:bound-symbol-manager",
        "//common/text:symbol",
        "//common/text:syntax-tree-context",
        "//common/text:token-info",
        "//common/text:tree-utils",
        "//verilog/CST:context-functions",
        "//verilog/CST:identifier",
        "//verilog/CST:tasks",
        "//verilog/CST:verilog-matchers",
        "//verilog/analysis:descriptions",
        "//verilog/analysis:lint-rule-registry",
        "//verilog/parser:verilog-token-enum",
        "@com_google_absl//absl/strings:string_view",
    ],
    alwayslink = 1,
)

cc_test(
    name = "explicit-task-lifetime-rule_test",
    srcs = ["explicit_task_lifetime_rule_test.cc"],
    deps = [
        ":explicit-task-lifetime-rule",
        "//common/analysis:linter-test-utils",
        "//common/analysis:syntax-tree-linter-test-utils",
        "//verilog/analysis:verilog-analyzer",
        "//verilog/parser:verilog-token-enum",
        "@com_google_googletest//:gtest",
        "@com_google_googletest//:gtest_main",
    ],
)

cc_library(
    name = "packed-dimensions-rule",
    srcs = ["packed_dimensions_rule.cc"],
    hdrs = ["packed_dimensions_rule.h"],
    deps = [
        "//common/analysis:lint-rule-status",
        "//common/analysis:syntax-tree-lint-rule",
        "//common/analysis/matcher",
        "//common/analysis/matcher:bound-symbol-manager",
        "//common/text:symbol",
        "//common/text:syntax-tree-context",
        "//common/text:token-info",
        "//common/text:tree-utils",
        "//common/util:logging",
        "//verilog/CST:context-functions",
        "//verilog/CST:dimensions",
        "//verilog/CST:expression",
        "//verilog/CST:verilog-matchers",
        "//verilog/analysis:descriptions",
        "//verilog/analysis:lint-rule-registry",
        "//verilog/parser:verilog-token-enum",
        "@com_google_absl//absl/strings:string_view",
    ],
    alwayslink = 1,
)

cc_test(
    name = "packed-dimensions-rule_test",
    srcs = ["packed_dimensions_rule_test.cc"],
    deps = [
        ":packed-dimensions-rule",
        "//common/analysis:linter-test-utils",
        "//common/analysis:syntax-tree-linter-test-utils",
        "//verilog/analysis:verilog-analyzer",
        "//verilog/parser:verilog-token-enum",
        "@com_google_googletest//:gtest",
        "@com_google_googletest//:gtest_main",
    ],
)

cc_library(
    name = "unpacked-dimensions-rule",
    srcs = ["unpacked_dimensions_rule.cc"],
    hdrs = ["unpacked_dimensions_rule.h"],
    deps = [
        "//common/analysis:lint-rule-status",
        "//common/analysis:syntax-tree-lint-rule",
        "//common/analysis/matcher",
        "//common/analysis/matcher:bound-symbol-manager",
        "//common/text:symbol",
        "//common/text:syntax-tree-context",
        "//common/text:token-info",
        "//common/text:tree-utils",
        "//common/util:logging",
        "//verilog/CST:context-functions",
        "//verilog/CST:dimensions",
        "//verilog/CST:expression",
        "//verilog/CST:verilog-matchers",
        "//verilog/CST:verilog-nonterminals",
        "//verilog/analysis:descriptions",
        "//verilog/analysis:lint-rule-registry",
        "//verilog/parser:verilog-token-enum",
        "@com_google_absl//absl/strings:string_view",
    ],
    alwayslink = 1,
)

cc_test(
    name = "unpacked-dimensions-rule_test",
    srcs = ["unpacked_dimensions_rule_test.cc"],
    deps = [
        ":unpacked-dimensions-rule",
        "//common/analysis:linter-test-utils",
        "//common/analysis:syntax-tree-linter-test-utils",
        "//verilog/analysis:verilog-analyzer",
        "//verilog/parser:verilog-token-enum",
        "@com_google_googletest//:gtest",
        "@com_google_googletest//:gtest_main",
    ],
)

cc_library(
    name = "forbid-negative-array-dim",
    srcs = ["forbid_negative_array_dim.cc"],
    hdrs = ["forbid_negative_array_dim.h"],
    deps = [
        "//common/analysis:lint-rule-status",
        "//common/analysis:syntax-tree-lint-rule",
        "//common/analysis/matcher",
        "//common/analysis/matcher:bound-symbol-manager",
        "//common/text:symbol",
        "//common/text:syntax-tree-context",
        "//common/text:token-info",
        "//common/text:tree-utils",
        "//verilog/CST:expression",
        "//verilog/CST:verilog-matchers",
        "//verilog/CST:verilog-nonterminals",
        "//verilog/analysis:descriptions",
        "//verilog/analysis:lint-rule-registry",
        "//verilog/parser:verilog-token-enum",
        "@com_google_absl//absl/strings:string_view",
    ],
    alwayslink = 1,
)

cc_test(
    name = "forbid-negative-array-dim_test",
    srcs = ["forbid_negative_array_dim_test.cc"],
    deps = [
        ":forbid-negative-array-dim",
        "//common/analysis:linter-test-utils",
        "//common/analysis:syntax-tree-linter-test-utils",
        "//verilog/analysis:verilog-analyzer",
        "//verilog/parser:verilog-token-enum",
        "@com_google_googletest//:gtest",
        "@com_google_googletest//:gtest_main",
    ],
)

cc_library(
    name = "case-missing-default-rule",
    srcs = ["case_missing_default_rule.cc"],
    hdrs = ["case_missing_default_rule.h"],
    deps = [
        "//common/analysis:lint-rule-status",
        "//common/analysis:syntax-tree-lint-rule",
        "//common/analysis/matcher",
        "//common/analysis/matcher:bound-symbol-manager",
        "//common/analysis/matcher:matcher-builders",
        "//common/text:concrete-syntax-tree",
        "//common/text:symbol",
        "//common/text:syntax-tree-context",
        "//common/text:tree-utils",
        "//verilog/CST:verilog-matchers",
        "//verilog/CST:verilog-nonterminals",
        "//verilog/analysis:descriptions",
        "//verilog/analysis:lint-rule-registry",
        "@com_google_absl//absl/strings:string_view",
    ],
    alwayslink = 1,
)

cc_test(
    name = "case-missing-default-rule_test",
    srcs = ["case_missing_default_rule_test.cc"],
    deps = [
        ":case-missing-default-rule",
        "//common/analysis:linter-test-utils",
        "//common/analysis:syntax-tree-linter-test-utils",
        "//verilog/analysis:verilog-analyzer",
        "//verilog/parser:verilog-token-enum",
        "@com_google_googletest//:gtest",
        "@com_google_googletest//:gtest_main",
    ],
)

cc_library(
    name = "constraint-name-style-rule",
    srcs = ["constraint_name_style_rule.cc"],
    hdrs = ["constraint_name_style_rule.h"],
    deps = [
        "//common/analysis:lint-rule-status",
        "//common/analysis:syntax-tree-lint-rule",
        "//common/analysis/matcher",
        "//common/analysis/matcher:bound-symbol-manager",
        "//common/strings:naming-utils",
        "//common/text:symbol",
        "//common/text:syntax-tree-context",
        "//common/text:token-info",
        "//verilog/CST:constraints",
        "//verilog/CST:verilog-matchers",
        "//verilog/analysis:descriptions",
        "//verilog/analysis:lint-rule-registry",
        "@com_google_absl//absl/strings",
        "@com_google_absl//absl/strings:string_view",
    ],
    alwayslink = 1,
)

cc_test(
    name = "constraint-name-style-rule_test",
    srcs = ["constraint_name_style_rule_test.cc"],
    deps = [
        ":constraint-name-style-rule",
        "//common/analysis:linter-test-utils",
        "//common/analysis:syntax-tree-linter-test-utils",
        "//verilog/analysis:verilog-analyzer",
        "//verilog/parser:verilog-token-enum",
        "@com_google_googletest//:gtest",
        "@com_google_googletest//:gtest_main",
    ],
)

cc_library(
    name = "disable-statement-rule",
    srcs = ["disable_statement_rule.cc"],
    hdrs = ["disable_statement_rule.h"],
    deps = [
        "//common/analysis:lint-rule-status",
        "//common/analysis:syntax-tree-lint-rule",
        "//common/analysis/matcher",
        "//common/analysis/matcher:bound-symbol-manager",
        "//common/text:symbol",
        "//common/text:syntax-tree-context",
        "//common/text:tree-utils",
        "//common/util:iterator-adaptors",
        "//verilog/CST:identifier",
        "//verilog/CST:verilog-matchers",
        "//verilog/CST:verilog-nonterminals",
        "//verilog/analysis:descriptions",
        "//verilog/analysis:lint-rule-registry",
        "@com_google_absl//absl/strings:string_view",
    ],
    alwayslink = 1,
)

cc_test(
    name = "disable-statement-rule_test",
    srcs = ["disable_statement_rule_test.cc"],
    deps = [
        ":disable-statement-rule",
        "//common/analysis:linter-test-utils",
        "//common/analysis:syntax-tree-linter-test-utils",
        "//verilog/analysis:verilog-analyzer",
        "//verilog/parser:verilog-token-enum",
        "@com_google_googletest//:gtest",
        "@com_google_googletest//:gtest_main",
    ],
)

cc_library(
    name = "macro-name-style-rule",
    srcs = ["macro_name_style_rule.cc"],
    hdrs = ["macro_name_style_rule.h"],
    deps = [
        "//common/analysis:lint-rule-status",
        "//common/analysis:token-stream-lint-rule",
        "//common/strings:naming-utils",
        "//common/text:token-info",
        "//verilog/analysis:descriptions",
        "//verilog/analysis:lint-rule-registry",
        "//verilog/parser:verilog-lexer",
        "//verilog/parser:verilog-token-classifications",
        "//verilog/parser:verilog-token-enum",
        "@com_google_absl//absl/strings",
        "@com_google_absl//absl/strings:string_view",
    ],
    alwayslink = 1,
)

cc_test(
    name = "macro-name-style-rule_test",
    srcs = ["macro_name_style_rule_test.cc"],
    deps = [
        ":macro-name-style-rule",
        "//common/analysis:linter-test-utils",
        "//common/analysis:token-stream-linter-test-utils",
        "//verilog/analysis:verilog-analyzer",
        "//verilog/parser:verilog-token-enum",
        "@com_google_googletest//:gtest",
        "@com_google_googletest//:gtest_main",
    ],
)

cc_library(
    name = "macro-string-concatenation-rule",
    srcs = ["macro_string_concatenation_rule.cc"],
    hdrs = ["macro_string_concatenation_rule.h"],
    deps = [
        "//common/analysis:lint-rule-status",
        "//common/analysis:token-stream-lint-rule",
        "//common/text:token-info",
        "//common/util:value-saver",
        "//verilog/analysis:descriptions",
        "//verilog/analysis:lint-rule-registry",
        "//verilog/parser:verilog-lexer",
        "//verilog/parser:verilog-token-classifications",
        "//verilog/parser:verilog-token-enum",
        "@com_google_absl//absl/strings:string_view",
    ],
    alwayslink = 1,
)

cc_test(
    name = "macro-string-concatenation-rule_test",
    srcs = ["macro_string_concatenation_rule_test.cc"],
    deps = [
        ":macro-string-concatenation-rule",
        "//common/analysis:linter-test-utils",
        "//common/analysis:token-stream-linter-test-utils",
        "//verilog/analysis:verilog-analyzer",
        "//verilog/parser:verilog-token-enum",
        "@com_google_googletest//:gtest",
        "@com_google_googletest//:gtest_main",
    ],
)

cc_library(
    name = "numeric-format-string-style-rule",
    srcs = ["numeric_format_string_style_rule.cc"],
    hdrs = ["numeric_format_string_style_rule.h"],
    deps = [
        "//common/analysis:lint-rule-status",
        "//common/analysis:token-stream-lint-rule",
        "//common/text:token-info",
        "//verilog/analysis:descriptions",
        "//verilog/analysis:lint-rule-registry",
        "//verilog/parser:verilog-lexer",
        "//verilog/parser:verilog-token-classifications",
        "//verilog/parser:verilog-token-enum",
        "@com_google_absl//absl/strings",
        "@com_google_absl//absl/strings:string_view",
    ],
    alwayslink = 1,
)

cc_test(
    name = "numeric-format-string-style-rule_test",
    srcs = ["numeric_format_string_style_rule_test.cc"],
    deps = [
        ":numeric-format-string-style-rule",
        "//common/analysis:linter-test-utils",
        "//common/analysis:token-stream-linter-test-utils",
        "//verilog/analysis:verilog-analyzer",
        "//verilog/parser:verilog-token-enum",
        "@com_google_googletest//:gtest",
        "@com_google_googletest//:gtest_main",
    ],
)

cc_library(
    name = "parameter-name-style-rule",
    srcs = ["parameter_name_style_rule.cc"],
    hdrs = ["parameter_name_style_rule.h"],
    deps = [
        "//common/analysis:lint-rule-status",
        "//common/analysis:syntax-tree-lint-rule",
        "//common/analysis/matcher",
        "//common/analysis/matcher:bound-symbol-manager",
        "//common/strings:naming-utils",
        "//common/text:config-utils",
        "//common/text:symbol",
        "//common/text:syntax-tree-context",
        "//common/text:token-info",
        "//verilog/CST:parameters",
        "//verilog/CST:verilog-matchers",
        "//verilog/analysis:descriptions",
        "//verilog/analysis:lint-rule-registry",
        "//verilog/parser:verilog-token-enum",
        "@com_google_absl//absl/status",
        "@com_google_absl//absl/strings",
        "@com_google_absl//absl/strings:string_view",
    ],
    alwayslink = 1,
)

cc_test(
    name = "parameter-name-style-rule_test",
    srcs = ["parameter_name_style_rule_test.cc"],
    deps = [
        ":parameter-name-style-rule",
        "//common/analysis:linter-test-utils",
        "//common/analysis:syntax-tree-linter-test-utils",
        "//verilog/analysis:verilog-analyzer",
        "//verilog/parser:verilog-token-enum",
        "@com_google_googletest//:gtest",
        "@com_google_googletest//:gtest_main",
    ],
)

cc_library(
    name = "parameter-type-name-style-rule",
    srcs = ["parameter_type_name_style_rule.cc"],
    hdrs = ["parameter_type_name_style_rule.h"],
    deps = [
        "//common/analysis:lint-rule-status",
        "//common/analysis:syntax-tree-lint-rule",
        "//common/analysis/matcher",
        "//common/analysis/matcher:bound-symbol-manager",
        "//common/strings:naming-utils",
        "//common/text:symbol",
        "//common/text:syntax-tree-context",
        "//common/text:token-info",
        "//verilog/CST:parameters",
        "//verilog/CST:verilog-matchers",
        "//verilog/analysis:descriptions",
        "//verilog/analysis:lint-rule-registry",
        "@com_google_absl//absl/strings",
        "@com_google_absl//absl/strings:string_view",
    ],
    alwayslink = 1,
)

cc_test(
    name = "parameter-type-name-style-rule_test",
    srcs = ["parameter_type_name_style_rule_test.cc"],
    deps = [
        ":parameter-type-name-style-rule",
        "//common/analysis:linter-test-utils",
        "//common/analysis:syntax-tree-linter-test-utils",
        "//verilog/analysis:verilog-analyzer",
        "//verilog/parser:verilog-token-enum",
        "@com_google_googletest//:gtest",
        "@com_google_googletest//:gtest_main",
    ],
)

cc_library(
    name = "proper-parameter-declaration-rule",
    srcs = ["proper_parameter_declaration_rule.cc"],
    hdrs = ["proper_parameter_declaration_rule.h"],
    deps = [
        "//common/analysis:lint-rule-status",
        "//common/analysis:syntax-tree-lint-rule",
        "//common/analysis/matcher",
        "//common/analysis/matcher:bound-symbol-manager",
        "//common/text:config-utils",
        "//common/text:symbol",
        "//common/text:syntax-tree-context",
        "//verilog/CST:context-functions",
        "//verilog/CST:parameters",
        "//verilog/CST:verilog-matchers",
        "//verilog/analysis:descriptions",
        "//verilog/analysis:lint-rule-registry",
        "//verilog/parser:verilog-token-enum",
        "@com_google_absl//absl/status",
        "@com_google_absl//absl/strings:string_view",
    ],
    alwayslink = 1,
)

cc_test(
    name = "proper-parameter-declaration-rule_test",
    srcs = ["proper_parameter_declaration_rule_test.cc"],
    deps = [
        ":proper-parameter-declaration-rule",
        "//common/analysis:linter-test-utils",
        "//common/analysis:syntax-tree-linter-test-utils",
        "//verilog/analysis:verilog-analyzer",
        "//verilog/parser:verilog-token-enum",
        "@com_google_googletest//:gtest",
        "@com_google_googletest//:gtest_main",
    ],
)

cc_library(
    name = "plusarg-assignment-rule",
    srcs = ["plusarg_assignment_rule.cc"],
    hdrs = ["plusarg_assignment_rule.h"],
    deps = [
        "//common/analysis:lint-rule-status",
        "//common/analysis:syntax-tree-lint-rule",
        "//common/analysis/matcher",
        "//common/analysis/matcher:bound-symbol-manager",
        "//common/text:concrete-syntax-leaf",
        "//common/text:symbol",
        "//common/text:syntax-tree-context",
        "//common/text:token-info",
        "//verilog/CST:verilog-matchers",
        "//verilog/analysis:descriptions",
        "//verilog/analysis:lint-rule-registry",
        "@com_google_absl//absl/strings",
        "@com_google_absl//absl/strings:string_view",
    ],
    alwayslink = 1,
)

cc_test(
    name = "plusarg-assignment-rule_test",
    srcs = ["plusarg_assignment_rule_test.cc"],
    deps = [
        ":plusarg-assignment-rule",
        "//common/analysis:linter-test-utils",
        "//common/analysis:syntax-tree-linter-test-utils",
        "//verilog/analysis:verilog-analyzer",
        "//verilog/parser:verilog-token-enum",
        "@com_google_googletest//:gtest",
        "@com_google_googletest//:gtest_main",
    ],
)

cc_library(
    name = "port-name-suffix-rule",
    srcs = ["port_name_suffix_rule.cc"],
    hdrs = ["port_name_suffix_rule.h"],
    deps = [
        "//common/analysis:lint-rule-status",
        "//common/analysis:syntax-tree-lint-rule",
        "//common/analysis/matcher",
        "//common/analysis/matcher:bound-symbol-manager",
        "//common/text:symbol",
        "//common/text:syntax-tree-context",
        "//common/text:token-info",
        "//common/util:logging",
        "//verilog/CST:port",
        "//verilog/CST:verilog-matchers",
        "//verilog/analysis:descriptions",
        "//verilog/analysis:lint-rule-registry",
        "@com_google_absl//absl/strings",
        "@com_google_absl//absl/strings:string_view",
    ],
    alwayslink = 1,
)

cc_test(
    name = "port-name-suffix-rule_test",
    srcs = ["port_name_suffix_rule_test.cc"],
    deps = [
        ":port-name-suffix-rule",
        "//common/analysis:linter-test-utils",
        "//common/analysis:syntax-tree-linter-test-utils",
        "//verilog/analysis:verilog-analyzer",
        "//verilog/parser:verilog-token-enum",
        "@com_google_googletest//:gtest",
        "@com_google_googletest//:gtest_main",
    ],
)

cc_library(
    name = "explicit-parameter-storage-type-rule",
    srcs = ["explicit_parameter_storage_type_rule.cc"],
    hdrs = ["explicit_parameter_storage_type_rule.h"],
    deps = [
        "//common/analysis:lint-rule-status",
        "//common/analysis:syntax-tree-lint-rule",
        "//common/analysis/matcher",
        "//common/analysis/matcher:bound-symbol-manager",
        "//common/text:config-utils",
        "//common/text:symbol",
        "//common/text:syntax-tree-context",
        "//common/text:token-info",
        "//common/text:tree-utils",
        "//common/util:logging",
        "//verilog/CST:parameters",
        "//verilog/CST:verilog-matchers",  # fixdeps: keep
        "//verilog/analysis:descriptions",
        "//verilog/analysis:lint-rule-registry",
        "//verilog/parser:verilog-token-enum",
        "@com_google_absl//absl/status",
        "@com_google_absl//absl/strings",
        "@com_google_absl//absl/strings:string_view",
    ],
    alwayslink = 1,
)

cc_test(
    name = "explicit-parameter-storage-type-rule_test",
    srcs = ["explicit_parameter_storage_type_rule_test.cc"],
    deps = [
        ":explicit-parameter-storage-type-rule",
        "//common/analysis:linter-test-utils",
        "//common/analysis:syntax-tree-linter-test-utils",
        "//verilog/analysis:verilog-analyzer",
        "//verilog/parser:verilog-token-enum",
        "@com_google_absl//absl/status",
        "@com_google_absl//absl/strings",
        "@com_google_googletest//:gtest",
        "@com_google_googletest//:gtest_main",
    ],
)

cc_library(
    name = "explicit-function-task-parameter-type-rule",
    srcs = ["explicit_function_task_parameter_type_rule.cc"],
    hdrs = ["explicit_function_task_parameter_type_rule.h"],
    deps = [
        "//common/analysis:lint-rule-status",
        "//common/analysis:syntax-tree-lint-rule",
        "//common/analysis/matcher",
        "//common/analysis/matcher:bound-symbol-manager",
        "//common/text:symbol",
        "//common/text:syntax-tree-context",
        "//common/util:logging",
        "//verilog/CST:port",
        "//verilog/CST:type",
        "//verilog/CST:verilog-matchers",
        "//verilog/analysis:descriptions",
        "//verilog/analysis:lint-rule-registry",
        "@com_google_absl//absl/strings:string_view",
    ],
    alwayslink = 1,
)

cc_test(
    name = "explicit-function-task-parameter-type-rule_test",
    srcs = ["explicit_function_task_parameter_type_rule_test.cc"],
    deps = [
        ":explicit-function-task-parameter-type-rule",
        "//common/analysis:linter-test-utils",
        "//common/analysis:syntax-tree-linter-test-utils",
        "//verilog/analysis:verilog-analyzer",
        "//verilog/parser:verilog-token-enum",
        "@com_google_googletest//:gtest",
        "@com_google_googletest//:gtest_main",
    ],
)

cc_library(
    name = "signal-name-style-rule",
    srcs = ["signal_name_style_rule.cc"],
    hdrs = ["signal_name_style_rule.h"],
    deps = [
        "//common/analysis:lint-rule-status",
        "//common/analysis:syntax-tree-lint-rule",
        "//common/analysis/matcher",
        "//common/analysis/matcher:bound-symbol-manager",
        "//common/strings:naming-utils",
        "//common/text:concrete-syntax-leaf",
        "//common/text:symbol",
        "//common/text:syntax-tree-context",
        "//common/text:token-info",
        "//common/util:logging",
        "//verilog/CST:data",
        "//verilog/CST:net",
        "//verilog/CST:port",
        "//verilog/CST:verilog-matchers",
        "//verilog/analysis:descriptions",
        "//verilog/analysis:lint-rule-registry",
        "@com_google_absl//absl/strings:string_view",
    ],
    alwayslink = 1,
)

cc_test(
    name = "signal-name-style-rule_test",
    srcs = ["signal_name_style_rule_test.cc"],
    deps = [
        ":signal-name-style-rule",
        "//common/analysis:linter-test-utils",
        "//common/analysis:syntax-tree-linter-test-utils",
        "//verilog/analysis:verilog-analyzer",
        "//verilog/parser:verilog-token-enum",
        "@com_google_googletest//:gtest",
        "@com_google_googletest//:gtest_main",
    ],
)

cc_library(
    name = "struct-union-name-style-rule",
    srcs = ["struct_union_name_style_rule.cc"],
    hdrs = ["struct_union_name_style_rule.h"],
    deps = [
        "//common/analysis:lint-rule-status",
        "//common/analysis:syntax-tree-lint-rule",
        "//common/analysis/matcher",
        "//common/analysis/matcher:bound-symbol-manager",
        "//common/text:config-utils",
        "//common/text:symbol",
        "//common/text:syntax-tree-context",
        "//common/util:logging",
        "//verilog/CST:type",
        "//verilog/CST:verilog-matchers",
        "//verilog/analysis:descriptions",
        "//verilog/analysis:lint-rule-registry",
        "@com_google_absl//absl/status",
        "@com_google_absl//absl/strings",
        "@com_google_absl//absl/strings:string_view",
    ],
    alwayslink = 1,
)

cc_test(
    name = "struct-union-name-style-rule_test",
    srcs = ["struct_union_name_style_rule_test.cc"],
    deps = [
        ":struct-union-name-style-rule",
        "//common/analysis:linter-test-utils",
        "//common/analysis:syntax-tree-linter-test-utils",
        "//verilog/analysis:verilog-analyzer",
        "//verilog/parser:verilog-token-enum",
        "@com_google_absl//absl/status",
        "@com_google_absl//absl/strings",
        "@com_google_absl//absl/strings:string_view",
        "@com_google_googletest//:gtest",
        "@com_google_googletest//:gtest_main",
    ],
)

cc_library(
    name = "interface-name-style-rule",
    srcs = ["interface_name_style_rule.cc"],
    hdrs = ["interface_name_style_rule.h"],
    deps = [
        "//common/analysis:lint-rule-status",
        "//common/analysis:syntax-tree-lint-rule",
        "//common/analysis/matcher",
        "//common/analysis/matcher:bound-symbol-manager",
        "//common/strings:naming-utils",
        "//common/text:symbol",
        "//common/text:syntax-tree-context",
        "//common/text:token-info",
        "//verilog/CST:module",
        "//verilog/CST:type",
        "//verilog/CST:verilog-matchers",
        "//verilog/analysis:descriptions",
        "//verilog/analysis:lint-rule-registry",
        "@com_google_absl//absl/strings",
        "@com_google_absl//absl/strings:string_view",
    ],
    alwayslink = 1,
)

cc_test(
    name = "interface-name-style-rule_test",
    srcs = ["interface_name_style_rule_test.cc"],
    deps = [
        ":interface-name-style-rule",
        "//common/analysis:linter-test-utils",
        "//common/analysis:syntax-tree-linter-test-utils",
        "//verilog/analysis:verilog-analyzer",
        "//verilog/parser:verilog-token-enum",
        "@com_google_googletest//:gtest",
        "@com_google_googletest//:gtest_main",
    ],
)

cc_library(
    name = "legacy-genvar-declaration-rule",
    srcs = ["legacy_genvar_declaration_rule.cc"],
    hdrs = ["legacy_genvar_declaration_rule.h"],
    deps = [
        "//common/analysis:lint-rule-status",
        "//common/analysis:syntax-tree-lint-rule",
        "//common/text:concrete-syntax-tree",
        "//common/text:symbol",
        "//common/text:syntax-tree-context",
        "//common/text:tree-utils",
        "//verilog/CST:identifier",
        "//verilog/CST:verilog-nonterminals",
        "//verilog/analysis:descriptions",
        "//verilog/analysis:lint-rule-registry",
        "@com_google_absl//absl/strings:string_view",
    ],
    alwayslink = 1,
)

cc_test(
    name = "legacy-genvar-declaration-rule_test",
    srcs = ["legacy_genvar_declaration_rule_test.cc"],
    deps = [
        ":legacy-genvar-declaration-rule",
        "//common/analysis:linter-test-utils",
        "//common/analysis:syntax-tree-linter-test-utils",
        "//verilog/analysis:verilog-analyzer",
        "//verilog/parser:verilog-token-enum",
        "@com_google_googletest//:gtest",
        "@com_google_googletest//:gtest_main",
    ],
)

cc_library(
    name = "legacy-generate-region-rule",
    srcs = ["legacy_generate_region_rule.cc"],
    hdrs = ["legacy_generate_region_rule.h"],
    deps = [
        "//common/analysis:lint-rule-status",
        "//common/analysis:syntax-tree-lint-rule",
        "//common/analysis/matcher:matcher-builders",
        "//common/text:concrete-syntax-tree",
        "//common/text:symbol",
        "//common/text:syntax-tree-context",
        "//common/text:tree-utils",
        "//common/util:logging",
        "//verilog/CST:verilog-nonterminals",
        "//verilog/analysis:descriptions",
        "//verilog/analysis:lint-rule-registry",
        "//verilog/parser:verilog-token-enum",
        "@com_google_absl//absl/strings:string_view",
    ],
    alwayslink = 1,
)

cc_test(
    name = "legacy-generate-region-rule_test",
    srcs = ["legacy_generate_region_rule_test.cc"],
    deps = [
        ":legacy-generate-region-rule",
        "//common/analysis:linter-test-utils",
        "//common/analysis:syntax-tree-linter-test-utils",
        "//verilog/analysis:verilog-analyzer",
        "//verilog/parser:verilog-token-enum",
        "@com_google_googletest//:gtest",
        "@com_google_googletest//:gtest_main",
    ],
)

cc_library(
    name = "uvm-macro-semicolon-rule",
    srcs = ["uvm_macro_semicolon_rule.cc"],
    hdrs = ["uvm_macro_semicolon_rule.h"],
    deps = [
        "//common/analysis:lint-rule-status",
        "//common/analysis:syntax-tree-lint-rule",
        "//common/text:concrete-syntax-leaf",
        "//common/text:symbol",
        "//common/text:syntax-tree-context",
        "//common/text:token-info",
        "//verilog/CST:context-functions",
        "//verilog/CST:macro",
        "//verilog/CST:verilog-nonterminals",
        "//verilog/analysis:descriptions",
        "//verilog/analysis:lint-rule-registry",
        "//verilog/parser:verilog-token-enum",
        "@com_google_absl//absl/strings",
        "@com_google_absl//absl/strings:string_view",
    ],
    alwayslink = 1,
)

cc_test(
    name = "uvm-macro-semicolon-rule_test",
    srcs = ["uvm_macro_semicolon_rule_test.cc"],
    deps = [
        ":uvm-macro-semicolon-rule",
        "//common/analysis:linter-test-utils",
        "//common/analysis:syntax-tree-linter-test-utils",
        "//verilog/analysis:verilog-analyzer",
        "@com_google_googletest//:gtest",
        "@com_google_googletest//:gtest_main",
    ],
)

cc_library(
    name = "suspicious-semicolon-rule",
    srcs = ["suspicious_semicolon_rule.cc"],
    hdrs = ["suspicious_semicolon_rule.h"],
    deps = [
        "//common/analysis:lint-rule-status",
        "//common/analysis:syntax-tree-lint-rule",
        "//common/analysis/matcher",
        "//common/analysis/matcher:bound-symbol-manager",
        "//common/text:concrete-syntax-tree",
        "//common/text:syntax-tree-context",
        "//common/text:tree-utils",
        "//verilog/CST:verilog-matchers",
        "//verilog/CST:verilog-nonterminals",
        "//verilog/analysis:descriptions",
        "//verilog/analysis:lint-rule-registry",
        "@com_google_absl//absl/strings:string_view",
    ],
    alwayslink = 1,
)

cc_test(
    name = "suspicious-semicolon-rule_test",
    srcs = ["suspicious_semicolon_rule_test.cc"],
    deps = [
        ":suspicious-semicolon-rule",
        "//common/analysis:linter-test-utils",
        "//common/analysis:syntax-tree-linter-test-utils",
        "//verilog/analysis:verilog-analyzer",
        "@com_google_googletest//:gtest",
        "@com_google_googletest//:gtest_main",
    ],
)
