# SystemVerilog Formatter

<!--*
freshness: { owner: 'hzeller' reviewed: '2020-10-07' }
*-->

`verible-verilog-format` is the SystemVerilog formatter tool. You can can
get a full set of avilable flags using the `--helpfull` flag.

For automatic formatting suggestions on github pull requests, there is a
[easy to integrate github action available][github-format-action].

## Usage

```
usage: verible-verilog-format [options] <file> [<file...>]
To pipe from stdin, use '-' as <file>.

  Flags from common/formatting/basic_format_style_init.cc:
    --column_limit (Target line length limit to stay under when formatting.);
      default: 100;
    --indentation_spaces (Each indentation level adds this many spaces.);
      default: 2;
    --line_break_penalty (Penalty added to solution for each introduced line
      break.); default: 2;
    --over_column_limit_penalty (For penalty minimization, this represents the
      baseline penalty value of exceeding the column limit. Additional penalty
      of 1 is incurred for each character over this limit); default: 100;
    --wrap_spaces (Each wrap level adds this many spaces. This applies when the
      first element after an open-group section is wrapped. Otherwise, the
      indentation level is set to the column position of the open-group
      operator.); default: 4;

  Flags from verilog/formatting/format_style_init.cc:
    --assignment_statement_alignment (Format various assignments:
      {align,flush-left,preserve,infer}); default: infer;
    --case_items_alignment (Format case items:
      {align,flush-left,preserve,infer}); default: infer;
    --class_member_variable_alignment (Format class member variables:
      {align,flush-left,preserve,infer}); default: infer;
    --compact_indexing_and_selections (Use compact binary expressions inside
      indexing / bit selection operators); default: true;
    --distribution_items_alignment (Aligh distribution items:
      {align,flush-left,preserve,infer}); default: infer;
    --enum_assignment_statement_alignment (Format assignments with enums:
      {align,flush-left,preserve,infer}); default: infer;
    --expand_coverpoints (If true, always expand coverpoints.); default: false;
    --formal_parameters_alignment (Format formal parameters:
      {align,flush-left,preserve,infer}); default: infer;
    --formal_parameters_indentation (Indent formal parameters: {indent,wrap});
      default: wrap;
    --module_net_variable_alignment (Format net/variable declarations:
      {align,flush-left,preserve,infer}); default: infer;
    --named_parameter_alignment (Format named actual parameters:
      {align,flush-left,preserve,infer}); default: infer;
    --named_parameter_indentation (Indent named parameter assignments:
      {indent,wrap}); default: wrap;
    --named_port_alignment (Format named port connections:
      {align,flush-left,preserve,infer}); default: infer;
    --named_port_indentation (Indent named port connections: {indent,wrap});
      default: wrap;
    --port_declarations_alignment (Format port declarations:
      {align,flush-left,preserve,infer}); default: infer;
    --port_declarations_indentation (Indent port declarations: {indent,wrap});
      default: wrap;
    --port_declarations_right_align_packed_dimensions (If true, packed
      dimensions in contexts with enabled alignment are aligned to the right.);
      default: false;
    --port_declarations_right_align_unpacked_dimensions (If true, unpacked
      dimensions in contexts with enabled alignment are aligned to the right.);
      default: false;
    --struct_union_members_alignment (Format struct/union members:
      {align,flush-left,preserve,infer}); default: infer;
    --try_wrap_long_lines (If true, let the formatter attempt to optimize line
      wrapping decisions where wrapping is needed, else leave them unformatted.
      This is a short-term measure to reduce risk-of-harm.); default: false;
    --wrap_end_else_clauses (Split end and else keywords into separate lines);
      default: false;

  Flags from verilog/tools/formatter/verilog_format.cc:
    --failsafe_success (If true, always exit with 0 status, even if there were
      input errors or internal errors. In all error conditions, the original
      text is always preserved. This is useful in deploying services where
      fail-safe behaviors should be considered a success.); default: true;
    --inplace (If true, overwrite the input file on successful conditions.);
      default: false;
    --lines (Specific lines to format, 1-based, comma-separated, inclusive N-M
      ranges, N is short for N-N. By default, left unspecified, all lines are
      enabled for formatting. (repeatable, cumulative)); default: ;
    --max_search_states (Limits the number of search states explored during line
      wrap optimization.); default: 100000;
    --show_equally_optimal_wrappings (If true, print when multiple optimal
      solutions are found (stderr), but continue to operate normally.);
      default: false;
    --show_inter_token_info (If true, along with show_token_partition_tree,
      include inter-token information such as spacing and break penalties.);
      default: false;
    --show_largest_token_partitions (If > 0, print token partitioning and then
      exit without formatting output.); default: 0;
    --show_token_partition_tree (If true, print diagnostics after token
      partitioning and then exit without formatting output.); default: false;
    --stdin_name (When using '-' to read from stdin, this gives an alternate
      name for diagnostic purposes. Otherwise this is ignored.);
      default: "<stdin>";
    --verbose (Be more verbose.); default: false;
    --verify_convergence (If true, and not incrementally formatting with
      --lines, verify that re-formatting the formatted output yields no further
      changes, i.e. formatting is convergent.); default: true;
```

## Disabling Formatting {#disable-formatting}

When you want to exempt a range of text from formatting, write:

```
// verilog_format: off
... untouched code ...
// verilog_format: on
```

or

```
/* verilog_format: off */
... untouched code ...
/* verilog_format: on */
```

As a good practice, include a reason why you choose to disable a section.

```
// verilog_format: off  // my alignment is prettier than the tool's

// verilog_format: off  // issue #N: working around.
```

These directives take precedence over `--lines` specifications.

## Formatting included files

For files that are included as snippets in other contexts, e.g. only statements,
or only module body items, adding a
[parsing mode directive](../../analysis#alternative-parsing-modes) (such as `//
verilog_syntax: parse-as-module-body`) will let the formatter parse and format
such files successfully.

## Interactive Formatting

The formatter is ever a work-in-progress and may not always behave the way a
user would like it to behave. If you wish to review changes introduced by the
formatter and apply them interactively, we've provided a helper script (that is
actually formatter-agnostic) for the job.

```
verible-transform-interactive.sh -- verible-verilog-format -- files...
```

This will run the formatter on the affected files without modifying them
initially, and pass the `diff`-generated patch into a tool
(`verible-patch-tool`) that interactively applies patch hunks.

Answer the \[y/n\] prompts.

The original files will be modified in-place with the elected changes.

For convenience, one could create an alias like:

```
alias verilog-format-interactive='verible-transform-interactive.sh -- verible-verilog-format --'
```

## Incremental Formatting

If you wish to format only changed lines (a.k.a. incremental or partial
formatting), the following tools are provided to assist.

### Git users

`git-verible-verilog-format.sh` (installed along with `verible-verilog-format`)
can be run from within any subdirectory of a Git project. It automatically
detects new and changed lines, and generates the `--lines` flag for each
modified file in your workspace.

From `--help`:

```
git-verible-verilog-format.sh:
Performs incremental file formatting (verible-verilog-format) based on current diffs.
New files explicitly git-add-ed by the user are wholly formatted.

Actions:
  1) Runs 'git add -u' to stage currently modified files to the index.
     To format new files (wholly), 'git add' those before calling this script.
  2) Runs 'git diff -u --cached' to generate a unified diff.
  3) Diff is scanned to determine added or modified lines in each file.
  4) Invokes 'verible-verilog-format --inplace' on all touched or new Verilog files,
     but does not 'git add' so that the changes may be examined and tested.
     Formatting can be easily undone with:
       'git diff | git apply --reverse -'.

usage: git-verible-verilog-format.sh [script options] [-- [verilog_format options]]
  (no positional arguments)
  Run from anywhere inside a git project tree.

script options: (options with arguments can be: --flag=VALUE or --flag VALUE)
  --help | -h : print help and exit
  --verbose | -v : execute verbosely
  --dry-run : stop before running formatter, and print formatting commands
  --formatter TOOL : path to verilog_format binary
       [using: /usr/local/bin/verible-verilog-format]
  -- : stops option processing, and forwards remaining args as flags to the
       underlying --formatter tool.
```

## Incremental Interactive Formatting

In your locally modified client (p4, git) run:

```shell
verible-verilog-format-changed-lines-interactive.sh
```

and follow the prompts.

> NOTE: git and hg users can pass in a different base revision to diff against:
>
> ```shell
> # Diff against the previous revision in hg
> verible-verilog-format-changed-lines-interactive.sh --rev .^
> # Diff against upstream mainline in git
> verible-verilog-format-changed-lines-interactive.sh --rev origin/main
> ```

## Aligned Formatting

There are several sections of code that are eligible for aligned formatting. In
each of these contexts, the user has some control over the alignment behavior.
Without alignment, the default behavior is to flush-left, which respects
indentation and minimum inter-token spacing constraints.

Yet, alignment is not always desired. So how does the formatter know whether or
not the user intended for alignment? It can only examine the original code and
make some judgment.

Consider the following example (module formal parameters):

```
module m #(
  int W,
  type T
);
  ...
endmodule
```

With alignment, this formats to:

```
module m #(
  int  W,
  type T
);
  ...
endmodule
```

When the spacing difference between between aligned and flushed-left formatting
is "sufficiently small", the formatter will align because the small difference
has low risk of compromising readability.

In contrast, aligning the following example _could_ be considered less readable.

original:

```systemverilog
module m #(
  int  W,
  some_long_name T
);
  ...
endmodule
```

aligned:

```systemverilog
module m #(
  int            W,
  some_long_name T
);
  ...
endmodule
```

To detect the above condition, the formatter examines the number of _excess_
spaces (spacing errors) within an alignment group (original vs. flushed-left).
If that value is lower than a threshold, the formatter infers that the author
intended flush-left formatting.

To induce alignment, the author needs to _inject_ four excess spaces _between_
any two tokens in any row in the aligned section, not before first tokens (which
fall under indentation, not alignment). In this example, spaces are deliberately
injected before `W` (but after `W` would work too):

```systemverilog
module m #(
  int      W,
  some_long_name T
);
  ...
endmodule
```

formatted with alignment:

```
module m #(
  int            W,
  some_long_name T
);
  ...
endmodule
```

This also implies that previously aligned code will most likely remain aligned.

Finally, if none of the above conditions hold, the formatter will leave the
original code as-is, preserving all pre-existing spaces.

## Failsafe Behavior

_A formatter must not corrupt data._

On rare conditions, you may observe that the formatter leaves the original code
untouched. You may have encountered a fail-safe condition, in which the
formatter conservatively gives up to keep your code from being corrupted. This
can happen for one of several reasons.

### Syntax Errors

On failure to lex/parse, the formatter gives up because it doesn't have a syntax
tree to work with.

### Internal Verification

Before outputting the formatted result, several properties are checked:

*   **Equivalence**: The output is lexically equivalent to the input, meaning
    that only whitespaces may have changed, but all other tokens are equal. For
    example, if two identifiers accidentally got merged together into one (by
    removing spaces between them), this check would fail. Lexical equivalence
    also implies that the output is still parseable.
*   **Convergence**: Re-formatting the _output_ results in no further changes.
    This property is particularly important when the formatter is used to
    _check_ that a file is formatted properly. Without convergence, such a check
    would fail after formatting, potentially asking the user to run formatting
    two or more times. Note that this requirement is stricter than _eventual
    convergence_, which allows multiple iterations before reaching a
    fixed-point.

### Internal Limits

If an optimization such as line-wrapping optimization fails to complete within
time resource limits, then the formatter also gives up to avoid hanging.

### Fatal Error

If the formatter crashes for any other reason, it will leave the original file
intact. The formatter does not attempt to open any file for writing in-place
until all formatting calculations have been done and internal verifications
pass.

[github-format-action]: https://github.com/chipsalliance/verible-formatter-action
