@echo off
rem Compila MiauPlayer y genera el instalador. Requiere MinGW-w64 (g++, windres) y NSIS (makensis) en el PATH.
rem Define FFMPEG con la carpeta del build "win64-lgpl-shared" de BtbN (la que tiene bin, include y lib).
setlocal
if "%FFMPEG%"=="" (
  echo Define la variable FFMPEG, por ejemplo:  set FFMPEG=C:\ffmpeg-master-latest-win64-lgpl-shared
  exit /b 1
)
windres app.rc -O coff -o app_res.o || exit /b 1
g++ -std=c++17 -O2 -DUNICODE -D_UNICODE -I"%FFMPEG%\include" MiauPlayer.cpp app_res.o -o MiauPlayer.exe ^
    -L"%FFMPEG%\lib" -lavformat -lavcodec -lavfilter -lswresample -lswscale -lavutil ^
    -mwindows -municode -static -lgdiplus -lcomctl32 -lwinmm -ldwmapi -lole32 -loleaut32 ^
    -lcomdlg32 -lshell32 -lshlwapi -luuid -lgdi32 -luser32 -lmsimg32 -ladvapi32 || exit /b 1
if exist dist rmdir /s /q dist
mkdir dist
copy /y MiauPlayer.exe dist\ >nul
copy /y "%FFMPEG%\bin\*.dll" dist\ >nul
del /q dist\avdevice-*.dll 2>nul
copy /y FFMPEG-LICENSE.txt dist\ >nul
copy /y LEEME.txt dist\ >nul
makensis MiauPlayer.nsi
