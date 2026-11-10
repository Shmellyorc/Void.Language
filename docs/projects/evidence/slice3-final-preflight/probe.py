from pathlib import Path
import json, os, subprocess, sys, tempfile
server=Path(sys.argv[1]).resolve()
rows={}
manifest="""schema=1
[project]
id='org.test.preflight'
name='Preflight'
version='1.0.0'
kind='application'
[application]
id='org.test.preflight'
"""
source="func main() -> i32:\n    return 3\n"
with tempfile.TemporaryDirectory(prefix="VOID preflight review ") as temporary:
    workspace=Path(temporary)
    root=workspace/'parent'
    child=root/'child'
    child.mkdir(parents=True)
    root_manifest=root/'void.project.toml'
    child_manifest=child/'void.project.toml'
    entry=root/'main.void'
    child_source=child/'launcher.void'
    root_manifest.write_text(manifest)
    child_manifest.write_text(manifest)
    entry.write_text(source)
    child_source.write_text(source)
    def run(label, active='', pinned=root, override='main.void', args=None):
        if args is None: args=[workspace,active,pinned,override]
        argv=[str(server),'--discover-project',*map(str,args)]
        p=subprocess.run(argv,capture_output=True,text=True,encoding='utf-8',timeout=30)
        rows[label]={'argv':argv,'exit':p.returncode,'stdout':p.stdout,'stderr':p.stderr}
        if p.stdout:
            j=json.loads(p.stdout)
            print(label,p.returncode,j['status'],j.get('root'),j.get('entry'),j.get('diagnostics'))
        else: print(label,p.returncode,p.stderr.strip())
    run('pinned-child-source',child_source)
    run('pinned-child-manifest',child_manifest)
    outside=workspace/'outside.void'
    outside.write_text(source)
    run('pinned-outside',outside)
    run('invalid-source-identity',root/'notes.txt')
    run('virtual-source',root/'new'/'draft.void')
    run('selected-manifest',root_manifest)
    run('nearest-child',child_source,'','')
    child_manifest.write_text('[=roject]\n')
    run('pinned-malformed-child-source',child_source)
    run('pinned-malformed-child-manifest',child_manifest)
    run('nearest-malformed-child',child_source,'','')
    child_manifest.unlink()
    (child/'void.Project.toml').write_text(manifest)
    run('pinned-wrong-case-child-source',child_source)
    run('pinned-wrong-case-child-manifest',child/'void.Project.toml')
    run('nearest-wrong-case-child',child_source,'','')
    run('bad-invocation',args=[workspace])
    run('unpinned-invalid-discovery-path',workspace.parent/'outside.void','','')
    if os.name!='nt':
        alias=root/'alias.void'
        alias.symlink_to(outside)
        run('physical-alias',alias)
        unreadable=root/'unreadable.void'
        unreadable.write_text(source)
        unreadable.chmod(0)
        try: run('physically-unreadable',unreadable)
        finally: unreadable.chmod(0o600)
Path(sys.argv[2]).write_text(json.dumps(rows,indent=2)+'\n')
