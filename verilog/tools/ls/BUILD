# This package contains a SystemVerilog language server [1] implementation.
#
# [1]: https://microsoft.github.io/language-server-protocol/specification

load("//bazel:sh_test_with_runfiles_lib.bzl", "sh_test_with_runfiles_lib")
load("//bazel:variables.bzl", "STATIC_EXECUTABLES_FEATURE")

package(
    default_applicable_licenses = ["//:license"],
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
        ":symbol-table-handler",
        "//common/lsp:lsp-protocol",
        "//common/text:text-structure",
        "//verilog/CST:declaration",
        "//verilog/CST:dimensions",
        "//verilog/CST:expression",
        "//verilog/CST:identifier",
        "//verilog/CST:module",
        "//verilog/CST:net",
        "//verilog/CST:port",
        "//verilog/CST:type",
        "//verilog/CST:verilog-matchers",
        "//verilog/analysis:symbol-table",
        "//verilog/formatting:format-style-init",
        "//verilog/formatting:formatter",
        "@com_google_absl//absl/container:flat_hash_map",
        "@com_google_absl//absl/container:flat_hash_set",
        "@com_google_absl//absl/container:node_hash_map",
        "@com_googlesource_code_re2//:re2",
    ],
)

cc_library(
    name = "lsp-parse-buffer",
    srcs = ["lsp-parse-buffer.cc"],
    hdrs = ["lsp-parse-buffer.h"],
    deps = [
        "//common/lsp:lsp-file-utils",
        "//common/lsp:lsp-text-buffer",
        "//common/util:logging",
        "//verilog/analysis:verilog-analyzer",
        "//verilog/analysis:verilog-linter",
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
        ":symbol-table-handler",
        "//common/lsp:lsp-protocol",
        "//common/lsp:lsp-protocol-enums",
        "//common/lsp:lsp-protocol-operators",
        "//common/text:text-structure",
        "//verilog/analysis:verilog-analyzer",
        "//verilog/analysis:verilog-linter",
        "//verilog/formatting:format-style-init",
        "//verilog/formatting:formatter",
        "//verilog/parser:verilog-token-enum",
        "@jsonhpp",
    ],
)

cc_library(
    name = "lsp-conversion",
    hdrs = ["lsp-conversion.h"],
    deps = [
        "//common/lsp:lsp-protocol",
        "//common/strings:line-column-map",
    ],
)

cc_library(
    name = "document-symbol-filler",
    srcs = ["document-symbol-filler.cc"],
    hdrs = ["document-symbol-filler.h"],
    deps = [
        ":lsp-conversion",
        "//common/lsp:lsp-protocol",
        "//common/lsp:lsp-protocol-enums",
        "//common/text:text-structure",
        "//common/text:visitors",
        "//common/util:value-saver",
        "//verilog/CST:class",
        "//verilog/CST:functions",
        "//verilog/CST:module",
        "//verilog/CST:package",
        "//verilog/CST:seq-block",
    ],
)

cc_library(
    name = "hover",
    srcs = ["hover.cc"],
    hdrs = ["hover.h"],
    deps = [
        ":lsp-parse-buffer",
        ":symbol-table-handler",
        "//common/util:casts",
        "//verilog/CST:seq_block",
        "//common/lsp:lsp-protocol",
        "//verilog/CST:verilog_nonterminals",
        "//common/text:tree_context_visitor",
        "//verilog/parser:verilog_token_enum",
    ],
)

cc_library(
    name = "symbol-table-handler",
    srcs = ["symbol-table-handler.cc"],
    hdrs = ["symbol-table-handler.h"],
    deps = [
        ":lsp-conversion",
        ":lsp-parse-buffer",
        "//common/lsp:lsp-file-utils",
        "//common/lsp:lsp-protocol",
        "//common/strings:line-column-map",
        "//common/util:file-util",
        "//common/util:range",
        "//verilog/analysis:symbol-table",
        "//verilog/analysis:verilog-filelist",
        "//verilog/analysis:verilog-project",
        "@com_google_absl//absl/container:flat_hash_set",
        "@com_google_absl//absl/flags:flag",
        "@com_google_absl//absl/strings",
        "@com_google_absl//absl/time",
        "@com_google_absl//absl/types:optional",
    ],
)

cc_test(
    name = "symbol-table-handler_test",
    srcs = ["symbol-table-handler_test.cc"],
    deps = [
        ":symbol-table-handler",
        "//common/util:file-util",
        "//verilog/analysis:verilog-project",
        "@com_google_absl//absl/strings",
        "@com_google_googletest//:gtest_main",
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
        ":hover",
        "//common/lsp:json-rpc-dispatcher",
        "//common/lsp:lsp-file-utils",
        "//common/lsp:lsp-protocol",
        "//common/lsp:lsp-text-buffer",
        "//common/lsp:message-stream-splitter",
        "//common/util:file-util",
        "//common/util:init-command-line",
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
        "//common/util:file-util",
        "//verilog/analysis:verilog-linter",
        "@com_google_absl//absl/flags:flag",
        "@com_google_absl//absl/strings",
        "@com_google_googletest//:gtest_main",
    ],
)

cc_binary(
    name = "verible-verilog-ls",
    srcs = ["verilog_ls.cc"],
    features = STATIC_EXECUTABLES_FEATURE,
    visibility = ["//visibility:public"],
    deps = [
        ":verilog-language-server",
        "//common/util:init-command-line",
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
        "//verilog/formatting:format-style-init",
        "//verilog/formatting:formatter",
        "@com_google_googletest//:gtest_main",
    ],
)
