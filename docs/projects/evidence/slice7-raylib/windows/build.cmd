@echo off
setlocal
set "exchange=\\vboxsvr\VM\slice7-raylib\compiler"
set "localroot=C:\Users\a\VoidS7-Raylib-Compiler"
mkdir "%localroot%"
copy /y "%exchange%\build.py" "%localroot%\build.py"
call "C:\BuildTools\VS2022\VC\Auxiliary\Build\vcvarsall.bat" x64 > "%exchange%\vcvars.log" 2>&1
if errorlevel 1 exit /b 1
"C:\Users\a\AppData\Local\Programs\Python\Python314\python.exe" "%localroot%\build.py" > "%exchange%\build-console.log" 2>&1
