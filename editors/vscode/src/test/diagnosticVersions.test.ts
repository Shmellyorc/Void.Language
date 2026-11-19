import assert from 'node:assert/strict';
import {test} from 'node:test';
import {DiagnosticVersions} from '../diagnosticVersions';

test('diagnostics require the reported version, same document and latest notification', () => {
    const guard = new DiagnosticVersions<{version: number; isClosed: boolean}>();
    const document = {version: 4, isClosed: false};
    assert.equal(guard.begin('file:///app.void', document, 3), undefined);
    const first = guard.begin('file:///app.void', document, 4)!;
    const newer = guard.begin('file:///app.void', document, 4)!;
    assert.equal(guard.current(first, document), false);
    assert.equal(guard.current(newer, document), true);
    ++document.version;
    assert.equal(guard.current(newer, document), false);
    const edit = guard.begin('file:///app.void', document, 5)!;
    document.isClosed = true;
    assert.equal(guard.current(edit, document), false);
    assert.equal(guard.current(edit, {version: 5, isClosed: false}), false);
    const closed = guard.begin('file:///other.void', undefined)!;
    assert.equal(guard.current(closed, undefined), true);
    assert.equal(guard.current(closed, {version: 1, isClosed: false}), false);
    guard.finish(first); // A late old conversion cannot retire the newer lease.
    guard.finish(closed);
    assert.equal(guard.current(closed, undefined), false);
    guard.clear();
    assert.equal(guard.current(closed, undefined), false);
});

test('unversioned older servers retain conversion-time edit protection', () => {
    const guard = new DiagnosticVersions<{version: number; isClosed: boolean}>();
    const document = {version: 2, isClosed: false};
    const ticket = guard.begin('legacy', document)!;
    assert(guard.current(ticket, document));
    ++document.version;
    assert(!guard.current(ticket, document));
});
