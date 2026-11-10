@echo off
call "C:\BuildTools\VS2022\VC\Auxiliary\Build\vcvarsall.bat" x64 > "\\vboxsvr\VM\project-slice2-verified\vcvars.log" 2>&1
"C:\Users\a\AppData\Local\Programs\Python\Python314\python.exe" "\\vboxsvr\VM\project-slice2-verified\native.py" > "\\vboxsvr\VM\project-slice2-verified\native-console.log" 2>&1
