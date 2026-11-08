import {ChildProcess} from 'node:child_process';
import {channel} from 'node:diagnostics_channel';
import {downloadAndUnzipVSCode, DownloadOptions, makeConsoleReporter, ProgressReporter, ProgressReportStage} from '@vscode/test-electron';

// test-electron 3.1.0 leaves the tar input open after a failed stream. Finish
// each owned extractor before its retry can remove/reuse the extraction tree.
// The upstream downloader still checks the archive and chooses the executable.
export async function downloadEditor(
    acquire: (options: Partial<DownloadOptions>) => Promise<string> = options => downloadAndUnzipVSCode(options),
    reporter?: ProgressReporter,
): Promise<string> {
    const output = reporter ?? await makeConsoleReporter();
    const processes = channel('child_process');
    for (let attempt = 0; attempt < 3; ++attempt) {
        const owned = new Map<ChildProcess, Promise<void>>();
        const track = (message: unknown) => {
            const child = (message as {process: ChildProcess}).process;
            owned.set(child, new Promise(resolve => child.once('close', () => resolve())));
        };
        let retry: Error | undefined;
        processes.subscribe(track);
        try {
            return await acquire({version: '1.141.0', reporter: {
                report(event) {
                    if (event.stage === ProgressReportStage.Retrying) {
                        retry = event.error;
                        throw retry; // Delay the retry until owned pipes/processes close.
                    }
                    output.report(event);
                },
                error(error) { output.error(error); },
            }});
        } catch (error) {
            if (error !== retry || attempt === 2) throw error;
            output.report({stage: ProgressReportStage.Retrying, error: retry!, attempt: attempt + 1, totalAttempts: 3});
        } finally {
            processes.unsubscribe(track);
            for (const child of owned.keys()) {
                if (child.exitCode === null && child.signalCode === null) {
                    child.stdin?.on('error', error => output.error(error));
                    child.stdin?.destroy();
                }
            }
            await Promise.all(owned.values());
        }
    }
    throw new Error('Editor download attempts exhausted');
}
