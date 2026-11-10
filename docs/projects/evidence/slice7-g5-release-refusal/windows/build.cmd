@echo off
setlocal
set "exchange=\\vboxsvr\VM\slice7-g5-release-refusal"
call "C:\BuildTools\VS2022\VC\Auxiliary\Build\vcvarsall.bat" x64 > "%exchange%\vcvars.log" 2>&1
if errorlevel 1 exit /b 1
"C:\Users\a\AppData\Local\Programs\Python\Python314\python.exe" "%exchange%\build.py" > "%exchange%\build-console.log" 2>&1
set "result=%errorlevel%"
echo %result%>"%exchange%\exit.txt"
exit /b %result%
