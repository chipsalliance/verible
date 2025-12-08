# Lint Rules

<!--*
freshness: { owner: 'hzeller' reviewed: '2020-10-07' }
*-->

This directory provides implementations of all lint rules. Lint rules make
themselves available by registering themselves (upon library loading), so they
can be referenced and selected by name.

User documentation for the lint rules is generated dynamically, and can be found
at https://chipsalliance.github.io/verible/verilog_lint.html, or by running
`verible-verilog-lint --help_rules` for text or `--generate_markdown`.

[Linter user documentation can be found here](../../tools/lint).
