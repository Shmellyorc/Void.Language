@echo off
copy /b /y "%USERPROFILE%\VoidS7-G3-concurrency\link.rsp" "\\vboxsvr\VM\project-slice7-g3\diagnostic-link.rsp" > nul || exit /b 1
copy /b /y "%USERPROFILE%\VoidS7-G3-concurrency\CTestTestfile.cmake" "\\vboxsvr\VM\project-slice7-g3\diagnostic-CTestTestfile.cmake" > nul || exit /b 1
