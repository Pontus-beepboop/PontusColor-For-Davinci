@echo off
REM  Installs the built bundle into the system OFX plugin folder.
REM  Right-click -> "Run as administrator".
setlocal
cd /d "%~dp0"

set DEST=C:\Program Files\Common Files\OFX\Plugins
if not exist "build\PontusColor.ofx.bundle" (
    echo Build first by running build_windows.bat
    exit /b 1
)
if not exist "%DEST%" mkdir "%DEST%"
if exist "%DEST%\PontusColor.ofx.bundle" rmdir /s /q "%DEST%\PontusColor.ofx.bundle"
xcopy /e /i /y "build\PontusColor.ofx.bundle" "%DEST%\PontusColor.ofx.bundle" >nul
echo Installed to %DEST%
echo Restart DaVinci Resolve, then open the Color page -^> Effects (OpenFX) -^> PontusColor.
endlocal
