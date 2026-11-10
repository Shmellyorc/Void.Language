"""Verify a role identity rejected by the shared manifest validator cannot publish."""
from pathlib import Path
import json
import subprocess
import sys
import tempfile

with tempfile.TemporaryDirectory(prefix="VOID S5 invalid identity ") as temporary:
    workspace = Path(temporary).resolve(strict=True)
    target = workspace / "application"
    results = []
    for compiler in sys.argv[1:]:
        args = [compiler, "new", str(target), "--app-id", "CON"]
        child = subprocess.run(args, cwd=workspace, capture_output=True, text=True, encoding="utf-8", timeout=30)
        result = {"argv": args, "status": child.returncode, "stdout": child.stdout, "stderr": child.stderr,
                  "remaining_entries": [entry.name for entry in workspace.iterdir()]}
        results.append(result)
        assert result["status"] == 1 and "VOID5004" in result["stderr"] and "CON" in result["stderr"], result
        assert not result["remaining_entries"], result
    print(json.dumps(results, indent=2))
