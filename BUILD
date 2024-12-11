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
        "//verible/common/tools:verible-patch-tool",
        "//verible/verilog/tools/diff:verible-verilog-diff",
        "//verible/verilog/tools/formatter:verible-verilog-format",
        "//verible/verilog/tools/kythe:verible-verilog-kythe-extractor",
        "//verible/verilog/tools/kythe:verible-verilog-kythe-kzip-writer",
        "//verible/verilog/tools/lint:verible-verilog-lint",
        "//verible/verilog/tools/ls:verible-verilog-ls",
        "//verible/verilog/tools/obfuscator:verible-verilog-obfuscate",
        "//verible/verilog/tools/preprocessor:verible-verilog-preprocessor",
        "//verible/verilog/tools/project:verible-verilog-project",
        "//verible/verilog/tools/syntax:verible-verilog-syntax",
    ],
)

filegroup(
    name = "install-scripts",
    srcs = [
        "//verible/common/tools:verible-transform-interactive",
        "//verible/verilog/tools/formatter:git-verible-verilog-format",
        "//verible/verilog/tools/formatter:verible-verilog-format-changed-lines-interactive",
    ],
)

genrule(
    name = "lint_doc",
    outs = ["documentation_verible_lint_rules.md"],
    cmd = "$(location //verible/verilog/tools/lint:verible-verilog-lint) " +
          "--generate_markdown > $(OUTS)",
    tools = [
        "//verible/verilog/tools/lint:verible-verilog-lint",
    ],
)
