@echo off
"C:\Users\a\AppData\Local\Programs\Python\Python314\python.exe" -c "import json,shutil,sys; print(json.dumps(dict(build=sys.getwindowsversion().build, free_bytes=shutil.disk_usage('C:/').free)))" > "\\vboxsvr\VM\g4w2\space-before-build.json"
