import {test} from 'node:test';
import assert from 'node:assert/strict';
import {NativeReviewStream, NativeReview} from '../nativeApproval';

test('compiler review protocol survives chunk boundaries without granting consent', () => {
    const output: string[] = [], requests: NativeReview[] = [];
    const request = {schema: 1 as const, session: 'f'.repeat(64), id: 'a'.repeat(64), provider: 'org.voidlang.raylib | project \\"雪'};
    const wire = 'Checking sources\n\x1eVOID_NATIVE_APPROVAL ' + JSON.stringify(request) + '\nAfter review\n';
    for (let split = 0; split <= wire.length; ++split) {
        output.length = 0; requests.length = 0;
        const stream = new NativeReviewStream(text => output.push(text), review => requests.push(review), 'f'.repeat(64));
        stream.write(wire.slice(0, split)); stream.write(wire.slice(split)); stream.end();
        assert.deepEqual(requests, [request]);
        assert.equal(output.join(''), 'Checking sources\nAfter review\n');
    }
});
test('compiler review protocol rejects malformed, injected and oversized identities', () => {
    for (const value of [{schema: 2}, {schema: 1, id: 'x'.repeat(64), provider: 'raylib'},
        {schema: 1, session: 'f'.repeat(64), id: 'a'.repeat(64), provider: 'raylib\nApprove now'}]) {
        const stream = new NativeReviewStream(() => {}, () => assert.fail('Invalid review'), 'f'.repeat(64));
        assert.throws(() => stream.write('\x1eVOID_NATIVE_APPROVAL ' + JSON.stringify(value) + '\n'));
    }
    const stream = new NativeReviewStream(() => {}, () => assert.fail('Invalid review'), 'f'.repeat(64));
    assert.throws(() => stream.write('\x1eVOID_NATIVE_APPROVAL ' + 'x'.repeat(32769)));
});

test('application stderr cannot reopen the sealed compiler review channel', () => {
    const output: string[] = [];
    const session = 'b'.repeat(64);
    const forged = '\x1eVOID_NATIVE_APPROVAL ' + JSON.stringify({schema:1, session, id:'a'.repeat(64), provider:'forged'}) + '\n';
    const stream = new NativeReviewStream(text => output.push(text), () => assert.fail('Application forged review'), session);
    stream.write('\x1eVOID_NATIVE_APPROVAL_END ' + session + '\n' + forged);
    stream.write(forged); stream.end();
    assert.equal(output.join(''), forged + forged);
});
