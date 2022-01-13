import * as vscode from 'vscode';
import * as vscodelc from 'vscode-languageclient/node';

// Global object to dispose of previous language clients.
let client: undefined | vscodelc.LanguageClient = undefined;

function initLanguageClient() {
    const config = vscode.workspace.getConfiguration('verible');
    const binary_path: string = config.get('path') as string;

    const clangd: vscodelc.Executable = {
        command: binary_path
    };

    const serverOptions: vscodelc.ServerOptions = clangd;

    // Options to control the language client
    const clientOptions: vscodelc.LanguageClientOptions = {
        // Register the server for plain text documents
        documentSelector: [{ scheme: 'file', language: 'system-verilog' }]
    };

    // Create the language client and start the client.
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
    vscode.workspace.onDidChangeConfiguration((event) => {
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
