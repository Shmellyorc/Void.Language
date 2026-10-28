"""Canonical-only intrinsics, type/arity boundaries, worker effects and existing cwd surface."""
from pathlib import Path
import sys
import tempfile
import support as h
compiler=Path(sys.argv[1]).resolve();small='--small-stack' in sys.argv
with tempfile.TemporaryDirectory(prefix='VOID OS frontend ') as directory:
    work=Path(directory);source=work/'main.void'
    def check(text,valid=True,diagnostic=None):
        source.write_text(text)
        r=h.run([compiler,'check',source],work,status=0 if valid else 1,small=small)
        if diagnostic:assert diagnostic.encode() in r.stderr,r.stderr
    calls=['data_dir("com.game")','config_dir("com.game")','cache_dir("com.game")','state_dir("com.game")','home_dir()','temp_dir()','executable_path()','resource_dir()','env("HOME")','platform()','architecture()','process_id()','is_bundle()']
    for call in calls:
        check('use os as system\nfunc main():\n    var answer = system.'+call+'\n')
    for call in ('data_dir()','data_dir(1)','data_dir("a", "b")','env()','env(true)','home_dir(0)','process_id(0)','platform(0)','is_bundle(0)','_os_env("HOME")','_reply()','cwd()','runtime_dir()','session_dir()'):
        check('use os\nfunc main():\n    var answer = os.'+call+'\n',False)
    for symbol in ('_os_env("HOME")','_os_process_id()'):
        check('func main():\n    var answer = '+symbol+'\n',False)
    check('use os\nconst p = os.process_id()\nfunc main():\n    pass\n',False)
    check('use os\nuse io\nfunc mismatch() -> result<string,io.error>:\n    return os.home_dir()\nfunc main():\n    pass\n',False)
    # All owner queries are forbidden through a worker helper, including scalar identity.
    for call in calls:
        for transitive in (False,True):
            helper='func helper(n: i32) -> i32:\n    var answer = os.'+call+'\n    return n\n'
            worker='func work(n: i32) -> i32:\n    return helper(n)\n' if transitive else helper.replace('helper','work')
            check('use os\n'+(helper if transitive else '')+worker+'async func main():\n    var task = spawn work(1)\n    var answer = await task\n',False)
    check('use os\nasync func main():\n    var p = os.process_id()\n    var home = os.home_dir()\n')
    check('use os\nvar home = os.home_dir()\nvar p = os.process_id()\nfunc main():\n    pass\n')
    # User modules with the same basename cannot acquire a native bridge.
    (work/'os.void').write_text('func private_query() -> i64:\n    return _os_process_id()\n')
    check('use "./os.void" as local\nfunc main():\n    print(local.private_query())\n',False)
    (work/'os.void').write_text('func process_id() -> i32:\n    return 42\n')
    check('use "./os.void" as local\nfunc main():\n    print(local.process_id())\n')
    # Existing cwd spellings remain io.error, with no new native operation.
    check('use path\nuse io\nfunc cwd() -> result<string,io.error>:\n    return path.absolute("", "")\nfunc main():\n    var first = cwd()\n    var second = path.absolute("")\n')
print(f'OS frontend: {h.checks} processes passed; hard_stack_1MiB={small}')
