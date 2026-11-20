import test from 'node:test';
import assert from 'node:assert/strict';
import {readFileSync} from 'node:fs';
import * as path from 'node:path';

test('integer operators receive full glyph lexical coloring', () => {
    const grammar = JSON.parse(readFileSync(path.resolve(__dirname, '../../syntaxes/void.tmLanguage.json'), 'utf8'));
    const patterns = grammar.patterns.filter((p: {name: string}) => p.name === 'keyword.operator.void').map((p: {match: string}) => new RegExp(p.match, 'g'));
    for (const op of ['&', '|', '^', '~', '<<', '>>', '&=', '|=', '^=', '<<=', '>>=']) {
        assert(patterns.some((r: RegExp) => { r.lastIndex = 0; return r.exec(op)?.[0] === op; }), op);
    }
});
