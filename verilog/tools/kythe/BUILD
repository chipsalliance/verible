# 'verible-verilog-kythe-extractor' is a program for extracting Verilog/SystemVerilog to kythe facts.

licenses(["notice"])

package(
    default_visibility = [
        "//verilog/tools/kythe:__subpackages__",
    ],
)

# Generates foreach list for IndexingFactType enum.
# Libraries that need these generated include files should list them
# under srcs, even if they are included by headers.
genrule(
    name = "verilog_extractor_indexing_fact_type_foreach_gen",
    srcs = ["verilog_extractor_indexing_fact_type.h"],
    outs = ["verilog_extractor_indexing_fact_type_foreach-gen.inc"],
    cmd = "sed -n -e '/BEGIN GENERATE/,/END GENERATE/p' $< | " +
          "grep -v GENERATE | " +
          "sed -e 's|^ *|CONSIDER(|' -e 's| =.*,|,|' -e 's|,|)|' > $@",
)

cc_library(
    name = "kythe_facts",
    srcs = [
        "kythe_facts.cc",
    ],
    hdrs = [
        "kythe_facts.h",
    ],
    deps = [
        "@com_google_absl//absl/strings",
    ],
)

cc_library(
    name = "scope_resolver",
    srcs = [
        "scope_resolver.cc",
    ],
    hdrs = [
        "scope_resolver.h",
    ],
    deps = [
        ":kythe_facts",
        "@com_google_absl//absl/strings",
        "//common/util:auto_pop_stack",
        "//common/util:iterator_range",
    ]
)

cc_library(
    name = "indexing_facts_tree_context",
    hdrs = [
        "indexing_facts_tree_context.h",
    ],
    deps = [
        ":indexing_facts_tree",
        "//common/util:auto_pop_stack",
    ],
)

cc_library(
    name = "kythe_schema_constants",
    hdrs = [
        "kythe_schema_constants.h",
    ],
    deps = ["@com_google_absl//absl/strings"],
)

cc_library(
    name = "kythe_facts_extractor",
    srcs = [
        "kythe_facts_extractor.cc",
    ],
    hdrs = [
        "kythe_facts_extractor.h",
    ],
    deps = [
        ":scope_resolver",
        ":indexing_facts_tree",
        ":indexing_facts_tree_context",
        ":indexing_facts_tree_extractor",
        ":kythe_facts",
        ":kythe_schema_constants",
        "@com_google_absl//absl/strings",
    ],
)

cc_library(
    name = "verilog_extractor_indexing_fact_type",
    srcs = [
        "verilog_extractor_indexing_fact_type.cc",
        "verilog_extractor_indexing_fact_type_foreach.inc",
        "verilog_extractor_indexing_fact_type_foreach-gen.inc",
    ],
    hdrs = [
        "verilog_extractor_indexing_fact_type.h",
    ],
    deps = [
        "@com_google_absl//absl/strings",
    ],
)

cc_library(
    name = "indexing_facts_tree",
    srcs = [
        "indexing_facts_tree.cc",
    ],
    hdrs = [
        "indexing_facts_tree.h",
    ],
    deps = [
        ":verilog_extractor_indexing_fact_type",
        "//common/text:token_info",
        "//common/util:vector_tree",
        "@com_google_absl//absl/strings",
    ],
)

cc_library(
    name = "indexing_facts_tree_extractor",
    srcs = [
        "indexing_facts_tree_extractor.cc",
    ],
    hdrs = [
        "indexing_facts_tree_extractor.h",
    ],
    deps = [
        ":indexing_facts_tree",
        ":indexing_facts_tree_context",
        "//common/analysis:syntax_tree_search",
        "//common/text:tree_context_visitor",
        "//common/text:tree_utils",
        "//verilog/CST:class",
        "//verilog/CST:declaration",
        "//verilog/CST:functions",
        "//verilog/CST:identifier",
        "//verilog/CST:macro",
        "//verilog/CST:module",
        "//verilog/CST:net",
        "//verilog/CST:package",
        "//verilog/CST:parameters",
        "//verilog/CST:port",
        "//verilog/CST:tasks",
        "//verilog/CST:verilog_matchers",
        "//verilog/CST:verilog_nonterminals",
        "//verilog/analysis:verilog_analyzer",
        "@com_google_absl//absl/strings",
    ],
)

cc_test(
    name = "indexing_facts_tree_extractor_test",
    srcs = ["indexing_facts_tree_extractor_test.cc"],
    deps = [
        ":indexing_facts_tree_extractor",
        "//common/analysis:syntax_tree_search_test_utils",
        "//common/text:concrete_syntax_tree",
        "//verilog/analysis:verilog_analyzer",
        "@com_google_googletest//:gtest_main",
    ],
)

cc_binary(
    name = "verible-verilog-kythe-extractor",
    srcs = [
        "verilog_extractor_indexing_fact_type_foreach.inc",
        "verilog_extractor_indexing_fact_type_foreach-gen.inc",
        "verilog_kythe_extractor.cc",
    ],
    visibility = ["//visibility:public"],
    deps = [
        ":indexing_facts_tree_extractor",
        ":kythe_facts_extractor",
        ":verilog_extractor_indexing_fact_type",
        "//common/text:concrete_syntax_tree",
        "//common/text:parser_verifier",
        "//common/text:symbol",
        "//common/text:text_structure",
        "//common/text:token_info",
        "//common/util:file_util",
        "//common/util:init_command_line",
        "//common/util:logging",
        "//verilog/CST:verilog_nonterminals",
        "//verilog/CST:verilog_tree_print",
        "//verilog/analysis:verilog_analyzer",
        "//verilog/analysis/checkers:verilog_lint_rules",
        "//verilog/parser:verilog_parser",
        "@com_google_absl//absl/flags:flag",
        "@com_google_absl//absl/status",
        "@com_google_absl//absl/strings",
        "@com_google_absl//absl/types:span",
    ],
)
