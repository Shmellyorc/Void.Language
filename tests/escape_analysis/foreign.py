import subprocess,sys
result=subprocess.run([sys.argv[1]],capture_output=True,text=True)
sys.stdout.write(result.stdout);sys.stderr.write(result.stderr)
assert result.returncode==86 and result.stderr.strip()=='preserved current-heap child validation',result.returncode
