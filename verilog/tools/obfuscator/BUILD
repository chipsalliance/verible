"""Tool for obfuscating verilog
"""

load("//bazel:sh_test_with_runfiles_lib.bzl", "sh_test_with_runfiles_lib")
load("//bazel:variables.bzl", "STATIC_EXECUTABLES_FEATURE")

package(
    default_applicable_licenses = ["//:license"],
    default_visibility = ["//visibility:private"],
)

cc_binary(
    name = "verible-verilog-obfuscate",
    srcs = ["verilog_obfuscate.cc"],
    features = STATIC_EXECUTABLES_FEATURE,
    visibility = ["//visibility:public"],
    deps = [
        "//common/strings:obfuscator",
        "//common/util:file-util",
        "//common/util:init-command-line",
        "//verilog/analysis:extractors",
        "//verilog/analysis:verilog-analyzer",
        "//verilog/transform:obfuscate",
        "@com_google_absl//absl/flags:flag",
        "@com_google_absl//absl/strings",
    ],
)

sh_test_with_runfiles_lib(
    name = "obfuscate_test",
    size = "small",
    srcs = ["obfuscate_test.sh"],
    args = [
        "$(location :verible-verilog-obfuscate)",
        "$(location //verilog/tools/diff:verible-verilog-diff)",
    ],
    data = [
        ":verible-verilog-obfuscate",
        "//verilog/tools/diff:verible-verilog-diff",
    ],
)
