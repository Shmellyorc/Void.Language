@echo off
setlocal
set "G4SHARE=\\vboxsvr\VM\project-slice7-g4-checkpoint"
set "G4WORK=%USERPROFILE%\VoidS7-G4-checkpoint"
set "G4LLVM=%USERPROFILE%\VoidCI42377496702-Native\.toolchains\llvm-23.1.4\bin"
call "C:\BuildTools\VS2022\VC\Auxiliary\Build\vcvarsall.bat" x64 > "%G4SHARE%\redirection-vcvars.log" 2>&1
if errorlevel 1 exit /b 1
if exist "%G4WORK%\redirection" exit /b 2
mkdir "%G4WORK%\redirection" || exit /b 1
for %%F in (redirected.c windows-redirection.c) do copy /b "%G4SHARE%\%%F" "%G4WORK%\redirection\%%F" >nul || exit /b 1
cd /d "%G4WORK%\redirection" || exit /b 1
(
  "%G4LLVM%\clang-cl.exe" /nologo /TC /c /GS- /Zl redirected.c /Foredirected.obj || exit /b 1
  "%G4LLVM%\lld-link.exe" /dll /entry:checkpoint_attach /nodefaultlib /out:void_g4_probe_main.dll redirected.obj || exit /b 1
  "%G4LLVM%\clang-cl.exe" /nologo /TC windows-redirection.c /Fe:bare.exe /link /manifest:no || exit /b 1
  "%G4LLVM%\clang-cl.exe" /nologo /TC windows-redirection.c /Fe:manifested.exe /link /manifest:embed || exit /b 1
) > "%G4SHARE%\redirection-build.log" 2>&1
reg query "HKLM\SOFTWARE\Microsoft\Windows NT\CurrentVersion\Image File Execution Options" /v DevOverrideEnable > "%G4SHARE%\redirection-registry.log" 2>&1
bare.exe "%G4WORK%\good" "%G4WORK%\good\void_g4_probe_main.dll" "%G4WORK%\good\void_g4_probe_dep.dll" > "%G4SHARE%\redirection-baseline.log" 2>&1
set "G4BASELINE=%errorlevel%"
type nul > bare.exe.local
type nul > manifested.exe.local
bare.exe "%G4WORK%\good" "%G4WORK%\good\void_g4_probe_main.dll" "%G4WORK%\good\void_g4_probe_dep.dll" > "%G4SHARE%\redirection-bare.log" 2>&1
set "G4BARE=%errorlevel%"
manifested.exe "%G4WORK%\good" "%G4WORK%\good\void_g4_probe_main.dll" "%G4WORK%\good\void_g4_probe_dep.dll" > "%G4SHARE%\redirection-manifested.log" 2>&1
set "G4MANIFESTED=%errorlevel%"
echo baseline=%G4BASELINE% bare=%G4BARE% manifested=%G4MANIFESTED% > "%G4SHARE%\redirection-status.txt"
