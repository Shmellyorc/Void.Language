@echo off
set "exchange=\\vboxsvr\VM\g4-conventional\diagnostic"
set "localroot=C:\Users\a\VoidS7-G4Conventional"
copy /y "%exchange%\inspect-native.py" "%localroot%\inspect-native.py"
"C:\Users\a\AppData\Local\Programs\Python\Python314\python.exe" "%localroot%\inspect-native.py" > "%exchange%\inspect-native-console.txt" 2>&1
