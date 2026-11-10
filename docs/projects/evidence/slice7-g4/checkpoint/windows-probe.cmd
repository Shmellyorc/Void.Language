@echo off
setlocal
set "G4SHARE=\\vboxsvr\VM\project-slice7-g4-checkpoint"
set "G4WORK=%USERPROFILE%\VoidS7-G4-checkpoint"
set "G4LLVM=%USERPROFILE%\VoidCI42377496702-Native\.toolchains\llvm-23.1.4\bin"
call "C:\BuildTools\VS2022\VC\Auxiliary\Build\vcvarsall.bat" x64 > "%G4SHARE%\vcvars.log" 2>&1
if errorlevel 1 exit /b 1
if exist "%G4WORK%" exit /b 2
mkdir "%G4WORK%\good" "%G4WORK%\bad" "%G4WORK%\replacement" || exit /b 1
for %%F in (dependency.c provider.c windows-mapping.c) do copy /b "%G4SHARE%\%%F" "%G4WORK%\%%F" >nul || exit /b 1
cd /d "%G4WORK%" || exit /b 1
(
  "%G4LLVM%\clang-cl.exe" /nologo /TC /c /GS- /Zl dependency.c /Fodependency.obj || exit /b 1
  "%G4LLVM%\clang-cl.exe" /nologo /TC /c /GS- /Zl /DFIXTURE_VALUE=99 dependency.c /Focompetitor.obj || exit /b 1
  "%G4LLVM%\lld-link.exe" /dll /noentry /nodefaultlib /out:good\void_g4_probe_dep.dll /implib:dep.lib dependency.obj || exit /b 1
  "%G4LLVM%\lld-link.exe" /dll /noentry /nodefaultlib /out:bad\void_g4_probe_dep.dll competitor.obj || exit /b 1
  copy /b bad\void_g4_probe_dep.dll replacement\void_g4_probe_dep.dll || exit /b 1
  "%G4LLVM%\clang-cl.exe" /nologo /TC /c /GS- /Zl provider.c /Foprovider.obj || exit /b 1
  "%G4LLVM%\lld-link.exe" /dll /noentry /nodefaultlib /out:good\void_g4_probe_main.dll provider.obj dep.lib || exit /b 1
  "%G4LLVM%\clang-cl.exe" /nologo /TC windows-mapping.c /Fe:probe.exe /link /subsystem:console || exit /b 1
  "%G4LLVM%\llvm-readobj.exe" --coff-imports --coff-exports good\void_g4_probe_main.dll || exit /b 1
) > "%G4SHARE%\windows-build.log" 2>&1
probe.exe clean "%G4WORK%\good" "%G4WORK%\good\void_g4_probe_main.dll" "%G4WORK%\bad" > "%G4SHARE%\windows-clean.log" 2>&1
set "G4CLEAN=%errorlevel%"
probe.exe collision "%G4WORK%\good" "%G4WORK%\good\void_g4_probe_main.dll" "%G4WORK%\bad" > "%G4SHARE%\windows-collision.log" 2>&1
set "G4COLLISION=%errorlevel%"
probe.exe replace "%G4WORK%\good" "%G4WORK%\good\void_g4_probe_main.dll" "%G4WORK%\replacement" > "%G4SHARE%\windows-replace.log" 2>&1
set "G4REPLACE=%errorlevel%"
echo clean=%G4CLEAN% collision=%G4COLLISION% replace=%G4REPLACE% > "%G4SHARE%\windows-status.txt"
