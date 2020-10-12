# SystemVerilog Style Linter

<!--*
freshness: { owner: 'fangism' reviewed: '2020-10-07' }
*-->

The SV style linter analyzes code for patterns and constructs that are deemed
undesirable according to the implemented lint rules. Ideally, each lint rule
should reference a passage from an authoritative style guide.

## Usage

```
usage: verible-verilog-lint [options] <file> [<file>...]

  Flags from verilog/analysis/verilog_linter.cc:
    --rules (Comma-separated of lint rules to enable. No prefix or a '+' prefix
      enables it, '-' disable it. Configuration values for each rules placed
      after '=' character.); default: ;
    --rules_config (Path to lint rules configuration file.);
      default: ".rules.verible_lint";
    --ruleset ([default|all|none], the base set of rules used by linter);
      default: default;

  Flags from verilog/tools/lint/verilog_lint.cc:
    --generate_markdown (If true, print the description of every rule formatted
      for the markdown and exit immediately. Intended for the output to be
      written to a snippet of markdown.); default: false;
    --help_rules ([all|<rule-name>], print the description of one rule/all rules
      and exit immediately.); default: "";
    --lint_fatal (If true, exit nonzero if linter finds violations.);
      default: false;
    --parse_fatal (If true, exit nonzero if there are any syntax errors.);
      default: false;
```

## Lint Rules

User documentation for the lint rules is generated dynamically, and can be found
at https://google.github.io/verible/verilog_lint.html, or by running
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
configuration can be passed after an `=` assignment.

The following example enables the
[`enum-name-style`][lint-rule-list_enum-name-style] rule, enables and configures
the [`line-length`][lint-rule-list_line-length] rule (80 characters length) and
disables the [`no-tabs`][lint-rule-list_no-tabs] rule.

```
verible-verilog-lint --rules=enum-name-style,+line-length=length:80,-no-tabs ...
```

Additionally, the `--rules_config` flag can be used to read configuration stored
in a file. The syntax is the same as above, except the rules can be also
separated with the newline character.

## Waiving Lint Violations {#lint-waiver}

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

Another option is to use a configuration file with the `--waiver_files` flag.
The flag accepts a single file or a list of files (comma-separated). Specifying
multiple files is equivalent to concatenating the files in order of appearance.
By default, the rules are applied to all files, but with `--location` you can to
choose to only apply them to filenames matching the location regexp.

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

### Syntax errors

Syntax errors cannot be waived. A common source of syntax errors is if the file
is not a standalone Verilog program as defined by the LRM, e.g. a body snippet
of a module, class, task, or function. In such cases, the parser can be directed
to treat the code as a snippet by selecting a
[parsing mode](../../analysis/README.md#alternative-parsing-modes), which looks
like a comment near the top-of-file like `// verilog_syntax:
parse-as-module-body`.

<!-- reference links -->

[lint-rule-list]: https://google.github.io/verible/lint.html
[lint-rule-list_enum-name-style]: https://google.github.io/verible/lint.html#enum-name-style
[lint-rule-list_line-length]: https://google.github.io/verible/lint.html#line-length
[lint-rule-list_no-tabs]: https://google.github.io/verible/lint.html#no-tabs
