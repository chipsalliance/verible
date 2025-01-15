# This package contains externally owned libraries.
#
# editscript - is a template-generic sequence diff algorithm based on
#     diff-match-patch.

package(
    default_applicable_licenses = ["//:license"],
    default_visibility = [
        "//:__subpackages__",
    ],
    features = ["layering_check"],
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
        "@abseil-cpp//absl/strings",
        "@googletest//:gtest",
        "@googletest//:gtest_main",
    ],
)
