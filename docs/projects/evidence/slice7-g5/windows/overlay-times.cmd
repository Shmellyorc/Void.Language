@echo off
set "work=C:\Users\a\VoidS7-G5-Compiler"
copy /y "\\vboxsvr\VM\slice7-g5\overlay-times.py" "%work%\overlay-times.py"
"C:\Users\a\AppData\Local\Programs\Python\Python314\python.exe" "%work%\overlay-times.py" > "\\vboxsvr\VM\slice7-g5\overlay-times.txt" 2>&1
