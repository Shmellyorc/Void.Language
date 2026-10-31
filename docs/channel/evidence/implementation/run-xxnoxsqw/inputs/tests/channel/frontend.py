"""Source-located rejection of type, payload authority, construction and worker misuse."""
from pathlib import Path
import re
import sys
import tempfile
import support as h

compiler = Path(sys.argv[1]).resolve()
base = '''use channel
struct message:
    amount: i32
func receive(const amount: i32):
    pass
'''
cases = {
    'inference': 'var bus = channel.router()',
    'token-construction': 'var token = channel.subscription()',
    'private-type': 'var state = channel._state<i32>()',
    'constructor-arguments': 'var bus = channel.router<i32>(7)',
    'payload-type': 'var bus = channel.router<i32>()\nbus.emit("t", "wrong")',
    'payload-width': 'var bus = channel.router<i32>()\nvar amount: i64 = 7\nbus.emit("t", amount)',
    'payload-null': 'var bus = channel.router<i32>()\nbus.emit("t", null)',
    'topic-type': 'var bus = channel.router<i32>()\nbus.emit(7, 7)',
    'missing-argument': 'var bus = channel.router<i32>()\nbus.subscribe("t")',
    'extra-argument': 'var bus = channel.router<i32>()\nbus.emit("t", 7, 8)',
    'callback-arity': 'var bus = channel.router<i32>()\nbus.subscribe("t", func():\n    pass\n)',
    'callback-result': 'var bus = channel.router<i32>()\nbus.subscribe("t", func(const amount: i32) -> i32:\n    return amount\n)',
    'callback-width': 'var bus = channel.router<i32>()\nbus.subscribe("t", func(const amount: i64):\n    pass\n)',
    'mutable-handler': 'var bus = channel.router<message>()\nbus.subscribe("t", func(payload: message):\n    payload.amount += 1\n)',
    'readonly-body': 'var bus = channel.router<message>()\nbus.subscribe("t", func(const payload: message):\n    payload.amount += 1\n)',
    'readonly-list': 'var bus = channel.router<[message]>()\nbus.subscribe("t", func(const payload: [message]):\n    payload[0].amount += 1\n)',
    'readonly-map': 'var bus = channel.router<{string: message}>()\nbus.subscribe("t", func(const payload: {string: message}):\n    payload["k"].amount += 1\n)',
    'const-subscribe': 'const bus = channel.router<i32>()\nbus.subscribe("t", receive)',
    'const-clear': 'const bus = channel.router<i32>()\nbus.clear()',
    'const-unsubscribe': 'var bus = channel.router<i32>()\nconst token = bus.subscribe("t", receive)\nconst alias = bus\nalias.unsubscribe(token)',
    'token-field': 'var bus = channel.router<i32>()\nconst token = bus.subscribe("t", receive)\nprint(token._identity)',
    'router-field': 'var bus = channel.router<i32>()\nprint(bus._state)',
    'wrong-token': 'var bus = channel.router<i32>()\nbus.unsubscribe(7)',
    'scope-disposal': 'with channel.router<i32>() as bus:\n    bus.emit("t", 7)',
}
programs = {name: base + 'func main():\n' + '\n'.join('    ' + line for line in body.splitlines()) + '\n' for name, body in cases.items()}
programs['async-handler'] = base + 'async func listener(const amount: i32):\n    pass\nfunc main():\n    var bus = channel.router<i32>()\n    bus.subscribe("t", listener)\n'
programs['async-transfer'] = base + 'async func carry(bus: channel.router<i32>) -> i32:\n    return 7\nasync func entry(input: i32) -> i32:\n    var bus = channel.router<i32>()\n    var job = carry(bus)\n    return await job\n'
programs['async-result'] = base + 'async func make() -> channel.router<i32>:\n    return channel.router<i32>()\nasync func entry(input: i32) -> i32:\n    var job = make()\n    var bus = await job\n    return 7\n'
programs['worker-global'] = base + 'var bus = channel.router<i32>()\nfunc work(input: i32) -> i32:\n    bus.emit("t", input)\n    return input\nasync func entry(input: i32) -> i32:\n    var job = spawn work(input)\n    return await job\n'
with tempfile.TemporaryDirectory(prefix='VOID-channel-rejections-') as directory:
    for name, source in programs.items():
        path = Path(directory) / (name + '.void')
        path.write_text(source)
        result = h.run([compiler, 'check', path], status=1)
        assert re.search(rb'\.void:\d+:\d+: error:', result.stderr), (name, result.stderr)
        assert b'code: VOID' in result.stderr and b'VOID IR:' not in result.stderr, (name, result.stderr)
print(f'Channel frontend: {len(programs)} source-located rejections passed')
