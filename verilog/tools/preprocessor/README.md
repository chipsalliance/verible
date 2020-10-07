# SystemVerilog (Pseudo) Preprocessor

<!--*
freshness: { owner: 'fangism' reviewed: '2020-10-04' }
*-->

`verible-verilog-preprocessor` is a collection of preprocessor-like tools, (but
does not include a fully-featured Verilog preprocessor yet.)

## Strip Comments {#strip-comments}

Removing comments can be useful for preparing to obfuscate code for sharing with
EDA tool vendors.

```shell
$ verible-verilog-preprocessor strip-comments FILE
```

This strips comments from a Verilog/SystemVerilog source file, _including_
comments nested inside macro definition bodies and macro call arguments.

Comments are actually replaced by an equal number of spaces (and newlines) to
preserve the byte offsets and line ranges of the original text. This makes it
easier to trace back diagnostics on obfuscated code back to the original code.
