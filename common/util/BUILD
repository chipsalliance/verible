# 'util' contains generic containers, adapters, algorithms

licenses(["notice"])

package(
    default_visibility = [
        "//:__subpackages__",
    ],
)

cc_library(
    name = "algorithm",
    hdrs = ["algorithm.h"],
)

cc_library(
    name = "auto_pop_stack",
    hdrs = ["auto_pop_stack.h"],
    deps = [
        ":iterator_adaptors",
        ":logging",
    ],
)

cc_library(
    name = "forward",
    hdrs = ["forward.h"],
)

cc_library(
    name = "range",
    hdrs = ["range.h"],
)

cc_library(
    name = "status_macros",
    hdrs = ["status_macros.h"],
    deps = ["@com_google_absl//absl/base:core_headers"],
)

cc_library(
    name = "file_util",
    srcs = ["file_util.cc"],
    hdrs = ["file_util.h"],
    deps = [
        ":logging",
        "@com_google_absl//absl/status",
        "@com_google_absl//absl/strings",
    ],
)

# Due to the hermetic builds bazel offers, we can't just fill a header file
# with a git hash. We have to use the workspace stamping features bazel
# offers.
# https://docs.bazel.build/versions/master/user-manual.html#workspace_status
# to contain a proper git timestamp, bazel has to be invoked with
#  bazel build --workspace_status_command=bazel/build-version.sh
genrule(
    name = "version_header",
    outs = ["verible_build_version.h"],
    cmd = "./$(location create_version_header.sh) > $@",
    stamp = 1,  # Tell bazel that we really are interested in the status file
    tools = ["create_version_header.sh"],
)

cc_library(
    name = "init_command_line",
    srcs = [
        "init_command_line.cc",
        "verible_build_version.h",
    ],
    hdrs = ["init_command_line.h"],
    deps = [
        "@com_google_absl//absl/flags:flag",
        "@com_google_absl//absl/flags:parse",
        "@com_google_absl//absl/flags:usage",
        "@com_google_absl//absl/strings",
        "@com_google_absl//absl/time",
    ],
)

cc_library(
    name = "interval",
    hdrs = ["interval.h"],
    deps = [
        ":forward",
        "@com_google_absl//absl/strings",
    ],
)

cc_library(
    name = "interval_set",
    hdrs = ["interval_set.h"],
    deps = [
        ":auto_iterator",
        ":interval",
        ":iterator_range",
        ":logging",
        "@com_google_absl//absl/random",
        "@com_google_absl//absl/strings",
    ],
)

cc_library(
    name = "logging",
    srcs = ["logging.cc"],
    hdrs = ["logging.h"],
    deps = [
        "@com_google_absl//absl/base",
        "@com_google_absl//absl/base:core_headers",
        "@com_google_absl//absl/base:log_severity",
        "@com_google_absl//absl/strings",
    ],
)

cc_library(
    name = "casts",
    hdrs = ["casts.h"],
)

cc_library(
    name = "container_util",
    hdrs = ["container_util.h"],
    deps = [
        ":logging",
    ],
)

cc_library(
    name = "container_iterator_range",
    hdrs = ["container_iterator_range.h"],
    deps = [
        ":iterator_range",
        ":range",
    ],
)

cc_library(
    name = "auto_iterator",
    hdrs = ["auto_iterator.h"],
)

cc_library(
    name = "iterator_adaptors",
    hdrs = ["iterator_adaptors.h"],
    deps = [
        ":auto_iterator",
        ":iterator_range",
    ],
)

cc_library(
    name = "iterator_range",
    hdrs = ["iterator_range.h"],
    deps = [],
)

cc_library(
    name = "spacer",
    srcs = ["spacer.cc"],
    hdrs = ["spacer.h"],
)

cc_library(
    name = "top_n",
    hdrs = ["top_n.h"],
)

cc_library(
    name = "value_saver",
    hdrs = ["value_saver.h"],
)

cc_library(
    name = "enum_flags",
    hdrs = ["enum_flags.h"],
    deps = [
        ":logging",
        "//common/strings:compare",
        "@com_google_absl//absl/strings",
    ],
)

cc_library(
    name = "enum_flags_test_util",
    testonly = 1,
    hdrs = ["enum_flags_test_util.h"],
    deps = [
        "@com_google_absl//absl/strings",
        "@com_google_googletest//:gtest",  # for library testonly
    ],
)

cc_library(
    name = "bijective_map",
    hdrs = ["bijective_map.h"],
    deps = [":forward"],
)

cc_library(
    name = "vector_tree",
    srcs = ["vector_tree.cc"],
    hdrs = ["vector_tree.h"],
    deps = [
        ":iterator_range",
        ":logging",
        ":spacer",
        "@com_google_absl//absl/strings",
    ],
)

cc_library(
    name = "expandable_tree_view",
    hdrs = ["expandable_tree_view.h"],
    deps = [
        ":logging",
        ":vector_tree",
    ],
)

cc_library(
    name = "vector_tree_test_util",
    testonly = 1,
    srcs = ["vector_tree_test_util.cc"],
    hdrs = ["vector_tree_test_util.h"],
    deps = [
        ":vector_tree",
        "@com_google_absl//absl/strings",
        "@com_google_googletest//:gtest",  # for library testonly
    ],
)

cc_library(
    name = "with_reason",
    hdrs = ["with_reason.h"],
)

cc_test(
    name = "algorithm_test",
    srcs = ["algorithm_test.cc"],
    deps = [
        ":algorithm",
        ":iterator_range",
        "@com_google_absl//absl/strings",
        "@com_google_googletest//:gtest_main",
    ],
)

cc_test(
    name = "auto_pop_stack_test",
    srcs = ["auto_pop_stack_test.cc"],
    deps = [
        ":auto_pop_stack",
        ":iterator_range",
        "@com_google_googletest//:gtest_main",
    ],
)

cc_test(
    name = "file_util_test",
    srcs = ["file_util_test.cc"],
    deps = [
        ":file_util",
        "@com_google_absl//absl/strings",
        "@com_google_googletest//:gtest_main",
    ],
)

cc_test(
    name = "interval_test",
    srcs = ["interval_test.cc"],
    deps = [
        ":interval",
        "@com_google_googletest//:gtest_main",
    ],
)

cc_test(
    name = "interval_set_test",
    srcs = ["interval_set_test.cc"],
    deps = [
        ":interval_set",
        "@com_google_googletest//:gtest_main",
    ],
)

cc_test(
    name = "forward_test",
    srcs = ["forward_test.cc"],
    deps = [
        ":forward",
        "@com_google_absl//absl/strings",
        "@com_google_googletest//:gtest_main",
    ],
)

cc_test(
    name = "range_test",
    srcs = ["range_test.cc"],
    deps = [
        ":range",
        "@com_google_absl//absl/strings",
        "@com_google_googletest//:gtest_main",
    ],
)

cc_test(
    name = "container_iterator_range_test",
    srcs = ["container_iterator_range_test.cc"],
    deps = [
        ":container_iterator_range",
        "@com_google_googletest//:gtest_main",
    ],
)

cc_test(
    name = "iterator_adaptors_test",
    srcs = ["iterator_adaptors_test.cc"],
    deps = [
        ":iterator_adaptors",
        "@com_google_googletest//:gtest_main",
    ],
)

cc_test(
    name = "iterator_range_test",
    srcs = ["iterator_range_test.cc"],
    deps = [
        ":iterator_range",
        ":range",
        "@com_google_googletest//:gtest_main",
    ],
)

cc_test(
    name = "spacer_test",
    srcs = ["spacer_test.cc"],
    deps = [
        ":spacer",
        "@com_google_googletest//:gtest_main",
    ],
)

cc_test(
    name = "top_n_test",
    srcs = ["top_n_test.cc"],
    deps = [
        ":top_n",
        "@com_google_googletest//:gtest_main",
    ],
)

cc_test(
    name = "value_saver_test",
    srcs = ["value_saver_test.cc"],
    deps = [
        ":value_saver",
        "@com_google_googletest//:gtest_main",
    ],
)

cc_test(
    name = "enum_flags_test",
    srcs = ["enum_flags_test.cc"],
    deps = [
        ":enum_flags",
        ":enum_flags_test_util",
        "@com_google_absl//absl/strings",
        "@com_google_googletest//:gtest_main",
    ],
)

cc_test(
    name = "bijective_map_test",
    srcs = ["bijective_map_test.cc"],
    deps = [
        ":bijective_map",
        ":logging",
        "//common/strings:compare",
        "@com_google_googletest//:gtest_main",
    ],
)

cc_test(
    name = "vector_tree_test",
    srcs = ["vector_tree_test.cc"],
    deps = [
        ":vector_tree",
        ":vector_tree_test_util",
        "@com_google_absl//absl/strings",
        "@com_google_googletest//:gtest_main",
    ],
)

cc_test(
    name = "expandable_tree_view_test",
    srcs = ["expandable_tree_view_test.cc"],
    deps = [
        ":expandable_tree_view",
        ":vector_tree",
        ":vector_tree_test_util",
        "@com_google_absl//absl/strings",
        "@com_google_googletest//:gtest_main",
    ],
)

cc_test(
    name = "with_reason_test",
    srcs = ["with_reason_test.cc"],
    deps = [
        ":with_reason",
        "@com_google_googletest//:gtest_main",
    ],
)
