# Verible Language Server Extension

## Features
The language server provides a couple of features from the [Verible SystemVerilog productivity suite](https://github.com/chipsalliance/verible) right in the editor.

 * **Linting**: Checks your code against a number of
   [lint rules](https://chipsalliance.github.io/verible/lint.html) and provides
   'wiggly lines' with diagnostic output and even offers auto-fixes when available.
 * **Formatting**: Offers Format Document/Selection according to the Verible
   formatting style. The 'look' can be configured if needed.
 * **Outline**: Shows the high-level structure of your modules and functions in the
   outline tree. Labelled begin/end blocks are also included.
 * **Hover**: Highlight symbols related to the one under the cursor.
 * **[&#x1F389; New]** **Go-To-Definition**: Jump to the definition of the symbol under the cursor.
 * **[&#x1F389; New]** **AUTO**-expansion: Features known from
   [emacs verilog mode](https://www.veripool.org/verilog-mode/). Currently
   `AUTOARG` (`AUTO_INST`, `AUTO_TEMPLATE` [coming soon](https://github.com/chipsalliance/verible/issues/1557)).

## Filing Issues
File bugs on the public [github issue tracker](https://github.com/chipsalliance/verible/issues/new/choose). Provide (sanitized) code examples if needed to illustrate an issue.
