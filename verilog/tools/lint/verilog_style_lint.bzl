# Copyright 2017-2020 The Verible Authors.
#
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
#      http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.

"""Build rules for running the Verilog style linter (and other checks)

Usage:
  load(
      "//verilog/tools:verilog_style_lint.bzl",
      "verilog_style_lint",
  )

  # Scan this package and subpackages for Verilog sources and analyze them.
  verilog_style_lint.package_test(
    name = "default_style_lint_test",
    subpackages = ["sub/dir1", "sub/dir2", ...  ],
  )

or if you want to manually list files to scan:
  verilog_style_lint.test(
    name = "style_lint_test",
    srcs = [targets...],
  )

"""

_syntax_tool = "//verilog/tools/syntax:verible-verilog-syntax"

_linter_tool = "//verilog/tools/lint:verible-verilog-lint"

_verilog_extensions = [
    ".v",
    ".sv",
    ".vh",
    ".svh",
]

_verilog_srcs_glob_patterns = ["**/*" + ext for ext in _verilog_extensions]

def has_verilog_extension(filename):
    for ext in _verilog_extensions:
        if filename.endswith(ext):
            return True
    return False

_style_lint_fail_message = """
Expected clean report but found style diagnostics.
To proceed, fix or waive violation.
"""

_syntax_test_attrs = {
    "expect_fail": attr.bool(),
    "srcs": attr.label_list(
        allow_files = True,
        mandatory = True,
    ),
    "_compiler": attr.label(
        allow_files = True,
        default = Label(_syntax_tool),
        executable = True,
        cfg = "host",
    ),
}

def _syntax_test_impl(ctx):
    """Checks inputs files for valid Verilog syntax.

    Automatically filters out and ignores non-verilog files.

    Args:
      ctx: Context of this rule invocation.

    Returns:
      runfiles information.
    """
    verilog_srcs = [f for f in ctx.files.srcs if has_verilog_extension(f.path)]
    args = [f.short_path for f in verilog_srcs]
    base_command = [ctx.executable._compiler.short_path] + args
    if not verilog_srcs:
        native.warn("verilog_syntax.test did not find any Verilog sources.")
        command = []  # no-op
    elif ctx.attr.expect_fail:
        command = ["!"] + base_command  # '!' inverts exit status (shell)
    else:
        command = base_command

    script = "#!/bin/bash\n" + " ".join(command) + "\n"
    ctx.actions.write(
        content = script,
        output = ctx.outputs.executable,
        is_executable = True,
    )
    exec_inputs = [ctx.executable._compiler] if verilog_srcs else []
    runfiles = ctx.runfiles(files = exec_inputs + verilog_srcs)
    return [DefaultInfo(runfiles = runfiles)]

_syntax_test = rule(
    attrs = _syntax_test_attrs,
    executable = True,
    test = True,
    implementation = _syntax_test_impl,
)

def _verilog_style_lint_report(name, srcs, flags = None):
    """Rule for producing a lint report on a collection of sources.

    Args:
      name: label of lint report.
      srcs: list of source files to scan (can use native.glob()).
      flags: list of flags for running the verilog_lint binary.
        Examples: --parse_fatal, --lint_fatal.
    """
    output = name + "-style_lint_report.txt"

    # ignore error status for the sake of generating full report
    use_flags = ["--noparse_fatal", "--nolint_fatal"] + (flags or [])
    if srcs:
        files_target = name + "_files"

        native.filegroup(
            name = files_target,
            srcs = srcs,
        )

        lint_cmd = "$(location {tool}) {flags} $(SRCS) > $@".format(
            tool = _linter_tool,
            flags = " ".join(use_flags),
        )

        native.genrule(
            name = name,
            tools = [_linter_tool],
            srcs = [files_target],
            outs = [output],
            cmd = lint_cmd,
        )
    else:
        # No sources, don't bother running linter.
        native.genrule(
            name = name,
            outs = [output],
            cmd = "touch $@",
        )

def _check_empty_script(filename, message, expect_fail):
    """Returns a script check that checks for empty diagnostics in a file.

    Args:
      filename: name of file containing diagnostic output.
      message: message to print when there are unexpected findings.
        message may be multiline.
      expect_fail: if True expect there to be findings, and invert test status.

    Returns:
      A string that is a shell script for the diagnostic test.
    """
    if expect_fail:
        return """
test -s {} ||
  {{ echo Expected diagnostics, but found none. ; exit 1 ;}}
""".format(filename)
    else:
        return """
test ! -s {filename} ||
  {{ cat <<EOF
{message}
EOF
     cat {filename} ;
     exit 1 ;}}
""".format(filename = filename, message = message)

# Attributes for _test_diagnostics.
_test_diagnostics_attrs = {
    # If true, invert the test's exit status, useful when expecting failures.
    "expect_fail": attr.bool(),

    # report refers to a single file that captures output from a linter.
    # An empty report is treated as lint-clean (pass), a non-empty report
    # is interpreted as a failure.
    "report": attr.label(
        mandatory = True,
        allow_single_file = True,
    ),
}

def _style_lint_test_diagnostics_impl(ctx):
    """Checks a report for diagnostics.  Fails if report contains any text."""
    report = ctx.file.report
    ctx.actions.write(
        output = ctx.outputs.executable,
        content = _check_empty_script(
            expect_fail = ctx.attr.expect_fail,
            filename = report.short_path,
            message = _style_lint_fail_message,
        ),
    )
    runfiles = ctx.runfiles(files = [report])
    return [DefaultInfo(runfiles = runfiles)]

_verilog_style_lint_diagnostics_test = rule(
    attrs = _test_diagnostics_attrs,
    test = True,
    implementation = _style_lint_test_diagnostics_impl,
)

def _verilog_style_lint_test(name, srcs, flags = None, expect_fail = None):
    """Macro for running Verilog style lint tests in the silo.

    Args:
      name: name of test.
      srcs: list of source files to scan (e.g. can use glob()).
      flags: list of flags for running the verilog_lint binary.
             Note: --norules_config_search is used for each test.
      expect_fail: if True, expect to find errors (invert status).
    """
    forced_flags = [
        "--norules_config_search",
    ]
    _verilog_style_lint_report(
        name = name + "-report",
        srcs = srcs,
        flags = (flags or []) + forced_flags,
    )
    _verilog_style_lint_diagnostics_test(
        name = name,
        expect_fail = expect_fail,
        report = name + "-report",
    )

def _package_style_lint_test(
        name,
        subpackages = None,
        exclude = None,
        expect_fail = False):
    """Tests Verilog lint on sources inside the invoking directory.

    Args:
      name: string, name of test.
      subpackages: paths to subpackages with lint test rules.
        TODO(fangism): can this be automated?
      exclude: list of files or patterns to exclude from checking.
      expect_fail: if True, expect to find errors (invert status).
    """
    _verilog_style_lint_test(
        name = name + "-local",
        srcs = native.glob(
            # Recursively scan for files, stopping at package boundaries,
            # i.e. directories with BUILD files.
            include = _verilog_srcs_glob_patterns,
            exclude = exclude or [],
        ),
        expect_fail = expect_fail,
    )
    if subpackages:
        native.test_suite(
            name = name + "-sub",
            tests = [
                "//{0}/{1}:{2}".format(native.package_name(), sub, name)
                for sub in subpackages
            ],
        )

    native.test_suite(
        name = name,
        tests = [name + "-local"] + ([name + "-sub"] if subpackages else []),
    )

# Load these modules to access public functions as its members.
verilog_syntax = struct(
    test = _syntax_test,
)

verilog_style_lint = struct(
    package_test = _package_style_lint_test,
    test = _verilog_style_lint_test,
)
