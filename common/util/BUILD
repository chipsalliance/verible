# 'util' contains generic containers, adapters, algorithms
# Any verible package may directly depend on this.

package(
    default_applicable_licenses = ["//:license"],
    default_visibility = [
        "//:__subpackages__",
    ],
    features = ["layering_check"],
)

cc_library(
    name = "algorithm",
    hdrs = ["algorithm.h"],
)

cc_library(
    name = "auto-pop-stack",
    hdrs = ["auto_pop_stack.h"],
    deps = [
        ":logging",
        "@com_google_absl//absl/base:core_headers",
    ],
)

cc_library(
    name = "forward",
    hdrs = ["forward.h"],
)

cc_library(
    name = "range",
    hdrs = ["range.h"],
    deps = [":logging"],
)

cc_library(
    name = "status-macros",
    hdrs = ["status_macros.h"],
    deps = ["@com_google_absl//absl/base:core_headers"],
)

cc_library(
    name = "subcommand",
    srcs = ["subcommand.cc"],
    hdrs = ["subcommand.h"],
    deps = [
        ":container-iterator-range",
        "//common/strings:compare",
        "@com_google_absl//absl/status",
        "@com_google_absl//absl/strings",
    ],
)

cc_library(
    name = "thread-pool",
    srcs = ["thread_pool.cc"],
    hdrs = ["thread_pool.h"],
    copts = select({
        "@platforms//os:windows": [],
        "//conditions:default": ["-fexceptions"],
    }),
    features = ["-use_header_modules"],  # precompiled headers incompatible with -fexceptions.
)

cc_library(
    name = "file-util",
    srcs = ["file_util.cc"],
    hdrs = ["file_util.h"],
    deps = [
        ":logging",
        "//common/strings:mem-block",
        "@com_google_absl//absl/status",
        "@com_google_absl//absl/status:statusor",
        "@com_google_absl//absl/strings",
    ],
)

# Due to the hermetic builds bazel offers, we can't just fill a header file
# with a git hash. We have to use the workspace stamping features bazel
# offers.
# https://docs.bazel.build/versions/master/user-manual.html#workspace_status
# to contain a proper git timestamp, bazel has to be invoked with
#  bazel build --workspace_status_command=bazel/build-version.py
genrule(
    name = "version-header",
    outs = ["generated_verible_build_version.h"],
    cmd = "sed -e 's/^/#define VERIBLE_/' bazel-out/volatile-status.txt > $@",
    stamp = 1,  # Tell bazel that we really are interested in the status file
    visibility = ["//visibility:private"],
)

cc_library(
    name = "build-version",
    hdrs = ["generated_verible_build_version.h"],
    visibility = ["//visibility:private"],
)

cc_library(
    name = "init-command-line",
    srcs = ["init_command_line.cc"],
    hdrs = ["init_command_line.h"],
    deps = [
        # these deps are needed by init_command_line.cc:
        ":build-version",
        "@com_google_absl//absl/debugging:failure_signal_handler",
        "@com_google_absl//absl/debugging:symbolize",
        "@com_google_absl//absl/flags:config",
        "@com_google_absl//absl/flags:parse",
        "@com_google_absl//absl/flags:usage",
        "@com_google_absl//absl/log:globals",
        "@com_google_absl//absl/base:log_severity",
        "@com_google_absl//absl/log:initialize",
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
    name = "interval-map",
    hdrs = ["interval_map.h"],
    deps = [
        ":auto-iterator",
        ":logging",
    ],
)

cc_library(
    name = "interval-set",
    hdrs = ["interval_set.h"],
    deps = [
        ":auto-iterator",
        ":interval",
        ":iterator-range",
        ":logging",
        "@com_google_absl//absl/random",
        "@com_google_absl//absl/strings",
    ],
)

# TODO: once all absl logging features are established in abseil-cpp, we
# should IWYU them directly in places where we need logging and remove
# this common/util:logging target.
cc_library(
    name = "logging",
    hdrs = ["logging.h"],
    deps = [
        "@com_google_absl//absl/log",
        "@com_google_absl//absl/log:check",
        "@com_google_absl//absl/log:die_if_null",
    ],
)

cc_library(
    name = "casts",
    hdrs = ["casts.h"],
)

cc_library(
    name = "container-util",
    hdrs = ["container_util.h"],
    deps = [
        ":logging",
    ],
)

cc_library(
    name = "container-iterator-range",
    hdrs = ["container_iterator_range.h"],
    deps = [
        ":iterator-range",
        ":range",
    ],
)

cc_library(
    name = "type-traits",
    hdrs = ["type_traits.h"],
)

cc_library(
    name = "auto-iterator",
    hdrs = ["auto_iterator.h"],
)

cc_library(
    name = "iterator-adaptors",
    hdrs = ["iterator_adaptors.h"],
    deps = [
        ":auto-iterator",
        ":iterator-range",
    ],
)

cc_library(
    name = "iterator-range",
    hdrs = ["iterator_range.h"],
    deps = [],
)

cc_library(
    name = "spacer",
    srcs = ["spacer.cc"],
    hdrs = ["spacer.h"],
)

cc_library(
    name = "top-n",
    hdrs = ["top_n.h"],
)

cc_library(
    name = "value-saver",
    hdrs = ["value_saver.h"],
)

cc_library(
    name = "enum-flags",
    hdrs = ["enum_flags.h"],
    deps = [
        ":bijective-map",
        "//common/strings:compare",
        "@com_google_absl//absl/strings",
    ],
)

cc_library(
    name = "bijective-map",
    hdrs = ["bijective_map.h"],
    deps = [":logging"],
)

cc_library(
    name = "vector-tree",
    hdrs = ["vector_tree.h"],
    deps = [
        ":container-proxy",
        ":iterator-range",
        ":logging",
    ],
)

cc_library(
    name = "tree-operations",
    srcs = ["tree_operations.cc"],
    hdrs = ["tree_operations.h"],
    deps = [
        ":logging",
        ":spacer",
        ":type-traits",
        "@com_google_absl//absl/strings",
    ],
)

cc_library(
    name = "expandable-tree-view",
    hdrs = ["expandable_tree_view.h"],
    deps = [
        ":logging",
        ":tree-operations",
        ":vector-tree",
    ],
)

cc_library(
    name = "vector-tree-test-util",
    testonly = 1,
    srcs = ["vector_tree_test_util.cc"],
    hdrs = ["vector_tree_test_util.h"],
    deps = [
        ":tree-operations",
        ":vector-tree",
        "@com_google_googletest//:gtest",  # for library testonly
    ],
)

cc_library(
    name = "vector-tree-iterators",
    hdrs = ["vector_tree_iterators.h"],
    deps = [
        ":iterator-range",
        ":logging",
        ":tree-operations",
    ],
)

cc_library(
    name = "map-tree",
    hdrs = ["map_tree.h"],
    deps = [
        ":logging",
        ":spacer",
    ],
)

cc_library(
    name = "with-reason",
    hdrs = ["with_reason.h"],
)

cc_library(
    name = "user-interaction",
    srcs = ["user_interaction.cc"],
    hdrs = ["user_interaction.h"],
)

cc_library(
    name = "container-proxy",
    hdrs = ["container_proxy.h"],
)

cc_test(
    name = "algorithm_test",
    srcs = ["algorithm_test.cc"],
    deps = [
        ":algorithm",
        ":iterator-range",
        "@com_google_googletest//:gtest",
        "@com_google_googletest//:gtest_main",
    ],
)

cc_test(
    name = "auto-iterator_test",
    srcs = ["auto_iterator_test.cc"],
    deps = [
        ":auto-iterator",
        "@com_google_googletest//:gtest",
        "@com_google_googletest//:gtest_main",
    ],
)

cc_test(
    name = "auto-pop-stack_test",
    srcs = ["auto_pop_stack_test.cc"],
    deps = [
        ":auto-pop-stack",
        ":iterator-range",
        "@com_google_googletest//:gtest",
        "@com_google_googletest//:gtest_main",
    ],
)

cc_test(
    name = "file-util_test",
    srcs = ["file_util_test.cc"],
    deps = [
        ":file-util",
        "@com_google_absl//absl/status",
        "@com_google_absl//absl/status:statusor",
        "@com_google_absl//absl/strings",
        "@com_google_googletest//:gtest",
        "@com_google_googletest//:gtest_main",
    ],
)

cc_test(
    name = "interval_test",
    srcs = ["interval_test.cc"],
    deps = [
        ":interval",
        "@com_google_googletest//:gtest",
        "@com_google_googletest//:gtest_main",
    ],
)

cc_test(
    name = "interval-map_test",
    srcs = ["interval_map_test.cc"],
    deps = [
        ":interval-map",
        ":range",
        "@com_google_googletest//:gtest",
        "@com_google_googletest//:gtest_main",
    ],
)

cc_test(
    name = "interval-set_test",
    srcs = ["interval_set_test.cc"],
    deps = [
        ":interval",
        ":interval-set",
        ":logging",
        "@com_google_googletest//:gtest",
        "@com_google_googletest//:gtest_main",
    ],
)

cc_test(
    name = "forward_test",
    srcs = ["forward_test.cc"],
    deps = [
        ":forward",
        "@com_google_googletest//:gtest",
        "@com_google_googletest//:gtest_main",
    ],
)

cc_test(
    name = "range_test",
    srcs = ["range_test.cc"],
    deps = [
        ":iterator-range",
        ":range",
        "@com_google_googletest//:gtest",
        "@com_google_googletest//:gtest_main",
    ],
)

cc_test(
    name = "thread-pool_test",
    srcs = ["thread_pool_test.cc"],
    copts = select({
        "@platforms//os:windows": [],
        "//conditions:default": ["-fexceptions"],
    }),
    features = ["-use_header_modules"],  # precompiled headers incompatible with -fexceptions.
    deps = [
        ":thread-pool",
        "@com_google_absl//absl/time",
        "@com_google_googletest//:gtest",
        "@com_google_googletest//:gtest_main",
    ],
)

cc_test(
    name = "container-iterator-range_test",
    srcs = ["container_iterator_range_test.cc"],
    deps = [
        ":container-iterator-range",
        "@com_google_googletest//:gtest",
        "@com_google_googletest//:gtest_main",
    ],
)

cc_test(
    name = "iterator-adaptors_test",
    srcs = ["iterator_adaptors_test.cc"],
    deps = [
        ":iterator-adaptors",
        ":iterator-range",
        "@com_google_googletest//:gtest",
        "@com_google_googletest//:gtest_main",
    ],
)

cc_test(
    name = "iterator-range_test",
    srcs = ["iterator_range_test.cc"],
    deps = [
        ":iterator-range",
        ":range",
        "@com_google_googletest//:gtest",
        "@com_google_googletest//:gtest_main",
    ],
)

cc_test(
    name = "spacer_test",
    srcs = ["spacer_test.cc"],
    deps = [
        ":spacer",
        "@com_google_googletest//:gtest",
        "@com_google_googletest//:gtest_main",
    ],
)

cc_test(
    name = "subcommand_test",
    srcs = ["subcommand_test.cc"],
    deps = [
        ":subcommand",
        "@com_google_absl//absl/status",
        "@com_google_absl//absl/strings",
        "@com_google_googletest//:gtest",
        "@com_google_googletest//:gtest_main",
    ],
)

cc_test(
    name = "top-n_test",
    srcs = ["top_n_test.cc"],
    deps = [
        ":top-n",
        "@com_google_googletest//:gtest",
        "@com_google_googletest//:gtest_main",
    ],
)

cc_test(
    name = "type-traits_test",
    srcs = ["type_traits_test.cc"],
    deps = [
        ":type-traits",
        "@com_google_googletest//:gtest",
        "@com_google_googletest//:gtest_main",
    ],
)

cc_test(
    name = "value-saver_test",
    srcs = ["value_saver_test.cc"],
    deps = [
        ":value-saver",
        "@com_google_googletest//:gtest",
        "@com_google_googletest//:gtest_main",
    ],
)

cc_test(
    name = "enum-flags_test",
    srcs = ["enum_flags_test.cc"],
    deps = [
        ":enum-flags",
        "@com_google_absl//absl/strings",
        "@com_google_googletest//:gtest",
        "@com_google_googletest//:gtest_main",
    ],
)

cc_test(
    name = "bijective-map_test",
    srcs = ["bijective_map_test.cc"],
    deps = [
        ":bijective-map",
        ":logging",
        "//common/strings:compare",
        "@com_google_googletest//:gtest",
        "@com_google_googletest//:gtest_main",
    ],
)

cc_test(
    name = "vector-tree_test",
    srcs = ["vector_tree_test.cc"],
    deps = [
        ":logging",
        ":tree-operations",
        ":vector-tree",
        ":vector-tree-test-util",
        "@com_google_absl//absl/strings",
        "@com_google_googletest//:gtest",
        "@com_google_googletest//:gtest_main",
    ],
)

[
    (
        cc_test(
            name = "tree-operations_" + shard + "_test",
            srcs = ["tree_operations_test.cc"],
            defines = ["COMPILATION_SHARD=" + shard],
            deps = [
                ":spacer",
                ":tree-operations",
                "@com_google_absl//absl/strings",
                "@com_google_absl//absl/strings:str_format",
                "@com_google_googletest//:gtest",
                "@com_google_googletest//:gtest_main",
            ],
        ),
    )
    for shard in [
        "1",
        "2",
        "3",
        "4",
        "5",
        "6",
    ]
]

cc_test(
    name = "expandable-tree-view_test",
    srcs = ["expandable_tree_view_test.cc"],
    deps = [
        ":expandable-tree-view",
        ":vector-tree",
        ":vector-tree-test-util",
        "@com_google_absl//absl/strings",
        "@com_google_googletest//:gtest",
        "@com_google_googletest//:gtest_main",
    ],
)

cc_test(
    name = "vector-tree-iterators_test",
    srcs = ["vector_tree_iterators_test.cc"],
    deps = [
        ":tree-operations",
        ":vector-tree",
        ":vector-tree-iterators",
        "@com_google_absl//absl/strings",
        "@com_google_googletest//:gtest",
        "@com_google_googletest//:gtest_main",
    ],
)

cc_test(
    name = "map-tree_test",
    srcs = ["map_tree_test.cc"],
    copts = select({
        "@platforms//os:windows": [],
        "//conditions:default": ["-Wno-array-bounds"],
    }),
    deps = [
        ":map-tree",
        ":spacer",
        "@com_google_googletest//:gtest",
        "@com_google_googletest//:gtest_main",
    ],
)

cc_test(
    name = "with-reason_test",
    srcs = ["with_reason_test.cc"],
    deps = [
        ":with-reason",
        "@com_google_googletest//:gtest",
        "@com_google_googletest//:gtest_main",
    ],
)

cc_test(
    name = "user-interaction_test",
    srcs = ["user_interaction_test.cc"],
    deps = [
        ":user-interaction",
        "@com_google_absl//absl/strings",
        "@com_google_googletest//:gtest",
        "@com_google_googletest//:gtest_main",
    ],
)

cc_test(
    name = "container-proxy_test",
    srcs = ["container_proxy_test.cc"],
    deps = [
        ":container-proxy",
        ":type-traits",
        "//common/strings:display-utils",
        "@com_google_googletest//:gtest",
        "@com_google_googletest//:gtest_main",
    ],
)

cc_library(
    name = "simple-zip",
    srcs = ["simple_zip.cc"],
    hdrs = ["simple_zip.h"],
    deps = [
        "//third_party/portable_endian",
        "@zlib",
    ],
)

cc_test(
    name = "simple-zip_test",
    srcs = ["simple_zip_test.cc"],
    deps = [
        ":file-util",
        ":simple-zip",
        "@com_google_googletest//:gtest",
        "@com_google_googletest//:gtest_main",
    ],
)

cc_library(
    name = "sha256",
    srcs = ["sha256.cc"],
    hdrs = ["sha256.h"],
    deps = ["@com_google_absl//absl/strings"],
)

cc_test(
    name = "sha256_test",
    srcs = ["sha256_test.cc"],
    deps = [
        ":sha256",
        "@com_google_absl//absl/strings",
        "@com_google_googletest//:gtest",
        "@com_google_googletest//:gtest_main",
    ],
)
