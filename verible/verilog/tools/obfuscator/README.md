# SystemVerilog Source Code Obfuscator

<!--*
freshness: { owner: 'hzeller' reviewed: '2020-10-05' }
*-->

Obfuscation is useful for preparing test cases that may have sensitive content
with tool vendors. `verible-verilog-obfuscate` transforms SystemVerilog code by
replacing identifiers with obfuscated names (of equal length), and preserving
all other text, including spaces. Output is written to stdout. The resulting
file size is the same as the original. Diagnostic positions (line, column) on
the obfuscated version of the file should correspond to positions in the
original file.

Usage: `verible-verilog-obfuscate [options] < original > output`

```
  Flags:
    --decode (If true, when used with --load_map, apply the translation
      dictionary in reverse to de-obfuscate the source code, and do not
      obfuscate any unseen identifiers. There is no need to --save_map with this
      option, because no new substitutions are established.); default: false;
    --load_map (If provided, pre-load an existing translation dictionary
      (written by --save_map). This is useful for applying pre-existing
      transforms.); default: "";
    --preserve_builtin_functions (If true, preserve built-in function names such
      as sin(), ceil()..); default: true;
    --preserve_interface (If true, module name, port names and parameter names
      will be preserved. The translation map saved with --save_map will have
      identity mappings for these identifiers. When used with --load_map, the
      mapping explicitly specified in the map file will have higher priority
      than this option.); default: false;
    --seed (Unsigned 64-bit integer seed for repeatable name generation.
      Empty uses random generation.); default: "";
    --token-size, --token_size (Length of newly generated names. 0 preserves
      original lengths; 1..4096 selects a uniform length.); default: 0;
    --save_map (If provided, save the translation to a dictionary for reuse in a
      future obfuscation with --load_map.); default: "";
```

## Repeatable and fixed-length names

```sh
verible-verilog-obfuscate --seed=123 --token-size=12 \
  --save_map=names.map < original.sv > obfuscated.sv
verible-verilog-obfuscate --decode --load_map=names.map \
  < obfuscated.sv > restored.sv
```

Without the new flags, the original random, equal-length behavior is unchanged.
`--seed=0` is a valid fixed seed. Given the same source, flags and initial map,
the same seed produces the same names and output on Windows and Linux.
Generation uses the original spelling and a per-name retry counter; candidate
generation does not depend on the order in which unrelated names are seen.
Collision resolution can depend on names already in the map, especially at
short lengths. Reuse a shared map for consistent names across separate files.
The seed is a reproducibility setting, not a cryptographic key.

`--token-size=N` (also accepted as `--token_size=N`) applies to newly generated
names. Explicit loaded mappings, preserved interface names and preserved
built-in names take priority and can have different lengths. Macro name lengths
exclude their leading backtick. Escaped identifiers remain unchanged, as before.
Whitespace, comments, strings, operators and literals remain unchanged; changing
name lengths changes file size and later token columns. Only the default
equal-length mode preserves original byte positions.

Fixed-length mode still checks token kinds, all non-identifier text and exact
decode reversibility. Decoding detects variable-length mappings automatically,
so neither the original seed nor token-size option is required to decode.
Keep the final mapping file for restoration. Very short lengths have a finite
namespace; repeated collisions report an error instead of hanging indefinitely.

Run the CLI regression against built binaries:

```sh
python verible/verilog/tools/obfuscator/seed_token_size_test.py \
  --binary bazel-bin/verible/verilog/tools/obfuscator/verible-verilog-obfuscate \
  --syntax bazel-bin/verible/verilog/tools/syntax/verible-verilog-syntax \
  --output /path/to/easyobf/results/fork
```
