load("@rules_python//python:defs.bzl", "py_test")

py_test(
    name = "verible_verilog_syntax_py_test",
    size = "small",
    srcs = [
        "verible_verilog_syntax.py",
        "verible_verilog_syntax_test.py"
    ],
    srcs_version = "PY3",
    main = "verible_verilog_syntax_test.py",
    data = ["//verilog/tools/syntax:verible-verilog-syntax"],
    args = ["$(location //verilog/tools/syntax:verible-verilog-syntax)"],
)
