import {test} from 'node:test';
import assert from 'node:assert/strict';
import * as path from 'node:path';
import {promises as fs} from 'node:fs';
import * as os from 'node:os';
import {inside, resolveProject, resolveSetting} from '../config';

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
        await fs.writeFile(server, 'not executed by unit tests'); await fs.chmod(server, 0o755);
        const settings = {server, entry: 'main.void', trace: 'off' as const};
        const project = await resolveProject(settings, [directory]);
        assert.equal(project.root, await fs.realpath(directory));
        assert.equal(project.entry, path.join(project.root, 'main.void'));
        assert(!project.singleFile);
        const fallback = await resolveProject({...settings, entry: ''}, [directory], path.join(directory, 'single.void'));
        assert(fallback.singleFile && fallback.entry.endsWith('single.void'));
        await assert.rejects(resolveProject(settings, [directory, directory]), /one project/);
        await assert.rejects(resolveProject({...settings, entry: ''}, [directory]), /entry/);
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

test('presentation only covers implemented tokens and space indentation', async () => {
    const root = path.resolve(__dirname, '../..');
    const grammar = JSON.parse(await fs.readFile(path.join(root, 'syntaxes/void.tmLanguage.json'), 'utf8'));
    const keyword = new RegExp(grammar.patterns.find((p:{name?:string}) => p.name === 'keyword.control.void').match);
    for (const word of ['func','const','for','break','continue','as']) assert(keyword.test(word));
    for (const word of ['while','struct','async','match','defer']) assert(!keyword.test(word));
    const config = JSON.parse(await fs.readFile(path.join(root, 'language-configuration.json'), 'utf8'));
    assert.equal(config.comments.lineComment, '#');
    const indent = new RegExp(config.indentationRules.increaseIndentPattern);
    assert(indent.test('    else if value < 0: # branch'));
    assert(indent.test('for:')); assert(!indent.test('# for:'));
    assert(indent.test('value struct point:')); assert(!indent.test('struct point:'));
    const aggregate = new RegExp(grammar.patterns.find((p:{match?:string}) => p.match?.includes('value')).match);
    assert(aggregate.test('value struct point:')); assert(!aggregate.test('value: i32'));
});
