"""
Rules for adding './configure && make' style dependencies.
"""

load("@bazel_skylib//rules:common_settings.bzl", "bool_flag")
load("@com_github_google_rules_install//installer:def.bzl", "installer")

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
)

config_setting(
    name = "use_local_flex_bison_enabled",
    flag_values = {":use_local_flex_bison": "true"},
)

bool_flag(
    name = "create_static_linked_executables",
    build_setting_default = False,
)

config_setting(
    name = "static_linked_executables",
    flag_values = {":create_static_linked_executables": "true"},
)

# This installer is here, so that the toplevel BUILD file does not have
# to pull in the full rules_install to keep that light of dependencies.
installer(
    name = "install",
    data = [
        "//:install-binaries",
        "//:install-scripts",
    ],
)
