import * as vscode from 'vscode';
import assert from 'node:assert/strict';
import {promises as fs} from 'node:fs';
import {join} from 'node:path';
import {until, workbench} from './creationWorkbench';

export async function run(): Promise<void> {
    const root = vscode.workspace.workspaceFolders![0].uri.fsPath;
    const evidence = process.env.VOID_F5_EVIDENCE!;
    const ui = await workbench(process.env.VOID_TEST_CDP_PORT!);
    const extension = vscode.extensions.getExtension('void-language.void-language')!;
    const api = await extension.activate() as {ready(): Promise<void>; deactivate(): Promise<void>};
    const source = vscode.Uri.file(join(root, 'src/app.void'));
    const document = await vscode.workspace.openTextDocument(source);
    const editor = await vscode.window.showTextDocument(document);
    const stops: Array<{session: vscode.DebugSession; threadId: number}> = [];
    const launches: string[] = [], builds: number[] = [], breakpoints: unknown[] = [];
    let ended = 0;
    const owned: vscode.Disposable[] = [vscode.tasks.onDidEndTaskProcess(e => {
        if (e.execution.task.definition.command === 'debugBuild') builds.push(e.exitCode ?? -1);
    }), vscode.debug.onDidTerminateDebugSession(() => ++ended),
    vscode.debug.registerDebugAdapterTrackerFactory('lldb-dap', {createDebugAdapterTracker(session) {
        return {onWillReceiveMessage(message) {if (message.command === 'launch') launches.push(message.arguments.program);},
            onDidSendMessage(message) {
                if (message.event === 'stopped') stops.push({session, threadId: message.body.threadId});
                if (message.type === 'response' && message.command === 'setBreakpoints') breakpoints.push(message.body);
            }};
    }})];
    const bp = new vscode.SourceBreakpoint(new vscode.Location(source, new vscode.Position(9, 0)));
    vscode.debug.addBreakpoints([bp]);
    const chooseReview = async (label: string) => {
        await ui.evaluate('document.querySelector(".quick-input-widget input").focus()');
        await ui.type(label);
        await until(() => ui.evaluate<boolean>(`document.querySelector('.quick-input-widget input')?.value === ${JSON.stringify(label)} &&
            document.querySelector('.quick-input-widget .monaco-list-row.focused .label-name')?.textContent === ${JSON.stringify(label)}`), Boolean, 'focused review action');
        ui.key('Enter', 'Enter', 13);
    };
    const start = () => vscode.debug.startDebugging(vscode.workspace.workspaceFolders![0],
        {type: 'void', request: 'launch', name: 'VOID F5 Experience'});
    try {
        await api.ready();
        assert(extension.packageJSON.contributes.debuggers.some((d: {type: string; label: string}) => d.type === 'void' && d.label === 'VOID Project'));
        await assert.rejects(fs.stat(join(root, '.vscode/launch.json')), {code: 'ENOENT'});
        await assert.rejects(fs.stat(join(root, '.vscode/tasks.json')), {code: 'ENOENT'});
        const discovery = vscode.commands.executeCommand('workbench.action.debug.selectandstart');
        await until(() => ui.visible('VOID', '.quick-input-widget'), Boolean, 'VOID in Run and Debug selector');
        await fs.writeFile(join(evidence, 'debug-discovery-dom.txt'), await ui.evaluate<string>('document.querySelector(".quick-input-widget")?.innerText'));
        ui.key('Escape', 'Escape', 27); await discovery;
        const denied = start();
        await until(() => ui.visible('Allow native code for this VOID application?', '.quick-input-widget'), Boolean, 'native review dialog');
        assert(await ui.visible('org.voidlang.raylib', '.quick-input-widget'));
        await chooseReview('Deny');
        assert.equal(await denied, false);
        assert.deepEqual(launches, []);
        assert.deepEqual(builds, [2]);
        await assert.rejects(fs.stat(join(root, '.void/build/app')), {code: 'ENOENT'});
        const approved = start();
        await until(() => ui.visible('Approval ID (SHA-256)', '.quick-input-widget'), Boolean, 'fresh native review');
        await fs.writeFile(join(evidence, 'native-review-dom.txt'), await ui.evaluate<string>('document.querySelector(".quick-input-widget")?.innerText'));
        await chooseReview('Approve Native Code');
        assert(await approved); console.log('F5 approved build launched');
        const stop = await until(() => stops[0], Boolean, 'raylib frame breakpoint');
        console.log('F5 source breakpoint hit');
        const trace = await stop.session.customRequest('stackTrace', {threadId: stop.threadId});
        const frame = trace.stackFrames[0];
        assert.equal(frame.source.path, source.fsPath); assert.equal(frame.line, 10);
        assert(frame.column <= 1, JSON.stringify(frame));
        const scopes = await stop.session.customRequest('scopes', {frameId: frame.id});
        const locals = scopes.scopes.find((s: {name: string}) => s.name === 'Locals');
        assert(locals);
        const variables = await stop.session.customRequest('variables', {variablesReference: locals.variablesReference});
        assert(variables.variables.some((v: {name: string; type: string}) => v.name === 'dt' && ['f32', 'float'].includes(v.type)), JSON.stringify(variables));
        console.log('F5 locals:', JSON.stringify(variables));
        const evaluations: Record<string, unknown> = {};
        for (const context of ['watch', 'hover', 'repl']) {
            console.log('F5 evaluate', context);
            const response = await stop.session.customRequest('evaluate', {expression: 'dt', frameId: frame.id, context});
            assert(!response.result.includes('error'), JSON.stringify(response)); evaluations[context] = response;
        }
        assert(trace.stackFrames.some((f: {name: string}) => f.name.includes('main')));
        await vscode.commands.executeCommand('workbench.view.debug');
        await ui.evaluate(`(() => {
            const row = [...document.querySelectorAll('.debug-variables .monaco-list-row')].find(e => e.innerText.trim() === 'Locals');
            const twistie = row?.querySelector('.monaco-tl-twistie');
            if (twistie?.classList.contains('collapsed')) twistie.click();
        })()`);
        await until(() => ui.visible('observed', '.debug-variables'), Boolean, 'visible scalar locals');
        editor.selection = new vscode.Selection(7, 12, 7, 12);
        const commands = await vscode.commands.getCommands(true);
        const hover = commands.find(command => /showDebugHover$/.test(command));
        assert(hover, commands.filter(command => /hover/i.test(command)).join('\n'));
        await vscode.commands.executeCommand(hover);
        const expectedHover = (evaluations.hover as {result: string}).result;
        await until(() => ui.visible(expectedHover, '.debug-hover-widget'), Boolean, 'paused debug hover widget');
        ui.key('Escape', 'Escape', 27);
        await ui.moveMouse(20, 20);
        const point = await ui.evaluate<{x: number; y: number}>(`(() => {
            const token = [...document.querySelectorAll('.view-line span')].find(e => e.textContent === 'dt' && e.getBoundingClientRect().width);
            const r = token.getBoundingClientRect(); return {x:r.x+r.width/2,y:r.y+r.height/2};
        })()`);
        await ui.moveMouse(point.x, point.y);
        await until(() => ui.visible(expectedHover, '.debug-hover-widget'), Boolean, 'mouse debug hover value');
        await fs.writeFile(join(evidence, 'paused-hover.png'), Buffer.from((await ui.screenshot()).data, 'base64'));
        const hoverText = await ui.evaluate<string>('document.querySelector(".debug-hover-widget")?.innerText');
        await fs.writeFile(join(evidence, 'hover-dom.txt'), hoverText);
        await fs.writeFile(join(evidence, 'editor-debug.json'), JSON.stringify({vscode: vscode.version, extension: extension.extensionPath,
            builds, launches, frame, trace, variables, evaluations, breakpoints, hoverText}, null, 2));
        await stop.session.customRequest('next', {threadId: stop.threadId});
        const step = await until(() => stops[1], Boolean, 'raylib source step');
        const stepped = await step.session.customRequest('stackTrace', {threadId: step.threadId});
        assert.equal(stepped.stackFrames[0].line, 11);
        await vscode.debug.stopDebugging();
        await until(() => ended, count => count >= 1, 'debug stop');
        // An exact saved approval launches again without another review dialog.
        assert(await start());
        const reused = await until(() => stops[2], Boolean, 'saved approval breakpoint');
        assert.equal(await ui.visible('Allow native code', '.quick-input-widget'), 0);
        assert.deepEqual(builds, [2, 0, 0]);
        await vscode.debug.stopDebugging(reused.session);
        const catalog = join(root, 'libs/raylib/.void/native/void.native.toml');
        const original = await fs.readFile(catalog);
        try {
            await fs.appendFile(catalog, '\n# Fresh identity requires review\n');
            const originalArtifact = await fs.readFile(join(root, '.void/build/app'));
            const canceled = start();
            await until(() => ui.visible('Approval ID (SHA-256)', '.quick-input-widget'), Boolean, 'task cancellation review');
            for (const execution of vscode.tasks.taskExecutions)
                if (execution.task.definition.command === 'debugBuild') execution.terminate();
            assert.equal(await canceled, false);
            await until(() => ui.visible('Allow native code', '.quick-input-widget'), count => count === 0, 'canceled review disposal');
            assert.equal(launches.length, 2);
            assert.deepEqual(await fs.readFile(join(root, '.void/build/app')), originalArtifact);
            const changed = start();
            await until(() => ui.visible('Approval ID (SHA-256)', '.quick-input-widget'), Boolean, 'changed provider review');
            ui.key('Escape', 'Escape', 27);
            assert.equal(await changed, false);
            assert.equal(launches.length, 2);
            assert.deepEqual(await fs.readFile(join(root, '.void/build/app')), originalArtifact);
        } finally {await fs.writeFile(catalog, original);}
        console.log('PASS F5: no launch/tasks JSON; deny, approve, reuse, task cancellation, changed identity cancel; native f32 Locals/Watch/REPL/hover, line breakpoint, source step and stack');
    } finally {
        if (vscode.debug.activeDebugSession) await vscode.debug.stopDebugging();
        vscode.debug.removeBreakpoints([bp]);
        for (const item of owned) item.dispose();
        ui.close(); await api.deactivate();
    }
}
