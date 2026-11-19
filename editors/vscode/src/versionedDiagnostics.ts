import * as vscode from 'vscode';
import {LanguageClient, PublishDiagnosticsNotification} from 'vscode-languageclient/node';
import {DiagnosticVersions} from './diagnosticVersions';

export function versionedDiagnostics(client: LanguageClient, isCurrent: () => boolean,
    update: (uri: vscode.Uri, diagnostics: vscode.Diagnostic[]) => void): void {
    const versions = new DiagnosticVersions<vscode.TextDocument>();
    client.registerFeature({
        fillClientCapabilities(capabilities) {
            capabilities.textDocument ??= {};
            capabilities.textDocument.publishDiagnostics ??= {};
            capabilities.textDocument.publishDiagnostics.versionSupport = true;
        },
        initialize() {}, getState: () => ({kind: 'static'}), clear: () => versions.clear()
    });
    // Public pending notification registration replaces the client's unversioned
    // diagnostic queue before initialized is sent; no private client APIs.
    client.onNotification(PublishDiagnosticsNotification.type, async params => {
        if (!isCurrent()) return;
        const uri = client.protocol2CodeConverter.asUri(params.uri);
        const document = () => vscode.workspace.textDocuments.find(d => d.uri.toString() === uri.toString());
        const ticket = versions.begin(uri.toString(), document(), params.version);
        if (!ticket) return;
        try {
            const diagnostics = await client.protocol2CodeConverter.asDiagnostics(params.diagnostics);
            if (!isCurrent() || !versions.current(ticket, document())) return;
            client.diagnostics?.set(uri, diagnostics);
            update(uri, diagnostics);
        } finally {versions.finish(ticket);}
    });
}
