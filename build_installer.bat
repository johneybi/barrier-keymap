@echo off
set INNO_ROOT=

if exist "C:\Program Files (x86)\Inno Setup 5\ISCC.exe" set "INNO_ROOT=C:\Program Files (x86)\Inno Setup 5"
if "%INNO_ROOT%"=="" if exist "C:\Program Files (x86)\Inno Setup 6\ISCC.exe" set "INNO_ROOT=C:\Program Files (x86)\Inno Setup 6"
if "%INNO_ROOT%"=="" if exist "C:\Program Files\Inno Setup 6\ISCC.exe" set "INNO_ROOT=C:\Program Files\Inno Setup 6"

set savedir=%cd%
cd /d %~dp0

if not exist build\bin\Release goto buildproject

echo Building 64-bit Windows installer...

if "%INNO_ROOT%"=="" goto missinginno

cd build\installer-inno
if ERRORLEVEL 1 goto buildproject
"%INNO_ROOT%\ISCC.exe" /Qp barrier.iss
if ERRORLEVEL 1 goto failed

echo Build completed successfully
goto done

:buildproject
echo To build a 64-bit Windows installer:
echo  - set B_BUILD_TYPE=Release in build_env.bat
echo  - also set other environmental overrides necessary for your build environment
echo  - run clean_build.bat to build Barrier and verify that it succeeds
echo  - re-run this script to create the installation package
goto done

:missinginno
echo Inno Setup was not found.
echo Install Inno Setup 5 or 6, then re-run this script.
goto done

:failed
echo Build failed

:done
set INNO_ROOT=

cd /d %savedir%
set savedir=
