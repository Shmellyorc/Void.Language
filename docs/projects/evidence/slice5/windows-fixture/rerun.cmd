@echo off
call "C:\BuildTools\VS2022\VC\Auxiliary\Build\vcvarsall.bat" x64 > "\\vboxsvr\VM\project-slice5\fixture\vcvars.log" 2>&1
if errorlevel 1 exit /b 1
"C:\Users\a\AppData\Local\Programs\Python\Python314\python.exe" "\\vboxsvr\VM\project-slice5\fixture\rerun.py" > "\\vboxsvr\VM\project-slice5\fixture\console.log" 2>&1
