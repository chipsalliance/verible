licenses(["notice"])

cc_library(
    name = "editscript",
    hdrs = ["editscript.h"],
)

cc_test(
    name = "editscript_test",
    srcs = ["editscript_test.cc"],
    deps = [
        ":editscript",
        "@com_google_absl//absl/strings",
        "@com_google_googletest//:gtest_main",
    ],
)
