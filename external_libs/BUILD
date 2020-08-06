# This package contains externally owned libraries.
#
# editscript - is a template-generic sequence diff algorithm based on
#     diff-match-patch.

licenses(["notice"])

package(
    default_visibility = [
        "//:__subpackages__",
    ],
)

cc_library(
    name = "editscript",
    hdrs = ["editscript.h"],
)

cc_test(
    name = "editscript_test",
    srcs = ["editscript_test.cc"],
    deps = [
        ":editscript",
        "@com_google_absl//absl/strings",
        "@com_google_googletest//:gtest_main",
    ],
)
