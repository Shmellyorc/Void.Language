export interface DiagnosticDocument {readonly version: number; readonly isClosed: boolean}
export interface DiagnosticTicket<T extends DiagnosticDocument> {
    readonly uri: string; readonly serial: number; readonly document: T | undefined; readonly version: number | undefined;
}

// Conversion may finish after a newer notification, edit, close or reopen.
export class DiagnosticVersions<T extends DiagnosticDocument> {
    private serial = 0;
    private readonly latest = new Map<string, number>();
    begin(uri: string, document: T | undefined, reportedVersion?: number): DiagnosticTicket<T> | undefined {
        if (document?.isClosed || (document && reportedVersion !== undefined && document.version !== reportedVersion)) return;
        const serial = ++this.serial;
        this.latest.delete(uri);
        this.latest.set(uri, serial);
        return {uri, serial, document, version: document?.version};
    }
    current(ticket: DiagnosticTicket<T>, document: T | undefined): boolean {
        return this.latest.get(ticket.uri) === ticket.serial && document === ticket.document &&
            !document?.isClosed && document?.version === ticket.version;
    }
    finish(ticket: DiagnosticTicket<T>): void {
        if (this.latest.get(ticket.uri) === ticket.serial) this.latest.delete(ticket.uri);
    }
    clear(): void {this.latest.clear();}
}
