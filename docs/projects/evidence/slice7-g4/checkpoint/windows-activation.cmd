@echo off
setlocal
set "G4SHARE=\\vboxsvr\VM\project-slice7-g4-checkpoint"
set "G4WORK=%USERPROFILE%\VoidS7-G4-checkpoint"
set "G4LLVM=%USERPROFILE%\VoidCI42377496702-Native\.toolchains\llvm-23.1.4\bin"
call "C:\BuildTools\VS2022\VC\Auxiliary\Build\vcvarsall.bat" x64 > "%G4SHARE%\activation-vcvars.log" 2>&1
if errorlevel 1 exit /b 1
cd /d "%G4WORK%\redirection" || exit /b 1
for %%F in (windows-activation.c override.manifest) do copy /b "%G4SHARE%\%%F" "%%F" >nul || exit /b 1
"%G4LLVM%\clang-cl.exe" /nologo /TC windows-activation.c /Fe:activation.exe /link /manifest:embed > "%G4SHARE%\activation-build.log" 2>&1
if errorlevel 1 exit /b 1
activation.exe "%G4WORK%\good" "%G4WORK%\good\void_g4_probe_main.dll" "%G4WORK%\good\void_g4_probe_dep.dll" "%CD%\override.manifest" > "%G4SHARE%\activation-override.log" 2>&1
set "G4OVERRIDE=%errorlevel%"
activation.exe "%G4WORK%\good" "%G4WORK%\good\void_g4_probe_main.dll" "%G4WORK%\good\void_g4_probe_dep.dll" "%CD%\override.manifest" default > "%G4SHARE%\activation-default.log" 2>&1
set "G4DEFAULT=%errorlevel%"
echo override=%G4OVERRIDE% default=%G4DEFAULT% > "%G4SHARE%\activation-status.txt"
