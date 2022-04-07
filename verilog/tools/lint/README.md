# SystemVerilog Style Linter

<!--*
freshness: { owner: 'hzeller' reviewed: '2021-09-23' }
*-->

The `verible-verilog-lint` SV style linter analyzes code for patterns and
constructs that are deemed undesirable according to the implemented lint rules.
Ideally, each lint rule should reference a passage from an authoritative style
guide. The style linter operates on *single unpreprocessed files* in isolation.

For automatic code-reviews on github, there is a [easy to integrate github
action available][github-lint-action].

The style linter excels at:

*   Finding patterns in code that can be expressed in terms of syntax tree or
    token matching rules.
    *   Expressing rules with syntactic context-sensitivity.

Consequences of reading unpreprocessed input:

*   Can examine comments.
*   Good at examining uses of _unexpanded_ macros.

Currrent limitations:

*   No attempt to understand preprocessing conditional branches.
*   No semantic analysis (such as connectivity). This requires:
    *   preprocessing
    *   multi-file analysis
    *   abstract syntax tree

## Developers

[Style lint rule development guide](../../../doc/style_lint.md).

## Usage

```
usage: verible-verilog-lint [options] <file> [<file>...]

  Flags from external/com_google_absl/absl/flags/parse.cc:
    --flagfile (comma-separated list of files to load flags from); default: ;
    --fromenv (comma-separated list of flags to set from the environment [use
      'export FLAGS_flag1=value']); default: ;
    --tryfromenv (comma-separated list of flags to try to set from the
      environment if present); default: ;
    --undefok (comma-separated list of flag names that it is okay to specify on
      the command line even if the program does not define a flag with that
      name); default: ;

  Flags from verilog/analysis/verilog_linter.cc:
    --rules (Comma-separated of lint rules to enable. No prefix or a '+' prefix
      enables it, '-' disable it. Configuration values for each rules placed
      after '=' character.); default: ;
    --rules_config (Path to lint rules configuration file. Disables
      --rule_config_search if set.); default: "";
    --rules_config_search (Look for lint rules configuration file
      '.rules.verible_lint' searching upward from the location of each analyzed
      file.); default: false;
    --ruleset ([default|all|none], the base set of rules used by linter);
      default: default;
    --waiver_files (Path to waiver config files (comma-separated). Please refer
      to the README file for information about its format.); default: "";

  Flags from verilog/parser/verilog_parser.cc:
    --verilog_trace_parser (Trace verilog parser); default: false;

  Flags from verilog/tools/lint/verilog_lint.cc:
    --autofix (autofix mode; one of
      [no|patch-interactive|patch|inplace-interactive|inplace|generate-waiver]);
      default: no;
    --autofix_output_file (File to write a patch with autofixes to if
      --autofix=patch or --autofix=patch-interactive or a waiver file if
      --autofix=generate-waiver); default: "";
    --check_syntax (If true, check for lexical and syntax errors, otherwise
      ignore.); default: true;
    --generate_markdown (If true, print the description of every rule formatted
      for the Markdown and exit immediately. Intended for the output to be
      written to a snippet of Markdown.); default: false;
    --help_rules ([all|<rule-name>], print the description of one rule/all rules
      and exit immediately.); default: "";
    --lint_fatal (If true, exit nonzero if linter finds violations.);
      default: true;
    --parse_fatal (If true, exit nonzero if there are any syntax errors.);
      default: true;
    --show_diagnostic_context (prints an additional line on which the diagnostic
      was found,followed by a line with a position marker); default: false;
```

We recommend each project maintain its own configuration file for convenience
and consistency among project members.

## Diagnostics

Syntax errors and lint rule findings have the following format:

```
FILE:LINE:COL: text...
```

Examples:

```
path/to/missing-endmodule.sv:3:1: syntax error (unexpected EOF).
path/to/number-as-statement.sv:2:3: syntax error, rejected "123".
path/to/bad-dimensions.sv:114:43: Packed dimension range must be in decreasing order. http://your.style/guide.html#packed-ordering [packed-dimensions-range-ordering]
```

## Lint Rules

User documentation for the lint rules is generated dynamically, and can be found
at https://chipsalliance.github.io/verible/verilog_lint.html, or by running
`verible-verilog-lint --help_rules` for text or `--generate_markdown`. We also
provide a Bazel build rule:

```bash
# Generating documentation
bazel build :lint_doc

# It will be generated into
bazel-bin/documentation_verible_lint_rules.md
```

## Rule Configuration

The `--rules` flag allows to enable/disable rules as well as pass configuration
to rules that accept them. It accepts a comma-separated list
[rule names][lint-rule-list]. If prefixed with a `-` (minus), the rule is
disabled. No prefix or a '+' (plus) prefix enables the rule. An optional
configuration can be passed after an `=` assignment. Each name/value is

So rule configurations with parameters looks like this
```
  --rules=[+-]rule-name="<param>:<paramvalue>;<param2>:<value>",[+-]next-rule...
```

The following example enables the
[`enum-name-style`][lint-rule-list_enum-name-style] rule, enables and configures
the [`line-length`][lint-rule-list_line-length] rule (80 characters length) and
disables the [`no-tabs`][lint-rule-list_no-tabs] rule.

```bash
verible-verilog-lint --rules=enum-name-style,+line-length=length:80,-no-tabs ...
```

Some lint rules have multiple parameters, these are separated with semicolon.
Since common shells treat semicolon as special character, you have to put
the parameters in quotes.

```bash
verible-verilog-lint --rules="undersized-binary-literal=hex:true;lint_zero:true" ...
```

Additionally, the `--rules_config` flag can be used to read configuration stored
in a file. The syntax is the same as above, except the rules can be also
separated with the newline character.

## Waiving Lint Violations {#lint-waiver}

### In-file waiver comments

In the rare circumstance where a line needs to be waived from a particular lint
rule, you can use the following waiver comment:

```
// This example waives the line after the waiver.
// verilog_lint: waive rule-name
The next non-comment line like this one is waived.

// This example waives the same line as the waiver.
This line is waived.  // verilog_lint: waive rule-name

// This example shows accumulation of waivers over multiple lines.
// verilog_lint: waive rule-name-1
// verilog_lint: waive rule-name-2
// Other comments, possibly waivers for other tools.
This line will be waived for both rule-name-1 and rule-name-2.

// This example shows how to waive an entire range of lines.
// verilog_lint: waive-start rule-X
...
All lines in between will be waived for rule-X
...
// verilog_lint: waive-stop rule-X
```

### External waiver files

If you prefer to manage waivers out-of-source, then waiver files may be a
suitable option, especially if the use of multiple linters risks cluttering your
source with too many lint waiver directives.

The `--waiver_files` flag accepts a single configuration file or a list of files
(comma-separated). Specifying multiple files is equivalent to concatenating the
files in order of appearance. By default, the rules are applied to all files,
but with `--location` you can choose to only apply them to filenames matching
the location regexp.

The format of this file is as follows:

```
waive --rule=rule-name-1 --line=10
waive --rule=rule-name-2 --line=5:10
waive --rule=rule-name-3 --regex="^\s*abc$"
waive --rule=rule-name-4 --line=42 --location=".*some_file.*"
```

The `--line` flag can be used to specify a single line to apply the waiver to or
a line range (separated with the `:` character). Additionally the `--regex` flag
can be used to dynamically match lines on which a given rule has to be waived.
This is especially useful for projects where some of the files are
auto-generated.

The name of the rule to waive is at the end of each diagnostic message in `[]`.

These waiver rules can be automatically generated with the AutoFix mode `generate-waiver` to stdout,
or to a waiver file if a path is passed to `--autofix_output_file`.
e.g.
```bash
verible-verilog-lint --autofix=generate-waiver --autofix_output_file=violations.waiver ...
```

Generated waiving rules will follow this format:
```
waive --rule=<rule_name> --line=<number> --location=<file_name>
```

### Syntax errors

Syntax errors cannot be waived. A common source of syntax errors is if the file
is not a standalone Verilog program as defined by the LRM, e.g. a body snippet
of a module, class, task, or function. In such cases, the parser can be directed
to treat the code as a snippet by selecting a
[parsing mode](../../analysis/README.md#alternative-parsing-modes), which looks
like a comment near the top-of-file like `// verilog_syntax:
parse-as-module-body`.

## Automatically fixing trivial violations

Some trivial violations (e.g. trailing spaces or repeated semicolons) can be
fixed automatically. The `--autofix` flag controls the mode in which fixes
are presented or applied.

|--autofix value       | Description
:----------------------|:---------------------------------------------------
| no                   | No fix is is shown or applied.
| patch-interactive    | Interactive choice of fixes that are written as unified diff to `--autofix_output_file`.  |
| inplace-interactive  | Interacive choice of fixes that are applied to the original file in place. **(modifies input)**
| patch                | _All_ available fixes are written as unified diff.
| inplace              | _All_ available fixes are applied to the original file in place. **(modifies input)**
| generate-waiver      | Generates a waiver rule for each violation, as a temporary fix.

If `--autofix_output_file` is not given, patch or waiver output is written to stdout.

The interactive modes `--autofix=patch-interactive` and
`--autofix=inplace-interactive` offer the following actions for each fix:

* `y` - apply fix
* `n` - reject fix
* `a` - apply this and all remaining fixes for violations of this rule
* `d` - reject this and all remaining fixes for violations of this rule
* `A` - apply this and all remaining fixes
* `D` - reject this and all remaining fixes
* `p` - show fix
* `P` - show fixes applied so far
* `?` - print this help and prompt again

Example interactive session (`--autofix=inplace-interactive`):

<pre>
$ verible-verilog-lint --rules="undersized-binary-literal=hex:true" --autofix=inplace-interactive autofixtest.sv

autofixtest.sv:2:30: Parenthesize condition expressions that appear in the true-clause of another condition expression. [Style: parentheses] [suggest-parentheses]
<b>[ Add parenthesis for readability ]</b>
@@ -1,3 +1,3 @@
 module foo();
-   assign foo = condition_a? condition_b ? condition_c ? a : b : c : d;
+   assign foo = condition_a? (condition_b ? condition_c ? a : b : c) : d;
    assign c = 32'h1;
<b>Autofix is available. Apply? [y,n,a,d,A,D,p,P,?]</b> y
autofixtest.sv:2:44: Parenthesize condition expressions that appear in the true-clause of another condition expression. [Style: parentheses] [suggest-parentheses]
<b>[ Add parenthesis for readability ]</b>
@@ -1,3 +1,3 @@
 module foo();
-   assign foo = condition_a? condition_b ? condition_c ? a : b : c : d;
+   assign foo = condition_a? condition_b ? (condition_c ? a : b) : c : d;
    assign c = 32'h1;
<b>Autofix is available. Apply? [y,n,a,d,A,D,p,P,?]</b> y
autofixtest.sv:3:19: Hex literal 32'h1 has less digits than expected for 32 bits. [Style: number-literals] [undersized-binary-literal]
<b>[ 1. Alternative Left-expand leading zeroes ]</b>
@@ -2,3 +2,3 @@
    assign foo = condition_a? condition_b ? condition_c ? a : b : c : d;
-   assign c = 32'h1;
+   assign c = 32'h00000001;
 endmodule
<b>[ 2. Alternative Replace with decimal ]</b>
@@ -2,3 +2,3 @@
    assign foo = condition_a? condition_b ? condition_c ? a : b : c : d;
-   assign c = 32'h1;
+   assign c = 32'd1;
 endmodule
<b>Autofix is available. Apply? [1,2,y,n,a,d,A,D,p,P,?]</b> 1
</pre>

<!-- reference links -->

[lint-rule-list]: https://chipsalliance.github.io/verible/lint.html
[lint-rule-list_enum-name-style]: https://chipsalliance.github.io/verible/lint.html#enum-name-style
[lint-rule-list_line-length]: https://chipsalliance.github.io/verible/lint.html#line-length
[lint-rule-list_no-tabs]: https://chipsalliance.github.io/verible/lint.html#no-tabs
[github-lint-action]: https://github.com/chipsalliance/verible-linter-action
