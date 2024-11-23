"""Tool for obfuscating verilog
"""

load("//bazel:sh_test_with_runfiles_lib.bzl", "sh_test_with_runfiles_lib")
load("//bazel:variables.bzl", "STATIC_EXECUTABLES_FEATURE")

package(
    default_applicable_licenses = ["//:license"],
    default_visibility = ["//visibility:private"],
    features = ["layering_check"],
)

cc_binary(
    name = "verible-verilog-obfuscate",
    srcs = ["verilog-obfuscate.cc"],
    features = STATIC_EXECUTABLES_FEATURE,
    visibility = ["//visibility:public"],
    deps = [
        "//common/strings:obfuscator",
        "//common/util:file-util",
        "//common/util:init-command-line",
        "//verilog/analysis:extractors",
        "//verilog/preprocessor:verilog-preprocess",
        "//verilog/transform:obfuscate",
        "@com_google_absl//absl/flags:flag",
        "@com_google_absl//absl/status",
        "@com_google_absl//absl/status:statusor",
        "@com_google_absl//absl/strings",
        "@com_google_absl//absl/strings:string_view",
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
