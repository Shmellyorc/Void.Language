@echo off
setlocal
set "G4SHARE=\\vboxsvr\VM\project-slice7-g4-checkpoint"
set "G4WORK=%USERPROFILE%\VoidS7-G4-checkpoint"
set "G4LLVM=%USERPROFILE%\VoidCI42377496702-Native\.toolchains\llvm-23.1.4\bin"
call "C:\BuildTools\VS2022\VC\Auxiliary\Build\vcvarsall.bat" x64 > "%G4SHARE%\edge-vcvars.log" 2>&1
if errorlevel 1 exit /b 1
cd /d "%G4WORK%\redirection" || exit /b 1
for %%F in (windows-activation-edge.c override-dependency.manifest windows-mapping.c) do copy /b "%G4SHARE%\%%F" "%%F" >nul || exit /b 1
(
  "%G4LLVM%\lld-link.exe" /dll /entry:checkpoint_attach /nodefaultlib /out:void_g4_probe_dep.dll redirected.obj ..\competitor.obj || exit /b 1
  "%G4LLVM%\clang-cl.exe" /nologo /TC windows-activation-edge.c /Fe:edge.exe /link /manifest:embed || exit /b 1
) > "%G4SHARE%\edge-build.log" 2>&1
edge.exe "%G4WORK%\good" "%G4WORK%\good\void_g4_probe_main.dll" "%G4WORK%\good\void_g4_probe_dep.dll" "%CD%\override-dependency.manifest" > "%G4SHARE%\edge-override.log" 2>&1
set "G4OVERRIDE=%errorlevel%"
edge.exe "%G4WORK%\good" "%G4WORK%\good\void_g4_probe_main.dll" "%G4WORK%\good\void_g4_probe_dep.dll" "%CD%\override-dependency.manifest" default > "%G4SHARE%\edge-default.log" 2>&1
set "G4DEFAULT=%errorlevel%"
echo override=%G4OVERRIDE% default=%G4DEFAULT% > "%G4SHARE%\edge-status.txt"
