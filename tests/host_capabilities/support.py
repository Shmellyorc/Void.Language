from pathlib import Path
import os,sys
ROOT=Path(__file__).resolve().parents[2]
os.environ.setdefault('VOID_CRASH_EVIDENCE',str(ROOT/'docs/voidup/evidence/crashes'))
sys.path.insert(0,str(ROOT/'tests/random'))
import harness
run=harness.run
stack=harness.stack
