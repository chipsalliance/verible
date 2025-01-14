# 'matcher' is a library of syntax tree matching utilities.

package(
    default_applicable_licenses = ["//:license"],
    default_visibility = [
        "//verible/common/analysis:__subpackages__",
        "//verible/common/parser:__subpackages__",  # for parser_test_util
        "//verible/verilog/CST:__subpackages__",
        "//verible/verilog/analysis:__subpackages__",
    ],
    features = ["layering_check"],
)

cc_library(
    name = "bound-symbol-manager",
    srcs = ["bound-symbol-manager.cc"],
    hdrs = ["bound-symbol-manager.h"],
    deps = [
        "//verible/common/text:symbol",
        "//verible/common/util:casts",
        "//verible/common/util:container-util",
        "//verible/common/util:logging",
    ],
)

cc_library(
    name = "matcher",
    srcs = ["matcher.cc"],
    hdrs = ["matcher.h"],
    deps = [
        ":bound-symbol-manager",
        "//verible/common/text:symbol",
        "@abseil-cpp//absl/types:optional",
    ],
)

cc_test(
    name = "matcher_test",
    srcs = ["matcher_test.cc"],
    deps = [
        ":bound-symbol-manager",
        ":inner-match-handlers",
        ":matcher",
        ":matcher-builders",
        "//verible/common/text:concrete-syntax-leaf",
        "//verible/common/text:concrete-syntax-tree",
        "//verible/common/text:symbol",
        "//verible/common/text:tree-builder-test-util",
        "//verible/common/util:casts",
        "@googletest//:gtest",
        "@googletest//:gtest_main",
    ],
)

cc_library(
    name = "core-matchers",
    hdrs = ["core-matchers.h"],
    deps = [
        ":inner-match-handlers",
        ":matcher",
        "//verible/common/text:symbol",
    ],
)

cc_test(
    name = "core-matchers_test",
    srcs = ["core-matchers_test.cc"],
    deps = [
        ":bound-symbol-manager",
        ":core-matchers",
        ":matcher",
        ":matcher-builders",
        ":matcher-test-utils",
        "//verible/common/text:symbol",
        "//verible/common/text:tree-builder-test-util",
        "@googletest//:gtest",
        "@googletest//:gtest_main",
    ],
)

cc_library(
    name = "inner-match-handlers",
    srcs = ["inner-match-handlers.cc"],
    hdrs = ["inner-match-handlers.h"],
    deps = [
        ":bound-symbol-manager",
        ":matcher",
        "//verible/common/text:symbol",
        "//verible/common/util:logging",
    ],
)

cc_library(
    name = "matcher-builders",
    hdrs = ["matcher-builders.h"],
    deps = [
        ":descent-path",
        ":inner-match-handlers",
        ":matcher",
        "//verible/common/text:symbol",
    ],
)

cc_test(
    name = "matcher-builders_test",
    srcs = ["matcher-builders_test.cc"],
    deps = [
        ":bound-symbol-manager",
        ":matcher",
        ":matcher-builders",
        ":matcher-test-utils",
        "//verible/common/text:concrete-syntax-leaf",
        "//verible/common/text:concrete-syntax-tree",
        "//verible/common/text:symbol",
        "//verible/common/text:tree-builder-test-util",
        "//verible/common/util:casts",
        "@googletest//:gtest",
        "@googletest//:gtest_main",
    ],
)

cc_library(
    name = "descent-path",
    srcs = ["descent-path.cc"],
    hdrs = ["descent-path.h"],
    deps = [
        "//verible/common/text:concrete-syntax-tree",
        "//verible/common/text:symbol",
        "//verible/common/util:casts",
    ],
)

cc_test(
    name = "descent-path_test",
    srcs = ["descent-path_test.cc"],
    deps = [
        ":descent-path",
        "//verible/common/text:concrete-syntax-leaf",
        "//verible/common/text:concrete-syntax-tree",
        "//verible/common/text:symbol",
        "//verible/common/text:token-info",
        "//verible/common/text:tree-builder-test-util",
        "//verible/common/util:casts",
        "@googletest//:gtest",
        "@googletest//:gtest_main",
    ],
)

cc_library(
    name = "matcher-test-utils",
    testonly = 1,
    srcs = ["matcher-test-utils.cc"],
    hdrs = ["matcher-test-utils.h"],
    deps = [
        ":bound-symbol-manager",
        ":matcher",
        "//verible/common/text:concrete-syntax-leaf",
        "//verible/common/text:concrete-syntax-tree",
        "//verible/common/text:symbol",
        "//verible/common/text:tree-utils",
        "//verible/common/text:visitors",
        "@abseil-cpp//absl/status",
        "@googletest//:gtest",  # for library testonly
    ],
)
