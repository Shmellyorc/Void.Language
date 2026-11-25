// Uses the existing pinned extension-test dependency and built extension.
// NODE=/absolute/node; $NODE run_editor_probe.cjs /absolute/editor /absolute/voidls
const fs = require('node:fs/promises');
const path = require('node:path');
const os = require('node:os');
const repo = path.resolve(__dirname, '../../..');
const {runTests} = require(path.join(repo, 'editors/vscode/node_modules/@vscode/test-electron'));
async function main() {
  const root = await fs.mkdtemp(path.join(os.tmpdir(), 'void-stage7-editor-'));
  try {
    await fs.mkdir(path.join(root, '.vscode'));
    await fs.writeFile(path.join(root, 'main.void'), 'use "/worker.void" as worker\nfunc entry(x: i32) -> i32:\n    return worker.calculate(x)\n');
    await fs.writeFile(path.join(root, 'worker.void'), 'func calculate(x: i32) -> i32:\n    return x + 1\n');
    await fs.writeFile(path.join(root, '.vscode/settings.json'), JSON.stringify({'void.server.path':path.resolve(process.argv[3]), 'void.project.entry':'main.void'}));
    await runTests({vscodeExecutablePath:process.argv[2],
      extensionDevelopmentPath:path.join(repo, 'editors/vscode'),
      extensionTestsPath:path.join(__dirname, 'editor_probe.cjs'),
      extensionTestsEnv:{VOID_STAGE7_FIXTURE:root, VOID_STAGE7_OUTPUT:path.join(__dirname, 'baseline-vscode-linux.json')},
      launchArgs:[root, '--user-data-dir='+path.join(root, 'profile'), '--extensions-dir='+path.join(root, 'extensions'),
        '--no-sandbox', '--disable-gpu-sandbox', '--disable-gpu', '--skip-welcome', '--skip-release-notes', '--disable-extensions', '--disable-updates']});
  } finally {await fs.rm(root, {recursive:true,force:true});}
}
main().catch(error => {console.error(error);process.exitCode=1;});
