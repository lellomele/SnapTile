@echo off
setlocal EnableExtensions
pushd "%~dp0" || exit /b 1
set "GCCBIN="
if defined MINGW_HOME if exist "%MINGW_HOME%\bin\gcc.exe" set "GCCBIN=%MINGW_HOME%\bin"
if not defined GCCBIN if exist "C:\mingw\bin\gcc.exe" set "GCCBIN=C:\mingw\bin"
if not defined GCCBIN if exist "C:\mingw64\bin\gcc.exe" set "GCCBIN=C:\mingw64\bin"
if not defined GCCBIN for %%G in (gcc.exe) do if not "%%~$PATH:G"=="" set "GCCBIN=%%~dp$PATH:G"
if not defined GCCBIN goto :manca
if not exist "%GCCBIN%\windres.exe" goto :manca
set "PATH=%GCCBIN%;%PATH%"
if not exist build mkdir build
if not exist build goto :errore
"%GCCBIN%\windres.exe" -I risorse -i risorse\affianca.rc -O coff -o build\risorse.o
if errorlevel 1 goto :errore
"%GCCBIN%\gcc.exe" -std=c11 -Os -Wall -Wextra -Wpedantic -D_WIN32_WINNT=0x0A00 -DUNICODE -D_UNICODE -ffunction-sections -fdata-sections -mwindows -nostartfiles -static-libgcc -Wl,-e,avvia_affianca,--gc-sections -s -o build\SnapTile.exe src\principale.c src\geometria.c src\finestre.c src\avvio.c build\risorse.o -luser32 -lgdi32 -lshell32 -ladvapi32 -ldwmapi -lshcore
if errorlevel 1 goto :errore
copy /y "build\SnapTile.exe" "SnapTile.exe" >nul
if errorlevel 1 goto :errore
echo Built SnapTile.exe with %GCCBIN%.
if /i "%~1"=="prova" goto :prova
popd
exit /b 0
:prova
"%GCCBIN%\gcc.exe" -std=c11 -Os -Wall -Wextra -Wpedantic -D_WIN32_WINNT=0x0A00 -DUNICODE -D_UNICODE -o build\prova.exe verifica\prova.c src\geometria.c src\finestre.c src\avvio.c -lgdi32 -lshell32 -ladvapi32 -ldwmapi -lshcore
if errorlevel 1 goto :errore
"build\prova.exe"
if errorlevel 1 goto :errore
popd
exit /b 0
:manca
echo MinGW-w64 with gcc.exe and windres.exe is required. Set MINGW_HOME to its installation directory.
:errore
popd
exit /b 1
