# SystemVerilog Formatting - ECE2300

<!--*
freshness: { owner: 'Anthony Song' reviewed: '2026-08-01' }
*-->

This directory contains all of the SystemVerilog-specific formatter
implementation.

[Tool user documentation can be found here](../tools/formatter).

## ECE2300 Fork Formatting

This fork changes the upstream formatter output in the following ways.

### Module declarations

The opening parenthesis of a module declaration's port list starts on its own
line, and the ports are expanded one per line:

```systemverilog
module foo
(
    input a,
    input b,
    output c
);
endmodule
```

### Module instantiations

Module instantiations are always expanded, including instances short enough to
fit on one line. The opening parenthesis starts on its own line, each named port
connection is placed on its own line, and there is one space between a named
port and its parenthesized expression:

```systemverilog
foo bar
(
    .a (a),
    .b (b)
);
```

### Preserved constructs

Formatting is disabled over complete `task`...`endtask` and
`casez`...`endcase` syntax-tree ranges so that hand-formatted ECE2300 code
inside those constructs is preserved. The formatter can still adjust the
indentation before the opening keyword when it formats the surrounding code.
Ordinary `case` statements and `function` declarations are not exempt and
continue to use the upstream formatting rules.

## Formatter Subsystems

[Full developer documentation can be found here](../../../doc/formatter.md).

Below is a quick summary of the major components.

[formatter.h](formatter.h) contains the top-level entry point into the
formatter. Text-in-text-out tests are in [formatter_test.cc](formatter_test.cc).

[FormatStyle](format_style.h) defines ways in which formatting can be
configured.

[token_annotator.h](token_annotator.h) marks up a token stream with formatting
constraints such as minimum spacing, and always/never-wrap.

[TreeUnwrapper](tree_unwrapper.h) converts a SV syntax tree into a
language-agnostic TokenPartitionTree representation for doing formatting
operations.

[align.h](align.h) implements everything related to tabular alignment of
specific sections of code.

[comment_controls.h](comment_controls.h) implements comment directives that
disable formatting on ranges of text.
