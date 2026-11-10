@echo off
setlocal
set "W2SHARE=\\vboxsvr\VM\g4w2"
copy /y "%W2SHARE%\native-run.py" "%~dp0native-run.py" > "%W2SHARE%\transfer.txt" 2>&1
if errorlevel 1 exit /b 1
call "C:\BuildTools\VS2022\VC\Auxiliary\Build\vcvarsall.bat" x64 > "%W2SHARE%\vcvars.txt" 2>&1
if errorlevel 1 exit /b 1
"C:\Users\a\AppData\Local\Programs\Python\Python314\python.exe" "%~dp0native-run.py" > "%W2SHARE%\native-console.txt" 2>&1
