# This package contains functions and classes related to analysis of
# structural representations of Verilog/SystemVerilog source code.

default_visibility = [
    "//verilog:__subpackages__",
]

package(
    default_applicable_licenses = ["//:license"],
    default_visibility = default_visibility,
)

cc_library(
    name = "default-rules",
    hdrs = ["default-rules.h"],
    deps = [
        "//verilog/analysis/checkers:verilog-lint-rules",
    ],
)

cc_test(
    name = "default-rules_test",
    srcs = ["default-rules_test.cc"],
    deps = [
        ":default-rules",
        ":lint-rule-registry",
        "//verilog/analysis/checkers:verilog-lint-rules",
        "@com_google_googletest//:gtest_main",
    ],
)

cc_library(
    name = "descriptions",
    hdrs = ["descriptions.h"],
    deps = ["@com_google_absl//absl/strings"],
    alwayslink = 1,
)

cc_library(
    name = "extractors",
    srcs = ["extractors.cc"],
    hdrs = ["extractors.h"],
    deps = [
        ":verilog-analyzer",
        "//verilog/CST:identifier",
        "//verilog/CST:module",
        "//verilog/preprocessor:verilog-preprocess",
        "@com_google_absl//absl/status",
        "@com_google_absl//absl/strings",
    ],
)

cc_test(
    name = "extractors_test",
    srcs = ["extractors_test.cc"],
    deps = [
        ":extractors",
        "@com_google_absl//absl/strings",
        "@com_google_googletest//:gtest_main",
    ],
)

cc_library(
    name = "json-diagnostics",
    srcs = ["json_diagnostics.cc"],
    hdrs = ["json_diagnostics.h"],
    deps = [
        ":verilog-analyzer",
        "//common/analysis:file-analyzer",
        "//common/strings:line-column-map",
        "@com_google_absl//absl/strings",
        "@jsonhpp",
    ],
)

cc_library(
    name = "flow-tree",
    srcs = ["flow_tree.cc"],
    hdrs = ["flow_tree.h"],
    deps = [
        "//common/text:token-stream-view",
        "//common/util:logging",
        "//common/util:status-macros",
        "//verilog/parser:verilog-token-enum",
        "@com_google_absl//absl/status",
        "@com_google_absl//absl/strings",
    ],
)

cc_test(
    name = "flow-tree_test",
    srcs = ["flow_tree_test.cc"],
    deps = [
        ":flow-tree",
        "//verilog/parser:verilog-lexer",
        "//verilog/parser:verilog-token-enum",
        "@com_google_absl//absl/status",
        "@com_google_absl//absl/strings",
        "@com_google_googletest//:gtest_main",
    ],
)

cc_library(
    name = "lint-rule-registry",
    srcs = ["lint-rule-registry.cc"],
    hdrs = ["lint-rule-registry.h"],
    deps = [
        ":descriptions",
        "//common/analysis:line-lint-rule",
        "//common/analysis:syntax-tree-lint-rule",
        "//common/analysis:text-structure-lint-rule",
        "//common/analysis:token-stream-lint-rule",
        "//common/strings:compare",
        "//common/util:container-util",
        "//common/util:logging",
        "@com_google_absl//absl/container:node_hash_map",
        "@com_google_absl//absl/strings",
    ],
)

cc_test(
    name = "json-diagnostics_test",
    srcs = ["json_diagnostics_test.cc"],
    deps = [
        ":json-diagnostics",
        ":verilog-analyzer",
        "//common/util:logging",
        "@com_google_absl//absl/strings",
        "@com_google_googletest//:gtest_main",
        "@jsonhpp",
    ],
)

cc_test(
    name = "lint-rule-registry_test",
    srcs = ["lint-rule-registry_test.cc"],
    deps = [
        ":descriptions",
        ":lint-rule-registry",
        "//common/analysis:line-lint-rule",
        "//common/analysis:lint-rule-status",
        "//common/analysis:syntax-tree-lint-rule",
        "//common/analysis:text-structure-lint-rule",
        "//common/analysis:token-stream-lint-rule",
        "//common/text:concrete-syntax-leaf",
        "//common/text:concrete-syntax-tree",
        "//common/text:syntax-tree-context",
        "//common/text:text-structure",
        "//common/text:token-info",
        "@com_google_absl//absl/strings",
        "@com_google_googletest//:gtest_main",
    ],
)

cc_library(
    name = "verilog-analyzer",
    srcs = [
        "verilog_analyzer.cc",
        "verilog_excerpt_parse.cc",
        # paired together because of mutual recursion
    ],
    hdrs = [
        "verilog_analyzer.h",
        "verilog_excerpt_parse.h",
    ],
    deps = [
        "//common/analysis:file-analyzer",
        "//common/lexer:token-stream-adapter",
        "//common/strings:comment-utils",
        "//common/strings:mem-block",
        "//common/text:concrete-syntax-leaf",
        "//common/text:concrete-syntax-tree",
        "//common/text:symbol",
        "//common/text:text-structure",
        "//common/text:token-info",
        "//common/text:token-stream-view",
        "//common/text:visitors",
        "//common/util:container-util",
        "//common/util:logging",
        "//common/util:status-macros",
        "//verilog/parser:verilog-lexer",
        "//verilog/parser:verilog-lexical-context",
        "//verilog/parser:verilog-parser",
        "//verilog/parser:verilog-token-classifications",
        "//verilog/parser:verilog-token-enum",
        "//verilog/preprocessor:verilog-preprocess",
        "@com_google_absl//absl/status",
        "@com_google_absl//absl/strings",
    ],
)

cc_library(
    name = "verilog-equivalence",
    srcs = ["verilog_equivalence.cc"],
    hdrs = ["verilog_equivalence.h"],
    deps = [
        "//common/lexer:token-stream-adapter",
        "//common/text:token-info",
        "//common/text:token-stream-view",
        "//common/util:enum-flags",
        "//common/util:logging",
        "//verilog/parser:verilog-lexer",
        "//verilog/parser:verilog-parser",
        "//verilog/parser:verilog-token-classifications",
        "//verilog/parser:verilog-token-enum",
        "@com_google_absl//absl/strings",
    ],
)

cc_library(
    name = "verilog-linter-configuration",
    srcs = ["verilog_linter_configuration.cc"],
    hdrs = ["verilog_linter_configuration.h"],
    deps = [
        ":default-rules",
        ":lint-rule-registry",
        "//common/analysis:line-lint-rule",
        "//common/analysis:syntax-tree-lint-rule",
        "//common/analysis:text-structure-lint-rule",
        "//common/analysis:token-stream-lint-rule",
        "//common/util:container-util",
        "//common/util:enum-flags",
        "//common/util:file-util",
        "//common/util:logging",
        "//common/util:status-macros",
        "@com_google_absl//absl/status",
        "@com_google_absl//absl/status:statusor",
        "@com_google_absl//absl/strings",
    ],
)

cc_library(
    name = "verilog-linter-constants",
    hdrs = ["verilog_linter_constants.h"],
    deps = [
        "@com_google_absl//absl/strings",
    ],
)

cc_library(
    name = "verilog-linter",
    srcs = ["verilog_linter.cc"],
    hdrs = ["verilog_linter.h"],
    deps = [
        ":default-rules",
        ":lint-rule-registry",
        ":verilog-analyzer",
        ":verilog-linter-configuration",
        ":verilog-linter-constants",
        "//common/analysis:line-lint-rule",
        "//common/analysis:line-linter",
        "//common/analysis:lint-rule-status",
        "//common/analysis:lint-waiver",
        "//common/analysis:syntax-tree-lint-rule",
        "//common/analysis:syntax-tree-linter",
        "//common/analysis:text-structure-lint-rule",
        "//common/analysis:text-structure-linter",
        "//common/analysis:token-stream-lint-rule",
        "//common/analysis:token-stream-linter",
        "//common/analysis:violation-handler",
        "//common/strings:line-column-map",
        "//common/text:concrete-syntax-tree",
        "//common/text:text-structure",
        "//common/text:token-info",
        "//common/util:file-util",
        "//common/util:logging",
        "//common/util:status-macros",
        "//verilog/parser:verilog-token-classifications",
        "//verilog/parser:verilog-token-enum",
        "@com_google_absl//absl/flags:flag",
        "@com_google_absl//absl/status",
        "@com_google_absl//absl/status:statusor",
        "@com_google_absl//absl/strings",
    ],
)

cc_test(
    name = "verilog-analyzer_test",
    srcs = ["verilog_analyzer_test.cc"],
    deps = [
        ":verilog-analyzer",
        "//common/analysis:file-analyzer",
        "//common/strings:display-utils",
        "//common/text:concrete-syntax-leaf",
        "//common/text:concrete-syntax-tree",
        "//common/text:constants",
        "//common/text:symbol",
        "//common/text:text-structure",
        "//common/text:token-info",
        "//common/text:token-info-test-util",
        "//common/text:token-stream-view",
        "//common/text:tree-utils",
        "//common/util:casts",
        "//common/util:logging",
        "//verilog/parser:verilog-parser",
        "//verilog/parser:verilog-token-enum",
        "@com_google_absl//absl/base",
        "@com_google_absl//absl/status",
        "@com_google_absl//absl/strings",
        "@com_google_absl//absl/types:span",
        "@com_google_googletest//:gtest_main",
    ],
)

cc_test(
    name = "verilog-linter-configuration_test",
    srcs = ["verilog_linter_configuration_test.cc"],
    deps = [
        ":default-rules",
        ":descriptions",
        ":lint-rule-registry",
        ":verilog-linter",
        ":verilog-linter-configuration",
        "//common/analysis:line-lint-rule",
        "//common/analysis:lint-rule-status",
        "//common/analysis:syntax-tree-lint-rule",
        "//common/analysis:text-structure-lint-rule",
        "//common/analysis:token-stream-lint-rule",
        "//common/strings:line-column-map",
        "//common/text:concrete-syntax-leaf",
        "//common/text:concrete-syntax-tree",
        "//common/text:syntax-tree-context",
        "//common/text:text-structure",
        "//common/text:token-info",
        "//common/text:tree-builder-test-util",
        "@com_google_absl//absl/strings",
        "@com_google_googletest//:gtest_main",
    ],
)

cc_test(
    name = "verilog-linter_test",
    srcs = ["verilog_linter_test.cc"],
    deps = [
        ":default-rules",
        ":verilog-analyzer",
        ":verilog-linter",
        ":verilog-linter-configuration",
        "//common/analysis:violation-handler",
        "//common/util:file-util",
        "//common/util:logging",
        "@com_google_absl//absl/status",
        "@com_google_absl//absl/status:statusor",
        "@com_google_absl//absl/strings",
        "@com_google_googletest//:gtest_main",
    ],
)

cc_test(
    name = "verilog-equivalence_test",
    srcs = ["verilog_equivalence_test.cc"],
    deps = [
        ":verilog-equivalence",
        "//common/text:token-info",
        "//common/util:logging",
        "@com_google_absl//absl/status",
        "@com_google_absl//absl/strings",
        "@com_google_absl//absl/types:span",
        "@com_google_googletest//:gtest_main",
    ],
)

cc_library(
    name = "verilog-filelist",
    srcs = ["verilog_filelist.cc"],
    hdrs = ["verilog_filelist.h"],
    deps = [
        "//common/util:file-util",
        "//common/util:iterator-range",
        "@com_google_absl//absl/status",
        "@com_google_absl//absl/strings",
    ],
)

cc_test(
    name = "verilog-filelist_test",
    srcs = ["verilog_filelist_test.cc"],
    deps = [
        ":verilog-filelist",
        "//common/util:file-util",
        "@com_google_googletest//:gtest_main",
    ],
)

cc_library(
    name = "verilog-project",
    srcs = ["verilog_project.cc"],
    hdrs = ["verilog_project.h"],
    deps = [
        ":verilog-analyzer",
        "//common/strings:mem-block",
        "//common/strings:string-memory-map",
        "//common/text:text-structure",
        "//common/util:file-util",
        "//common/util:logging",
        "@com_google_absl//absl/status",
        "@com_google_absl//absl/status:statusor",
        "@com_google_absl//absl/strings",
        "@com_google_absl//absl/time",
        "@com_google_absl//absl/types:optional",
    ],
)

cc_test(
    name = "verilog-project_test",
    srcs = ["verilog_project_test.cc"],
    deps = [
        ":verilog-project",
        "//common/text:text-structure",
        "//common/util:file-util",
        "//common/util:logging",
        "//common/util:range",
        "//verilog/CST:module",
        "@com_google_absl//absl/strings",
        "@com_google_googletest//:gtest_main",
    ],
)

cc_library(
    name = "symbol-table",
    srcs = ["symbol_table.cc"],
    hdrs = ["symbol_table.h"],
    deps = [
        ":verilog-project",
        "//common/strings:compare",
        "//common/strings:display-utils",
        "//common/text:concrete-syntax-leaf",
        "//common/text:concrete-syntax-tree",
        "//common/text:symbol",
        "//common/text:token-info",
        "//common/text:tree-context-visitor",
        "//common/text:tree-utils",
        "//common/text:visitors",
        "//common/util:enum-flags",
        "//common/util:logging",
        "//common/util:map-tree",
        "//common/util:spacer",
        "//common/util:tree-operations",
        "//common/util:value-saver",
        "//common/util:vector-tree",
        "//verilog/CST:class",
        "//verilog/CST:declaration",
        "//verilog/CST:functions",
        "//verilog/CST:macro",
        "//verilog/CST:module",
        "//verilog/CST:net",
        "//verilog/CST:package",
        "//verilog/CST:parameters",
        "//verilog/CST:port",
        "//verilog/CST:seq-block",
        "//verilog/CST:statement",
        "//verilog/CST:tasks",
        "//verilog/CST:type",
        "//verilog/CST:verilog-nonterminals",
        "//verilog/parser:verilog-parser",
        "//verilog/parser:verilog-token-enum",
        "@com_google_absl//absl/status",
        "@com_google_absl//absl/strings",
        "@com_google_absl//absl/strings:str_format",
        "@com_google_absl//absl/time",
    ],
)

cc_test(
    name = "symbol-table_test",
    srcs = ["symbol_table_test.cc"],
    deps = [
        ":symbol-table",
        ":verilog-project",
        "//common/text:symbol",
        "//common/text:tree-utils",
        "//common/util:file-util",
        "//common/util:logging",
        "//common/util:range",
        "//common/util:tree-operations",
        "@com_google_absl//absl/base:core_headers",
        "@com_google_absl//absl/status",
        "@com_google_absl//absl/strings",
        "@com_google_googletest//:gtest_main",
    ],
)

cc_library(
    name = "dependencies",
    srcs = ["dependencies.cc"],
    hdrs = ["dependencies.h"],
    deps = [
        ":symbol-table",
        ":verilog-project",
        "//common/strings:compare",
        "//common/strings:display-utils",
        "//common/util:logging",
        "@com_google_absl//absl/strings",
    ],
)

cc_test(
    name = "dependencies_test",
    srcs = ["dependencies_test.cc"],
    deps = [
        ":dependencies",
        "//common/util:file-util",
        "@com_google_absl//absl/status",
        "@com_google_googletest//:gtest_main",
    ],
)
