"""Tool for comparing Verilog source code.
"""

licenses(["notice"])

cc_binary(
    name = "verilog_diff",
    srcs = ["verilog_diff.cc"],
    visibility = ["//visibility:public"],
    deps = [
        "//common/strings:obfuscator",
        "//common/util:enum_flags",
        "//common/util:file_util",
        "//common/util:init_command_line",
        "//verilog/analysis:verilog_equivalence",
        "@com_google_absl//absl/flags:flag",
        "@com_google_absl//absl/strings",
    ],
)
