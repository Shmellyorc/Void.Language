@echo off
copy /b /y "%USERPROFILE%\VoidS7-G3-Release\Testing\Temporary\LastTest.log" "\\vboxsvr\VM\project-slice7-g3\Release-LastTest.log" > nul || exit /b 1
copy /b /y "%USERPROFILE%\VoidS7-G3-Debug\Testing\Temporary\LastTest.log" "\\vboxsvr\VM\project-slice7-g3\Debug-LastTest.log" > nul || exit /b 1
