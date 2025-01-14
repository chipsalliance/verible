# 'verible-verilog-kythe-extractor' is a program for extracting Verilog/SystemVerilog to kythe facts.

load("//bazel:sh_test_with_runfiles_lib.bzl", "sh_test_with_runfiles_lib")
load("//bazel:variables.bzl", "STATIC_EXECUTABLES_FEATURE")

package(
    default_applicable_licenses = ["//:license"],
    default_visibility = [
        "//visibility:private",
    ],
    features = ["layering_check"],
)

# Generates foreach list for IndexingFactType enum.
# Libraries that need these generated include files should list them
# under srcs, even if they are included by headers.
genrule(
    name = "verilog-extractor-indexing-fact-type-foreach-gen",
    srcs = ["verilog-extractor-indexing-fact-type.h"],
    outs = ["verilog_extractor_indexing_fact_type_foreach-gen.inc"],
    cmd = "sed -n -e '/BEGIN GENERATE/,/END GENERATE/p' $< | " +
          "grep -v GENERATE | " +
          "sed -e 's|^ *|CONSIDER(|' -e 's| =.*,|,|' -e 's|,|)|' > $@",
)

cc_library(
    name = "kythe-facts",
    srcs = ["kythe-facts.cc"],
    hdrs = ["kythe-facts.h"],
    deps = [
        "//verible/common/util:spacer",
        "@abseil-cpp//absl/hash",
        "@abseil-cpp//absl/log:check",
        "@abseil-cpp//absl/strings",
    ],
)

cc_test(
    name = "kythe-facts_test",
    srcs = ["kythe-facts_test.cc"],
    deps = [
        ":kythe-facts",
        "@googletest//:gtest",
        "@googletest//:gtest_main",
    ],
)

cc_library(
    name = "scope-resolver",
    srcs = ["scope-resolver.cc"],
    hdrs = ["scope-resolver.h"],
    deps = [
        ":kythe-facts",
        "//verible/common/util:logging",
        "@abseil-cpp//absl/container:flat_hash_map",
        "@abseil-cpp//absl/container:flat_hash_set",
        "@abseil-cpp//absl/container:node_hash_map",
        "@abseil-cpp//absl/strings",
    ],
)

cc_test(
    name = "scope-resolver_test",
    srcs = ["scope-resolver_test.cc"],
    deps = [
        ":kythe-facts",
        ":scope-resolver",
        "@googletest//:gtest",
        "@googletest//:gtest_main",
    ],
)

cc_library(
    name = "indexing-facts-tree-context",
    hdrs = ["indexing-facts-tree-context.h"],
    deps = [
        ":indexing-facts-tree",
        "//verible/common/util:auto-pop-stack",
        "//verible/common/util:logging",
    ],
)

cc_library(
    name = "kythe-schema-constants",
    hdrs = ["kythe-schema-constants.h"],
)

cc_library(
    name = "kythe-facts-extractor",
    srcs = ["kythe-facts-extractor.cc"],
    hdrs = ["kythe-facts-extractor.h"],
    deps = [
        ":indexing-facts-tree",
        ":kythe-facts",
        ":kythe-schema-constants",
        ":scope-resolver",
        ":verilog-extractor-indexing-fact-type",
        "//verible/common/util:auto-pop-stack",
        "//verible/common/util:logging",
        "//verible/common/util:tree-operations",
        "//verible/verilog/analysis:verilog-project",
        "@abseil-cpp//absl/container:flat_hash_set",
        "@abseil-cpp//absl/container:node_hash_set",
        "@abseil-cpp//absl/hash",
        "@abseil-cpp//absl/strings",
        "@abseil-cpp//absl/time",
    ],
)

cc_library(
    name = "verilog-extractor-indexing-fact-type",
    srcs = [
        "verilog-extractor-indexing-fact-type.cc",
        "verilog_extractor_indexing_fact_type_foreach-gen.inc",
    ],
    hdrs = [
        "verilog-extractor-indexing-fact-type.h",
        "verilog_extractor_indexing_fact_type_foreach.inc",
    ],
    deps = [
        "@abseil-cpp//absl/strings",
    ],
)

cc_library(
    name = "indexing-facts-tree",
    srcs = ["indexing-facts-tree.cc"],
    hdrs = ["indexing-facts-tree.h"],
    deps = [
        ":verilog-extractor-indexing-fact-type",
        "//verible/common/text:token-info",
        "//verible/common/util:tree-operations",
        "//verible/common/util:vector-tree",
        "@abseil-cpp//absl/strings",
    ],
)

cc_test(
    name = "indexing-facts-tree_test",
    srcs = ["indexing-facts-tree_test.cc"],
    deps = [
        ":indexing-facts-tree",
        ":verilog-extractor-indexing-fact-type",
        "//verible/common/util:range",
        "@googletest//:gtest",
        "@googletest//:gtest_main",
    ],
)

cc_library(
    name = "indexing-facts-tree-extractor",
    srcs = ["indexing-facts-tree-extractor.cc"],
    hdrs = ["indexing-facts-tree-extractor.h"],
    deps = [
        ":indexing-facts-tree",
        ":indexing-facts-tree-context",
        ":verilog-extractor-indexing-fact-type",
        "//verible/common/analysis:syntax-tree-search",
        "//verible/common/text:concrete-syntax-tree",
        "//verible/common/text:token-info",
        "//verible/common/text:tree-context-visitor",
        "//verible/common/text:tree-utils",
        "//verible/common/util:logging",
        "//verible/common/util:tree-operations",
        "//verible/verilog/CST:class",
        "//verible/verilog/CST:declaration",
        "//verible/verilog/CST:functions",
        "//verible/verilog/CST:identifier",
        "//verible/verilog/CST:macro",
        "//verible/verilog/CST:module",
        "//verible/verilog/CST:net",
        "//verible/verilog/CST:package",
        "//verible/verilog/CST:parameters",
        "//verible/verilog/CST:port",
        "//verible/verilog/CST:statement",
        "//verible/verilog/CST:tasks",
        "//verible/verilog/CST:type",
        "//verible/verilog/CST:verilog-matchers",
        "//verible/verilog/CST:verilog-nonterminals",
        "//verible/verilog/analysis:verilog-project",
        "//verible/verilog/parser:verilog-token-enum",
        "@abseil-cpp//absl/status",
        "@abseil-cpp//absl/strings",
    ],
)

cc_test(
    name = "indexing-facts-tree-extractor_test",
    srcs = ["indexing-facts-tree-extractor_test.cc"],
    deps = [
        ":indexing-facts-tree",
        ":indexing-facts-tree-extractor",
        ":verilog-extractor-indexing-fact-type",
        "//verible/common/analysis:syntax-tree-search-test-utils",
        "//verible/common/util:file-util",
        "//verible/common/util:logging",
        "//verible/common/util:tree-operations",
        "//verible/verilog/analysis:verilog-project",
        "@abseil-cpp//absl/status",
        "@abseil-cpp//absl/strings",
        "@googletest//:gtest",
        "@googletest//:gtest_main",
    ],
)

cc_library(
    name = "kythe-proto-output",
    srcs = ["kythe-proto-output.cc"],
    hdrs = ["kythe-proto-output.h"],
    deps = [
        ":kythe-facts",
        ":kythe-facts-extractor",
        "//third_party/proto/kythe:storage_cc_proto",
        "@protobuf//src/google/protobuf/io",
    ],
)

cc_binary(
    name = "verible-verilog-kythe-extractor",
    srcs = [
        "verilog-kythe-extractor.cc",
    ],
    features = STATIC_EXECUTABLES_FEATURE,
    visibility = ["//visibility:public"],
    deps = [
        ":indexing-facts-tree",
        ":indexing-facts-tree-extractor",
        ":kythe-facts",
        ":kythe-facts-extractor",
        ":kythe-proto-output",
        "//verible/common/util:enum-flags",
        "//verible/common/util:init-command-line",
        "//verible/common/util:logging",
        "//verible/common/util:tree-operations",
        "//verible/verilog/analysis:verilog-filelist",
        "//verible/verilog/analysis:verilog-project",
        "//verible/verilog/analysis/checkers:verilog-lint-rules",
        "@abseil-cpp//absl/flags:flag",
        "@abseil-cpp//absl/status",
        "@abseil-cpp//absl/strings",
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
    srcs = ["kzip-creator.cc"],
    hdrs = ["kzip-creator.h"],
    deps = [
        "//third_party/proto/kythe:analysis_cc_proto",
        "//verible/common/util:file-util",
        "//verible/common/util:sha256",
        "//verible/common/util:simple-zip",
        "@abseil-cpp//absl/status",
    ],
)

cc_binary(
    name = "verible-verilog-kythe-kzip-writer",
    srcs = [
        "verilog-kythe-kzip-writer.cc",
    ],
    features = STATIC_EXECUTABLES_FEATURE,
    visibility = ["//visibility:public"],
    deps = [
        ":kzip-creator",
        "//third_party/proto/kythe:analysis_cc_proto",
        "//verible/common/util:file-util",
        "//verible/common/util:init-command-line",
        "//verible/common/util:logging",
        "//verible/verilog/analysis:verilog-filelist",
        "@abseil-cpp//absl/flags:flag",
        "@abseil-cpp//absl/status:statusor",
        "@abseil-cpp//absl/strings",
    ],
)
