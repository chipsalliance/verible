# Toplevel build-file for Verible libraries and tools.
# To compile the tools, invoke
#  bazel build ...
# Run tests with
#  bazel test ...

load("@com_github_google_rules_install//installer:def.bzl", "installer")

licenses(["notice"])  # Apache 2.0

exports_files([
    "LICENSE",
])

installer(
    name = "install",
    data = [
        "//verilog/tools/diff:verilog_diff",
#        "//verilog/tools/formatter:git_verilog_format",  # currently does not work.
        "//verilog/tools/formatter:verilog_format",
        "//verilog/tools/lint:verilog_lint",
        "//verilog/tools/obfuscator:verilog_obfuscate",
        "//verilog/tools/syntax:verilog_syntax",
    ],
)

genrule(
    name = "lint_doc",
    outs = ["documentation_verible_lint_rules.md"],
    cmd = "$(location //verilog/tools/lint:verilog_lint) --generate_markdown " +
          "> $(OUTS)",
    tools = [
        "//verilog/tools/lint:verilog_lint",
    ],
)
