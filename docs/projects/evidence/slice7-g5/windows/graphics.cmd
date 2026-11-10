@echo off
set "work=C:\Users\a\VoidS7-G5-Graphics"
if not exist "%work%" mkdir "%work%"
copy /y "\\vboxsvr\VM\slice7-g5\qualify-windows.py" "%work%\qualify-windows.py"
call "C:\BuildTools\VS2022\VC\Auxiliary\Build\vcvarsall.bat" x64 > "\\vboxsvr\VM\slice7-g5\graphics-vcvars.txt" 2>&1
if errorlevel 1 exit /b 1
"C:\Users\a\AppData\Local\Programs\Python\Python314\python.exe" "%work%\qualify-windows.py" corrected > "\\vboxsvr\VM\slice7-g5\graphics-corrected.txt" 2>&1
