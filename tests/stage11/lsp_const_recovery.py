"""Completion must survive pure declarations, global inference and dirty edits."""
from pathlib import Path
import sys
import tempfile

sys.path.insert(0, str(Path(__file__).resolve().parents[2] / "tools"))
from lsp_client import Client

server = Path(sys.argv[1]).resolve()
if "--small-stack" in sys.argv:
    import resource
    resource.setrlimit(resource.RLIMIT_STACK, (1024 * 1024, 1024 * 1024))

checks = 0


def check(valid, detail):
    global checks
    assert valid, detail
    checks += 1


with tempfile.TemporaryDirectory(prefix="VOID const recovery é🎮 ") as directory:
    root = Path(directory)
    entry = root / "main.void"
    (root / "game.void").write_text("var shared = 9\n")
    valid = "var score = 7\nconst limit = 4\nfunc main() -> i32:\n    return score\n"
    entry.write_text(valid)
    with Client(server, entry) as client:
        client.initialize()
        version = 0
        current = valid

        def replace(text):
            global current, version
            current = text
            version += 1
            if version == 1:
                client.notify("textDocument/didOpen", {"textDocument": {
                    "uri": entry.as_uri(), "languageId": "void", "version": version, "text": text}})
            else:
                client.notify("textDocument/didChange", {"textDocument": {
                    "uri": entry.as_uri(), "version": version}, "contentChanges": [{"text": text}]})
            return [diagnostic for notice in client.sync() for diagnostic in notice["diagnostics"]]

        def completion(template):
            marker = template.index("|")
            prefix = template[:marker]
            text = template.replace("|", "", 1)
            replace(text)
            position = {"line": prefix.count("\n"),
                        "character": len(prefix.rsplit("\n", 1)[-1].encode("utf-16-le")) // 2}
            for repeat in range(3):
                response = client.request("textDocument/completion", {
                    "textDocument": {"uri": entry.as_uri()}, "position": position})[0]
                check("error" not in response, (template, response))
                result = response["result"]
                check(isinstance(result, (list, dict)), response)
                check(client.process.poll() is None, "completion terminated voidls")
                client.sync()
            return {item["label"] for item in (result["items"] if isinstance(result, dict) else result)}

        for declaration in ("var score = 7\n", "var score: i64 = 7\n",
                            "var score = later\nvar later = 7\n",
                            "use \"./game.void\" as game\nvar score = game.shared\n"):
            for initializer in ("", "1 + ", "score + ", "(1 + "):
                source = declaration + "const limit = 4\nconst pending = " + initializer + "|\nfunc main():\n    print(score)\n"
                labels = completion(source)
                check("score" not in labels, "pure completion offered a runtime global")
                check(not replace(valid), "valid edit did not recover after constant completion")
            for annotation in ("", ": i32"):
                source = declaration + "const limit = 4\nfunc main():\n    const pending" + annotation + " = |\n    print(score)\n"
                labels = completion(source)
                check("score" in labels and "limit" in labels, (source, labels))
                check(not replace(valid), "local const recovery failed")

        for source in (
            "var score = \nconst pending = |\nfunc main():\n    print(1)\n",
            "var score: i64 = \nconst pending: i32 = |\nfunc main():\n    print(1)\n",
            "var score = other\nvar other = score\nconst pending = |\nfunc main():\n    print(1)\n",
            "var score: unknown = 7\nconst pending = |\nfunc main():\n    print(1)\n",
            "var score = 7\nfunc inspect(value: i32 = |):\n    print(value)\nfunc main():\n    print(score)\n",
            "var score = 7\nvalue struct holder:\n    value: i32 = |\nfunc main():\n    print(score)\n",
            "var score = 7\nvalue struct holder:\n    data: [i32, |]\nfunc main():\n    print(score)\n",
            "var score = 7\nvar pending = |\nfunc main():\n    print(score)\n",
        ):
            completion(source)
            check(not replace(valid), "server failed to recover from malformed globals/defaults")

        minimal = Path(__file__).with_name("const_global_minimal.void").read_text()
        check(not replace(minimal), "minimal Boss fixture has unexpected diagnostics")
        for replacement in ("const max_tries = |", "const max_tries = 3|",
                            "const max_tries = 3\n|"):
            completion(minimal.replace("const max_tries = 3", replacement, 1))
            check(not replace(minimal), "minimal Boss fixture did not recover")

        game = Path(__file__).with_name("guessing_game.void").read_text()
        check(not replace(game), "Boss guessing game has unexpected diagnostics")
        for replacement in ("const max_tries = |", "const max_tries = 3|",
                            "const max_tries = 3 |", "const max_tries = 3\n|",
                            "const max_tries |"):
            edited = game.replace("const max_tries = 3", replacement, 1)
            completion(edited)
            check(not replace(game), "Boss game did not recover after its original edit")
        local_game = game.replace("    var won = false", "    const pending = |\n    var won = false")
        labels = completion(local_game)
        check("index" in labels, labels)
        for bad_api in (game.replace("convert.parse", "convert.not_ready"),
                        game.replace("readline(", "missing_readline(")):
            check(bool(replace(bad_api)), "missing game API did not produce ordinary diagnostics")
            completion(bad_api.replace("const max_tries = 3", "const max_tries = |", 1))
            check(not replace(game), "game did not recover from a missing API")

        diagnostics = replace("var score = 7\nconst invalid = score + 1\nfunc main():\n    print(score)\n")
        check(any("compile-time" in item["message"] and "runtime global" in item["message"]
                  for item in diagnostics), diagnostics)
        check(not replace(valid), "pure-expression diagnostic did not recover")
        labels = completion(valid.replace("    return score", "    const pending = |\n    return score"))
        check("score" in labels, labels)
        check(not replace(valid), "final recovery diagnostics")
        client.shutdown()
        check("bad_function_call" not in client.logs.decode() and
              "Internal language server failure" not in client.logs.decode(), client.logs.decode())

print(f"Const/global completion recovery: {checks} assertions passed" +
      (" with actual 1 MiB server stack" if "--small-stack" in sys.argv else ""))
