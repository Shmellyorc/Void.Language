import test from 'node:test';
import assert from 'node:assert/strict';
import * as path from 'node:path';
import {configureCrashDiagnostics, crashEnvironment} from '../crashDiagnostics';

test('editor crash policy is explicit, reversible and applies to all native launches', () => {
    const inherited = process.env.VOID_CRASH_DIAGNOSTICS;
    const worker = process.env.VOID_CRASH_WORKER;
    try {
        process.env.VOID_CRASH_DIAGNOSTICS = '1';
        process.env.VOID_CRASH_WORKER = 'untrusted inherited marker';
        assert.equal(crashEnvironment().VOID_CRASH_DIAGNOSTICS, '0');
        assert.equal(crashEnvironment().VOID_CRASH_WORKER, undefined);
        const directory = path.resolve('private reports');
        configureCrashDiagnostics(() => ({enabled: true, directory}));
        assert.equal(crashEnvironment().VOID_CRASH_DIRECTORY, directory);
        assert.equal(crashEnvironment().VOID_CRASH_DIAGNOSTICS, '1');
        configureCrashDiagnostics(() => ({enabled: true, directory: 'relative'}));
        assert.throws(crashEnvironment, /absolute/);
        configureCrashDiagnostics(() => ({enabled: false, directory: 'relative'}));
        assert.equal(crashEnvironment().VOID_CRASH_DIAGNOSTICS, '0');
    } finally {
        configureCrashDiagnostics(() => ({enabled: false, directory: ''}));
        if (inherited === undefined) delete process.env.VOID_CRASH_DIAGNOSTICS; else process.env.VOID_CRASH_DIAGNOSTICS = inherited;
        if (worker === undefined) delete process.env.VOID_CRASH_WORKER; else process.env.VOID_CRASH_WORKER = worker;
    }
});
