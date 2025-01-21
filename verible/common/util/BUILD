# 'util' contains generic containers, adapters, algorithms
# Any verible package may directly depend on this.

load(":module-version.bzl", "get_version_define_from_module")

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
    hdrs = ["auto-pop-stack.h"],
    deps = [
        ":logging",
        "@abseil-cpp//absl/base:core_headers",
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
    hdrs = ["status-macros.h"],
    deps = ["@abseil-cpp//absl/base:core_headers"],
)

cc_library(
    name = "subcommand",
    srcs = ["subcommand.cc"],
    hdrs = ["subcommand.h"],
    deps = [
        ":container-iterator-range",
        "//verible/common/strings:compare",
        "@abseil-cpp//absl/status",
        "@abseil-cpp//absl/strings",
    ],
)

cc_library(
    name = "thread-pool",
    srcs = ["thread-pool.cc"],
    hdrs = ["thread-pool.h"],
    copts = select({
        "@platforms//os:windows": [],
        "//conditions:default": ["-fexceptions"],
    }),
    features = ["-use_header_modules"],  # precompiled headers incompatible with -fexceptions.
)

cc_library(
    name = "file-util",
    srcs = ["file-util.cc"],
    hdrs = ["file-util.h"],
    deps = [
        ":logging",
        "//verible/common/strings:mem-block",
        "@abseil-cpp//absl/status",
        "@abseil-cpp//absl/status:statusor",
        "@abseil-cpp//absl/strings",
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
    outs = ["generated-verible-build-version.h"],
    cmd = "sed -e 's/^/#define VERIBLE_/' bazel-out/volatile-status.txt > $@",
    stamp = 1,  # Tell bazel that we really are interested in the status file
    visibility = ["//visibility:private"],
)

cc_library(
    name = "build-version",
    hdrs = ["generated-verible-build-version.h"],
    visibility = ["//visibility:private"],
)

cc_library(
    name = "init-command-line",
    srcs = ["init-command-line.cc"],
    hdrs = ["init-command-line.h"],
    copts = get_version_define_from_module(),
    deps = [
        # these deps are needed by init_command_line.cc:
        ":build-version",
        "@abseil-cpp//absl/debugging:failure_signal_handler",
        "@abseil-cpp//absl/debugging:symbolize",
        "@abseil-cpp//absl/flags:config",
        "@abseil-cpp//absl/flags:parse",
        "@abseil-cpp//absl/flags:usage",
        "@abseil-cpp//absl/log:globals",
        "@abseil-cpp//absl/base:log_severity",
        "@abseil-cpp//absl/log:initialize",
        "@abseil-cpp//absl/strings",
        "@abseil-cpp//absl/time",
    ],
)

cc_library(
    name = "interval",
    hdrs = ["interval.h"],
    deps = [
        ":forward",
        "@abseil-cpp//absl/strings",
    ],
)

cc_library(
    name = "interval-map",
    hdrs = ["interval-map.h"],
    deps = [
        ":auto-iterator",
        ":logging",
    ],
)

cc_library(
    name = "interval-set",
    hdrs = ["interval-set.h"],
    deps = [
        ":auto-iterator",
        ":interval",
        ":iterator-range",
        ":logging",
        "@abseil-cpp//absl/random",
        "@abseil-cpp//absl/strings",
    ],
)

# TODO: once all absl logging features are established in abseil-cpp, we
# should IWYU them directly in places where we need logging and remove
# this common/util:logging target.
cc_library(
    name = "logging",
    hdrs = ["logging.h"],
    deps = [
        "@abseil-cpp//absl/log",
        "@abseil-cpp//absl/log:check",
        "@abseil-cpp//absl/log:die_if_null",
    ],
)

cc_library(
    name = "casts",
    hdrs = ["casts.h"],
)

cc_library(
    name = "container-util",
    hdrs = ["container-util.h"],
    deps = [
        ":logging",
    ],
)

cc_library(
    name = "container-iterator-range",
    hdrs = ["container-iterator-range.h"],
    deps = [
        ":iterator-range",
        ":range",
    ],
)

cc_library(
    name = "type-traits",
    hdrs = ["type-traits.h"],
)

cc_library(
    name = "auto-iterator",
    hdrs = ["auto-iterator.h"],
)

cc_library(
    name = "iterator-adaptors",
    hdrs = ["iterator-adaptors.h"],
    deps = [
        ":auto-iterator",
        ":iterator-range",
    ],
)

cc_library(
    name = "iterator-range",
    hdrs = ["iterator-range.h"],
    deps = [],
)

cc_library(
    name = "spacer",
    srcs = ["spacer.cc"],
    hdrs = ["spacer.h"],
)

cc_library(
    name = "top-n",
    hdrs = ["top-n.h"],
)

cc_library(
    name = "value-saver",
    hdrs = ["value-saver.h"],
)

cc_library(
    name = "enum-flags",
    hdrs = ["enum-flags.h"],
    deps = [
        ":bijective-map",
        "//verible/common/strings:compare",
        "@abseil-cpp//absl/strings",
    ],
)

cc_library(
    name = "bijective-map",
    hdrs = ["bijective-map.h"],
    deps = [":logging"],
)

cc_library(
    name = "vector-tree",
    hdrs = ["vector-tree.h"],
    deps = [
        ":container-proxy",
        ":iterator-range",
        ":logging",
    ],
)

cc_library(
    name = "tree-operations",
    srcs = ["tree-operations.cc"],
    hdrs = ["tree-operations.h"],
    deps = [
        ":logging",
        ":spacer",
        ":type-traits",
        "@abseil-cpp//absl/strings",
    ],
)

cc_library(
    name = "expandable-tree-view",
    hdrs = ["expandable-tree-view.h"],
    deps = [
        ":logging",
        ":tree-operations",
        ":vector-tree",
    ],
)

cc_library(
    name = "vector-tree-test-util",
    testonly = 1,
    srcs = ["vector-tree-test-util.cc"],
    hdrs = ["vector-tree-test-util.h"],
    deps = [
        ":tree-operations",
        ":vector-tree",
        "@googletest//:gtest",  # for library testonly
    ],
)

cc_library(
    name = "vector-tree-iterators",
    hdrs = ["vector-tree-iterators.h"],
    deps = [
        ":iterator-range",
        ":logging",
        ":tree-operations",
    ],
)

cc_library(
    name = "map-tree",
    hdrs = ["map-tree.h"],
    deps = [
        ":logging",
        ":spacer",
    ],
)

cc_library(
    name = "with-reason",
    hdrs = ["with-reason.h"],
)

cc_library(
    name = "user-interaction",
    srcs = ["user-interaction.cc"],
    hdrs = ["user-interaction.h"],
)

cc_library(
    name = "container-proxy",
    hdrs = ["container-proxy.h"],
)

cc_test(
    name = "algorithm_test",
    srcs = ["algorithm_test.cc"],
    deps = [
        ":algorithm",
        ":iterator-range",
        "@googletest//:gtest",
        "@googletest//:gtest_main",
    ],
)

cc_test(
    name = "auto-iterator_test",
    srcs = ["auto-iterator_test.cc"],
    deps = [
        ":auto-iterator",
        "@googletest//:gtest",
        "@googletest//:gtest_main",
    ],
)

cc_test(
    name = "auto-pop-stack_test",
    srcs = ["auto-pop-stack_test.cc"],
    deps = [
        ":auto-pop-stack",
        ":iterator-range",
        "@googletest//:gtest",
        "@googletest//:gtest_main",
    ],
)

cc_test(
    name = "file-util_test",
    srcs = ["file-util_test.cc"],
    deps = [
        ":file-util",
        "@abseil-cpp//absl/status",
        "@abseil-cpp//absl/status:statusor",
        "@abseil-cpp//absl/strings",
        "@googletest//:gtest",
        "@googletest//:gtest_main",
    ],
)

cc_test(
    name = "interval_test",
    srcs = ["interval_test.cc"],
    deps = [
        ":interval",
        "@googletest//:gtest",
        "@googletest//:gtest_main",
    ],
)

cc_test(
    name = "interval-map_test",
    srcs = ["interval-map_test.cc"],
    deps = [
        ":interval-map",
        ":range",
        "@googletest//:gtest",
        "@googletest//:gtest_main",
    ],
)

cc_test(
    name = "interval-set_test",
    srcs = ["interval-set_test.cc"],
    deps = [
        ":interval",
        ":interval-set",
        ":logging",
        "@googletest//:gtest",
        "@googletest//:gtest_main",
    ],
)

cc_test(
    name = "forward_test",
    srcs = ["forward_test.cc"],
    deps = [
        ":forward",
        "@googletest//:gtest",
        "@googletest//:gtest_main",
    ],
)

cc_test(
    name = "range_test",
    srcs = ["range_test.cc"],
    deps = [
        ":iterator-range",
        ":range",
        "@googletest//:gtest",
        "@googletest//:gtest_main",
    ],
)

cc_test(
    name = "thread-pool_test",
    srcs = ["thread-pool_test.cc"],
    copts = select({
        "@platforms//os:windows": [],
        "//conditions:default": ["-fexceptions"],
    }),
    features = ["-use_header_modules"],  # precompiled headers incompatible with -fexceptions.
    deps = [
        ":thread-pool",
        "@abseil-cpp//absl/time",
        "@googletest//:gtest",
        "@googletest//:gtest_main",
    ],
)

cc_test(
    name = "container-iterator-range_test",
    srcs = ["container-iterator-range_test.cc"],
    deps = [
        ":container-iterator-range",
        "@googletest//:gtest",
        "@googletest//:gtest_main",
    ],
)

cc_test(
    name = "iterator-adaptors_test",
    srcs = ["iterator-adaptors_test.cc"],
    deps = [
        ":iterator-adaptors",
        ":iterator-range",
        "@googletest//:gtest",
        "@googletest//:gtest_main",
    ],
)

cc_test(
    name = "iterator-range_test",
    srcs = ["iterator-range_test.cc"],
    deps = [
        ":iterator-range",
        ":range",
        "@googletest//:gtest",
        "@googletest//:gtest_main",
    ],
)

cc_test(
    name = "spacer_test",
    srcs = ["spacer_test.cc"],
    deps = [
        ":spacer",
        "@googletest//:gtest",
        "@googletest//:gtest_main",
    ],
)

cc_test(
    name = "subcommand_test",
    srcs = ["subcommand_test.cc"],
    deps = [
        ":subcommand",
        "@abseil-cpp//absl/status",
        "@abseil-cpp//absl/strings",
        "@googletest//:gtest",
        "@googletest//:gtest_main",
    ],
)

cc_test(
    name = "top-n_test",
    srcs = ["top-n_test.cc"],
    deps = [
        ":top-n",
        "@googletest//:gtest",
        "@googletest//:gtest_main",
    ],
)

cc_test(
    name = "type-traits_test",
    srcs = ["type-traits_test.cc"],
    deps = [
        ":type-traits",
        "@googletest//:gtest",
        "@googletest//:gtest_main",
    ],
)

cc_test(
    name = "value-saver_test",
    srcs = ["value-saver_test.cc"],
    deps = [
        ":value-saver",
        "@googletest//:gtest",
        "@googletest//:gtest_main",
    ],
)

cc_test(
    name = "enum-flags_test",
    srcs = ["enum-flags_test.cc"],
    deps = [
        ":enum-flags",
        "@abseil-cpp//absl/strings",
        "@googletest//:gtest",
        "@googletest//:gtest_main",
    ],
)

cc_test(
    name = "bijective-map_test",
    srcs = ["bijective-map_test.cc"],
    deps = [
        ":bijective-map",
        ":logging",
        "//verible/common/strings:compare",
        "@googletest//:gtest",
        "@googletest//:gtest_main",
    ],
)

cc_test(
    name = "vector-tree_test",
    srcs = ["vector-tree_test.cc"],
    deps = [
        ":logging",
        ":tree-operations",
        ":vector-tree",
        ":vector-tree-test-util",
        "@abseil-cpp//absl/strings",
        "@googletest//:gtest",
        "@googletest//:gtest_main",
    ],
)

[
    (
        cc_test(
            name = "tree-operations_" + shard + "_test",
            srcs = ["tree-operations_test.cc"],
            defines = ["COMPILATION_SHARD=" + shard],
            deps = [
                ":logging",
                ":spacer",
                ":tree-operations",
                ":type-traits",
                "@abseil-cpp//absl/strings",
                "@abseil-cpp//absl/strings:str_format",
                "@googletest//:gtest",
                "@googletest//:gtest_main",
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
    srcs = ["expandable-tree-view_test.cc"],
    deps = [
        ":expandable-tree-view",
        ":vector-tree",
        ":vector-tree-test-util",
        "@abseil-cpp//absl/strings",
        "@googletest//:gtest",
        "@googletest//:gtest_main",
    ],
)

cc_test(
    name = "vector-tree-iterators_test",
    srcs = ["vector-tree-iterators_test.cc"],
    deps = [
        ":tree-operations",
        ":vector-tree",
        ":vector-tree-iterators",
        "@abseil-cpp//absl/strings",
        "@googletest//:gtest",
        "@googletest//:gtest_main",
    ],
)

cc_test(
    name = "map-tree_test",
    srcs = ["map-tree_test.cc"],
    copts = select({
        "@platforms//os:windows": [],
        "//conditions:default": ["-Wno-array-bounds"],
    }),
    deps = [
        ":map-tree",
        ":spacer",
        "@googletest//:gtest",
        "@googletest//:gtest_main",
    ],
)

cc_test(
    name = "with-reason_test",
    srcs = ["with-reason_test.cc"],
    deps = [
        ":with-reason",
        "@googletest//:gtest",
        "@googletest//:gtest_main",
    ],
)

cc_test(
    name = "user-interaction_test",
    srcs = ["user-interaction_test.cc"],
    deps = [
        ":user-interaction",
        "@abseil-cpp//absl/strings",
        "@googletest//:gtest",
        "@googletest//:gtest_main",
    ],
)

cc_test(
    name = "container-proxy_test",
    srcs = ["container-proxy_test.cc"],
    deps = [
        ":container-proxy",
        ":type-traits",
        "//verible/common/strings:display-utils",
        "@googletest//:gtest",
        "@googletest//:gtest_main",
    ],
)

cc_library(
    name = "simple-zip",
    srcs = ["simple-zip.cc"],
    hdrs = ["simple-zip.h"],
    deps = [
        "//third_party/portable_endian",
        "@zlib",
    ],
)

cc_test(
    name = "simple-zip_test",
    srcs = ["simple-zip_test.cc"],
    deps = [
        ":file-util",
        ":simple-zip",
        "@googletest//:gtest",
        "@googletest//:gtest_main",
    ],
)

cc_library(
    name = "sha256",
    srcs = ["sha256.cc"],
    hdrs = ["sha256.h"],
    deps = ["@abseil-cpp//absl/strings"],
)

cc_test(
    name = "sha256_test",
    srcs = ["sha256_test.cc"],
    deps = [
        ":sha256",
        "@abseil-cpp//absl/strings",
        "@googletest//:gtest",
        "@googletest//:gtest_main",
    ],
)
