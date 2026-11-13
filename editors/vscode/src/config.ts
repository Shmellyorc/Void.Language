import * as path from 'node:path';
import {execFile} from 'node:child_process';
import {promisify} from 'node:util';
import {promises as fs, constants} from 'node:fs';

export interface Settings { server: string; entry: string; root?: string; trace: 'off'|'messages'|'verbose'; }
export interface Project { server: string; entry: string | null; root: string; manifestMode: boolean; requestedRoot?: string; entryOverride?: string; trace: Settings['trace']; singleFile: boolean; }

// Explicit paths are literal. Discovery searches only the installed PATH.
export function resolveSetting(value: string, workspace?: string, paths: path.PlatformPath = path): string {
    if (!value || value.includes('\0')) throw new Error('Configure void.server.path and void.project.entry with valid literal paths.');
    if (paths.sep === '\\' && (/^[a-z]:($|[^\\/])/i.test(value) || /^[\\/](?![\\/])/i.test(value)))
        throw new Error('Use a full Windows drive path or a workspace-relative path, not a drive-relative/root-relative path.');
    if (paths.isAbsolute(value)) return paths.normalize(value);
    if (!workspace) throw new Error('Relative VOID paths require one workspace folder. Use an absolute path instead.');
    return paths.resolve(workspace, value);
}
export function inside(root: string, file: string, paths: path.PlatformPath = path): boolean {
    const relative = paths.relative(root, file);
    return relative !== '..' && !relative.startsWith('..' + paths.sep) && !paths.isAbsolute(relative);
}
export async function discoverServer(explicit: string, workspace?: string, bundled?: string, search = process.env.PATH ?? ''): Promise<string> {
    if (explicit) return resolveSetting(explicit, workspace);
    if (bundled) { try { if ((await fs.stat(bundled)).isFile()) return bundled; } catch { /* PATH fallback */ } }
    const executable = process.platform === 'win32' ? 'voidls.exe' : 'voidls';
    for (const directory of search.split(path.delimiter)) {
        if (!path.isAbsolute(directory)) continue;
        const candidate = path.join(directory, executable);
        try { if ((await fs.stat(candidate)).isFile()) { await fs.access(candidate, process.platform === 'win32' ? constants.F_OK : constants.X_OK); return candidate; } } catch { /* next PATH entry */ }
    }
    throw new Error('Cannot find voidls. Configure void.server.path or install the native server on PATH.');
}
export async function resolveServer(explicit: string, workspace?: string, bundled?: string): Promise<string> {
    const server = await discoverServer(explicit, workspace, bundled);
    const stat = await fs.stat(server).catch(() => { throw new Error(`Cannot find voidls executable: ${server}. Build voidls and configure void.server.path.`); });
    if (!stat.isFile()) throw new Error(`voidls is not a regular executable file: ${server}`);
    if (process.platform === 'win32') {
        if (path.extname(server).toLowerCase() !== '.exe') throw new Error('Configure the native voidls.exe, not a script or shell command.');
    } else {
        await fs.access(server, constants.X_OK).catch(() => { throw new Error(`voidls is not executable: ${server}. Check its execute permissions.`); });
    }
    return server;
}
export async function resolveCompiler(server: string, explicit: string, workspace: string): Promise<string> {
    const compiler = explicit ? resolveSetting(explicit, workspace) :
        path.join(path.dirname(server), process.platform === 'win32' ? 'voidc.exe' : 'voidc');
    const stat = await fs.stat(compiler).catch(() => undefined);
    if (!stat?.isFile() || (process.platform === 'win32' && path.extname(compiler).toLowerCase() !== '.exe'))
        throw new Error('Set void.compiler.path to the matching native voidc executable.');
    if (process.platform !== 'win32') await fs.access(compiler, constants.X_OK);
    return compiler;
}
export async function resolveProject(settings: Settings, workspaces: readonly string[], active?: string, bundled?: string): Promise<Project> {
    if (workspaces.length > 1) throw new Error('VOID supports one project in one workspace folder. Open each project in a separate window.');
    const workspace = workspaces[0];
    const server = await resolveServer(settings.server, workspace, bundled);
    const override = settings.entry ? resolveSetting(settings.entry, workspace) : '';
    const selectedRoot = settings.root ? resolveSetting(settings.root, workspace) : '';
    let entryOverride: string | undefined;
    if (selectedRoot && override) {
        if (!inside(selectedRoot, override)) throw new Error('Project entry is outside the selected manifest root.');
        entryOverride = path.relative(selectedRoot, override).split(path.sep).join('/');
    }
    // Explicit legacy entry settings keep the old endpoint/server behavior.
    // Automatic/manifest selection requires the new compiler-owned preflight.
    const legacy = !selectedRoot && !!override;
    const args = legacy ? ['--discover', workspace ?? '', active ?? '', override] :
        ['--discover-project', workspace ?? '', active ?? '', selectedRoot, entryOverride ?? ''];
    let stdout: string;
    // Debug/instrumented analysis of a large locked package can exceed ten seconds.
    try { ({stdout} = await promisify(execFile)(server, args, {encoding: 'utf8', maxBuffer: 65536, timeout: 60000})); }
    catch (error) { throw new Error(`VOID project preflight failed. Automatic/manifest selection requires a matching voidProjectProtocol 1 server; no legacy downgrade was attempted. ${String(error)}`); }
    const selected = JSON.parse(stdout) as {schema?: number; mode?: string; requestedRoot?: string; entry: string|null; root: string; standalone: boolean; status?: string};
    const {entry, root} = selected;
    const manifestMode = !legacy && selected.schema === 2 && selected.mode === 'project';
    if ((!legacy && (selected.schema !== 2 || !['project','legacy'].includes(selected.mode ?? ''))) ||
        typeof root !== 'string' || !path.isAbsolute(root) || typeof selected.standalone !== 'boolean' ||
        (entry !== null && (typeof entry !== 'string' || !path.isAbsolute(entry))) || (!manifestMode && entry === null) ||
        (manifestMode && (typeof selected.requestedRoot !== 'string' || !path.isAbsolute(selected.requestedRoot))))
        throw new Error('Invalid voidls project discovery response');
    return {server, entry, root, manifestMode, requestedRoot: selected.requestedRoot, entryOverride, trace: settings.trace, singleFile: selected.standalone};
}
