@echo off
setlocal
set "W2SHARE=\\vboxsvr\VM\g4w2"
copy /y "%W2SHARE%\acl-run.py" "%~dp0acl-run.py" > "%W2SHARE%\acl-transfer.txt" 2>&1
if errorlevel 1 exit /b 1
call "C:\BuildTools\VS2022\VC\Auxiliary\Build\vcvarsall.bat" x64 > "%W2SHARE%\acl-vcvars.txt" 2>&1
if errorlevel 1 exit /b 1
"C:\Users\a\AppData\Local\Programs\Python\Python314\python.exe" "%~dp0acl-run.py" > "%W2SHARE%\acl-console.txt" 2>&1
