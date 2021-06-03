# SystemVerilog Lexical Diff Tool

<!--*
freshness: { owner: 'hzeller' reviewed: '2020-10-04' }
*-->

`verible-verilog-diff` compares two SystemVerilog source files and reports the
first lexical difference. Equivalence is determined by the comparison `--mode`.

*   `--mode=format` Checks for equivalence of text ignoring whitespaces.
*   `--mode=obfuscate` Checks for equivalence including spaces, and verifies
    lengths of identifiers.

Equivalence analysis also looks inside macro definition bodies and macro call
arguments, recursively.

Exit codes:

*   0: files are equivalent
*   1: files differ, or contain lexical errors
*   2: error reading file
