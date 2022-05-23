# This package contains a SystemVerilog language server [1] implementation.
#
# [1]: https://microsoft.github.io/language-server-protocol/specification

load("//bazel:sh_test_with_runfiles_lib.bzl", "sh_test_with_runfiles_lib")

licenses(["notice"])

package(
    default_visibility = [
        "//:__subpackages__",
    ],
)

cc_library(
    name = "lsp-parse-buffer",
    srcs = ["lsp-parse-buffer.cc"],
    hdrs = ["lsp-parse-buffer.h"],
    deps = [
        "//common/lsp:lsp-text-buffer",
        "//common/util:logging",
        "//verilog/analysis:verilog_analyzer",
        "//verilog/analysis:verilog_linter",
        "@com_google_absl//absl/status",
    ],
)

cc_library(
    name = "verible-lsp-adapter",
    srcs = ["verible-lsp-adapter.cc"],
    hdrs = ["verible-lsp-adapter.h"],
    deps = [
        ":document-symbol-filler",
        ":lsp-parse-buffer",
        "//common/lsp:lsp-protocol",
        "//common/lsp:lsp-protocol-operators",
        "//common/text:text_structure",
        "//verilog/analysis:verilog_analyzer",
        "//verilog/analysis:verilog_linter",
        "//verilog/formatting:format_style_init",
        "//verilog/formatting:formatter",
        "//verilog/parser:verilog_token_enum",
        "@jsonhpp",
    ],
)

cc_library(
    name = "document-symbol-filler",
    srcs = ["document-symbol-filler.cc"],
    hdrs = ["document-symbol-filler.h"],
    deps = [
        "//common/lsp:lsp-protocol",
        "//common/lsp:lsp-protocol-enums",
        "//common/text:text_structure",
        "//common/text:visitors",
        "//common/util:value_saver",
        "//verilog/CST:class",
        "//verilog/CST:functions",
        "//verilog/CST:module",
        "//verilog/CST:package",
        "//verilog/CST:seq_block",
    ],
)

cc_binary(
    name = "verible-verilog-ls",
    srcs = ["verilog_ls.cc"],
    visibility = ["//visibility:public"],
    deps = [
        ":lsp-parse-buffer",
        ":verible-lsp-adapter",
        "//common/lsp:json-rpc-dispatcher",
        "//common/lsp:lsp-protocol",
        "//common/lsp:message-stream-splitter",
        "//common/util:init_command_line",
    ],
)

sh_test_with_runfiles_lib(
    name = "verible-verilog-ls_test",
    size = "small",
    srcs = ["verible-verilog-ls_test.sh"],
    args = [
        "$(location :verible-verilog-ls)",
        "$(location //common/lsp:json-rpc-expect)",
    ],
    data = [
        ":verible-verilog-ls",
        "//common/lsp:json-rpc-expect",
    ],
    deps = [],
)
