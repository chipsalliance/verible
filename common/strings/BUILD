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
        "@com_google_googletest//:gtest",
        "@com_google_googletest//:gtest_main",
    ],
)

cc_library(
    name = "comment-utils",
    srcs = ["comment_utils.cc"],
    hdrs = ["comment_utils.h"],
    deps = [
        "//common/util:logging",
        "//common/util:range",
        "@com_google_absl//absl/strings",
    ],
)

cc_test(
    name = "comment-utils_test",
    srcs = ["comment_utils_test.cc"],
    deps = [
        ":comment-utils",
        "//common/util:range",
        "@com_google_googletest//:gtest",
        "@com_google_googletest//:gtest_main",
    ],
)

cc_library(
    name = "diff",
    srcs = ["diff.cc"],
    hdrs = ["diff.h"],
    deps = [
        ":position",
        ":split",
        "//common/util:iterator-range",
        "//external_libs:editscript",
    ],
)

cc_test(
    name = "diff_test",
    srcs = ["diff_test.cc"],
    deps = [
        ":diff",
        ":position",
        "//external_libs:editscript",
        "@com_google_googletest//:gtest",
        "@com_google_googletest//:gtest_main",
    ],
)

cc_library(
    name = "display-utils",
    srcs = ["display_utils.cc"],
    hdrs = ["display_utils.h"],
    deps = ["@com_google_absl//absl/strings"],
)

cc_test(
    name = "display-utils_test",
    srcs = ["display_utils_test.cc"],
    deps = [
        ":display-utils",
        "@com_google_googletest//:gtest",
        "@com_google_googletest//:gtest_main",
    ],
)

cc_library(
    name = "obfuscator",
    srcs = ["obfuscator.cc"],
    hdrs = ["obfuscator.h"],
    deps = [
        ":compare",
        "//common/util:bijective-map",
        "//common/util:logging",
        "@com_google_absl//absl/status",
        "@com_google_absl//absl/strings",
    ],
)

cc_test(
    name = "obfuscator_test",
    srcs = ["obfuscator_test.cc"],
    deps = [
        ":obfuscator",
        ":random",
        "//common/util:bijective-map",
        "//common/util:logging",
        "@com_google_absl//absl/status",
        "@com_google_googletest//:gtest",
        "@com_google_googletest//:gtest_main",
    ],
)

cc_library(
    name = "mem-block",
    hdrs = ["mem_block.h"],
)

cc_library(
    name = "naming-utils",
    srcs = ["naming_utils.cc"],
    hdrs = ["naming_utils.h"],
    deps = ["@com_google_absl//absl/strings"],
)

cc_test(
    name = "naming-utils_test",
    srcs = ["naming_utils_test.cc"],
    deps = [
        ":naming-utils",
        "@com_google_googletest//:gtest",
        "@com_google_googletest//:gtest_main",
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
        "//common/util:algorithm",
        "//common/util:container-iterator-range",
        "//common/util:file-util",
        "//common/util:iterator-adaptors",
        "//common/util:iterator-range",
        "//common/util:logging",
        "//common/util:status-macros",
        "//common/util:user-interaction",
        "@com_google_absl//absl/base:core_headers",
        "@com_google_absl//absl/status",
        "@com_google_absl//absl/status:statusor",
        "@com_google_absl//absl/strings",
    ],
)

cc_test(
    name = "patch_test",
    srcs = ["patch_test.cc"],
    deps = [
        ":patch",
        ":position",
        "@com_google_absl//absl/status",
        "@com_google_absl//absl/status:statusor",
        "@com_google_absl//absl/strings",
        "@com_google_googletest//:gtest",
        "@com_google_googletest//:gtest_main",
    ],
)

cc_library(
    name = "position",
    srcs = ["position.cc"],
    hdrs = ["position.h"],
    deps = [
        "//common/util:interval",
        "//common/util:interval-set",
    ],
)

cc_test(
    name = "position_test",
    srcs = ["position_test.cc"],
    deps = [
        ":position",
        "@com_google_googletest//:gtest",
        "@com_google_googletest//:gtest_main",
    ],
)

cc_library(
    name = "random",
    srcs = ["random.cc"],
    hdrs = ["random.h"],
    deps = [
        "//common/util:interval-set",
        "//common/util:iterator-range",
        "//common/util:logging",
    ],
)

cc_test(
    name = "random_test",
    srcs = ["random_test.cc"],
    deps = [
        ":random",
        "@com_google_googletest//:gtest",
        "@com_google_googletest//:gtest_main",
    ],
)

cc_library(
    name = "range",
    srcs = ["range.cc"],
    hdrs = ["range.h"],
    deps = [
        "//common/util:logging",
        "//common/util:range",
    ],
)

cc_test(
    name = "range_test",
    srcs = ["range_test.cc"],
    deps = [
        ":range",
        "//common/util:range",
        "@com_google_googletest//:gtest",
        "@com_google_googletest//:gtest_main",
    ],
)

cc_library(
    name = "split",
    srcs = ["split.cc"],
    hdrs = ["split.h"],
    deps = ["@com_google_absl//absl/strings"],
)

cc_test(
    name = "split_test",
    srcs = ["split_test.cc"],
    deps = [
        ":range",
        ":split",
        "//common/util:range",
        "@com_google_googletest//:gtest",
        "@com_google_googletest//:gtest_main",
    ],
)

cc_library(
    name = "rebase",
    srcs = ["rebase.cc"],
    hdrs = ["rebase.h"],
    deps = ["//common/util:logging"],
)

cc_test(
    name = "rebase_test",
    srcs = ["rebase_test.cc"],
    deps = [
        ":rebase",
        "//common/util:range",
        "@com_google_googletest//:gtest",
        "@com_google_googletest//:gtest_main",
    ],
)

cc_library(
    name = "utf8",
    hdrs = ["utf8.h"],
)

cc_library(
    name = "line-column-map",
    srcs = ["line_column_map.cc"],
    hdrs = ["line_column_map.h"],
    visibility = [
        "//common/analysis:__pkg__",
        "//common/text:__pkg__",
        "//verilog/analysis:__pkg__",
        "//verilog/formatting:__pkg__",
        "//verilog/tools/ls:__pkg__",
    ],
    deps = [":utf8"],
)

cc_test(
    name = "utf8_test",
    srcs = ["utf8_test.cc"],
    deps = [
        ":utf8",
        "@com_google_googletest//:gtest",
        "@com_google_googletest//:gtest_main",
    ],
)

cc_test(
    name = "line-column-map_test",
    srcs = ["line_column_map_test.cc"],
    deps = [
        ":line-column-map",
        "@com_google_absl//absl/strings",
        "@com_google_googletest//:gtest",
        "@com_google_googletest//:gtest_main",
    ],
)

cc_library(
    name = "string-memory-map",
    hdrs = ["string_memory_map.h"],
    deps = [
        ":range",
        "//common/util:interval-map",
        "//common/util:interval-set",
        "//common/util:logging",
    ],
)

cc_test(
    name = "string-memory-map_test",
    srcs = ["string_memory_map_test.cc"],
    deps = [
        ":range",
        ":string-memory-map",
        "//common/util:logging",
        "//common/util:range",
        "@com_google_googletest//:gtest",
        "@com_google_googletest//:gtest_main",
    ],
)
