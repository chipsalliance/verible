# Verible's Language-Agnostic Core Library

Despite this package living under `//third_party/verible/common`, the classes
and functions defined herein are language-agnostic and have nothing to do with
Verilog.

TODO(b/140521618): Migrate out of `//third_party/verible`.

## Subdirectory Summary

*   util: generic algorithms, data structures, patterns
*   strings: functions that operate purely on strings
*   lexer: lexer interfaces and adaptors that produce token streams
*   parser: parser interfaces and adaptors that build syntax trees out of tokens
*   text: structural representations of source code (tokens, lines, trees, etc.)
*   analysis: various analysis and query facilities on text structures
*   formatting: reusable source code formatting operations
