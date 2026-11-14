# MiauPlayer

Reproductor de audio y video minimalista para Windows, con forma de onda, ganancia hasta +10 dB, ecualizador de 3 bandas,
limitador suave, control de velocidad, captura de fotogramas, playlists guardadas y soporte de rutas de red (RTSP, HTTP, RTMP...).
Interfaz en espanol, ingles y portugues.

- C++17, Win32 + GDI+ (interfaz dibujada a medida), sin dependencias graficas externas.
- Audio y video con [FFmpeg](https://ffmpeg.org) (enlace dinamico, build LGPL).
- Instalador con NSIS.

## Compilar

Lee [COMPILAR.txt](COMPILAR.txt). Resumen: instala MinGW-w64 y NSIS, descarga el build **win64-lgpl-shared** de
[BtbN](https://github.com/BtbN/FFmpeg-Builds/releases), define `FFMPEG` con su carpeta y ejecuta `build.bat`.

## Licencias

FFmpeg se distribuye bajo LGPL v3; ver [THIRD_PARTY_NOTICES.md](THIRD_PARTY_NOTICES.md) y `FFMPEG-LICENSE.txt`.
MiauPlayer: MIT (ver [LICENSE](LICENSE)).
