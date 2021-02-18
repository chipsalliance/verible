# 'verilog_syntax' is a program for checking Verilog/SystemVerilog syntax.

licenses(["notice"])

cc_binary(
    name = "verible-verilog-syntax",
    srcs = ["verilog_syntax.cc"],
    visibility = ["//visibility:public"],  # for verilog_style_lint.bzl
    deps = [
        "//common/strings:compare",
        "//common/text:concrete_syntax_tree",
        "//common/text:parser_verifier",
        "//common/text:text_structure",
        "//common/text:token_info",
        "//common/text:token_info_json",
        "//common/util:bijective_map",
        "//common/util:enum_flags",
        "//common/util:file_util",
        "//common/util:init_command_line",
        "//common/util:logging",
        "//verilog/CST:verilog_tree_print",
        "//verilog/CST:verilog_tree_json",
        "//verilog/analysis:verilog_analyzer",
        "//verilog/analysis:json_diagnostics",
        "//verilog/analysis/checkers:verilog_lint_rules",
        "//verilog/parser:verilog_parser",
        "//verilog/parser:verilog_token",
        "@com_google_absl//absl/flags:flag",
        "@com_google_absl//absl/status",
        "@com_google_absl//absl/strings",
        "@com_google_absl//absl/types:span",
        "@jsoncpp_git//:jsoncpp",
    ],
)

sh_binary(
    name = "triage_parser",
    srcs = ["triage_parser.sh"],
)

sh_test(
    name = "verilog_syntax_test",
    size = "small",
    srcs = ["verilog_syntax_test.sh"],
    args = ["$(location :verible-verilog-syntax)"],
    data = [":verible-verilog-syntax"],
    deps = [],
)
