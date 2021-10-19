# SystemVerilog Language Server

The language server can be hooked up to your IDE or text editor to provide
assistance while programming. It helps highlight syntax errors or lint
violations and provides fixes for issues if they are available.

The language server is part of the Verible suite and called
`verible-verilog-ls`.

### This is work in progress

There are a few changes in the review pipeline, so here is the current
progress

  - [x] Publish diagnostics for syntax errors and lint rules
    - [ ] Use lint configuration from `.rules.verible_lint` instead of all enabled
  - [x] Provide code actions for autofixes provided by lint rules
  - [ ] Generate file symbol outline ('navigation tree')
  - [ ] Provide formatting.
  - [ ] Highlight all the symbols that are the same as current under cursor.
  - [ ] Find definition of symbol even if in another file.
  - [ ] Rename refactor a symbol

## Hooking up to editor

After [installing the verible tools](../../../README.md#installation), you
can configure your editor to use the language server.

This will be specific to your editor. In essence, you need to tell
it to start the `verible-verilog-ls` language server whenever it works with
Verilog or SystemVerilog files.

Here are a few common editors, but there are many more that support language
servers. If you have configured it for your editor, consider sending
a pull request that adds a section to this README (or file an issue an
mention what you had to do and we add it here).

In alphabetical order

### Emacs
The `lsp-mode` needs to be installed from wherever you get your emacs
packages.

Here is a simple setup: put this in your `~/.emacs` file
and make sure the binary is in your `$PATH` (or use full path).

```lisp
(require 'lsp-mode)
(add-to-list 'lsp-language-id-configuration '(verilog-mode . "verilog"))
(lsp-register-client
 (make-lsp-client :new-connection (lsp-stdio-connection "verible-verilog-ls")
                  :major-modes '(verilog-mode)
                  :server-id 'verible-ls))

(add-hook 'verilog-mode-hook 'lsp)
```

### Kate

https://docs.kde.org/trunk5/en/kate/kate/kate-application-plugin-lspclient.html

For our example, it seems that kate does not consider 'text' a separate
language, so let's configure that in markdown.

First, enable LSP by checking `Settings > Configure Kate > Plugins > LSP Client`
Then, there is a new `{} LSP Client` icon appearing on the left of the configure dialog. In the _User Server Settings_ tab, enter the lsp server configuration
to get it started up on our Verilog/SystemVerilog projects.

```json
{
    "servers": {
        "verilog": {
            "command": ["verible-verilog-ls"],
            "root": "",
            "url": "https://github.com/chipsalliance/verible"
        },
        "systemverilog": {
            "command": ["verible-verilog-ls"],
            "root": "",
            "url": "https://github.com/chipsalliance/verible"
        }
    }
}
```

### Neovim

TBD (similar to Vim ?)

### Sublime
Consult https://lsp.readthedocs.io/

Installation steps

  1. Enable package control if not already `Tools > Install Package control...`
  2. Install LSP base package: `Preferences > Package Control` search for
     `Install Package`. Confirm, then search for `LSP`.
  3. Also, while at it, if you haven't already, install the `SystemVerilog`
     package, which gives  you general syntax highlighting.
  4. Go to `Preferences > Package Settings > LSP > Settings`. It opens
     a global setting file and a user setting skeleton. Put the following
     in your user `LSP.sublime-settings`; it already provides the empty outer
     braces, you need to add the `"clients"` section.

```json
// Settings in here override those in "LSP/LSP.sublime-settings"
{
  "clients": {
    "verible-verilog-ls": {
      "command": ["verible-verilog-ls"],
      "enabled": true,
      "selector": "source.systemverilog"
    }
  }
}
```

There is a `Tools > LSP > Troubleshoot Server Configuration` which might
be helpful in case of issues.

### Vim

TBD. What I found so far, there is
[vim-lsp](https://github.com/prabirshrestha/vim-lsp) that can be used.
There is also neovim.
