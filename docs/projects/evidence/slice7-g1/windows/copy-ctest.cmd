@echo off
copy /b /y "%USERPROFILE%\VoidS7-G1-Release\Testing\Temporary\LastTest.log" "\\vboxsvr\VM\project-slice7-g1\Release-LastTest.log" > nul || exit /b 1
copy /b /y "%USERPROFILE%\VoidS7-G1-Debug\Testing\Temporary\LastTest.log" "\\vboxsvr\VM\project-slice7-g1\Debug-LastTest.log" > nul || exit /b 1
