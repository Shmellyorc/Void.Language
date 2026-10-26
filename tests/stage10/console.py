"""Compare actual source stdout/stderr bytes in JIT and independently linked AOT."""
import hashlib
import json
import os
from pathlib import Path
import shutil
import subprocess
import sys
import tempfile
import queue
import threading
import time

compiler, clang, linker, runtime, fixture, sanitizer = sys.argv[1:]
compiler, runtime, fixture = map(lambda x: str(Path(x).resolve()), (compiler, runtime, fixture))
fixtures = Path(__file__).resolve().parent
checks = 0
outputs = []
cache_statuses = []
integration_records = []
workloads = []
flags = ["--driver-mode=g++", "-pthread"]
if sys.platform.startswith("linux"):
    flags.append("-ldl")
if linker:
    flags.append("--ld-path=" + linker)
if sanitizer == "ON":
    flags.append("-fsanitize=address,undefined")
if os.name == "nt":
    flags += ["-fms-runtime-lib=dll", "-llegacy_stdio_definitions"]


def run(args, stdin=b"", status=0, env=None):
    global checks
    p = subprocess.run([str(a) for a in args], input=stdin, capture_output=True, timeout=120, env=env or environment)
    assert p.returncode == status, (args, p.returncode, p.stdout, p.stderr)
    checks += 1
    return p


with tempfile.TemporaryDirectory(prefix="VOID console source ") as directory:
    root = Path(directory)
    # Verify a stable standard-library snapshot; keep historical Step 37 observations.
    stdlib = root / "stdlib"
    shutil.copytree(fixtures.parents[1] / "stdlib", stdlib)
    hashes = {p.name: hashlib.sha256(p.read_bytes()).hexdigest() for p in stdlib.glob("*.void")}
    environment = dict(os.environ, VOID_STDLIB_ROOT=str(stdlib))
    for dependency in fixtures.glob("operator_*.void"):
        (root / dependency.name).write_bytes(dependency.read_bytes())
    packet = b"Ok(/operator_console_model.void::packet.data(v:q:002/9))"
    combined = (b"["+packet+b"]|{item: "+packet+b"}\n["+packet+b", "+packet+b"] [/operator_console_model.void::branch.nested(<cycle>)] Ok(2024-02-29)\n"
        b"account:010/010 q:008\n002|Ok(2024-02-29)|00:00:01:05.500\nOk(2026-11-01 01:30:00 -05:00 America/New_York)\n")
    combined_cleanup = b"done [10, 20, 30, 12, 10, 31, 40, 10, 10, 1, 2, 3, 50, 50, 50, 50, 51];"
    prompt_prefix = "Name é🎮: ".encode()
    read_tail = "Ada🎮 Ok(/@stdlib/console.void::bytes_read.data([10])) Ok(/@stdlib/console.void::text_read.text(é)) Ok(/@stdlib/console.void::bytes_read.data([0])) Ok(/@stdlib/console.void::text_read.eof)\n".encode()
    display = (b"\nHELLO FROM VOID!\nLevel: 42 true 1.25\n-128 -32768 -2147483648 -9223372036854775808 255 65535 4294967295 18446744073709551615 1.5 2.5\nLoading...a, b\n" + "é🎮\0\0END\nticket:9\n<resource> <resource> null\n2026-10-22 2026/10/22\nwrite\0!".encode())
    terminal_redirected = b"caps true false false false false false false false true false\nsize unsupported 0\ncursor_position unsupported 0\n" + b"".join(name+b" unsupported 0\n" for name in [b"set_cursor", b"set_cursor_visible", b"set_cursor_visible", b"set_color", b"reset_color", *([b"set_color"]*16), b"clear", b"set_title", b"bell"]) + b"set_title invalid_argument 0\n"
    # Only the legacy CLI/printf result trailer uses host CRT text newlines.
    # Source print records remain exact UTF-8 bytes, including LF on Windows.
    legacy_zero = b"0\r\n" if os.name == "nt" else b"0\n"
    cases = [
        ("operator_console_systems", b"", combined+legacy_zero, combined_cleanup, 0),
        ("console_operator_systems", "Ada🎮\r\né\0".encode(), combined+prompt_prefix+read_tail, combined_cleanup, 7),
        ("console_operator_failure", b"", b"", b"Format rejected: integrated--1", 1),
        ("console_operator_compound_failure", b"", b"", b"Division by zero", 1),
        ("console_operator_workload", b"", b"[Ok(q:002)]|2024-02-29|002\n"*2048, b"", 0),
        ("console_sum_display", b"", b'/console_sum_display.void::shade.plain /console_sum_display.void::shade.pair(7, \xc3\xa9\x00\xf0\x9f\x8e\xae)\nOk(q:007) Err(\xc3\xa9\x00\xf0\x9f\x8e\xae)\nOk(q:007) Err(\xc3\xa9\x00\xf0\x9f\x8e\xae)\n   Ok(q:007)\ninvalid pattern\nOk(null) Err(active)\n{k: [Ok(q:007), Err(\xc3\xa9\x00\xf0\x9f\x8e\xae)]}\nOk(Ok(q:007))\n/console_sum_display.void::shade.items([q:002, q:003])\n[/console_sum_display.void::branch.nested(<cycle>)]\n[/console_sum_display.void::branch.nested([/console_sum_display.void::branch.leaf]), /console_sum_display.void::branch.nested([/console_sum_display.void::branch.leaf])]\nOk(2026-10-23)\nalias 2\nOk(/console_sum_display.void::ordered.pair(A, B)) [1, 2]\nOk(7) [1, 2]\n', b"", 0),
        ("console_sum_failure", b"", b"cleanup;", b"sum-hook-failure", 1),
        ("console_collections", b"", ("[1, 2, 3] [9, 2, 3]\n[[é\0🎮, raw text], [é\0🎮, raw text]]\n" + "{z: [q:007], a: [q:002]}\n"*3 + "snapshot {K: q:007} q:099\n[null]\n[<cycle>]\n    [1, 2]\n   [é, 🎮]   \ninvalid pattern\n[2026-10-23]\nlarge 1050624\n").encode(), b"", 0),
        ("console_collection_invalidated", b"", b"cleanup;", b"Dictionary structurally modified during iteration", 1),
        ("console_collection_failed_hook", b"", b"cleanup;", b"collection-hook-failure", 1),
        ("console_keys", b"", b"caps false false\nmode unsupported\nkey unsupported\navailable unsupported\n", b"", 0),
        ("console_terminal", b"", b"", terminal_redirected, 0),
        ("console_exit", b"", b"body;block;inner;outer;", b"", 7),
        ("console_zero", b"", b"top;done;", b"", 0),
        ("console_cleanup_primary", b"", b"middle;last;", b"Format rejected: primary", 1),
        ("console_cleanup_only", b"", b"body;", b"Format rejected: cleanup-only", 1),
        ("console_print", b"", display, b"error: -7\n\0\xff\n", 0),
        ("console_order", b"", b"rhs;flush;end;file;sep;hook:7/9;item:7/9|tail!", b"", 0),
        ("console_reads", b"abc\r\n\xc3\xa9\n", "0\nline: abc\n10\nchar: é\neof\nread_text stdin invalid_argument 0 0\n".encode(), b"", 0),
        ("console_fail", b"", b"", b"original-hook-failure", 1),
        ("console_endpoints", b"", b"copy\nrepeat\nmanaged-init\ngeneric\n", b"value-init\nmanaged-default\n", 0),
        ("console_read_errors", b"\xff\x80A\xc3", b"invalid_encoding 2 2 255 128\nready: A\ninvalid_encoding 1 195\neof\n", b"", 0),
    ]
    for name, stdin, stdout, stderr, status in cases:
        source = root / (name + ".void")
        source.write_bytes((fixtures / (name + ".void")).read_bytes())
        for optimized in (False, True):
            switches = [] if optimized else ["--no-opt"]
            started = time.perf_counter()
            p = run([compiler, "run", source, *switches], stdin, status)
            assert p.stdout == stdout, (name, optimized, "JIT stdout", p.stdout, stdout)
            assert (p.stderr == stderr if status != 1 else stderr in p.stderr), (name, p.stderr, stderr)
            if name == "console_fail":
                assert p.stderr.startswith(b"cleanup;")
                checks += 1
            if name == "console_cleanup_primary":
                assert p.stderr.index(b"Format rejected: primary") < p.stderr.index(b"Additional failure during defer:") < p.stderr.index(b"Format rejected: cleanup-one")
                checks += 1
            checks += 2
            outputs.append(dict(case=name, path="JIT", optimized=optimized, status=p.returncode, stdout_hex=p.stdout.hex(), stderr_hex=p.stderr.hex()))
            if name == "console_operator_workload":
                workloads.append(dict(engine="JIT", optimized=optimized, records=2048, bytes=len(p.stdout), sha256=hashlib.sha256(p.stdout).hexdigest(), wall_seconds=time.perf_counter()-started, memory="not sampled by this process runner"))
            if name.startswith("console_operator") or name == "operator_console_systems":
                integration_records.append(outputs[-1])
            obj = root / (name + ("-O2.o" if optimized else "-normal.o"))
            executable = obj.with_suffix(".exe" if os.name == "nt" else ".bin")
            run([compiler, "emit", source, "-o", obj, *switches])
            run([clang, *flags, obj, runtime, fixture, "-o", executable])
            started = time.perf_counter()
            p = run([executable], stdin, status)
            assert p.stdout == stdout, (name, optimized, "AOT stdout", p.stdout, stdout)
            assert (p.stderr == stderr if status != 1 else stderr in p.stderr), (name, p.stderr, stderr)
            if name == "console_fail":
                assert p.stderr.startswith(b"cleanup;")
                checks += 1
            if name == "console_cleanup_primary":
                assert p.stderr.index(b"Format rejected: primary") < p.stderr.index(b"Additional failure during defer:") < p.stderr.index(b"Format rejected: cleanup-one")
                checks += 1
            checks += 2
            outputs.append(dict(case=name, path="linked AOT", optimized=optimized, status=p.returncode, stdout_hex=p.stdout.hex(), stderr_hex=p.stderr.hex()))
            if name == "console_operator_workload":
                workloads.append(dict(engine="linked AOT", optimized=optimized, records=2048, bytes=len(p.stdout), sha256=hashlib.sha256(p.stdout).hexdigest(), wall_seconds=time.perf_counter()-started, memory="not sampled by this process runner"))
            if name.startswith("console_operator") or name == "operator_console_systems":
                integration_records.append(outputs[-1])
            if name == "console_operator_failure":
                assert p.stderr.index(b"integrated--1") < p.stderr.index(b"Additional failure during defer") < p.stderr.index(b"integrated--2"), p.stderr
                assert p.stderr.startswith(b"done [50, 51, 51];") and b"operator_console_model.void" in p.stderr, p.stderr
                checks += 2
            if name == "console_operator_compound_failure":
                assert p.stderr.startswith(b"uncommitted 4;done [10, 20, 30, 10];"), p.stderr
                assert b"current.amount += rhs()" in p.stderr and b"operator +" in p.stderr, p.stderr
                checks += 2
        print(f"{name}: exact normal/O2 JIT and linked AOT verified",flush=True)
    # Full diagnostics (including frames and secondary failures) must match
    # byte-for-byte across both optimizations and independent execution paths.
    for name, *_ in cases:
        assert len({record["stderr_hex"] for record in outputs if record["case"] == name}) == 1, name
        checks += 1
    # The child cannot receive input until its exact no-newline prompt is visible.
    # Readers have a deadline; always kill/reap on failure before joining the thread.
    prompt_records = []
    source = root / "console_prompt.void"
    source.write_bytes((fixtures / source.name).read_bytes())
    prefix = "Name é🎮: ".encode()
    for optimized in (False, True):
        switches = [] if optimized else ["--no-opt"]
        obj = root / ("prompt-O2.o" if optimized else "prompt-normal.o")
        executable = obj.with_suffix(".exe" if os.name == "nt" else ".bin")
        run([compiler, "emit", source, "-o", obj, *switches])
        run([clang, *flags, obj, runtime, fixture, "-o", executable])
        for label, args in [("JIT", [compiler, "run", source, *switches]), ("linked AOT", [executable])]:
            for case, payload, ending in [("unicode-CR-open", "Ada🎮\r".encode(), "Hello, Ada🎮\ndone\n".encode()), ("empty-line", b"\n", b"Hello, \ndone\n"), ("EOF", b"", b"EOF\ndone\n"), ("invalid-UTF8", b"\xff", b"input error invalid_encoding 1\ndone\n")]:
                child = subprocess.Popen([str(a) for a in args], stdin=subprocess.PIPE, stdout=subprocess.PIPE, stderr=subprocess.PIPE, env=environment)
                observed = queue.Queue()
                def read_prompt():
                    observed.put(child.stdout.read(len(prefix)))
                reader = threading.Thread(target=read_prompt, daemon=True)
                reader.start()
                try:
                    received = observed.get(timeout=180)
                    assert received == prefix, (label, case, received)
                    reader.join(timeout=1)
                    if payload:
                        child.stdin.write(payload)
                        child.stdin.flush()
                    else:
                        child.stdin.close()
                        child.stdin = None
                    # CR must complete even while the parent's input pipe stays open.
                    child.wait(timeout=60)
                    stdout, stderr = child.communicate(timeout=10)
                    assert child.returncode == 0 and stdout == ending and not stderr, (label, case, child.returncode, stdout, stderr)
                    checks += 4
                    record = dict(case="console_prompt/"+case, path=label, optimized=optimized, status=child.returncode, stdout_hex=(received+stdout).hex(), stderr_hex=stderr.hex(), prompt_before_input=True, input_left_open_until_exit=bool(payload))
                    outputs.append(record)
                    prompt_records.append(record)
                finally:
                    if child.poll() is None:
                        child.kill()
                    child.communicate()
                    reader.join(timeout=1)
    # Startup includes Debug/O2 compilation under consolidated acceptance load.
    # Keep exact prompt-before-input checks; source startup has a separate budget.
    # Full four-system prompt must be visible before the host supplies input.
    source = root / "console_operator_systems.void"
    for optimized in (False, True):
        switches = [] if optimized else ["--no-opt"]
        executable = root / ("console_operator_systems"+("-O2.bin" if optimized else "-normal.bin"))
        if os.name == "nt": executable=executable.with_suffix(".exe")
        for label,args in [("JIT",[compiler,"run",source,*switches]),("linked AOT",[executable])]:
            child=subprocess.Popen([str(x) for x in args],stdin=subprocess.PIPE,stdout=subprocess.PIPE,stderr=subprocess.PIPE,env=environment)
            ready=queue.Queue();prefix=combined+prompt_prefix
            reader=threading.Thread(target=lambda:ready.put(child.stdout.read(len(prefix))),daemon=True);reader.start()
            try:
                assert ready.get(timeout=180)==prefix,(label,"integrated prompt bytes")
                reader.join(1)
                tail,errors=child.communicate("Ada🎮\r\né\0".encode(),timeout=60)
                assert child.returncode==7 and tail==read_tail and errors==combined_cleanup,(label,tail,errors)
                checks+=3
                integration_records.append(dict(case="four-system/live-prompt",path=label,optimized=optimized,status=7,stdout_hex=(prefix+tail).hex(),stderr_hex=errors.hex()))
            finally:
                if child.poll() is None:child.kill()
                child.communicate();reader.join(1)
    # Production native build and observed startup use the same integrated program.
    native=root/("integrated-native.exe" if os.name=="nt" else "integrated-native")
    started=time.perf_counter();run([compiler,"build",root/"console_operator_systems.void","-o",native]);build_seconds=time.perf_counter()-started
    started=time.perf_counter();p=run([native],"Ada🎮\r\né\0".encode(),7)
    assert p.stdout==combined+prompt_prefix+read_tail and p.stderr==combined_cleanup,(p.stdout,p.stderr)
    workloads.append(dict(engine="production native build/run",build_seconds=build_seconds,wall_seconds=time.perf_counter()-started,stdin_bytes=len("Ada🎮\r\né\0".encode()),stdout_bytes=len(p.stdout),sha256=hashlib.sha256(p.stdout).hexdigest(),memory="not sampled by this process runner"));checks+=2
    terminal_records = []
    if os.name == "posix":
        import fcntl, pty, select, struct, termios, time
        sys.path.insert(0,str(fixtures.parents[0]/"runtime"))
        from console_posix import mode_record, same_termios
        source = root / "console_terminal.void"
        # The same source fixture also ran with all three endpoints redirected.
        for optimized in (False, True):
            switches = [] if optimized else ["--no-opt"]
            obj = root / ("terminal-O2.o" if optimized else "terminal-normal.o")
            executable = obj.with_suffix(".bin")
            run([compiler, "emit", source, "-o", obj, *switches])
            run([clang, *flags, obj, runtime, fixture, "-o", executable])
            for label, args in [("JIT", [compiler, "run", source, *switches]), ("linked AOT", [executable])]:
                master, slave = pty.openpty()
                fcntl.ioctl(slave, termios.TIOCSWINSZ, struct.pack("HHHH", 24, 80, 0, 0))
                mode = termios.tcgetattr(slave)
                mode[3] &= ~(termios.ICANON | termios.ECHO)
                mode[6][termios.VMIN], mode[6][termios.VTIME] = 1, 0
                termios.tcsetattr(slave, termios.TCSANOW, mode)
                os.write(master, b"old\x1b[1;1R")
                child = subprocess.Popen([str(a) for a in args], stdin=slave, stdout=slave, stderr=subprocess.PIPE, env=dict(environment, TERM="xterm-256color"))
                captured, answered, started = b"", False, time.monotonic()
                try:
                    while child.poll() is None:
                        assert time.monotonic()-started < 60, (label, captured)
                        if select.select([master], [], [], 0.02)[0]:
                            captured += os.read(master, 65536)
                        if b"\x1b[6n" in captured and not answered:
                            os.write(master, b"typed\x1b[12;34Rrest")
                            answered = True
                    while select.select([master], [], [], 0)[0]:
                        captured += os.read(master, 65536)
                    _, errors = child.communicate(timeout=5)
                    expected = b"caps true true true true true true true true true false\nsize 80 24\nposition 33 11\nqueued 18 111\n"+b"ok true\n"*24+b"set_title invalid_argument 0\n"
                    assert child.returncode == 0 and errors == expected, (label, optimized, child.returncode, errors, captured)
                    assert answered and b"\x1b[4;3H" in captured and b"\x1b[91m\x1b[44m" in captured and b"\x1b]2;VOID " in captured and "é🎮".encode() in captured and captured.endswith(b"\a") and b"injection" not in captured, captured
                    for foreground in range(16):
                        code = 30+foreground if foreground<8 else 90+foreground-8
                        assert ("\x1b[%dm\x1b[40m" % code).encode() in captured, (foreground,captured)
                    assert termios.tcgetattr(slave)==mode
                    checks += 5
                    record = dict(case="console_terminal/PTY", path=label, optimized=optimized, status=child.returncode, stdout_hex=captured.hex(), stderr_hex=errors.hex())
                    outputs.append(record)
                    terminal_records.append(record)
                finally:
                    if child.poll() is None:
                        child.kill()
                    child.communicate()
                    os.close(master)
                    os.close(slave)
        assert len({r["stdout_hex"] for r in terminal_records}) == 1 and len({r["stderr_hex"] for r in terminal_records}) == 1
        checks += 1
    key_records = []
    if os.name == "posix":
        for key_source in ["console_keys", "console_operator_modes", "console_operator_mode_failure"]:
            source = root / (key_source+".void")
            source.write_bytes((fixtures / source.name).read_bytes())
            payload = "é🎮\r\t\x7f\x1b[A\x1b[24~\x1b[999~\x03\x1b".encode()
            expected_keys = b"caps true true\navailable false\nREADY\n" + "écharacter é 2 false false 1\n🎮character 🎮 4 false false 1\n".encode() + b"enter  1 false false 1\ntab  1 false false 1\nbackspace  1 false false 1\nup  3 false false 1\nf12  5 false false 1\nunknown  6 false false 1\n\x03character \x03 1 false false 1\nescape  1 false false 1\nclosed closed\n"
            if key_source != "console_keys":
                payload = "é".encode()
                expected_keys = combined+b"READY\n"+(b"[Ok(\xc3\xa9)]\n" if key_source=="console_operator_modes" else b"")
            for optimized in [False, True]:
                switches = [] if optimized else ["--no-opt"]
                obj = root / ("keys-opt.o" if optimized else "keys.o")
                executable = obj.with_suffix(".bin")
                run([compiler, "emit", source, "-o", obj, *switches])
                run([clang, *flags, obj, runtime, fixture, "-o", executable])
                for label, args in [("JIT", [compiler, "run", source, *switches]), ("linked AOT", [executable])]:
                    master, slave = pty.openpty()
                    mode = termios.tcgetattr(slave)
                    child = subprocess.Popen([str(a) for a in args], stdin=slave, stdout=subprocess.PIPE, stderr=subprocess.PIPE, env=dict(environment, TERM="xterm-256color"))
                    captured, sent, started = b"", False, time.monotonic()
                    try:
                        while child.poll() is None:
                            # JIT compilation precedes READY; use the same process
                            # deadline as run(), retaining the key-read timeout.
                            assert time.monotonic()-started < 120, (key_source, label, optimized, captured)
                            if select.select([child.stdout], [], [], .02)[0]:
                                captured += os.read(child.stdout.fileno(), 65536)
                            if b"READY\n" in captured and not sent:
                                os.write(master, payload)
                                sent = True
                        tail, errors = child.communicate(timeout=5)
                        captured += tail
                        assert child.returncode == (1 if key_source=="console_operator_mode_failure" else 0) and captured == expected_keys and (not errors if key_source=="console_keys" else errors==combined_cleanup if key_source=="console_operator_modes" else errors.startswith(combined_cleanup[:-2]+b", 50, 51];") and b"integrated--1" in errors), (label, optimized, child.returncode, captured, errors)
                        assert same_termios(mode,termios.tcgetattr(slave)), (mode,termios.tcgetattr(slave))
                        checks += 3
                        record = dict(case=key_source+"/PTY", path=label, optimized=optimized, status=child.returncode, stdout_hex=captured.hex(), stderr_hex=errors.hex(), restored_exactly=termios.tcgetattr(slave)==mode, restored_configuration=True, original_mode=mode_record(mode), restored_mode=mode_record(termios.tcgetattr(slave)))
                        key_records.append(record)
                        outputs.append(record)
                    finally:
                        if child.poll() is None:
                            child.kill()
                        child.communicate()
                        os.close(master)
                        os.close(slave)
    if key_records:
        for key_source in ["console_keys", "console_operator_modes", "console_operator_mode_failure"]:
            selected=[r for r in key_records if r["case"]==key_source+"/PTY"]
            assert len(selected)==4 and len({r["stdout_hex"] for r in selected})==1 and len({r["stderr_hex"] for r in selected})==1,selected
            checks+=3
    broken_paths = []
    if os.name == "posix":
        source = root / "broken.void"
        source.write_text('use console\nfunc done():\n    print("broken-cleanup;", file = console.stderr, end = "")\nfunc again():\n    print("also broken")\nfunc main():\n    defer done()\n    defer again()\n    print("broken")\n', encoding="utf-8")
        obj, executable = root / "broken.o", root / "broken.bin"
        run([compiler, "emit", source, "-o", obj])
        run([clang, *flags, obj, runtime, fixture, "-o", executable])
        for label, args in [("JIT/O2", [compiler, "run", source]), ("linked AOT/O2", [executable])]:
            read_fd, write_fd = os.pipe()
            os.close(read_fd)
            try:
                failed = subprocess.run([str(a) for a in args], input=b"", stdout=write_fd, stderr=subprocess.PIPE, timeout=120, env=environment)
            finally:
                os.close(write_fd)
            assert failed.returncode == 1, (label, failed.returncode, failed.stderr)
            assert b"broken_pipe" in failed.stderr and b"completed=0" in failed.stderr, failed.stderr
            assert b"source call" in failed.stderr, failed.stderr
            assert failed.stderr.startswith(b"broken-cleanup;")
            assert b"Additional failure during defer:" in failed.stderr and failed.stderr.count(b"console print stdout: broken_pipe") == 2
            checks += 5
            broken_paths.append(dict(path=label, status=failed.returncode, stderr_hex=failed.stderr.hex()))
    if broken_paths:
        assert len({record["stderr_hex"] for record in broken_paths}) == 1
        checks += 1
    partial_records=[]
    if sys.platform.startswith("linux"):
        import array
        source=root/"console_operator_partial.void";source.write_bytes((fixtures/source.name).read_bytes())
        for optimized in (False,True):
            switches=[] if optimized else ["--no-opt"]
            obj=root/("partial-O2.o" if optimized else "partial-normal.o");exe=obj.with_suffix(".bin")
            run([compiler,"emit",source,"-o",obj,*switches]);run([clang,*flags,obj,runtime,fixture,"-o",exe])
            for label,args in [("JIT",[compiler,"run",source,*switches]),("linked AOT",[exe])]:
                read_fd,write_fd=os.pipe();capacity=fcntl.fcntl(read_fd,fcntl.F_GETPIPE_SZ)
                child=subprocess.Popen([str(x) for x in args],stdin=subprocess.DEVNULL,stdout=write_fd,stderr=subprocess.PIPE,env=environment);os.close(write_fd)
                try:
                    deadline=time.monotonic()+60
                    while True:
                        available=array.array('i',[0]);fcntl.ioctl(read_fd,termios.FIONREAD,available,True)
                        if available[0]==capacity:break
                        assert child.poll() is None and time.monotonic()<deadline,(label,available[0],capacity)
                        time.sleep(.01)
                    os.close(read_fd);read_fd=-1
                    _,errors=child.communicate(timeout=60)
                    assert child.returncode==0 and errors==f"partial {capacity} 180224\ncleanup;".encode(),(label,capacity,errors)
                    checks+=3;partial_records.append(dict(path=label,optimized=optimized,pipe_capacity=capacity,observed_queued_bytes=available[0],status=0,stderr_hex=errors.hex()))
                finally:
                    if read_fd>=0:os.close(read_fd)
                    if child.poll() is None:child.kill()
                    child.communicate()
    # Legacy entry result printing remains CLI behavior, appended after source output.
    legacy = root / "legacy.void"
    legacy.write_text('func entry(n: i32) -> i32:\n    print("legacy", end = ":")\n    return n\n', encoding="utf-8")
    p = run([compiler, "run", legacy, "17"])
    assert p.stdout == (b"legacy:17\r\n" if os.name == "nt" else b"legacy:17\n") and p.stderr == b""
    checks += 1
    # Existing native cache identity paths: cold, hit, corrupted entry, source change.
    cached = root / "cached.void"
    cached.write_text('print("cache-v1")\n', encoding="utf-8")
    for _ in range(2):
        p = run([compiler, "run", cached, "--cache"])
        assert p.stdout == b"cache-v1\n"
        cache_statuses.append(p.stderr.decode())
        checks += 1
    if "Debug cache: unavailable" not in cache_statuses[0]:
        assert "Debug cache: hit" in cache_statuses[1]
        entry = next((root / ".void/cache/v1").iterdir())
        entry.write_bytes(b"corrupted console native object")
        repaired = run([compiler, "run", cached, "--cache"])
        assert repaired.stdout == b"cache-v1\n" and b"Debug cache: miss" in repaired.stderr
        cache_statuses.append(repaired.stderr.decode())
        checks += 2
    cached.write_text('print("cache-v2")\n', encoding="utf-8")
    assert run([compiler, "run", cached, "--cache"]).stdout == b"cache-v2\n"
    checks += 1
    combined_cache=[]
    cached=root/"operator_console_systems.void"
    def cached_combined(label, expected):
        p=run([compiler,"run",cached,"0","--cache"])
        assert p.stdout==expected+legacy_zero and p.stderr.endswith(combined_cleanup),(label,p.stdout,p.stderr)
        combined_cache.append(dict(case=label,stdout_hex=p.stdout.hex(),stderr_hex=p.stderr.hex()));return p
    cache_directory=root/".void/cache/v1"
    before_files=set(cache_directory.iterdir()) if cache_directory.exists() else set()
    first=cached_combined("cold",combined);second=cached_combined("hit",combined)
    if b"Debug cache: unavailable" not in first.stderr:
        assert b"Debug cache: miss" in first.stderr and b"Debug cache: hit" in second.stderr,(first.stderr,second.stderr)
        new_files=set((root/".void/cache/v1").iterdir())-before_files;assert len(new_files)==1,new_files
        next(iter(new_files)).write_bytes(b"corrupt four-system native object")
        assert b"Debug cache: miss" in cached_combined("corrupt",combined).stderr
        expected_cache=b"Debug cache: miss"
        checks+=3
    else:
        assert b"Debug cache: unavailable" in second.stderr,(first.stderr,second.stderr)
        assert not cache_directory.exists(),"unavailable fallback created cache storage"
        expected_cache=b"Debug cache: unavailable"
        checks+=2
    model=root/"operator_console_model.void";original=model.read_text(encoding="utf-8")
    model.write_text(original.replace('return "q:{self.stored:000}"','return "r:{self.stored:000}"'), encoding="utf-8")
    assert expected_cache in cached_combined("hook-body",combined.replace(b"v:q:",b"v:r:")).stderr
    model.write_text(original.replace('return "q:{self.stored:000}"','return "q:{self.stored:0000}"'), encoding="utf-8")
    assert expected_cache in cached_combined("format-spec",combined.replace(b"v:q:002",b"v:q:0002")).stderr
    model.write_text(original, encoding="utf-8")
    checks+=4
    # Actual installed source identity/discovery (no stdlib override).
    installed = root / "installed"
    run(["cmake", "--install", Path(compiler).parent, "--prefix", installed, "--component", "compiler"])
    installed_environment = dict(os.environ)
    installed_environment.pop("VOID_STDLIB_ROOT", None)
    installed_compiler = installed / "bin" / Path(compiler).name
    assert (installed / "share/void/stdlib/console.void").read_bytes() == (stdlib / "console.void").read_bytes()
    checks += 1
    p = run([installed_compiler, "run", root / "console_print.void"], env=installed_environment)
    assert p.stdout == display and p.stderr == b"error: -7\n\0\xff\n"
    checks += 2
    # A standalone copied library is not admitted as the private standard library.
    standalone = root / "copied_console.void"
    standalone.write_bytes((stdlib / "console.void").read_bytes())
    p = run([compiler, "check", standalone], status=1)
    assert b"_console_" in p.stderr
    checks += 1
    assert hashes == {p.name: hashlib.sha256(p.read_bytes()).hexdigest() for p in stdlib.glob("*.void")}
    checks += 1
Path(runtime).parent.joinpath("step30-output-results.json").write_text(json.dumps(dict(checks=checks, outputs=outputs, cache_statuses=cache_statuses, broken_pipe_paths=broken_paths, installed_console_identity="/@stdlib/console.void", stdlib_snapshot_hashes=hashes, prompt_records=prompt_records, terminal_records=terminal_records, key_records=key_records, integration_records=integration_records, integrated_workloads=workloads, combined_cache=combined_cache, partial_records=partial_records), indent=2)+"\n", encoding="utf-8")
print("source console JIT/AOT driver checks:", checks)
