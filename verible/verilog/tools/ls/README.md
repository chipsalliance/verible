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
    - [x] Use lint configuration from `.rules.verible_lint` instead of all enabled
  - [x] Provide code actions for autofixes provided by lint rules
  - [x] Generate file symbol outline ('navigation tree')
  - [x] Provide formatting.
  - [x] Highlight all the symbols that are the same as current under cursor.
    - [ ] Take scope and type into account to only highlight _same_ symbols.
  - [ ] Provide useful information on hover
        ([#1187](https://github.com/chipsalliance/verible/issues/1187))
  - [x] Find definition of a symbol even if in another file (check [Configuring the Language Server for a project](#configuring-the-language-server-for-a-project)).
  - [x] Find references of a symbol even if in another file (check [Configuring the Language Server for a project](#configuring-the-language-server-for-a-project)).
  - [ ] Find declaration of a symbol even if in another file.
        ([#1189](https://github.com/chipsalliance/verible/issues/1189))
  - [ ] Provide Document Links (e.g. opening include files)
        ([#1190](https://github.com/chipsalliance/verible/issues/1190))
  - [ ] Rename refactor a symbol

## Configuring the Language Server for a project

### Adding `verible.filelist` for project-wide symbol discovery

`verible-verilog-ls` by default loads and analyses only currently edited files in the editor.
To be able to utilize such features as going to definition, going to references, printing hover info project-wide, add a `verible.filelist` file to the project.

`verible.filelist` is a file containing a list of design files (listed line by line).
It is used to collect data necessary to build a symbol table and other helper structures to allow Language Server to find symbols' origin and type to support afore-mentioned features.

If a list of files used in design is already present, it can just be linked in the project root under name `verible.filelist`.
Otherwise, it can be easily created using such tools as `find`, e.g.:

```bash
find . -name "*.sv" -o -name "*.svh" -o -name "*.v" | sort > verible.filelist
```

The paths in the `verible.filelist` can be either relative to the location of the file or absolute.
It is possible to change the default name of the filelist with the `--file_list_path <new-file-name>` flag.

### Project Root
The Language Server looks for the `verible.filelist` file in the project root.

#### From Editor
The project root is typically determined by the editor and sent to the language server.
The provided workspace directory can vary between editors, usually it is a directory:

* Where the editor was started,
* Where the editor found the root of the project (e.g. based on `.git` directory)
* That is provided by the user in the project's settings in the editor.

#### Fallback
If there is no valid project root directory provided by the editor, Verible falls back to use the current directory.

You can override the project root that is used with the environment variable `VERIBLE_LS_PROJECTROOT_OVERRIDE`.
If that is set, it takes precedence over the editor-provided project root.

### Adding project-specific linter configuration

By default, Language Server publishes all possible linting issues for a given document.
It is possible to disable certain linter warnings for a given project using the `.rules.verible_lint` file.
It is a simple file that consists of comma-separated or newline-separated settings for rules, e.g.:

```
-module-filename
+posix-eof
-no-tabs
```

Disables the check for matching module and file name, tabs instead of spaces and enables rule disallowing ending file without an empty newline.
For more information on linter setup and available flags, check [SystemVerilog Style Linter](../lint/README.md).

Possibly the easiest way to introduce per-project linter configuration for the Language Server would be to run it with `--rules_config_search` path.
It will search for the `.rules.verible_lint` file up in the directory hierarchy with respect to the file's current path.

It is also possible to provide a direct path to the linter configuration, e.g.:
```bash
verible-verilog-ls --rules_config <path-to-config>
```

Or provide rules configuration directly, e.g.:
```bash
verible-verilog-ls --rules=+line-length=length:80,-no-tabs
```

### Other customizations of the Language Server

To check other configuration options for the `verible-verilog-ls`, run:

```bash
verible-verilog-ls --helpfull
```

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

```elisp
(require 'lsp-mode)
(add-to-list 'lsp-language-id-configuration '(verilog-mode . "verilog"))
(lsp-register-client
 (make-lsp-client :new-connection (lsp-stdio-connection "verible-verilog-ls")
                  :major-modes '(verilog-mode)
                  :server-id 'verible-ls))

(add-hook 'verilog-mode-hook 'lsp)
```

It is also possible to automatically configure `eglot` and `lsp-mode` using
the [verilog-ext](https://github.com/gmlarumbe/verilog-ext.git) package:
```elisp
(require 'verilog-ext)
(verilog-ext-mode-setup)
(verilog-ext-eglot-set-server 've-verible-ls) ;`eglot' config
(verilog-ext-lsp-set-server 've-verible-ls)   ; `lsp' config
```

### Kakoune

First, go to [kak-lsp](https://github.com/kak-lsp/kak-lsp) project and follow the installation and configuration steps.
Then, either find a `kak-lsp.toml` language server configuration file or create a new one in the `~/.config/kak-lsp` directory (using [a default template](https://github.com/kak-lsp/kak-lsp/blob/master/kak-lsp.toml) from the project repository).

After this, in the `kak-lsp.toml` file create a new entry:

```toml
[languages.verilog]
filetypes = ["v", "sv"]
roots = ["verible.filelist", ".git"]
command = "verible-verilog-ls"
offset_encoding = "utf-8"
```

To add additional configuration arguments to the `verible-verilog-ls`, add `args` list, e.g.:

```toml
args = ["--rules_config_search"]
```

Later, in the `kakrc` file (usually located in `~/.config/kak/kakrc`), adjust a hook for enabling Language Server to start for `verilog` language, e.g.:

```kak
hook global WinSetOption filetype=(rust|python|go|javascript|typescript|c|cpp|verilog) %{
    lsp-enable-window
}
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

It is possible to provide additional flags and arguments in `command` entry as list, e.g. `["verible-verilog-ls", "--rules_config_search"]`.

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
    root_dir = function() return vim.uv.cwd() end
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

To customize the Language Server with additional flags, type the command flags in the `command` list, e.g. `["verible-verilog-ls", "--rules_config_search"]`.

### Vim

To make LSP work in Vim a dedicated LSP plugin is required.

The recommended plugin is [vim-lsp](https://github.com/prabirshrestha/vim-lsp), which is compatible with Vim8 and later releases.
Please refer to its README for installation guides and configuration recommendations.

To enable Verible with the following plugins, add the corresponding snippet to your configuration file (e.g. ``~/.vimrc``):

Configure with [vim-lsp](https://github.com/prabirshrestha/vim-lsp):

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
To add configuration flags to customize the Language Server, add them to the list in the `server_info`.


Alternatively, Verible can be configured with [lsp](https://github.com/yegappan/lsp), which is written in vim9script and compatible with `vim v9+`.

Configure with [lsp](https://github.com/yegappan/lsp):

```viml
call LspAddServer([#{
    \   name: 'verible-verilog-ls',
    \   filetype: ['systemverilog', 'verilog'],
    \   path: 'verible-verilog-ls',
    \   args: []
    \ }])
```

Add configuration flags to args (eg. `args: ['--column_limit=80']`).

### VSCode

#### Use released extension

You can install the extension directly from the [VSCode marketplace](https://marketplace.visualstudio.com/items?itemName=CHIPSAlliance.verible).

To install the extension launch VS Code Quick Open (Ctrl+P), paste the following command, and press enter.

```
ext install CHIPSAlliance.verible
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

#### Configuring Language Server arguments

To configure the extension, in Extensions list select Verible, and select `Extension Settings`.
In there you can find an `Arguments` setting, where you can add command-line arguments for `verible-verilog-ls` executable, e.g.:

* `--rules_config_search` - search recursively for linter configuration, starting from edited file's directory.
* `--rules_config="<path-to-config>"` - use linter configuration in a specified path
* `--wrap_end_else_clauses` - splits `end else` into separate lines.
* `--indentation_spaces=4` - indent width specified for the formatter.

There should be one flag per item.

For more language server options, check:

```bash
verible-verilog-ls --helpfull
```
