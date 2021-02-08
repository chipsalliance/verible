load("@rules_python//python:defs.bzl", "py_test", "py_library")
load("@pip//:requirements.bzl", "requirement")

licenses(["notice"])

py_library(
    name = "verible_verilog_syntax_py",
    srcs = ["verible_verilog_syntax.py"],
    srcs_version = "PY3",
    data = ["//verilog/tools/syntax:verible-verilog-syntax"],
    deps = [requirement("anytree")],
)

py_test(
    name = "verible_verilog_syntax_py_test",
    size = "small",
    srcs = ["verible_verilog_syntax_test.py"],
    srcs_version = "PY3",
    python_version = "PY3",
    main = "verible_verilog_syntax_test.py",
    deps = [":verible_verilog_syntax_py"],
    data = ["//verilog/tools/syntax:verible-verilog-syntax"],
    args = ["$(location //verilog/tools/syntax:verible-verilog-syntax)"],
)
