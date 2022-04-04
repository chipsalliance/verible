# 'verible-verilog-kythe-extractor' is a program for extracting Verilog/SystemVerilog to kythe facts.

load("//bazel:sh_test_with_runfiles_lib.bzl", "sh_test_with_runfiles_lib")

licenses(["notice"])

package(
    default_visibility = [
        "//visibility:private",
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
    srcs = ["kythe_facts.cc"],
    hdrs = ["kythe_facts.h"],
    deps = [
        "//common/util:spacer",
        "@com_google_absl//absl/strings",
    ],
)

cc_test(
    name = "kythe_facts_test",
    srcs = ["kythe_facts_test.cc"],
    deps = [
        ":kythe_facts",
        "@com_google_googletest//:gtest_main",
    ],
)

cc_library(
    name = "scope_resolver",
    srcs = ["scope_resolver.cc"],
    hdrs = ["scope_resolver.h"],
    deps = [
        ":kythe_facts",
        "//common/util:auto_pop_stack",
        "//common/util:iterator_range",
        "@com_google_absl//absl/container:node_hash_map",
        "@com_google_absl//absl/strings",
    ],
)

cc_test(
    name = "scope_resolver_test",
    srcs = ["scope_resolver_test.cc"],
    deps = [
        ":kythe_facts",
        ":scope_resolver",
        "@com_google_googletest//:gtest_main",
    ],
)

cc_library(
    name = "indexing_facts_tree_context",
    hdrs = ["indexing_facts_tree_context.h"],
    deps = [
        ":indexing_facts_tree",
        "//common/util:auto_pop_stack",
    ],
)

cc_library(
    name = "kythe_schema_constants",
    hdrs = ["kythe_schema_constants.h"],
    deps = ["@com_google_absl//absl/strings"],
)

cc_library(
    name = "kythe_facts_extractor",
    srcs = ["kythe_facts_extractor.cc"],
    hdrs = ["kythe_facts_extractor.h"],
    deps = [
        ":indexing_facts_tree",
        ":kythe_facts",
        ":kythe_schema_constants",
        ":scope_resolver",
        ":verilog_extractor_indexing_fact_type",
        "//common/strings:compare",
        "//common/util:logging",
        "//common/util:tree_operations",
        "//verilog/analysis:verilog_project",
        "@com_google_absl//absl/container:btree",
        "@com_google_absl//absl/container:flat_hash_set",
        "@com_google_absl//absl/container:node_hash_set",
        "@com_google_absl//absl/memory",
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
    hdrs = ["verilog_extractor_indexing_fact_type.h"],
    deps = [
        "@com_google_absl//absl/strings",
    ],
)

cc_library(
    name = "indexing_facts_tree",
    srcs = ["indexing_facts_tree.cc"],
    hdrs = ["indexing_facts_tree.h"],
    deps = [
        ":verilog_extractor_indexing_fact_type",
        "//common/text:token_info",
        "//common/util:range",
        "//common/util:tree_operations",
        "//common/util:vector_tree",
        "@com_google_absl//absl/memory",
        "@com_google_absl//absl/strings",
        "@com_google_absl//absl/strings:str_format",
    ],
)

cc_test(
    name = "indexing_facts_tree_test",
    srcs = ["indexing_facts_tree_test.cc"],
    deps = [
        ":indexing_facts_tree",
        ":verilog_extractor_indexing_fact_type",
        "//common/text:token_info",
        "//common/util:range",
        "@com_google_absl//absl/memory",
        "@com_google_absl//absl/strings",
        "@com_google_googletest//:gtest_main",
    ],
)

cc_library(
    name = "indexing_facts_tree_extractor",
    srcs = ["indexing_facts_tree_extractor.cc"],
    hdrs = ["indexing_facts_tree_extractor.h"],
    deps = [
        ":indexing_facts_tree",
        ":indexing_facts_tree_context",
        "//common/text:concrete_syntax_tree",
        "//common/text:tree_context_visitor",
        "//common/text:tree_utils",
        "//common/util:file_util",
        "//common/util:logging",
        "//common/util:tree_operations",
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
        "//verilog/CST:statement",
        "//verilog/CST:tasks",
        "//verilog/CST:type",
        "//verilog/CST:verilog_matchers",
        "//verilog/CST:verilog_nonterminals",
        "//verilog/CST:verilog_tree_print",
        "//verilog/analysis:verilog_analyzer",
        "//verilog/analysis:verilog_project",
        "@com_google_absl//absl/status",
        "@com_google_absl//absl/strings",
    ],
)

cc_test(
    name = "indexing_facts_tree_extractor_test",
    srcs = ["indexing_facts_tree_extractor_test.cc"],
    deps = [
        ":indexing_facts_tree",
        ":indexing_facts_tree_extractor",
        ":verilog_extractor_indexing_fact_type",
        "//common/analysis:syntax_tree_search_test_utils",
        "//common/text:concrete_syntax_tree",
        "//common/util:file_util",
        "//common/util:range",
        "//common/util:tree_operations",
        "//verilog/analysis:verilog_analyzer",
        "//verilog/analysis:verilog_project",
        "@com_google_absl//absl/status",
        "@com_google_googletest//:gtest_main",
    ],
)

cc_library(
    name = "kythe_proto_output",
    srcs = ["kythe_proto_output.cc"],
    hdrs = ["kythe_proto_output.h"],
    deps = [
        ":kythe_facts",
        ":kythe_facts_extractor",
        "//third_party/proto/kythe:storage_cc_proto",
        "@com_google_protobuf//:protobuf",
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
        ":kythe_proto_output",
        "//common/util:bijective_map",
        "//common/util:enum_flags",
        "//common/util:file_util",
        "//common/util:init_command_line",
        "//common/util:tree_operations",
        "//verilog/analysis:verilog_analyzer",
        "//verilog/analysis:verilog_project",
        "//verilog/analysis/checkers:verilog_lint_rules",
        "//verilog/parser:verilog_parser",
        "@com_google_absl//absl/flags:flag",
        "@com_google_absl//absl/status",
        "@com_google_absl//absl/strings",
        "@com_google_absl//absl/types:span",
    ],
)

sh_test_with_runfiles_lib(
    name = "verilog_kythe_extractor_test",
    size = "small",
    srcs = ["verilog_kythe_extractor_test.sh"],
    args = ["$(location :verible-verilog-kythe-extractor)"],
    data = [":verible-verilog-kythe-extractor"],
    deps = [],
)

sh_test(
    name = "verification_test",
    srcs = ["verification_test.sh"],
    data = glob([
        "testdata/**",
    ]) + [
        ":verible-verilog-kythe-extractor",
    ],
    tags = ["manual"],
    deps = [
        "@bazel_tools//tools/bash/runfiles",
    ],
)
