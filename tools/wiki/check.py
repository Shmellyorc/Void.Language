"""Check publication routes/content and execute every displayed VOID example privately."""
import argparse
import hashlib
import json
import os
from pathlib import Path
import re
import shutil
import subprocess
import sys
import time
from urllib.parse import unquote, urlsplit

ROOT = Path(__file__).resolve().parents[2]
SOURCE = ROOT / 'docs/wiki'
sys.path.insert(0, str(ROOT / 'tools'))
from test_platform import native_manifest
sys.path.insert(0, str(Path(__file__).resolve().parent))
from prepare import inventory, WIKI


def anchors(text):
    counts, result = {}, set(re.findall(r'<a\s+(?:id|name)="([^"]+)"', text))
    for heading in re.findall(r'^#{1,6}\s+(.+?)\s*#*$', text, re.M):
        slug = re.sub(r'[^\w\- ]', '', heading.replace('`', '').lower()).replace(' ', '-')
        number = counts.get(slug, 0)
        counts[slug] = number + 1
        result.add(slug + ('-' + str(number) if number else ''))
    return result


def structure():
    actual = inventory()
    assert json.loads((Path(__file__).resolve().parent / 'publication.json').read_text()) == actual, 'Stale publication inventory'
    files = [row['file'] for row in actual['files']]
    pages = {Path(name).stem for name in files if not name.startswith('_')}
    assert len(pages) == 24 and len(files) == 26
    sidebar = (SOURCE / '_Sidebar.md').read_text()
    assert {unquote(urlsplit(link).path).rsplit('/', 1)[-1] for link in re.findall(r'\]\((https://[^)]+)\)', sidebar)} == pages
    example_rows = json.loads((Path(__file__).resolve().parent / 'examples.json').read_text())['examples']
    assert len({row['id'] for row in example_rows}) == len(example_rows)
    expected_blocks = {(row['page'], row['block']) for row in example_rows}
    found_blocks, links, relation_count = set(), set(), {}
    for name in files:
        text = (SOURCE / name).read_text()
        assert not re.search(r'\b(?:clanker|clankers|golden wrench|keyboard clanker|boss|milestones?|G\d+)\b', text, re.I), name
        assert not re.search(r'(?i)draft for|pending approval|review evidence|independent acceptance', text), name
        assert not re.search(r'\]\((?:\.\.?/|[^/:)]+\.md(?:#|\)))', text), (name, 'repository-relative link')
        assert all(f in {'void', 'sh', 'powershell', 'text', 'json', 'toml'} for f in re.findall(r'^```([^\n]+)$', text, re.M)), name
        assert len(re.findall(r'^```', text, re.M)) % 2 == 0, name
        headings = re.findall(r'^(#+) ', text, re.M)
        if not name.startswith('_'):
            assert headings and headings[0] == '#' and headings.count('#') == 1, name
            assert 'Next:' in text or name == 'Resources-and-status.md', name
        previous = 0
        for h in headings:
            assert len(h) <= previous + 1, (name, 'heading jump')
            previous = len(h)
        for number, block in enumerate(re.findall(r'```void\n(.*?)```', text, re.S)):
            assert '...' not in block and 'func ' in block, (name, number)
            found_blocks.add((name, number))
        for language, block in re.findall(r'```(json|toml)\n(.*?)```', text, re.S):
            if language == 'json':
                json.loads(block)
            else:
                import tomllib
                tomllib.loads(block)
        body_links = []
        for link in re.findall(r'\]\(([^\s)]+)\)', text):
            links.add(link)
            parsed = urlsplit(link)
            if link.startswith(WIKI):
                page = unquote(parsed.path).rsplit('/', 1)[-1]
                assert page in pages, (name, link)
                if parsed.fragment:
                    assert unquote(parsed.fragment) in anchors((SOURCE / (page + '.md')).read_text()), (name, link)
                if page != Path(name).stem:
                    body_links.append(link)
            elif link.startswith('https://github.com/Shmellyorc/Void.Language/blob/main/'):
                path = unquote(parsed.path.split('/blob/main/', 1)[1])
                target = ROOT / path
                assert target.is_file(), (name, link)
                if parsed.fragment:
                    assert unquote(parsed.fragment) in anchors(target.read_text()), (name, link)
            else:
                assert parsed.scheme == 'https' and parsed.netloc, (name, link)
        if not name.startswith('_'):
            # A topic must be reachable without returning to the sidebar; keep
            # contextual links in the prose as well as end-of-page next steps.
            before_next = text.split('\nNext:', 1)[0]
            contextual = [l for l in body_links if l in before_next]
            assert len(set(contextual)) >= 2, (name, 'missing contextual links')
            relation_count[name] = len(set(contextual))
    assert found_blocks == expected_blocks, (found_blocks - expected_blocks, expected_blocks - found_blocks)
    assert len(found_blocks) == 34
    return dict(publication_files=len(files), topic_pages=len(pages), void_examples=len(found_blocks),
                unique_links=len(links), contextual_links=relation_count, publication=actual)


def limits():
    if os.name == 'posix':
        import resource
        resource.setrlimit(resource.RLIMIT_STACK, (1048576, 1048576))
        resource.setrlimit(resource.RLIMIT_CORE, (0, 0))


def execute(args, summary):
    out = args.output.resolve()
    out.mkdir(mode=0o700, parents=False, exist_ok=False)
    compiler, build = args.compiler.resolve(), args.build.resolve()
    environment = dict(os.environ, TMPDIR=str(out), PYTHONDONTWRITEBYTECODE='1',
        ASAN_OPTIONS='detect_leaks=1:halt_on_error=1:abort_on_error=1:exitcode=87',
        UBSAN_OPTIONS='halt_on_error=1:print_stacktrace=1:exitcode=86')
    environment.pop('LSAN_OPTIONS', None)
    records = []
    result = dict(structure=summary, stack_bytes=1048576, platform=sys.platform,
        compiler=str(compiler), compiler_sha256=hashlib.sha256(compiler.read_bytes()).hexdigest(),
        environment={k: environment[k] for k in ('ASAN_OPTIONS', 'UBSAN_OPTIONS')}, records=records)
    def save():
        (out / 'results.json').write_text(json.dumps(result, indent=2) + '\n')
    def run(command, cwd, expected=None, env=None):
        start = time.monotonic()
        child = subprocess.run(list(map(str, command)), cwd=cwd, env=env or environment,
            preexec_fn=limits if os.name == 'posix' else None,
            capture_output=True, text=True, timeout=180)
        row = dict(command=list(map(str, command)), cwd=str(cwd), exit=child.returncode,
                   stdout=child.stdout, stderr=child.stderr, seconds=time.monotonic() - start)
        records.append(row); save()
        assert child.returncode == 0, row
        if expected is not None:
            assert child.stdout == expected, row
        assert not any(s in child.stderr for s in ('AddressSanitizer', 'LeakSanitizer', 'runtime error:')), row
        return child.stdout
    rows = json.loads((Path(__file__).resolve().parent / 'examples.json').read_text())['examples']
    snippets = {row['id']: re.findall(r'```void\n(.*?)```', (SOURCE / row['page']).read_text(), re.S)[row['block']] for row in rows}
    result['examples'] = {row['id']: dict(source=snippets[row['id']], mode=row['mode']) for row in rows}
    for row in rows:
        work = out / row['id']; work.mkdir()
        entry = work / 'main.void';entry.write_text(snippets[row['id']])
        for name, contents in row.get('fixtures', {}).items():
            (work / name).write_text(contents)
        mode, env = row['mode'], environment
        selector = [entry]
        if mode in ('module', 'import'):
            entry = work / row['file'];(work / 'main.void').unlink()
            for member in rows:
                if member.get('group') == row['group']:
                    (work / member['file']).write_text(snippets[member['id']])
            selector = [entry]
        elif mode == 'feature':
            project = work / 'feature'
            run([compiler, 'new', 'app', project], work)
            manifest = project / 'void.project.toml'
            manifest.write_text(manifest.read_text() + '\n[build.conditions]\ntracing = true\n')
            (project / 'src/app.void').write_text(snippets[row['id']]);selector=['--project',project]
        elif mode == 'raylib':
            assert args.catalog and args.raylib_project, 'Raylib execution needs existing --catalog and --raylib-project'
            project = work / 'consumer';shutil.copytree(args.raylib_project.resolve(), project)
            assert (project / 'libs/raylib/src/raylib.void').read_bytes() == (ROOT / 'verified-packages/raylib/src/raylib.void').read_bytes()
            (project / 'src/app.void').write_text(snippets[row['id']])
            env=dict(environment, VOID_NATIVE_CATALOG=str(args.catalog.resolve()))
            result['catalog']={p.name:hashlib.sha256(p.read_bytes()).hexdigest() for p in args.catalog.resolve().iterdir() if p.is_file()}
            run([compiler,'tidy','--project',project],work,env=env);selector=['--project',project]
        run([compiler,'check',*selector],work,env=env)
        if mode == 'module':
            continue
        if mode == 'async':
            emitter=build/('void_async_tests.exe' if os.name == 'nt' else 'void_async_tests')
            result['async_emitter_sha256']=hashlib.sha256(emitter.read_bytes()).hexdigest()
            link=native_manifest(compiler);flags=['--driver-mode=g++',*link[3:]]
            if link[2]:flags.append('--ld-path='+link[2])
            for opt in ('normal','optimized'):
                obj=work/(opt+'.o');exe=work/(opt+('.exe' if os.name=='nt' else '.app'))
                run([emitter,'--emit',entry,obj,opt],work)
                run([link[1],obj,*flags,'-o',exe],work)
                run([exe,row['input']],work,row['stdout'])
            continue
        expected=row['stdout']
        if mode == 'target':expected=row['outputs'].get(sys.platform if sys.platform!='win32' else 'windows',row['outputs']['other'])
        native=['--allow-native'] if mode == 'raylib' else []
        for flags in (['--no-opt'],[]):
            run([compiler,'run',*selector,*native,'--no-cache',*flags],work,expected,env=env)
        if row['id'] in ('first-program-1','packages-and-raylib-1','conditional-compilation-2'):
            exe=work/('example.exe' if os.name=='nt' else 'example.app')
            run([compiler,'build',*selector,*native,'-o',exe],work,env=env);run([exe],work,expected,env=env)
        if mode == 'feature':
            manifest.write_text(manifest.read_text().replace('tracing = true','tracing = false'))
            run([compiler,'check',*selector],work)
            run([compiler,'run',*selector,'--no-cache'],work,row['disabled_stdout'])
    # Exercise the documented project workflow, without changing user stores.
    workflow=out/'workflow';workflow.mkdir();hello=workflow/'Hello'
    run([compiler,'new','app','--name','Hello'],workflow)
    (hello/'src/app.void').write_text(snippets['first-program-1'])
    run([compiler,'check','.'],hello)
    run([compiler,'run','.'],hello,'Hello, world!\n')
    executable=hello/('hello.exe' if os.name=='nt' else 'hello')
    run([compiler,'build','.','--profile','release','-o',executable],hello)
    run([executable],hello,'Hello, world!\n')
    run([compiler,'new','classlib','--name','geometry'],workflow)
    run([compiler,'check','--project',workflow/'geometry'],workflow)
    run([compiler,'--version'],out)
    for command in ('check','run','build','emit','new','init','add','tidy','deps','package','native'):
        run([compiler,command,'--help'],out)
    result['completed']=True;save()
    print(f'Wiki examples: all {len(rows)} displayed programs checked; {len(records)} process commands passed')


def main():
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--compiler',type=Path)
    parser.add_argument('--build',type=Path)
    parser.add_argument('--catalog',type=Path)
    parser.add_argument('--raylib-project',type=Path)
    parser.add_argument('--output',type=Path)
    args=parser.parse_args();summary=structure()
    print(f'Wiki structure: {summary["topic_pages"]} pages, 2 shared files, {summary["void_examples"]} VOID blocks, {summary["unique_links"]} links verified')
    if args.compiler:
        if not args.build or not args.output:parser.error('--compiler requires --build and a fresh --output directory')
        execute(args,summary)
    elif any((args.build,args.catalog,args.raylib_project,args.output)):
        parser.error('Execution options require --compiler')


if __name__=='__main__':
    main()
