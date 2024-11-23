"""Tool for preprocessing Verilog
"""

load("//bazel:sh_test_with_runfiles_lib.bzl", "sh_test_with_runfiles_lib")
load("//bazel:variables.bzl", "STATIC_EXECUTABLES_FEATURE")

package(
    default_applicable_licenses = ["//:license"],
    default_visibility = ["//visibility:private"],
    features = ["layering_check"],
)

cc_binary(
    name = "verible-verilog-preprocessor",
    srcs = ["verilog-preprocessor.cc"],
    features = STATIC_EXECUTABLES_FEATURE,
    visibility = ["//visibility:public"],
    deps = [
        "//verible/common/text:token-stream-view",
        "//verible/common/util:file-util",
        "//verible/common/util:init-command-line",
        "//verible/common/util:status-macros",
        "//verible/common/util:subcommand",
        "//verible/verilog/analysis:flow-tree",
        "//verible/verilog/analysis:verilog-filelist",
        "//verible/verilog/analysis:verilog-project",
        "//verible/verilog/parser:verilog-lexer",
        "//verible/verilog/preprocessor:verilog-preprocess",
        "//verible/verilog/transform:strip-comments",
        "@com_google_absl//absl/flags:flag",
        "@com_google_absl//absl/flags:usage",
        "@com_google_absl//absl/status",
        "@com_google_absl//absl/status:statusor",
        "@com_google_absl//absl/strings",
        "@com_google_absl//absl/strings:string_view",
    ],
)

sh_test_with_runfiles_lib(
    name = "verilog-preprocessor_test",
    size = "small",
    srcs = ["verilog_preprocessor_test.sh"],
    args = ["$(location :verible-verilog-preprocessor)"],
    data = [":verible-verilog-preprocessor"],
    deps = [],
)
