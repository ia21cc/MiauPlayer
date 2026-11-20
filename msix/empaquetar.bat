@echo off
rem Crea MiauPlayer-1.7.13.0-x64.msix (sin firmar) a partir de la carpeta "Paquete". Ese es el archivo que se sube a Partner Center.
rem Antes genera resources.pri (makepri): sin ese indice Windows ignora las variantes de icono (targetsize / altform-unplated)
rem y dibuja el icono pequeno sobre una placa azul en la barra de tareas.
setlocal
set "MAKEAPPX="
set "MAKEPRI="
for /f "delims=" %%F in ('dir /b /s "%ProgramFiles(x86)%\Windows Kits\10\bin\makeappx.exe" 2^>nul ^| findstr /i "\\x64\\"') do set "MAKEAPPX=%%F"
for /f "delims=" %%F in ('dir /b /s "%ProgramFiles(x86)%\Windows Kits\10\bin\makepri.exe" 2^>nul ^| findstr /i "\\x64\\"') do set "MAKEPRI=%%F"
if not defined MAKEAPPX (
  echo No se encontro makeappx.exe. Instala el "Windows SDK" ^(https://developer.microsoft.com/windows/downloads/windows-sdk/^) y vuelve a ejecutar.
  pause & exit /b 1
)
echo Usando: %MAKEAPPX%
if defined MAKEPRI (
  echo Generando resources.pri con: %MAKEPRI%
  if exist "%~dp0Paquete\resources.pri" del "%~dp0Paquete\resources.pri"
  "%MAKEPRI%" createconfig /cf "%TEMP%\miau-priconfig.xml" /dq es-ES /pv 10.0.0 /o
  if errorlevel 1 ( echo ERROR en makepri createconfig. & pause & exit /b 1 )
  "%MAKEPRI%" new /pr "%~dp0Paquete" /cf "%TEMP%\miau-priconfig.xml" /mn "%~dp0Paquete\AppxManifest.xml" /of "%~dp0Paquete\resources.pri" /o
  if errorlevel 1 ( echo ERROR en makepri new. & pause & exit /b 1 )
) else (
  echo AVISO: no se encontro makepri.exe; se empaqueta sin resources.pri ^(los iconos de la barra de tareas saldran con placa^).
)
if exist "%~dp0MiauPlayer-1.7.13.0-x64.msix" del "%~dp0MiauPlayer-1.7.13.0-x64.msix"
"%MAKEAPPX%" pack /d "%~dp0Paquete" /p "%~dp0MiauPlayer-1.7.13.0-x64.msix" /o
if errorlevel 1 ( echo ERROR al empaquetar. & pause & exit /b 1 )
echo.
echo Listo: MiauPlayer-1.7.13.0-x64.msix
pause
