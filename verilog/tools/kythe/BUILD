# 'verible-verilog-kythe-extractor' is a program for extracting Verilog/SystemVerilog to kythe facts.

load("//bazel:sh_test_with_runfiles_lib.bzl", "sh_test_with_runfiles_lib")
load("//bazel:variables.bzl", "STATIC_EXECUTABLES_FEATURE")

package(
    default_applicable_licenses = ["//:license"],
    default_visibility = [
        "//visibility:private",
    ],
)

# Generates foreach list for IndexingFactType enum.
# Libraries that need these generated include files should list them
# under srcs, even if they are included by headers.
genrule(
    name = "verilog-extractor-indexing-fact-type-foreach-gen",
    srcs = ["verilog_extractor_indexing_fact_type.h"],
    outs = ["verilog_extractor_indexing_fact_type_foreach-gen.inc"],
    cmd = "sed -n -e '/BEGIN GENERATE/,/END GENERATE/p' $< | " +
          "grep -v GENERATE | " +
          "sed -e 's|^ *|CONSIDER(|' -e 's| =.*,|,|' -e 's|,|)|' > $@",
)

cc_library(
    name = "kythe-facts",
    srcs = ["kythe_facts.cc"],
    hdrs = ["kythe_facts.h"],
    deps = [
        "//common/util:spacer",
        "@com_google_absl//absl/hash",
        "@com_google_absl//absl/log:check",
        "@com_google_absl//absl/strings",
    ],
)

cc_test(
    name = "kythe-facts_test",
    srcs = ["kythe_facts_test.cc"],
    deps = [
        ":kythe-facts",
        "@com_google_googletest//:gtest_main",
    ],
)

cc_library(
    name = "scope-resolver",
    srcs = ["scope_resolver.cc"],
    hdrs = ["scope_resolver.h"],
    deps = [
        ":kythe-facts",
        "//common/util:logging",
        "@com_google_absl//absl/container:flat_hash_map",
        "@com_google_absl//absl/container:flat_hash_set",
        "@com_google_absl//absl/container:node_hash_map",
        "@com_google_absl//absl/strings",
    ],
)

cc_test(
    name = "scope-resolver_test",
    srcs = ["scope_resolver_test.cc"],
    deps = [
        ":kythe-facts",
        ":scope-resolver",
        "@com_google_absl//absl/strings",
        "@com_google_googletest//:gtest_main",
    ],
)

cc_library(
    name = "indexing-facts-tree-context",
    hdrs = ["indexing_facts_tree_context.h"],
    deps = [
        ":indexing-facts-tree",
        "//common/util:auto-pop-stack",
    ],
)

cc_library(
    name = "kythe-schema-constants",
    hdrs = ["kythe_schema_constants.h"],
    deps = ["@com_google_absl//absl/strings"],
)

cc_library(
    name = "kythe-facts-extractor",
    srcs = ["kythe_facts_extractor.cc"],
    hdrs = ["kythe_facts_extractor.h"],
    deps = [
        ":indexing-facts-tree",
        ":kythe-facts",
        ":kythe-schema-constants",
        ":scope-resolver",
        ":verilog-extractor-indexing-fact-type",
        "//common/util:auto-pop-stack",
        "//common/util:logging",
        "//common/util:tree-operations",
        "//verilog/analysis:verilog-project",
        "@com_google_absl//absl/container:btree",
        "@com_google_absl//absl/container:flat_hash_set",
        "@com_google_absl//absl/container:node_hash_set",
        "@com_google_absl//absl/hash",
        "@com_google_absl//absl/strings",
        "@com_google_absl//absl/time",
    ],
)

cc_library(
    name = "verilog-extractor-indexing-fact-type",
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
    name = "indexing-facts-tree",
    srcs = ["indexing_facts_tree.cc"],
    hdrs = ["indexing_facts_tree.h"],
    deps = [
        ":verilog-extractor-indexing-fact-type",
        "//common/text:token-info",
        "//common/util:range",
        "//common/util:tree-operations",
        "//common/util:vector-tree",
        "@com_google_absl//absl/memory",
        "@com_google_absl//absl/strings",
        "@com_google_absl//absl/strings:str_format",
    ],
)

cc_test(
    name = "indexing-facts-tree_test",
    srcs = ["indexing_facts_tree_test.cc"],
    deps = [
        ":indexing-facts-tree",
        ":verilog-extractor-indexing-fact-type",
        "//common/text:token-info",
        "//common/util:range",
        "@com_google_absl//absl/memory",
        "@com_google_absl//absl/strings",
        "@com_google_googletest//:gtest_main",
    ],
)

cc_library(
    name = "indexing-facts-tree-extractor",
    srcs = ["indexing_facts_tree_extractor.cc"],
    hdrs = ["indexing_facts_tree_extractor.h"],
    deps = [
        ":indexing-facts-tree",
        ":indexing-facts-tree-context",
        "//common/text:concrete-syntax-tree",
        "//common/text:tree-context-visitor",
        "//common/text:tree-utils",
        "//common/util:file-util",
        "//common/util:logging",
        "//common/util:tree-operations",
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
        "//verilog/CST:verilog-matchers",
        "//verilog/CST:verilog-nonterminals",
        "//verilog/CST:verilog-tree-print",
        "//verilog/analysis:verilog-analyzer",
        "//verilog/analysis:verilog-project",
        "@com_google_absl//absl/status",
        "@com_google_absl//absl/strings",
    ],
)

cc_test(
    name = "indexing-facts-tree-extractor_test",
    srcs = ["indexing_facts_tree_extractor_test.cc"],
    deps = [
        ":indexing-facts-tree",
        ":indexing-facts-tree-extractor",
        ":verilog-extractor-indexing-fact-type",
        "//common/analysis:syntax-tree-search-test-utils",
        "//common/text:concrete-syntax-tree",
        "//common/util:file-util",
        "//common/util:logging",
        "//common/util:range",
        "//common/util:tree-operations",
        "//verilog/analysis:verilog-analyzer",
        "//verilog/analysis:verilog-project",
        "@com_google_absl//absl/status",
        "@com_google_googletest//:gtest_main",
    ],
)

cc_library(
    name = "kythe-proto-output",
    srcs = ["kythe_proto_output.cc"],
    hdrs = ["kythe_proto_output.h"],
    deps = [
        ":kythe-facts",
        ":kythe-facts-extractor",
        "//third_party/proto/kythe:storage_cc_proto",
        "@com_google_protobuf//src/google/protobuf/io",
    ],
)

cc_binary(
    name = "verible-verilog-kythe-extractor",
    srcs = [
        "verilog_extractor_indexing_fact_type_foreach.inc",
        "verilog_extractor_indexing_fact_type_foreach-gen.inc",
        "verilog_kythe_extractor.cc",
    ],
    features = STATIC_EXECUTABLES_FEATURE,
    visibility = ["//visibility:public"],
    deps = [
        ":indexing-facts-tree-extractor",
        ":kythe-facts-extractor",
        ":kythe-proto-output",
        "//common/util:bijective-map",
        "//common/util:enum-flags",
        "//common/util:file-util",
        "//common/util:init-command-line",
        "//common/util:tree-operations",
        "//verilog/analysis:verilog-analyzer",
        "//verilog/analysis:verilog-project",
        "//verilog/analysis/checkers:verilog-lint-rules",
        "//verilog/parser:verilog-parser",
        "@com_google_absl//absl/flags:flag",
        "@com_google_absl//absl/status",
        "@com_google_absl//absl/strings",
        "@com_google_absl//absl/types:span",
    ],
)

sh_test_with_runfiles_lib(
    name = "verilog-kythe-extractor_test",
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

cc_library(
    name = "kzip-creator",
    srcs = ["kzip_creator.cc"],
    hdrs = ["kzip_creator.h"],
    deps = [
        "//common/util:file-util",
        "//common/util:simple-zip",
        "//third_party/proto/kythe:analysis_cc_proto",
        "@com_google_absl//absl/status",
        "@com_google_absl//absl/strings",
        "@com_google_boringssl//:crypto",
    ],
)

cc_binary(
    name = "verible-verilog-kythe-kzip-writer",
    srcs = [
        "verilog_kythe_kzip_writer.cc",
    ],
    features = STATIC_EXECUTABLES_FEATURE,
    visibility = ["//visibility:public"],
    deps = [
        ":kzip-creator",
        "//common/util:file-util",
        "//common/util:init-command-line",
        "//common/util:logging",
        "//third_party/proto/kythe:analysis_cc_proto",
        "//verilog/analysis:verilog-filelist",
        "@com_google_absl//absl/flags:flag",
        "@com_google_absl//absl/strings",
    ],
)
