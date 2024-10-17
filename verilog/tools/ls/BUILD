# This package contains a SystemVerilog language server [1] implementation.
#
# [1]: https://microsoft.github.io/language-server-protocol/specification

load("//bazel:sh_test_with_runfiles_lib.bzl", "sh_test_with_runfiles_lib")
load("//bazel:variables.bzl", "STATIC_EXECUTABLES_FEATURE")

package(
    default_applicable_licenses = ["//:license"],
    default_visibility = ["//visibility:private"],
    features = ["layering_check"],
)

cc_library(
    name = "autoexpand",
    srcs = ["autoexpand.cc"],
    hdrs = ["autoexpand.h"],
    deps = [
        ":lsp-parse-buffer",
        ":symbol-table-handler",
        "//common/analysis:syntax-tree-search",
        "//common/lsp:lsp-protocol",
        "//common/strings:line-column-map",
        "//common/strings:position",
        "//common/text:symbol",
        "//common/text:text-structure",
        "//common/text:token-info",
        "//common/text:tree-utils",
        "//common/util:logging",
        "//verilog/CST:declaration",
        "//verilog/CST:dimensions",
        "//verilog/CST:module",
        "//verilog/CST:net",
        "//verilog/CST:port",
        "//verilog/CST:type",
        "//verilog/CST:verilog-matchers",
        "//verilog/CST:verilog-nonterminals",
        "//verilog/analysis:verilog-analyzer",
        "//verilog/formatting:format-style",
        "//verilog/formatting:format-style-init",
        "//verilog/formatting:formatter",
        "@com_google_absl//absl/container:flat_hash_map",
        "@com_google_absl//absl/container:flat_hash_set",
        "@com_google_absl//absl/container:node_hash_map",
        "@com_google_absl//absl/status",
        "@com_google_absl//absl/strings",
        "@com_google_absl//absl/strings:string_view",
        "@com_googlesource_code_re2//:re2",
    ],
)

cc_library(
    name = "lsp-parse-buffer",
    srcs = ["lsp-parse-buffer.cc"],
    hdrs = ["lsp-parse-buffer.h"],
    deps = [
        "//common/analysis:lint-rule-status",
        "//common/lsp:lsp-file-utils",
        "//common/lsp:lsp-text-buffer",
        "//common/util:logging",
        "//verilog/analysis:verilog-analyzer",
        "//verilog/analysis:verilog-linter",
        "//verilog/analysis:verilog-linter-configuration",
        "@com_google_absl//absl/status",
        "@com_google_absl//absl/status:statusor",
        "@com_google_absl//absl/strings:string_view",
    ],
)

cc_test(
    name = "lsp-parse-buffer_test",
    srcs = ["lsp-parse-buffer_test.cc"],
    deps = [
        ":lsp-parse-buffer",
        "//common/lsp:lsp-text-buffer",
        "//common/text:text-structure",
        "@com_google_absl//absl/strings",
        "@com_google_googletest//:gtest",
        "@com_google_googletest//:gtest_main",
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
        "//common/analysis:file-analyzer",
        "//common/analysis:lint-rule-status",
        "//common/lsp:lsp-protocol",
        "//common/lsp:lsp-protocol-enums",
        "//common/lsp:lsp-protocol-operators",
        "//common/strings:line-column-map",
        "//common/text:text-structure",
        "//common/text:token-info",
        "//common/util:interval",
        "//verilog/analysis:verilog-analyzer",
        "//verilog/analysis:verilog-linter",
        "//verilog/formatting:format-style",
        "//verilog/formatting:format-style-init",
        "//verilog/formatting:formatter",
        "//verilog/parser:verilog-token-enum",
        "@com_google_absl//absl/strings",
        "@com_google_absl//absl/strings:string_view",
        "@jsonhpp//:singleheader-json",
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
        "//common/text:token-info",
        "//common/text:tree-utils",
        "//common/text:visitors",
        "//common/util:value-saver",
        "//verilog/CST:class",
        "//verilog/CST:functions",
        "//verilog/CST:module",
        "//verilog/CST:package",
        "//verilog/CST:seq-block",
        "//verilog/CST:verilog-nonterminals",
        "@jsonhpp//:singleheader-json",
    ],
)

cc_library(
    name = "hover",
    srcs = ["hover.cc"],
    hdrs = ["hover.h"],
    deps = [
        ":lsp-parse-buffer",
        ":symbol-table-handler",
        "//common/lsp:lsp-protocol",
        "//common/text:concrete-syntax-leaf",
        "//common/text:concrete-syntax-tree",
        "//common/text:symbol",
        "//common/text:token-info",
        "//common/text:tree-context-visitor",
        "//common/text:tree-utils",
        "//common/util:casts",
        "//common/util:range",
        "//verilog/CST:seq-block",
        "//verilog/CST:verilog-nonterminals",
        "//verilog/analysis:symbol-table",
        "//verilog/parser:verilog-token-enum",
        "@com_google_absl//absl/strings",
        "@com_google_absl//absl/strings:string_view",
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
        "//common/text:symbol",
        "//common/text:text-structure",
        "//common/text:token-info",
        "//common/util:file-util",
        "//common/util:iterator-adaptors",
        "//common/util:logging",
        "//common/util:range",
        "//verilog/analysis:symbol-table",
        "//verilog/analysis:verilog-analyzer",
        "//verilog/analysis:verilog-filelist",
        "//verilog/analysis:verilog-project",
        "@com_google_absl//absl/container:flat_hash_map",
        "@com_google_absl//absl/flags:flag",
        "@com_google_absl//absl/log",
        "@com_google_absl//absl/status",
        "@com_google_absl//absl/status:statusor",
        "@com_google_absl//absl/strings:str_format",
        "@com_google_absl//absl/strings:string_view",
        "@com_google_absl//absl/time",
        "@com_google_absl//absl/types:optional",
    ],
)

cc_test(
    name = "symbol-table-handler_test",
    srcs = ["symbol-table-handler_test.cc"],
    deps = [
        ":lsp-parse-buffer",
        ":symbol-table-handler",
        "//common/lsp:lsp-file-utils",
        "//common/lsp:lsp-protocol",
        "//common/lsp:lsp-text-buffer",
        "//common/util:file-util",
        "//verilog/analysis:verilog-project",
        "@com_google_absl//absl/status",
        "@com_google_absl//absl/strings:string_view",
        "@com_google_googletest//:gtest",
        "@com_google_googletest//:gtest_main",
    ],
)

cc_library(
    name = "verilog-language-server",
    srcs = ["verilog-language-server.cc"],
    hdrs = ["verilog-language-server.h"],
    deps = [
        ":hover",
        ":lsp-parse-buffer",
        ":symbol-table-handler",
        ":verible-lsp-adapter",
        "//common/lsp:json-rpc-dispatcher",
        "//common/lsp:lsp-file-utils",
        "//common/lsp:lsp-protocol",
        "//common/lsp:lsp-text-buffer",
        "//common/lsp:message-stream-splitter",
        "//common/util:file-util",
        "//common/util:init-command-line",
        "//common/util:logging",
        "//verilog/analysis:verilog-project",
        "@com_google_absl//absl/flags:flag",
        "@com_google_absl//absl/status",
        "@com_google_absl//absl/strings:string_view",
        "@jsonhpp//:singleheader-json",
    ],
)

cc_test(
    name = "verilog-language-server_test",
    srcs = ["verilog-language-server_test.cc"],
    deps = [
        ":verilog-language-server",
        "//common/lsp:lsp-file-utils",
        "//common/lsp:lsp-protocol",
        "//common/lsp:lsp-protocol-enums",
        "//common/strings:line-column-map",
        "//common/util:file-util",
        "//verilog/analysis:verilog-linter",
        "@com_google_absl//absl/flags:flag",
        "@com_google_absl//absl/status",
        "@com_google_absl//absl/strings",
        "@com_google_absl//absl/strings:string_view",
        "@com_google_googletest//:gtest",
        "@com_google_googletest//:gtest_main",
        "@jsonhpp//:singleheader-json",
    ],
)

cc_binary(
    name = "verible-verilog-ls",
    srcs = ["verible-verilog-ls.cc"],
    features = STATIC_EXECUTABLES_FEATURE,
    visibility = ["//visibility:public"],
    deps = [
        ":verilog-language-server",
        "//common/util:init-command-line",
        "@com_google_absl//absl/status",
        "@com_google_absl//absl/strings:string_view",
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
        ":lsp-parse-buffer",
        ":symbol-table-handler",
        "//common/lsp:lsp-protocol",
        "//common/lsp:lsp-text-buffer",
        "//common/text:text-structure",
        "//verilog/analysis:verilog-analyzer",
        "//verilog/analysis:verilog-project",
        "//verilog/formatting:format-style",
        "//verilog/formatting:format-style-init",
        "//verilog/formatting:formatter",
        "@com_google_absl//absl/status",
        "@com_google_absl//absl/strings",
        "@com_google_absl//absl/strings:string_view",
        "@com_google_googletest//:gtest",
        "@com_google_googletest//:gtest_main",
        "@jsonhpp//:singleheader-json",
    ],
)
