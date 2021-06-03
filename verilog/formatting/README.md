# SystemVerilog Formatting

<!--*
freshness: { owner: 'hzeller' reviewed: '2020-10-16' }
*-->

This directory contains all of the SystemVerilog-specific formatter
implementation.

[Tool user documentation can be found here](../tools/formatter).

## Formatter Subsystems

[Full developer documentation can be found here](../../doc/formatter.md).

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

[comment_controls.h](comment_controls.h) implements comment directives thet
disable formatting on ranges of text.
