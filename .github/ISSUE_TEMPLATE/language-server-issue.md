---
name: Language Server
about: Language Server (LSP) issues
title: ''
labels: language-server
assignees: ''

---

A couple of questions first

 - Your IDE/editor (e.g. vscode, emacs,...) you use with verible LSP ?
 - IDE version:
 - What other SystemVerilog plugins are active alongside ?
-----

**What activity failed**
_(what functionality did not work as expected (e.g. Linting, Formatting, AUTO-expansion, go-to-definition, outline, hover,...)_

**Expectation**

**What actually happened**

**Test case**

If needed, include SystemVerilog sample file(s) that show the code you want to do an operation on
```systemverilog
// Sample SystemVerilog file in case
```

**Logfiles**

If you invoke your editor with the environment variable `VERIBLE_LOGTHRESHOLD` set to 0, then useful log information will
be generated that you can include here

```bash
export VERIBLE_LOGTHRESHOLD=0
code foo.sv
```

***How to see the log output***
 - _vscode_: go to `View -> Output`, then choose `Verible Language Server` in the dropdown
 - _emacs_: there is a buffer `*verible-ls::stderr*` that contains the log output
 - _kate_: output is printed on the terminal

```txt
(paste log output here)
```
