@echo off
setlocal
set "G4WSHARE=\\vboxsvr\VM\project-slice7-g4-windows"
set "G4WLOCAL=%USERPROFILE%\VoidS7-G4W-architecture"
copy /b /y "%G4WSHARE%\native-run.cmd" "%G4WLOCAL%\replay-launch.cmd" > "%G4WSHARE%\transfer.log" 2>&1 || exit /b 1
copy /b /y "%G4WSHARE%\native-run.py" "%G4WLOCAL%\replay-run.py" >> "%G4WSHARE%\transfer.log" 2>&1 || exit /b 1
call "%G4WLOCAL%\replay-launch.cmd"
