# 'verilog_syntax' is a program for checking Verilog/SystemVerilog syntax.

licenses(["notice"])

cc_binary(
    name = "verilog_syntax",
    srcs = ["verilog_syntax.cc"],
    visibility = ["//visibility:public"],  # for verilog_style_lint.bzl
    deps = [
        "//common/text:concrete_syntax_tree",
        "//common/text:parser_verifier",
        "//common/text:text_structure",
        "//common/text:token_info",
        "//common/util:file_util",
        "//common/util:logging",
        "//common/util:status",
        "//verilog/CST:verilog_tree_print",
        "//verilog/analysis:verilog_analyzer",
        "//verilog/analysis/checkers:verilog_lint_rules",
        "//verilog/parser:verilog_parser",
        "@com_google_absl//absl/flags:flag",
        "@com_google_absl//absl/flags:parse",
        "@com_google_absl//absl/flags:usage",
        "@com_google_absl//absl/strings",
        "@com_google_absl//absl/types:span",
    ],
)
