load("@com_google_verible//bazel:win_flex_bison.bzl", "win_bison_toolchain", "win_flex_toolchain")

filegroup(
    name = "win_flex",
    srcs = ["win_flex.exe"],
    visibility = ["//visibility:public"],
)

filegroup(
    name = "flex_lexer_h",
    srcs = ["FlexLexer.h"],
    visibility = ["//visibility:public"],
)

filegroup(
    name = "win_bison",
    srcs = ["win_bison.exe"],
    visibility = ["//visibility:public"],
)

win_flex_toolchain(
    name = "flex",
    flex_lexer_h = ":flex_lexer_h",
    flex_tool = ":win_flex",
    visibility = ["//visibility:public"],
)

toolchain(
    name = "flex_toolchain",
    exec_compatible_with = ["@platforms//os:windows"],
    target_compatible_with = ["@platforms//os:windows"],
    toolchain = ":flex",
    toolchain_type = "@rules_flex//flex:toolchain_type",
)

win_bison_toolchain(
    name = "bison",
    bison_tool = ":win_bison",
    visibility = ["//visibility:public"],
)

toolchain(
    name = "bison_toolchain",
    exec_compatible_with = ["@platforms//os:windows"],
    target_compatible_with = ["@platforms//os:windows"],
    toolchain = ":bison",
    toolchain_type = "@rules_bison//bison:toolchain_type",
)
