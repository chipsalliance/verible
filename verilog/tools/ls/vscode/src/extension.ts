import * as vscode from 'vscode';
import * as vscodelc from 'vscode-languageclient/node';
import { checkAndDownloadBinaries } from './download-ls';

// Global object to dispose of previous language clients.
let client: undefined | vscodelc.LanguageClient = undefined;

async function initLanguageClient() {
    const output = vscode.window.createOutputChannel('Verible Language Server');
    const config = vscode.workspace.getConfiguration('verible');
    const binary_path: string = await checkAndDownloadBinaries(config.get('path') as string, output);

    output.appendLine(`Using executable from path: ${binary_path}`);

    const verible_ls: vscodelc.Executable = {
        command: binary_path,
        args: await config.get<string[]>('arguments')
    };

    const serverOptions: vscodelc.ServerOptions = verible_ls;

    // Options to control the language client
    const clientOptions: vscodelc.LanguageClientOptions = {
        // Register the server for (System)Verilog documents
        documentSelector: [{ scheme: 'file', language: 'systemverilog' },
                           { scheme: 'file', language: 'verilog' }],
        outputChannel: output
    };

    // Create the language client and start the client.
    output.appendLine("Starting Language Server");
    client = new vscodelc.LanguageClient(
        'verible',
        'Verible Language Server',
        serverOptions,
        clientOptions
    );
    client.start();
}

// VSCode entrypoint to bootstrap an extension
export function activate(_: vscode.ExtensionContext) {
    // If a configuration change even it fired, let's dispose
    // of the previous client and create a new one.
    vscode.workspace.onDidChangeConfiguration(async (event) => {
        if (!event.affectsConfiguration('verible')) {
            return;
        }
        if (!client) {
            return initLanguageClient();
        }
        client.stop().finally(() => {
            initLanguageClient();
        });
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
