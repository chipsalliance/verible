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
    srcs = [ "lsp-parse-buffer.cc"],
    hdrs = [ "lsp-parse-buffer.h" ],
    deps = [
      "//verilog/analysis:verilog_analyzer",
      "//verilog/analysis:verilog_linter",
      "//common/lsp:lsp-text-buffer",
    ]
)

cc_binary(
    name = "verible-verilog-ls",
    srcs = ["verilog_ls.cc", "lsp-parse-buffer.h"],
    deps = [
      ":lsp-parse-buffer",
      "//common/lsp:lsp-protocol",
      "//common/lsp:json-rpc-dispatcher",
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
