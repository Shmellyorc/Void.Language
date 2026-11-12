"""Exercise D3 through conventional smart HTTPS and disposable real Git repositories."""
from concurrent.futures import ThreadPoolExecutor
from contextlib import contextmanager
import hashlib
import json
from http.server import BaseHTTPRequestHandler, ThreadingHTTPServer
import os
from pathlib import Path
import secrets
import shutil
import socket
import ssl
import subprocess
import sys
import tempfile
from threading import Thread
import tomllib
from urllib.parse import urlsplit
from dependency_explain import explain

compiler, git = (Path(arg).resolve() for arg in sys.argv[1:3])
replay = sys.argv[3:] == ["--reproduce-packfile-uris"]
assert not sys.argv[3:] or replay, sys.argv[3:]
manifest = "void.project.toml"
store = ".void/git-deps"


def run(*args, env=None, cwd=None, status=0, message=None):
    command = list(map(str, args))
    result = subprocess.run(command, capture_output=True, timeout=45, cwd=cwd, env=env)
    assert result.returncode == status, (command, result)
    if message:
        assert message.encode() in result.stderr, (command, result.stderr)
    return result


def write(path, data):
    path.parent.mkdir(parents=True, exist_ok=True)
    path.write_bytes(data.encode())
    return path


def pin(name, origin, revision):
    return (f'\n[dependencies.{name}]\nid="org.example.{name}"\ngit="{origin}"\n'
            f'rev="{revision}"\nversion="=1.0.0"\n')


def project(root, name, source, dependencies="", library=False):
    kind = "library" if library else "application"
    target = (f'[package]\nid="org.example.{name}"\nname="{name}"\nentry="src/api.void"\n'
              if library else f'[application]\nid="org.example.{name}.app"\n[sources]\nroots=["src"]\n')
    write(root / manifest, f'schema=1\n[project]\nid="org.example.{name}.project"\nname="{name}"\n'
                           f'version="1.0.0"\nkind="{kind}"\n' + target + dependencies)
    return write(root / "src" / ("api.void" if library else "app.void"), source)


def commit(repo):
    run(git, "add", ".", cwd=repo, env=git_env)
    run(git, "-c", "user.name=D3 HTTPS", "-c", "user.email=d3@example.invalid",
        "-c", "commit.gpgsign=false", "commit", "--quiet", "-m", "HTTPS fixture", cwd=repo, env=git_env)
    return run(git, "rev-parse", "HEAD", cwd=repo, env=git_env).stdout.decode().strip()


def create_repo(directory, name, source, dependencies=""):
    repo = directory / f"{name}.git"
    repo.mkdir()
    run(git, "init", "--quiet", "--object-format=sha1", cwd=repo, env=git_env)
    project(repo, name, source, dependencies, library=True)
    return repo, commit(repo)


@contextmanager
def serve_crl():
    if os.name != "nt":
        yield None
        return

    # A unique URL keeps Schannel's CRL cache separate between fixture runs.
    path = f"/{secrets.token_hex(16)}.crl"

    class Handler(BaseHTTPRequestHandler):
        def log_message(self, *_):
            pass

        def do_GET(self):
            content = self.server.crl
            if self.path != path or not content:
                self.send_error(404)
                return
            self.send_response(200)
            self.send_header("Content-Type", "application/pkix-crl")
            self.send_header("Content-Length", str(len(content)))
            self.end_headers()
            self.wfile.write(content)

    with ThreadingHTTPServer(("127.0.0.1", 0), Handler) as server:
        server.crl = b""
        server.crl_url = f"http://127.0.0.1:{server.server_port}{path}"
        thread = Thread(target=server.serve_forever, daemon=True)
        thread.start()
        try:
            yield server
        finally:
            server.shutdown()
            thread.join(timeout=5)


def certificates(base, revocation):
    config = """[req]
distinguished_name=dn
[dn]
[ca]
basicConstraints=critical,CA:TRUE
keyUsage=critical,keyCertSign,cRLSign
[leaf]
basicConstraints=critical,CA:FALSE
keyUsage=critical,digitalSignature,keyEncipherment
extendedKeyUsage=serverAuth
subjectAltName=IP:127.0.0.1
"""
    if revocation:
        config += f"""crlDistributionPoints=URI:{revocation.crl_url}
[crl_issuer]
database=ca-index.txt
private_key=ca.key
certificate=ca.pem
default_md=sha256
"""
    write(base / "tls.cnf", config)
    openssl = shutil.which("openssl")
    if not openssl and os.name == "nt":
        exec_path = Path(run(git, "--exec-path", env=git_env).stdout.decode().strip())
        bundled = exec_path.parents[1] / "bin" / "openssl.exe"
        if bundled.is_file():
            openssl = bundled
    assert openssl, "The focused Git HTTPS fixture requires the openssl command."
    openssl = Path(openssl).resolve(strict=True)
    if os.name == "nt":
        print(f"OpenSSL executable {ascii(str(openssl))} SHA-256 {hashlib.sha256(openssl.read_bytes()).hexdigest()}",
              flush=True)
        version = run(openssl, "version", "-a").stdout.decode(errors="backslashreplace")
        print(version.replace("\r\n", "\n"), end="", flush=True)
    # The child process receives a Unicode cwd from Python. Its OpenSSL file
    # operands stay ASCII across the executable's narrow path boundary.
    run(openssl, "req", "-new", "-x509", "-nodes", "-newkey", "rsa:2048", "-days", "3",
        "-config", "tls.cnf", "-extensions", "ca", "-subj", "/CN=VOID D3 HTTPS CA",
        "-keyout", "ca.key", "-out", "ca.pem", cwd=base)
    run(openssl, "req", "-new", "-nodes", "-newkey", "rsa:2048", "-config", "tls.cnf",
        "-subj", "/CN=VOID D3 HTTPS fixture", "-keyout", "server.key", "-out", "server.csr", cwd=base)
    run(openssl, "x509", "-req", "-in", "server.csr", "-CA", "ca.pem", "-CAkey", "ca.key",
        "-set_serial", "1", "-days", "2", "-extfile", "tls.cnf", "-extensions", "leaf", "-out", "server.pem",
        cwd=base)
    if revocation:
        write(base / "ca-index.txt", "")
        run(openssl, "ca", "-batch", "-name", "crl_issuer", "-config", "tls.cnf", "-gencrl",
            "-crldays", "2", "-out", "ca.crl.pem", cwd=base)
        run(openssl, "crl", "-in", "ca.crl.pem", "-outform", "DER", "-out", "ca.crl", cwd=base)
        revocation.crl = (base / "ca.crl").read_bytes()
    context = ssl.SSLContext(ssl.PROTOCOL_TLS_SERVER)
    context.load_cert_chain(base / "server.pem", base / "server.key")
    return base / "ca.pem", context


def packet(data):
    return f"{len(data) + 4:04x}".encode() + data


@contextmanager
def serve(directory, context):
    class Handler(BaseHTTPRequestHandler):
        def log_message(self, *_):
            pass

        def do_GET(self):
            self.do_POST()

        def do_POST(self):
            path, query = urlsplit(self.path).path, urlsplit(self.path).query
            repo, _, endpoint = path[1:].partition("/")
            protocol = self.headers.get("Git-Protocol", "")
            server.requests.append((repo, endpoint, self.headers.get("Authorization"), protocol))
            if repo == "unsolicited.pack":
                self.send_error(404)
                return
            if repo == "redirect.git":
                self.send_response(302)
                self.send_header("Location", f"{server.redirect_to}/units.git/{endpoint}?{query}")
                self.end_headers()
                return
            if repo == "auth.git":
                self.send_response(401)
                self.send_header("WWW-Authenticate", 'Basic realm="D3 fixture"')
                self.end_headers()
                return
            if repo == "noisy.git":
                body = b"D3 remote error\n" * 24000
                self.send_response(500)
                self.send_header("Content-Type", "text/plain")
                self.send_header("Content-Length", str(len(body)))
                self.end_headers()
                self.wfile.write(body)
                return
            if repo == "dumb.git":
                if endpoint == "info/refs":
                    data = f"{server.pack_revision}\trefs/heads/main\n"
                elif endpoint == "HEAD":
                    data = "ref: refs/heads/main\n"
                elif endpoint == "objects/info/http-alternates":
                    data = f"{server.redirect_to}/units.git/objects\n"
                else:
                    self.send_error(404)
                    return
                body = data.encode()
                self.send_response(200)
                self.send_header("Content-Type", "text/plain")
                self.send_header("Content-Length", str(len(body)))
                self.end_headers()
                self.wfile.write(body)
                return

            uri_case = repo in ("pack-uris.git", "forced-v2.git")
            original = "units.git" if uri_case or repo in ("restricted.git", "mirror.git") else repo
            env = dict(git_env, GIT_PROJECT_ROOT=str(directory), GIT_HTTP_EXPORT_ALL="1",
                       REQUEST_METHOD=self.command, PATH_INFO=f"/{original}/{endpoint}", QUERY_STRING=query,
                       CONTENT_TYPE=self.headers.get("Content-Type", ""),
                       CONTENT_LENGTH=self.headers.get("Content-Length", "0"))
            if repo != "restricted.git":
                env["HTTP_GIT_PROTOCOL"] = "version=2" if repo == "forced-v2.git" else protocol
            body = self.rfile.read(int(env["CONTENT_LENGTH"]))
            legacy_rpc = repo == "forced-v2.git" and body.startswith(b"command=")
            if legacy_rpc:
                # The older HTTP helper strips the outer packet lengths when
                # the server forces v2. Forward equivalent framed requests to
                # the real backend and supply its missing response delimiter.
                if body.startswith(b"command=ls-refs"):
                    body = packet(b"command=ls-refs\n") + b"0001" + packet(b"symrefs\n") + b"0000"
                else:
                    assert body.startswith(b"command=fetch"), body
                    body = (packet(b"command=fetch\n") + b"0001" + packet(b"no-progress\n")
                            + packet(b"deepen 1\n") + packet(f"want {server.pack_revision}\n".encode())
                            + packet(b"done\n") + b"0000")
                env["CONTENT_LENGTH"] = str(len(body))
            result = subprocess.run([str(git), "http-backend"], input=body, capture_output=True, env=env, timeout=15)
            # upload-pack can exit nonzero after sending a valid Git error packet.
            headers, separator, content = result.stdout.partition(b"\r\n\r\n")
            assert separator, result.stderr
            if uri_case and b"PACK" in content:
                assert b"packfile-uris" not in body, body
                line = f"{'0' * 40} {server.pack_uri}\n".encode()
                offer = packet(b"packfile-uris\n") + packet(line) + b"0001"
                at = content.find(packet(b"packfile\n"))
                if protocol == "version=2":
                    assert at >= 0, content[:128]
                at = max(at, 0)
                content = content[:at] + offer + content[at:]
                server.uri_offers.append(repo)
            if legacy_rpc:
                content += b"0002"
            status, fields = 200, []
            for line in headers.decode().splitlines():
                name, value = line.split(":", 1)
                if name.lower() == "status":
                    status = int(value.split()[0])
                else:
                    fields.append((name, value.strip()))
            self.send_response(status)
            for name, value in fields:
                self.send_header(name, value)
            self.send_header("Content-Length", str(len(content)))
            self.end_headers()

            # Interrupt the real upload-pack response after part of the pack.
            if repo == "units.git" and b"PACK" in content and server.transfer_fault:
                content = (content[:-5] if server.transfer_fault == "drop"
                           else content[:-5] + bytes([content[-5] ^ 1]) + content[-4:])
            try:
                self.wfile.write(content)
                self.wfile.flush()
                self.connection.shutdown(socket.SHUT_WR)
            except (BrokenPipeError, ConnectionResetError):
                pass

    class Server(ThreadingHTTPServer):
        def handle_error(self, request, address):
            if not isinstance(sys.exception(), (ConnectionError, ssl.SSLError)):
                super().handle_error(request, address)

    with Server(("127.0.0.1", 0), Handler) as server:
        server.socket = context.wrap_socket(server.socket, server_side=True)
        server.requests = []
        server.uri_offers = []
        server.transfer_fault = None
        server.origin = f"https://127.0.0.1:{server.server_port}"
        thread = Thread(target=server.serve_forever, daemon=True)
        thread.start()
        try:
            yield server
        finally:
            server.shutdown()
            thread.join(timeout=5)


def reproduce_packfile_uris(base, ca, server, forbidden, revision):
    """Opt-in positive controls for Git 2.53.0.697, using loopback only."""
    print(run(git, "--version", env=git_env).stdout.decode().strip(), flush=True)
    for label, config, repo, expected in (
        ("original v2", [], "pack-uris.git", 1),
        ("empty fetch.uriProtocols", ["-c", "fetch.uriProtocols="], "pack-uris.git", 1),
        ("v1 response", ["-c", "protocol.version=1"], "pack-uris.git", 0),
        ("forced v2 response to v1", ["-c", "protocol.version=1"], "forced-v2.git", 1),
    ):
        with tempfile.TemporaryDirectory(dir=base) as temporary:
            work = Path(temporary)
            git_dir = work / "repo.git"
            (git_dir / "objects").mkdir(parents=True)
            (git_dir / "refs").mkdir()
            write(git_dir / "HEAD", "ref: refs/heads/void\n")
            write(git_dir / "config", "[core]\n\trepositoryformatversion = 0\n\tbare = true\n")
            env = dict(GIT_OBJECT_DIRECTORY=str(git_dir / "objects"), GIT_CONFIG_GLOBAL=str(work / "no-config"),
                       GIT_CONFIG_SYSTEM=str(work / "no-config"), GIT_CONFIG_NOSYSTEM="1", GIT_NO_REPLACE_OBJECTS="1",
                       GIT_NO_LAZY_FETCH="1", GIT_PROTOCOL_FROM_USER="0",
                       LC_ALL="C.UTF-8" if os.name == "nt" else "C", GIT_ALLOW_PROTOCOL="https",
                       GIT_TERMINAL_PROMPT="0", GIT_ALLOC_LIMIT="134217728", HOME=str(work), USERPROFILE=str(work),
                       XDG_CONFIG_HOME=str(work), NETRC=str(work / "no-netrc"), TMPDIR=str(work), TEMP=str(work),
                       TMP=str(work), GIT_SSL_CAINFO=str(ca))
            if os.name == "nt":
                env["SystemRoot"] = os.environ["SystemRoot"]
            before, offers, requests = len(forbidden.requests), len(server.uri_offers), len(server.requests)
            result = subprocess.run(
                [str(git), f"--git-dir={git_dir}", "-c", "credential.helper=", "-c", "credential.interactive=false",
                 "-c", "http.sslVerify=true", "-c", "http.schannelUseSSLCAInfo=true", "-c", "http.followRedirects=false",
                 "-c", "http.emptyAuth=false", "-c", "http.lowSpeedLimit=1", "-c", "http.lowSpeedTime=10",
                 "-c", "fetch.fsckObjects=true", "-c", "fetch.unpackLimit=1", *config, "fetch", "--quiet", "--depth=1",
                 "--no-tags", "--no-recurse-submodules", "--no-write-fetch-head", "--no-auto-gc",
                 "--no-write-commit-graph", "--", f"{server.origin}/{repo}", revision],
                env=env, cwd=work, stdin=subprocess.DEVNULL, capture_output=True, timeout=30)
            assert result.returncode != 0 and server.uri_offers[offers:] == [repo], result
            assert [entry[0] for entry in forbidden.requests[before:]] == ["unsolicited.pack"] * expected, result
            protocols = [entry[3] for entry in server.requests[requests:]]
            if repo == "forced-v2.git":
                assert protocols[0] == "version=1" and "version=2" in protocols[1:], protocols
            else:
                assert set(protocols) == {"version=1" if "protocol.version=1" in config else "version=2"}
            print(f"{label}: {expected} request(s) to the second TLS origin", flush=True)


def cli(root, *args, env=None, **options):
    return run(compiler, *args, "--project", root, cwd=root, env=env if env is not None else trusted, **options)


def snapshot(root):
    pin_dir, = (root / store).iterdir()
    content, = pin_dir.iterdir()
    return pin_dir.name, content.name, {
        path.relative_to(content).as_posix(): path.read_bytes()
        for path in content.rglob("*") if path.is_file()
    }


def lock_row(root):
    lock = tomllib.loads((root / "void.lock").read_text(encoding="utf-8"))
    assert lock["format"] == lock["resolver"] == 2
    row, = lock["package"]
    return row


def cache_run(root, expected, state):
    result = cli(root, "run", "--profile", "debug", "--cache")
    assert result.stdout == expected, result
    if os.name == "posix":
        assert f"Debug cache: {state}".encode() in result.stderr, result


with tempfile.TemporaryDirectory(prefix="VOID D3 HTTPS 雪 ") as temp, serve_crl() as revocation:
    base = Path(temp).resolve()
    git_env = {k: v for k, v in os.environ.items() if not k.startswith("GIT_")}
    git_env.update(GIT_CONFIG_NOSYSTEM="1", GIT_CONFIG_GLOBAL=str(base / "empty-git-config"))
    scratch = base / "transport"
    scratch.mkdir()
    clean = dict(git_env, TMPDIR=str(scratch), TEMP=str(scratch), TMP=str(scratch))
    ca, context = certificates(base, revocation)
    trusted = dict(clean, GIT_SSL_CAINFO=str(ca))
    repos = base / "repositories"
    repos.mkdir()
    units_api = "public func next() -> i32:\n    return 41\n"
    program = "use pkg/units\nfunc main():\n    print(units.next() + 1)\n"
    units, old = create_repo(repos, "units", units_api)
    app = base / "consumer"
    with serve(repos, context) as server, serve(repos, context) as forbidden:
        server.redirect_to = forbidden.origin
        server.pack_uri = forbidden.origin + "/unsolicited.pack"
        server.pack_revision = old
        origin = server.origin + "/units.git"
        project(app, "app", program, pin("units", origin, old))
        if replay:
            reproduce_packfile_uris(base, ca, server, forbidden, old)
            server.requests.clear()
            forbidden.requests.clear()

        # These commands must fail before any Git traffic, even without a snapshot.
        cli(app, "resolve", status=1, message="Missing Git dependency snapshot")
        explain(app, status=1, message="Missing void.lock")
        for args in [("check",), ("run",), ("build", "-o", base / "missing"),
                     ("emit", "-o", base / "missing.o")]:
            cli(app, *args, status=1, message="Missing void.lock")
        assert not server.requests and not (app / store).exists()

        # Caller Git config and environment cannot redirect the URL, change TLS
        # verification or provide netrc, credential helpers or askpass.
        user = base / "caller"
        write(user / ".netrc", "machine 127.0.0.1 login forbidden password forbidden\n")
        config = write(user / ".gitconfig", f'[url "{server.origin}/redirect.git"]\n'
                       f'  insteadOf = {origin}\n[http]\n  sslVerify = false\n')
        hostile = dict(trusted, HOME=str(user), USERPROFILE=str(user), NETRC=str(user / ".netrc"), LC_ALL="C",
                       GIT_CONFIG_GLOBAL=str(config), GIT_CONFIG_COUNT="1", GIT_CONFIG_KEY_0="http.proxy",
                       GIT_CONFIG_VALUE_0="http://127.0.0.1:1", HTTPS_PROXY="http://127.0.0.1:1", NO_PROXY="*",
                       GIT_SSL_NO_VERIFY="1", GIT_ASKPASS=str(base / "forbidden-askpass"),
                       GIT_EXEC_PATH=str(base / "forbidden-helpers"), GIT_OBJECT_DIRECTORY=str(base / "foreign"))
        cli(app, "deps", "fetch", env=dict(hostile, GIT_SSL_CAINFO=str(ca.with_name("missing.pem"))),
            status=1, message="GIT_SSL_CAINFO")
        untrusted = dict(hostile)
        del untrusted["GIT_SSL_CAINFO"]
        cli(app, "deps", "fetch", env=untrusted, status=1, message="HTTPS Git fetch failed")
        assert not server.requests and not (app / store).exists()
        cli(app, "deps", "fetch", env=hostile)
        assert {repo for repo, _, _, _ in server.requests} == {"units.git"}
        acquired = snapshot(app)
        calls = len(server.requests)
        cli(app, "deps", "fetch", env=untrusted)
        cli(app, "resolve")
        cli(app, "resolve", "--locked")
        assert len(server.requests) == calls and snapshot(app) == acquired
        lock = app / "void.lock"
        first_lock = lock.read_bytes()
        row = lock_row(app)
        assert row["git"] == origin and row["rev"] == old and row["subdir"] == ""
        report = explain(app, env=untrusted)
        assert json.loads(report)["packages"][0]["source"] == {
            "kind": "git", "origin": origin, "revision": old, "subdir": "",
        }
        assert explain(app, env=hostile) == report and len(server.requests) == calls
        assert os.fsencode(base) not in first_lock and b".void/git-deps" not in first_lock
        pin_dir = app / store / acquired[0]
        parked = base / "parked snapshot"
        pin_dir.rename(parked)
        try:
            explain(app, status=1, message="Missing Git dependency snapshot", env=trusted)
            for args in [("resolve", "--locked"), ("check",), ("run",), ("build", "-o", base / "missing"),
                         ("emit", "-o", base / "missing.o")]:
                cli(app, *args, status=1, message="Missing Git dependency snapshot")
            assert len(server.requests) == calls and not list((app / store).iterdir())
        finally:
            parked.rename(pin_dir)
        cache_run(app, b"42\n", "miss")
        cache_run(app, b"42\n", "hit")

        # Compare origin-independent D1 identity and exported bytes with file:.
        local = base / "file comparison"
        shutil.copytree(units, local / "units.git")
        project(local, "app", program, pin("units", "file:units.git", old))
        cli(local, "deps", "fetch")
        cli(local, "resolve")
        local_snapshot = snapshot(local)
        assert local_snapshot[:2] != acquired[:2] and local_snapshot[2] == acquired[2]
        assert all(lock_row(local)[key] == row[key] for key in ("manifest", "tree", "rev"))

        write(units / "src/api.void", units_api.replace("41", "57"))
        new = commit(units)
        assert new != old
        fresh = base / "fresh pin"
        project(fresh, "app", program, pin("units", origin, old))
        cli(fresh, "deps", "fetch")
        assert snapshot(fresh) == acquired
        cli(fresh, "resolve")
        assert (fresh / "void.lock").read_bytes() == first_lock

        probe = base / "negative"
        for target in (forbidden, server):
            server.pack_uri = target.origin + "/unsolicited.pack"
            project(probe, "app", program, pin("units", server.origin + "/pack-uris.git", new))
            before, offers = len(server.requests), len(server.uri_offers)
            cli(probe, "deps", "fetch", env=hostile, status=1)
            assert server.uri_offers[offers:] == ["pack-uris.git"]
            assert {(repo, protocol) for repo, _, _, protocol in server.requests[before:]} == {
                ("pack-uris.git", "version=2")
            }
            assert not forbidden.requests and not (probe / store).exists() and not list(scratch.iterdir())

        project(probe, "app", program, pin("units", server.origin + "/dumb.git", old))
        before = len(server.requests)
        cli(probe, "deps", "fetch", status=1, message="dumb http transport does not support shallow capabilities")
        contacted = {(repo, endpoint) for repo, endpoint, _, _ in server.requests[before:]}
        assert {("dumb.git", "info/refs")} <= contacted <= {("dumb.git", "info/refs"), ("dumb.git", "HEAD")}
        assert not forbidden.requests and not (probe / store).exists()

        for url in (f"{origin}?query=1", origin.replace("https://", "http://"),
                    "https://127.0.0.1:0/units.git", "https://127.0.0.1:65536/units.git"):
            project(probe, "app", program, pin("units", url, old))
            before = len(server.requests)
            cli(probe, "deps", "fetch", status=1, message="Invalid Git dependency origin or revision")
            assert len(server.requests) == before
        for url, revision, diagnostic in [
            (origin, "f" * 40, "did not supply the pinned commit"),
            (server.origin + "/restricted.git", old, "server may refuse exact object-ID requests"),
            (server.origin.replace("127.0.0.1", "localhost") + "/units.git", old, "HTTPS Git fetch failed"),
            (server.origin + "/redirect.git", old, "HTTPS Git fetch failed"),
            (server.origin + "/auth.git", old, "HTTPS Git fetch failed"),
            (server.origin + "/noisy.git", old, "private storage limit"),
        ]:
            project(probe, "app", program, pin("units", url, revision))
            before = len(server.requests)
            cli(probe, "deps", "fetch", env=hostile, status=1, message=diagnostic)
            assert not (probe / store).exists() and not list(scratch.iterdir())
            if url.endswith("/redirect.git"):
                assert [repo for repo, _, _, _ in server.requests[before:]] == ["redirect.git"]
                assert not forbidden.requests

        project(probe, "app", program, pin("units", origin, old))
        for fault in ("drop", "corrupt"):
            server.transfer_fault = fault
            cli(probe, "deps", "fetch", status=1, message="HTTPS Git fetch failed")
            assert not (probe / store).exists() and not list(scratch.iterdir())
        server.transfer_fault = None
        abandoned = scratch / "void-git-abandoned"
        write(abandoned / "repo.git/objects/pack/tmp_pack_partial", "interrupted data")
        with ThreadPoolExecutor(max_workers=3) as pool:
            list(pool.map(lambda _: cli(probe, "deps", "fetch"), range(3)))
        assert snapshot(probe) == acquired and list(scratch.iterdir()) == [abandoned]
        shutil.rmtree(abandoned)

        mirror = server.origin + "/mirror.git"
        top = base / "transitive"
        _, shapes = create_repo(repos, "shapes", "public func answer():\n    print(42)\n", pin("units", mirror, old))
        project(top, "app", "func main():\n    print(42)\n", pin("shapes", server.origin + "/shapes.git", shapes))
        before = len(server.requests)
        cli(top, "deps", "fetch", "--allow-git", origin, old, status=1, message="explicit acquisition authorization")
        assert {repo for repo, _, _, _ in server.requests[before:]} == {"shapes.git"}
        assert not (top / store).exists()
        cli(top, "deps", "fetch", "--allow-git", mirror, old)
        assert len(list((top / store).iterdir())) == 2

        project(app, "app", program, pin("units", mirror, old))
        cli(app, "check", status=1, message="Missing Git dependency snapshot")
        cli(app, "deps", "fetch")
        cli(app, "check", status=1, message="Stale or tampered void.lock")
        assert lock.read_bytes() == first_lock
        cli(app, "resolve", "--refresh")
        assert lock.read_bytes() != first_lock and lock_row(app)["tree"] == row["tree"]
        cache_run(app, b"42\n", "miss")

        project(app, "app", program, pin("units", origin, new))
        cli(app, "resolve", "--refresh", status=1, message="Missing Git dependency snapshot")
        cli(app, "deps", "fetch")
        cli(app, "run", status=1, message="Stale or tampered void.lock")
        cli(app, "resolve", "--refresh")
        assert lock_row(app)["rev"] == new and lock_row(app)["tree"] != row["tree"]
        cache_run(app, b"58\n", "miss")
        assert all(auth is None for _, _, auth, _ in server.requests)

    # The TLS origin is gone. A locked project must compile from held bytes.
    final_lock = lock.read_bytes()
    cli(app, "deps", "fetch", env=clean)
    cli(app, "resolve", "--locked", env=clean)
    assert json.loads(explain(app, env=clean))["lock"]["sha256"] == hashlib.sha256(final_lock).hexdigest()
    cli(app, "check", env=clean)
    cli(app, "emit", "-o", base / "offline.o", env=clean)
    cache_run(app, b"58\n", "hit")
    for profile in ("debug", "release"):
        assert cli(app, "run", "--profile", profile, "--no-cache", env=clean).stdout == b"58\n"
        image = base / (profile + (".exe" if os.name == "nt" else ""))
        # AOT uses the caller's admitted native-link root. The transport
        # scratch only controls Git workspaces and their cleanup assertions.
        cli(app, "build", "--profile", profile, "-o", image, env=git_env)
        assert run(image, cwd=base, env=clean).stdout == b"58\n"
    assert lock.read_bytes() == final_lock
    exported, = (fresh / store).glob("*/*/tree/src/api.void")
    exported.write_bytes(b"corrupt")
    cli(fresh, "deps", "fetch", env=clean, status=1, message="Git object identity mismatch")
    exported.unlink()
    cli(fresh, "check", env=clean, status=1)
    assert not list(scratch.iterdir())

print("D3 HTTPS exact pins, TLS isolation, publication and offline JIT/AOT passed")
