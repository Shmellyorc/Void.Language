// One receipt per managed launch. A VS Code task summary (including its synthetic
// "Debug Anyway" success) is never evidence that our compiler child succeeded.
export interface BuildSelection {root: string; program: string; entry: string}
export class DebugBuildReceipts {
    private serial = 0;
    private running?: number;
    private attempt?: {id: number; selection: BuildSelection; state: 'pending'|'running'|'success'|'failure'|'cancelled'};
    begin(selection: BuildSelection): number {
        if (this.running !== undefined)
            throw new Error('A VOID Debug build launch is already pending. Stop it before starting another.');
        // The host can abandon a launch without cancelling its provider token.
        // A new attempt supersedes that receipt; its task must carry its own ID.
        const id = ++this.serial;
        this.attempt = {id, selection, state: 'pending'};
        return id;
    }
    current(): number | undefined { return this.attempt?.id; }
    start(id: number, selection: BuildSelection): void {
        const attempt = this.attempt;
        if (!attempt || attempt.id !== id || attempt.state !== 'pending' ||
            attempt.selection.root !== selection.root || attempt.selection.program !== selection.program || attempt.selection.entry !== selection.entry)
            throw new Error('The VOID Debug build invocation changed. Start debugging again.');
        attempt.state = 'running';
        this.running = id;
    }
    complete(id: number, success: boolean): boolean {
        if (this.running === id) this.running = undefined;
        const attempt = this.attempt;
        if (attempt?.id === id && ['pending','running'].includes(attempt.state)) {
            attempt.state = success && attempt.state === 'running' ? 'success' : 'failure';
            return attempt.state === 'success';
        }
        return false;
    }
    cancel(id: number): void { if (this.attempt?.id === id) this.attempt.state = 'cancelled'; }
    require(id: unknown, selection: BuildSelection): void {
        const attempt = this.attempt;
        if (!attempt || id !== attempt.id || attempt.state !== 'success' ||
            attempt.selection.root !== selection.root || attempt.selection.program !== selection.program || attempt.selection.entry !== selection.entry) {
            if (typeof id === 'number') this.cancel(id);
            throw new Error('VOID requires this launch’s compiler process to finish successfully before debugging.');
        }
        this.attempt = undefined; // A successful receipt is consumed once.
    }
}
