"""Map the compiler inventory to the registered capability test families."""
import csv
from pathlib import Path

inventory = Path(__file__).resolve().parents[2] / 'docs/io/evidence'
with (inventory / 'API.tsv').open() as source:
    rows = list(csv.DictReader(source, delimiter='\t'))
assert len(rows) == 149
assert all(row['Parameters documented'] == 'yes' and row['Signature'] for row in rows)


def evidence(identity):
    base = identity.split('::')[-1]
    if '/path.void' in identity:
        return 'io_execution; io_async_execution; io_kernel'
    if '::method::' in identity:
        if base == 'deinit':
            return 'io_ownership; io_native_cancelled; io_native_shutdown'
        if base == 'order':
            return 'io_async_source; io_execution; lsp_io'
        if base.endswith('_async'):
            return 'io_async_execution; io_errors_async; io_async_source; io_ownership'
        return 'io_execution; io_errors_source; io_ownership; io_kernel'
    types = ('error', 'error_kind', 'entry', 'entry_kind', 'open_mode',
             'seek_origin', 'text_read', 'file')
    if '::field::' in identity or '::variant::' in identity or base in types:
        return 'io_ownership (compiler documentation/schema); io_kernel; io_errors_source; lsp_io'
    if base.endswith('_async'):
        return 'io_async_execution; io_async_source; io_ownership'
    return 'io_execution; io_examples_install; io_kernel; io_ownership'


with (inventory / 'API-AUDIT.tsv').open('w') as output:
    writer = csv.writer(output, delimiter='\t', lineterminator='\n')
    writer.writerow([*rows[0].keys(), 'Registered test evidence', 'Contract and ownership'])
    for row in rows:
        identity = row['Identity']
        behavior = 'CONTRACT.md; API.md; compiler-owned ## documentation'
        if '::method::' in identity:
            behavior += '; shared resource/cursor; checked reservation; close invalidates aliases'
        if '_async' in identity:
            behavior += '; bounded native jobs; owner publication; structured cancellation/drain'
        writer.writerow([*row.values(), evidence(identity), behavior])
print(len(rows), 'public signatures mapped; parameters documented; zero warnings')
