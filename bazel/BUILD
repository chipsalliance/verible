"""
Rules for adding './configure && make' style dependencies.
"""

load("@bazel_skylib//rules:common_settings.bzl", "bool_flag")
load("@rules_cc//cc:defs.bzl", "cc_library")

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
    "sh_test_with_runfiles_lib.bzl",
    "sh_test_with_runfiles_lib.sh",
])

bool_flag(
    name = "use_local_flex_bison",
    build_setting_default = False,
    deprecation = "This flag has been disabled; see https://github.com/chipsalliance/verible/issues/2435 if you want to help fix re-enable it.",
)

bool_flag(
    name = "create_static_linked_executables",
    build_setting_default = False,
)

config_setting(
    name = "static_linked_executables",
    flag_values = {":create_static_linked_executables": "true"},
)
