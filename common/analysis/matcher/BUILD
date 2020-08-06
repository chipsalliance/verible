# 'matcher' is a library of syntax tree matching utilities.

licenses(["notice"])

package(
    default_visibility = [
        "//common/analysis:__subpackages__",
        "//common/parser:__subpackages__",  # for parser_test_util
        "//verilog/CST:__subpackages__",
        "//verilog/analysis:__subpackages__",
    ],
)

cc_library(
    name = "bound_symbol_manager",
    srcs = ["bound_symbol_manager.cc"],
    hdrs = ["bound_symbol_manager.h"],
    deps = [
        "//common/text:symbol",
        "//common/util:casts",
        "//common/util:container_util",
        "//common/util:logging",
    ],
)

cc_library(
    name = "matcher",
    srcs = ["matcher.cc"],
    hdrs = ["matcher.h"],
    deps = [
        ":bound_symbol_manager",
        "//common/text:symbol",
        "@com_google_absl//absl/types:optional",
    ],
)

cc_test(
    name = "matcher_test",
    srcs = ["matcher_test.cc"],
    deps = [
        ":bound_symbol_manager",
        ":inner_match_handlers",
        ":matcher",
        ":matcher_builders",
        "//common/text:concrete_syntax_leaf",
        "//common/text:concrete_syntax_tree",
        "//common/text:symbol",
        "//common/text:tree_builder_test_util",
        "@com_google_googletest//:gtest_main",
    ],
)

cc_library(
    name = "core_matchers",
    hdrs = ["core_matchers.h"],
    deps = [
        ":inner_match_handlers",
        ":matcher",
        "//common/text:symbol",
    ],
)

cc_test(
    name = "core_matchers_test",
    srcs = ["core_matchers_test.cc"],
    deps = [
        ":bound_symbol_manager",
        ":core_matchers",
        ":matcher",
        ":matcher_builders",
        ":matcher_test_utils",
        "//common/text:symbol",
        "//common/text:tree_builder_test_util",
        "@com_google_googletest//:gtest_main",
    ],
)

cc_library(
    name = "inner_match_handlers",
    srcs = ["inner_match_handlers.cc"],
    hdrs = ["inner_match_handlers.h"],
    deps = [
        ":bound_symbol_manager",
        ":matcher",
        "//common/text:symbol",
        "//common/util:logging",
    ],
)

cc_library(
    name = "matcher_builders",
    hdrs = ["matcher_builders.h"],
    deps = [
        ":descent_path",
        ":inner_match_handlers",
        ":matcher",
        "//common/text:symbol",
    ],
)

cc_test(
    name = "matcher_builders_test",
    srcs = ["matcher_builders_test.cc"],
    deps = [
        ":bound_symbol_manager",
        ":matcher",
        ":matcher_builders",
        ":matcher_test_utils",
        "//common/text:concrete_syntax_leaf",
        "//common/text:concrete_syntax_tree",
        "//common/text:symbol",
        "//common/text:tree_builder_test_util",
        "@com_google_googletest//:gtest_main",
    ],
)

cc_library(
    name = "descent_path",
    srcs = ["descent_path.cc"],
    hdrs = ["descent_path.h"],
    deps = [
        "//common/text:concrete_syntax_tree",
        "//common/text:symbol",
    ],
)

cc_test(
    name = "descent_path_test",
    srcs = ["descent_path_test.cc"],
    deps = [
        ":descent_path",
        "//common/text:concrete_syntax_leaf",
        "//common/text:concrete_syntax_tree",
        "//common/text:symbol",
        "//common/text:token_info",
        "//common/text:tree_builder_test_util",
        "@com_google_googletest//:gtest_main",
    ],
)

cc_library(
    name = "matcher_test_utils",
    testonly = 1,
    srcs = ["matcher_test_utils.cc"],
    hdrs = ["matcher_test_utils.h"],
    deps = [
        ":bound_symbol_manager",
        ":matcher",
        "//common/text:concrete_syntax_leaf",
        "//common/text:concrete_syntax_tree",
        "//common/text:symbol",
        "//common/text:visitors",
        "@com_google_absl//absl/status",
        "@com_google_absl//absl/strings",
        "@com_google_googletest//:gtest",  # for library testonly
    ],
)
