# Cambios

## 1.7.9
- Preparacion para Microsoft Store (MSIX): si el programa corre como paquete MSIX no pregunta por asociar archivos (lo declara el manifiesto), "Asociar tipos de archivo" abre la pagina de apps predeterminadas de Windows y las playlists se guardan en Documentos\MiauPlayer\playlist (la carpeta del paquete es de solo lectura). Sin cambios para el instalador normal.
- Anadido PRIVACY.md (politica de privacidad).

## 1.7.8
- Estirar la ventana con un video: ya no se muestran las listas de reproduccion (ocultas en modo video) encima de los demas controles. Antes, cada cambio de tamano mostraba todos los controles y volvia a ocultar algunos; ahora solo se muestran todos al salir de pantalla completa.

## 1.7.7
- Al terminar el ultimo video de la cola, el ultimo fotograma se queda en pantalla (antes volvia a la vista normal).
- Estirar la ventana: los controles se mueven sin copiar pixeles viejos y, mientras dura el arrastre, la ventana se pinta en un solo bufer (WS_EX_COMPOSITED) para evitar los cuadros blancos, tambien con video.

## 1.7.6
- "Abrir ubicacion del archivo" selecciona la pista tambien la primera vez (usa la API del shell en lugar de lanzar explorer.exe).
- Las marcas verticales del volumen se dibujan encima de la barra; las de la ganancia siguen debajo.

## 1.7.5
- "Informacion de la pista" muestra tambien la profundidad de bits (16, 24, 32...; "no aplica" en formatos con perdida como MP3).
- Los avisos de la opcion de pista ya no llevan icono, asi Windows no reproduce el sonido de alerta.

## 1.7.4
- Clic derecho sobre una pista de la lista: nuevas opciones "Abrir ubicacion del archivo" (abre la carpeta con el archivo seleccionado) e "Informacion de la pista" (tipo de archivo y codecs, bitrate y frecuencia de muestreo).
- Volumen: un clic en la linea va a la marca mas cercana (como la ganancia), en lugar de saltar dos marcas.
- Las marcas verticales del volumen y de la ganancia usan el color de su perilla (blanco y rosado), para diferenciarlas.

## 1.7.3
- Los archivos asociados a MiauPlayer (audio y video) muestran su propio icono (`icono_archivo.ico`, incrustado en el ejecutable) en lugar del icono de la aplicacion.

## 1.7.2
- Corregido un fallo que cerraba el programa al arrancar en la 1.7.1 (desbordamiento de pila al crear la lista de reproduccion).

## 1.7.1
- FFmpeg pasa al build LGPL (`win64-lgpl-shared`), con DLL intercambiables y avisos de licencia.
- Corregido: cuadros blancos en los botones al estirar la ventana.
- Playlist y listas guardadas: se pueden reordenar arrastrando con el mouse (el orden de las guardadas se conserva en `playlist\orden.lst`).
- Rueda del mouse sobre Volumen y Ganancia: hacia arriba sube, hacia abajo baja.
- Ganancia: un clic en la linea va a la marca mas cercana.
- Nuevo menu Configuracion (engranaje): idioma (espanol / English / Portugues) y "Asociar tipos de archivo".
- Instalador: ya no pregunta el idioma (usa el de Windows) y avisa que se cambia en Configuracion.
- Instalacion silenciosa (`/S`): no asocia tipos de archivo; el programa lo ofrece la primera vez que se abre.
