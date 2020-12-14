# SystemVerilog Source Code Obfuscator

<!--*
freshness: { owner: 'fangism' reviewed: '2020-10-05' }
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
    --preserve_interface (If true, module name, port names and parameter names
      will be preserved. The translation map saved with --save_map will have
      identity mappings for these identifiers. When used with --load_map, the
      mapping explicitly specified in the map file will have higher priority
      than this option.); default: false;
    --save_map (If provided, save the translation to a dictionary for reuse in a
      future obfuscation with --load_map.); default: "";
```
