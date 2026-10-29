# Original CTest failure blocks

## json_preservation

```text
----------------------------------------------------------
CRASH EVIDENCE: /var/folders/n6/hn37q6pj7sj_9dmqqc2113800000gn/T/VOID random synthetic preservation s6t0hsuk/evidence/incident-vmo66sk2
CRASH EVIDENCE: /var/folders/n6/hn37q6pj7sj_9dmqqc2113800000gn/T/VOID random synthetic preservation s6t0hsuk/evidence/incident-mlbc195z
CRASH EVIDENCE: /var/folders/n6/hn37q6pj7sj_9dmqqc2113800000gn/T/VOID random synthetic preservation s6t0hsuk/evidence/incident-n0spc7w6
Traceback (most recent call last):
  File "/Users/runner/work/Void.Language/Void.Language/tests/random/preservation.py", line 32, in <module>
    preserved=next(x for x in record['sources'] if x['original']==str(source))
              ^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^
StopIteration
```


## math_preservation

```text
----------------------------------------------------------
CRASH EVIDENCE: /var/folders/n6/hn37q6pj7sj_9dmqqc2113800000gn/T/VOID math synthetic preservation rtc503_3/synthetic-evidence/incident-qx99zmms
CRASH EVIDENCE: /var/folders/n6/hn37q6pj7sj_9dmqqc2113800000gn/T/VOID math synthetic preservation rtc503_3/synthetic-evidence/incident-6zfv3elc
CRASH EVIDENCE: /var/folders/n6/hn37q6pj7sj_9dmqqc2113800000gn/T/VOID math synthetic preservation rtc503_3/synthetic-evidence/incident-33vss00z
Traceback (most recent call last):
  File "/Users/runner/work/Void.Language/Void.Language/tests/math/preservation.py", line 63, in <module>
    assert record['cwd']==str(root)
           ^^^^^^^^^^^^^^^^^^^^^^^^
AssertionError
```


## random_preservation

```text
----------------------------------------------------------
CRASH EVIDENCE: /var/folders/n6/hn37q6pj7sj_9dmqqc2113800000gn/T/VOID random synthetic preservation o7sj2b3z/evidence/incident-knbgm9he
CRASH EVIDENCE: /var/folders/n6/hn37q6pj7sj_9dmqqc2113800000gn/T/VOID random synthetic preservation o7sj2b3z/evidence/incident-0g_h0mbf
CRASH EVIDENCE: /var/folders/n6/hn37q6pj7sj_9dmqqc2113800000gn/T/VOID random synthetic preservation o7sj2b3z/evidence/incident-rvx85849
Traceback (most recent call last):
  File "/Users/runner/work/Void.Language/Void.Language/tests/random/preservation.py", line 32, in <module>
    preserved=next(x for x in record['sources'] if x['original']==str(source))
              ^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^
StopIteration
```


## lsp_const_recovery_stack

```text
----------------------------------------------------------
Traceback (most recent call last):
  File "/Users/runner/work/Void.Language/Void.Language/tests/stage11/lsp_const_recovery.py", line 12, in <module>
    resource.setrlimit(resource.RLIMIT_STACK, (1024 * 1024, 1024 * 1024))
ValueError: current limit exceeds maximum limit
```


## text_encoding_crash_preservation

```text
----------------------------------------------------------
Traceback (most recent call last):
  File "/Users/runner/work/Void.Language/Void.Language/tests/text_encoding/crash_test.py", line 207, in <module>
    exercise(disposable)
  File "/Users/runner/work/Void.Language/Void.Language/tests/text_encoding/crash_test.py", line 37, in exercise
    first = run([selected_executable, '-c', script, original], cwd=root, capture_output=True,
            ^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^
  File "/Users/runner/work/Void.Language/Void.Language/tests/text_encoding/crash.py", line 252, in run
    with subprocess.Popen(command, *args, **kwargs) as process:
         ^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^
  File "/Users/runner/work/Void.Language/Void.Language/tests/text_encoding/crash.py", line 343, in __init__
    super().__init__(command, *args, **kwargs)
  File "/Library/Frameworks/Python.framework/Versions/3.12/lib/python3.12/subprocess.py", line 1026, in __init__
    self._execute_child(args, executable, preexec_fn, close_fds,
  File "/Library/Frameworks/Python.framework/Versions/3.12/lib/python3.12/subprocess.py", line 1955, in _execute_child
    raise child_exception_type(errno_num, err_msg, err_filename)
FileNotFoundError: [Errno 2] No such file or directory: '../../../../../../Library/Frameworks/Python.framework/Versions/3.12/bin/python3.12'
```


## lsp_text_encoding_stack

```text
----------------------------------------------------------
Traceback (most recent call last):
  File "/Users/runner/work/Void.Language/Void.Language/tests/text_encoding/lsp.py", line 14, in <module>
    resource.setrlimit(resource.RLIMIT_STACK, (1024 * 1024, 1024 * 1024))
ValueError: current limit exceeds maximum limit
```


## log_install

```text
----------------------------------------------------------
CRASH EVIDENCE: /Users/runner/work/Void.Language/Void.Language/docs/log/evidence/crashes/incident-unf751cp
Traceback (most recent call last):
  File "/Users/runner/work/Void.Language/Void.Language/tests/log/install.py", line 34, in <module>
    h.run([sys.executable,Path(__file__).with_name('lsp.py'),server],cwd=work,env=env,sources=[source],context={'mode':'installed-lsp'});checks+=1
    ^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^
  File "/Users/runner/work/Void.Language/Void.Language/tests/log/support.py", line 73, in run
    assert not result.expectation_failed, (receipt, args, result.returncode, result.stdout, result.stderr.decode(errors='replace'))
           ^^^^^^^^^^^^^^^^^^^^^^^^^^^^^
AssertionError: (PosixPath('/Users/runner/work/Void.Language/Void.Language/docs/log/evidence/run-gfbqry_h'), ['/Library/Frameworks/Python.framework/Versions/3.12/bin/python3.12', '/Users/runner/work/Void.Language/Void.Language/tests/log/lsp.py', '/var/folders/n6/hn37q6pj7sj_9dmqqc2113800000gn/T/VOID-log-installed-g1xzq3ml/relocated 雪 # %/bin/voidls'], 1, b'', 'Traceback (most recent call last):\n  File "/Users/runner/work/Void.Language/Void.Language/tests/log/lsp.py", line 11, in <module>\n    h.stack()\n  File "/Users/runner/work/Void.Language/Void.Language/tests/log/support.py", line 20, in stack\n    resource.setrlimit(resource.RLIMIT_STACK, (1048576, 1048576))\nValueError: current limit exceeds maximum limit\n')
```


## lsp_log

```text
----------------------------------------------------------
Traceback (most recent call last):
  File "/Users/runner/work/Void.Language/Void.Language/tests/log/lsp.py", line 11, in <module>
    h.stack()
  File "/Users/runner/work/Void.Language/Void.Language/tests/log/support.py", line 20, in stack
    resource.setrlimit(resource.RLIMIT_STACK, (1048576, 1048576))
ValueError: current limit exceeds maximum limit
```


## collections_stdlib_install

```text
----------------------------------------------------------
CRASH EVIDENCE: /Users/runner/work/Void.Language/Void.Language/docs/collections/evidence/crashes/incident-04yorn9x
Traceback (most recent call last):
  File "/Users/runner/work/Void.Language/Void.Language/tests/collections_stdlib/install.py", line 32, in <module>
    h.run([sys.executable, Path(__file__).with_name('lsp.py'), lsp], cwd=work, env=env, sources=[source], context={'mode': 'installed-lsp'})
  File "/Users/runner/work/Void.Language/Void.Language/tests/collections_stdlib/support.py", line 70, in run
    assert not result.expectation_failed, (receipt, args, result.returncode, result.stdout, result.stderr.decode(errors='replace'))
           ^^^^^^^^^^^^^^^^^^^^^^^^^^^^^
AssertionError: (PosixPath('/Users/runner/work/Void.Language/Void.Language/docs/collections/evidence/run-dba4a8l6'), ['/Library/Frameworks/Python.framework/Versions/3.12/bin/python3.12', '/Users/runner/work/Void.Language/Void.Language/tests/collections_stdlib/lsp.py', '/var/folders/n6/hn37q6pj7sj_9dmqqc2113800000gn/T/VOID-collections-installed-wggcksp8/relocated 雪 # %/bin/voidls'], 1, b'CRASH EVIDENCE: /Users/runner/work/Void.Language/Void.Language/docs/collections/evidence/crashes/incident-tly78wro\n', 'Traceback (most recent call last):\n  File "/Users/runner/work/Void.Language/Void.Language/tests/collections_stdlib/lsp.py", line 73, in <module>\n    check(\'error\' not in response and \'main.void\' in str(response.get(\'result\')), response)\n  File "/Users/runner/work/Void.Language/Void.Language/tests/collections_stdlib/lsp.py", line 18, in check\n    assert ok, detail\n           ^^\nAssertionError: {\'error\': {\'code\': -32602, \'message\': \'<document identity>:1:1: error: Import escapes the project root.\\n    code: VOID3001\\n\\n 1 | \\n   | ^\\n    Hint: External imports require an explicit permission/identity contract and are not enabled in this phase.\\n\'}, \'id\': 28, \'jsonrpc\': \'2.0\'}\n')
```


## io_crash_preservation

```text
----------------------------------------------------------
CRASH EVIDENCE: /var/folders/n6/hn37q6pj7sj_9dmqqc2113800000gn/T/VOID disposable io preservation o2s_090_/incidents/incident-fp188k1n
Traceback (most recent call last):
  File "/Users/runner/work/Void.Language/Void.Language/tests/io/preservation.py", line 128, in <module>
    main()
  File "/Users/runner/work/Void.Language/Void.Language/tests/io/preservation.py", line 95, in main
    assert record['cwd'] == str(working)
           ^^^^^^^^^^^^^^^^^^^^^^^^^^^^^
AssertionError
```


## lsp_io_stack

```text
----------------------------------------------------------
Traceback (most recent call last):
  File "/Users/runner/work/Void.Language/Void.Language/tests/io/lsp.py", line 12, in <module>
    resource.setrlimit(resource.RLIMIT_STACK, (1024 * 1024, 1024 * 1024))
ValueError: current limit exceeds maximum limit
```


## io_kernel

```text
----------------------------------------------------------
invalid native filename escaped UTF-8 validation
```
