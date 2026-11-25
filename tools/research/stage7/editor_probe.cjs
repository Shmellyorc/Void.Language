// Research-only VS Code Extension Test entrypoint, launched by run_editor_probe.cjs.
const vscode = require('vscode');
const fs = require('node:fs');
const path = require('node:path');
const assert = require('node:assert/strict');
const now = () => performance.now();
const summarize = values => ({samples: values.length,
  median_ms: [...values].sort((a,b) => a-b)[Math.floor(values.length/2)],
  raw_ms: values});
exports.run = async function () {
  const root = process.env.VOID_STAGE7_FIXTURE;
  const mainUri = vscode.Uri.file(path.join(root, 'main.void'));
  const workerUri = vscode.Uri.file(path.join(root, 'worker.void'));
  const started = now();
  const main = await vscode.workspace.openTextDocument(mainUri);
  await vscode.window.showTextDocument(main);
  const extension = vscode.extensions.getExtension('void-language.void-language');
  const api = await extension.activate(); await api.ready();
  const definitions = () => vscode.commands.executeCommand('vscode.executeDefinitionProvider', mainUri, new vscode.Position(2, 18));
  assert.equal((await definitions())[0].uri.toString(), workerUri.toString());
  const result = {editor_version: vscode.version, node: process.versions.node,
    open_activate_ready_definition_ms: now() - started,
    note: 'Already running editor; activation may start on document open. Excludes GUI launch. Diagnostic timings include client debounce and actual editor event delivery.'};
  const worker = await vscode.workspace.openTextDocument(workerUri);
  const original = worker.getText();
  async function replaceAndObserve(text, invalid) {
    const start = now();
    let timer;
    const observed = new Promise((resolve, reject) => {
      const sub = vscode.languages.onDidChangeDiagnostics(event => {
        if (!event.uris.some(u => u.toString() === workerUri.toString())) return;
        const diagnostics = vscode.languages.getDiagnostics(workerUri);
        if (invalid ? diagnostics.some(d => (typeof d.code === 'object' ? d.code.value : d.code) === 'VOID2002') : diagnostics.length === 0) {
          sub.dispose(); clearTimeout(timer); resolve(now() - start);
        }
      });
      timer = setTimeout(() => {sub.dispose(); reject(new Error('diagnostic event timeout'));}, 20000);
    });
    const edit = new vscode.WorkspaceEdit();
    edit.replace(workerUri, new vscode.Range(worker.positionAt(0), worker.positionAt(worker.getText().length)), text);
    assert(await vscode.workspace.applyEdit(edit));
    return observed;
  }
  const errors = [], repairs = [], query = [];
  for (let i=0; i<7; i++) {
    errors.push(await replaceAndObserve(original.replace('x + 1', 'missing'), true));
    assert.equal((await definitions()).length, 0);
    repairs.push(await replaceAndObserve(original, false));
  }
  for (let i=0; i<70; i++) {
    const t = now(); assert.equal((await definitions())[0].uri.toString(), workerUri.toString()); query.push(now()-t);
  }
  result.invalid_edit_diagnostic = summarize(errors);
  result.repair_diagnostic = summarize(repairs);
  result.definition_command = summarize(query);
  const t = now(); await api.deactivate(); result.deactivate_ms = now()-t;
  fs.writeFileSync(process.env.VOID_STAGE7_OUTPUT, JSON.stringify(result, null, 2) + '\n');
};
