"""
Rules for adding './configure && make' style dependencies.
"""

load("@bazel_skylib//rules:common_settings.bzl", "bool_flag")

licenses(["notice"])  # Apache v2.0

package(
    default_visibility = [
        "//:__subpackages__",
    ],
)

cc_library(
    name = "flex",
    deps = [
        "@rules_flex//flex:current_flex_toolchain",  # For FlexLexer.h
    ],
)

exports_files([
    "bison.bzl",
    "flex.bzl",
    "sh_test_with_runfiles_lib.bzl",
    "sh_test_with_runfiles_lib.sh",
])

bool_flag(
    name = "use_local_flex_bison",
    build_setting_default = False,
)

config_setting(
    name = "use_local_flex_bison_enabled",
    flag_values = {":use_local_flex_bison": "true"},
)
