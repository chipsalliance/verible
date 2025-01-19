# This package contains functions and classes related to analysis of
# structural representations of Verilog/SystemVerilog source code.

default_visibility = [
    "//verible/verilog:__subpackages__",
]

package(
    default_applicable_licenses = ["//:license"],
    default_visibility = default_visibility,
    features = ["layering_check"],
)

cc_library(
    name = "default-rules",
    hdrs = ["default-rules.h"],
    deps = [
        "//verible/verilog/analysis/checkers:verilog-lint-rules",
    ],
)

cc_test(
    name = "default-rules_test",
    srcs = ["default-rules_test.cc"],
    deps = [
        ":default-rules",
        ":lint-rule-registry",
        "//verible/verilog/analysis/checkers:verilog-lint-rules",
        "@googletest//:gtest",
        "@googletest//:gtest_main",
    ],
)

cc_library(
    name = "descriptions",
    hdrs = ["descriptions.h"],
    alwayslink = 1,
)

cc_library(
    name = "extractors",
    srcs = ["extractors.cc"],
    hdrs = ["extractors.h"],
    deps = [
        ":verilog-analyzer",
        "//verible/common/text:tree-utils",
        "//verible/common/util:logging",
        "//verible/verilog/CST:identifier",
        "//verible/verilog/CST:module",
        "//verible/verilog/preprocessor:verilog-preprocess",
        "@abseil-cpp//absl/status",
    ],
)

cc_test(
    name = "extractors_test",
    srcs = ["extractors_test.cc"],
    deps = [
        ":extractors",
        "//verible/verilog/preprocessor:verilog-preprocess",
        "@googletest//:gtest",
        "@googletest//:gtest_main",
    ],
)

cc_library(
    name = "json-diagnostics",
    srcs = ["json-diagnostics.cc"],
    hdrs = ["json-diagnostics.h"],
    deps = [
        ":verilog-analyzer",
        "//verible/common/analysis:file-analyzer",
        "//verible/common/strings:line-column-map",
        "@nlohmann_json//:singleheader-json",
    ],
)

cc_library(
    name = "flow-tree",
    srcs = ["flow-tree.cc"],
    hdrs = ["flow-tree.h"],
    deps = [
        "//verible/common/text:token-stream-view",
        "//verible/common/util:logging",
        "//verible/verilog/parser:verilog-token-enum",
        "@abseil-cpp//absl/status",
    ],
)

cc_test(
    name = "flow-tree_test",
    srcs = ["flow-tree_test.cc"],
    deps = [
        ":flow-tree",
        "//verible/common/text:token-stream-view",
        "//verible/verilog/parser:verilog-lexer",
        "@abseil-cpp//absl/status",
        "@googletest//:gtest",
        "@googletest//:gtest_main",
    ],
)

cc_library(
    name = "lint-rule-registry",
    srcs = ["lint-rule-registry.cc"],
    hdrs = ["lint-rule-registry.h"],
    deps = [
        ":descriptions",
        "//verible/common/analysis:line-lint-rule",
        "//verible/common/analysis:syntax-tree-lint-rule",
        "//verible/common/analysis:text-structure-lint-rule",
        "//verible/common/analysis:token-stream-lint-rule",
        "//verible/common/strings:compare",
        "//verible/common/util:container-util",
        "//verible/common/util:logging",
        "@abseil-cpp//absl/container:node_hash_map",
    ],
)

cc_test(
    name = "json-diagnostics_test",
    srcs = ["json-diagnostics_test.cc"],
    deps = [
        ":json-diagnostics",
        ":verilog-analyzer",
        "//verible/common/util:logging",
        "@googletest//:gtest",
        "@googletest//:gtest_main",
        "@nlohmann_json//:singleheader-json",
    ],
)

cc_test(
    name = "lint-rule-registry_test",
    srcs = ["lint-rule-registry_test.cc"],
    deps = [
        ":descriptions",
        ":lint-rule-registry",
        "//verible/common/analysis:line-lint-rule",
        "//verible/common/analysis:lint-rule-status",
        "//verible/common/analysis:syntax-tree-lint-rule",
        "//verible/common/analysis:text-structure-lint-rule",
        "//verible/common/analysis:token-stream-lint-rule",
        "//verible/common/text:concrete-syntax-leaf",
        "//verible/common/text:concrete-syntax-tree",
        "//verible/common/text:syntax-tree-context",
        "//verible/common/text:text-structure",
        "//verible/common/text:token-info",
        "@abseil-cpp//absl/status",
        "@abseil-cpp//absl/strings",
        "@googletest//:gtest",
        "@googletest//:gtest_main",
    ],
)

cc_library(
    name = "verilog-analyzer",
    srcs = [
        "verilog-analyzer.cc",
        "verilog-excerpt-parse.cc",
        # paired together because of mutual recursion
    ],
    hdrs = [
        "verilog-analyzer.h",
        "verilog-excerpt-parse.h",
    ],
    # Can only add @abseil-cpp//absl/log:vlog_is_on after > absl 20240116
    # https://github.com/chipsalliance/verible/issues/2336
    features = ["-layering_check"],
    deps = [
        "//verible/common/analysis:file-analyzer",
        "//verible/common/lexer:token-stream-adapter",
        "//verible/common/strings:comment-utils",
        "//verible/common/strings:mem-block",
        "//verible/common/text:concrete-syntax-leaf",
        "//verible/common/text:concrete-syntax-tree",
        "//verible/common/text:symbol",
        "//verible/common/text:text-structure",
        "//verible/common/text:token-info",
        "//verible/common/text:token-stream-view",
        "//verible/common/text:visitors",
        "//verible/common/util:container-util",
        "//verible/common/util:logging",
        "//verible/common/util:status-macros",
        "//verible/verilog/parser:verilog-lexer",
        "//verible/verilog/parser:verilog-lexical-context",
        "//verible/verilog/parser:verilog-parser",
        "//verible/verilog/parser:verilog-token-classifications",
        "//verible/verilog/parser:verilog-token-enum",
        "//verible/verilog/preprocessor:verilog-preprocess",
        "@abseil-cpp//absl/base:config",
        "@abseil-cpp//absl/log",
        "@abseil-cpp//absl/status",
        "@abseil-cpp//absl/strings",
    ],
)

cc_library(
    name = "verilog-equivalence",
    srcs = ["verilog-equivalence.cc"],
    hdrs = ["verilog-equivalence.h"],
    deps = [
        "//verible/common/lexer:token-stream-adapter",
        "//verible/common/text:token-info",
        "//verible/common/text:token-stream-view",
        "//verible/common/util:enum-flags",
        "//verible/common/util:logging",
        "//verible/verilog/parser:verilog-lexer",
        "//verible/verilog/parser:verilog-parser",
        "//verible/verilog/parser:verilog-token-classifications",
        "//verible/verilog/parser:verilog-token-enum",
    ],
)

cc_library(
    name = "verilog-linter-configuration",
    srcs = ["verilog-linter-configuration.cc"],
    hdrs = ["verilog-linter-configuration.h"],
    deps = [
        ":default-rules",
        ":descriptions",
        ":lint-rule-registry",
        "//verible/common/analysis:line-lint-rule",
        "//verible/common/analysis:syntax-tree-lint-rule",
        "//verible/common/analysis:text-structure-lint-rule",
        "//verible/common/analysis:token-stream-lint-rule",
        "//verible/common/util:container-util",
        "//verible/common/util:enum-flags",
        "//verible/common/util:file-util",
        "//verible/common/util:logging",
        "//verible/common/util:status-macros",
        "@abseil-cpp//absl/status",
        "@abseil-cpp//absl/status:statusor",
        "@abseil-cpp//absl/strings",
    ],
)

cc_library(
    name = "verilog-linter-constants",
    hdrs = ["verilog-linter-constants.h"],
)

cc_library(
    name = "verilog-linter",
    srcs = ["verilog-linter.cc"],
    hdrs = ["verilog-linter.h"],
    # Can only add @abseil-cpp//absl/log:vlog_is_on after > absl 20240116
    # https://github.com/chipsalliance/verible/issues/2336
    features = ["-layering_check"],
    deps = [
        ":default-rules",
        ":lint-rule-registry",
        ":verilog-analyzer",
        ":verilog-linter-configuration",
        ":verilog-linter-constants",
        "//verible/common/analysis:citation",
        "//verible/common/analysis:line-linter",
        "//verible/common/analysis:lint-rule-status",
        "//verible/common/analysis:lint-waiver",
        "//verible/common/analysis:syntax-tree-linter",
        "//verible/common/analysis:text-structure-linter",
        "//verible/common/analysis:token-stream-linter",
        "//verible/common/analysis:violation-handler",
        "//verible/common/strings:line-column-map",
        "//verible/common/text:concrete-syntax-tree",
        "//verible/common/text:text-structure",
        "//verible/common/text:token-info",
        "//verible/common/util:file-util",
        "//verible/common/util:logging",
        "//verible/common/util:status-macros",
        "//verible/verilog/parser:verilog-token-classifications",
        "//verible/verilog/parser:verilog-token-enum",
        "@abseil-cpp//absl/base:config",
        "@abseil-cpp//absl/flags:flag",
        "@abseil-cpp//absl/log",
        "@abseil-cpp//absl/status",
        "@abseil-cpp//absl/status:statusor",
        "@abseil-cpp//absl/strings",
    ],
)

cc_test(
    name = "verilog-analyzer_test",
    srcs = ["verilog-analyzer_test.cc"],
    deps = [
        ":verilog-analyzer",
        "//verible/common/analysis:file-analyzer",
        "//verible/common/strings:display-utils",
        "//verible/common/text:concrete-syntax-leaf",
        "//verible/common/text:concrete-syntax-tree",
        "//verible/common/text:symbol",
        "//verible/common/text:text-structure",
        "//verible/common/text:token-info",
        "//verible/common/text:token-info-test-util",
        "//verible/common/text:tree-utils",
        "//verible/common/util:casts",
        "//verible/common/util:logging",
        "//verible/verilog/parser:verilog-token-enum",
        "@abseil-cpp//absl/status",
        "@abseil-cpp//absl/strings",
        "@googletest//:gtest",
        "@googletest//:gtest_main",
    ],
)

cc_test(
    name = "verilog-linter-configuration_test",
    srcs = ["verilog-linter-configuration_test.cc"],
    deps = [
        ":default-rules",
        ":descriptions",
        ":lint-rule-registry",
        ":verilog-linter",
        ":verilog-linter-configuration",
        "//verible/common/analysis:line-lint-rule",
        "//verible/common/analysis:lint-rule-status",
        "//verible/common/analysis:syntax-tree-lint-rule",
        "//verible/common/analysis:text-structure-lint-rule",
        "//verible/common/analysis:token-stream-lint-rule",
        "//verible/common/strings:line-column-map",
        "//verible/common/text:concrete-syntax-leaf",
        "//verible/common/text:concrete-syntax-tree",
        "//verible/common/text:syntax-tree-context",
        "//verible/common/text:text-structure",
        "//verible/common/text:token-info",
        "//verible/common/text:tree-builder-test-util",
        "@abseil-cpp//absl/strings",
        "@googletest//:gtest",
        "@googletest//:gtest_main",
    ],
)

cc_test(
    name = "verilog-linter_test",
    srcs = ["verilog-linter_test.cc"],
    deps = [
        ":default-rules",
        ":verilog-analyzer",
        ":verilog-linter",
        ":verilog-linter-configuration",
        "//verible/common/analysis:lint-rule-status",
        "//verible/common/analysis:violation-handler",
        "//verible/common/util:file-util",
        "//verible/common/util:logging",
        "@abseil-cpp//absl/status",
        "@abseil-cpp//absl/status:statusor",
        "@abseil-cpp//absl/strings",
        "@googletest//:gtest",
        "@googletest//:gtest_main",
    ],
)

cc_test(
    name = "verilog-equivalence_test",
    srcs = ["verilog-equivalence_test.cc"],
    deps = [
        ":verilog-equivalence",
        "@abseil-cpp//absl/strings",
        "@abseil-cpp//absl/types:span",
        "@googletest//:gtest",
        "@googletest//:gtest_main",
    ],
)

cc_library(
    name = "verilog-filelist",
    srcs = ["verilog-filelist.cc"],
    hdrs = ["verilog-filelist.h"],
    deps = [
        "//verible/common/util:file-util",
        "//verible/common/util:iterator-range",
        "@abseil-cpp//absl/status",
        "@abseil-cpp//absl/strings",
    ],
)

cc_test(
    name = "verilog-filelist_test",
    srcs = ["verilog-filelist_test.cc"],
    deps = [
        ":verilog-filelist",
        "//verible/common/util:file-util",
        "@googletest//:gtest",
        "@googletest//:gtest_main",
    ],
)

cc_library(
    name = "verilog-project",
    srcs = ["verilog-project.cc"],
    hdrs = ["verilog-project.h"],
    deps = [
        ":verilog-analyzer",
        "//verible/common/strings:mem-block",
        "//verible/common/strings:string-memory-map",
        "//verible/common/text:text-structure",
        "//verible/common/util:file-util",
        "//verible/common/util:logging",
        "@abseil-cpp//absl/status",
        "@abseil-cpp//absl/status:statusor",
        "@abseil-cpp//absl/strings",
        "@abseil-cpp//absl/time",
        "@abseil-cpp//absl/types:optional",
    ],
)

cc_test(
    name = "verilog-project_test",
    srcs = ["verilog-project_test.cc"],
    deps = [
        ":verilog-analyzer",
        ":verilog-project",
        "//verible/common/text:text-structure",
        "//verible/common/util:file-util",
        "//verible/common/util:logging",
        "//verible/common/util:range",
        "//verible/verilog/CST:module",
        "@abseil-cpp//absl/status",
        "@abseil-cpp//absl/strings",
        "@googletest//:gtest",
        "@googletest//:gtest_main",
    ],
)

cc_library(
    name = "symbol-table",
    srcs = ["symbol-table.cc"],
    hdrs = ["symbol-table.h"],
    deps = [
        ":verilog-project",
        "//verible/common/strings:compare",
        "//verible/common/strings:display-utils",
        "//verible/common/text:concrete-syntax-leaf",
        "//verible/common/text:concrete-syntax-tree",
        "//verible/common/text:symbol",
        "//verible/common/text:token-info",
        "//verible/common/text:tree-compare",
        "//verible/common/text:tree-context-visitor",
        "//verible/common/text:tree-utils",
        "//verible/common/text:visitors",
        "//verible/common/util:casts",
        "//verible/common/util:enum-flags",
        "//verible/common/util:logging",
        "//verible/common/util:map-tree",
        "//verible/common/util:spacer",
        "//verible/common/util:tree-operations",
        "//verible/common/util:value-saver",
        "//verible/common/util:vector-tree",
        "//verible/verilog/CST:class",
        "//verible/verilog/CST:declaration",
        "//verible/verilog/CST:functions",
        "//verible/verilog/CST:macro",
        "//verible/verilog/CST:module",
        "//verible/verilog/CST:net",
        "//verible/verilog/CST:package",
        "//verible/verilog/CST:parameters",
        "//verible/verilog/CST:port",
        "//verible/verilog/CST:seq-block",
        "//verible/verilog/CST:statement",
        "//verible/verilog/CST:tasks",
        "//verible/verilog/CST:type",
        "//verible/verilog/CST:verilog-nonterminals",
        "//verible/verilog/parser:verilog-parser",
        "//verible/verilog/parser:verilog-token-enum",
        "@abseil-cpp//absl/status",
        "@abseil-cpp//absl/status:statusor",
        "@abseil-cpp//absl/strings",
        "@abseil-cpp//absl/time",
    ],
)

cc_test(
    name = "symbol-table_test",
    srcs = ["symbol-table_test.cc"],
    deps = [
        ":symbol-table",
        ":verilog-filelist",
        ":verilog-project",
        "//verible/common/text:tree-utils",
        "//verible/common/util:file-util",
        "//verible/common/util:logging",
        "//verible/common/util:range",
        "//verible/common/util:tree-operations",
        "@abseil-cpp//absl/base:core_headers",
        "@abseil-cpp//absl/status",
        "@googletest//:gtest",
        "@googletest//:gtest_main",
    ],
)

cc_library(
    name = "dependencies",
    srcs = ["dependencies.cc"],
    hdrs = ["dependencies.h"],
    deps = [
        ":symbol-table",
        ":verilog-project",
        "//verible/common/strings:compare",
        "//verible/common/strings:display-utils",
        "//verible/common/util:logging",
    ],
)

cc_test(
    name = "dependencies_test",
    srcs = ["dependencies_test.cc"],
    deps = [
        ":dependencies",
        ":symbol-table",
        ":verilog-project",
        "//verible/common/util:file-util",
        "//verible/common/util:logging",
        "@abseil-cpp//absl/status",
        "@googletest//:gtest",
        "@googletest//:gtest_main",
    ],
)
