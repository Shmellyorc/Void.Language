"""Build and run the optional private fixed-call behavior tests."""
import argparse
import subprocess
from pathlib import Path

parser = argparse.ArgumentParser(description=__doc__)
parser.add_argument('--build', type=Path, required=True)
args = parser.parse_args()
shapes = ((0, 2), (3, 2), (7, 16384))
targets = [f'void_private_fixed_call_{mode}_{selected}_{extent}_{witness}'
           for mode in ('normal', 'optimized')
           for selected, extent in shapes
           for witness in ('observed', 'unobserved')]
subprocess.run(['cmake', '--build', str(args.build), '--target', *targets, '-j', '2'], check=True)
subprocess.run(['ctest', '--test-dir', str(args.build), '-L', 'private_calls', '--output-on-failure'], check=True)
