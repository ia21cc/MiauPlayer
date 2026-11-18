@echo off
rem Crea MiauPlayer-1.7.12.0-x64.msix (sin firmar) a partir de la carpeta "Paquete". Ese es el archivo que se sube a Partner Center.
setlocal
set "MAKEAPPX="
for /f "delims=" %%F in ('dir /b /s "%ProgramFiles(x86)%\Windows Kits\10\bin\makeappx.exe" 2^>nul ^| findstr /i "\\x64\\"') do set "MAKEAPPX=%%F"
if not defined MAKEAPPX (
  echo No se encontro makeappx.exe. Instala el "Windows SDK" ^(https://developer.microsoft.com/windows/downloads/windows-sdk/^) y vuelve a ejecutar.
  pause & exit /b 1
)
echo Usando: %MAKEAPPX%
if exist "%~dp0MiauPlayer-1.7.12.0-x64.msix" del "%~dp0MiauPlayer-1.7.12.0-x64.msix"
"%MAKEAPPX%" pack /d "%~dp0Paquete" /p "%~dp0MiauPlayer-1.7.12.0-x64.msix" /o
if errorlevel 1 ( echo ERROR al empaquetar. & pause & exit /b 1 )
echo.
echo Listo: MiauPlayer-1.7.12.0-x64.msix
pause
