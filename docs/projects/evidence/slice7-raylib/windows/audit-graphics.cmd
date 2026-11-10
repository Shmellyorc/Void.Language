@echo off
set "work=C:\Users\a\VoidS7-Raylib-Graphics"
copy /y "\\vboxsvr\VM\slice7-raylib\audit-graphics.py" "%work%\audit-graphics.py"
"C:\Users\a\AppData\Local\Programs\Python\Python314\python.exe" "%work%\audit-graphics.py" > "\\vboxsvr\VM\slice7-raylib\graphics-audit.txt" 2>&1
