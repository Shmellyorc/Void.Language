import {spawn} from 'node:child_process';
import {promises as fs} from 'node:fs';
import * as path from 'node:path';

async function main(): Promise<void> {
    const editor = process.env.VSCODE_EXECUTABLE_PATH;
    const profile = process.env.VOID_METADATA_PROFILE;
    if (!editor || !profile || !(await fs.stat(profile)).isDirectory())
        throw new Error('Set VSCODE_EXECUTABLE_PATH and VOID_METADATA_PROFILE to an existing editor and test profile. Uses voidc/voidls on PATH.');
    const extension = path.resolve(__dirname, '../..');
    const root = path.resolve(extension, '../../tests/lsp/fixtures/editor_metadata/app');
    const child = spawn(editor, [root, '--user-data-dir=' + path.resolve(profile),
        '--extensionDevelopmentPath=' + extension,
        '--extensionTestsPath=' + path.join(__dirname, 'metadataRefreshHost.js'),
        '--disable-extensions', '--disable-workspace-trust', '--no-sandbox',
        '--disable-gpu', '--disable-updates', '--skip-welcome', '--skip-release-notes', '--new-window'],
        {env: process.env, stdio: 'inherit'});
    const code = await new Promise<number | null>((resolve, reject) => {
        child.once('error', reject);
        child.once('exit', resolve);
    });
    if (code !== 0) throw new Error('Metadata refresh editor test exited ' + code);
}

main().catch(error => { console.error(error); process.exitCode = 1; });
