# This package contains the style linter binary and end-to-end tests for it.

load("//bazel:sh_test_with_runfiles_lib.bzl", "sh_test_with_runfiles_lib")
load("//bazel:variables.bzl", "STATIC_EXECUTABLES_FEATURE")
load(
    ":verilog_style_lint.bzl",
    "verilog_style_lint",
    "verilog_syntax",
)

package(
    default_applicable_licenses = ["//:license"],
    default_visibility = ["//visibility:private"],
    features = ["layering_check"],
)

# SystemVerilog-parser based linter
cc_binary(
    name = "verible-verilog-lint",
    srcs = ["verilog-lint.cc"],
    features = STATIC_EXECUTABLES_FEATURE +
               select({
                   "//bazel:static_linked_executables": ["-supports_start_end_lib"],
                   "//conditions:default": [],
               }),
    linkopts = select({
        "//bazel:static_linked_executables": ["-fuse-ld=bfd"],
        "//conditions:default": [],
    }),
    visibility = ["//visibility:public"],
    deps = [
        "//verible/common/analysis:lint-rule-status",
        "//verible/common/analysis:violation-handler",
        "//verible/common/util:enum-flags",
        "//verible/common/util:init-command-line",
        "//verible/common/util:iterator-range",
        "//verible/common/util:logging",
        "//verible/verilog/analysis:verilog-linter",
        "//verible/verilog/analysis:verilog-linter-configuration",
        "@abseil-cpp//absl/flags:flag",
        "@abseil-cpp//absl/status",
        "@abseil-cpp//absl/strings",
    ],
)

# Integration tests for different flags and configurations
# These tests help confirm that rules' cc_libraries are properly alwayslink-ed.
# TODO(fangism): re-organize this into structs instead of tuples,
#   and provide the group in a .bzl macro,
#   and define a test_suite for all of these.
_linter_test_configs = [
    # (rule-name, testdata/FILE.sv, default-enabled?)
    # See verilog/analysis/default_rules.h for default-enabled rules.
    ("always-comb", "always_comb_module", True),
    ("suggest-parentheses", "suggest_parentheses_example", True),
    ("always-comb-blocking", "always_comb_blocking", True),
    ("always-ff-non-blocking", "always_ff_non_blocking", True),
    ("case-missing-default", "case_missing_default", True),
    ("constraint-name-style", "constraint_name_style", True),
    ("dff-name-style", "dff_name_style", False),
    ("disable-statement", "disable_statement", False),
    ("endif-comment", "endif_comment", False),
    ("enum-name-style", "enum_name_style", True),
    ("explicit-begin", "explicit_begin", False),
    ("explicit-function-lifetime", "explicit_function_lifetime", True),
    ("explicit-function-task-parameter-type", "explicit_function_parameter_type", True),
    ("explicit-function-task-parameter-type", "explicit_task_parameter_type", True),
    ("explicit-parameter-storage-type", "explicit_parameter_storage_type", True),
    ("explicit-task-lifetime", "explicit_task_lifetime", True),
    ("forbid-consecutive-null-statements", "forbid_consecutive_null_statements", True),
    ("forbid-line-continuations", "forbid_line_continuations", True),
    ("forbid-negative-array-dim", "forbid_negative_array_dim", False),
    ("generate-label", "generate_label_module", True),
    ("generate-label", "generate-label-module-body", True),  # uses parse directive
    ("generate-label-prefix", "generate_label_prefix", True),
    ("instance-shadowing", "shadow_parameter", False),
    ("invalid-system-task-function", "psprintf", True),
    ("interface-name-style", "interface_type_name_style", True),
    ("legacy-genvar-declaration", "legacy_genvar_declaration", False),
    ("legacy-generate-region", "legacy_generate_region", False),
    ("macro-name-style", "macro_name_style", True),
    ("macro-string-concatenation", "macro_string_concatenation", False),
    ("mismatched-labels", "mismatched_labels", False),
    ("module-begin-block", "module_begin_block", True),
    ("module-filename", "module_filename", True),
    ("numeric-format-string-style", "numeric_format_string_style", False),
    ("package-filename", "package_filename_pkg", True),
    ("packed-dimensions-range-ordering", "packed_dimensions", True),
    ("parameter-name-style", "localparam_name_style", True),
    ("parameter-name-style=localparam_style:ALL_CAPS", "localparam_name_style_all_caps", False),
    ("parameter-name-style=localparam_style:CamelCase", "localparam_name_style_camel_case", True),
    ("parameter-name-style", "parameter_name_style", True),
    ("parameter-type-name-style", "parameter_type_name_style", False),
    ("parameter-type-name-style", "localparam_type_name_style", False),
    ("plusarg-assignment", "plusarg_assignment", True),
    ("port-name-suffix", "port_name_suffix", False),
    ("positive-meaning-parameter-name", "positive_meaning_parameter_name", True),
    ("proper-parameter-declaration", "proper_parameter_declaration", False),
    ("proper-parameter-declaration", "proper_localparam_declaration", False),
    ("module-parameter", "instance_parameters", True),
    ("module-port", "instance_ports", True),
    ("module-port", "instance-ports-module-body", True),  # uses parse directive
    ("signal-name-style", "signal_name_style", False),
    ("struct-union-name-style", "struct_name_style", True),
    ("struct-union-name-style", "union_name_style", True),
    ("suspicious-semicolon", "suspicious_semicolon", False),
    ("v2001-generate-begin", "generate_begin_module", True),
    ("void-cast", "void-cast", True),
    ("undersized-binary-literal", "undersized_binary_literal", True),
    ("unpacked-dimensions-range-ordering", "unpacked_dimensions", True),
    ("forbidden-macro", "uvm_warning", True),
    ("create-object-name-match", "object_creation_name", True),
    ("posix-eof", "posix_eof", True),
    ("no-tabs", "tabs", True),
    ("line-length", "long_line", True),
    ("no-trailing-spaces", "trailing_spaces", True),
    ("forbid-defparam", "defparam_usage", True),
    ("typedef-enums", "typedef_enums", True),
    ("typedef-structs-unions", "typedef_structs", True),
    ("typedef-structs-unions", "typedef_unions", True),
    ("truncated-numeric-literal", "truncated_numeric_literal", True),
    ("one-module-per-file", "one_module_per_file", False),
    ("uvm-macro-semicolon", "uvm_macro_semicolon", False),
    ("banned-declared-name-patterns", "banned_declared_name_patterns", False),
]

[
    (
        # Verifies that with no lint rules, test case is syntactically valid.
        verilog_style_lint.test(
            name = testfilebase + "-syntax_test",
            srcs = ["testdata/{}.sv".format(testfilebase)],
            flags = ["--ruleset=none"],
        ),
        # Verifies that rule is available as part of the 'all' set.
        # This is only relevant for rules that are not testing specific
        # configurations.
        verilog_style_lint.test(
            name = testfilebase + "-allrules_test",
            srcs = ["testdata/{}.sv".format(testfilebase)],
            expect_fail = True,
            flags = ["--ruleset=all"],
        ) if "=" not in rule else (),  # Only useful when not configured
        # Verifies that the rule is/is not enabled in the 'default' set.
        verilog_style_lint.test(
            name = testfilebase + "-defaultrules_test",
            srcs = ["testdata/{}.sv".format(testfilebase)],
            expect_fail = default,
        ),
        # Verifies that one specific rule alone fails this test case.
        verilog_style_lint.test(
            name = testfilebase + "-onerule_test",
            srcs = ["testdata/{}.sv".format(testfilebase)],
            expect_fail = True,
            flags = [
                "--ruleset=none",
                "--rules=" + rule,
            ],
        ),
        # Verifies that all rules *except* this one is lint clean.
        verilog_style_lint.test(
            name = testfilebase + "-minusrule_test",
            srcs = ["testdata/{}.sv".format(testfilebase)],
            flags = [
                "--ruleset=all",
                "--rules=-" + rule,  # exclude one rule
            ],
        ),
    )
    for rule, testfilebase, default in _linter_test_configs
]

# Modify a file with a known violation by inserting a waiver above the offending line.
genrule(
    name = "psprintf-waived-next-line",
    srcs = ["testdata/psprintf.sv"],
    outs = ["psprintf-waived-next-line.sv"],
    cmd = "sed -e '/psprintf/i\\\n  // verilog_lint: waive invalid-system-task-function\n' $< > $@",
)

# Modify a file with a known violation by inserting a waiver on the offending line.
genrule(
    name = "psprintf-waived-same-line",
    srcs = ["testdata/psprintf.sv"],
    outs = ["psprintf-waived-same-line.sv"],
    cmd = "sed -e '/psprintf/s|$$|  // verilog_lint: waive invalid-system-task-function|' $< > $@",
)

# Modify a file with a known violation by inserting a waiver range around the offending line.
genrule(
    name = "psprintf-waived-line-range",
    srcs = ["testdata/psprintf.sv"],
    outs = ["psprintf-waived-line-range.sv"],
    cmd = "sed " +
          "-e '/psprintf/i\\\n  // verilog_lint: waive-start invalid-system-task-function\n' " +
          "-e '/psprintf/a\\\n  // verilog_lint: waive-stop invalid-system-task-function\n' " +
          "$< > $@",
)

# Test that waiver works on one rule, one line, and line range.
[
    verilog_style_lint.test(
        name = "psprintf-waive-" + variant + "_test",
        srcs = [
            ":psprintf-waived-" + variant,
        ],
        flags = [
            "--ruleset=none",
            "--rules=invalid-system-task-function",
        ],
    )
    for variant in ("same-line", "next-line", "line-range")
]

# Modify a file with a known violation by inserting a waiver on the offending line.
genrule(
    name = "line-length-in-module-body-waived",
    srcs = ["testdata/line-length-in-module-body.sv"],
    outs = ["line-length-in-module-body-waived.sv"],
    cmd = "sed -e '$$s|$$|  // verilog_lint: waive line-length|' $< > $@",
)

# Test that one long line is caught.
verilog_style_lint.test(
    name = "line-length-in-module-body-fail_test",
    srcs = [
        "testdata/line-length-in-module-body.sv",
    ],
    expect_fail = True,
    flags = [
        "--ruleset=none",
        "--rules=line-length",
    ],
)

# Test that one long line is caught and waived.
verilog_style_lint.test(
    name = "line-length-in-module-body-waived_test",
    srcs = [
        ":line-length-in-module-body-waived",
    ],
    expect_fail = False,
    flags = [
        "--ruleset=none",
        "--rules=line-length",
    ],
)

# Verifies that lexer errors are properly rejected.
verilog_style_lint.test(
    name = "verilog-lint-lexer-fail_test",
    srcs = ["testdata/bad-id-lex.sv"],
    expect_fail = True,
)

verilog_syntax.test(
    name = "verilog-syntax-lexer-fail_test",
    srcs = ["testdata/bad-id-lex.sv"],
    expect_fail = True,
)

sh_test_with_runfiles_lib(
    name = "lint-tool_test",
    size = "small",
    srcs = ["lint_tool_test.sh"],
    args = ["$(location :verible-verilog-lint)"],
    data = [":verible-verilog-lint"],
    deps = [],
)
