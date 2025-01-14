# This package contains functions for dealing specifically with strings.

package(
    default_applicable_licenses = ["//:license"],
    default_visibility = [
        "//:__subpackages__",
    ],
    features = ["layering_check"],
)

cc_library(
    name = "compare",
    hdrs = ["compare.h"],
)

cc_test(
    name = "compare_test",
    srcs = ["compare_test.cc"],
    deps = [
        ":compare",
        "@googletest//:gtest",
        "@googletest//:gtest_main",
    ],
)

cc_library(
    name = "comment-utils",
    srcs = ["comment-utils.cc"],
    hdrs = ["comment-utils.h"],
    deps = [
        "//verible/common/util:logging",
        "//verible/common/util:range",
        "@abseil-cpp//absl/strings",
    ],
)

cc_test(
    name = "comment-utils_test",
    srcs = ["comment-utils_test.cc"],
    deps = [
        ":comment-utils",
        "//verible/common/util:range",
        "@googletest//:gtest",
        "@googletest//:gtest_main",
    ],
)

cc_library(
    name = "diff",
    srcs = ["diff.cc"],
    hdrs = ["diff.h"],
    deps = [
        ":position",
        ":split",
        "//external_libs:editscript",
        "//verible/common/util:iterator-range",
    ],
)

cc_test(
    name = "diff_test",
    srcs = ["diff_test.cc"],
    deps = [
        ":diff",
        ":position",
        "//external_libs:editscript",
        "@googletest//:gtest",
        "@googletest//:gtest_main",
    ],
)

cc_library(
    name = "display-utils",
    srcs = ["display-utils.cc"],
    hdrs = ["display-utils.h"],
    deps = ["@abseil-cpp//absl/strings"],
)

cc_test(
    name = "display-utils_test",
    srcs = ["display-utils_test.cc"],
    deps = [
        ":display-utils",
        "@googletest//:gtest",
        "@googletest//:gtest_main",
    ],
)

cc_library(
    name = "obfuscator",
    srcs = ["obfuscator.cc"],
    hdrs = ["obfuscator.h"],
    deps = [
        ":compare",
        "//verible/common/util:bijective-map",
        "//verible/common/util:logging",
        "@abseil-cpp//absl/status",
        "@abseil-cpp//absl/strings",
    ],
)

cc_test(
    name = "obfuscator_test",
    srcs = ["obfuscator_test.cc"],
    deps = [
        ":obfuscator",
        ":random",
        "//verible/common/util:bijective-map",
        "//verible/common/util:logging",
        "@abseil-cpp//absl/status",
        "@googletest//:gtest",
        "@googletest//:gtest_main",
    ],
)

cc_library(
    name = "mem-block",
    hdrs = ["mem-block.h"],
)

cc_library(
    name = "naming-utils",
    srcs = ["naming-utils.cc"],
    hdrs = ["naming-utils.h"],
    deps = ["@abseil-cpp//absl/strings"],
)

cc_test(
    name = "naming-utils_test",
    srcs = ["naming-utils_test.cc"],
    deps = [
        ":naming-utils",
        "@googletest//:gtest",
        "@googletest//:gtest_main",
    ],
)

cc_library(
    name = "patch",
    srcs = ["patch.cc"],
    hdrs = ["patch.h"],
    deps = [
        ":compare",
        ":position",
        ":split",
        "//verible/common/util:algorithm",
        "//verible/common/util:container-iterator-range",
        "//verible/common/util:file-util",
        "//verible/common/util:iterator-adaptors",
        "//verible/common/util:iterator-range",
        "//verible/common/util:logging",
        "//verible/common/util:status-macros",
        "//verible/common/util:user-interaction",
        "@abseil-cpp//absl/base:core_headers",
        "@abseil-cpp//absl/status",
        "@abseil-cpp//absl/status:statusor",
        "@abseil-cpp//absl/strings",
    ],
)

cc_test(
    name = "patch_test",
    srcs = ["patch_test.cc"],
    deps = [
        ":patch",
        ":position",
        "@abseil-cpp//absl/status",
        "@abseil-cpp//absl/status:statusor",
        "@abseil-cpp//absl/strings",
        "@googletest//:gtest",
        "@googletest//:gtest_main",
    ],
)

cc_library(
    name = "position",
    srcs = ["position.cc"],
    hdrs = ["position.h"],
    deps = [
        "//verible/common/util:interval",
        "//verible/common/util:interval-set",
    ],
)

cc_test(
    name = "position_test",
    srcs = ["position_test.cc"],
    deps = [
        ":position",
        "@googletest//:gtest",
        "@googletest//:gtest_main",
    ],
)

cc_library(
    name = "random",
    srcs = ["random.cc"],
    hdrs = ["random.h"],
    deps = [
        "//verible/common/util:interval-set",
        "//verible/common/util:iterator-range",
        "//verible/common/util:logging",
    ],
)

cc_test(
    name = "random_test",
    srcs = ["random_test.cc"],
    deps = [
        ":random",
        "@googletest//:gtest",
        "@googletest//:gtest_main",
    ],
)

cc_library(
    name = "range",
    srcs = ["range.cc"],
    hdrs = ["range.h"],
    deps = [
        "//verible/common/util:logging",
        "//verible/common/util:range",
    ],
)

cc_test(
    name = "range_test",
    srcs = ["range_test.cc"],
    deps = [
        ":range",
        "//verible/common/util:range",
        "@googletest//:gtest",
        "@googletest//:gtest_main",
    ],
)

cc_library(
    name = "split",
    srcs = ["split.cc"],
    hdrs = ["split.h"],
    deps = ["@abseil-cpp//absl/strings"],
)

cc_test(
    name = "split_test",
    srcs = ["split_test.cc"],
    deps = [
        ":range",
        ":split",
        "//verible/common/util:range",
        "@googletest//:gtest",
        "@googletest//:gtest_main",
    ],
)

cc_library(
    name = "rebase",
    srcs = ["rebase.cc"],
    hdrs = ["rebase.h"],
    deps = ["//verible/common/util:logging"],
)

cc_test(
    name = "rebase_test",
    srcs = ["rebase_test.cc"],
    deps = [
        ":rebase",
        "//verible/common/util:range",
        "@googletest//:gtest",
        "@googletest//:gtest_main",
    ],
)

cc_library(
    name = "utf8",
    hdrs = ["utf8.h"],
)

cc_library(
    name = "line-column-map",
    srcs = ["line-column-map.cc"],
    hdrs = ["line-column-map.h"],
    visibility = [
        "//verible/common/analysis:__pkg__",
        "//verible/common/text:__pkg__",
        "//verible/verilog/analysis:__pkg__",
        "//verible/verilog/formatting:__pkg__",
        "//verible/verilog/tools/ls:__pkg__",
    ],
    deps = [":utf8"],
)

cc_test(
    name = "utf8_test",
    srcs = ["utf8_test.cc"],
    deps = [
        ":utf8",
        "@googletest//:gtest",
        "@googletest//:gtest_main",
    ],
)

cc_test(
    name = "line-column-map_test",
    srcs = ["line-column-map_test.cc"],
    deps = [
        ":line-column-map",
        "@abseil-cpp//absl/strings",
        "@googletest//:gtest",
        "@googletest//:gtest_main",
    ],
)

cc_library(
    name = "string-memory-map",
    hdrs = ["string-memory-map.h"],
    deps = [
        ":range",
        "//verible/common/util:interval-map",
        "//verible/common/util:interval-set",
        "//verible/common/util:logging",
    ],
)

cc_test(
    name = "string-memory-map_test",
    srcs = ["string-memory-map_test.cc"],
    deps = [
        ":range",
        ":string-memory-map",
        "//verible/common/util:logging",
        "//verible/common/util:range",
        "@googletest//:gtest",
        "@googletest//:gtest_main",
    ],
)
