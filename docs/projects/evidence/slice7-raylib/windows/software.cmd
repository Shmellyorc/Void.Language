@echo off
set "work=C:\Users\a\VoidS7-Raylib-Compatibility"
if not exist "%work%" mkdir "%work%"
copy /y "\\vboxsvr\VM\slice7-raylib\compat-software.py" "%work%\compat-software.py"
call "C:\BuildTools\VS2022\VC\Auxiliary\Build\vcvarsall.bat" x64 > "\\vboxsvr\VM\slice7-raylib\software-vcvars.txt" 2>&1
if errorlevel 1 exit /b 1
"C:\Users\a\AppData\Local\Programs\Python\Python314\python.exe" "%work%\compat-software.py" > "\\vboxsvr\VM\slice7-raylib\software-console.txt" 2>&1
