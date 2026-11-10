@echo off
setlocal
set "G4WSHARE=\\vboxsvr\VM\project-slice7-g4-windows"
call "C:\BuildTools\VS2022\VC\Auxiliary\Build\vcvarsall.bat" x64 > "%G4WSHARE%\vcvars.log" 2>&1
if errorlevel 1 exit /b 1
"C:\Users\a\AppData\Local\Programs\Python\Python314\python.exe" "%G4WSHARE%\native-run.py" > "%G4WSHARE%\native-console.log" 2>&1
