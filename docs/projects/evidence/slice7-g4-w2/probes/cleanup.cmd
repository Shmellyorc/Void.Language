@echo off
setlocal
set "W2SHARE=\\vboxsvr\VM\g4w2"
copy /y "%W2SHARE%\cleanup.py" "%~dp0cleanup.py" > "%W2SHARE%\cleanup-transfer.txt" 2>&1
if errorlevel 1 exit /b 1
"C:\Users\a\AppData\Local\Programs\Python\Python314\python.exe" "%~dp0cleanup.py" > "%W2SHARE%\cleanup-console.txt" 2>&1
