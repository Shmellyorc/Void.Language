"""Run the shipped adapters in real isolated editors; never writes user config."""
import argparse, json, os, pathlib, subprocess, tempfile, time, signal
p=argparse.ArgumentParser();p.add_argument('--server',required=True);p.add_argument('--nvim');p.add_argument('--vim');p.add_argument('--vim-client');p.add_argument('--output',required=True);a=p.parse_args()
repo=pathlib.Path(__file__).resolve().parents[2];results={}
with tempfile.TemporaryDirectory(prefix='VOID editor installation é😀 ') as d:
    root=pathlib.Path(d);env=dict(os.environ)
    env.update(VOID_STAGE7_FIXTURE=str(root),VOID_REPO=str(repo),VOIDLS_PATH=str(pathlib.Path(a.server).resolve()),XDG_STATE_HOME=str(root/'state'),XDG_CACHE_HOME=str(root/'cache'),XDG_DATA_HOME=str(root/'data'))
    for name in ['nvim','vim']:
        editor=getattr(a,name)
        if not editor:continue
        version=subprocess.check_output([editor,'--version'],text=True)
        if name=='vim' and version.startswith('NVIM'):
            raise SystemExit('--vim resolves to Neovim; select an actual Vim 9 executable (for example vim.basic)')
        (root/'main.void').write_text('use "/worker.void" as worker\nfunc entry(x: i32) -> i32:\n    return worker.calculate(x)\n')
        (root/'worker.void').write_text('func calculate(x: i32) -> i32:\n    return x + 1\n')
        output=root/(name+'.json');env['VOID_STAGE7_OUTPUT']=str(output)
        if name=='nvim':cmd=[editor,'--headless','-u','NONE','-i','NONE','-l',str(repo/'tests/editors/neovim.lua')]
        else:
            if not a.vim_client:raise SystemExit('--vim-client required')
            env['VOID_VIM_CLIENT']=str(pathlib.Path(a.vim_client).resolve());cmd=[editor,'-V1'+str(root/'vim.log'),'-Nu','NONE','-i','NONE','-n','-es','-S',str(repo/'tests/editors/vim.vim')]
        completed=subprocess.run(cmd,env=env,stdout=subprocess.PIPE,stderr=subprocess.STDOUT,text=True,timeout=60)
        if completed.returncode or not output.exists():
            failure=pathlib.Path(str(output)+'.error');raise RuntimeError(name+': '+completed.stdout+'\n'+(failure.read_text() if failure.exists() else (root/'vim.log').read_text() if (root/'vim.log').exists() else 'no result'))
        if pathlib.Path('/proc').is_dir():
            def orphans():
                found=[]
                for process in pathlib.Path('/proc').iterdir():
                    if not process.name.isdigit():continue
                    try:args=process.joinpath('cmdline').read_bytes().split(b'\0')
                    except (OSError, PermissionError):continue
                    if str(root/'main.void').encode() in args and env['VOIDLS_PATH'].encode() in args:found.append(int(process.name))
                return found
            deadline=time.monotonic()+3
            while orphans() and time.monotonic()<deadline:time.sleep(.02)
            leaked=orphans()
            for pid in leaked:
                try:os.kill(pid,signal.SIGTERM)
                except ProcessLookupError:pass
            assert not leaked,(name,'orphan native servers',leaked)
        results[name]=json.loads(output.read_text())
        results[name]['executable']=str(pathlib.Path(editor).resolve())
        results[name]['passed'].append('no orphan server')
pathlib.Path(a.output).write_text(json.dumps(results,indent=2)+'\n')
print(', '.join(results)+' shipped adapters passed')
