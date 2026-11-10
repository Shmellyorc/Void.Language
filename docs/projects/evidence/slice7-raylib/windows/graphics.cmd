@echo off
set "work=C:\Users\a\VoidS7-Raylib-Graphics"
if not exist "%work%" mkdir "%work%"
copy /y "\\vboxsvr\VM\slice7-raylib\qualify-windows.py" "%work%\qualify-windows.py"
call "C:\BuildTools\VS2022\VC\Auxiliary\Build\vcvarsall.bat" x64 > "\\vboxsvr\VM\slice7-raylib\graphics-vcvars.txt" 2>&1
if errorlevel 1 exit /b 1
"C:\Users\a\AppData\Local\Programs\Python\Python314\python.exe" "%work%\qualify-windows.py" psapi > "\\vboxsvr\VM\slice7-raylib\graphics-psapi.txt" 2>&1
