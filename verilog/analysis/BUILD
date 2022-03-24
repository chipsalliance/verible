# This package contains functions and classes related to analysis of
# structural representations of Verilog/SystemVerilog source code.

licenses(["notice"])

default_visibility = [
    "//verilog:__subpackages__",
]

package(
    default_visibility = default_visibility,
)

cc_library(
    name = "default_rules",
    hdrs = ["default_rules.h"],
    deps = [
        "//verilog/analysis/checkers:verilog_lint_rules",
    ],
)

cc_test(
    name = "default_rules_test",
    srcs = ["default_rules_test.cc"],
    deps = [
        ":default_rules",
        ":lint_rule_registry",
        "//verilog/analysis/checkers:verilog_lint_rules",
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
        "//verilog/CST:identifier",
        "//verilog/CST:module",
        "//verilog/analysis:verilog_analyzer",
        "//verilog/preprocessor:verilog_preprocess",
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
    name = "json_diagnostics",
    srcs = ["json_diagnostics.cc"],
    hdrs = ["json_diagnostics.h"],
    deps = [
        ":verilog_analyzer",
        "//common/analysis:file_analyzer",
        "//common/strings:line_column_map",
        "@com_google_absl//absl/strings",
        "@jsonhpp",
    ],
)

cc_library(
    name = "lint_rule_registry",
    srcs = ["lint_rule_registry.cc"],
    hdrs = ["lint_rule_registry.h"],
    deps = [
        ":descriptions",
        "//common/analysis:line_lint_rule",
        "//common/analysis:syntax_tree_lint_rule",
        "//common/analysis:text_structure_lint_rule",
        "//common/analysis:token_stream_lint_rule",
        "//common/strings:compare",
        "//common/util:container_util",
        "//common/util:logging",
        "@com_google_absl//absl/container:node_hash_map",
        "@com_google_absl//absl/strings",
    ],
)

cc_test(
    name = "json_diagnostics_test",
    srcs = ["json_diagnostics_test.cc"],
    deps = [
        ":json_diagnostics",
        ":verilog_analyzer",
        "//common/util:logging",
        "@com_google_absl//absl/memory",
        "@com_google_absl//absl/strings",
        "@com_google_googletest//:gtest_main",
        "@jsonhpp",
    ],
)

cc_test(
    name = "lint_rule_registry_test",
    srcs = ["lint_rule_registry_test.cc"],
    deps = [
        ":descriptions",
        ":lint_rule_registry",
        "//common/analysis:line_lint_rule",
        "//common/analysis:lint_rule_status",
        "//common/analysis:syntax_tree_lint_rule",
        "//common/analysis:text_structure_lint_rule",
        "//common/analysis:token_stream_lint_rule",
        "//common/text:concrete_syntax_leaf",
        "//common/text:concrete_syntax_tree",
        "//common/text:syntax_tree_context",
        "//common/text:text_structure",
        "//common/text:token_info",
        "@com_google_absl//absl/strings",
        "@com_google_googletest//:gtest_main",
    ],
)

cc_library(
    name = "verilog_analyzer",
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
        "//common/analysis:file_analyzer",
        "//common/lexer:token_stream_adapter",
        "//common/strings:comment_utils",
        "//common/text:concrete_syntax_leaf",
        "//common/text:concrete_syntax_tree",
        "//common/text:symbol",
        "//common/text:text_structure",
        "//common/text:token_info",
        "//common/text:token_stream_view",
        "//common/text:visitors",
        "//common/util:container_util",
        "//common/util:logging",
        "//common/util:status_macros",
        "//verilog/parser:verilog_lexer",
        "//verilog/parser:verilog_lexical_context",
        "//verilog/parser:verilog_parser",
        "//verilog/parser:verilog_token_classifications",
        "//verilog/parser:verilog_token_enum",
        "//verilog/preprocessor:verilog_preprocess",
        "@com_google_absl//absl/memory",
        "@com_google_absl//absl/status",
        "@com_google_absl//absl/strings",
    ],
)

cc_library(
    name = "verilog_equivalence",
    srcs = ["verilog_equivalence.cc"],
    hdrs = ["verilog_equivalence.h"],
    deps = [
        "//common/lexer:token_stream_adapter",
        "//common/text:token_info",
        "//common/text:token_stream_view",
        "//common/util:enum_flags",
        "//common/util:logging",
        "//verilog/parser:verilog_lexer",
        "//verilog/parser:verilog_parser",
        "//verilog/parser:verilog_token_classifications",
        "//verilog/parser:verilog_token_enum",
        "@com_google_absl//absl/strings",
    ],
)

cc_library(
    name = "verilog_linter_configuration",
    srcs = ["verilog_linter_configuration.cc"],
    hdrs = ["verilog_linter_configuration.h"],
    deps = [
        ":default_rules",
        ":lint_rule_registry",
        "//common/analysis:line_lint_rule",
        "//common/analysis:syntax_tree_lint_rule",
        "//common/analysis:text_structure_lint_rule",
        "//common/analysis:token_stream_lint_rule",
        "//common/util:container_util",
        "//common/util:enum_flags",
        "//common/util:file_util",
        "//common/util:logging",
        "@com_google_absl//absl/status",
        "@com_google_absl//absl/status:statusor",
        "@com_google_absl//absl/strings",
    ],
)

cc_library(
    name = "verilog_linter_constants",
    hdrs = ["verilog_linter_constants.h"],
)

cc_library(
    name = "verilog_linter",
    srcs = ["verilog_linter.cc"],
    hdrs = ["verilog_linter.h"],
    deps = [
        ":default_rules",
        ":lint_rule_registry",
        ":verilog_analyzer",
        ":verilog_linter_configuration",
        ":verilog_linter_constants",
        "//common/analysis:line_lint_rule",
        "//common/analysis:line_linter",
        "//common/analysis:lint_rule_status",
        "//common/analysis:lint_waiver",
        "//common/analysis:syntax_tree_lint_rule",
        "//common/analysis:syntax_tree_linter",
        "//common/analysis:text_structure_lint_rule",
        "//common/analysis:text_structure_linter",
        "//common/analysis:token_stream_lint_rule",
        "//common/analysis:token_stream_linter",
        "//common/analysis:violation_handler",
        "//common/strings:line_column_map",
        "//common/text:concrete_syntax_tree",
        "//common/text:text_structure",
        "//common/text:token_info",
        "//common/util:file_util",
        "//common/util:logging",
        "//verilog/parser:verilog_token_classifications",
        "//verilog/parser:verilog_token_enum",
        "@com_google_absl//absl/flags:flag",
        "@com_google_absl//absl/status",
        "@com_google_absl//absl/status:statusor",
        "@com_google_absl//absl/strings",
    ],
)

cc_test(
    name = "verilog_analyzer_test",
    srcs = ["verilog_analyzer_test.cc"],
    deps = [
        ":verilog_analyzer",
        "//common/analysis:file_analyzer",
        "//common/strings:display_utils",
        "//common/text:concrete_syntax_leaf",
        "//common/text:concrete_syntax_tree",
        "//common/text:constants",
        "//common/text:symbol",
        "//common/text:text_structure",
        "//common/text:token_info",
        "//common/text:token_info_test_util",
        "//common/text:token_stream_view",
        "//common/text:tree_utils",
        "//common/util:casts",
        "//common/util:logging",
        "//verilog/parser:verilog_parser",
        "//verilog/parser:verilog_token_enum",
        "@com_google_absl//absl/base",
        "@com_google_absl//absl/memory",
        "@com_google_absl//absl/status",
        "@com_google_absl//absl/strings",
        "@com_google_absl//absl/types:span",
        "@com_google_googletest//:gtest_main",
    ],
)

cc_test(
    name = "verilog_linter_configuration_test",
    srcs = ["verilog_linter_configuration_test.cc"],
    deps = [
        ":default_rules",
        ":descriptions",
        ":lint_rule_registry",
        ":verilog_linter",
        ":verilog_linter_configuration",
        "//common/analysis:line_lint_rule",
        "//common/analysis:lint_rule_status",
        "//common/analysis:syntax_tree_lint_rule",
        "//common/analysis:text_structure_lint_rule",
        "//common/analysis:token_stream_lint_rule",
        "//common/strings:line_column_map",
        "//common/text:concrete_syntax_leaf",
        "//common/text:concrete_syntax_tree",
        "//common/text:syntax_tree_context",
        "//common/text:text_structure",
        "//common/text:token_info",
        "//common/text:tree_builder_test_util",
        "@com_google_absl//absl/strings",
        "@com_google_googletest//:gtest_main",
    ],
)

cc_test(
    name = "verilog_linter_test",
    srcs = ["verilog_linter_test.cc"],
    deps = [
        ":default_rules",
        ":verilog_analyzer",
        ":verilog_linter",
        ":verilog_linter_configuration",
        "//common/analysis:violation_handler",
        "//common/util:file_util",
        "//common/util:logging",
        "@com_google_absl//absl/memory",
        "@com_google_absl//absl/status",
        "@com_google_absl//absl/status:statusor",
        "@com_google_absl//absl/strings",
        "@com_google_googletest//:gtest_main",
    ],
)

cc_test(
    name = "verilog_equivalence_test",
    srcs = ["verilog_equivalence_test.cc"],
    deps = [
        ":verilog_equivalence",
        "//common/text:token_info",
        "//common/util:logging",
        "@com_google_absl//absl/status",
        "@com_google_absl//absl/strings",
        "@com_google_absl//absl/types:span",
        "@com_google_googletest//:gtest_main",
    ],
)

cc_library(
    name = "verilog_project",
    srcs = ["verilog_project.cc"],
    hdrs = ["verilog_project.h"],
    deps = [
        ":verilog_analyzer",
        "//common/strings:string_memory_map",
        "//common/text:text_structure",
        "//common/util:file_util",
        "//common/util:logging",
        "@com_google_absl//absl/memory",
        "@com_google_absl//absl/status",
        "@com_google_absl//absl/status:statusor",
        "@com_google_absl//absl/strings",
        "@com_google_absl//absl/types:optional",
    ],
)

cc_test(
    name = "verilog_project_test",
    srcs = ["verilog_project_test.cc"],
    deps = [
        ":verilog_project",
        "//common/text:text_structure",
        "//common/util:file_util",
        "//common/util:logging",
        "//common/util:range",
        "//verilog/CST:module",
        "@com_google_absl//absl/memory",
        "@com_google_absl//absl/strings",
        "@com_google_googletest//:gtest_main",
    ],
)

cc_library(
    name = "symbol_table",
    srcs = ["symbol_table.cc"],
    hdrs = ["symbol_table.h"],
    deps = [
        ":verilog_project",
        "//common/strings:compare",
        "//common/strings:display_utils",
        "//common/text:concrete_syntax_leaf",
        "//common/text:concrete_syntax_tree",
        "//common/text:symbol",
        "//common/text:token_info",
        "//common/text:tree_context_visitor",
        "//common/text:tree_utils",
        "//common/text:visitors",
        "//common/util:enum_flags",
        "//common/util:logging",
        "//common/util:map_tree",
        "//common/util:spacer",
        "//common/util:tree_operations",
        "//common/util:value_saver",
        "//common/util:vector_tree",
        "//verilog/CST:class",
        "//verilog/CST:declaration",
        "//verilog/CST:functions",
        "//verilog/CST:macro",
        "//verilog/CST:module",
        "//verilog/CST:net",
        "//verilog/CST:package",
        "//verilog/CST:parameters",
        "//verilog/CST:port",
        "//verilog/CST:seq_block",
        "//verilog/CST:statement",
        "//verilog/CST:tasks",
        "//verilog/CST:type",
        "//verilog/CST:verilog_nonterminals",
        "//verilog/parser:verilog_parser",
        "//verilog/parser:verilog_token_enum",
        "@com_google_absl//absl/status",
        "@com_google_absl//absl/strings",
        "@com_google_absl//absl/strings:str_format",
    ],
)

cc_test(
    name = "symbol_table_test",
    srcs = ["symbol_table_test.cc"],
    deps = [
        ":symbol_table",
        ":verilog_project",
        "//common/text:symbol",
        "//common/text:tree_utils",
        "//common/util:file_util",
        "//common/util:logging",
        "//common/util:range",
        "//common/util:tree_operations",
        "@com_google_absl//absl/base:core_headers",
        "@com_google_absl//absl/memory",
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
        ":symbol_table",
        ":verilog_project",
        "//common/strings:compare",
        "//common/strings:display_utils",
        "//common/util:logging",
        "@com_google_absl//absl/strings",
    ],
)

cc_test(
    name = "dependencies_test",
    srcs = ["dependencies_test.cc"],
    deps = [
        ":dependencies",
        "//common/util:file_util",
        "@com_google_absl//absl/status",
        "@com_google_googletest//:gtest_main",
    ],
)
