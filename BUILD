# Toplevel build-file for Verible libraries and tools.
# To compile the tools, invoke
#  bazel build ...
# Run tests with
#  bazel test ...

load("@rules_license//rules:license.bzl", "license")

package(
    default_applicable_licenses = [":license"],
    default_visibility = ["//visibility:public"],
    features = ["layering_check"],
)

# Machine-readable license specification.
license(
    name = "license",
    package_name = "verible",
    license_kind = "@rules_license//licenses/spdx:Apache-2.0",
    license_text = "LICENSE",
)

filegroup(
    name = "install-binaries",
    srcs = [
        "//common/tools:verible-patch-tool",
        "//verilog/tools/diff:verible-verilog-diff",
        "//verilog/tools/formatter:verible-verilog-format",
        "//verilog/tools/kythe:verible-verilog-kythe-extractor",
        "//verilog/tools/kythe:verible-verilog-kythe-kzip-writer",
        "//verilog/tools/lint:verible-verilog-lint",
        "//verilog/tools/ls:verible-verilog-ls",
        "//verilog/tools/obfuscator:verible-verilog-obfuscate",
        "//verilog/tools/preprocessor:verible-verilog-preprocessor",
        "//verilog/tools/project:verible-verilog-project",
        "//verilog/tools/syntax:verible-verilog-syntax",
    ],
)

filegroup(
    name = "install-scripts",
    srcs = [
        "//common/tools:verible-transform-interactive",
        "//verilog/tools/formatter:git-verilog-format",
        "//verilog/tools/formatter:verible-verilog-format-changed-lines-interactive",
    ],
)

# Installing; see README.md
alias(
    name = "install",
    actual = "//bazel:install",
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
