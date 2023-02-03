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
  - [x] Generate file symbol outline ('navigation tree')
  - [x] Provide formatting.
  - [x] Highlight all the symbols that are the same as current under cursor.
    - [ ] Take scope and type into account to only highlight _same_ symbols.
  - [ ] Provide useful information on hover
        ([#1187](https://github.com/chipsalliance/verible/issues/1187))
  - [ ] Find definition of symbol even if in another file.
        ([#1189](https://github.com/chipsalliance/verible/issues/1189))
  - [ ] Provide Document Links (e.g. opening include files)
        ([#1190](https://github.com/chipsalliance/verible/issues/1190))
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

Make sure to have version `0.5.0` or newer and install the [nvim-lspconfig](https://github.com/neovim/nvim-lspconfig/) plugin.
You can install it with the popular [vim-plug](https://github.com/junegunn/vim-plug) plugin manager by adding the following code to your ~/.config/nvim/init.vim file:
```vim
call plug#begin()
Plug 'neovim/nvim-lspconfig'
call plug#end()
```
Then to install it open neovim and type: ``` :PlugInstall ```

After installing nvim-lspconfig, enable the verible config by appending the following script to your ~/.config/nvim/init.nvim file:

```lua
lua << EOF

-- Mappings.
-- See `:help vim.diagnostic.*` for documentation on any of the below functions
local opts = { noremap=true, silent=true }
vim.keymap.set('n', '<space>e', vim.diagnostic.open_float, opts)
vim.keymap.set('n', '[d', vim.diagnostic.goto_prev, opts)
vim.keymap.set('n', ']d', vim.diagnostic.goto_next, opts)
vim.keymap.set('n', '<space>q', vim.diagnostic.setloclist, opts)

-- Use an on_attach function to only map the following keys
-- after the language server attaches to the current buffer
local on_attach = function(client, bufnr)
  -- Enable completion triggered by <c-x><c-o>
  vim.api.nvim_buf_set_option(bufnr, 'omnifunc', 'v:lua.vim.lsp.omnifunc')

  -- Mappings.
  -- See `:help vim.lsp.*` for documentation on any of the below functions
  local bufopts = { noremap=true, silent=true, buffer=bufnr }
  vim.keymap.set('n', 'gD', vim.lsp.buf.declaration, bufopts)
  vim.keymap.set('n', 'gd', vim.lsp.buf.definition, bufopts)
  vim.keymap.set('n', 'K', vim.lsp.buf.hover, bufopts)
  vim.keymap.set('n', 'gi', vim.lsp.buf.implementation, bufopts)
  vim.keymap.set('n', '<C-k>', vim.lsp.buf.signature_help, bufopts)
  vim.keymap.set('n', '<space>wa', vim.lsp.buf.add_workspace_folder, bufopts)
  vim.keymap.set('n', '<space>wr', vim.lsp.buf.remove_workspace_folder, bufopts)
  vim.keymap.set('n', '<space>wl', function()
    print(vim.inspect(vim.lsp.buf.list_workspace_folders()))
  end, bufopts)
  vim.keymap.set('n', '<space>D', vim.lsp.buf.type_definition, bufopts)
  vim.keymap.set('n', '<space>rn', vim.lsp.buf.rename, bufopts)
  vim.keymap.set('n', '<space>a', vim.lsp.buf.code_action, bufopts)
  vim.keymap.set('n', 'gr', vim.lsp.buf.references, bufopts)
  vim.keymap.set('n', '<space>f', vim.lsp.buf.formatting, bufopts)
end

local lsp_flags = {
  -- This is the default in Nvim 0.7+
  debounce_text_changes = 150,
}

require'lspconfig'.verible.setup {
    on_attach = on_attach,
    flags = lsp_flags,
    root_dir = function() return vim.loop.cwd() end
}
EOF
```
This script initializes the verible language server in neovim and also enables shortcuts for functionalities such as auto-fix (space + a).
See https://github.com/neovim/nvim-lspconfig/blob/master/doc/server_configurations.md#verible for configuration options.

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

To make LSP work in Vim an dedicated LSP addon is required.
The recommendeda addon is [vim-lsp](https://github.com/prabirshrestha/vim-lsp).
Please refer to its README for installation guides and configuration recommendations.

To enable Verible with this plugin, add the following snippet to your configuration (e.g. ``~/.vimrc``):

```viml
if executable('verible-verilog-ls')
    au User lsp_setup call lsp#register_server({
        \ 'name': 'verible-verilog-ls',
        \ 'cmd': {server_info->['verible-verilog-ls']},
        \ 'allowlist': ['verilog', 'systemverilog'],
        \ })
endif
```

Make sure ``verible-verilog-ls`` is available in your ``PATH`` and can be executed.
Alternatively modify the snippet above to use an absolute path.

### VSCode

#### Use released extension
You can get the extension from the [release] files and download `verible.vsix`.

Then, run vscode with the following flag to install the extension:

```bash
code --install-extension verible.vsix
```

#### Build yourself

This is based on the VSCode [packaging extension](https://code.visualstudio.com/api/working-with-extensions/publishing-extension#packaging-extensions) guide.

First [install the verible tools](../../README.md#installation) and [vscode](https://code.visualstudio.com/Download).

For the following, you need a recent version of both
[nodejs](https://nodejs.org/) and [npm](https://www.npmjs.com/).

Change into the [vscode/](./vscode) subdirectory and run the following:

```bash
npm install
npm run vsix
code --install-extension verible.vsix
```

[release]: https://github.com/chipsalliance/verible/releases
