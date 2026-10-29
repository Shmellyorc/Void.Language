# Original CTest failure blocks

## math_execution

```text
94/319 Testing: math_execution
94/319 Test: math_execution
Command: "C:/hostedtoolcache/windows/Python/3.12.10/x64/python3.exe" "D:/a/Void.Language/Void.Language/tests/math/execution.py" "D:/a/Void.Language/Void.Language/build/Debug/voidc.exe"
Directory: D:/a/Void.Language/Void.Language/build/Debug
"math_execution" start time: Oct 29 11:20 Coordinated Universal Time
Output:
----------------------------------------------------------
Math check: 1372 vectors, 59675 source bytes
Math JIT O0
Math linked AOT O0
Traceback (most recent call last):
  File "D:\a\Void.Language\Void.Language\tests\math\execution.py", line 243, in <module>
    h.run([compiler,'build',fixture,'-o',executable,*flags],work,small=small,timeout=300)
  File "D:\a\Void.Language\Void.Language\tests\random\harness.py", line 27, in run
    assert b'warning' not in result.stderr.lower(), result.stderr
           ^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^
AssertionError: b"clang: warning: argument unused during compilation: '-pthread' [-Wunused-command-line-argument]\r\n"
```


## math_frontend

```text
95/319 Testing: math_frontend
95/319 Test: math_frontend
Command: "C:/hostedtoolcache/windows/Python/3.12.10/x64/python3.exe" "D:/a/Void.Language/Void.Language/tests/math/frontend.py" "D:/a/Void.Language/Void.Language/build/Debug/voidc.exe"
Directory: D:/a/Void.Language/Void.Language/build/Debug
"math_frontend" start time: Oct 29 11:20 Coordinated Universal Time
Output:
----------------------------------------------------------
Traceback (most recent call last):
  File "D:\a\Void.Language\Void.Language\tests\math\frontend.py", line 32, in <module>
    for fn,arity in floating.items():check(f'var input: {target} = 1\nvar bad = math.{fn}('+', '.join(['input']*arity)+')',False,diagnostic='requires exactly f32 or f64')
                                     ^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^
  File "D:\a\Void.Language\Void.Language\tests\math\frontend.py", line 21, in check
    assert str(source).encode()+b':' in result.stderr,result.stderr
           ^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^
AssertionError: b"C:/Users/runneradmin/AppData/Local/Temp/VOID math frontend fyi2_39m/case2.void:4:20: error: Math function 'is_nan' requires exactly f32 or f64.\r\n    code: VOID1103\r\n\r\n 4 |     var bad = math.is_nan(input)\r\n   |                    ^^^^^^\r\n    Module: /case2.void\r\n\r\n"
```


## math_errors

```text
96/319 Testing: math_errors
96/319 Test: math_errors
Command: "C:/hostedtoolcache/windows/Python/3.12.10/x64/python3.exe" "D:/a/Void.Language/Void.Language/tests/math/errors.py" "D:/a/Void.Language/Void.Language/build/Debug/voidc.exe"
Directory: D:/a/Void.Language/Void.Language/build/Debug
"math_errors" start time: Oct 29 11:20 Coordinated Universal Time
Output:
----------------------------------------------------------
Traceback (most recent call last):
  File "D:\a\Void.Language\Void.Language\tests\math\errors.py", line 19, in <module>
    assert b'runtime error:' in result.stderr and str(source).encode() in result.stderr and b'Called from' in result.stderr,result.stderr
           ^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^
AssertionError: b'D:/a/Void.Language/Void.Language/stdlib/math.void:49:14: runtime error: Signed minimum abs is out of range\r\nIn function $numeric::i8::abs\r\nCalled from /@stdlib/math.void::abs\r\nD:/a/Void.Language/Void.Language/stdlib/math.void:49:14: source call\r\n    return t.abs(value)\r\n             ^\r\nCalled from /checked0.void::main\r\nC:/Users/runneradmin/AppData/Local/Temp/VOID math checked errors 5e_53_pz/checked0.void:6:22: source call\r\n    var value = math.abs<i8>(i8.min_value)\r\n                     ^\r\nCalled from /checked0.void::$app_adapter\r\nC:/Users/runneradmin/AppData/Local/Temp/VOID math checked errors 5e_53_pz/checked0.void:4:6: source call\r\n\r\n'
```


## math_worker

```text
97/319 Testing: math_worker
97/319 Test: math_worker
Command: "C:/hostedtoolcache/windows/Python/3.12.10/x64/python3.exe" "D:/a/Void.Language/Void.Language/tests/math/worker.py" "D:/a/Void.Language/Void.Language/build/Debug/voidc.exe" "D:/a/Void.Language/Void.Language/build/Debug/void_io_runner.exe"
Directory: D:/a/Void.Language/Void.Language/build/Debug
"math_worker" start time: Oct 29 11:20 Coordinated Universal Time
Output:
----------------------------------------------------------
Traceback (most recent call last):
  File "D:\a\Void.Language\Void.Language\tests\math\worker.py", line 17, in <module>
    h.run([runner,'--async',source],work,b'Debug source execution passed\nO2 source execution passed\n',sources=(source,))
  File "D:\a\Void.Language\Void.Language\tests\random\harness.py", line 29, in run
    assert result.stdout == expected, (args, result.stdout[:4000], expected[:4000], result.stderr)
           ^^^^^^^^^^^^^^^^^^^^^^^^^
AssertionError: (['D:\\a\\Void.Language\\Void.Language\\build\\Debug\\void_io_runner.exe', '--async', 'C:\\Users\\RUNNER~1\\AppData\\Local\\Temp\\VOID math worker l7a7jr2p\\worker.void'], b'Debug source execution passed\r\nO2 source execution passed\r\n', b'Debug source execution passed\nO2 source execution passed\n', b'')
```


## math_install

```text
99/319 Testing: math_install
99/319 Test: math_install
Command: "C:/hostedtoolcache/windows/Python/3.12.10/x64/python3.exe" "D:/a/Void.Language/Void.Language/tests/math/install.py" "D:/a/Void.Language/Void.Language/build/Debug/voidc.exe"
Directory: D:/a/Void.Language/Void.Language/build/Debug
"math_install" start time: Oct 29 11:20 Coordinated Universal Time
Output:
----------------------------------------------------------
Traceback (most recent call last):
  File "D:\a\Void.Language\Void.Language\tests\math\install.py", line 29, in <module>
    h.run([installed,'run',fixture,'2',*flags],work,expected,env=env)
  File "D:\a\Void.Language\Void.Language\tests\random\harness.py", line 29, in run
    assert result.stdout == expected, (args, result.stdout[:4000], expected[:4000], result.stderr)
           ^^^^^^^^^^^^^^^^^^^^^^^^^
AssertionError: (['C:\\Users\\RUNNER~1\\AppData\\Local\\Temp\\VOID math relocated fwlqespg\\relocated install\\bin\\voidc.exe', 'run', 'C:\\Users\\RUNNER~1\\AppData\\Local\\Temp\\VOID math relocated fwlqespg\\installed_math.void', '2', '--no-opt'], b'2\n2\ntrue\n-8\ntrue\n0\r\n', b'2\n2\ntrue\n-8\ntrue\n0\n', b'')
```


## math_preservation

```text
100/319 Testing: math_preservation
100/319 Test: math_preservation
Command: "C:/hostedtoolcache/windows/Python/3.12.10/x64/python3.exe" "D:/a/Void.Language/Void.Language/tests/math/preservation.py" "D:/a/Void.Language/Void.Language/build/Debug/voidc.exe"
Directory: D:/a/Void.Language/Void.Language/build/Debug
"math_preservation" start time: Oct 29 11:20 Coordinated Universal Time
Output:
----------------------------------------------------------
CRASH EVIDENCE: C:\Users\RUNNER~1\AppData\Local\Temp\VOID math synthetic preservation bqm_284e\synthetic-evidence\incident-rgm91nr8
CRASH EVIDENCE: C:\Users\RUNNER~1\AppData\Local\Temp\VOID math synthetic preservation bqm_284e\synthetic-evidence\incident-4bbkvf9m
CRASH EVIDENCE: C:\Users\RUNNER~1\AppData\Local\Temp\VOID math synthetic preservation bqm_284e\synthetic-evidence\incident-c_o336v9
Traceback (most recent call last):
  File "D:\a\Void.Language\Void.Language\tests\math\preservation.py", line 63, in <module>
    assert record['cwd']==str(root)
           ^^^^^^^^^^^^^^^^^^^^^^^^
AssertionError
```


## os_errors

```text
102/319 Testing: os_errors
102/319 Test: os_errors
Command: "C:/hostedtoolcache/windows/Python/3.12.10/x64/python3.exe" "D:/a/Void.Language/Void.Language/tests/os/errors.py" "D:/a/Void.Language/Void.Language/build/Debug/voidc.exe"
Directory: D:/a/Void.Language/Void.Language/build/Debug
"os_errors" start time: Oct 29 11:20 Coordinated Universal Time
Output:
----------------------------------------------------------
Traceback (most recent call last):
  File "D:\a\Void.Language\Void.Language\tests\os\errors.py", line 22, in <module>
    fixture.write_text('\n'.join(s)+'\n')
  File "C:\hostedtoolcache\windows\Python\3.12.10\x64\Lib\pathlib.py", line 1048, in write_text
    return f.write(data)
           ^^^^^^^^^^^^^
  File "C:\hostedtoolcache\windows\Python\3.12.10\x64\Lib\encodings\cp1252.py", line 19, in encode
    return codecs.charmap_encode(input,self.errors,encoding_table)[0]
           ^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^
UnicodeEncodeError: 'charmap' codec can't encode character '\u96ea' in position 1337: character maps to <undefined>
```


## lsp_os

```text
104/319 Testing: lsp_os
104/319 Test: lsp_os
Command: "C:/hostedtoolcache/windows/Python/3.12.10/x64/python3.exe" "D:/a/Void.Language/Void.Language/tests/os/lsp.py" "D:/a/Void.Language/Void.Language/build/Debug/voidls.exe"
Directory: D:/a/Void.Language/Void.Language/build/Debug
"lsp_os" start time: Oct 29 11:21 Coordinated Universal Time
Output:
----------------------------------------------------------
Traceback (most recent call last):
  File "D:\a\Void.Language\Void.Language\tests\os\lsp.py", line 22, in <module>
    entry.write_text(base)
  File "C:\hostedtoolcache\windows\Python\3.12.10\x64\Lib\pathlib.py", line 1048, in write_text
    return f.write(data)
           ^^^^^^^^^^^^^
  File "C:\hostedtoolcache\windows\Python\3.12.10\x64\Lib\encodings\cp1252.py", line 19, in encode
    return codecs.charmap_encode(input,self.errors,encoding_table)[0]
           ^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^
UnicodeEncodeError: 'charmap' codec can't encode character '\U0001f600' in position 48: character maps to <undefined>
```


## os_owner

```text
106/319 Testing: os_owner
106/319 Test: os_owner
Command: "C:/hostedtoolcache/windows/Python/3.12.10/x64/python3.exe" "D:/a/Void.Language/Void.Language/tests/os/owner.py" "D:/a/Void.Language/Void.Language/build/Debug/voidc.exe" "D:/a/Void.Language/Void.Language/build/Debug/void_io_runner.exe"
Directory: D:/a/Void.Language/Void.Language/build/Debug
"os_owner" start time: Oct 29 11:21 Coordinated Universal Time
Output:
----------------------------------------------------------
CRASH EVIDENCE: D:\a\Void.Language\Void.Language\docs\os\evidence\crashes\incident-6ll1vfyy
Traceback (most recent call last):
  File "D:\a\Void.Language\Void.Language\tests\os\owner.py", line 17, in <module>
    h.run([runner,'--async',source],work,b'Debug source execution passed\nO2 source execution passed\n',sources=(source,))
  File "D:\a\Void.Language\Void.Language\tests\os\support.py", line 11, in run
    return harness.run(command, cwd, *args, **kwargs)
           ^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^
  File "D:\a\Void.Language\Void.Language\tests\random\harness.py", line 29, in run
    assert result.stdout == expected, (args, result.stdout[:4000], expected[:4000], result.stderr)
           ^^^^^^^^^^^^^^^^^^^^^^^^^
AssertionError: (['D:\\a\\Void.Language\\Void.Language\\build\\Debug\\void_io_runner.exe', '--async', 'C:\\Users\\RUNNER~1\\AppData\\Local\\Temp\\VOID OS owner 37vbs9_t\\owner.void'], b'Debug source execution passed\r\nO2 source execution passed\r\n', b'Debug source execution passed\nO2 source execution passed\n', b'')
```


## os_examples

```text
107/319 Testing: os_examples
107/319 Test: os_examples
Command: "C:/hostedtoolcache/windows/Python/3.12.10/x64/python3.exe" "D:/a/Void.Language/Void.Language/tests/os/examples.py" "D:/a/Void.Language/Void.Language/build/Debug/voidc.exe"
Directory: D:/a/Void.Language/Void.Language/build/Debug
"os_examples" start time: Oct 29 11:21 Coordinated Universal Time
Output:
----------------------------------------------------------
CRASH EVIDENCE: D:\a\Void.Language\Void.Language\docs\os\evidence\crashes\incident-mwjxel2h
Traceback (most recent call last):
  File "D:\a\Void.Language\Void.Language\tests\os\examples.py", line 20, in <module>
    h.run([compiler,'build',examples/'resources.void','-o',native,*flags],work)
  File "D:\a\Void.Language\Void.Language\tests\os\support.py", line 11, in run
    return harness.run(command, cwd, *args, **kwargs)
           ^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^
  File "D:\a\Void.Language\Void.Language\tests\random\harness.py", line 27, in run
    assert b'warning' not in result.stderr.lower(), result.stderr
           ^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^
AssertionError: b"clang: warning: argument unused during compilation: '-pthread' [-Wunused-command-line-argument]\r\n"
```


## os_install

```text
108/319 Testing: os_install
108/319 Test: os_install
Command: "C:/hostedtoolcache/windows/Python/3.12.10/x64/python3.exe" "D:/a/Void.Language/Void.Language/tests/os/install.py" "D:/a/Void.Language/Void.Language/build/Debug/voidc.exe"
Directory: D:/a/Void.Language/Void.Language/build/Debug
"os_install" start time: Oct 29 11:22 Coordinated Universal Time
Output:
----------------------------------------------------------
CRASH EVIDENCE: D:\a\Void.Language\Void.Language\docs\os\evidence\crashes\incident-o5ap3d1i
Traceback (most recent call last):
  File "D:\a\Void.Language\Void.Language\tests\os\install.py", line 44, in <module>
    h.run([installed,'build',fixture,'-o',binary,*args],work,env=env)
  File "D:\a\Void.Language\Void.Language\tests\os\support.py", line 11, in run
    return harness.run(command, cwd, *args, **kwargs)
           ^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^
  File "D:\a\Void.Language\Void.Language\tests\random\harness.py", line 27, in run
    assert b'warning' not in result.stderr.lower(), result.stderr
           ^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^
AssertionError: b"clang: warning: argument unused during compilation: '-pthread' [-Wunused-command-line-argument]\r\n"
```


## random_execution

```text
110/319 Testing: random_execution
110/319 Test: random_execution
Command: "C:/hostedtoolcache/windows/Python/3.12.10/x64/python3.exe" "D:/a/Void.Language/Void.Language/tests/random/execution.py" "D:/a/Void.Language/Void.Language/build/Debug/voidc.exe"
Directory: D:/a/Void.Language/Void.Language/build/Debug
"random_execution" start time: Oct 29 11:22 Coordinated Universal Time
Output:
----------------------------------------------------------
Traceback (most recent call last):
  File "D:\a\Void.Language\Void.Language\tests\random\execution.py", line 183, in <module>
    h.run([compiler,'build',fixture,'-o',executable,*flags],work,small=small)
  File "D:\a\Void.Language\Void.Language\tests\random\harness.py", line 27, in run
    assert b'warning' not in result.stderr.lower(), result.stderr
           ^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^
AssertionError: b"clang: warning: argument unused during compilation: '-pthread' [-Wunused-command-line-argument]\r\n"
```


## random_install

```text
113/319 Testing: random_install
113/319 Test: random_install
Command: "C:/hostedtoolcache/windows/Python/3.12.10/x64/python3.exe" "D:/a/Void.Language/Void.Language/tests/random/install.py" "D:/a/Void.Language/Void.Language/build/Debug/voidc.exe"
Directory: D:/a/Void.Language/Void.Language/build/Debug
"random_install" start time: Oct 29 11:23 Coordinated Universal Time
Output:
----------------------------------------------------------
Traceback (most recent call last):
  File "D:\a\Void.Language\Void.Language\tests\random\install.py", line 33, in <module>
    h.run([installed,'build',fixture,'-o',executable,*flags],work,env=env)
  File "D:\a\Void.Language\Void.Language\tests\random\harness.py", line 27, in run
    assert b'warning' not in result.stderr.lower(), result.stderr
           ^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^
AssertionError: b"clang: warning: argument unused during compilation: '-pthread' [-Wunused-command-line-argument]\r\n"
```


## random_errors

```text
114/319 Testing: random_errors
114/319 Test: random_errors
Command: "C:/hostedtoolcache/windows/Python/3.12.10/x64/python3.exe" "D:/a/Void.Language/Void.Language/tests/random/errors.py" "D:/a/Void.Language/Void.Language/build/Debug/voidc.exe"
Directory: D:/a/Void.Language/Void.Language/build/Debug
"random_errors" start time: Oct 29 11:23 Coordinated Universal Time
Output:
----------------------------------------------------------
Traceback (most recent call last):
  File "D:\a\Void.Language\Void.Language\tests\random\errors.py", line 15, in <module>
    assert str(source).encode() in result.stderr,result.stderr
           ^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^
AssertionError: b'D:/a/Void.Language/Void.Language/stdlib/convert.void:55:12: runtime error: convert.unwrap received Err; inspect the result or use unwrap_or\r\n    return _convert_unwrap<t, e>(outcome)\r\n           ^\r\nIn function /@stdlib/convert.void::unwrap\r\nCalled from /@stdlib/random.void::_arity_rand_i32_1\r\nD:/a/Void.Language/Void.Language/stdlib/random.void:814:20: source call\r\n    return convert.unwrap(_arity_try_rand_i32_1(maximum))\r\n                   ^\r\nCalled from /error0.void::main\r\nC:/Users/runneradmin/AppData/Local/Temp/VOID random errors 724f9049/error0.void:6:24: source call\r\n    var value = random.rand_i32(0)\r\n                       ^\r\nCalled from /error0.void::$app_adapter\r\nC:/Users/runneradmin/AppData/Local/Temp/VOID random errors 724f9049/error0.void:4:6: source call\r\n\r\n'
```


## random_preservation

```text
115/319 Testing: random_preservation
115/319 Test: random_preservation
Command: "C:/hostedtoolcache/windows/Python/3.12.10/x64/python3.exe" "D:/a/Void.Language/Void.Language/tests/random/preservation.py"
Directory: D:/a/Void.Language/Void.Language/build/Debug
"random_preservation" start time: Oct 29 11:23 Coordinated Universal Time
Output:
----------------------------------------------------------
CRASH EVIDENCE: C:\Users\RUNNER~1\AppData\Local\Temp\VOID random synthetic preservation 7cnw7xud\evidence\incident-34je2qdp
CRASH EVIDENCE: C:\Users\RUNNER~1\AppData\Local\Temp\VOID random synthetic preservation 7cnw7xud\evidence\incident-4jgtrcv1
CRASH EVIDENCE: C:\Users\RUNNER~1\AppData\Local\Temp\VOID random synthetic preservation 7cnw7xud\evidence\incident-ew3cd85e
Traceback (most recent call last):
  File "D:\a\Void.Language\Void.Language\tests\random\preservation.py", line 32, in <module>
    preserved=next(x for x in record['sources'] if x['original']==str(source))
              ^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^
StopIteration
```


## random_worker

```text
116/319 Testing: random_worker
116/319 Test: random_worker
Command: "C:/hostedtoolcache/windows/Python/3.12.10/x64/python3.exe" "D:/a/Void.Language/Void.Language/tests/random/worker.py" "D:/a/Void.Language/Void.Language/build/Debug/voidc.exe" "D:/a/Void.Language/Void.Language/build/Debug/void_io_runner.exe"
Directory: D:/a/Void.Language/Void.Language/build/Debug
"random_worker" start time: Oct 29 11:23 Coordinated Universal Time
Output:
----------------------------------------------------------
Traceback (most recent call last):
  File "D:\a\Void.Language\Void.Language\tests\random\worker.py", line 17, in <module>
    h.run([runner,'--async',source],work,b'Debug source execution passed\nO2 source execution passed\n',sources=(source,))
  File "D:\a\Void.Language\Void.Language\tests\random\harness.py", line 29, in run
    assert result.stdout == expected, (args, result.stdout[:4000], expected[:4000], result.stderr)
           ^^^^^^^^^^^^^^^^^^^^^^^^^
AssertionError: (['D:\\a\\Void.Language\\Void.Language\\build\\Debug\\void_io_runner.exe', '--async', 'C:\\Users\\RUNNER~1\\AppData\\Local\\Temp\\VOID random worker xm548pen\\worker.void'], b'Debug source execution passed\r\nO2 source execution passed\r\n', b'Debug source execution passed\nO2 source execution passed\n', b'')
```


## binary_execution

```text
118/319 Testing: binary_execution
118/319 Test: binary_execution
Command: "C:/hostedtoolcache/windows/Python/3.12.10/x64/python3.exe" "D:/a/Void.Language/Void.Language/tests/binary/execution.py" "D:/a/Void.Language/Void.Language/build/Debug/voidc.exe"
Directory: D:/a/Void.Language/Void.Language/build/Debug
"binary_execution" start time: Oct 29 11:23 Coordinated Universal Time
Output:
----------------------------------------------------------
Traceback (most recent call last):
  File "D:\a\Void.Language\Void.Language\tests\binary\execution.py", line 196, in <module>
    run([COMPILER, "build", path, "-o", executable, *flags])
  File "D:\a\Void.Language\Void.Language\tests\binary\execution.py", line 22, in run
    assert b"warning" not in result.stderr.lower(), (arguments, result.stderr)
           ^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^
AssertionError: ([WindowsPath('D:/a/Void.Language/Void.Language/build/Debug/voidc.exe'), 'build', WindowsPath('C:/Users/RUNNER~1/AppData/Local/Temp/VOID binary \xe9\U0001f600 fkgbiedw/main.void'), '-o', WindowsPath('C:/Users/RUNNER~1/AppData/Local/Temp/VOID binary \xe9\U0001f600 fkgbiedw/binaryTrue.exe'), '--no-opt'], b"clang: warning: argument unused during compilation: '-pthread' [-Wunused-command-line-argument]\r\n")
```


## combined_execution

```text
125/319 Testing: combined_execution
125/319 Test: combined_execution
Command: "C:/hostedtoolcache/windows/Python/3.12.10/x64/python3.exe" "D:/a/Void.Language/Void.Language/tests/combined/execution.py" "D:/a/Void.Language/Void.Language/build/Debug/voidc.exe"
Directory: D:/a/Void.Language/Void.Language/build/Debug
"combined_execution" start time: Oct 29 11:24 Coordinated Universal Time
Output:
----------------------------------------------------------
Traceback (most recent call last):
  File "D:\a\Void.Language\Void.Language\tests\combined\execution.py", line 145, in <module>
    run([compiler, 'build', source, '-o', executable, *flags])
  File "D:\a\Void.Language\Void.Language\tests\combined\execution.py", line 25, in run
    assert b'warning' not in result.stderr.lower(), result.stderr
           ^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^
AssertionError: b"clang: warning: argument unused during compilation: '-pthread' [-Wunused-command-line-argument]\r\n"
```


## memory_execution

```text
127/319 Testing: memory_execution
127/319 Test: memory_execution
Command: "C:/hostedtoolcache/windows/Python/3.12.10/x64/python3.exe" "D:/a/Void.Language/Void.Language/tests/memory/execution.py" "D:/a/Void.Language/Void.Language/build/Debug/voidc.exe"
Directory: D:/a/Void.Language/Void.Language/build/Debug
"memory_execution" start time: Oct 29 11:24 Coordinated Universal Time
Output:
----------------------------------------------------------
Traceback (most recent call last):
  File "D:\a\Void.Language\Void.Language\tests\memory\execution.py", line 252, in <module>
    run([compiler, "build", path, "-o", executable, *flags])
  File "D:\a\Void.Language\Void.Language\tests\memory\execution.py", line 21, in run
    assert b"warning" not in result.stderr.lower(), (arguments, result.stderr)
           ^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^
AssertionError: ([WindowsPath('D:/a/Void.Language/Void.Language/build/Debug/voidc.exe'), 'build', WindowsPath('D:/a/Void.Language/Void.Language/tests/memory/semantics.void'), '-o', WindowsPath('C:/Users/RUNNER~1/AppData/Local/Temp/VOID memory \xe9\U0001f600 pnlcnezn/semanticsTrue.exe'), '--no-opt'], b"clang: warning: argument unused during compilation: '-pthread' [-Wunused-command-line-argument]\r\n")
```


## text_encoding_text

```text
129/319 Testing: text_encoding_text
129/319 Test: text_encoding_text
Command: "C:/hostedtoolcache/windows/Python/3.12.10/x64/python3.exe" "D:/a/Void.Language/Void.Language/tests/text_encoding/execution.py" "D:/a/Void.Language/Void.Language/build/Debug/voidc.exe" "text"
Directory: D:/a/Void.Language/Void.Language/build/Debug
"text_encoding_text" start time: Oct 29 11:24 Coordinated Universal Time
Output:
----------------------------------------------------------
PASS D:\a\Void.Language\Void.Language\build\Debug\voidc.exe check D:\a\Void.Language\Void.Language\tests\text_encoding\text.void 0.140s
PASS D:\a\Void.Language\Void.Language\build\Debug\voidc.exe run D:\a\Void.Language\Void.Language\tests\text_encoding\text.void --no-opt 1.204s
PASS D:\a\Void.Language\Void.Language\build\Debug\voidc.exe run D:\a\Void.Language\Void.Language\tests\text_encoding\text.void 1.281s
Traceback (most recent call last):
  File "D:\a\Void.Language\Void.Language\tests\text_encoding\execution.py", line 50, in <module>
    invoke(build, cwd, fixture)
  File "D:\a\Void.Language\Void.Language\tests\text_encoding\execution.py", line 27, in invoke
    assert b'warning' not in result.stderr.lower(), result.stderr
           ^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^
AssertionError: b"clang: warning: argument unused during compilation: '-pthread' [-Wunused-command-line-argument]\r\n"
```


## text_encoding_hex

```text
130/319 Testing: text_encoding_hex
130/319 Test: text_encoding_hex
Command: "C:/hostedtoolcache/windows/Python/3.12.10/x64/python3.exe" "D:/a/Void.Language/Void.Language/tests/text_encoding/execution.py" "D:/a/Void.Language/Void.Language/build/Debug/voidc.exe" "hex"
Directory: D:/a/Void.Language/Void.Language/build/Debug
"text_encoding_hex" start time: Oct 29 11:24 Coordinated Universal Time
Output:
----------------------------------------------------------
PASS D:\a\Void.Language\Void.Language\build\Debug\voidc.exe check D:\a\Void.Language\Void.Language\tests\text_encoding\hex.void 0.047s
PASS D:\a\Void.Language\Void.Language\build\Debug\voidc.exe run D:\a\Void.Language\Void.Language\tests\text_encoding\hex.void --no-opt 0.344s
PASS D:\a\Void.Language\Void.Language\build\Debug\voidc.exe run D:\a\Void.Language\Void.Language\tests\text_encoding\hex.void 0.375s
Traceback (most recent call last):
  File "D:\a\Void.Language\Void.Language\tests\text_encoding\execution.py", line 50, in <module>
    invoke(build, cwd, fixture)
  File "D:\a\Void.Language\Void.Language\tests\text_encoding\execution.py", line 27, in invoke
    assert b'warning' not in result.stderr.lower(), result.stderr
           ^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^
AssertionError: b"clang: warning: argument unused during compilation: '-pthread' [-Wunused-command-line-argument]\r\n"
```


## text_encoding_base64

```text
131/319 Testing: text_encoding_base64
131/319 Test: text_encoding_base64
Command: "C:/hostedtoolcache/windows/Python/3.12.10/x64/python3.exe" "D:/a/Void.Language/Void.Language/tests/text_encoding/execution.py" "D:/a/Void.Language/Void.Language/build/Debug/voidc.exe" "base64"
Directory: D:/a/Void.Language/Void.Language/build/Debug
"text_encoding_base64" start time: Oct 29 11:24 Coordinated Universal Time
Output:
----------------------------------------------------------
PASS D:\a\Void.Language\Void.Language\build\Debug\voidc.exe check D:\a\Void.Language\Void.Language\tests\text_encoding\base64.void 0.094s
PASS D:\a\Void.Language\Void.Language\build\Debug\voidc.exe run D:\a\Void.Language\Void.Language\tests\text_encoding\base64.void --no-opt 0.688s
PASS D:\a\Void.Language\Void.Language\build\Debug\voidc.exe run D:\a\Void.Language\Void.Language\tests\text_encoding\base64.void 0.734s
Traceback (most recent call last):
  File "D:\a\Void.Language\Void.Language\tests\text_encoding\execution.py", line 50, in <module>
    invoke(build, cwd, fixture)
  File "D:\a\Void.Language\Void.Language\tests\text_encoding\execution.py", line 27, in invoke
    assert b'warning' not in result.stderr.lower(), result.stderr
           ^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^
AssertionError: b"clang: warning: argument unused during compilation: '-pthread' [-Wunused-command-line-argument]\r\n"
```


## text_encoding_ascii

```text
132/319 Testing: text_encoding_ascii
132/319 Test: text_encoding_ascii
Command: "C:/hostedtoolcache/windows/Python/3.12.10/x64/python3.exe" "D:/a/Void.Language/Void.Language/tests/text_encoding/execution.py" "D:/a/Void.Language/Void.Language/build/Debug/voidc.exe" "ascii"
Directory: D:/a/Void.Language/Void.Language/build/Debug
"text_encoding_ascii" start time: Oct 29 11:24 Coordinated Universal Time
Output:
----------------------------------------------------------
PASS D:\a\Void.Language\Void.Language\build\Debug\voidc.exe check D:\a\Void.Language\Void.Language\tests\text_encoding\ascii.void 0.047s
PASS D:\a\Void.Language\Void.Language\build\Debug\voidc.exe run D:\a\Void.Language\Void.Language\tests\text_encoding\ascii.void --no-opt 0.343s
PASS D:\a\Void.Language\Void.Language\build\Debug\voidc.exe run D:\a\Void.Language\Void.Language\tests\text_encoding\ascii.void 0.360s
Traceback (most recent call last):
  File "D:\a\Void.Language\Void.Language\tests\text_encoding\execution.py", line 50, in <module>
    invoke(build, cwd, fixture)
  File "D:\a\Void.Language\Void.Language\tests\text_encoding\execution.py", line 27, in invoke
    assert b'warning' not in result.stderr.lower(), result.stderr
           ^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^
AssertionError: b"clang: warning: argument unused during compilation: '-pthread' [-Wunused-command-line-argument]\r\n"
```


## text_encoding_oracles

```text
133/319 Testing: text_encoding_oracles
133/319 Test: text_encoding_oracles
Command: "C:/hostedtoolcache/windows/Python/3.12.10/x64/python3.exe" "D:/a/Void.Language/Void.Language/tests/text_encoding/execution.py" "D:/a/Void.Language/Void.Language/build/Debug/voidc.exe" "oracles"
Directory: D:/a/Void.Language/Void.Language/build/Debug
"text_encoding_oracles" start time: Oct 29 11:24 Coordinated Universal Time
Output:
----------------------------------------------------------
PASS D:\a\Void.Language\Void.Language\build\Debug\voidc.exe check D:\a\Void.Language\Void.Language\tests\text_encoding\oracles.void 2.281s
PASS D:\a\Void.Language\Void.Language\build\Debug\voidc.exe run D:\a\Void.Language\Void.Language\tests\text_encoding\oracles.void --no-opt 38.547s
PASS D:\a\Void.Language\Void.Language\build\Debug\voidc.exe run D:\a\Void.Language\Void.Language\tests\text_encoding\oracles.void 43.782s
Traceback (most recent call last):
  File "D:\a\Void.Language\Void.Language\tests\text_encoding\execution.py", line 50, in <module>
    invoke(build, cwd, fixture)
  File "D:\a\Void.Language\Void.Language\tests\text_encoding\execution.py", line 27, in invoke
    assert b'warning' not in result.stderr.lower(), result.stderr
           ^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^
AssertionError: b"clang: warning: argument unused during compilation: '-pthread' [-Wunused-command-line-argument]\r\n"
```


## text_encoding_interop

```text
134/319 Testing: text_encoding_interop
134/319 Test: text_encoding_interop
Command: "C:/hostedtoolcache/windows/Python/3.12.10/x64/python3.exe" "D:/a/Void.Language/Void.Language/tests/text_encoding/execution.py" "D:/a/Void.Language/Void.Language/build/Debug/voidc.exe" "interop"
Directory: D:/a/Void.Language/Void.Language/build/Debug
"text_encoding_interop" start time: Oct 29 11:26 Coordinated Universal Time
Output:
----------------------------------------------------------
PASS D:\a\Void.Language\Void.Language\build\Debug\voidc.exe check D:\a\Void.Language\Void.Language\tests\text_encoding\interop.void 0.813s
PASS D:\a\Void.Language\Void.Language\build\Debug\voidc.exe run D:\a\Void.Language\Void.Language\tests\text_encoding\interop.void --no-opt 3.453s
PASS D:\a\Void.Language\Void.Language\build\Debug\voidc.exe run D:\a\Void.Language\Void.Language\tests\text_encoding\interop.void 3.609s
Traceback (most recent call last):
  File "D:\a\Void.Language\Void.Language\tests\text_encoding\execution.py", line 50, in <module>
    invoke(build, cwd, fixture)
  File "D:\a\Void.Language\Void.Language\tests\text_encoding\execution.py", line 27, in invoke
    assert b'warning' not in result.stderr.lower(), result.stderr
           ^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^
AssertionError: b"clang: warning: argument unused during compilation: '-pthread' [-Wunused-command-line-argument]\r\n"
```


## text_encoding_large

```text
135/319 Testing: text_encoding_large
135/319 Test: text_encoding_large
Command: "C:/hostedtoolcache/windows/Python/3.12.10/x64/python3.exe" "D:/a/Void.Language/Void.Language/tests/text_encoding/execution.py" "D:/a/Void.Language/Void.Language/build/Debug/voidc.exe" "large"
Directory: D:/a/Void.Language/Void.Language/build/Debug
"text_encoding_large" start time: Oct 29 11:26 Coordinated Universal Time
Output:
----------------------------------------------------------
PASS D:\a\Void.Language\Void.Language\build\Debug\voidc.exe check D:\a\Void.Language\Void.Language\tests\text_encoding\large.void 0.188s
PASS D:\a\Void.Language\Void.Language\build\Debug\voidc.exe run D:\a\Void.Language\Void.Language\tests\text_encoding\large.void --no-opt 20.765s
PASS D:\a\Void.Language\Void.Language\build\Debug\voidc.exe run D:\a\Void.Language\Void.Language\tests\text_encoding\large.void 20.672s
Traceback (most recent call last):
  File "D:\a\Void.Language\Void.Language\tests\text_encoding\execution.py", line 50, in <module>
    invoke(build, cwd, fixture)
  File "D:\a\Void.Language\Void.Language\tests\text_encoding\execution.py", line 27, in invoke
    assert b'warning' not in result.stderr.lower(), result.stderr
           ^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^
AssertionError: b"clang: warning: argument unused during compilation: '-pthread' [-Wunused-command-line-argument]\r\n"
```


## text_encoding_errors

```text
136/319 Testing: text_encoding_errors
136/319 Test: text_encoding_errors
Command: "C:/hostedtoolcache/windows/Python/3.12.10/x64/python3.exe" "D:/a/Void.Language/Void.Language/tests/text_encoding/execution.py" "D:/a/Void.Language/Void.Language/build/Debug/voidc.exe" "errors"
Directory: D:/a/Void.Language/Void.Language/build/Debug
"text_encoding_errors" start time: Oct 29 11:27 Coordinated Universal Time
Output:
----------------------------------------------------------
PASS D:\a\Void.Language\Void.Language\build\Debug\voidc.exe check D:\a\Void.Language\Void.Language\tests\text_encoding\errors.void 0.219s
PASS D:\a\Void.Language\Void.Language\build\Debug\voidc.exe run D:\a\Void.Language\Void.Language\tests\text_encoding\errors.void --no-opt 1.453s
PASS D:\a\Void.Language\Void.Language\build\Debug\voidc.exe run D:\a\Void.Language\Void.Language\tests\text_encoding\errors.void 1.516s
Traceback (most recent call last):
  File "D:\a\Void.Language\Void.Language\tests\text_encoding\execution.py", line 50, in <module>
    invoke(build, cwd, fixture)
  File "D:\a\Void.Language\Void.Language\tests\text_encoding\execution.py", line 27, in invoke
    assert b'warning' not in result.stderr.lower(), result.stderr
           ^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^
AssertionError: b"clang: warning: argument unused during compilation: '-pthread' [-Wunused-command-line-argument]\r\n"
```


## text_encoding_crash_preservation

```text
137/319 Testing: text_encoding_crash_preservation
137/319 Test: text_encoding_crash_preservation
Command: "C:/hostedtoolcache/windows/Python/3.12.10/x64/python3.exe" "D:/a/Void.Language/Void.Language/tests/text_encoding/crash_test.py"
Directory: D:/a/Void.Language/Void.Language/build/Debug
"text_encoding_crash_preservation" start time: Oct 29 11:27 Coordinated Universal Time
Output:
----------------------------------------------------------
Traceback (most recent call last):
  File "D:\a\Void.Language\Void.Language\tests\text_encoding\crash_test.py", line 207, in <module>
    exercise(disposable)
  File "D:\a\Void.Language\Void.Language\tests\text_encoding\crash_test.py", line 37, in exercise
    first = run([selected_executable, '-c', script, original], cwd=root, capture_output=True,
            ^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^
  File "D:\a\Void.Language\Void.Language\tests\text_encoding\crash.py", line 252, in run
    with subprocess.Popen(command, *args, **kwargs) as process:
         ^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^
  File "D:\a\Void.Language\Void.Language\tests\text_encoding\crash.py", line 343, in __init__
    super().__init__(command, *args, **kwargs)
  File "C:\hostedtoolcache\windows\Python\3.12.10\x64\Lib\subprocess.py", line 1026, in __init__
    self._execute_child(args, executable, preexec_fn, close_fds,
  File "C:\hostedtoolcache\windows\Python\3.12.10\x64\Lib\subprocess.py", line 1538, in _execute_child
    hp, ht, pid, tid = _winapi.CreateProcess(executable, args,
                       ^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^
FileNotFoundError: [WinError 2] The system cannot find the file specified
```


## lsp_text_encoding

```text
138/319 Testing: lsp_text_encoding
138/319 Test: lsp_text_encoding
Command: "C:/hostedtoolcache/windows/Python/3.12.10/x64/python3.exe" "D:/a/Void.Language/Void.Language/tests/text_encoding/lsp.py" "D:/a/Void.Language/Void.Language/build/Debug/voidls.exe"
Directory: D:/a/Void.Language/Void.Language/build/Debug
"lsp_text_encoding" start time: Oct 29 11:27 Coordinated Universal Time
Output:
----------------------------------------------------------
Traceback (most recent call last):
  File "D:\a\Void.Language\Void.Language\tests\text_encoding\lsp.py", line 98, in <module>
    replace(base + '    var selected = ' + call)
  File "D:\a\Void.Language\Void.Language\tests\text_encoding\lsp.py", line 56, in replace
    entry.write_text(updated)
  File "C:\hostedtoolcache\windows\Python\3.12.10\x64\Lib\pathlib.py", line 1048, in write_text
    return f.write(data)
           ^^^^^^^^^^^^^
  File "C:\hostedtoolcache\windows\Python\3.12.10\x64\Lib\encodings\cp1252.py", line 19, in encode
    return codecs.charmap_encode(input,self.errors,encoding_table)[0]
           ^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^
UnicodeEncodeError: 'charmap' codec can't encode character '\u96ea' in position 348: character maps to <undefined>

During handling of the above exception, another exception occurred:

Traceback (most recent call last):
  File "D:\a\Void.Language\Void.Language\tests\text_encoding\lsp.py", line 120, in <module>
    check(status == 0, (status, bytes(client.logs)))
  File "D:\a\Void.Language\Void.Language\tests\text_encoding\lsp.py", line 20, in check
    assert valid, detail
           ^^^^^
AssertionError: (1, b'')
```


## text_encoding_install

```text
139/319 Testing: text_encoding_install
139/319 Test: text_encoding_install
Command: "C:/hostedtoolcache/windows/Python/3.12.10/x64/python3.exe" "D:/a/Void.Language/Void.Language/tests/text_encoding/install.py" "D:/a/Void.Language/Void.Language/build/Debug/voidc.exe" "D:/a/Void.Language/Void.Language/build/Debug" "Debug"
Directory: D:/a/Void.Language/Void.Language/build/Debug
"text_encoding_install" start time: Oct 29 11:27 Coordinated Universal Time
Output:
----------------------------------------------------------
Traceback (most recent call last):
  File "D:\a\Void.Language\Void.Language\tests\text_encoding\install.py", line 39, in <module>
    invoke([installed, 'build', fixture, '-o', output], root)
  File "D:\a\Void.Language\Void.Language\tests\text_encoding\install.py", line 19, in invoke
    assert b'warning' not in result.stderr.lower(), result.stderr
           ^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^
AssertionError: b"clang: warning: argument unused during compilation: '-pthread' [-Wunused-command-line-argument]\r\n"
```


## log_execution

```text
150/319 Testing: log_execution
150/319 Test: log_execution
Command: "C:/hostedtoolcache/windows/Python/3.12.10/x64/python3.exe" "D:/a/Void.Language/Void.Language/tests/log/execution.py" "D:/a/Void.Language/Void.Language/build/Debug/voidc.exe" "--small-stack"
Directory: D:/a/Void.Language/Void.Language/build/Debug
"log_execution" start time: Oct 29 11:32 Coordinated Universal Time
Output:
----------------------------------------------------------
CRASH EVIDENCE: D:\a\Void.Language\Void.Language\docs\log\evidence\crashes\incident-zjgbt_dy
Traceback (most recent call last):
  File "D:\a\Void.Language\Void.Language\tests\log\execution.py", line 25, in <module>
    h.run([compiler,'run',source,'7','--no-cache',*([] if optimized else ['--no-opt'])],cwd=cwd,expected=expected,small=small)
  File "D:\a\Void.Language\Void.Language\tests\log\support.py", line 73, in run
    assert not result.expectation_failed, (receipt, args, result.returncode, result.stdout, result.stderr.decode(errors='replace'))
           ^^^^^^^^^^^^^^^^^^^^^^^^^^^^^
AssertionError: (WindowsPath('D:/a/Void.Language/Void.Language/docs/log/evidence/run-rvys5brg'), ['D:\\a\\Void.Language\\Void.Language\\build\\Debug\\voidc.exe', 'run', 'D:\\a\\Void.Language\\Void.Language\\tests\\log\\smoke.void', '7', '--no-cache', '--no-opt'], 0, b'INFO "" "hello\\n\xe9\x9b\xaa"\n7\r\n', '')
```


## log_oracles

```text
153/319 Testing: log_oracles
153/319 Test: log_oracles
Command: "C:/hostedtoolcache/windows/Python/3.12.10/x64/python3.exe" "D:/a/Void.Language/Void.Language/tests/log/oracles.py" "D:/a/Void.Language/Void.Language/build/Debug/voidc.exe"
Directory: D:/a/Void.Language/Void.Language/build/Debug
"log_oracles" start time: Oct 29 11:34 Coordinated Universal Time
Output:
----------------------------------------------------------
Traceback (most recent call last):
  File "D:\a\Void.Language\Void.Language\tests\log\oracles.py", line 71, in <module>
    value=datetime.fromtimestamp(seconds,timezone.utc)
          ^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^
OSError: [Errno 22] Invalid argument
```


## log_cache

```text
154/319 Testing: log_cache
154/319 Test: log_cache
Command: "C:/hostedtoolcache/windows/Python/3.12.10/x64/python3.exe" "D:/a/Void.Language/Void.Language/tests/log/cache.py" "D:/a/Void.Language/Void.Language/build/Debug/voidc.exe"
Directory: D:/a/Void.Language/Void.Language/build/Debug
"log_cache" start time: Oct 29 11:35 Coordinated Universal Time
Output:
----------------------------------------------------------
Traceback (most recent call last):
  File "D:\a\Void.Language\Void.Language\tests\log\cache.py", line 21, in <module>
    artifacts=set(cache.iterdir());assert len(artifacts-before)==1,'miss did not publish code'
              ^^^^^^^^^^^^^^^^^^^^
  File "C:\hostedtoolcache\windows\Python\3.12.10\x64\Lib\pathlib.py", line 1056, in iterdir
    for name in os.listdir(self):
                ^^^^^^^^^^^^^^^^
FileNotFoundError: [WinError 3] The system cannot find the path specified: 'C:\\Users\\RUNNER~1\\AppData\\Local\\Temp\\VOID-log-cache-42x0zcd5\\.void\\cache\\v1'
```


## log_install

```text
155/319 Testing: log_install
155/319 Test: log_install
Command: "C:/hostedtoolcache/windows/Python/3.12.10/x64/python3.exe" "D:/a/Void.Language/Void.Language/tests/log/install.py" "D:/a/Void.Language/Void.Language/build/Debug/voidc.exe"
Directory: D:/a/Void.Language/Void.Language/build/Debug
"log_install" start time: Oct 29 11:35 Coordinated Universal Time
Output:
----------------------------------------------------------
CRASH EVIDENCE: D:\a\Void.Language\Void.Language\docs\log\evidence\crashes\incident-gvjmq5cr
Traceback (most recent call last):
  File "D:\a\Void.Language\Void.Language\tests\log\install.py", line 34, in <module>
    h.run([sys.executable,Path(__file__).with_name('lsp.py'),server],cwd=work,env=env,sources=[source],context={'mode':'installed-lsp'});checks+=1
    ^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^
  File "D:\a\Void.Language\Void.Language\tests\log\support.py", line 73, in run
    assert not result.expectation_failed, (receipt, args, result.returncode, result.stdout, result.stderr.decode(errors='replace'))
           ^^^^^^^^^^^^^^^^^^^^^^^^^^^^^
AssertionError: (WindowsPath('D:/a/Void.Language/Void.Language/docs/log/evidence/run-n3eoi39u'), ['C:\\hostedtoolcache\\windows\\Python\\3.12.10\\x64\\python3.exe', 'D:\\a\\Void.Language\\Void.Language\\tests\\log\\lsp.py', 'C:\\Users\\RUNNER~1\\AppData\\Local\\Temp\\VOID-log-installed-qx50bxjl\\relocated \u96ea # %\\bin\\voidls.exe'], 1, b'', 'Traceback (most recent call last):\r\n  File "D:\\a\\Void.Language\\Void.Language\\tests\\log\\lsp.py", line 30, in <module>\r\n    entry.write_text(base)\r\n  File "C:\\hostedtoolcache\\windows\\Python\\3.12.10\\x64\\Lib\\pathlib.py", line 1048, in write_text\r\n    return f.write(data)\r\n           ^^^^^^^^^^^^^\r\n  File "C:\\hostedtoolcache\\windows\\Python\\3.12.10\\x64\\Lib\\encodings\\cp1252.py", line 19, in encode\r\n    return codecs.charmap_encode(input,self.errors,encoding_table)[0]\r\n           ^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^\r\nUnicodeEncodeError: \'charmap\' codec can\'t encode character \'\\U0001f525\' in position 58: character maps to <undefined>\r\n')
```


## log_console

```text
156/319 Testing: log_console
156/319 Test: log_console
Command: "C:/hostedtoolcache/windows/Python/3.12.10/x64/python3.exe" "D:/a/Void.Language/Void.Language/tests/log/console.py" "D:/a/Void.Language/Void.Language/build/Debug/voidc.exe"
Directory: D:/a/Void.Language/Void.Language/build/Debug
"log_console" start time: Oct 29 11:36 Coordinated Universal Time
Output:
----------------------------------------------------------
CRASH EVIDENCE: D:\a\Void.Language\Void.Language\docs\log\evidence\crashes\incident-n__jvtfe
Traceback (most recent call last):
  File "D:\a\Void.Language\Void.Language\tests\text_encoding\crash.py", line 254, in run
    stdout, stderr = process.communicate(supplied, timeout=timeout)
                     ^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^
  File "C:\hostedtoolcache\windows\Python\3.12.10\x64\Lib\subprocess.py", line 1209, in communicate
    stdout, stderr = self._communicate(input, endtime, timeout)
                     ^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^
  File "C:\hostedtoolcache\windows\Python\3.12.10\x64\Lib\subprocess.py", line 1634, in _communicate
    raise TimeoutExpired(self.args, orig_timeout)
subprocess.TimeoutExpired: Command '['D:\\a\\Void.Language\\Void.Language\\build\\Debug\\voidc.exe', 'run', 'C:\\Users\\RUNNER~1\\AppData\\Local\\Temp\\VOID-log-console-xl17nrgg\\main.void', '--no-cache', '--no-opt']' timed out after 600 seconds

During handling of the above exception, another exception occurred:

Traceback (most recent call last):
  File "D:\a\Void.Language\Void.Language\tests\log\console.py", line 59, in <module>
    r=h.run(command,cwd=cwd,small=True,capture_output=False,stdout=writer,stderr=subprocess.PIPE,sources=[source])
      ^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^
  File "D:\a\Void.Language\Void.Language\tests\log\support.py", line 57, in run
    result = crash.run(args, cwd=working, capture_output=capture, timeout=timeout,
             ^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^
  File "D:\a\Void.Language\Void.Language\tests\text_encoding\crash.py", line 263, in run
    raise subprocess.TimeoutExpired(command, timeout, output=stdout, stderr=stderr)
subprocess.TimeoutExpired: Command '['D:\\a\\Void.Language\\Void.Language\\build\\Debug\\voidc.exe', 'run', 'C:\\Users\\RUNNER~1\\AppData\\Local\\Temp\\VOID-log-console-xl17nrgg\\main.void', '--no-cache', '--no-opt']' timed out after 600 seconds
```


## lsp_log

```text
157/319 Testing: lsp_log
157/319 Test: lsp_log
Command: "C:/hostedtoolcache/windows/Python/3.12.10/x64/python3.exe" "D:/a/Void.Language/Void.Language/tests/log/lsp.py" "D:/a/Void.Language/Void.Language/build/Debug/voidls.exe"
Directory: D:/a/Void.Language/Void.Language/build/Debug
"lsp_log" start time: Oct 29 11:46 Coordinated Universal Time
Output:
----------------------------------------------------------
Traceback (most recent call last):
  File "D:\a\Void.Language\Void.Language\tests\log\lsp.py", line 30, in <module>
    entry.write_text(base)
  File "C:\hostedtoolcache\windows\Python\3.12.10\x64\Lib\pathlib.py", line 1048, in write_text
    return f.write(data)
           ^^^^^^^^^^^^^
  File "C:\hostedtoolcache\windows\Python\3.12.10\x64\Lib\encodings\cp1252.py", line 19, in encode
    return codecs.charmap_encode(input,self.errors,encoding_table)[0]
           ^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^
UnicodeEncodeError: 'charmap' codec can't encode character '\U0001f525' in position 58: character maps to <undefined>
```


## collections_stdlib_host

```text
160/319 Testing: collections_stdlib_host
160/319 Test: collections_stdlib_host
Command: "C:/hostedtoolcache/windows/Python/3.12.10/x64/python3.exe" "D:/a/Void.Language/Void.Language/tests/collections_stdlib/host.py" "D:/a/Void.Language/Void.Language/build/Debug/void_collections_stdlib_host.exe"
Directory: D:/a/Void.Language/Void.Language/build/Debug
"collections_stdlib_host" start time: Oct 29 11:46 Coordinated Universal Time
Output:
----------------------------------------------------------
CRASH EVIDENCE: D:\a\Void.Language\Void.Language\docs\collections\evidence\crashes\incident-95veesv2
Traceback (most recent call last):
  File "D:\a\Void.Language\Void.Language\tests\collections_stdlib\host.py", line 8, in <module>
    result = h.run([exe, h.ROOT, directory, *sys.argv[2:]], sources=[h.ROOT / 'tests/collections_stdlib', Path(directory)], timeout=900,
             ^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^
  File "D:\a\Void.Language\Void.Language\tests\collections_stdlib\support.py", line 70, in run
    assert not result.expectation_failed, (receipt, args, result.returncode, result.stdout, result.stderr.decode(errors='replace'))
           ^^^^^^^^^^^^^^^^^^^^^^^^^^^^^
AssertionError: (WindowsPath('D:/a/Void.Language/Void.Language/docs/collections/evidence/run-34bffj7c'), ['D:\\a\\Void.Language\\Void.Language\\build\\Debug\\void_collections_stdlib_host.exe', 'D:\\a\\Void.Language\\Void.Language', 'C:\\Users\\RUNNER~1\\AppData\\Local\\Temp\\VOID-collections-host-8u2dgsti'], 1, b'operation=1 optimized=0 injected_allocation_failures=454\r\noperation=2 optimized=0 injected_allocation_failures=76\r\noperation=3 optimized=0 injected_allocation_failures=107\r\noperation=4 optimized=0 injected_allocation_failures=335\r\noperation=5 optimized=0 injected_allocation_failures=368\r\noperation=6 optimized=0 injected_allocation_failures=318\r\noperation=7 optimized=0 injected_allocation_failures=444\r\noperation=8 optimized=0 injected_allocation_failures=116\r\nmap publication optimized=0 partial_effect_failures=77 passed\r\n', 'collections cache did not miss\r\n')
```


## collections_stdlib_execution

```text
161/319 Testing: collections_stdlib_execution
161/319 Test: collections_stdlib_execution
Command: "C:/hostedtoolcache/windows/Python/3.12.10/x64/python3.exe" "D:/a/Void.Language/Void.Language/tests/collections_stdlib/execution.py" "D:/a/Void.Language/Void.Language/build/Debug/voidc.exe"
Directory: D:/a/Void.Language/Void.Language/build/Debug
"collections_stdlib_execution" start time: Oct 29 11:47 Coordinated Universal Time
Output:
----------------------------------------------------------
CRASH EVIDENCE: D:\a\Void.Language\Void.Language\docs\collections\evidence\crashes\incident-pwmrduiq
Traceback (most recent call last):
  File "D:\a\Void.Language\Void.Language\tests\collections_stdlib\execution.py", line 22, in <module>
    h.run([compiler, 'run', source, *arguments, '--no-cache', *switches], expected=expected, small=small, context=context)
  File "D:\a\Void.Language\Void.Language\tests\collections_stdlib\support.py", line 70, in run
    assert not result.expectation_failed, (receipt, args, result.returncode, result.stdout, result.stderr.decode(errors='replace'))
           ^^^^^^^^^^^^^^^^^^^^^^^^^^^^^
AssertionError: (WindowsPath('D:/a/Void.Language/Void.Language/docs/collections/evidence/run-olang9v1'), ['D:\\a\\Void.Language\\Void.Language\\build\\Debug\\voidc.exe', 'run', 'D:\\a\\Void.Language\\Void.Language\\tests\\collections_stdlib\\semantics.void', '7', '--no-cache', '--no-opt'], 0, b'7\r\n', '')
```


## collections_stdlib_failures

```text
162/319 Testing: collections_stdlib_failures
162/319 Test: collections_stdlib_failures
Command: "C:/hostedtoolcache/windows/Python/3.12.10/x64/python3.exe" "D:/a/Void.Language/Void.Language/tests/collections_stdlib/failures.py" "D:/a/Void.Language/Void.Language/build/Debug/voidc.exe"
Directory: D:/a/Void.Language/Void.Language/build/Debug
"collections_stdlib_failures" start time: Oct 29 11:47 Coordinated Universal Time
Output:
----------------------------------------------------------
CRASH EVIDENCE: D:\a\Void.Language\Void.Language\docs\collections\evidence\crashes\incident-xc178hoo
Traceback (most recent call last):
  File "D:\a\Void.Language\Void.Language\tests\collections_stdlib\failures.py", line 25, in <module>
    h.run(arguments, expected=b'7\n', sources=[source], small=True)
  File "D:\a\Void.Language\Void.Language\tests\collections_stdlib\support.py", line 70, in run
    assert not result.expectation_failed, (receipt, args, result.returncode, result.stdout, result.stderr.decode(errors='replace'))
           ^^^^^^^^^^^^^^^^^^^^^^^^^^^^^
AssertionError: (WindowsPath('D:/a/Void.Language/Void.Language/docs/collections/evidence/run-63p7ctqd'), ['D:\\a\\Void.Language\\Void.Language\\build\\Debug\\voidc.exe', 'run', 'D:\\a\\Void.Language\\Void.Language\\tests\\collections_stdlib\\mutation.void', '7', '--no-cache'], 0, b'7\r\n', '')
```


## lsp_collections_stdlib

```text
164/319 Testing: lsp_collections_stdlib
164/319 Test: lsp_collections_stdlib
Command: "C:/hostedtoolcache/windows/Python/3.12.10/x64/python3.exe" "D:/a/Void.Language/Void.Language/tests/collections_stdlib/lsp.py" "D:/a/Void.Language/Void.Language/build/Debug/voidls.exe"
Directory: D:/a/Void.Language/Void.Language/build/Debug
"lsp_collections_stdlib" start time: Oct 29 11:48 Coordinated Universal Time
Output:
----------------------------------------------------------
Traceback (most recent call last):
  File "D:\a\Void.Language\Void.Language\tests\collections_stdlib\lsp.py", line 28, in <module>
    entry.write_text(base)
  File "C:\hostedtoolcache\windows\Python\3.12.10\x64\Lib\pathlib.py", line 1048, in write_text
    return f.write(data)
           ^^^^^^^^^^^^^
  File "C:\hostedtoolcache\windows\Python\3.12.10\x64\Lib\encodings\cp1252.py", line 19, in encode
    return codecs.charmap_encode(input,self.errors,encoding_table)[0]
           ^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^
UnicodeEncodeError: 'charmap' codec can't encode character '\U0001f525' in position 53: character maps to <undefined>
```


## collections_stdlib_install

```text
166/319 Testing: collections_stdlib_install
166/319 Test: collections_stdlib_install
Command: "C:/hostedtoolcache/windows/Python/3.12.10/x64/python3.exe" "D:/a/Void.Language/Void.Language/tests/collections_stdlib/install.py" "D:/a/Void.Language/Void.Language/build/Debug/voidc.exe"
Directory: D:/a/Void.Language/Void.Language/build/Debug
"collections_stdlib_install" start time: Oct 29 11:48 Coordinated Universal Time
Output:
----------------------------------------------------------
CRASH EVIDENCE: D:\a\Void.Language\Void.Language\docs\collections\evidence\crashes\incident-zffm1ofa
Traceback (most recent call last):
  File "D:\a\Void.Language\Void.Language\tests\collections_stdlib\install.py", line 32, in <module>
    h.run([sys.executable, Path(__file__).with_name('lsp.py'), lsp], cwd=work, env=env, sources=[source], context={'mode': 'installed-lsp'})
  File "D:\a\Void.Language\Void.Language\tests\collections_stdlib\support.py", line 70, in run
    assert not result.expectation_failed, (receipt, args, result.returncode, result.stdout, result.stderr.decode(errors='replace'))
           ^^^^^^^^^^^^^^^^^^^^^^^^^^^^^
AssertionError: (WindowsPath('D:/a/Void.Language/Void.Language/docs/collections/evidence/run-rpzbgi6c'), ['C:\\hostedtoolcache\\windows\\Python\\3.12.10\\x64\\python3.exe', 'D:\\a\\Void.Language\\Void.Language\\tests\\collections_stdlib\\lsp.py', 'C:\\Users\\RUNNER~1\\AppData\\Local\\Temp\\VOID-collections-installed-x3uk03so\\relocated \u96ea # %\\bin\\voidls.exe'], 1, b'', 'Traceback (most recent call last):\r\n  File "D:\\a\\Void.Language\\Void.Language\\tests\\collections_stdlib\\lsp.py", line 28, in <module>\r\n    entry.write_text(base)\r\n  File "C:\\hostedtoolcache\\windows\\Python\\3.12.10\\x64\\Lib\\pathlib.py", line 1048, in write_text\r\n    return f.write(data)\r\n           ^^^^^^^^^^^^^\r\n  File "C:\\hostedtoolcache\\windows\\Python\\3.12.10\\x64\\Lib\\encodings\\cp1252.py", line 19, in encode\r\n    return codecs.charmap_encode(input,self.errors,encoding_table)[0]\r\n           ^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^\r\nUnicodeEncodeError: \'charmap\' codec can\'t encode character \'\\U0001f525\' in position 53: character maps to <undefined>\r\n')
```


## collections_stdlib_cache

```text
167/319 Testing: collections_stdlib_cache
167/319 Test: collections_stdlib_cache
Command: "C:/hostedtoolcache/windows/Python/3.12.10/x64/python3.exe" "D:/a/Void.Language/Void.Language/tests/collections_stdlib/cache.py" "D:/a/Void.Language/Void.Language/build/Debug/voidc.exe"
Directory: D:/a/Void.Language/Void.Language/build/Debug
"collections_stdlib_cache" start time: Oct 29 11:48 Coordinated Universal Time
Output:
----------------------------------------------------------
CRASH EVIDENCE: D:\a\Void.Language\Void.Language\docs\collections\evidence\crashes\incident-cdf2q5uq
Traceback (most recent call last):
  File "D:\a\Void.Language\Void.Language\tests\collections_stdlib\cache.py", line 16, in <module>
    assert len(artifacts - before) == 1, 'automatic cache did not publish once'
           ^^^^^^^^^^^^^^^^^^^^^^^^^^^^
AssertionError: automatic cache did not publish once
```


## channel_host

```text
169/319 Testing: channel_host
169/319 Test: channel_host
Command: "C:/hostedtoolcache/windows/Python/3.12.10/x64/python3.exe" "D:/a/Void.Language/Void.Language/tests/channel/host.py" "D:/a/Void.Language/Void.Language/build/Debug/void_channel_host.exe" "D:/a/Void.Language/Void.Language" "D:/a/Void.Language/Void.Language/build/Debug/channel-host"
Directory: D:/a/Void.Language/Void.Language/build/Debug
"channel_host" start time: Oct 29 11:48 Coordinated Universal Time
Output:
----------------------------------------------------------
subscribe O0 once=0 path=0 failures=557 released_markers=539
subscribe O0 once=0 path=1 failures=230 released_markers=206
subscribe O0 once=0 path=2 failures=454 released_markers=419
subscribe O0 once=0 path=3 failures=383 released_markers=359
unsubscribe O0 once=0 survivor=0 before=31 after=36
unsubscribe O0 once=0 survivor=1 before=37 after=36
subscribe O0 once=1 path=0 failures=557 released_markers=539
subscribe O0 once=1 path=1 failures=230 released_markers=206
subscribe O0 once=1 path=2 failures=454 released_markers=419
subscribe O0 once=1 path=3 failures=383 released_markers=359
unsubscribe O0 once=1 survivor=0 before=31 after=36
unsubscribe O0 once=1 survivor=1 before=37 after=36
emit O0 no_delivery_failures=277 partial_failures=90
subscribe O2 once=0 path=0 failures=557 released_markers=539
subscribe O2 once=0 path=1 failures=230 released_markers=206
subscribe O2 once=0 path=2 failures=454 released_markers=419
subscribe O2 once=0 path=3 failures=383 released_markers=359
unsubscribe O2 once=0 survivor=0 before=31 after=36
unsubscribe O2 once=0 survivor=1 before=37 after=36
subscribe O2 once=1 path=0 failures=557 released_markers=539
subscribe O2 once=1 path=1 failures=230 released_markers=206
subscribe O2 once=1 path=2 failures=454 released_markers=419
subscribe O2 once=1 path=3 failures=383 released_markers=359
unsubscribe O2 once=1 survivor=0 before=31 after=36
unsubscribe O2 once=1 survivor=1 before=37 after=36
emit O2 no_delivery_failures=277 partial_failures=90
62036 channel host checks passed
Traceback (most recent call last):
  File "D:\a\Void.Language\Void.Language\tests\channel\host.py", line 12, in <module>
    h.run([native, '7'], expected=b'7\n', small=sys.platform != 'win32')
  File "D:\a\Void.Language\Void.Language\tests\channel\support.py", line 73, in run
    assert result.stdout == expected, (receipt, result.stdout, expected)
           ^^^^^^^^^^^^^^^^^^^^^^^^^
AssertionError: (WindowsPath('D:/a/Void.Language/Void.Language/docs/channel/evidence/implementation/run-utoh759v'), b'7\r\n', b'7\n')
```


## channel_execution

```text
171/319 Testing: channel_execution
171/319 Test: channel_execution
Command: "C:/hostedtoolcache/windows/Python/3.12.10/x64/python3.exe" "D:/a/Void.Language/Void.Language/tests/channel/execution.py" "D:/a/Void.Language/Void.Language/build/Debug/voidc.exe"
Directory: D:/a/Void.Language/Void.Language/build/Debug
"channel_execution" start time: Oct 29 11:49 Coordinated Universal Time
Output:
----------------------------------------------------------
Traceback (most recent call last):
  File "D:\a\Void.Language\Void.Language\tests\channel\execution.py", line 34, in <module>
    arguments = ['7'] if 'func entry(' in source.read_text() else []
                                          ^^^^^^^^^^^^^^^^^^
  File "C:\hostedtoolcache\windows\Python\3.12.10\x64\Lib\pathlib.py", line 1028, in read_text
    return f.read()
           ^^^^^^^^
  File "C:\hostedtoolcache\windows\Python\3.12.10\x64\Lib\encodings\cp1252.py", line 23, in decode
    return codecs.charmap_decode(input,self.errors,decoding_table)[0]
           ^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^
UnicodeDecodeError: 'charmap' codec can't decode byte 0x81 in position 1359: character maps to <undefined>
```


## io_execution

```text
196/319 Testing: io_execution
196/319 Test: io_execution
Command: "C:/hostedtoolcache/windows/Python/3.12.10/x64/python3.exe" "D:/a/Void.Language/Void.Language/tests/io/execution.py" "D:/a/Void.Language/Void.Language/build/Debug/voidc.exe"
Directory: D:/a/Void.Language/Void.Language/build/Debug
"io_execution" start time: Oct 29 11:52 Coordinated Universal Time
Output:
----------------------------------------------------------
Traceback (most recent call last):
  File "D:\a\Void.Language\Void.Language\tests\io\execution.py", line 267, in <module>
    main()
  File "D:\a\Void.Language\Void.Language\tests\io\execution.py", line 230, in main
    run([compiler, 'check', fixture], root, expected=b'Source checked successfully\n',
  File "D:\a\Void.Language\Void.Language\tests\io\execution.py", line 36, in run
    assert result.stdout == expected, (arguments, result.stdout, expected, result.stderr)
           ^^^^^^^^^^^^^^^^^^^^^^^^^
AssertionError: ([WindowsPath('D:/a/Void.Language/Void.Language/build/Debug/voidc.exe'), 'check', WindowsPath('C:/Users/RUNNER~1/AppData/Local/Temp/VOID io source 9q_9y27q/main.void')], b'Source checked successfully\r\n', b'Source checked successfully\n', b'')
```


## io_crash_preservation

```text
197/319 Testing: io_crash_preservation
197/319 Test: io_crash_preservation
Command: "C:/hostedtoolcache/windows/Python/3.12.10/x64/python3.exe" "D:/a/Void.Language/Void.Language/tests/io/preservation.py"
Directory: D:/a/Void.Language/Void.Language/build/Debug
"io_crash_preservation" start time: Oct 29 11:52 Coordinated Universal Time
Output:
----------------------------------------------------------
CRASH EVIDENCE: C:\Users\RUNNER~1\AppData\Local\Temp\VOID disposable io preservation ens_32ws\incidents\incident-d9s7ti0z
Traceback (most recent call last):
  File "D:\a\Void.Language\Void.Language\tests\io\preservation.py", line 128, in <module>
    main()
  File "D:\a\Void.Language\Void.Language\tests\io\preservation.py", line 95, in main
    assert record['cwd'] == str(working)
           ^^^^^^^^^^^^^^^^^^^^^^^^^^^^^
AssertionError
```


## io_async_execution

```text
198/319 Testing: io_async_execution
198/319 Test: io_async_execution
Command: "C:/hostedtoolcache/windows/Python/3.12.10/x64/python3.exe" "D:/a/Void.Language/Void.Language/tests/io/execution.py" "D:/a/Void.Language/Void.Language/build/Debug/voidc.exe" "D:/a/Void.Language/Void.Language/build/Debug/void_io_runner.exe" "--async"
Directory: D:/a/Void.Language/Void.Language/build/Debug
"io_async_execution" start time: Oct 29 11:52 Coordinated Universal Time
Output:
----------------------------------------------------------
Traceback (most recent call last):
  File "D:\a\Void.Language\Void.Language\tests\io\execution.py", line 267, in <module>
    main()
  File "D:\a\Void.Language\Void.Language\tests\io\execution.py", line 217, in main
    run([compiler, 'check', fixture], root, expected=b'Source checked successfully\n',
  File "D:\a\Void.Language\Void.Language\tests\io\execution.py", line 36, in run
    assert result.stdout == expected, (arguments, result.stdout, expected, result.stderr)
           ^^^^^^^^^^^^^^^^^^^^^^^^^
AssertionError: ([WindowsPath('D:/a/Void.Language/Void.Language/build/Debug/voidc.exe'), 'check', WindowsPath('C:/Users/RUNNER~1/AppData/Local/Temp/VOID io async wire jv1k_c4p/main.void')], b'Source checked successfully\r\n', b'Source checked successfully\n', b'')
```


## io_examples_install

```text
214/319 Testing: io_examples_install
214/319 Test: io_examples_install
Command: "C:/hostedtoolcache/windows/Python/3.12.10/x64/python3.exe" "D:/a/Void.Language/Void.Language/tests/io/examples.py" "D:/a/Void.Language/Void.Language/build/Debug/voidc.exe" "D:/a/Void.Language/Void.Language/build/Debug" "D:/a/Void.Language/Void.Language/examples/io/filesystem.void" "Debug"
Directory: D:/a/Void.Language/Void.Language/build/Debug
"io_examples_install" start time: Oct 29 11:53 Coordinated Universal Time
Output:
----------------------------------------------------------
Traceback (most recent call last):
  File "D:\a\Void.Language\Void.Language\tests\io\examples.py", line 40, in <module>
    run(arguments, cwd)
  File "D:\a\Void.Language\Void.Language\tests\io\examples.py", line 19, in run
    assert b'warning' not in result.stderr.lower(), result.stderr
           ^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^
AssertionError: b"clang: warning: argument unused during compilation: '-pthread' [-Wunused-command-line-argument]\r\n"
```


## async_syntax_aot_values_normal

```text
261/319 Testing: async_syntax_aot_values_normal
261/319 Test: async_syntax_aot_values_normal
Command: "C:/hostedtoolcache/windows/Python/3.12.10/x64/python3.exe" "D:/a/Void.Language/Void.Language/tests/async/syntax_aot.py" "D:/a/Void.Language/Void.Language/build/Debug/voidc.exe" "D:/a/Void.Language/Void.Language/build/Debug/void_async_syntax_values_normal.exe" "D:/a/Void.Language/Void.Language/tests/async/syntax/values.void" "130" "D:/a/Void.Language/Void.Language/build/Debug/void_async_tests.exe" "normal" "D:/a/Void.Language/Void.Language/build/Debug/async-syntax-values-normal.obj"
Directory: D:/a/Void.Language/Void.Language/build/Debug
"async_syntax_aot_values_normal" start time: Oct 29 11:55 Coordinated Universal Time
Output:
----------------------------------------------------------
Traceback (most recent call last):
  File "D:\a\Void.Language\Void.Language\tests\async\syntax_aot.py", line 12, in <module>
    h.run([compiler, 'check', source], ROOT, expected=b'Source checked successfully\n', sources=[source], small=small)
  File "D:\a\Void.Language\Void.Language\tests\random\harness.py", line 29, in run
    assert result.stdout == expected, (args, result.stdout[:4000], expected[:4000], result.stderr)
           ^^^^^^^^^^^^^^^^^^^^^^^^^
AssertionError: (['D:\\a\\Void.Language\\Void.Language\\build\\Debug\\voidc.exe', 'check', 'D:\\a\\Void.Language\\Void.Language\\tests\\async\\syntax\\values.void'], b'Source checked successfully\r\n', b'Source checked successfully\n', b'')
```


## async_syntax_aot_values_optimized

```text
262/319 Testing: async_syntax_aot_values_optimized
262/319 Test: async_syntax_aot_values_optimized
Command: "C:/hostedtoolcache/windows/Python/3.12.10/x64/python3.exe" "D:/a/Void.Language/Void.Language/tests/async/syntax_aot.py" "D:/a/Void.Language/Void.Language/build/Debug/voidc.exe" "D:/a/Void.Language/Void.Language/build/Debug/void_async_syntax_values_optimized.exe" "D:/a/Void.Language/Void.Language/tests/async/syntax/values.void" "130" "D:/a/Void.Language/Void.Language/build/Debug/void_async_tests.exe" "optimized" "D:/a/Void.Language/Void.Language/build/Debug/async-syntax-values-optimized.obj"
Directory: D:/a/Void.Language/Void.Language/build/Debug
"async_syntax_aot_values_optimized" start time: Oct 29 11:55 Coordinated Universal Time
Output:
----------------------------------------------------------
Traceback (most recent call last):
  File "D:\a\Void.Language\Void.Language\tests\async\syntax_aot.py", line 12, in <module>
    h.run([compiler, 'check', source], ROOT, expected=b'Source checked successfully\n', sources=[source], small=small)
  File "D:\a\Void.Language\Void.Language\tests\random\harness.py", line 29, in run
    assert result.stdout == expected, (args, result.stdout[:4000], expected[:4000], result.stderr)
           ^^^^^^^^^^^^^^^^^^^^^^^^^
AssertionError: (['D:\\a\\Void.Language\\Void.Language\\build\\Debug\\voidc.exe', 'check', 'D:\\a\\Void.Language\\Void.Language\\tests\\async\\syntax\\values.void'], b'Source checked successfully\r\n', b'Source checked successfully\n', b'')
```


## async_syntax_aot_errors_normal

```text
263/319 Testing: async_syntax_aot_errors_normal
263/319 Test: async_syntax_aot_errors_normal
Command: "C:/hostedtoolcache/windows/Python/3.12.10/x64/python3.exe" "D:/a/Void.Language/Void.Language/tests/async/syntax_aot.py" "D:/a/Void.Language/Void.Language/build/Debug/voidc.exe" "D:/a/Void.Language/Void.Language/build/Debug/void_async_syntax_errors_normal.exe" "D:/a/Void.Language/Void.Language/tests/async/syntax/errors.void" "7" "D:/a/Void.Language/Void.Language/build/Debug/void_async_tests.exe" "normal" "D:/a/Void.Language/Void.Language/build/Debug/async-syntax-errors-normal.obj"
Directory: D:/a/Void.Language/Void.Language/build/Debug
"async_syntax_aot_errors_normal" start time: Oct 29 11:55 Coordinated Universal Time
Output:
----------------------------------------------------------
Traceback (most recent call last):
  File "D:\a\Void.Language\Void.Language\tests\async\syntax_aot.py", line 12, in <module>
    h.run([compiler, 'check', source], ROOT, expected=b'Source checked successfully\n', sources=[source], small=small)
  File "D:\a\Void.Language\Void.Language\tests\random\harness.py", line 29, in run
    assert result.stdout == expected, (args, result.stdout[:4000], expected[:4000], result.stderr)
           ^^^^^^^^^^^^^^^^^^^^^^^^^
AssertionError: (['D:\\a\\Void.Language\\Void.Language\\build\\Debug\\voidc.exe', 'check', 'D:\\a\\Void.Language\\Void.Language\\tests\\async\\syntax\\errors.void'], b'Source checked successfully\r\n', b'Source checked successfully\n', b'')
```


## async_syntax_aot_errors_optimized

```text
264/319 Testing: async_syntax_aot_errors_optimized
264/319 Test: async_syntax_aot_errors_optimized
Command: "C:/hostedtoolcache/windows/Python/3.12.10/x64/python3.exe" "D:/a/Void.Language/Void.Language/tests/async/syntax_aot.py" "D:/a/Void.Language/Void.Language/build/Debug/voidc.exe" "D:/a/Void.Language/Void.Language/build/Debug/void_async_syntax_errors_optimized.exe" "D:/a/Void.Language/Void.Language/tests/async/syntax/errors.void" "7" "D:/a/Void.Language/Void.Language/build/Debug/void_async_tests.exe" "optimized" "D:/a/Void.Language/Void.Language/build/Debug/async-syntax-errors-optimized.obj"
Directory: D:/a/Void.Language/Void.Language/build/Debug
"async_syntax_aot_errors_optimized" start time: Oct 29 11:55 Coordinated Universal Time
Output:
----------------------------------------------------------
Traceback (most recent call last):
  File "D:\a\Void.Language\Void.Language\tests\async\syntax_aot.py", line 12, in <module>
    h.run([compiler, 'check', source], ROOT, expected=b'Source checked successfully\n', sources=[source], small=small)
  File "D:\a\Void.Language\Void.Language\tests\random\harness.py", line 29, in run
    assert result.stdout == expected, (args, result.stdout[:4000], expected[:4000], result.stderr)
           ^^^^^^^^^^^^^^^^^^^^^^^^^
AssertionError: (['D:\\a\\Void.Language\\Void.Language\\build\\Debug\\voidc.exe', 'check', 'D:\\a\\Void.Language\\Void.Language\\tests\\async\\syntax\\errors.void'], b'Source checked successfully\r\n', b'Source checked successfully\n', b'')
```


## async_syntax_aot_cancelled_normal

```text
265/319 Testing: async_syntax_aot_cancelled_normal
265/319 Test: async_syntax_aot_cancelled_normal
Command: "C:/hostedtoolcache/windows/Python/3.12.10/x64/python3.exe" "D:/a/Void.Language/Void.Language/tests/async/syntax_aot.py" "D:/a/Void.Language/Void.Language/build/Debug/voidc.exe" "D:/a/Void.Language/Void.Language/build/Debug/void_async_syntax_cancelled_normal.exe" "D:/a/Void.Language/Void.Language/tests/async/syntax/cancelled.void" "failure:Async task cancelled" "D:/a/Void.Language/Void.Language/build/Debug/void_async_tests.exe" "normal" "D:/a/Void.Language/Void.Language/build/Debug/async-syntax-cancelled-normal.obj"
Directory: D:/a/Void.Language/Void.Language/build/Debug
"async_syntax_aot_cancelled_normal" start time: Oct 29 11:55 Coordinated Universal Time
Output:
----------------------------------------------------------
Traceback (most recent call last):
  File "D:\a\Void.Language\Void.Language\tests\async\syntax_aot.py", line 12, in <module>
    h.run([compiler, 'check', source], ROOT, expected=b'Source checked successfully\n', sources=[source], small=small)
  File "D:\a\Void.Language\Void.Language\tests\random\harness.py", line 29, in run
    assert result.stdout == expected, (args, result.stdout[:4000], expected[:4000], result.stderr)
           ^^^^^^^^^^^^^^^^^^^^^^^^^
AssertionError: (['D:\\a\\Void.Language\\Void.Language\\build\\Debug\\voidc.exe', 'check', 'D:\\a\\Void.Language\\Void.Language\\tests\\async\\syntax\\cancelled.void'], b'Source checked successfully\r\n', b'Source checked successfully\n', b'')
```


## async_syntax_aot_cancelled_optimized

```text
266/319 Testing: async_syntax_aot_cancelled_optimized
266/319 Test: async_syntax_aot_cancelled_optimized
Command: "C:/hostedtoolcache/windows/Python/3.12.10/x64/python3.exe" "D:/a/Void.Language/Void.Language/tests/async/syntax_aot.py" "D:/a/Void.Language/Void.Language/build/Debug/voidc.exe" "D:/a/Void.Language/Void.Language/build/Debug/void_async_syntax_cancelled_optimized.exe" "D:/a/Void.Language/Void.Language/tests/async/syntax/cancelled.void" "failure:Async task cancelled" "D:/a/Void.Language/Void.Language/build/Debug/void_async_tests.exe" "optimized" "D:/a/Void.Language/Void.Language/build/Debug/async-syntax-cancelled-optimized.obj"
Directory: D:/a/Void.Language/Void.Language/build/Debug
"async_syntax_aot_cancelled_optimized" start time: Oct 29 11:55 Coordinated Universal Time
Output:
----------------------------------------------------------
Traceback (most recent call last):
  File "D:\a\Void.Language\Void.Language\tests\async\syntax_aot.py", line 12, in <module>
    h.run([compiler, 'check', source], ROOT, expected=b'Source checked successfully\n', sources=[source], small=small)
  File "D:\a\Void.Language\Void.Language\tests\random\harness.py", line 29, in run
    assert result.stdout == expected, (args, result.stdout[:4000], expected[:4000], result.stderr)
           ^^^^^^^^^^^^^^^^^^^^^^^^^
AssertionError: (['D:\\a\\Void.Language\\Void.Language\\build\\Debug\\voidc.exe', 'check', 'D:\\a\\Void.Language\\Void.Language\\tests\\async\\syntax\\cancelled.void'], b'Source checked successfully\r\n', b'Source checked successfully\n', b'')
```


## async_syntax_aot_failure_normal

```text
267/319 Testing: async_syntax_aot_failure_normal
267/319 Test: async_syntax_aot_failure_normal
Command: "C:/hostedtoolcache/windows/Python/3.12.10/x64/python3.exe" "D:/a/Void.Language/Void.Language/tests/async/syntax_aot.py" "D:/a/Void.Language/Void.Language/build/Debug/voidc.exe" "D:/a/Void.Language/Void.Language/build/Debug/void_async_syntax_failure_normal.exe" "D:/a/Void.Language/Void.Language/tests/async/syntax/failure.void" "failure:Division by zero" "D:/a/Void.Language/Void.Language/build/Debug/void_async_tests.exe" "normal" "D:/a/Void.Language/Void.Language/build/Debug/async-syntax-failure-normal.obj"
Directory: D:/a/Void.Language/Void.Language/build/Debug
"async_syntax_aot_failure_normal" start time: Oct 29 11:55 Coordinated Universal Time
Output:
----------------------------------------------------------
Traceback (most recent call last):
  File "D:\a\Void.Language\Void.Language\tests\async\syntax_aot.py", line 12, in <module>
    h.run([compiler, 'check', source], ROOT, expected=b'Source checked successfully\n', sources=[source], small=small)
  File "D:\a\Void.Language\Void.Language\tests\random\harness.py", line 29, in run
    assert result.stdout == expected, (args, result.stdout[:4000], expected[:4000], result.stderr)
           ^^^^^^^^^^^^^^^^^^^^^^^^^
AssertionError: (['D:\\a\\Void.Language\\Void.Language\\build\\Debug\\voidc.exe', 'check', 'D:\\a\\Void.Language\\Void.Language\\tests\\async\\syntax\\failure.void'], b'Source checked successfully\r\n', b'Source checked successfully\n', b'')
```


## async_syntax_aot_failure_optimized

```text
268/319 Testing: async_syntax_aot_failure_optimized
268/319 Test: async_syntax_aot_failure_optimized
Command: "C:/hostedtoolcache/windows/Python/3.12.10/x64/python3.exe" "D:/a/Void.Language/Void.Language/tests/async/syntax_aot.py" "D:/a/Void.Language/Void.Language/build/Debug/voidc.exe" "D:/a/Void.Language/Void.Language/build/Debug/void_async_syntax_failure_optimized.exe" "D:/a/Void.Language/Void.Language/tests/async/syntax/failure.void" "failure:Division by zero" "D:/a/Void.Language/Void.Language/build/Debug/void_async_tests.exe" "optimized" "D:/a/Void.Language/Void.Language/build/Debug/async-syntax-failure-optimized.obj"
Directory: D:/a/Void.Language/Void.Language/build/Debug
"async_syntax_aot_failure_optimized" start time: Oct 29 11:55 Coordinated Universal Time
Output:
----------------------------------------------------------
Traceback (most recent call last):
  File "D:\a\Void.Language\Void.Language\tests\async\syntax_aot.py", line 12, in <module>
    h.run([compiler, 'check', source], ROOT, expected=b'Source checked successfully\n', sources=[source], small=small)
  File "D:\a\Void.Language\Void.Language\tests\random\harness.py", line 29, in run
    assert result.stdout == expected, (args, result.stdout[:4000], expected[:4000], result.stderr)
           ^^^^^^^^^^^^^^^^^^^^^^^^^
AssertionError: (['D:\\a\\Void.Language\\Void.Language\\build\\Debug\\voidc.exe', 'check', 'D:\\a\\Void.Language\\Void.Language\\tests\\async\\syntax\\failure.void'], b'Source checked successfully\r\n', b'Source checked successfully\n', b'')
```


## async_syntax_aot_spawn_normal

```text
269/319 Testing: async_syntax_aot_spawn_normal
269/319 Test: async_syntax_aot_spawn_normal
Command: "C:/hostedtoolcache/windows/Python/3.12.10/x64/python3.exe" "D:/a/Void.Language/Void.Language/tests/async/syntax_aot.py" "D:/a/Void.Language/Void.Language/build/Debug/voidc.exe" "D:/a/Void.Language/Void.Language/build/Debug/void_async_syntax_spawn_normal.exe" "D:/a/Void.Language/Void.Language/tests/async/syntax/spawn.void" "9" "D:/a/Void.Language/Void.Language/build/Debug/void_async_tests.exe" "normal" "D:/a/Void.Language/Void.Language/build/Debug/async-syntax-spawn-normal.obj"
Directory: D:/a/Void.Language/Void.Language/build/Debug
"async_syntax_aot_spawn_normal" start time: Oct 29 11:55 Coordinated Universal Time
Output:
----------------------------------------------------------
Traceback (most recent call last):
  File "D:\a\Void.Language\Void.Language\tests\async\syntax_aot.py", line 12, in <module>
    h.run([compiler, 'check', source], ROOT, expected=b'Source checked successfully\n', sources=[source], small=small)
  File "D:\a\Void.Language\Void.Language\tests\random\harness.py", line 29, in run
    assert result.stdout == expected, (args, result.stdout[:4000], expected[:4000], result.stderr)
           ^^^^^^^^^^^^^^^^^^^^^^^^^
AssertionError: (['D:\\a\\Void.Language\\Void.Language\\build\\Debug\\voidc.exe', 'check', 'D:\\a\\Void.Language\\Void.Language\\tests\\async\\syntax\\spawn.void'], b'Source checked successfully\r\n', b'Source checked successfully\n', b'')
```


## async_syntax_aot_spawn_optimized

```text
270/319 Testing: async_syntax_aot_spawn_optimized
270/319 Test: async_syntax_aot_spawn_optimized
Command: "C:/hostedtoolcache/windows/Python/3.12.10/x64/python3.exe" "D:/a/Void.Language/Void.Language/tests/async/syntax_aot.py" "D:/a/Void.Language/Void.Language/build/Debug/voidc.exe" "D:/a/Void.Language/Void.Language/build/Debug/void_async_syntax_spawn_optimized.exe" "D:/a/Void.Language/Void.Language/tests/async/syntax/spawn.void" "9" "D:/a/Void.Language/Void.Language/build/Debug/void_async_tests.exe" "optimized" "D:/a/Void.Language/Void.Language/build/Debug/async-syntax-spawn-optimized.obj"
Directory: D:/a/Void.Language/Void.Language/build/Debug
"async_syntax_aot_spawn_optimized" start time: Oct 29 11:55 Coordinated Universal Time
Output:
----------------------------------------------------------
Traceback (most recent call last):
  File "D:\a\Void.Language\Void.Language\tests\async\syntax_aot.py", line 12, in <module>
    h.run([compiler, 'check', source], ROOT, expected=b'Source checked successfully\n', sources=[source], small=small)
  File "D:\a\Void.Language\Void.Language\tests\random\harness.py", line 29, in run
    assert result.stdout == expected, (args, result.stdout[:4000], expected[:4000], result.stderr)
           ^^^^^^^^^^^^^^^^^^^^^^^^^
AssertionError: (['D:\\a\\Void.Language\\Void.Language\\build\\Debug\\voidc.exe', 'check', 'D:\\a\\Void.Language\\Void.Language\\tests\\async\\syntax\\spawn.void'], b'Source checked successfully\r\n', b'Source checked successfully\n', b'')
```
