import {test} from 'node:test';
import assert from 'node:assert/strict';
import * as path from 'node:path';
import {promises as fs} from 'node:fs';
import * as os from 'node:os';
import {inside, resolveProject, resolveSetting, discoverServer} from '../config';

test('literal workspace paths, spaces, no shell expansion', () => {
    assert.equal(resolveSetting('build/server space', '/project', path.posix), '/project/build/server space');
    assert.equal(resolveSetting('/tool/voidls', undefined, path.posix), '/tool/voidls');
    assert.equal(resolveSetting('$HOME/~/voidls', '/p', path.posix), '/p/$HOME/~/voidls');
    assert.throws(() => resolveSetting('voidls'));
    assert.throws(() => resolveSetting(''));
    assert.throws(() => resolveSetting('a\0b', '/p'));
});
test('Windows paths are literal, drive/root boundaries preserved', () => {
    assert.equal(resolveSetting('build\\server space.exe', 'C:\\project', path.win32), 'C:\\project\\build\\server space.exe');
    assert.throws(() => resolveSetting('C:server.exe', 'D:\\workspace', path.win32));
    assert.throws(() => resolveSetting('\\server.exe', 'C:\\workspace', path.win32));
    assert.equal(resolveSetting('D:\\tools\\voidls.exe', undefined, path.win32), 'D:\\tools\\voidls.exe');
    assert(!inside('C:\\project', 'D:\\project\\a.void', path.win32));
    assert(!inside('C:\\project', 'C:\\projects\\a.void', path.win32));
    assert(inside('C:\\project', 'C:\\project\\sub\\a.void', path.win32));
    assert(!inside('/project', '/project-other/a.void', path.posix));
});
test('validate executable and deterministic canonical project selection', async () => {
    const directory = await fs.mkdtemp(path.join(os.tmpdir(), 'VOID config space '));
    try {
        const server = path.join(directory, process.platform === 'win32' ? 'voidls.exe' : 'voidls');
        const native = process.env.VOIDLS_TEST_SERVER ?? path.resolve(__dirname, '../../../../build/debug/', process.platform === 'win32' ? 'voidls.exe' : 'voidls');
        await fs.copyFile(native, server); await fs.chmod(server, 0o755);
        await fs.writeFile(path.join(directory, 'main.void'), '');
        assert.equal(await discoverServer('', undefined, server, ''), server);
        assert.equal(await discoverServer('', undefined, undefined, directory), server);
        assert.equal(await discoverServer(server, undefined, '/ignored', ''), server);
        await assert.rejects(discoverServer('', undefined, undefined, '.'), /Cannot find voidls/);
        const settings = {server, entry: 'main.void', trace: 'off' as const};
        const project = await resolveProject(settings, [directory]);
        assert.equal(path.normalize(project.root), path.normalize(await fs.realpath(directory)));
        assert.equal(path.normalize(project.entry), path.normalize(path.join(project.root, 'main.void')));
        assert(!project.singleFile);
        await fs.rm(path.join(directory, 'main.void'));
        const fallback = await resolveProject({...settings, entry: ''}, [directory], path.join(directory, 'single.void'));
        assert(fallback.singleFile);
        assert.equal(path.normalize(fallback.root), path.normalize(await fs.realpath(directory)));
        assert.equal(path.normalize(fallback.entry), path.normalize(path.join(await fs.realpath(directory), 'single.void')));
        await assert.rejects(resolveProject(settings, [directory, directory]), /one project/);
        await assert.rejects(resolveProject({...settings, entry: ''}, [directory]), /main.void/);
        await assert.rejects(resolveProject({...settings, server: directory}, [directory]), /regular executable/);
        await assert.rejects(resolveProject({...settings, server: path.join(directory, 'missing')}, [directory]), /Cannot find/);
        if (process.platform !== 'win32') {
            await fs.chmod(server, 0o644);
            await assert.rejects(resolveProject(settings, [directory]), /not executable/);
        } else {
            await fs.copyFile(server, server + '.cmd');
            await assert.rejects(resolveProject({...settings, server: server + '.cmd'}, [directory]), /native voidls.exe/);
        }
    } finally { await fs.rm(directory, {recursive: true, force: true}); }
});

test('presentation covers approved keyword colors and space indentation', async () => {
    const root = path.resolve(__dirname, '../..');
    const grammar = JSON.parse(await fs.readFile(path.join(root, 'syntaxes/void.tmLanguage.json'), 'utf8'));
    const stringRule = grammar.patterns.find((p:{name?:string}) => p.name === 'string.quoted.double.void');
    assert(stringRule && stringRule.begin === '"' && stringRule.end === '"|$');
    const escape = new RegExp(stringRule.patterns.find((p:{name:string}) => p.name === 'constant.character.escape.void').match);
    for (const text of ['\\n', '\\r', '\\t', '\\0', '\\"', '\\\\', '\\u{1f3ae}']) assert(escape.test(text), `Missing string escape: ${text}`);
    for (const text of ['\\q', '\\xFF', '\\u{}']) assert(!escape.test(text), `Unsupported string escape: ${text}`);
    const braces = new RegExp(stringRule.patterns.find((p:{name:string}) => p.name === 'constant.character.escape.brace.void').match);
    assert(braces.test('{{') && braces.test('}}') && !braces.test('{'));
    const interpolation = stringRule.patterns.find((p:{name:string}) => p.name === 'meta.interpolation.void');
    assert(interpolation && new RegExp(interpolation.begin).test('{text}'));
    assert(!new RegExp('^(?:' + interpolation.begin + ')').test('{{'));

    const types = new RegExp(grammar.patterns.find((p:{name?:string}) => p.name === 'storage.type.void').match);
    assert(types.test('string') && !types.test('strings'));
    const keywords = grammar.patterns.filter((p:{name?:string}) => p.name === 'keyword.control.void')
        .map((p:{match:string}) => new RegExp(p.match));
    assert(keywords.length > 0);
    const highlighted = (text:string) => keywords.some((rule:RegExp) => rule.test(text));
    for (const word of ['func','property','enum','is','match', 'has','defer','with','event','async','await','spawn','return','pass','const','for','break','continue','as','and','or','not','in','reverse','struct','value struct'])
        assert(highlighted(word), `Missing keyword highlighting: ${word}`);
    for (const word of ['spawning','awaited','while','structure','function','returning','passing','bypass','value'])
        assert(!highlighted(word), `Unexpected keyword highlighting: ${word}`);
    assert(highlighted('    get:')); assert(highlighted('    set(value):'));
    assert(!highlighted('item.get')); assert(!highlighted('item.set'));
    assert(keywords.some((rule:RegExp) => rule.test('return') && rule.test('pass')), 'pass and return must share the control keyword rule');
    assert(keywords.some((rule:RegExp) => rule.test('spawn') && rule.test('return')), 'spawn must share the control keyword rule');
    assert(keywords.some((rule:RegExp) => rule.test('async') && rule.test('return')), 'async and return must share the control keyword rule');
    assert(keywords.some((rule:RegExp) => ['and','or','not','as','if','return'].every(word => rule.test(word))), 'logical words must share the ordinary control keyword category');
    const config = JSON.parse(await fs.readFile(path.join(root, 'language-configuration.json'), 'utf8'));
    assert.equal(config.comments.lineComment, '#');
    const indent = new RegExp(config.indentationRules.increaseIndentPattern);
    assert(indent.test('    else if value < 0: # branch'));
    assert(indent.test('func (p point) move(dx: i32, dy: i32):'));
    assert(indent.test('for:')); assert(!indent.test('# for:'));
    assert(indent.test('value struct point:')); assert(indent.test('struct point:'));
    assert(keywords.some((rule:RegExp) => rule.exec('value struct point:')?.[0] === 'value struct'));
    assert(keywords.some((rule:RegExp) => rule.exec('struct point:')?.[0] === 'struct'));
    assert(!highlighted('value: i32'));
    for (const text of ['    using movement', '    using motion: movement']) assert(highlighted(text));
    for (const text of ['using: movement', 'p.using', 'haste', 'extends', 'interface', 'virtual', 'override']) assert(!highlighted(text));
    for (const [text, token] of [['struct derived extends base:', 'extends'], ['virtual func (b base) read():', 'virtual'], ['override func (d derived) read():', 'override'], ['super.read()', 'super'], ['super .read()', 'super']])
        assert(keywords.some((rule:RegExp) => rule.exec(text)?.[0] === token), `Missing contextual keyword: ${token}`);
    assert(indent.test('struct derived extends module.base:'));
    assert(indent.test('interface readable:'));
    assert(keywords.some((rule:RegExp) => rule.exec('interface readable:')?.[0] === 'interface'));
    assert(!highlighted('p.interface')); assert(!highlighted('interface: i32'));
    assert(indent.test('public virtual func (b base) read():'));
    assert(indent.test('override func (d derived) read():'));
    const literals = grammar.patterns.filter((p:{name?:string}) => p.name === 'constant.language.void')
        .map((p:{match:string}) => new RegExp(p.match));
    for (const word of ['true','false','null']) assert(literals.some((r:RegExp) => r.test(word)));
    assert(!literals.some((r:RegExp) => r.test('nullify')));
    assert(!indent.test('structure node:'));
});
