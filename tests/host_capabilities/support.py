from pathlib import Path
import os,sys
ROOT=Path(__file__).resolve().parents[2]
sys.path.insert(0,str(ROOT/'tests/random'))
import harness
run=harness.run
stack=harness.stack

def output_directory():
    override = os.environ.get('VOID_HOST_EVIDENCE_DIR')
    directory = Path(override) if override else harness.output_directory()
    directory.mkdir(parents=True, exist_ok=True)
    return directory
