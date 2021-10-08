# This package contains the basic language server protocol [1] implementation
# needed in language specific parts to provide LSP services.
# LSP is using JSON-PRC [2] for the RPC protocol, the transport layer chunks
# messages with header+body blocks, similar to HTTP.
#
# [1]: https://microsoft.github.io/language-server-protocol/specification
# [2]: https://www.jsonrpc.org/specification

licenses(["notice"])

package(
    default_visibility = [
        "//:__subpackages__",
    ],
)

cc_library(
    name = "message-stream-splitter",
    hdrs = ["message-stream-splitter.h"],
    srcs = ["message-stream-splitter.cc"],
    deps = [
        "@com_google_absl//absl/strings",
        "@com_google_absl//absl/status",
    ],
)

cc_test(
    name = "message-stream-splitter_test",
    srcs = ["message-stream-splitter_test.cc"],
    deps = [
        ":message-stream-splitter",
        "@com_google_absl//absl/status",
        "@com_google_absl//absl/strings",
        "@com_google_googletest//:gtest_main",
    ],
)

cc_library(
    name = "json-rpc-dispatcher",
    hdrs = ["json-rpc-dispatcher.h"],
    srcs = ["json-rpc-dispatcher.cc"],
    copts = select({
        "@platforms//os:windows": [],
        "//conditions:default": ["-fexceptions"],
    }),
    features = ["-use_header_modules"],  # precompiled headers incompatible with -fexceptions.
    deps = [
        "@com_google_absl//absl/strings",
        "@jsonhpp//:jsonhpp",
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
        "@com_google_absl//absl/status",
        "@com_google_absl//absl/strings",
        "@com_google_googletest//:gtest_main",
    ],
)

cc_library(
    name = "lsp-text-buffer",
    srcs = ["lsp-text-buffer.cc"],
    hdrs = ["lsp-text-buffer.h", "lsp-protocol.h"],
    deps = [
        ":json-rpc-dispatcher",
        "@com_google_absl//absl/strings",
        "@jsonhpp//:jsonhpp",
    ]
)

cc_test(
    name = "lsp-text-buffer_test",
    srcs = ["lsp-text-buffer_test.cc"],
    deps = [
        ":json-rpc-dispatcher",
        ":lsp-text-buffer",
        "@com_google_absl//absl/strings",
        "@com_google_googletest//:gtest_main",
    ]
)

genrule(
    name = "lsp-protocol_h",
    srcs = ["lsp-protocol.yaml"],
    outs = ["lsp-protocol.h"],
    cmd = '$(location jcxxgen) --json_header=\'"nlohmann/json.hpp"\' --class_namespace verible::lsp --output $@ $(location lsp-protocol.yaml)',
    tools = [":jcxxgen"],
    visibility = ["//visibility:private"],
)

cc_binary(
    name = "jcxxgen",
    srcs = ["jcxxgen.cc"],
    deps = [
        "//common/util:init_command_line",
       "@com_google_absl//absl/strings",
       "@com_google_absl//absl/flags:flag",
    ]
)
