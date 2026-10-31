"""Serialize equivalent native-event/router workloads, excluding compile/setup/warmup."""
from pathlib import Path
import json
import hashlib
import sys
import support as h

exe = Path(sys.argv[1]).resolve()
work = h.ROOT / 'build/channel/performance'
work.mkdir(parents=True, exist_ok=True)
cases = [(f'dispatch-{n}-topics-{topics}', n, topics, 'dispatch') for n in (0,1,16,128) for topics in (1,8)]
cases += [('unknown',16,8,'unknown'), ('duplicates',16,1,'dispatch'), ('churn',1,1,'churn'),
          ('clear-reuse',1,1,'clear'), ('nested',1,1,'nested')]
results = []
for name, subscribers, topics, operation in cases:
    for routed in (False, True):
        engine = 'channel' if routed else 'event'
        if routed:
            declarations = 'use channel\nuse convert\nvar bus = channel.router<i32>()\n'
            add = lambda topic: f'bus.subscribe("{topic}", receive)'
            emit = lambda topic, value: f'bus.emit("{topic}", {value})'
            remove = 'bus.unsubscribe(token)'
            clear = 'bus.clear()'
        else:
            declarations = 'use convert\nstruct source:\n    event delivered: func(const i32)\nfunc (self source) fire(const value: i32):\n    self.delivered.emit(value)\n'
            declarations += ''.join(f'var bus{n} = source()\n' for n in range(topics))
            if operation == 'unknown':
                declarations += 'var missing = source()\n'
            add = lambda topic: f'bus{int(topic)}.delivered += receive'
            emit = lambda topic, value: f'bus{int(topic)}.fire({value})'
            remove = 'bus0.delivered -= receive'
            clear = 'bus0 = source()'
        declarations += 'var hits = 0\nfunc receive(const value: i32):\n    hits += value\n'
        if operation == 'nested':
            declarations += '    if value > 0:\n        ' + emit('0','value - 1') + '\n'
        setup = ''.join('        for registration in 0..<' + str(subscribers) + ':\n            ' + add(str(topic)) + '\n' for topic in range(topics)) if subscribers else ''
        source = declarations + 'func entry(rounds: i32) -> i32:\n    if rounds == 0:\n' + (setup or '        pass\n') + '        return 0\n    hits = 0\n    for index in 0..<rounds:\n'
        if operation == 'churn':
            # Unique names exercise pruning; direct event has no topic lookup/storage.
            source += '        const token = bus.subscribe(convert.to_string(index), receive)\n        bus.unsubscribe(token)\n' if routed else '        var topic = convert.to_string(index)\n        bus0.delivered += receive\n        bus0.delivered -= receive\n'
            per = 0
        elif operation == 'clear':
            source += '        ' + clear + '\n        ' + add('0') + '\n        ' + emit('0','1') + '\n'
            per = 1
        elif operation == 'unknown':
            source += '        bus.emit("missing", 1)\n' if routed else '        missing.fire(1)\n'
            per = 0
        else:
            source += '        ' + emit('0','8' if operation == 'nested' else '1') + '\n'
            per = 36 if operation == 'nested' else subscribers
        source += '    return hits\n'
        path = work / (name + '-' + engine + '.void'); path.write_text(source)
        iterations = 300 if operation in ('churn','clear','nested') or subscribers == 128 else 1500
        data = json.loads(h.run([exe, path, iterations, per, 3], cwd=work, timeout=600).stdout)
        data.update(name=name, engine=engine, subscribers=subscribers, topics=topics, operation=operation,
                    source_sha256=hashlib.sha256(path.read_bytes()).hexdigest(),
                    dispatches_per_sample=iterations * (9 if operation == 'nested' else 0 if operation == 'churn' else 1),
                    handler_calls_per_sample=iterations * (9 if operation == 'nested' else per))
        results.append(data)
(h.EVIDENCE / 'performance.json').write_text(json.dumps(results, indent=2) + '\n')
print(f'Channel performance: {len(results)} workloads, {len(results)*3} samples with exact checksums')
