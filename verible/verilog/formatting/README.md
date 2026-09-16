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
formatter. Text-in-text-out tests are split by concern to reduce merge
conflicts:

- [formatter_test.cc](formatter_test.cc) — core smoke / `VerifyFormatting`
- [formatter_macro_test.cc](formatter_macro_test.cc) — preprocessor, `` `uvm ``, directives
- [formatter_module_test.cc](formatter_module_test.cc) — modules, ports, nets, expressions
- [formatter_class_package_test.cc](formatter_class_package_test.cc) — classes, packages, constraints
- [formatter_function_task_test.cc](formatter_function_task_test.cc) — functions, tasks, properties
- [formatter_align_test.cc](formatter_align_test.cc) — tabular / param alignment
- [formatter_wrap_style_test.cc](formatter_wrap_style_test.cc) — wrap/indent options, diagnostics
- [formatter_disable_test.cc](formatter_disable_test.cc) — `verilog_format` on/off
- [formatter_issue_regression_test.cc](formatter_issue_regression_test.cc) — GitHub issue regressions
- Shared helpers: [formatter-test-utils.h](formatter-test-utils.h)

**New GitHub-issue formatter regressions belong in
`formatter_issue_regression_test.cc` only** (do not append them to
`formatter_test.cc`).

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
