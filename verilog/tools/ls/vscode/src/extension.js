"use strict";
Object.defineProperty(exports, "__esModule", { value: true });
exports.deactivate = exports.activate = void 0;
const vscode = require("vscode");
const vscodelc = require("vscode-languageclient/node");
const download_ls_1 = require("./download-ls");
const util_1 = require("util");
// Global object to dispose of previous language clients.
let client = undefined;
async function initLanguageClient() {
    const output = vscode.window.createOutputChannel("Verible Language Server");
    const config = vscode.workspace.getConfiguration("verible");
    const binary_path = await (0, download_ls_1.checkAndDownloadBinaries)(config.get("path"), output);
    output.appendLine(`Using executable from path: ${binary_path}`);
    const verible_ls = {
        command: binary_path,
        args: await config.get("arguments"),
    };
    const serverOptions = verible_ls;
    // Options to control the language client
    const clientOptions = {
        // Register the server for (System)Verilog documents
        documentSelector: [
            { scheme: "file", language: "systemverilog" },
            { scheme: "file", language: "verilog" },
        ],
        outputChannel: output,
    };
    // Create the language client and start the client.
    output.appendLine("Starting Language Server");
    client = new vscodelc.LanguageClient("verible", "Verible Language Server", serverOptions, clientOptions);
    client.start();
}
function restartLanguageClient() {
    if (!client) {
        return initLanguageClient();
    }
    return client.stop().finally(() => {
        initLanguageClient();
    });
}
function bindGenerateFileListCommand(ctx) {
    const cmd = "verible.generateFileList";
    const encoder = new util_1.TextEncoder();
    const cmdHandler = () => {
        const fs = vscode.workspace.fs;
        vscode.workspace.workspaceFolders?.forEach(async (folder) => {
            var folders = ["."];
            var fileList = [];
            while (folders.length > 0) {
                var cur = folders.pop();
                var files = await fs.readDirectory(vscode.Uri.joinPath(folder.uri, cur));
                files.forEach(([fileName, type]) => {
                    switch (type) {
                        case vscode.FileType.File:
                            if (fileName.endsWith(".sv") ||
                                fileName.endsWith(".v") ||
                                fileName.endsWith(".vh")) {
                                fileList.push(cur + "/" + fileName);
                            }
                            break;
                        case vscode.FileType.Directory:
                            // console.log(cur + "/" + fileName);
                            folders.push(cur + "/" + fileName);
                            break;
                    }
                });
            }
            fs.writeFile(vscode.Uri.joinPath(folder.uri, "verible.filelist"), encoder.encode(fileList.join("\n")));
        });
        return restartLanguageClient();
    };
    ctx.subscriptions.push(vscode.commands.registerCommand(cmd, cmdHandler));
}
// VSCode entrypoint to bootstrap an extension
function activate(context) {
    // Bind Commands
    bindGenerateFileListCommand(context);
    // If a configuration change even it fired, let's dispose
    // of the previous client and create a new one.
    vscode.workspace.onDidChangeConfiguration(async (event) => {
        if (!event.affectsConfiguration("verible")) {
            return;
        }
        restartLanguageClient();
    });
    return initLanguageClient();
}
exports.activate = activate;
// Entrypoint to tear it down.
function deactivate() {
    if (!client) {
        return undefined;
    }
    return client.stop();
}
exports.deactivate = deactivate;
//# sourceMappingURL=extension.js.map