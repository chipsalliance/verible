import * as vscode from "vscode";
import * as vscodelc from "vscode-languageclient/node";
import { checkAndDownloadBinaries } from "./download-ls";
import { TextEncoder } from "util";

// Global object to dispose of previous language clients.
let client: undefined | vscodelc.LanguageClient = undefined;

async function initLanguageClient() {
  const output = vscode.window.createOutputChannel("Verible Language Server");
  const config = vscode.workspace.getConfiguration("verible");
  const binary_path: string = await checkAndDownloadBinaries(
    config.get("path") as string,
    output
  );

  output.appendLine(`Using executable from path: ${binary_path}`);

  const verible_ls: vscodelc.Executable = {
    command: binary_path,
    args: await config.get<string[]>("arguments"),
  };

  const serverOptions: vscodelc.ServerOptions = verible_ls;

  // Options to control the language client
  const clientOptions: vscodelc.LanguageClientOptions = {
    // Register the server for (System)Verilog documents
    documentSelector: [
      { scheme: "file", language: "systemverilog" },
      { scheme: "file", language: "verilog" },
    ],
    outputChannel: output,
  };

  // Create the language client and start the client.
  output.appendLine("Starting Language Server");
  client = new vscodelc.LanguageClient(
    "verible",
    "Verible Language Server",
    serverOptions,
    clientOptions
  );
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

function bindGenerateFileListCommand(ctx: vscode.ExtensionContext) {
  const cmd = "verible.generateFileList";
  const encoder = new TextEncoder();

  const cmdHandler = () => {
    const fs = vscode.workspace.fs;
    vscode.workspace.workspaceFolders?.forEach(async (folder) => {
      var folders = ["."];
      var fileList: string[] = [];
      while (folders.length > 0) {
        var cur = folders.pop()!;
        var files = await fs.readDirectory(
          vscode.Uri.joinPath(folder.uri, cur)
        );
        files.forEach(([fileName, type]) => {
          switch (type) {
            case vscode.FileType.File:
              if (
                fileName.endsWith(".sv") ||
                fileName.endsWith(".v") ||
                fileName.endsWith(".vh")
              ) {
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
      fs.writeFile(
        vscode.Uri.joinPath(folder.uri, "verible.filelist"),
        encoder.encode(fileList.join("\n"))
      );
    });
    return restartLanguageClient();
  };
  ctx.subscriptions.push(vscode.commands.registerCommand(cmd, cmdHandler));
}

// VSCode entrypoint to bootstrap an extension
export function activate(context: vscode.ExtensionContext) {
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

// Entrypoint to tear it down.
export function deactivate(): Thenable<void> | undefined {
  if (!client) {
    return undefined;
  }
  return client.stop();
}
