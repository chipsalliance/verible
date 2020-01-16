# A set of linter rules to be used in analysis of verilog code.

licenses(["notice"])

default_visibility = [
    "//verilog/analysis:__subpackages__",
    "//verilog/tools:__subpackages__",
]

package(
    default_visibility = default_visibility,
)

cc_library(
    name = "verilog_lint_rules",
    deps = [
        ":always_comb_blocking_rule",
        ":always_comb_rule",
        ":always_ff_non_blocking_rule",
        ":case_missing_default_rule",
        ":create_object_name_match_rule",
        ":endif_comment_rule",
        ":enum_name_style_rule",
        ":explicit_function_lifetime_rule",
        ":explicit_function_task_parameter_type_rule",
        ":explicit_parameter_storage_type_rule",
        ":explicit_task_lifetime_rule",
        ":forbid_defparam_rule",
        ":forbidden_anonymous_enums_rule",
        ":forbidden_anonymous_structs_unions_rule",
        ":forbidden_macro_rule",
        ":forbidden_symbol_rule",
        ":generate_label_rule",
        ":line_length_rule",
        ":macro_name_style_rule",
        ":mismatched_labels_rule",
        ":module_begin_block_rule",
        ":module_filename_rule",
        ":module_instantiation_rules",
        ":no_tabs_rule",
        ":no_trailing_spaces_rule",
        ":package_filename_rule",
        ":packed_dimensions_rule",
        ":parameter_name_style_rule",
        ":plusarg_assignment_rule",
        ":posix_eof_rule",
        ":proper_parameter_declaration_rule",
        ":signal_name_style_rule",
        ":struct_union_name_style_rule",
        ":undersized_binary_literal_rule",
        ":unpacked_dimensions_rule",
        ":v2001_generate_begin_rule",
        ":void_cast_rule",
    ],
)

cc_library(
    name = "no_tabs_rule",
    srcs = ["no_tabs_rule.cc"],
    hdrs = ["no_tabs_rule.h"],
    deps = [
        "//common/analysis:citation",
        "//common/analysis:line_lint_rule",
        "//common/analysis:lint_rule_status",
        "//common/text:token_info",
        "//verilog/analysis:descriptions",
        "//verilog/analysis:lint_rule_registry",
        "//verilog/parser:verilog_token_enum",
        "@com_google_absl//absl/strings",
    ],
    alwayslink = 1,
)

cc_test(
    name = "no_tabs_rule_test",
    srcs = ["no_tabs_rule_test.cc"],
    deps = [
        ":no_tabs_rule",
        "//common/analysis:line_linter_test_utils",
        "//common/analysis:linter_test_utils",
        "//common/text:symbol",
        "//verilog/analysis:verilog_analyzer",
        "//verilog/parser:verilog_token_enum",
        "@com_google_googletest//:gtest_main",
    ],
)

cc_library(
    name = "no_trailing_spaces_rule",
    srcs = ["no_trailing_spaces_rule.cc"],
    hdrs = ["no_trailing_spaces_rule.h"],
    deps = [
        "//common/analysis:citation",
        "//common/analysis:line_lint_rule",
        "//common/analysis:lint_rule_status",
        "//common/text:token_info",
        "//verilog/analysis:descriptions",
        "//verilog/analysis:lint_rule_registry",
        "//verilog/parser:verilog_token_enum",
        "@com_google_absl//absl/strings",
    ],
    alwayslink = 1,
)

cc_test(
    name = "no_trailing_spaces_rule_test",
    srcs = ["no_trailing_spaces_rule_test.cc"],
    deps = [
        ":no_trailing_spaces_rule",
        "//common/analysis:line_linter_test_utils",
        "//common/analysis:linter_test_utils",
        "//common/text:symbol",
        "//verilog/analysis:verilog_analyzer",
        "//verilog/parser:verilog_token_enum",
        "@com_google_googletest//:gtest_main",
    ],
)

cc_library(
    name = "posix_eof_rule",
    srcs = ["posix_eof_rule.cc"],
    hdrs = ["posix_eof_rule.h"],
    deps = [
        "//common/analysis:citation",
        "//common/analysis:lint_rule_status",
        "//common/analysis:text_structure_lint_rule",
        "//common/text:text_structure",
        "//common/text:token_info",
        "//verilog/analysis:descriptions",
        "//verilog/analysis:lint_rule_registry",
        "//verilog/parser:verilog_token_enum",
        "@com_google_absl//absl/strings",
    ],
    alwayslink = 1,
)

cc_test(
    name = "posix_eof_rule_test",
    srcs = ["posix_eof_rule_test.cc"],
    deps = [
        ":posix_eof_rule",
        "//common/analysis:linter_test_utils",
        "//common/analysis:text_structure_linter_test_utils",
        "//common/text:symbol",
        "//verilog/analysis:verilog_analyzer",
        "//verilog/parser:verilog_token_enum",
        "@com_google_googletest//:gtest_main",
    ],
)

cc_library(
    name = "line_length_rule",
    srcs = ["line_length_rule.cc"],
    hdrs = ["line_length_rule.h"],
    deps = [
        "//common/analysis:citation",
        "//common/analysis:lint_rule_status",
        "//common/analysis:text_structure_lint_rule",
        "//common/strings:comment_utils",
        "//common/text:constants",
        "//common/text:text_structure",
        "//common/text:token_info",
        "//common/text:token_stream_view",
        "//common/util:iterator_range",
        "//common/util:logging",
        "//verilog/analysis:descriptions",
        "//verilog/analysis:lint_rule_registry",
        "//verilog/analysis:verilog_linter_constants",
        "//verilog/parser:verilog_token_enum",
        "@com_google_absl//absl/strings",
    ],
    alwayslink = 1,
)

cc_test(
    name = "line_length_rule_test",
    srcs = ["line_length_rule_test.cc"],
    deps = [
        ":line_length_rule",
        "//common/analysis:linter_test_utils",
        "//common/analysis:text_structure_linter_test_utils",
        "//common/text:symbol",
        "//verilog/analysis:verilog_analyzer",
        "//verilog/parser:verilog_token_enum",
        "@com_google_googletest//:gtest_main",
    ],
)

cc_library(
    name = "forbid_defparam_rule",
    srcs = ["forbid_defparam_rule.cc"],
    hdrs = ["forbid_defparam_rule.h"],
    deps = [
        "//common/analysis:citation",
        "//common/analysis:lint_rule_status",
        "//common/analysis:syntax_tree_lint_rule",
        "//common/analysis/matcher",
        "//common/analysis/matcher:bound_symbol_manager",
        "//common/analysis/matcher:matcher_builders",
        "//common/text:symbol",
        "//common/text:syntax_tree_context",
        "//verilog/CST:verilog_matchers",  # fixdeps: keep
        "//verilog/analysis:descriptions",
        "//verilog/analysis:lint_rule_registry",
        "@com_google_absl//absl/strings",
    ],
    alwayslink = 1,
)

cc_test(
    name = "forbid_defparam_rule_test",
    srcs = ["forbid_defparam_rule_test.cc"],
    deps = [
        ":forbid_defparam_rule",
        "//common/analysis:linter_test_utils",
        "//common/analysis:syntax_tree_linter_test_utils",
        "//common/text:symbol",
        "//verilog/CST:verilog_nonterminals",
        "//verilog/analysis:verilog_analyzer",
        "@com_google_googletest//:gtest_main",
    ],
)

cc_library(
    name = "mismatched_labels_rule",
    srcs = ["mismatched_labels_rule.cc"],
    hdrs = ["mismatched_labels_rule.h"],
    deps = [
        "//common/analysis:citation",
        "//common/analysis:lint_rule_status",
        "//common/analysis:syntax_tree_lint_rule",
        "//common/analysis:syntax_tree_search",
        "//common/analysis/matcher",
        "//common/analysis/matcher:bound_symbol_manager",
        "//common/analysis/matcher:matcher_builders",
        "//common/text:concrete_syntax_tree",
        "//common/text:symbol",
        "//common/text:syntax_tree_context",
        "//common/text:tree_utils",
        "//verilog/CST:identifier",
        "//verilog/CST:seq_block",
        "//verilog/CST:verilog_matchers",  # fixdeps: keep
        "//verilog/analysis:descriptions",
        "//verilog/analysis:lint_rule_registry",
        "@com_google_absl//absl/strings",
    ],
    alwayslink = 1,
)

cc_test(
    name = "mismatched_labels_rule_test",
    srcs = ["mismatched_labels_rule_test.cc"],
    deps = [
        ":mismatched_labels_rule",
        "//common/analysis:linter_test_utils",
        "//common/analysis:syntax_tree_linter_test_utils",
        "//common/text:symbol",
        "//verilog/CST:verilog_nonterminals",
        "//verilog/analysis:verilog_analyzer",
        "@com_google_googletest//:gtest_main",
    ],
)

cc_library(
    name = "module_begin_block_rule",
    srcs = ["module_begin_block_rule.cc"],
    hdrs = ["module_begin_block_rule.h"],
    deps = [
        "//common/analysis:citation",
        "//common/analysis:lint_rule_status",
        "//common/analysis:syntax_tree_lint_rule",
        "//common/analysis/matcher",
        "//common/analysis/matcher:bound_symbol_manager",
        "//common/analysis/matcher:matcher_builders",
        "//common/text:symbol",
        "//common/text:syntax_tree_context",
        "//verilog/CST:verilog_matchers",  # fixdeps: keep
        "//verilog/analysis:descriptions",
        "//verilog/analysis:lint_rule_registry",
        "@com_google_absl//absl/strings",
    ],
    alwayslink = 1,
)

cc_test(
    name = "module_begin_block_rule_test",
    srcs = ["module_begin_block_rule_test.cc"],
    deps = [
        ":module_begin_block_rule",
        "//common/analysis:linter_test_utils",
        "//common/analysis:syntax_tree_linter_test_utils",
        "//common/text:symbol",
        "//verilog/CST:verilog_nonterminals",
        "//verilog/analysis:verilog_analyzer",
        "@com_google_googletest//:gtest_main",
    ],
)

cc_library(
    name = "module_filename_rule",
    srcs = ["module_filename_rule.cc"],
    hdrs = ["module_filename_rule.h"],
    deps = [
        "//common/analysis:citation",
        "//common/analysis:lint_rule_status",
        "//common/analysis:syntax_tree_search",
        "//common/analysis:text_structure_lint_rule",
        "//common/text:symbol",
        "//common/text:text_structure",
        "//common/text:token_info",
        "//common/util:file_util",
        "//verilog/CST:module",
        "//verilog/analysis:descriptions",
        "//verilog/analysis:lint_rule_registry",
        "@com_google_absl//absl/strings",
    ],
    alwayslink = 1,
)

cc_test(
    name = "module_filename_rule_test",
    srcs = ["module_filename_rule_test.cc"],
    deps = [
        ":module_filename_rule",
        "//common/analysis:linter_test_utils",
        "//common/analysis:text_structure_linter_test_utils",
        "//common/text:symbol",
        "//verilog/analysis:verilog_analyzer",
        "//verilog/parser:verilog_token_enum",
        "@com_google_googletest//:gtest_main",
    ],
)

cc_library(
    name = "package_filename_rule",
    srcs = ["package_filename_rule.cc"],
    hdrs = ["package_filename_rule.h"],
    deps = [
        "//common/analysis:citation",
        "//common/analysis:lint_rule_status",
        "//common/analysis:syntax_tree_search",
        "//common/analysis:text_structure_lint_rule",
        "//common/text:text_structure",
        "//common/text:token_info",
        "//common/util:file_util",
        "//common/util:status",
        "//verilog/CST:package",
        "//verilog/analysis:descriptions",
        "//verilog/analysis:lint_rule_registry",
        "@com_google_absl//absl/strings",
    ],
    alwayslink = 1,
)

cc_test(
    name = "package_filename_rule_test",
    srcs = ["package_filename_rule_test.cc"],
    deps = [
        ":package_filename_rule",
        "//common/analysis:linter_test_utils",
        "//common/analysis:text_structure_linter_test_utils",
        "//common/text:symbol",
        "//verilog/analysis:verilog_analyzer",
        "//verilog/parser:verilog_token_enum",
        "@com_google_googletest//:gtest_main",
    ],
)

cc_library(
    name = "module_instantiation_rules",
    srcs = ["module_instantiation_rules.cc"],
    hdrs = ["module_instantiation_rules.h"],
    deps = [
        "//common/analysis:citation",
        "//common/analysis:lint_rule_status",
        "//common/analysis:syntax_tree_lint_rule",
        "//common/analysis/matcher",
        "//common/analysis/matcher:bound_symbol_manager",
        "//common/analysis/matcher:matcher_builders",
        "//common/text:concrete_syntax_leaf",
        "//common/text:concrete_syntax_tree",
        "//common/text:symbol",
        "//common/text:syntax_tree_context",
        "//common/text:token_info",
        "//common/text:tree_utils",
        "//common/util:casts",
        "//common/util:logging",
        "//verilog/CST:context_functions",
        "//verilog/CST:verilog_matchers",
        "//verilog/CST:verilog_nonterminals",
        "//verilog/analysis:descriptions",
        "//verilog/analysis:lint_rule_registry",
        "@com_google_absl//absl/strings",
    ],
    alwayslink = 1,
)

cc_test(
    name = "module_instantiation_rules_test",
    srcs = ["module_instantiation_rules_test.cc"],
    deps = [
        ":module_instantiation_rules",
        "//common/analysis:linter_test_utils",
        "//common/analysis:syntax_tree_linter_test_utils",
        "//common/text:symbol",
        "//verilog/CST:verilog_nonterminals",
        "//verilog/CST:verilog_treebuilder_utils",
        "//verilog/analysis:verilog_analyzer",
        "//verilog/parser:verilog_token_enum",
        "@com_google_googletest//:gtest_main",
    ],
)

cc_library(
    name = "endif_comment_rule",
    srcs = ["endif_comment_rule.cc"],
    hdrs = ["endif_comment_rule.h"],
    deps = [
        "//common/analysis:citation",
        "//common/analysis:lint_rule_status",
        "//common/analysis:token_stream_lint_rule",
        "//common/strings:comment_utils",
        "//common/text:token_info",
        "//verilog/analysis:descriptions",
        "//verilog/analysis:lint_rule_registry",
        "//verilog/parser:verilog_token_enum",
        "@com_google_absl//absl/strings",
    ],
    alwayslink = 1,
)

cc_test(
    name = "endif_comment_rule_test",
    srcs = ["endif_comment_rule_test.cc"],
    deps = [
        ":endif_comment_rule",
        "//common/analysis:linter_test_utils",
        "//common/analysis:token_stream_linter_test_utils",
        "//common/text:symbol",
        "//verilog/analysis:verilog_analyzer",
        "//verilog/parser:verilog_token_enum",
        "@com_google_googletest//:gtest_main",
    ],
)

cc_library(
    name = "forbidden_anonymous_enums_rule",
    srcs = ["forbidden_anonymous_enums_rule.cc"],
    hdrs = ["forbidden_anonymous_enums_rule.h"],
    deps = [
        "//common/analysis:citation",
        "//common/analysis:lint_rule_status",
        "//common/analysis:syntax_tree_lint_rule",
        "//common/analysis/matcher",
        "//common/analysis/matcher:bound_symbol_manager",
        "//common/analysis/matcher:matcher_builders",
        "//common/text:symbol",
        "//common/text:syntax_tree_context",
        "//verilog/CST:verilog_matchers",
        "//verilog/analysis:descriptions",
        "//verilog/analysis:lint_rule_registry",
        "@com_google_absl//absl/strings",
    ],
    alwayslink = 1,
)

cc_test(
    name = "forbidden_anonymous_enums_rule_test",
    srcs = ["forbidden_anonymous_enums_rule_test.cc"],
    deps = [
        ":forbidden_anonymous_enums_rule",
        "//common/analysis:syntax_tree_linter_test_utils",
        "//common/text:symbol",
        "//verilog/analysis:verilog_analyzer",
        "@com_google_googletest//:gtest_main",
    ],
)

cc_library(
    name = "enum_name_style_rule",
    srcs = ["enum_name_style_rule.cc"],
    hdrs = ["enum_name_style_rule.h"],
    deps = [
        "//common/analysis:citation",
        "//common/analysis:lint_rule_status",
        "//common/analysis:syntax_tree_lint_rule",
        "//common/analysis/matcher",
        "//common/analysis/matcher:bound_symbol_manager",
        "//common/analysis/matcher:matcher_builders",
        "//common/strings:naming_utils",
        "//common/text:symbol",
        "//common/text:syntax_tree_context",
        "//verilog/CST:type",
        "//verilog/CST:verilog_matchers",
        "//verilog/analysis:descriptions",
        "//verilog/analysis:lint_rule_registry",
        "@com_google_absl//absl/strings",
    ],
    alwayslink = 1,
)

cc_test(
    name = "enum_name_style_rule_test",
    srcs = ["enum_name_style_rule_test.cc"],
    deps = [
        ":enum_name_style_rule",
        "//common/analysis:linter_test_utils",
        "//common/analysis:syntax_tree_linter_test_utils",
        "//common/text:symbol",
        "//verilog/CST:verilog_nonterminals",
        "//verilog/analysis:verilog_analyzer",
        "@com_google_googletest//:gtest_main",
    ],
)

cc_library(
    name = "forbidden_macro_rule",
    srcs = ["forbidden_macro_rule.cc"],
    hdrs = ["forbidden_macro_rule.h"],
    deps = [
        "//common/analysis:citation",
        "//common/analysis:lint_rule_status",
        "//common/analysis:syntax_tree_lint_rule",
        "//common/analysis/matcher",
        "//common/analysis/matcher:bound_symbol_manager",
        "//common/analysis/matcher:matcher_builders",
        "//common/text:concrete_syntax_leaf",
        "//common/text:symbol",
        "//common/text:syntax_tree_context",
        "//common/text:token_info",
        "//common/util:container_util",
        "//verilog/CST:verilog_matchers",
        "//verilog/analysis:descriptions",
        "//verilog/analysis:lint_rule_registry",
        "@com_google_absl//absl/strings",
    ],
    alwayslink = 1,
)

cc_library(
    name = "forbidden_anonymous_structs_unions_rule",
    srcs = ["forbidden_anonymous_structs_unions_rule.cc"],
    hdrs = ["forbidden_anonymous_structs_unions_rule.h"],
    deps = [
        "//common/analysis:citation",
        "//common/analysis:lint_rule_status",
        "//common/analysis:syntax_tree_lint_rule",
        "//common/analysis/matcher",
        "//common/analysis/matcher:bound_symbol_manager",
        "//common/analysis/matcher:matcher_builders",
        "//common/text:symbol",
        "//common/text:syntax_tree_context",
        "//verilog/CST:verilog_matchers",
        "//verilog/analysis:descriptions",
        "//verilog/analysis:lint_rule_registry",
        "@com_google_absl//absl/strings",
    ],
    alwayslink = 1,
)

cc_test(
    name = "forbidden_anonymous_structs_unions_rule_test",
    srcs = ["forbidden_anonymous_structs_unions_rule_test.cc"],
    deps = [
        ":forbidden_anonymous_structs_unions_rule",
        "//common/analysis:syntax_tree_linter_test_utils",
        "//common/text:symbol",
        "//verilog/analysis:verilog_analyzer",
        "@com_google_googletest//:gtest_main",
    ],
)

cc_test(
    name = "forbidden_macro_rule_test",
    srcs = ["forbidden_macro_rule_test.cc"],
    deps = [
        ":forbidden_macro_rule",
        "//common/analysis:linter_test_utils",
        "//common/analysis:syntax_tree_linter_test_utils",
        "//common/text:symbol",
        "//verilog/CST:verilog_treebuilder_utils",
        "//verilog/analysis:verilog_analyzer",
        "//verilog/parser:verilog_token_enum",
        "@com_google_googletest//:gtest_main",
    ],
)

cc_library(
    name = "forbidden_symbol_rule",
    srcs = ["forbidden_symbol_rule.cc"],
    hdrs = ["forbidden_symbol_rule.h"],
    deps = [
        "//common/analysis:citation",
        "//common/analysis:lint_rule_status",
        "//common/analysis:syntax_tree_lint_rule",
        "//common/analysis/matcher",
        "//common/analysis/matcher:bound_symbol_manager",
        "//common/analysis/matcher:matcher_builders",
        "//common/text:concrete_syntax_leaf",
        "//common/text:symbol",
        "//common/text:syntax_tree_context",
        "//common/text:token_info",
        "//common/util:container_util",
        "//verilog/CST:verilog_matchers",
        "//verilog/analysis:descriptions",
        "//verilog/analysis:lint_rule_registry",
        "@com_google_absl//absl/strings",
    ],
    alwayslink = 1,
)

cc_test(
    name = "forbidden_symbol_rule_test",
    srcs = ["forbidden_symbol_rule_test.cc"],
    deps = [
        ":forbidden_symbol_rule",
        "//common/analysis:linter_test_utils",
        "//common/analysis:syntax_tree_linter_test_utils",
        "//common/text:symbol",
        "//verilog/CST:verilog_treebuilder_utils",
        "//verilog/analysis:verilog_analyzer",
        "//verilog/parser:verilog_token_enum",
        "@com_google_googletest//:gtest_main",
    ],
)

cc_library(
    name = "generate_label_rule",
    srcs = ["generate_label_rule.cc"],
    hdrs = ["generate_label_rule.h"],
    deps = [
        "//common/analysis:citation",
        "//common/analysis:lint_rule_status",
        "//common/analysis:syntax_tree_lint_rule",
        "//common/analysis/matcher",
        "//common/analysis/matcher:bound_symbol_manager",
        "//common/analysis/matcher:core_matchers",
        "//common/analysis/matcher:matcher_builders",
        "//common/text:symbol",
        "//common/text:syntax_tree_context",
        "//verilog/CST:verilog_matchers",
        "//verilog/analysis:descriptions",
        "//verilog/analysis:lint_rule_registry",
        "@com_google_absl//absl/strings",
    ],
    alwayslink = 1,
)

cc_test(
    name = "generate_label_rule_test",
    srcs = ["generate_label_rule_test.cc"],
    deps = [
        ":generate_label_rule",
        "//common/analysis:linter_test_utils",
        "//common/analysis:syntax_tree_linter_test_utils",
        "//common/text:symbol",
        "//verilog/CST:verilog_nonterminals",
        "//verilog/CST:verilog_treebuilder_utils",
        "//verilog/analysis:verilog_analyzer",
        "@com_google_googletest//:gtest_main",
    ],
)

cc_library(
    name = "v2001_generate_begin_rule",
    srcs = ["v2001_generate_begin_rule.cc"],
    hdrs = ["v2001_generate_begin_rule.h"],
    deps = [
        "//common/analysis:citation",
        "//common/analysis:lint_rule_status",
        "//common/analysis:syntax_tree_lint_rule",
        "//common/analysis/matcher",
        "//common/analysis/matcher:bound_symbol_manager",
        "//common/analysis/matcher:matcher_builders",
        "//common/text:concrete_syntax_leaf",
        "//common/text:concrete_syntax_tree",
        "//common/text:symbol",
        "//common/text:syntax_tree_context",
        "//common/text:tree_utils",
        "//verilog/CST:verilog_matchers",
        "//verilog/analysis:descriptions",
        "//verilog/analysis:lint_rule_registry",
        "@com_google_absl//absl/strings",
    ],
    alwayslink = 1,
)

cc_test(
    name = "v2001_generate_begin_rule_test",
    srcs = ["v2001_generate_begin_rule_test.cc"],
    deps = [
        ":forbidden_symbol_rule",
        ":v2001_generate_begin_rule",
        "//common/analysis:linter_test_utils",
        "//common/analysis:syntax_tree_linter_test_utils",
        "//common/text:symbol",
        "//verilog/CST:verilog_nonterminals",
        "//verilog/CST:verilog_treebuilder_utils",
        "//verilog/analysis:verilog_analyzer",
        "@com_google_googletest//:gtest_main",
    ],
)

cc_library(
    name = "void_cast_rule",
    srcs = ["void_cast_rule.cc"],
    hdrs = ["void_cast_rule.h"],
    deps = [
        "//common/analysis:citation",
        "//common/analysis:lint_rule_status",
        "//common/analysis:syntax_tree_lint_rule",
        "//common/analysis/matcher",
        "//common/analysis/matcher:bound_symbol_manager",
        "//common/analysis/matcher:core_matchers",
        "//common/analysis/matcher:matcher_builders",
        "//common/text:concrete_syntax_leaf",
        "//common/text:concrete_syntax_tree",
        "//common/text:symbol",
        "//common/text:syntax_tree_context",
        "//common/text:token_info",
        "//common/text:tree_utils",
        "//verilog/CST:verilog_matchers",
        "//verilog/analysis:descriptions",
        "//verilog/analysis:lint_rule_registry",
        "@com_google_absl//absl/strings",
    ],
    alwayslink = 1,
)

cc_test(
    name = "void_cast_rule_test",
    srcs = ["void_cast_rule_test.cc"],
    deps = [
        ":void_cast_rule",
        "//common/analysis:linter_test_utils",
        "//common/analysis:syntax_tree_linter_test_utils",
        "//common/text:symbol",
        "//verilog/CST:verilog_nonterminals",
        "//verilog/CST:verilog_treebuilder_utils",
        "//verilog/analysis:verilog_analyzer",
        "//verilog/parser:verilog_token_enum",
        "@com_google_googletest//:gtest_main",
    ],
)

cc_library(
    name = "undersized_binary_literal_rule",
    srcs = ["undersized_binary_literal_rule.cc"],
    hdrs = ["undersized_binary_literal_rule.h"],
    deps = [
        "//common/analysis:citation",
        "//common/analysis:lint_rule_status",
        "//common/analysis:syntax_tree_lint_rule",
        "//common/analysis/matcher",
        "//common/analysis/matcher:bound_symbol_manager",
        "//common/analysis/matcher:matcher_builders",
        "//common/text:concrete_syntax_leaf",
        "//common/text:symbol",
        "//common/text:syntax_tree_context",
        "//common/text:token_info",
        "//common/util:logging",
        "//verilog/CST:numbers",
        "//verilog/CST:verilog_matchers",
        "//verilog/analysis:descriptions",
        "//verilog/analysis:lint_rule_registry",
        "@com_google_absl//absl/strings",
    ],
    alwayslink = 1,
)

cc_test(
    name = "undersized_binary_literal_rule_test",
    srcs = ["undersized_binary_literal_rule_test.cc"],
    deps = [
        ":undersized_binary_literal_rule",
        "//common/analysis:linter_test_utils",
        "//common/analysis:syntax_tree_linter_test_utils",
        "//common/text:symbol",
        "//verilog/CST:verilog_nonterminals",
        "//verilog/analysis:verilog_analyzer",
        "@com_google_googletest//:gtest_main",
    ],
)

cc_library(
    name = "create_object_name_match_rule",
    srcs = ["create_object_name_match_rule.cc"],
    hdrs = ["create_object_name_match_rule.h"],
    deps = [
        "//common/analysis:citation",
        "//common/analysis:lint_rule_status",
        "//common/analysis:syntax_tree_lint_rule",
        "//common/analysis/matcher",
        "//common/analysis/matcher:bound_symbol_manager",
        "//common/analysis/matcher:core_matchers",
        "//common/analysis/matcher:matcher_builders",
        "//common/text:concrete_syntax_leaf",
        "//common/text:concrete_syntax_tree",
        "//common/text:symbol",
        "//common/text:syntax_tree_context",
        "//common/text:token_info",
        "//common/util:casts",
        "//verilog/CST:verilog_matchers",
        "//verilog/CST:verilog_nonterminals",
        "//verilog/analysis:descriptions",
        "//verilog/analysis:lint_rule_registry",
        "//verilog/parser:verilog_token_enum",
        "@com_google_absl//absl/strings",
    ],
    alwayslink = 1,
)

cc_test(
    name = "create_object_name_match_rule_test",
    srcs = ["create_object_name_match_rule_test.cc"],
    deps = [
        ":create_object_name_match_rule",
        "//common/analysis:linter_test_utils",
        "//common/analysis:syntax_tree_linter_test_utils",
        "//common/text:symbol",
        "//verilog/CST:verilog_treebuilder_utils",
        "//verilog/analysis:verilog_analyzer",
        "//verilog/parser:verilog_token_enum",
        "@com_google_googletest//:gtest_main",
    ],
)

cc_library(
    name = "always_comb_rule",
    srcs = ["always_comb_rule.cc"],
    hdrs = ["always_comb_rule.h"],
    deps = [
        "//common/analysis:citation",
        "//common/analysis:lint_rule_status",
        "//common/analysis:syntax_tree_lint_rule",
        "//common/analysis/matcher",
        "//common/analysis/matcher:bound_symbol_manager",
        "//common/analysis/matcher:matcher_builders",
        "//common/text:symbol",
        "//common/text:syntax_tree_context",
        "//verilog/CST:verilog_matchers",
        "//verilog/analysis:descriptions",
        "//verilog/analysis:lint_rule_registry",
        "@com_google_absl//absl/strings",
    ],
    alwayslink = 1,
)

cc_test(
    name = "always_comb_rule_test",
    srcs = ["always_comb_rule_test.cc"],
    deps = [
        ":always_comb_rule",
        "//common/analysis:linter_test_utils",
        "//common/analysis:syntax_tree_linter_test_utils",
        "//common/text:symbol",
        "//verilog/CST:verilog_nonterminals",
        "//verilog/CST:verilog_treebuilder_utils",
        "//verilog/analysis:verilog_analyzer",
        "@com_google_googletest//:gtest_main",
    ],
)

cc_library(
    name = "always_comb_blocking_rule",
    srcs = ["always_comb_blocking_rule.cc"],
    hdrs = ["always_comb_blocking_rule.h"],
    deps = [
        "//common/analysis:citation",
        "//common/analysis:lint_rule_status",
        "//common/analysis:syntax_tree_lint_rule",
        "//common/analysis:syntax_tree_search",
        "//common/analysis/matcher",
        "//common/analysis/matcher:bound_symbol_manager",
        "//common/analysis/matcher:matcher_builders",
        "//common/text:symbol",
        "//common/text:syntax_tree_context",
        "//verilog/CST:verilog_matchers",
        "//verilog/analysis:descriptions",
        "//verilog/analysis:lint_rule_registry",
        "@com_google_absl//absl/strings",
    ],
    alwayslink = 1,
)

cc_test(
    name = "always_comb_blocking_rule_test",
    srcs = ["always_comb_blocking_rule_test.cc"],
    deps = [
        ":always_comb_blocking_rule",
        "//common/analysis:linter_test_utils",
        "//common/analysis:syntax_tree_linter_test_utils",
        "//common/text:symbol",
        "//verilog/CST:verilog_nonterminals",
        "//verilog/CST:verilog_treebuilder_utils",
        "//verilog/analysis:verilog_analyzer",
        "@com_google_googletest//:gtest_main",
    ],
)

cc_library(
    name = "always_ff_non_blocking_rule",
    srcs = ["always_ff_non_blocking_rule.cc"],
    hdrs = ["always_ff_non_blocking_rule.h"],
    deps = [
        "//common/analysis:citation",
        "//common/analysis:lint_rule_status",
        "//common/analysis:syntax_tree_lint_rule",
        "//common/analysis:syntax_tree_search",
        "//common/analysis/matcher",
        "//common/analysis/matcher:bound_symbol_manager",
        "//common/analysis/matcher:matcher_builders",
        "//common/text:symbol",
        "//common/text:syntax_tree_context",
        "//verilog/CST:verilog_matchers",
        "//verilog/analysis:descriptions",
        "//verilog/analysis:lint_rule_registry",
        "@com_google_absl//absl/strings",
    ],
    alwayslink = 1,
)

cc_test(
    name = "always_ff_non_blocking_rule_test",
    srcs = ["always_ff_non_blocking_rule_test.cc"],
    deps = [
        ":always_ff_non_blocking_rule",
        "//common/analysis:linter_test_utils",
        "//common/analysis:syntax_tree_linter_test_utils",
        "//common/text:symbol",
        "//verilog/CST:verilog_nonterminals",
        "//verilog/CST:verilog_treebuilder_utils",
        "//verilog/analysis:verilog_analyzer",
        "@com_google_googletest//:gtest_main",
    ],
)

cc_library(
    name = "explicit_function_lifetime_rule",
    srcs = ["explicit_function_lifetime_rule.cc"],
    hdrs = ["explicit_function_lifetime_rule.h"],
    deps = [
        "//common/analysis:citation",
        "//common/analysis:lint_rule_status",
        "//common/analysis:syntax_tree_lint_rule",
        "//common/analysis/matcher",
        "//common/analysis/matcher:bound_symbol_manager",
        "//common/analysis/matcher:matcher_builders",
        "//common/text:symbol",
        "//common/text:syntax_tree_context",
        "//common/util:logging",
        "//verilog/CST:context_functions",
        "//verilog/CST:functions",
        "//verilog/CST:identifier",
        "//verilog/CST:verilog_matchers",
        "//verilog/analysis:descriptions",
        "//verilog/analysis:lint_rule_registry",
        "@com_google_absl//absl/strings",
    ],
    alwayslink = 1,
)

cc_test(
    name = "explicit_function_lifetime_rule_test",
    srcs = ["explicit_function_lifetime_rule_test.cc"],
    deps = [
        ":explicit_function_lifetime_rule",
        "//common/analysis:linter_test_utils",
        "//common/analysis:syntax_tree_linter_test_utils",
        "//common/text:symbol",
        "//verilog/CST:verilog_nonterminals",
        "//verilog/analysis:verilog_analyzer",
        "@com_google_googletest//:gtest_main",
    ],
)

cc_library(
    name = "explicit_task_lifetime_rule",
    srcs = ["explicit_task_lifetime_rule.cc"],
    hdrs = ["explicit_task_lifetime_rule.h"],
    deps = [
        "//common/analysis:citation",
        "//common/analysis:lint_rule_status",
        "//common/analysis:syntax_tree_lint_rule",
        "//common/analysis/matcher",
        "//common/analysis/matcher:bound_symbol_manager",
        "//common/analysis/matcher:matcher_builders",
        "//common/text:symbol",
        "//common/text:syntax_tree_context",
        "//common/text:tree_utils",
        "//verilog/CST:context_functions",
        "//verilog/CST:identifier",
        "//verilog/CST:tasks",
        "//verilog/CST:verilog_matchers",
        "//verilog/analysis:descriptions",
        "//verilog/analysis:lint_rule_registry",
        "@com_google_absl//absl/strings",
    ],
    alwayslink = 1,
)

cc_test(
    name = "explicit_task_lifetime_rule_test",
    srcs = ["explicit_task_lifetime_rule_test.cc"],
    deps = [
        ":explicit_task_lifetime_rule",
        "//common/analysis:linter_test_utils",
        "//common/analysis:syntax_tree_linter_test_utils",
        "//common/text:symbol",
        "//verilog/CST:verilog_nonterminals",
        "//verilog/analysis:verilog_analyzer",
        "@com_google_googletest//:gtest_main",
    ],
)

cc_library(
    name = "packed_dimensions_rule",
    srcs = ["packed_dimensions_rule.cc"],
    hdrs = ["packed_dimensions_rule.h"],
    deps = [
        "//common/analysis:citation",
        "//common/analysis:lint_rule_status",
        "//common/analysis:syntax_tree_lint_rule",
        "//common/analysis/matcher",
        "//common/analysis/matcher:bound_symbol_manager",
        "//common/analysis/matcher:matcher_builders",
        "//common/text:symbol",
        "//common/text:syntax_tree_context",
        "//common/text:tree_utils",
        "//common/util:logging",
        "//verilog/CST:context_functions",
        "//verilog/CST:dimensions",
        "//verilog/CST:expression",
        "//verilog/CST:verilog_matchers",
        "//verilog/analysis:descriptions",
        "//verilog/analysis:lint_rule_registry",
        "@com_google_absl//absl/strings",
    ],
    alwayslink = 1,
)

cc_test(
    name = "packed_dimensions_rule_test",
    srcs = ["packed_dimensions_rule_test.cc"],
    deps = [
        ":packed_dimensions_rule",
        "//common/analysis:linter_test_utils",
        "//common/analysis:syntax_tree_linter_test_utils",
        "//common/text:symbol",
        "//verilog/CST:verilog_nonterminals",
        "//verilog/analysis:verilog_analyzer",
        "@com_google_googletest//:gtest_main",
    ],
)

cc_library(
    name = "unpacked_dimensions_rule",
    srcs = ["unpacked_dimensions_rule.cc"],
    hdrs = ["unpacked_dimensions_rule.h"],
    deps = [
        "//common/analysis:citation",
        "//common/analysis:lint_rule_status",
        "//common/analysis:syntax_tree_lint_rule",
        "//common/analysis/matcher",
        "//common/analysis/matcher:bound_symbol_manager",
        "//common/analysis/matcher:matcher_builders",
        "//common/text:symbol",
        "//common/text:syntax_tree_context",
        "//common/util:logging",
        "//verilog/CST:context_functions",
        "//verilog/CST:dimensions",
        "//verilog/CST:expression",
        "//verilog/CST:verilog_matchers",
        "//verilog/analysis:descriptions",
        "//verilog/analysis:lint_rule_registry",
        "@com_google_absl//absl/strings",
    ],
    alwayslink = 1,
)

cc_test(
    name = "unpacked_dimensions_rule_test",
    srcs = ["unpacked_dimensions_rule_test.cc"],
    deps = [
        ":unpacked_dimensions_rule",
        "//common/analysis:linter_test_utils",
        "//common/analysis:syntax_tree_linter_test_utils",
        "//common/text:symbol",
        "//verilog/CST:verilog_nonterminals",
        "//verilog/analysis:verilog_analyzer",
        "@com_google_googletest//:gtest_main",
    ],
)

cc_library(
    name = "case_missing_default_rule",
    srcs = ["case_missing_default_rule.cc"],
    hdrs = ["case_missing_default_rule.h"],
    deps = [
        "//common/analysis:citation",
        "//common/analysis:lint_rule_status",
        "//common/analysis:syntax_tree_lint_rule",
        "//common/analysis/matcher",
        "//common/analysis/matcher:bound_symbol_manager",
        "//common/analysis/matcher:core_matchers",
        "//common/analysis/matcher:matcher_builders",
        "//common/text:symbol",
        "//common/text:syntax_tree_context",
        "//verilog/CST:verilog_matchers",
        "//verilog/analysis:descriptions",
        "//verilog/analysis:lint_rule_registry",
        "@com_google_absl//absl/strings",
    ],
    alwayslink = 1,
)

cc_test(
    name = "case_missing_default_rule_test",
    srcs = ["case_missing_default_rule_test.cc"],
    deps = [
        ":case_missing_default_rule",
        "//common/analysis:linter_test_utils",
        "//common/analysis:syntax_tree_linter_test_utils",
        "//common/text:symbol",
        "//verilog/CST:verilog_nonterminals",
        "//verilog/analysis:verilog_analyzer",
        "@com_google_googletest//:gtest_main",
    ],
)

cc_library(
    name = "macro_name_style_rule",
    srcs = ["macro_name_style_rule.cc"],
    hdrs = ["macro_name_style_rule.h"],
    deps = [
        "//common/analysis:citation",
        "//common/analysis:lint_rule_status",
        "//common/analysis:token_stream_lint_rule",
        "//common/strings:naming_utils",
        "//common/text:token_info",
        "//verilog/analysis:descriptions",
        "//verilog/analysis:lint_rule_registry",
        "//verilog/parser:verilog_token_enum",
        "@com_google_absl//absl/strings",
    ],
    alwayslink = 1,
)

cc_test(
    name = "macro_name_style_rule_test",
    srcs = ["macro_name_style_rule_test.cc"],
    deps = [
        ":macro_name_style_rule",
        "//common/analysis:linter_test_utils",
        "//common/analysis:token_stream_linter_test_utils",
        "//common/text:symbol",
        "//verilog/analysis:verilog_analyzer",
        "//verilog/parser:verilog_token_enum",
        "@com_google_googletest//:gtest_main",
    ],
)

cc_library(
    name = "parameter_name_style_rule",
    srcs = ["parameter_name_style_rule.cc"],
    hdrs = ["parameter_name_style_rule.h"],
    deps = [
        "//common/analysis:citation",
        "//common/analysis:lint_rule_status",
        "//common/analysis:syntax_tree_lint_rule",
        "//common/analysis/matcher",
        "//common/analysis/matcher:bound_symbol_manager",
        "//common/analysis/matcher:matcher_builders",
        "//common/strings:naming_utils",
        "//common/text:symbol",
        "//common/text:syntax_tree_context",
        "//common/text:token_info",
        "//verilog/CST:parameters",
        "//verilog/CST:verilog_matchers",
        "//verilog/analysis:descriptions",
        "//verilog/analysis:lint_rule_registry",
        "//verilog/parser:verilog_token_enum",
        "@com_google_absl//absl/strings",
    ],
    alwayslink = 1,
)

cc_test(
    name = "parameter_name_style_rule_test",
    srcs = ["parameter_name_style_rule_test.cc"],
    deps = [
        ":parameter_name_style_rule",
        "//common/analysis:linter_test_utils",
        "//common/analysis:syntax_tree_linter_test_utils",
        "//common/text:symbol",
        "//verilog/CST:verilog_nonterminals",
        "//verilog/analysis:verilog_analyzer",
        "@com_google_googletest//:gtest_main",
    ],
)

cc_library(
    name = "proper_parameter_declaration_rule",
    srcs = ["proper_parameter_declaration_rule.cc"],
    hdrs = ["proper_parameter_declaration_rule.h"],
    deps = [
        "//common/analysis:citation",
        "//common/analysis:lint_rule_status",
        "//common/analysis:syntax_tree_lint_rule",
        "//common/analysis/matcher",
        "//common/analysis/matcher:bound_symbol_manager",
        "//common/analysis/matcher:matcher_builders",
        "//common/text:symbol",
        "//common/text:syntax_tree_context",
        "//verilog/CST:context_functions",
        "//verilog/CST:parameters",
        "//verilog/CST:verilog_matchers",
        "//verilog/analysis:descriptions",
        "//verilog/analysis:lint_rule_registry",
        "//verilog/parser:verilog_token_enum",
        "@com_google_absl//absl/strings",
    ],
    alwayslink = 1,
)

cc_test(
    name = "proper_parameter_declaration_rule_test",
    srcs = ["proper_parameter_declaration_rule_test.cc"],
    deps = [
        ":proper_parameter_declaration_rule",
        "//common/analysis:linter_test_utils",
        "//common/analysis:syntax_tree_linter_test_utils",
        "//common/text:symbol",
        "//verilog/CST:verilog_nonterminals",
        "//verilog/analysis:verilog_analyzer",
        "@com_google_googletest//:gtest_main",
    ],
)

cc_library(
    name = "plusarg_assignment_rule",
    srcs = ["plusarg_assignment_rule.cc"],
    hdrs = ["plusarg_assignment_rule.h"],
    deps = [
        "//common/analysis:citation",
        "//common/analysis:lint_rule_status",
        "//common/analysis:syntax_tree_lint_rule",
        "//common/analysis/matcher",
        "//common/analysis/matcher:bound_symbol_manager",
        "//common/analysis/matcher:matcher_builders",
        "//common/text:concrete_syntax_leaf",
        "//common/text:symbol",
        "//common/text:syntax_tree_context",
        "//common/text:token_info",
        "//verilog/CST:verilog_matchers",
        "//verilog/analysis:descriptions",
        "//verilog/analysis:lint_rule_registry",
        "@com_google_absl//absl/strings",
    ],
    alwayslink = 1,
)

cc_test(
    name = "plusarg_assignment_rule_test",
    srcs = ["plusarg_assignment_rule_test.cc"],
    deps = [
        ":plusarg_assignment_rule",
        "//common/analysis:linter_test_utils",
        "//common/analysis:syntax_tree_linter_test_utils",
        "//common/text:symbol",
        "//verilog/CST:verilog_treebuilder_utils",
        "//verilog/analysis:verilog_analyzer",
        "//verilog/parser:verilog_token_enum",
        "@com_google_googletest//:gtest_main",
    ],
)

cc_library(
    name = "explicit_parameter_storage_type_rule",
    srcs = ["explicit_parameter_storage_type_rule.cc"],
    hdrs = ["explicit_parameter_storage_type_rule.h"],
    deps = [
        "//common/analysis:citation",
        "//common/analysis:lint_rule_status",
        "//common/analysis:syntax_tree_lint_rule",
        "//common/analysis/matcher",
        "//common/analysis/matcher:bound_symbol_manager",
        "//common/analysis/matcher:matcher_builders",
        "//common/text:symbol",
        "//common/text:syntax_tree_context",
        "//common/util:logging",
        "//verilog/CST:parameters",
        "//verilog/CST:verilog_matchers",  # fixdeps: keep
        "//verilog/analysis:descriptions",
        "//verilog/analysis:lint_rule_registry",
        "@com_google_absl//absl/strings",
    ],
    alwayslink = 1,
)

cc_test(
    name = "explicit_parameter_storage_type_rule_test",
    srcs = ["explicit_parameter_storage_type_rule_test.cc"],
    deps = [
        ":explicit_parameter_storage_type_rule",
        "//common/analysis:linter_test_utils",
        "//common/analysis:syntax_tree_linter_test_utils",
        "//common/text:symbol",
        "//verilog/CST:verilog_nonterminals",
        "//verilog/analysis:verilog_analyzer",
        "@com_google_googletest//:gtest_main",
    ],
)

cc_library(
    name = "explicit_function_task_parameter_type_rule",
    srcs = ["explicit_function_task_parameter_type_rule.cc"],
    hdrs = ["explicit_function_task_parameter_type_rule.h"],
    deps = [
        "//common/analysis:citation",
        "//common/analysis:lint_rule_status",
        "//common/analysis:syntax_tree_lint_rule",
        "//common/analysis/matcher",
        "//common/analysis/matcher:bound_symbol_manager",
        "//common/analysis/matcher:matcher_builders",
        "//common/text:concrete_syntax_leaf",
        "//common/text:symbol",
        "//common/text:syntax_tree_context",
        "//common/util:logging",
        "//verilog/CST:port",
        "//verilog/CST:type",
        "//verilog/CST:verilog_matchers",
        "//verilog/analysis:descriptions",
        "//verilog/analysis:lint_rule_registry",
        "@com_google_absl//absl/strings",
    ],
    alwayslink = 1,
)

cc_test(
    name = "explicit_function_task_parameter_type_rule_test",
    srcs = ["explicit_function_task_parameter_type_rule_test.cc"],
    deps = [
        ":explicit_function_task_parameter_type_rule",
        "//common/analysis:lint_rule_status",
        "//common/analysis:linter_test_utils",
        "//common/analysis:syntax_tree_lint_rule",
        "//common/analysis:syntax_tree_linter",
        "//common/analysis:syntax_tree_linter_test_utils",
        "//common/text:symbol",
        "//common/text:token_info",
        "//common/util:status",
        "//verilog/analysis:verilog_analyzer",
        "//verilog/parser:verilog_token_enum",
        "@com_google_absl//absl/memory",
        "@com_google_googletest//:gtest_main",
    ],
)

cc_library(
    name = "signal_name_style_rule",
    srcs = ["signal_name_style_rule.cc"],
    hdrs = ["signal_name_style_rule.h"],
    deps = [
        "//common/analysis:citation",
        "//common/analysis:lint_rule_status",
        "//common/analysis:syntax_tree_lint_rule",
        "//common/analysis/matcher",
        "//common/analysis/matcher:bound_symbol_manager",
        "//common/analysis/matcher:matcher_builders",
        "//common/strings:naming_utils",
        "//common/text:concrete_syntax_leaf",
        "//common/text:symbol",
        "//common/text:syntax_tree_context",
        "//common/text:token_info",
        "//common/util:logging",
        "//verilog/CST:data",
        "//verilog/CST:identifier",
        "//verilog/CST:net",
        "//verilog/CST:port",
        "//verilog/CST:verilog_matchers",
        "//verilog/analysis:descriptions",
        "//verilog/analysis:lint_rule_registry",
        "@com_google_absl//absl/strings",
    ],
    alwayslink = 1,
)

cc_test(
    name = "signal_name_style_rule_test",
    srcs = ["signal_name_style_rule_test.cc"],
    deps = [
        ":signal_name_style_rule",
        "//common/analysis:linter_test_utils",
        "//common/analysis:syntax_tree_linter_test_utils",
        "//common/text:symbol",
        "//verilog/CST:verilog_nonterminals",
        "//verilog/analysis:verilog_analyzer",
        "@com_google_googletest//:gtest_main",
    ],
)

cc_library(
    name = "struct_union_name_style_rule",
    srcs = ["struct_union_name_style_rule.cc"],
    hdrs = ["struct_union_name_style_rule.h"],
    deps = [
        "//common/analysis:citation",
        "//common/analysis:lint_rule_status",
        "//common/analysis:syntax_tree_lint_rule",
        "//common/analysis/matcher",
        "//common/analysis/matcher:bound_symbol_manager",
        "//common/analysis/matcher:matcher_builders",
        "//common/strings:naming_utils",
        "//common/text:symbol",
        "//common/text:syntax_tree_context",
        "//verilog/CST:type",
        "//verilog/CST:verilog_matchers",
        "//verilog/analysis:descriptions",
        "//verilog/analysis:lint_rule_registry",
        "@com_google_absl//absl/strings",
    ],
    alwayslink = 1,
)

cc_test(
    name = "struct_union_name_style_rule_test",
    srcs = ["struct_union_name_style_rule_test.cc"],
    deps = [
        ":struct_union_name_style_rule",
        "//common/analysis:linter_test_utils",
        "//common/analysis:syntax_tree_linter_test_utils",
        "//common/text:symbol",
        "//verilog/CST:verilog_nonterminals",
        "//verilog/analysis:verilog_analyzer",
        "@com_google_googletest//:gtest_main",
    ],
)
