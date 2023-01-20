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
    name = "autoexpand",
    srcs = ["autoexpand.cc"],
    hdrs = ["autoexpand.h"],
    deps = [
        ":lsp-parse-buffer",
        "//common/lsp:lsp-protocol",
        "//common/text:text_structure",
        "//verilog/CST:identifier",
        "//verilog/CST:module",
        "//verilog/CST:port",
        "//verilog/formatting:format_style_init",
        "//verilog/formatting:formatter",
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
        ":autoexpand",
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

cc_library(
    name = "symbol-table-handler",
    srcs = ["symbol-table-handler.cc"],
    hdrs = ["symbol-table-handler.h"],
    deps = [
        "@com_google_absl//absl/container:flat_hash_set",
        "@com_google_absl//absl/flags:flag",
        "@com_google_absl//absl/strings",
        "//common/lsp:lsp-protocol",
        "//common/strings:line_column_map",
        "//common/util:file_util",
        "//verilog/analysis:symbol_table",
        "//verilog/analysis:verilog_filelist",
        "//verilog/analysis:verilog_project",
        ":lsp-parse-buffer"
    ],
)

cc_library(
    name = "verilog-language-server",
    srcs = ["verilog-language-server.cc"],
    hdrs = ["verilog-language-server.h"],
    visibility = ["//visibility:public"],
    deps = [
        ":lsp-parse-buffer",
        ":symbol-table-handler",
        ":verible-lsp-adapter",
        "//common/lsp:json-rpc-dispatcher",
        "//common/lsp:lsp-protocol",
        "//common/lsp:lsp-text-buffer",
        "//common/lsp:message-stream-splitter",
        "//common/util:init_command_line",
        "@com_google_absl//absl/status",
        "@com_google_absl//absl/strings",
    ],
)

cc_test(
    name = "verilog-language-server_test",
    srcs = ["verilog-language-server_test.cc"],
    deps = [
        ":verilog-language-server",
        "//common/lsp:lsp-protocol",
        "//common/lsp:lsp-protocol-enums",
        "@com_google_absl//absl/strings",
        "@com_google_googletest//:gtest_main",
    ],
)

cc_binary(
    name = "verible-verilog-ls",
    srcs = ["verilog_ls.cc"],
    visibility = ["//visibility:public"],
    deps = [
        ":verilog-language-server",
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

cc_test(
    name = "autoexpand_test",
    srcs = ["autoexpand_test.cc"],
    deps = [
        ":autoexpand",
        ":verible-lsp-adapter",
        "//common/lsp:lsp-protocol",
        "@com_google_googletest//:gtest_main",
    ],
)
