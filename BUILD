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
        "//common/tools:verible-patch-tool",
        "//common/tools:verible-transform-interactive",
        "//verilog/tools/diff:verible-verilog-diff",
        "//verilog/tools/formatter:git-verilog-format",
        "//verilog/tools/formatter:verible-verilog-format",
        "//verilog/tools/formatter:verible-verilog-format-changed-lines-interactive",
        "//verilog/tools/lint:verible-verilog-lint",
        "//verilog/tools/obfuscator:verible-verilog-obfuscate",
        "//verilog/tools/preprocessor:verible-verilog-preprocessor",
        "//verilog/tools/syntax:verible-verilog-syntax",

        # Deprecated legacy names, available for a transition period
        "//verilog/tools/diff:verilog_diff-deprecated",
        "//verilog/tools/lint:verilog_lint-deprecated",
        "//verilog/tools/syntax:verilog_syntax-deprecated",
        "//verilog/tools/formatter:verilog_format-deprecated",
        "//verilog/tools/obfuscator:verilog_obfuscate-deprecated",
    ],
)

genrule(
    name = "lint_doc",
    outs = ["documentation_verible_lint_rules.md"],
    cmd = "$(location //verilog/tools/lint:verible-verilog-lint) " +
          "--generate_markdown > $(OUTS)",
    tools = [
        "//verilog/tools/lint:verible-verilog-lint",
    ],
)
