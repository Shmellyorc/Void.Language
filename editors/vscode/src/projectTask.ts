import * as vscode from 'vscode';
import {spawn, ChildProcessWithoutNullStreams} from 'node:child_process';
import {constants} from 'node:os';

export interface CompilerInvocation {compiler: string; args: string[]; cwd: string}

export function compilerExecution(invocation: CompilerInvocation | (() => Promise<CompilerInvocation>)): vscode.CustomExecution {
    // Task ProcessExecution substitutes ${...} in paths and arguments. Launch
    // directly so the compiler and application receive the exact argument vector.
    return new vscode.CustomExecution(async () => {
        const output = new vscode.EventEmitter<string>();
        const finished = new vscode.EventEmitter<number>();
        let process: ChildProcessWithoutNullStreams | undefined;
        let launchFailed = false;
        let closed = false;
        const finish = (code: number) => {
            finished.fire(code);
            output.dispose();
            finished.dispose();
        };
        return {
            onDidWrite: output.event,
            onDidClose: finished.event,
            open: async () => {
                try {
                    const {compiler, args, cwd} = typeof invocation === 'function' ? await invocation() : invocation;
                    if (closed) { finish(1); return; }
                    process = spawn(compiler, args, {cwd, shell: false, windowsHide: true});
                } catch (error) {
                    output.fire(`VOID: ${error instanceof Error ? error.message : String(error)}\r\n`);
                    finish(1);
                    return;
                }
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
                    finish(launchFailed ? 1 : code ?? (signal ? 128 + constants.signals[signal] : 1));
                });
                // The child may exit or close its stdin while the user is typing.
                process.stdin.on('error', () => undefined);
            },
            close: () => { closed = true; process?.kill(); },
            handleInput: input => {
                if (input === '\x03') process?.kill('SIGINT');
                else if (input === '\x04') process?.stdin.end();
                else process?.stdin.write(input.replaceAll('\r', '\n'));
            }
        };
    });
}
