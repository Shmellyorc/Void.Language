import * as vscode from 'vscode';
import {spawn, ChildProcessWithoutNullStreams} from 'node:child_process';
import {constants} from 'node:os';

export function compilerExecution(compiler: string, args: string[], cwd: string): vscode.CustomExecution {
    // Task ProcessExecution substitutes ${...} in paths and arguments. Launch
    // directly so the compiler and application receive the exact argument vector.
    return new vscode.CustomExecution(async () => {
        const output = new vscode.EventEmitter<string>();
        const finished = new vscode.EventEmitter<number>();
        let process: ChildProcessWithoutNullStreams | undefined;
        let launchFailed = false;
        return {
            onDidWrite: output.event,
            onDidClose: finished.event,
            open: () => {
                process = spawn(compiler, args, {cwd, shell: false, windowsHide: true});
                const streamOutput = (stream: NodeJS.ReadableStream) => {
                    let previousCR = false;
                    stream.setEncoding('utf8').on('data', (chunk: string) => {
                        const text = (previousCR ? '\r' : '') + chunk;
                        output.fire(text.replace(/(?<!\r)\n/g, '\r\n').slice(previousCR ? 1 : 0));
                        previousCR = chunk.endsWith('\r');
                    });
                };
                streamOutput(process.stdout);
                streamOutput(process.stderr);
                process.on('error', error => {
                    launchFailed = true;
                    output.fire(`VOID: ${error.message}\r\n`);
                });
                process.on('close', (code, signal) => {
                    process = undefined;
                    finished.fire(launchFailed ? 1 : code ?? (signal ? 128 + constants.signals[signal] : 1));
                    output.dispose();
                    finished.dispose();
                });
                // The child may exit or close its stdin while the user is typing.
                process.stdin.on('error', () => undefined);
            },
            close: () => { process?.kill(); },
            handleInput: input => {
                if (input === '\x03') process?.kill('SIGINT');
                else if (input === '\x04') process?.stdin.end();
                else process?.stdin.write(input.replaceAll('\r', '\n'));
            }
        };
    });
}
