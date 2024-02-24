# 'matcher' is a library of syntax tree matching utilities.

package(
    default_applicable_licenses = ["//:license"],
    default_visibility = [
        "//common/analysis:__subpackages__",
        "//common/parser:__subpackages__",  # for parser_test_util
        "//verilog/CST:__subpackages__",
        "//verilog/analysis:__subpackages__",
    ],
    features = ["layering_check"],
)

cc_library(
    name = "bound-symbol-manager",
    srcs = ["bound_symbol_manager.cc"],
    hdrs = ["bound_symbol_manager.h"],
    deps = [
        "//common/text:symbol",
        "//common/util:casts",
        "//common/util:container-util",
        "//common/util:logging",
    ],
)

cc_library(
    name = "matcher",
    srcs = ["matcher.cc"],
    hdrs = ["matcher.h"],
    deps = [
        ":bound-symbol-manager",
        "//common/text:symbol",
        "@com_google_absl//absl/types:optional",
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
        "//common/text:concrete-syntax-leaf",
        "//common/text:concrete-syntax-tree",
        "//common/text:symbol",
        "//common/text:tree-builder-test-util",
        "//common/util:casts",
        "@com_google_googletest//:gtest",
        "@com_google_googletest//:gtest_main",
    ],
)

cc_library(
    name = "core-matchers",
    hdrs = ["core_matchers.h"],
    deps = [
        ":inner-match-handlers",
        ":matcher",
        "//common/text:symbol",
    ],
)

cc_test(
    name = "core-matchers_test",
    srcs = ["core_matchers_test.cc"],
    deps = [
        ":bound-symbol-manager",
        ":core-matchers",
        ":matcher",
        ":matcher-builders",
        ":matcher-test-utils",
        "//common/text:symbol",
        "//common/text:tree-builder-test-util",
        "@com_google_googletest//:gtest",
        "@com_google_googletest//:gtest_main",
    ],
)

cc_library(
    name = "inner-match-handlers",
    srcs = ["inner_match_handlers.cc"],
    hdrs = ["inner_match_handlers.h"],
    deps = [
        ":bound-symbol-manager",
        ":matcher",
        "//common/text:symbol",
        "//common/util:logging",
    ],
)

cc_library(
    name = "matcher-builders",
    hdrs = ["matcher_builders.h"],
    deps = [
        ":descent-path",
        ":inner-match-handlers",
        ":matcher",
        "//common/text:symbol",
    ],
)

cc_test(
    name = "matcher-builders_test",
    srcs = ["matcher_builders_test.cc"],
    deps = [
        ":bound-symbol-manager",
        ":matcher",
        ":matcher-builders",
        ":matcher-test-utils",
        "//common/text:concrete-syntax-leaf",
        "//common/text:concrete-syntax-tree",
        "//common/text:symbol",
        "//common/text:tree-builder-test-util",
        "//common/util:casts",
        "@com_google_googletest//:gtest",
        "@com_google_googletest//:gtest_main",
    ],
)

cc_library(
    name = "descent-path",
    srcs = ["descent_path.cc"],
    hdrs = ["descent_path.h"],
    deps = [
        "//common/text:concrete-syntax-tree",
        "//common/text:symbol",
        "//common/util:casts",
    ],
)

cc_test(
    name = "descent-path_test",
    srcs = ["descent_path_test.cc"],
    deps = [
        ":descent-path",
        "//common/text:concrete-syntax-leaf",
        "//common/text:concrete-syntax-tree",
        "//common/text:symbol",
        "//common/text:token-info",
        "//common/text:tree-builder-test-util",
        "//common/util:casts",
        "@com_google_googletest//:gtest",
        "@com_google_googletest//:gtest_main",
    ],
)

cc_library(
    name = "matcher-test-utils",
    testonly = 1,
    srcs = ["matcher_test_utils.cc"],
    hdrs = ["matcher_test_utils.h"],
    deps = [
        ":bound-symbol-manager",
        ":matcher",
        "//common/text:concrete-syntax-leaf",
        "//common/text:concrete-syntax-tree",
        "//common/text:symbol",
        "//common/text:tree-utils",
        "//common/text:visitors",
        "@com_google_absl//absl/status",
        "@com_google_absl//absl/strings:string_view",
        "@com_google_googletest//:gtest",  # for library testonly
    ],
)
