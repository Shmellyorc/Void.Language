export interface NativeReview {schema: 1; session: string; id: string; provider: string}
const seal = '\x1eVOID_NATIVE_APPROVAL_END ';
const prefix = '\x1eVOID_NATIVE_APPROVAL ';

// Only stderr from the directly spawned, capability-checked compiler enters
// this parser. Chunk boundaries and ordinary compiler diagnostics are preserved.
export class NativeReviewStream {
    private pending = '';
    private sealed = false;
    constructor(private readonly output: (text: string) => void,
        private readonly review: (request: NativeReview) => void, private readonly session: string) {}
    write(chunk: string): void {
        if (this.sealed) {this.output(chunk); return;}
        this.pending += chunk;
        let end: number;
        while ((end = this.pending.indexOf('\n')) >= 0) {
            const line = this.pending.slice(0, end);
            this.pending = this.pending.slice(end + 1);
            if (line === seal + this.session) {
                this.sealed = true; this.output(this.pending); this.pending = ''; return;
            }
            if (!line.startsWith(prefix)) { this.output(line + '\n'); continue; }
            if (line.length > 32768) throw new Error('Oversized native approval request.');
            const request: unknown = JSON.parse(line.slice(prefix.length));
            const value = request as Partial<NativeReview> | null;
            if (!value || value.session !== this.session || value.schema !== 1 || typeof value.id !== 'string' || !/^[a-f0-9]{64}$/.test(value.id) ||
                typeof value.provider !== 'string' || !value.provider || /[\x00-\x1f\x7f]/.test(value.provider))
                throw new Error('Invalid compiler native approval request.');
            this.review(value as NativeReview);
        }
        if (this.pending.length > 32768) {
            if (this.pending.startsWith(prefix)) throw new Error('Oversized native approval request.');
            this.output(this.pending); this.pending = '';
        }
    }
    end(): void {
        if (this.pending.startsWith(prefix)) throw new Error('Incomplete native approval request.');
        this.output(this.pending); this.pending = '';
    }
}
