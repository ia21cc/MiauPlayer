# Cambios

## 1.7.25
- Salida de audio desconectada (auriculares o Bluetooth apagados, dispositivo retirado): la pista que suena se pone en pausa en el mismo punto, se selecciona automaticamente la salida de audio predeterminada de Windows (y se guarda como salida elegida) y se muestra un aviso. Al pulsar Play continua desde donde estaba, sin tener que buscar la salida. Si la salida elegida era la predeterminada, solo reacciona cuando se desconecta el dispositivo que se estaba usando.

## 1.7.24
- Posicion de reproduccion tras cambiar la salida de audio (p. ej. se apaga el Bluetooth y Windows pasa el sonido a los parlantes): el contador del dispositivo podia saltar o no reiniciarse tras un seek, y al pulsar la linea de tiempo la posicion quedaba casi al final de la pista. Ahora la posicion se contrasta con lo escrito menos lo que sigue en cola y, si el contador se aparta mas de 0,4 s, se resincroniza.

## 1.7.23
- Corregido un cierre del programa al capturar un fotograma de videos cuyo ancho no es multiplo de 16 (p. ej. videos verticales de 1080 px de ancho): los buffers de imagen donde swscale convierte el cuadro no tenian margen y la conversion SIMD escribia unos bytes pasado el final (acceso invalido en swscale-10.dll). Ahora todos los buffers de conversion (captura, video en pantalla y reescalado) llevan relleno.

## 1.7.22
- Captura de fotograma reforzada: la conversion y el guardado del PNG se hacen en un hilo aparte (la ventana ya no se congela en videos grandes o verticales), se limita el tamano maximo de la imagen, se corrige una relacion de aspecto anomala y cualquier error de memoria se controla en vez de cerrar el programa. Solo se permite una captura a la vez.
- Registro de fallos: si el programa se cierra por un error, escribe el detalle (codigo, modulo, version) en %LOCALAPPDATA%\MiauPlayer\crash.log para poder diagnosticarlo.

## 1.7.21
- Controles multimedia del sistema (SMTC): MiauPlayer ahora crea una sesion de transporte multimedia de Windows, que es por donde llegan los botones de auriculares Bluetooth (siguiente, anterior, pausa, avance/retroceso), igual que hace Spotify. Antes solo usaba teclas globales, y Windows podia enviar el boton a otra app. Se carga de forma dinamica: si el sistema no lo admite, el programa sigue funcionando como antes con las teclas globales.
- Un mismo comando repetido en menos de 400 ms se ignora, para que un boton no se atienda dos veces por dos vias.

## 1.7.20
- Teclas multimedia (boton siguiente/anterior/pausa del auricular Bluetooth): ya no se sueltan al cambiar de pista ni al detenerse. Antes se desregistraban y volvian a registrar al pasar por "detenido" (y la comprobacion era periodica), y en ese hueco otra app (navegador, etc.) podia quedarse con la tecla. Ahora se registran en cuanto empieza la reproduccion, se mantienen hasta 10 minutos despues de detenerse, y las que otra app tuviera ocupadas se reintentan cada 2 s.

## 1.7.19
- Carga progresiva en vez de cargar la pista entera antes de sonar: la reproduccion empieza en cuanto el demuxer tiene lo que necesita (unos cientos de KB en audio, unos pocos MB en video) y el resto del archivo se va leyendo a memoria en segundo plano mientras suena. Si el indice del archivo esta lejos de lo cargado (p. ej. MP4 con el indice al final) se lee directamente esa zona sin esperar. Aplica a archivos locales de hasta 160 MB; los mayores y las rutas de red se abren como antes.

## 1.7.18
- Menos cortes de audio en equipos lentos: el colchon de audio pasa de 4 a 8 bloques por defecto. En el menu del engranaje, "Bufer de audio" permite elegir Normal (4), Grande (8, recomendado) o Muy grande (16).
- Mejor reproduccion desde Dropbox, pCloud, discos lentos o de red: la pista se lee completa a memoria antes de sonar (archivos de hasta 160 MB), asi la reproduccion y los saltos no dependen de lecturas durante la musica. Ademas se lee por adelantado la pista siguiente para que ya este descargada y en cache.
- Nueva opcion "Acerca de MiauPlayer" en el menu del engranaje: abre la misma ventana de informacion del boton de informacion.

## 1.7.17
- Icono de "Lista nueva" mas sencillo: hoja en blanco con la esquina doblada, sin el signo +.

## 1.7.16
- "Limpiar toda la lista" (menu contextual de la lista): si la lista actual es una lista guardada, ahora muestra una advertencia ("Esta es una lista guardada... Seguro que deseas vaciar la lista?") con "No" como opcion predeterminada. Si la lista no esta guardada, vacia sin preguntar como antes.

## 1.7.15
- Guardar: si la lista actual ya es una lista guardada (cargada desde "Listas guardadas" o guardada antes), el boton Guardar la actualiza directamente, sin preguntar si se desea reemplazarla. Solo se pide el nombre cuando la lista es nueva (y si ese nombre ya existe se sigue preguntando).
- Nuevo boton "Lista nueva" (hoja con +) a la izquierda de Importar: vacia la lista actual para empezar otra desde cero. Las listas guardadas no se tocan. Si la lista actual no estaba guardada pide confirmacion.

## 1.7.14
- Carpeta de las listas de reproduccion configurable: en el menu del engranaje, "Cambiar carpeta de listas..." permite elegir donde se guardan (por ejemplo dentro de Dropbox) y "Usar la carpeta de listas predeterminada" vuelve a la de siempre. Al cambiarla se ofrece copiar las listas actuales (sin pisar las que ya existan en el destino). Si la carpeta elegida no esta disponible (disco desconectado, Dropbox cerrado) se usa la predeterminada y se avisa. El ajuste es por usuario y por equipo.
- Las listas ya guardaban las rutas bajo la carpeta del usuario como %USERPROFILE%\..., asi que funcionan en otro equipo con la misma estructura (p. ej. Dropbox). Ahora tambien se convierten asi las listas que se importan.

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
