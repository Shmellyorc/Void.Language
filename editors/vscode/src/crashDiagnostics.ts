import {isAbsolute} from 'node:path';

interface CrashSettings {enabled: boolean; directory: string}
let settings = (): CrashSettings => ({enabled: false, directory: ''});
export function configureCrashDiagnostics(read: () => CrashSettings): void {settings = read;}

// The extension supplies machine settings. Every launch, including preflights,
// uses the same policy; workspace files cannot turn local evidence capture on.
export function crashEnvironment(): NodeJS.ProcessEnv {
    const {enabled, directory} = settings();
    if (enabled && directory && !isAbsolute(directory))
        throw new Error('void.crashDiagnostics.directory must be an absolute path.');
    const env: NodeJS.ProcessEnv = {...process.env, VOID_CRASH_DIAGNOSTICS: enabled ? '1' : '0',
        ...(enabled && directory ? {VOID_CRASH_DIRECTORY: directory} : {})};
    delete env.VOID_CRASH_WORKER;
    return env;
}
