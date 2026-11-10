@echo off
setlocal
set "exchange=\\vboxsvr\VM\slice7-final-integrated"
call "C:\BuildTools\VS2022\VC\Auxiliary\Build\vcvarsall.bat" x64 > "%exchange%\vcvars.txt" 2>&1
if errorlevel 1 exit /b 1
"C:\Users\a\AppData\Local\Programs\Python\Python314\python.exe" "%exchange%\retry-windows.py" Debug > "%exchange%\retry-debug-console.txt" 2>&1
set "result=%errorlevel%"
> "%exchange%\retry-debug-exit.txt" echo %result%
exit /b %result%
