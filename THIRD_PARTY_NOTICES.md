# Avisos de terceros

## FFmpeg

MiauPlayer usa [FFmpeg](https://ffmpeg.org) para leer y decodificar audio y video.

- Build usado: `N-128837-g986bab2b47-20261112`, de [BtbN/FFmpeg-Builds](https://github.com/BtbN/FFmpeg-Builds), variante **win64-lgpl-shared**.
- Licencia: **GNU LGPL v3** (el texto, junto con la GPL v3 que incorpora por referencia, esta en `FFMPEG-LICENSE.txt`).
- FFmpeg se enlaza **dinamicamente**: las DLL (`avcodec`, `avformat`, `avfilter`, `avutil`, `swresample`, `swscale`) viajan
  aparte y sin modificar junto a `MiauPlayer.exe`. Se pueden reemplazar por otra version compatible de FFmpeg copiandolas
  en la carpeta del programa.
- Codigo fuente de FFmpeg: <https://ffmpeg.org/download.html>
- Importante: al compilar, usa el build **lgpl**. El build `gpl` obliga a distribuir todo el programa bajo GPL, y los builds
  `nonfree` no se pueden redistribuir.

FFmpeg es una marca registrada de Fabrice Bellard, creador del proyecto.
