# VS Code Extension

## Installation
Be sure your system as a recent version of both [nodejs](https://nodejs.org/)
and [npm](https://www.npmjs.com/).
From the directory of this Readme, run `npm install`.

After [installing the verible tools](../../../README.md#installation),
run `npm run vsix` to generate the extension file `verible.vsix`.

Finally, run `code --install-extension verible.vsix` to install the extension.

# References
- [Language server extension guide][vscode-lsp]
- [Packaging the extension][packaging]

[vscode-lsp]: https://code.visualstudio.com/api/language-extensions/language-server-extension-guide
[packaging]: https://code.visualstudio.com/api/working-with-extensions/publishing-extension#packaging-extensions
