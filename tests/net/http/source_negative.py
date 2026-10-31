"""Authority and private-kernel rejection before any HTTP effects."""
import argparse
import json
from pathlib import Path
import subprocess
import tempfile

CASES = {
    'readonly_client': ('check', 'use net/http\nfunc probe(const owner: http.client):\n    owner.close()\nreturn 0\n', 'readonly'),
    'readonly_transfer': ('check', 'use net/http\nfunc probe(const exchange: http.transfer):\n    exchange.read_bytes()\nreturn 0\n', 'readonly'),
    'private_factory': ('check', 'use net/http\nvar owner = http.client()\nreturn 0\n', 'private'),
    'private_kernel': ('check', 'use net/http\n_http_capabilities()\nreturn 0\n', 'Unknown function'),
    'worker_network': ('run', 'use net/http\nfunc work() -> i32:\n    http.get("http://127.0.0.1:1/")\n    return 7\nasync func entry(n: i32) -> i32:\n    var pending = spawn work()\n    return await pending\n', 'worker'),
}
def main():
    parser = argparse.ArgumentParser(); parser.add_argument('compiler', type=Path); parser.add_argument('--evidence', type=Path, required=True)
    args = parser.parse_args(); args.compiler = args.compiler.resolve(); args.evidence.mkdir(parents=True, exist_ok=True)
    receipts = []
    with tempfile.TemporaryDirectory(prefix='VOID HTTP rejection ') as directory:
        for name, (command, text, diagnostic) in CASES.items():
            source = Path(directory) / (name + '.void'); source.write_text(text)
            result = subprocess.run([str(args.compiler), command, str(source)], text=True, capture_output=True, timeout=60)
            (args.evidence / (name + '.stderr')).write_text(result.stderr); (args.evidence / (name + '.void')).write_text(text)
            receipts.append({'name': name, 'status': result.returncode, 'diagnostic': diagnostic})
            assert result.returncode == 1 and diagnostic in result.stderr, (name, result)
    (args.evidence / 'commands.json').write_text(json.dumps(receipts, indent=2) + '\n')
    print(f'{len(receipts)} HTTP source authority/private/worker rejections passed')
if __name__ == '__main__': main()
