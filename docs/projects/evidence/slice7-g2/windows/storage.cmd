@echo off
C:\Windows\System32\WindowsPowerShell\v1.0\powershell.exe -NoProfile -NonInteractive -Command "$p='\\vboxsvr\VM\project-slice7-g2\storage.ps1'; Invoke-Expression ([IO.File]::ReadAllText($p))" > "\\vboxsvr\VM\project-slice7-g2\storage-console.txt" 2>&1
