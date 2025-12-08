# SystemVerilog (Pseudo) Preprocessor

<!--*
freshness: { owner: 'hzeller' reviewed: '2022-11-16' }
*-->

`verible-verilog-preprocessor` is a collection of preprocessor-like tools, (but
does not include a fully-featured Verilog preprocessor yet.)

```shell
$ verible-verilog-preprocessor help
available commands:
  generate-variants
  preprocess
  strip-comments
```

## Preprocess

Preprocess the given file(s) and expand macros and includes.

#### Synopsis
```
verible-verilog-preprocessor preprocess [define-include-flags] file [file...]
```

#### Inputs
  Accepts one or more Verilog or SystemVerilog source files to preprocess.
  Each one of them will be prepropcessed independently which means that
  declaration scopes will end by the end of each file, and won't be seen from
  other files (so multiple files will _not_ be treated as compilation unit).
  The `+define+` and `+incdir+` directives on the commandline are honored by
  the preprocessor.

#### Output
  The preprocessed files content (same contents with directives interpreted)
  will be written to stdout, concatenated.

## Strip Comments

Removing comments can be useful for preparing to obfuscate code for sharing with
EDA tool vendors.

The `strip-comments` command strips comments from a Verilog/SystemVerilog
source file, _including_ comments nested inside macro definition bodies and
macro call arguments.

Comments are actually replaced by an equal number of spaces (and newlines) to
preserve the byte offsets and line ranges of the original text. This makes it
easier to trace back diagnostics on obfuscated code back to the original code.

#### Synopsis
```
verible-verilog-preprocessor strip-comments file [replacement-char]
```

#### Inputs
  `file` is a Verilog or SystemVerilog source file (Use `-` to read from stdin).
  The optional `replacement-char` is a character to replace comments with.
  If not given, or given as a single space character, the comment contents and
  delimiters are replaced with spaces.
  If an empty string, the comment contents and delimiters are deleted. Newlines
  are not deleted.
  If a single character, the comment contents are replaced with the character.

#### Output
  Writes to stdout with contents of original file with `//` and `/**/`
  comments removed.


## Generate Variants

Generates a number of variants of the input file by selectively switching
on/off available macros.

#### Synopsis
```
verible-verilog-preprocessor generate-variants file [-limit_variants number]
```


#### Inputs
  `file` is a Verilog or SystemVerilog source file.
  `-limit_variants` flag limits variants to 'number' (20 by default).

#### Output
   Output to stdout. Generates every possible variant of `ifdef
   blocks considering the conditional directives.
