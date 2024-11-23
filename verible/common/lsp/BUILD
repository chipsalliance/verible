# This package contains the basic language server protocol [1] implementation
# needed in language specific parts to provide LSP services.
# LSP is using JSON-PRC [2] for the RPC protocol, the transport layer chunks
# messages with header+body blocks, similar to HTTP.
#
# [1]: https://microsoft.github.io/language-server-protocol/specification
# [2]: https://www.jsonrpc.org/specification

load("//bazel:sh_test_with_runfiles_lib.bzl", "sh_test_with_runfiles_lib")

package(
    default_applicable_licenses = ["//:license"],
    default_visibility = [
        "//verible/verilog/tools/ls:__subpackages__",
    ],
    features = ["layering_check"],
)

cc_library(
    name = "message-stream-splitter",
    srcs = ["message-stream-splitter.cc"],
    hdrs = ["message-stream-splitter.h"],
    deps = [
        "//verible/common/util:status-macros",
        "@com_google_absl//absl/status",
        "@com_google_absl//absl/strings",
        "@com_google_absl//absl/strings:string_view",
    ],
)

cc_test(
    name = "message-stream-splitter_test",
    srcs = ["message-stream-splitter_test.cc"],
    deps = [
        ":message-stream-splitter",
        "@com_google_absl//absl/status",
        "@com_google_absl//absl/strings",
        "@com_google_absl//absl/strings:string_view",
        "@com_google_googletest//:gtest",
        "@com_google_googletest//:gtest_main",
    ],
)

cc_library(
    name = "json-rpc-dispatcher",
    srcs = ["json-rpc-dispatcher.cc"],
    hdrs = ["json-rpc-dispatcher.h"],
    copts = select({
        "@platforms//os:windows": [],
        "//conditions:default": ["-fexceptions"],
    }),
    features = ["-use_header_modules"],  # precompiled headers incompatible with -fexceptions.
    deps = [
        "//verible/common/util:logging",
        "@com_google_absl//absl/strings:string_view",
        "@jsonhpp//:singleheader-json",
    ],
)

cc_test(
    name = "json-rpc-dispatcher_test",
    srcs = ["json-rpc-dispatcher_test.cc"],
    copts = select({
        "@platforms//os:windows": [],
        "//conditions:default": ["-fexceptions"],
    }),
    features = ["-use_header_modules"],  # precompiled headers incompatible with -fexceptions.
    deps = [
        ":json-rpc-dispatcher",
        "@com_google_absl//absl/strings:string_view",
        "@com_google_googletest//:gtest",
        "@com_google_googletest//:gtest_main",
        "@jsonhpp//:singleheader-json",
    ],
)

genrule(
    name = "lsp-protocol-gen",
    srcs = ["lsp-protocol.yaml"],
    outs = ["lsp-protocol.h"],
    cmd = "$(location :jcxxgen) --class_namespace verible::lsp --output $@ $<",
    tools = [":jcxxgen"],
)

cc_library(
    name = "lsp-protocol",
    hdrs = ["lsp-protocol.h"],
    deps = ["@jsonhpp//:singleheader-json"],
)

cc_library(
    name = "lsp-protocol-enums",
    hdrs = ["lsp-protocol-enums.h"],
)

cc_library(
    name = "lsp-protocol-operators",
    hdrs = ["lsp-protocol-operators.h"],
    deps = [":lsp-protocol"],
)

cc_test(
    name = "lsp-protocol-operators_test",
    srcs = ["lsp-protocol-operators_test.cc"],
    deps = [
        ":lsp-protocol",
        ":lsp-protocol-operators",
        "@com_google_googletest//:gtest",
        "@com_google_googletest//:gtest_main",
    ],
)

cc_library(
    name = "lsp-text-buffer",
    srcs = ["lsp-text-buffer.cc"],
    hdrs = ["lsp-text-buffer.h"],
    deps = [
        ":json-rpc-dispatcher",
        ":lsp-protocol",
        "//verible/common/strings:utf8",
        "@com_google_absl//absl/container:flat_hash_map",
        "@com_google_absl//absl/strings",
        "@com_google_absl//absl/strings:string_view",
    ],
)

cc_test(
    name = "lsp-text-buffer_test",
    srcs = ["lsp-text-buffer_test.cc"],
    deps = [
        ":json-rpc-dispatcher",
        ":lsp-protocol",
        ":lsp-text-buffer",
        "@com_google_absl//absl/strings",
        "@com_google_absl//absl/strings:string_view",
        "@com_google_googletest//:gtest",
        "@com_google_googletest//:gtest_main",
    ],
)

cc_library(
    name = "lsp-file-utils",
    srcs = ["lsp-file-utils.cc"],
    hdrs = ["lsp-file-utils.h"],
    deps = [
        "@com_google_absl//absl/strings",
        "@com_google_absl//absl/strings:string_view",
    ],
)

cc_test(
    name = "lsp-file-utils_test",
    srcs = ["lsp-file-utils_test.cc"],
    deps = [
        ":lsp-file-utils",
        "@com_google_absl//absl/strings",
        "@com_google_googletest//:gtest",
        "@com_google_googletest//:gtest_main",
    ],
)

cc_binary(
    name = "json-rpc-expect",
    srcs = ["json-rpc-expect.cc"],
    deps = [
        ":message-stream-splitter",
        "@com_google_absl//absl/status",
        "@com_google_absl//absl/strings",
        "@com_google_absl//absl/strings:string_view",
        "@jsonhpp//:singleheader-json",
    ],
)

cc_binary(
    name = "dummy-ls",
    srcs = ["dummy-ls.cc"],
    deps = [
        ":json-rpc-dispatcher",
        ":lsp-protocol",
        ":lsp-text-buffer",
        ":message-stream-splitter",
        "@com_google_absl//absl/status",
        "@com_google_absl//absl/strings:string_view",
        "@jsonhpp//:singleheader-json",
    ],
)

sh_test_with_runfiles_lib(
    name = "dummy-ls_test",
    size = "small",
    srcs = ["dummy-ls_test.sh"],
    args = [
        "$(location :dummy-ls)",
        "$(location :json-rpc-expect)",
    ],
    data = [
        ":dummy-ls",
        ":json-rpc-expect",
    ],
    deps = [],
)

cc_binary(
    name = "jcxxgen",
    srcs = ["jcxxgen.cc"],
    visibility = ["//visibility:private"],
    deps = [
        "//verible/common/util:init-command-line",
        "@com_google_absl//absl/container:flat_hash_map",
        "@com_google_absl//absl/flags:flag",
        "@com_google_absl//absl/strings",
        "@com_googlesource_code_re2//:re2",
    ],
)

genrule(
    name = "jcxxgen-testfile-gen",
    srcs = ["jcxxgen-testfile.yaml"],
    outs = ["jcxxgen-testfile.h"],
    cmd = "$(location :jcxxgen) --class_namespace verible::test --output $@ $<",
    tools = [":jcxxgen"],
)

cc_library(
    name = "jcxxgen-testfile",
    hdrs = ["jcxxgen-testfile.h"],
    deps = ["@jsonhpp//:singleheader-json"],
)

cc_test(
    name = "jcxxgen_test",
    srcs = ["jcxxgen_test.cc"],
    copts = select({
        "@platforms//os:windows": [],
        "//conditions:default": ["-fexceptions"],
    }),
    features = ["-use_header_modules"],  # precompiled headers incompatible with -fexceptions.
    deps = [
        ":jcxxgen-testfile",
        "@com_google_absl//absl/strings",
        "@com_google_googletest//:gtest",
        "@com_google_googletest//:gtest_main",
        "@jsonhpp//:singleheader-json",
    ],
)
