# Cambios

## 1.7.13
- Paquete MSIX (Microsoft Store): el menu contextual "Anadir a la lista de MiauPlayer" ahora es un comando de shell (IExplorerCommand, MiauShell.dll) que recibe toda la seleccion de una vez, asi que ya no hay limite de 15 archivos. Si MiauPlayer no esta abierto, lo inicia con la primera pista y envia el resto. El instalador normal no cambia (ya no tenia ese limite). Se conserva un manifiesto de respaldo con los verbos clasicos.
- Paquete MSIX: el empaquetado genera ahora resources.pri (makepri) e incluye mas tamanos de icono sin placa, para que el icono de la barra de tareas ocupe todo el espacio en vez de verse pequeno sobre un cuadro azul.
- Paquete MSIX: el texto "Abrir con MiauPlayer" del menu contextual se traduce segun el idioma de Windows (espanol, ingles, portugues) mediante Strings\<idioma>\Resources.resw. En el instalador normal ya estaba traducido.

## 1.7.12
- Mas espacio para el video: la interfaz es mas compacta en altura (barra de titulo, franja de botones, filas de volumen y ganancia, ecualizador y forma de onda). No se quito ninguna opcion; solo se redujeron alturas de filas, sliders e iconos. La misma presentacion mas baja se ve tambien al reproducir audio. La altura minima de la ventana baja de 600 a 560 y la inicial de 660 a 590.
- Informacion de la pista (clic derecho): ahora muestra el tamano del archivo (y los bytes exactos) y la ruta completa. La ruta se parte en varias lineas, preferentemente tras una barra, para que la ventana no se ensanche.

## 1.7.11
- ReplayGain: MiauPlayer respeta la ganancia de normalizacion que traen las etiquetas del archivo (replaygain_track_gain / replaygain_album_gain y sus picos, en ID3, APE, Vorbis y MP4). Nuevo submenu "ReplayGain" en el engranaje: Desactivado / Pista (por defecto) / Album. Si la ganancia es positiva y el archivo trae pico, no se supera 0 dBFS. La informacion de la pista muestra los valores leidos. Nota: MP3Gain en su modo normal ya modifica el audio del MP3 y se oye sin necesitar esto; la funcion cubre las herramientas que solo escriben etiquetas.

## 1.7.10
- Paquete MSIX (Microsoft Store): el manifiesto declara los verbos "Abrir" y "Anadir a la lista de MiauPlayer" para cada tipo de archivo asociado, para que el menu contextual de Windows ofrezca anadir pistas a la lista, como hace el instalador normal. El verbo "Anadir a la lista" usa MultiSelectModel="Document": con varias pistas seleccionadas, Windows lanza una vez el verbo por cada pista y la instancia unica las va sumando a la lista.

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
