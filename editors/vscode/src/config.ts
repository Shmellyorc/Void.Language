import * as path from 'node:path';
import {promises as fs, constants} from 'node:fs';

export interface Settings { server: string; entry: string; trace: 'off'|'messages'|'verbose'; }
export interface Project { server: string; entry: string; root: string; trace: Settings['trace']; singleFile: boolean; }

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
export async function resolveProject(settings: Settings, workspaces: readonly string[], active?: string, bundled?: string): Promise<Project> {
    if (workspaces.length > 1) throw new Error('VOID supports one project in one workspace folder. Open each project in a separate window.');
    const workspace = workspaces[0];
    const server = await discoverServer(settings.server, workspace, bundled);
    const selected = settings.entry ? resolveSetting(settings.entry, workspace) : active;
    if (!selected || path.extname(selected) !== '.void') throw new Error('Configure void.project.entry, or open a local .void file for a single-file project.');
    // Match voidls canonical root output. A new unsaved file still needs an existing parent.
    const root = await fs.realpath(path.dirname(selected));
    const entry = path.join(root, path.basename(selected));
    if (entry.startsWith('\\\\') || (process.platform !== 'win32' && entry.startsWith('//'))) throw new Error('VOID network/UNC projects are unsupported.');
    const stat = await fs.stat(server).catch(() => { throw new Error(`Cannot find voidls executable: ${server}. Build voidls and configure void.server.path.`); });
    if (!stat.isFile()) throw new Error(`voidls is not a regular executable file: ${server}`);
    if (process.platform === 'win32') {
        if (path.extname(server).toLowerCase() !== '.exe') throw new Error('Configure the native voidls.exe, not a script or shell command.');
    } else {
        await fs.access(server, constants.X_OK).catch(() => { throw new Error(`voidls is not executable: ${server}. Check its execute permissions.`); });
    }
    return {server, entry, root, trace: settings.trace, singleFile: !settings.entry};
}
