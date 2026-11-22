import * as vscode from 'vscode';
import {crashEnvironment} from './crashDiagnostics';
import {spawn, ChildProcessWithoutNullStreams} from 'node:child_process';
import {constants} from 'node:os';
import {compilerFileIdentity} from './config';
import {NativeReviewStream} from './nativeApproval';
import {nativeReviewUI} from './nativeReviewUI';

export interface CompilerInvocation {compiler: string; args: string[]; cwd: string; capabilityIdentity?: string; verifyApproval?: () => Promise<void>}

export interface CompilerCompletion {code: number; spawned: boolean; cancelled: boolean}
export function compilerExecution(invocation: CompilerInvocation | (() => Promise<CompilerInvocation>), completed?: (result: CompilerCompletion) => void | Promise<void>): vscode.CustomExecution {
    // Task ProcessExecution substitutes ${...} in paths and arguments. Launch
    // directly so the compiler and application receive the exact argument vector.
    return new vscode.CustomExecution(async () => {
        const output = new vscode.EventEmitter<string>();
        const finished = new vscode.EventEmitter<number>();
        let process: ChildProcessWithoutNullStreams | undefined;
        let launchFailed = false;
        let closed = false;
        let spawned = false;
        let didFinish = false;
        let reviewing = false;
        let reviewUI: ReturnType<typeof nativeReviewUI> | undefined;
        let approvalSession = '';
        let approvalEnabled = false;
        let verifyApproval: (() => Promise<void>) | undefined;
        const finish = async (code: number) => {
            if (didFinish) return;
            didFinish = true; reviewUI?.dispose(); reviewUI = undefined;
            try { await completed?.({code, spawned, cancelled: closed}); }
            catch { /* Receipt failure cannot manufacture compiler success. */ }
            finished.fire(code);
            output.dispose();
            finished.dispose();
        };
        return {
            onDidWrite: output.event,
            onDidClose: finished.event,
            open: async () => {
                try {
                    const selected = typeof invocation === 'function' ? await invocation() : invocation;
                    const {compiler, args, cwd, capabilityIdentity} = selected;
                    verifyApproval = selected.verifyApproval;
                    approvalEnabled = args.includes('--native-approval-stdio');
                    approvalSession = approvalEnabled ? args[args.indexOf('--native-approval-stdio') + 1] : '';
                    if (capabilityIdentity && await compilerFileIdentity(compiler) !== capabilityIdentity)
                        throw new Error('The compiler changed after capability negotiation. Invoke the command again.');
                    if (closed) { finish(1); return; }
                    process = spawn(compiler, args, {cwd, shell: false, windowsHide: true, env: crashEnvironment()});
                } catch (error) {
                    output.fire(`VOID: ${error instanceof Error ? error.message : String(error)}\r\n`);
                    finish(1);
                    return;
                }
                process.once('spawn', () => { spawned = true; });
                const streamOutput = (stream: NodeJS.ReadableStream) => {
                    let previousCR = false;
                    stream.setEncoding('utf8').on('data', (chunk: string) => {
                        const text = (previousCR ? '\r' : '') + chunk;
                        output.fire(text.replace(/(?<!\r)\n/g, '\r\n').slice(previousCR ? 1 : 0));
                        previousCR = chunk.endsWith('\r');
                    });
                };
                streamOutput(process.stdout);
                if (approvalEnabled) {
                    const child = process;
                    const parser = new NativeReviewStream(text => output.fire(text.replace(/(?<!\r)\n/g, '\r\n')), request => {
                        if (reviewing || !verifyApproval) { child.kill(); return; }
                        reviewing = true;
                        output.fire('Review native-code approval in the VS Code dialog. Review expires after 120 seconds.\r\n');
                        void (async () => {
                            try {
                                reviewUI = nativeReviewUI(request);
                                const decision = await reviewUI.decision;
                                reviewUI = undefined;
                                if (closed || process !== child) return;
                                await verifyApproval();
                                child.stdin.write(decision ? `approve ${request.id}\n` : 'deny\n');
                            } catch (error) {
                                output.fire(`VOID: ${error instanceof Error ? error.message : String(error)}\r\n`);
                                child.kill();
                            } finally { reviewing = false; }
                        })();
                    }, approvalSession);
                    const fail = (error: unknown) => {output.fire(`VOID: ${String(error)}\r\n`); launchFailed = true; child.kill();};
                    child.stderr.setEncoding('utf8').on('data', (chunk: string) => {try {parser.write(chunk);} catch(error) {fail(error);}});
                    child.stderr.on('end', () => {try {parser.end();} catch(error) {fail(error);}});
                } else streamOutput(process.stderr);
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
            close: () => { closed = true; reviewUI?.dispose(); process?.kill(); },
            handleInput: input => {
                if (input === '\x03') process?.kill('SIGINT');
                else if (input === '\x04') process?.stdin.end();
                else if (!reviewing) process?.stdin.write(input.replaceAll('\r', '\n'));
            }
        };
    });
}
