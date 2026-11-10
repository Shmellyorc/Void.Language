@echo off
setlocal
set "G4SHARE=\\vboxsvr\VM\project-slice7-g4-checkpoint"
set "G4WORK=%USERPROFILE%\VoidS7-G4-checkpoint"
set "G4LLVM=%USERPROFILE%\VoidCI42377496702-Native\.toolchains\llvm-23.1.4\bin"
call "C:\BuildTools\VS2022\VC\Auxiliary\Build\vcvarsall.bat" x64 > "%G4SHARE%\process-default-vcvars.log" 2>&1
if errorlevel 1 exit /b 1
cd /d "%G4WORK%\redirection" || exit /b 1
"%G4LLVM%\clang-cl.exe" /nologo /TC windows-activation-edge.c /Fe:edge-process-default.exe /link /manifest:embed /manifestinput:override-dependency.manifest > "%G4SHARE%\process-default-build.log" 2>&1
if errorlevel 1 exit /b 1
edge-process-default.exe "%G4WORK%\good" "%G4WORK%\good\void_g4_probe_main.dll" "%G4WORK%\good\void_g4_probe_dep.dll" "%CD%\override-dependency.manifest" default > "%G4SHARE%\process-default-run.log" 2>&1
echo process-default=%errorlevel% > "%G4SHARE%\process-default-status.txt"
