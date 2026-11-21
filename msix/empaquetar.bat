@echo off
rem Crea MiauPlayer-1.7.17.0-x64.msix (sin firmar) a partir de la carpeta "Paquete". Ese es el archivo que se sube a Partner Center.
rem Antes genera resources.pri (makepri): sin ese indice Windows ignora las variantes de icono (targetsize / altform-unplated)
rem y dibuja el icono pequeno sobre una placa azul en la barra de tareas.
setlocal
set "MAKEAPPX="
set "MAKEPRI="
for /f "delims=" %%F in ('dir /b /s "%ProgramFiles(x86)%\Windows Kits\10\bin\makeappx.exe" 2^>nul ^| findstr /i "\\x64\\"') do set "MAKEAPPX=%%F"
for /f "delims=" %%F in ('dir /b /s "%ProgramFiles(x86)%\Windows Kits\10\bin\makepri.exe" 2^>nul ^| findstr /i "\\x64\\"') do set "MAKEPRI=%%F"
if not defined MAKEAPPX goto :nosdk
echo Usando makeappx: "%MAKEAPPX%"
if not defined MAKEPRI goto :sinpri
echo Usando makepri: "%MAKEPRI%"
if exist "%~dp0Paquete\resources.pri" del "%~dp0Paquete\resources.pri"
"%MAKEPRI%" createconfig /cf "%TEMP%\miau-priconfig.xml" /dq es-ES /pv 10.0.0 /o
if errorlevel 1 goto :errpri
"%MAKEPRI%" new /pr "%~dp0Paquete" /cf "%TEMP%\miau-priconfig.xml" /mn "%~dp0Paquete\AppxManifest.xml" /of "%~dp0Paquete\resources.pri" /o
if errorlevel 1 goto :errpri
goto :empaquetar

:sinpri
echo AVISO: no se encontro makepri.exe. Se empaqueta sin resources.pri y los iconos de la barra de tareas saldran con placa.

:empaquetar
if exist "%~dp0MiauPlayer-1.7.17.0-x64.msix" del "%~dp0MiauPlayer-1.7.17.0-x64.msix"
"%MAKEAPPX%" pack /d "%~dp0Paquete" /p "%~dp0MiauPlayer-1.7.17.0-x64.msix" /o
if errorlevel 1 goto :errpack
echo.
echo Listo: MiauPlayer-1.7.17.0-x64.msix
pause
exit /b 0

:nosdk
echo No se encontro makeappx.exe. Instala el "Windows SDK" y vuelve a ejecutar.
pause
exit /b 1

:errpri
echo ERROR al generar resources.pri con makepri. Copia el mensaje de arriba y pasaselo a Claude.
pause
exit /b 1

:errpack
echo ERROR al empaquetar con makeappx. Copia el mensaje de arriba y pasaselo a Claude.
pause
exit /b 1
