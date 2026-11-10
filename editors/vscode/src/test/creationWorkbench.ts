import assert from 'node:assert/strict';

export const pause = (ms: number) => new Promise(resolve => setTimeout(resolve, ms));
export const offer = 'Start a VOID project for these first sources?';

export async function until<T>(read: () => T | Promise<T>, accept: (value: T) => boolean, label: string): Promise<T> {
    const end = Date.now() + 15000;
    while (Date.now() < end) {
        const value = await read();
        if (accept(value)) return value;
        await pause(100);
    }
    throw new Error('Timed out waiting for ' + label);
}

export async function workbench(port: string) {
    const target = await until(async () => {
        try {
            const targets = await (await fetch(`http://127.0.0.1:${port}/json/list`)).json() as {url: string; webSocketDebuggerUrl: string}[];
            return targets.find(t => t.url.includes('workbench.html'));
        } catch { return undefined; }
    }, Boolean, 'the VS Code workbench');
    assert(target);
    const socket = new WebSocket(target.webSocketDebuggerUrl);
    await new Promise<void>((resolve, reject) => {
        socket.addEventListener('open', () => resolve(), {once: true});
        socket.addEventListener('error', reject, {once: true});
    });
    let sequence = 0;
    const replies = new Map<number, (value: unknown) => void>();
    socket.addEventListener('message', event => {
        const message = JSON.parse(String(event.data));
        replies.get(message.id)?.(message.result?.result?.value);
        replies.delete(message.id);
    });
    function send<T>(method: string, params: object): Promise<T> {
        return new Promise(resolve => {
            const id = ++sequence;
            replies.set(id, result => resolve(result as T));
            socket.send(JSON.stringify({id, method, params}));
        });
    }
    const evaluate = <T>(expression: string) => send<T>('Runtime.evaluate', {expression, returnByValue: true});
    const key = (value: string, code: string, windowsVirtualKeyCode: number, modifiers = 0) => {
        // Fire both events without waiting for a reply. Quit can close the socket.
        for (const type of ['keyDown', 'keyUp'])
            socket.send(JSON.stringify({id: ++sequence, method: 'Input.dispatchKeyEvent',
                params: {type, key: value, code, windowsVirtualKeyCode, modifiers}}));
    };
    const visible = (needle: string, region: string) => evaluate<number>(
        `[...document.querySelectorAll(${JSON.stringify(region)})].filter(e => e.getBoundingClientRect().width && e.innerText.includes(${JSON.stringify(needle)})).length`);
    const click = async (label: string, region: string) => {
        await until(() => evaluate<boolean>(`(() => {
            const buttons = [...document.querySelectorAll(${JSON.stringify(region + ' .monaco-button, ' + region + ' .monaco-list-row')})];
            const button = buttons.find(e => e.getBoundingClientRect().width && (e.querySelector('.label-name')?.textContent ?? e.innerText).trim() === ${JSON.stringify(label)});
            if (!button) return false;
            button.click();
            return true;
        })()`), Boolean, label);
    };
    return {evaluate, visible, click, key, close: () => socket.close()};
}
