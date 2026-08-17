"""
Rules for adding './configure && make' style dependencies.
"""

load("@bazel_skylib//rules:common_settings.bzl", "bool_flag")
load("@rules_cc//cc:defs.bzl", "cc_library")
load(":no_toolchain.bzl", "no_toolchain")

package(
    default_applicable_licenses = ["//:license"],
    default_visibility = [
        "//:__subpackages__",
    ],
    features = ["layering_check"],
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
    "no_toolchain.bzl",
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

# Placeholder used where a select() would otherwise need to switch a
# genrule's non-configurable `toolchains` attribute (see flex.bzl/bison.bzl):
no_toolchain(
    name = "no_toolchain",
)

bool_flag(
    name = "create_static_linked_executables",
    build_setting_default = False,
)

config_setting(
    name = "static_linked_executables",
    flag_values = {":create_static_linked_executables": "true"},
)
