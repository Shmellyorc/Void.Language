from hashlib import sha256
from pathlib import Path
import sys

descriptor, main, dependency, output = map(Path, sys.argv[1:])
contract = descriptor.read_text()
thunk = next(row.split()[1] for row in contract.splitlines() if row.startswith('thunk '))
exports = [row.split()[2] for row in contract.splitlines() if row.startswith('bind ')]
output.write_text(
    'void-native-closure 1\nprovider org.voidlang.g4-fixture\n'
    'target x86_64-unknown-linux-gnu sysv-x64\nsystem none\nlayout executable-sibling-native\n'
    f'thunk {thunk} vd_c_primitive_dispatch_v3\n'
    f'image {dependency.name} {sha256(dependency.read_bytes()).hexdigest()}\n'
    f'image {main.name} {sha256(main.read_bytes()).hexdigest()} {dependency.name}\n'
    + f'export {dependency.name} g4_bias\n'
    + ''.join(f'export {main.name} {symbol}\n' for symbol in ['vd_c_primitive_dispatch_v3', *exports])
    + 'descriptor\n' + contract
)
