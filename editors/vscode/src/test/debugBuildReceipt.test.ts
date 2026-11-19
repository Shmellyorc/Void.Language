import {test} from 'node:test';
import assert from 'node:assert/strict';
import {DebugBuildReceipts} from '../debugBuildReceipt';
import {plainStatusCapability} from '../config';
import {promises as fs} from 'node:fs';
import {tmpdir} from 'node:os';
import {join} from 'node:path';

test('actual-child receipts reject host synthetic success and stale/concurrent selections', () => {
    const receipts=new DebugBuildReceipts();
    const selected={root:'/project',program:'/project/build/app',entry:'/project/src/app.void'};
    const id=receipts.begin(selected);
    assert.throws(()=>receipts.require(id,selected)); // pending, even if an old executable exists
    const next=receipts.begin(selected);receipts.start(next,selected);
    assert.throws(()=>receipts.begin(selected)); // actual child still running
    receipts.complete(id,true);assert.throws(()=>receipts.require(next,selected));
    receipts.complete(next,false); // reap the rejected launch's actual child
    const success=receipts.begin(selected);receipts.start(success,selected);receipts.complete(success,true);
    receipts.require(success,selected);assert.throws(()=>receipts.require(success,selected));
    const changed=receipts.begin(selected);assert.throws(()=>receipts.start(changed,{...selected,root:'/other'}));
    receipts.complete(changed,true);assert.throws(()=>receipts.require(changed,selected)); // never spawned
    const entry=receipts.begin(selected);receipts.start(entry,selected);receipts.complete(entry,true);
    assert.throws(()=>receipts.require(entry,{...selected,entry:'/project/other/app.void'}));
    const cancelled=receipts.begin(selected);receipts.start(cancelled,selected);receipts.cancel(cancelled);
    assert.throws(()=>receipts.begin(selected)); // cancellation cannot race a still-running child
    assert.equal(receipts.complete(cancelled,true),false);assert.throws(()=>receipts.require(cancelled,selected));
});

test('abandoned launches are superseded without transferring their task or success', () => {
    const receipts=new DebugBuildReceipts();
    const selected={root:'/project',program:'/project/build/app',entry:'/project/src/app.void'};
    const abandoned=receipts.begin(selected);
    const retry=receipts.begin(selected); // host returned false without cancelling its token
    assert.throws(()=>receipts.start(abandoned,selected));
    assert.equal(receipts.complete(abandoned,true),false);
    receipts.start(retry,selected);
    receipts.cancel(abandoned); // late callback cannot invalidate the retry
    assert.equal(receipts.complete(retry,true),true);
    const concurrent=receipts.begin(selected); // supersedes an unconsumed success
    assert.throws(()=>receipts.require(retry,selected));
    assert.throws(()=>receipts.require(concurrent,selected));
    const fresh=receipts.begin(selected);receipts.start(fresh,selected);receipts.complete(fresh,true);
    receipts.require(fresh,selected);
});

test('capability negotiation adds no status flag for old/invalid compilers', async () => {
    if(process.platform==='win32')return; // native Windows executable fixtures are a separate lane
    const root=await fs.mkdtemp(join(tmpdir(),'void-capabilities-'));
    const compiler=join(root,'voidc');
    try {
        for(const [body,expected] of [
            ['exit 2',false],
            ['printf \'%s\\n\' \'{"schema":1,"cliPresentation":1,"statusPlain":true}\'',true],
            ['printf \'%s\\n\' \'{"schema":2,"cliPresentation":1,"statusPlain":true}\'',false],
            ['echo not-json',false],
            ['echo failure >&2; echo \'{"schema":1,"cliPresentation":1,"statusPlain":true}\'',false],
        ] as const) {
            await fs.writeFile(compiler,'#!/bin/sh\n[ "$1" = "--capabilities" ] || exit 99\n'+body+'\n',{mode:0o700});
            assert.equal(!!await plainStatusCapability(compiler,root),expected);
        }
    } finally {await fs.rm(root,{recursive:true,force:true});}
});
