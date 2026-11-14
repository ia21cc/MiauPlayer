// MiauPlayer - reproductor de audio/video minimalista: forma de onda, volumen y ganancia hasta +10 dB.
// Win32 + FFmpeg (libavformat/libavcodec/libswresample/libswscale), salida de audio por waveOut.
#ifndef UNICODE
#define UNICODE
#endif
#ifndef _UNICODE
#define _UNICODE
#endif
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <windowsx.h>
#include <ole2.h>
#include <mmsystem.h>
#include <dwmapi.h>
#include <gdiplus.h>
#include <commctrl.h>
#include <commdlg.h>
#include <shellapi.h>
#include <shlobj.h>
#include <mmdeviceapi.h>
#include <atomic>
#include <string>
#include <vector>
#include <algorithm>
#include <unordered_map>
#include <initializer_list>
#include <cmath>
#include <cstdio>

extern "C" {
#include <libavformat/avformat.h>
#include <libavcodec/avcodec.h>
#include <libswresample/swresample.h>
#include <libswscale/swscale.h>
#include <libavutil/channel_layout.h>
#include <libavutil/imgutils.h>
#include <libavutil/opt.h>
#include <libavutil/avutil.h>
#include <libavutil/frame.h>
#include <libavutil/samplefmt.h>
#include <libavfilter/avfilter.h>
#include <libavfilter/buffersrc.h>
#include <libavfilter/buffersink.h>
}

// ------------------------------------------------------------ utilidades
static std::string utf8(const std::wstring& w) {
    if (w.empty()) return std::string();
    int n = WideCharToMultiByte(CP_UTF8, 0, w.c_str(), (int)w.size(), nullptr, 0, nullptr, nullptr);
    std::string s(n, 0);
    WideCharToMultiByte(CP_UTF8, 0, w.c_str(), (int)w.size(), &s[0], n, nullptr, nullptr);
    return s;
}

static std::wstring fmtTime(double s) {
    if (s < 0 || !std::isfinite(s)) s = 0;
    int t = (int)(s + 0.5);
    wchar_t b[32];
    if (t >= 3600) swprintf(b, 32, L"%d:%02d:%02d", t / 3600, (t / 60) % 60, t % 60);
    else swprintf(b, 32, L"%d:%02d", t / 60, t % 60);
    return b;
}

struct Lock {
    CRITICAL_SECTION* c;
    explicit Lock(CRITICAL_SECTION* cs) : c(cs) { EnterCriticalSection(c); }
    ~Lock() { LeaveCriticalSection(c); }
};

// ------------------------------------------------------------ idiomas (es / en / pt)
// El texto base del programa esta en espanol; TX("texto en espanol") devuelve su traduccion segun el idioma elegido.
enum { LNG_ES = 0, LNG_EN = 1, LNG_PT = 2 };
static int g_lang = LNG_ES;
struct TrEntry { const wchar_t* es; const wchar_t* en; const wchar_t* pt; };
static const TrEntry kTr[] = {
    { L"Elige una carpeta de música",
      L"Choose a music folder",
      L"Escolha uma pasta de m\u00fasica" },
    { L"Aceptar",
      L"OK",
      L"OK" },
    { L"Cancelar",
      L"Cancel",
      L"Cancelar" },
    { L"No se pudo leer el archivo de la playlist.",
      L"The playlist file could not be read.",
      L"N\u00e3o foi poss\u00edvel ler o arquivo da playlist." },
    { L"No se encontr\u00f3 ninguno de los archivos de esta playlist (\u00bfse movieron o borraron?).",
      L"None of this playlist's files were found (were they moved or deleted?).",
      L"Nenhum dos arquivos desta playlist foi encontrado (foram movidos ou apagados?)." },
    { L" archivo(s) de la playlist no se encontraron)",
      L" file(s) from the playlist were not found)",
      L" arquivo(s) da playlist n\u00e3o foram encontrados)" },
    { L"Abrir ruta de red",
      L"Open network stream",
      L"Abrir transmiss\u00e3o de rede" },
    { L"Direcci\u00f3n de la transmisi\u00f3n (rtsp://, rtsps://, http://, https://, rtmp://, udp://\u2026):\r\n",
      L"Stream address (rtsp://, rtsps://, http://, https://, rtmp://, udp://\u2026):\r\n",
      L"Endere\u00e7o da transmiss\u00e3o (rtsp://, rtsps://, http://, https://, rtmp://, udp://\u2026):\r\n" },
    { L"Ejemplo: rtsp://usuario:clave@192.168.1.10/stream",
      L"Example: rtsp://user:password@192.168.1.10/stream",
      L"Exemplo: rtsp://usuario:senha@192.168.1.10/stream" },
    { L"Reproducir",
      L"Play",
      L"Reproduzir" },
    { L"La direcci\u00f3n debe empezar con el protocolo, por ejemplo rtsp://, http://, https:// o rtmp://",
      L"The address must start with the protocol, for example rtsp://, http://, https:// or rtmp://",
      L"O endere\u00e7o deve come\u00e7ar com o protocolo, por exemplo rtsp://, http://, https:// ou rtmp://" },
    { L"La lista actual est\u00e1 vac\u00eda. Agrega canciones y vuelve a guardar.",
      L"The current list is empty. Add some songs and save again.",
      L"A lista atual est\u00e1 vazia. Adicione m\u00fasicas e salve novamente." },
    { L"Guardar playlist",
      L"Save playlist",
      L"Salvar playlist" },
    { L"Nombre de la playlist:",
      L"Playlist name:",
      L"Nome da playlist:" },
    { L"El nombre no es v\u00e1lido.",
      L"The name is not valid.",
      L"O nome n\u00e3o \u00e9 v\u00e1lido." },
    { L"Ya existe una playlist con ese nombre. \u00bfReemplazarla?",
      L"A playlist with that name already exists. Replace it?",
      L"J\u00e1 existe uma playlist com esse nome. Deseja substitu\u00ed-la?" },
    { L"No se pudo guardar el archivo en:\n",
      L"The file could not be saved to:\n",
      L"N\u00e3o foi poss\u00edvel salvar o arquivo em:\n" },
    { L"Importar playlist (.txt)",
      L"Import playlist (.txt)",
      L"Importar playlist (.txt)" },
    { L"El archivo no contiene rutas de canciones (una por l\u00ednea).",
      L"The file contains no song paths (one per line).",
      L"O arquivo n\u00e3o cont\u00e9m caminhos de m\u00fasicas (um por linha)." },
    { L"Importada",
      L"Imported",
      L"Importada" },
    { L"Ya existe una playlist llamada \u201c",
      L"A playlist named \u201c",
      L"J\u00e1 existe uma playlist chamada \u201c" },
    { L"\u201d. \u00bfReemplazarla?",
      L"\u201d already exists. Replace it?",
      L"\u201d. Deseja substitu\u00ed-la?" },
    { L"La lista actual est\u00e1 vac\u00eda, no hay nada que exportar.",
      L"The current list is empty, there is nothing to export.",
      L"A lista atual est\u00e1 vazia, n\u00e3o h\u00e1 nada para exportar." },
    { L"Exportar la lista actual a .txt",
      L"Export the current list to .txt",
      L"Exportar a lista atual para .txt" },
    { L"No se pudo exportar el archivo.",
      L"The file could not be exported.",
      L"N\u00e3o foi poss\u00edvel exportar o arquivo." },
    { L"Exportar playlist",
      L"Export playlist",
      L"Exportar playlist" },
    { L"\u00bfEliminar la playlist \u201c",
      L"Delete the playlist \u201c",
      L"Excluir a playlist \u201c" },
    { L"\u201d?\nSe borrar\u00e1 su archivo .txt.",
      L"\u201d?\nIts .txt file will be deleted.",
      L"\u201d?\nO arquivo .txt dela ser\u00e1 apagado." },
    { L"Exportar a .txt\u2026",
      L"Export to .txt\u2026",
      L"Exportar para .txt\u2026" },
    { L"Eliminar",
      L"Delete",
      L"Excluir" },
    { L"Ganancia: %+.1f dB  (%.0f%%)",
      L"Gain: %+.1f dB  (%.0f%%)",
      L"Ganho: %+.1f dB  (%.0f%%)" },
    { L"Graves",
      L"Bass",
      L"Graves" },
    { L"Medios",
      L"Mid",
      L"M\u00e9dios" },
    { L"Agudos",
      L"Treble",
      L"Agudos" },
    { L"Volumen: %.0f%%%ls",
      L"Volume: %.0f%%%ls",
      L"Volume: %.0f%%%ls" },
    { L"  (silencio)",
      L"  (muted)",
      L"  (mudo)" },
    { L"Activar",
      L"Unmute",
      L"Ativar som" },
    { L"Silencio",
      L"Mute",
      L"Silenciar" },
    { L"Predeterminado del sistema",
      L"System default",
      L"Padr\u00e3o do sistema" },
    { L" (no disponible)",
      L" (unavailable)",
      L" (indispon\u00edvel)" },
    { L"\u25CF  EN VIVO  \u2014  transmisi\u00f3n de red",
      L"\u25cf  LIVE  \u2014  network stream",
      L"\u25cf  AO VIVO  \u2014  transmiss\u00e3o de rede" },
    { L"Arrastra archivos o carpetas aqui, o usa Abrir / Carpeta",
      L"Drag files or folders here, or use Open / Folder",
      L"Arraste arquivos ou pastas aqui, ou use Abrir / Pasta" },
    { L"Sin forma de onda",
      L"No waveform",
      L"Sem forma de onda" },
    { L"Analizando forma de onda...",
      L"Analyzing waveform...",
      L"Analisando forma de onda..." },
    { L"\u25CF EN VIVO",
      L"\u25cf LIVE",
      L"\u25cf AO VIVO" },
    { L"Velocidad: 1.00x  (normal)",
      L"Speed: 1.00x  (normal)",
      L"Velocidade: 1.00x  (normal)" },
    { L"Velocidad: %.2fx",
      L"Speed: %.2fx",
      L"Velocidade: %.2fx" },
    { L"Listas Guardadas",
      L"Saved Lists",
      L"Listas Salvas" },
    { L"   \u25CF EN VIVO",
      L"   \u25cf LIVE",
      L"   \u25cf AO VIVO" },
    { L"  [Reproduciendo]",
      L"  [Playing]",
      L"  [Reproduzindo]" },
    { L"  [Pausa]",
      L"  [Paused]",
      L"  [Pausado]" },
    { L"  [Detenido]",
      L"  [Stopped]",
      L"  [Parado]" },
    { L"Pico: %+.1f dBFS  - SATURA (el limitador suaviza)",
      L"Peak: %+.1f dBFS  - CLIPPING (the limiter smooths it)",
      L"Pico: %+.1f dBFS  - SATURANDO (o limitador suaviza)" },
    { L"Pico: %+.1f dBFS  - SATURA",
      L"Peak: %+.1f dBFS  - CLIPPING",
      L"Pico: %+.1f dBFS  - SATURANDO" },
    { L"Pico: %+.1f dBFS",
      L"Peak: %+.1f dBFS",
      L"Pico: %+.1f dBFS" },
    { L"Pico: -inf dBFS",
      L"Peak: -inf dBFS",
      L"Pico: -inf dBFS" },
    { L"%ls (compilaci\u00f3n %lu)",
      L"%ls (build %lu)",
      L"%ls (compila\u00e7\u00e3o %lu)" },
    { L" (compilaci\u00f3n ",
      L" (build ",
      L" (compila\u00e7\u00e3o " },
    { L"Elaborado por IA21\r\n",
      L"Made by IA21\r\n",
      L"Feito por IA21\r\n" },
    { L"Versi\u00f3n ",
      L"Version ",
      L"Vers\u00e3o " },
    { L"  \u00b7  compilada el ",
      L"  \u00b7  built on ",
      L"  \u00b7  compilada em " },
    { L"Reproductor de audio y video minimalista",
      L"Minimalist audio and video player",
      L"Reprodutor de \u00e1udio e v\u00eddeo minimalista" },
    { L"Elaborado por",
      L"Made by",
      L"Feito por" },
    { L"Sitio web",
      L"Website",
      L"Site" },
    { L"Motor multimedia",
      L"Media engine",
      L"Motor de m\u00eddia" },
    { L"Licencia de FFmpeg",
      L"FFmpeg license",
      L"Licen\u00e7a do FFmpeg" },
    { L"Formatos",
      L"Formats",
      L"Formatos" },
    { L"Todos los que soporta FFmpeg, y rutas de red",
      L"All FFmpeg formats and network streams",
      L"Todos os formatos do FFmpeg e rede" },
    { L"Sistema",
      L"System",
      L"Sistema" },
    { L"Usa FFmpeg (ffmpeg.org). Se ofrece sin garant\u00eda de ning\u00fan tipo.",
      L"Uses FFmpeg (ffmpeg.org). Provided without warranty of any kind.",
      L"Usa o FFmpeg (ffmpeg.org). Fornecido sem garantia de qualquer tipo." },
    { L"Si te gust\u00f3 MiauPlayer, inv\u00edtame un caf\u00e9",
      L"Enjoying MiauPlayer? Buy me a coffee",
      L"Gostou do MiauPlayer? Me pague um caf\u00e9" },
    { L"\u00a1Copiado!",
      L"Copied!",
      L"Copiado!" },
    { L"Copiar informaci\u00f3n",
      L"Copy info",
      L"Copiar informa\u00e7\u00f5es" },
    { L"Cerrar",
      L"Close",
      L"Fechar" },
    { L"Acerca de MiauPlayer",
      L"About MiauPlayer",
      L"Sobre o MiauPlayer" },
    { L"Quitar de la lista",
      L"Remove from list",
      L"Remover da lista" },
    { L"Limpiar toda la lista",
      L"Clear entire list",
      L"Limpar toda a lista" },
    { L"A\u00fan no hay playlists guardadas.\nPulsa el bot\u00f3n de guardar para crear una con la lista actual.",
      L"No saved playlists yet.\nPress the save button to create one from the current list.",
      L"Ainda n\u00e3o h\u00e1 playlists salvas.\nClique no bot\u00e3o de salvar para criar uma com a lista atual." },
    { L"Abrir",
      L"Open",
      L"Abrir" },
    { L"Carpeta",
      L"Folder",
      L"Pasta" },
    { L"Play/Pausa",
      L"Play/Pause",
      L"Reproduzir/Pausar" },
    { L"Stop",
      L"Stop",
      L"Parar" },
    { L"Limitador suave",
      L"Soft limiter",
      L"Limitador suave" },
    { L"Capturar fotograma",
      L"Capture frame",
      L"Capturar quadro" },
    { L"Abrir carpeta de capturas",
      L"Open captures folder",
      L"Abrir pasta de capturas" },
    { L"Opciones de captura",
      L"Capture options",
      L"Op\u00e7\u00f5es de captura" },
    { L"Carpeta actual: ",
      L"Current folder: ",
      L"Pasta atual: " },
    { L"Cambiar carpeta de capturas\u2026",
      L"Change captures folder\u2026",
      L"Alterar pasta de capturas\u2026" },
    { L"Usar la carpeta predeterminada",
      L"Use the default folder",
      L"Usar a pasta padr\u00e3o" },
    { L"Elige la carpeta donde se guardar\u00e1n las capturas",
      L"Choose the folder where captures will be saved",
      L"Escolha a pasta onde as capturas ser\u00e3o salvas" },
    { L"Fotograma guardado",
      L"Frame saved",
      L"Quadro salvo" },
    { L"Fotograma guardado: ",
      L"Frame saved: ",
      L"Quadro salvo: " },
    { L"No hay video para capturar",
      L"There is no video to capture",
      L"N\u00e3o h\u00e1 v\u00eddeo para capturar" },
    { L"No se pudo guardar el fotograma",
      L"The frame could not be saved",
      L"N\u00e3o foi poss\u00edvel salvar o quadro" },
    { L"Pico",
      L"Peak",
      L"Pico" },
    { L"Lista",
      L"List",
      L"Lista" },
    { L"Salida",
      L"Output",
      L"Sa\u00edda" },
    { L"Acerca de",
      L"About",
      L"Sobre" },
    { L"Importar",
      L"Import",
      L"Importar" },
    { L"Exportar",
      L"Export",
      L"Exportar" },
    { L"Abrir archivos",
      L"Open files",
      L"Abrir arquivos" },
    { L"Detener",
      L"Stop",
      L"Parar" },
    { L"Anterior",
      L"Previous",
      L"Anterior" },
    { L"Siguiente",
      L"Next",
      L"Pr\u00f3xima" },
    { L"Guardar la lista actual como playlist",
      L"Save the current list as a playlist",
      L"Salvar a lista atual como playlist" },
    { L"Abrir ruta de red (RTSP, HTTP\u2026)",
      L"Open network stream (RTSP, HTTP\u2026)",
      L"Abrir transmiss\u00e3o de rede (RTSP, HTTP\u2026)" },
    { L"Abrir carpeta",
      L"Open folder",
      L"Abrir pasta" },
    { L"Reproducir / Pausa",
      L"Play / Pause",
      L"Reproduzir / Pausar" },
    { L"Lista de reproducci\u00f3n",
      L"Playlist",
      L"Lista de reprodu\u00e7\u00e3o" },
    { L"Salida de audio",
      L"Audio output",
      L"Sa\u00edda de \u00e1udio" },
    { L"Velocidad de reproducci\u00f3n",
      L"Playback speed",
      L"Velocidade de reprodu\u00e7\u00e3o" },
    { L"No se pudo abrir el dispositivo de audio",
      L"Could not open the audio device",
      L"N\u00e3o foi poss\u00edvel abrir o dispositivo de \u00e1udio" },
    { L"No se pudo conectar con la transmisi\u00f3n (revisa la direcci\u00f3n, el usuario y la contrase\u00f1a)",
      L"Could not connect to the stream (check the address, user and password)",
      L"N\u00e3o foi poss\u00edvel conectar \u00e0 transmiss\u00e3o (verifique o endere\u00e7o, o usu\u00e1rio e a senha)" },
    { L"No se pudo abrir/decodificar el archivo",
      L"Could not open/decode the file",
      L"N\u00e3o foi poss\u00edvel abrir/decodificar o arquivo" },
    { L"(se omitio un archivo no valido)",
      L"(an invalid file was skipped)",
      L"(um arquivo inv\u00e1lido foi ignorado)" },
    { L"Audio y video",
      L"Audio and video",
      L"\u00c1udio e v\u00eddeo" },
    { L"Todos los archivos",
      L"All files",
      L"Todos os arquivos" },
    { L"Playlists de texto (*.txt)",
      L"Text playlists (*.txt)",
      L"Playlists de texto (*.txt)" },
    { L"Texto (*.txt)",
      L"Text (*.txt)",
      L"Texto (*.txt)" },
    { L"Configuración",
      L"Settings",
      L"Configurações" },
    { L"Idioma",
      L"Language",
      L"Idioma" },
    { L"Asociar tipos de archivo…",
      L"Associate file types…",
      L"Associar tipos de arquivo…" },
    { L"El idioma cambiará la próxima vez que abras MiauPlayer.",
      L"The language will change the next time you open MiauPlayer.",
      L"O idioma mudará na próxima vez que você abrir o MiauPlayer." },
    { L"Archivo de audio (MiauPlayer)",
      L"Audio file (MiauPlayer)",
      L"Arquivo de áudio (MiauPlayer)" },
    { L"Archivo de video (MiauPlayer)",
      L"Video file (MiauPlayer)",
      L"Arquivo de vídeo (MiauPlayer)" },
    { L"Abrir ubicaci\u00f3n del archivo",
      L"Open file location",
      L"Abrir local do arquivo" },
    { L"Informaci\u00f3n de la pista",
      L"Track information",
      L"Informa\u00e7\u00f5es da faixa" },
    { L"El archivo ya no existe en esa ubicaci\u00f3n.",
      L"The file no longer exists at that location.",
      L"O arquivo n\u00e3o existe mais nesse local." },
    { L"No se pudo leer la informaci\u00f3n de este archivo.",
      L"The information for this file could not be read.",
      L"N\u00e3o foi poss\u00edvel ler as informa\u00e7\u00f5es deste arquivo." },
    { L"Archivo",
      L"File",
      L"Arquivo" },
    { L"Tipo",
      L"Type",
      L"Tipo" },
    { L"Frecuencia",
      L"Sample rate",
      L"Frequ\u00eancia" },
    { L"Profundidad de bits",
      L"Bit depth",
      L"Profundidade de bits" },
    { L"bits (flotante)",
      L"bits (float)",
      L"bits (ponto flutuante)" },
    { L"no aplica (formato con p\u00e9rdida)",
      L"not applicable (lossy format)",
      L"n\u00e3o se aplica (formato com perdas)" },
    { L"audio",
      L"audio",
      L"\u00e1udio" },
    { L"video",
      L"video",
      L"v\u00eddeo" },
    { L"desconocido",
      L"unknown",
      L"desconhecido" },
    { L"total del archivo",
      L"file total",
      L"total do arquivo" },
    { L"¿Quieres abrir tus archivos de audio y video con MiauPlayer?\n\n"
      L"Tipos de archivo: .mp3  .mp4  .ogg  .wma  .wmv  .aac  .m4a  .mpg  .mov  .mkv\n\n"
      L"Windows te pedirá confirmar la elección. Podrás cambiarlo después en Configuración (engranaje) > Asociar tipos de archivo.",
      L"Do you want to open your audio and video files with MiauPlayer?\n\n"
      L"File types: .mp3  .mp4  .ogg  .wma  .wmv  .aac  .m4a  .mpg  .mov  .mkv\n\n"
      L"Windows will ask you to confirm the choice. You can change it later in Settings (gear icon) > Associate file types.",
      L"Deseja abrir seus arquivos de áudio e vídeo com o MiauPlayer?\n\n"
      L"Tipos de arquivo: .mp3  .mp4  .ogg  .wma  .wmv  .aac  .m4a  .mpg  .mov  .mkv\n\n"
      L"O Windows pedirá que você confirme a escolha. Você poderá alterá-la depois em Configurações (engrenagem) > Associar tipos de arquivo." },
};
static std::unordered_map<std::wstring, const TrEntry*> g_trMap;
static const wchar_t* TX(const wchar_t* es) {
    if (g_lang == LNG_ES) return es;
    auto it = g_trMap.find(es);
    if (it == g_trMap.end()) return es;
    return g_lang == LNG_EN ? it->second->en : it->second->pt;
}
// arma el filtro de un dialogo abrir/guardar: "etiqueta\0patron\0...\0\0" con las etiquetas traducidas
static std::wstring makeFilter(std::initializer_list<std::pair<const wchar_t*, const wchar_t*>> items) {
    std::wstring f;
    for (auto& it : items) { f += TX(it.first); f.push_back(L'\0'); f += it.second; f.push_back(L'\0'); }
    f.push_back(L'\0');
    return f;
}
// idioma: /lang=es|en|pt, luego HKCU\\Software\\MiauPlayer\\Language, luego HKLM (lo escribe el instalador), luego el de Windows
static void initLanguage(const std::wstring& forced) {
    std::wstring v = forced;
    auto rd = [&](HKEY root) {
        HKEY k;
        if (RegOpenKeyExW(root, L"Software\\MiauPlayer", 0, KEY_QUERY_VALUE | KEY_WOW64_64KEY, &k) == ERROR_SUCCESS) {
            wchar_t b[16] = L"";
            DWORD sz = sizeof(b) - sizeof(wchar_t), ty = 0;
            if (RegQueryValueExW(k, L"Language", nullptr, &ty, (BYTE*)b, &sz) == ERROR_SUCCESS && ty == REG_SZ) v = b;
            RegCloseKey(k);
        }
    };
    if (v.empty()) rd(HKEY_CURRENT_USER);
    if (v.empty()) rd(HKEY_LOCAL_MACHINE);
    if (v == L"es") g_lang = LNG_ES;
    else if (v == L"en") g_lang = LNG_EN;
    else if (v == L"pt") g_lang = LNG_PT;
    else {
        const WORD pl = PRIMARYLANGID(GetUserDefaultUILanguage());
        g_lang = pl == LANG_SPANISH ? LNG_ES : pl == LANG_PORTUGUESE ? LNG_PT : LNG_EN;
    }
    for (const TrEntry& e : kTr) g_trMap[e.es] = &e;
}

// ------------------------------------------------------------ rutas de red (RTSP, HTTP, RTMP...)
static std::atomic<bool> g_stop(false);
static std::atomic<bool> g_live(false);   // la pista actual es una transmision de red (en vivo)

static bool isUrl(const std::wstring& p) {
    const size_t i = p.find(L"://");
    if (i == std::wstring::npos || i < 2 || i > 10 || p.size() <= i + 3) return false;
    for (size_t k = 0; k < i; k++)
        if (!iswalnum(p[k]) && p[k] != L'+' && p[k] != L'-' && p[k] != L'.') return false;
    return true;
}
static bool isUrlA(const std::string& p) {
    const size_t i = p.find("://");
    if (i == std::string::npos || i < 2 || i > 10 || p.size() <= i + 3) return false;
    for (size_t k = 0; k < i; k++)
        if (!isalnum((unsigned char)p[k]) && p[k] != '+' && p[k] != '-' && p[k] != '.') return false;
    return true;
}
static int avInterrupt(void*) { return g_stop.load() ? 1 : 0; }   // permite cancelar una conexion en curso

// abre un archivo o una ruta de red; para RTSP prueba TCP y, si falla, UDP
static int openInput(AVFormatContext** out, const std::string& path) {
    if (!isUrlA(path)) return avformat_open_input(out, path.c_str(), nullptr, nullptr);
    const bool rtsp = path.compare(0, 4, "rtsp") == 0;
    int last = AVERROR(EIO);
    for (int attempt = 0; attempt < (rtsp ? 2 : 1); attempt++) {
        AVFormatContext* ctx = avformat_alloc_context();
        if (!ctx) return AVERROR(ENOMEM);
        ctx->interrupt_callback.callback = avInterrupt;
        ctx->interrupt_callback.opaque = nullptr;
        AVDictionary* o = nullptr;
        if (rtsp) {
            av_dict_set(&o, "rtsp_transport", attempt == 0 ? "tcp" : "udp", 0);
            av_dict_set(&o, "timeout", "8000000", 0);    // RTSP: microsegundos (en RTMP "timeout" activaria el modo servidor)
        }
        av_dict_set(&o, "rw_timeout", "8000000", 0);     // genérico, microsegundos
        av_dict_set(&o, "fflags", "nobuffer", 0);
        av_dict_set(&o, "flags", "low_delay", 0);
        av_dict_set(&o, "analyzeduration", "1500000", 0);
        av_dict_set(&o, "probesize", "1000000", 0);
        last = avformat_open_input(&ctx, path.c_str(), nullptr, &o);   // si falla, libera ctx
        av_dict_free(&o);
        if (last >= 0) { *out = ctx; return 0; }
        if (g_stop.load()) break;
    }
    return last;
}

// ------------------------------------------------------------ decodificador
struct Decoder {
    AVFormatContext* fmt = nullptr;
    AVCodecContext* cc = nullptr;
    SwrContext* swr = nullptr;
    AVPacket* pkt = nullptr;
    AVFrame* frm = nullptr;
    int si = -1;
    int outRate = 48000, outCh = 2;
    double duration = 0;     // segundos (0 = desconocida)
    double startOffset = 0;  // start_time del contenedor
    bool eof = false, drained = false;
    double skipUntil = -1;   // tras un seek: descartar muestras hasta este instante (absoluto)
    double nextPos = -1;     // instante absoluto de la siguiente muestra de salida
    std::vector<float> tmp;
    // modo "sin audio" (video mudo): genera silencio para tener reloj de reproduccion
    bool noAudio = false;
    long long silentFrames = 0, totalFrames = 0;
    std::atomic<bool>* videoEnded = nullptr;

    ~Decoder() { close(); }

    void close() {
        if (swr) swr_free(&swr);
        if (frm) av_frame_free(&frm);
        if (pkt) av_packet_free(&pkt);
        if (cc) avcodec_free_context(&cc);
        if (fmt) avformat_close_input(&fmt);
    }

    bool initSwr() {
        if (swr) swr_free(&swr);
        if (cc->ch_layout.order == AV_CHANNEL_ORDER_UNSPEC)
            av_channel_layout_default(&cc->ch_layout, cc->ch_layout.nb_channels);
        AVChannelLayout out;
        av_channel_layout_default(&out, outCh);
        int r = swr_alloc_set_opts2(&swr, &out, AV_SAMPLE_FMT_FLT, outRate, &cc->ch_layout,
                                    cc->sample_fmt, cc->sample_rate, 0, nullptr);
        av_channel_layout_uninit(&out);
        if (r < 0 || !swr) return false;
        return swr_init(swr) >= 0;
    }

    // rate <= 0: la salida usa la frecuencia de muestreo original del archivo (sin remuestrear)
    bool open(const std::string& path, int rate, int ch, bool allowNoAudio = false) {
        outRate = rate > 0 ? rate : 48000; outCh = ch;
        if (openInput(&fmt, path) < 0) { fmt = nullptr; return false; }
        if (avformat_find_stream_info(fmt, nullptr) < 0) return false;
        if (fmt->start_time != AV_NOPTS_VALUE) startOffset = fmt->start_time / (double)AV_TIME_BASE;
        if (fmt->duration != AV_NOPTS_VALUE && fmt->duration > 0) duration = fmt->duration / (double)AV_TIME_BASE;
        const AVCodec* codec = nullptr;
        si = av_find_best_stream(fmt, AVMEDIA_TYPE_AUDIO, -1, -1, &codec, 0);
        if (si < 0 || !codec) {
            if (!allowNoAudio) return false;
            if (av_find_best_stream(fmt, AVMEDIA_TYPE_VIDEO, -1, -1, nullptr, 0) < 0) return false;
            noAudio = true;
            totalFrames = (long long)(duration * outRate);
            return true;
        }
        cc = avcodec_alloc_context3(codec);
        if (!cc) return false;
        if (avcodec_parameters_to_context(cc, fmt->streams[si]->codecpar) < 0) return false;
        if (avcodec_open2(cc, codec, nullptr) < 0) return false;
        if (rate <= 0 && cc->sample_rate >= 8000 && cc->sample_rate <= 384000) outRate = cc->sample_rate;
        if (!initSwr()) return false;
        pkt = av_packet_alloc();
        frm = av_frame_alloc();
        if (duration <= 0) {
            AVStream* st = fmt->streams[si];
            if (st->duration != AV_NOPTS_VALUE && st->duration > 0) duration = st->duration * av_q2d(st->time_base);
        }
        return true;
    }

    bool setOutRate(int r) { outRate = r; return cc ? initSwr() : true; }   // p. ej. volver a 48 kHz si el dispositivo no acepta la original

    void seek(double sec) {
        if (noAudio) { silentFrames = (long long)(std::max(0.0, sec) * outRate); return; }
        double target = sec + startOffset;
        if (target < 0) target = 0;
        av_seek_frame(fmt, -1, (int64_t)(target * AV_TIME_BASE), AVSEEK_FLAG_BACKWARD);
        avcodec_flush_buffers(cc);
        initSwr();
        eof = false; drained = false;
        skipUntil = sec > 0.05 ? target : -1;
        nextPos = -1;
    }

    void convert(AVFrame* f, std::vector<float>& out) {
        int cap = swr_get_out_samples(swr, f->nb_samples);
        if (cap <= 0) return;
        tmp.resize((size_t)cap * outCh);
        uint8_t* o[1] = { (uint8_t*)tmp.data() };
        int n = swr_convert(swr, o, cap, (const uint8_t**)f->extended_data, f->nb_samples);
        if (n <= 0) return;
        int start = 0;
        if (skipUntil >= 0) {
            if (nextPos < 0 && f->best_effort_timestamp != AV_NOPTS_VALUE)
                nextPos = f->best_effort_timestamp * av_q2d(fmt->streams[si]->time_base);
            if (nextPos < 0) skipUntil = -1;
            else {
                double len = n / (double)outRate;
                if (nextPos + len <= skipUntil) { nextPos += len; return; }
                start = (int)std::max(0.0, (skipUntil - nextPos) * outRate);
                skipUntil = -1;
            }
        }
        out.insert(out.end(), tmp.begin() + (size_t)start * outCh, tmp.begin() + (size_t)n * outCh);
    }

    // Agrega audio decodificado (float intercalado) a 'out'. false = fin del archivo.
    bool more(std::vector<float>& out) {
        if (noAudio) {
            if (totalFrames > 0 ? silentFrames >= totalFrames : (videoEnded && videoEnded->load())) return false;
            out.insert(out.end(), (size_t)2048 * outCh, 0.f);
            silentFrames += 2048;
            return true;
        }
        for (;;) {
            int r = avcodec_receive_frame(cc, frm);
            if (r == 0) {
                size_t before = out.size();
                convert(frm, out);
                av_frame_unref(frm);
                if (out.size() > before) return true;
                continue;
            }
            if (r == AVERROR(EAGAIN)) {
                if (eof) return false;
                int rr = av_read_frame(fmt, pkt);
                if (rr < 0) { eof = true; avcodec_send_packet(cc, nullptr); continue; }
                if (pkt->stream_index == si) avcodec_send_packet(cc, pkt);
                av_packet_unref(pkt);
                continue;
            }
            if (r == AVERROR_EOF) {
                if (drained) return false;
                drained = true;
                int cap = swr_get_out_samples(swr, 0) + 4096;
                tmp.resize((size_t)cap * outCh);
                uint8_t* o[1] = { (uint8_t*)tmp.data() };
                int n = swr_convert(swr, o, cap, nullptr, 0);
                if (n > 0) { out.insert(out.end(), tmp.begin(), tmp.begin() + (size_t)n * outCh); return true; }
                return false;
            }
            return false;
        }
    }
};

// ------------------------------------------------------------ estado global
enum { ST_STOPPED, ST_PLAYING, ST_PAUSED };
enum { WM_APP_END = WM_APP + 1, WM_APP_ERROR, WM_APP_OPENED, WM_APP_VIDEOINFO };
enum {
    ID_OPEN = 100, ID_FOLDER, ID_PREV, ID_PLAY, ID_STOP, ID_NEXT, ID_RESET,
    ID_GAIN, ID_BASS, ID_MID, ID_TREBLE, ID_LIMIT, ID_LIST,
    ID_TIME, ID_GAINTXT, ID_PEAKTXT, ID_EQTXT0, ID_EQTXT1, ID_EQTXT2,
    ID_VOL, ID_VOLTXT, ID_MUTE, ID_PEAKON, ID_LISTBTN, ID_OUTBTN, ID_SPEED, ID_SPDRESET, ID_SPD, ID_PLLIST, ID_PLSAVE, ID_PLIMP, ID_PLEXP, ID_ABOUT, ID_NETURL, ID_SNAP, ID_SNAPDIR, ID_SNAPOPT
};
static const int OUT_RATE = 48000;   // solo para el modo de respaldo y los videos sin audio
#ifndef WAVE_FORMAT_IEEE_FLOAT
#define WAVE_FORMAT_IEEE_FLOAT 0x0003
#endif

static HWND g_hwnd, g_list, g_gain, g_eq[3], g_limit, g_time, g_gainTxt, g_peakTxt, g_eqTxt[3];
static HWND g_vol, g_volTxt, g_mute, g_peakChk, g_spd;
static bool g_spdOpen = false;                 // panel de velocidad abierto
static HWND g_plList, g_plSave, g_plImp, g_plExp;                // columna derecha: playlists guardadas + boton guardar
static bool g_panelFloating = false;          // panel de listas flotante sobre el video
static DWORD g_outMenuClosedOnBtn = 0;
static DWORD g_optMenuClosedOnBtn = 0;         // idem para el menu de opciones de captura         // el menu de salida se cerro con un clic sobre su propio boton
static int g_plMid = 0;                        // x de la division entre las dos columnas (0 = sin dividir)
static std::wstring g_loadedPl;                // nombre de la playlist cargada
static std::vector<std::wstring> g_savedNames; // nombres de las playlists guardadas (filas de la columna derecha)
static bool g_tbCompact = false;               // barra de botones en modo compacto (ventana estrecha)
static RECT g_spdRect = { 0, 0, 0, 0 };
static std::atomic<float> g_vol01(1.0f);      // volumen maestro 0..1 (curva cuadratica)
static std::atomic<bool> g_muted(false);
static std::atomic<bool> g_hasVideo(false);
static std::atomic<bool> g_videoEnded(false);
static std::atomic<int> g_vSeekSerial(0);
static std::atomic<double> g_vSeekTarget(0.0);
static std::atomic<long long> g_posQpc(0);    // QPC del ultimo g_posSec (para interpolar el reloj)
static std::atomic<int> g_vidAreaW(0), g_vidAreaH(0);
static bool g_fullscreen = false;
static bool g_liveSize = false;                // la ventana se esta estirando/moviendo con el mouse
// controles flotantes de pantalla completa (ver paintFsControls)
static bool g_fsVisible = true;
static DWORD g_fsLastMove = 0;
static POINT g_fsLastPos = { -1, -1 };
static int g_fsDrag = 0;        // 1 = arrastrando la barra de tiempo, 2 = arrastrando el volumen
static int g_fsHot = 0;         // 1 play, 2 barra, 3 volumen, 4 salir, 5 deslizador de volumen
static bool g_fsVolHover = false;
static const DWORD kFsHideMs = 3000;
static std::wstring g_toast;          // aviso breve sobre el video (pantalla completa)
static DWORD g_toastUntil = 0;
static RECT g_videoRect = { 0, 0, 0, 0 };
static CRITICAL_SECTION g_vidCS;
static std::vector<uint8_t> g_vidPix;         // ultimo cuadro BGRA ya escalado
static AVFrame* g_lastFrame = nullptr;        // ultimo cuadro decodificado (tamano original); se protege con g_vidCS
static double g_lastDar = 1.0;                // relacion de aspecto de ese cuadro
static int g_vidW = 0, g_vidH = 0;
static HANDLE g_videoThread = nullptr;
static WINDOWPLACEMENT g_savedPlacement = { sizeof(WINDOWPLACEMENT) };
static LONG g_savedStyle = 0;
static std::vector<std::wstring> g_playlist;
static int g_cur = -1;
static std::atomic<int> g_state(ST_STOPPED);
static std::atomic<int> g_gen(0);            // generación de pista (invalida hilos viejos)
static std::atomic<float> g_gainDb(0.f);
static std::atomic<float> g_speed(1.0f);     // velocidad de reproduccion 1/3x .. 3x (el tono se conserva)
static std::atomic<float> g_eqDb[3];
static std::atomic<bool> g_limiter(false);
static std::atomic<bool> g_peakOn(false);
static bool g_listFloat = false;              // lista de reproduccion flotante sobre el video     // mostrar indicador de pico (apagado por defecto)
static std::atomic<float> g_peakNow(0.f);
static std::atomic<double> g_posSec(0.0);
static std::atomic<double> g_duration(0.0);
static std::atomic<double> g_seekReq(-1.0);
static HANDLE g_audioThread = nullptr;
static CRITICAL_SECTION g_hwoCS;
static HWAVEOUT g_hwo = nullptr;
static std::wstring g_status;
static float g_peakHold = 0.f;
static bool g_dragging = false;
static double g_dragFrac = 0;
static RECT g_waveRect = { 10, 46, 600, 176 };

// ------------------------------------------------------------ forma de onda
static const int PEAK_BLOCK = 256, WAVE_RATE = 8000;
static const double SEC_PER_PEAK = (double)PEAK_BLOCK / WAVE_RATE;
static const float WAVE_REF = 0.70f;   // fraccion de la altura que ocupa un pico de 0 dBFS con ganancia 0 dB
static CRITICAL_SECTION g_waveCS;
static std::vector<float> g_peaks;
static std::atomic<int> g_waveGen(0);
static std::atomic<bool> g_waveDone(false);

struct WaveJob { std::wstring path; int gen; };

static DWORD WINAPI waveThread(LPVOID p) {
    WaveJob* job = (WaveJob*)p;
    {
        Decoder dec;
        if (dec.open(utf8(job->path), WAVE_RATE, 1)) {
            std::vector<float> buf, local;
            float mx = 0; int cnt = 0;
            bool alive = true;
            while (alive && g_waveGen == job->gen) {
                buf.clear();
                if (!dec.more(buf)) alive = false;
                local.clear();
                for (float v : buf) {
                    float a = fabsf(v);
                    if (a > mx) mx = a;
                    if (++cnt >= PEAK_BLOCK) { local.push_back(mx); mx = 0; cnt = 0; }
                }
                if (!alive && cnt > 0) local.push_back(mx);
                if (!local.empty()) {
                    Lock l(&g_waveCS);
                    if (g_waveGen != job->gen) break;
                    g_peaks.insert(g_peaks.end(), local.begin(), local.end());
                }
            }
            if (g_waveGen == job->gen) {
                Lock l(&g_waveCS);
                if (g_waveGen == job->gen) {
                    g_waveDone = true;
                    if (g_duration.load() <= 0) g_duration = g_peaks.size() * SEC_PER_PEAK;
                }
            }
        } else if (g_waveGen == job->gen) {
            g_waveDone = true;   // archivo sin audio: no hay forma de onda
        }
    }
    delete job;
    return 0;
}

static void startWave(const std::wstring& path) {
    int gen = ++g_waveGen;
    {
        Lock l(&g_waveCS);
        g_peaks.clear();
        g_waveDone = false;
    }
    WaveJob* j = new WaveJob{ path, gen };
    HANDLE h = CreateThread(nullptr, 0, waveThread, j, 0, nullptr);
    if (h) { SetThreadPriority(h, THREAD_PRIORITY_BELOW_NORMAL); CloseHandle(h); } else delete j;
}

// ------------------------------------------------------------ DSP (EQ + ganancia + limitador)
struct Biquad {
    double b0 = 1, b1 = 0, b2 = 0, a1 = 0, a2 = 0;
    double z1[2] = { 0, 0 }, z2[2] = { 0, 0 };
    void set(int type, double f0, double dB, double fs) {  // 0=low shelf, 1=peak, 2=high shelf
        const double PI = 3.14159265358979323846;
        double A = pow(10.0, dB / 40.0), w = 2 * PI * f0 / fs, cs = cos(w), sn = sin(w);
        double B0, B1, B2, A0, A1, A2;
        if (type == 1) {
            double al = sn / (2 * 0.9);
            B0 = 1 + al * A; B1 = -2 * cs; B2 = 1 - al * A; A0 = 1 + al / A; A1 = -2 * cs; A2 = 1 - al / A;
        } else {
            double al = sn / 2 * sqrt(2.0), t = 2 * sqrt(A) * al;
            if (type == 0) {
                B0 = A * ((A + 1) - (A - 1) * cs + t); B1 = 2 * A * ((A - 1) - (A + 1) * cs);
                B2 = A * ((A + 1) - (A - 1) * cs - t); A0 = (A + 1) + (A - 1) * cs + t;
                A1 = -2 * ((A - 1) + (A + 1) * cs); A2 = (A + 1) + (A - 1) * cs - t;
            } else {
                B0 = A * ((A + 1) + (A - 1) * cs + t); B1 = -2 * A * ((A - 1) + (A + 1) * cs);
                B2 = A * ((A + 1) + (A - 1) * cs - t); A0 = (A + 1) - (A - 1) * cs + t;
                A1 = 2 * ((A - 1) - (A + 1) * cs); A2 = (A + 1) - (A - 1) * cs - t;
            }
        }
        b0 = B0 / A0; b1 = B1 / A0; b2 = B2 / A0; a1 = A1 / A0; a2 = A2 / A0;
    }
    inline float proc(int ch, float x) {
        double y = b0 * x + z1[ch];
        z1[ch] = b1 * x - a1 * y + z2[ch];
        z2[ch] = b2 * x - a2 * y;
        return (float)y;
    }
    void reset() { z1[0] = z1[1] = z2[0] = z2[1] = 0; }
};

static inline float softLimit(float x) {
    float a = fabsf(x);
    if (a <= 0.9f) return x;
    float y = 0.9f + 0.1f * tanhf((a - 0.9f) / 0.1f);
    return x < 0 ? -y : y;
}

// ------------------------------------------------------------ hilo de audio
struct AudioJob { std::wstring path; double start; int gen; };

// ------------------------------------------------------------ velocidad (atempo: cambia la velocidad sin cambiar el tono)
struct Tempo {
    int rate = 48000;   // frecuencia del audio que se procesa
    AVFilterGraph* g = nullptr;
    AVFilterContext *src = nullptr, *sink = nullptr, *t1 = nullptr, *t2 = nullptr;
    double cur = 1.0;
    int64_t pts = 0;
    ~Tempo() { freeGraph(); }
    void freeGraph() { if (g) avfilter_graph_free(&g); g = nullptr; src = sink = t1 = t2 = nullptr; }
    static bool twoStages(double t) { return t < 0.5 || t > 2.0; }
    bool build(double t) {
        freeGraph();
        g = avfilter_graph_alloc();
        if (!g) return false;
        char args[160], a1[48], fa[128];
        snprintf(args, sizeof(args), "sample_rate=%d:sample_fmt=flt:channel_layout=stereo:time_base=1/%d", rate, rate);
        const bool two = twoStages(t);
        snprintf(a1, sizeof(a1), "tempo=%.6f", two ? sqrt(t) : t);
        snprintf(fa, sizeof(fa), "sample_fmts=flt:sample_rates=%d:channel_layouts=stereo", rate);
        AVFilterContext* fmt = nullptr;
        if (avfilter_graph_create_filter(&src, avfilter_get_by_name("abuffer"), "in", args, nullptr, g) < 0 ||
            avfilter_graph_create_filter(&t1, avfilter_get_by_name("atempo"), "t1", a1, nullptr, g) < 0 ||
            (two && avfilter_graph_create_filter(&t2, avfilter_get_by_name("atempo"), "t2", a1, nullptr, g) < 0) ||
            avfilter_graph_create_filter(&fmt, avfilter_get_by_name("aformat"), "fmt", fa, nullptr, g) < 0 ||
            avfilter_graph_create_filter(&sink, avfilter_get_by_name("abuffersink"), "out", nullptr, nullptr, g) < 0) {
            freeGraph(); return false;
        }
        AVFilterContext* chain[4] = { t1, t2, fmt, sink };
        AVFilterContext* prev = src;
        for (AVFilterContext* f : chain) {
            if (!f) continue;
            if (avfilter_link(prev, 0, f, 0) < 0) { freeGraph(); return false; }
            prev = f;
        }
        if (avfilter_graph_config(g, nullptr) < 0) { freeGraph(); return false; }
        cur = t; pts = 0;
        return true;
    }
    // fija la velocidad; cerca de 1x se desactiva (sin procesar, sin perdida)
    void set(double t) {
        if (fabs(t - 1.0) < 0.0005) { freeGraph(); cur = 1.0; return; }
        if (!g || twoStages(t) != (t2 != nullptr)) { if (!build(t)) cur = 1.0; return; }
        char v[32];
        snprintf(v, sizeof(v), "%.6f", twoStages(t) ? sqrt(t) : t);
        if (avfilter_graph_send_command(g, "t1", "tempo", v, nullptr, 0, 0) < 0 ||
            (t2 && avfilter_graph_send_command(g, "t2", "tempo", v, nullptr, 0, 0) < 0)) {
            if (!build(t)) cur = 1.0;
            return;
        }
        cur = t;
    }
    void restart(double t) { freeGraph(); cur = 1.0; set(t); }   // tras un salto: descarta lo que tenia dentro
    void drain(std::vector<float>& out) {
        AVFrame* o = av_frame_alloc();
        if (!o) return;
        while (av_buffersink_get_frame(sink, o) >= 0) {
            const float* d = (const float*)o->data[0];
            out.insert(out.end(), d, d + (size_t)o->nb_samples * 2);
            av_frame_unref(o);
        }
        av_frame_free(&o);
    }
    void process(std::vector<float>& io) {   // io: estereo intercalado float; se reemplaza por el audio ya acelerado/frenado
        if (!g) return;
        const int n = (int)(io.size() / 2);
        if (n <= 0) return;
        AVFrame* f = av_frame_alloc();
        if (!f) return;
        f->format = AV_SAMPLE_FMT_FLT;
        f->sample_rate = rate;
        av_channel_layout_default(&f->ch_layout, 2);
        f->nb_samples = n;
        if (av_frame_get_buffer(f, 0) < 0) { av_frame_free(&f); return; }
        memcpy(f->data[0], io.data(), (size_t)n * 2 * sizeof(float));
        f->pts = pts; pts += n;
        io.clear();
        if (av_buffersrc_add_frame_flags(src, f, 0) >= 0) drain(io);
        av_frame_free(&f);
    }
    void flush(std::vector<float>& io) {
        if (!g) return;
        (void)av_buffersrc_add_frame_flags(src, nullptr, 0);
        drain(io);
    }
};

// ------------------------------------------------------------ salida de audio (tarjeta de sonido)
struct OutDev { UINT id; std::wstring name; };
static std::wstring g_outName;   // "" = predeterminado del sistema; protegido con g_hwoCS

static std::vector<OutDev> listOutputs() {
    std::vector<OutDev> v;
    UINT n = waveOutGetNumDevs();
    for (UINT i = 0; i < n; i++) {
        WAVEOUTCAPS c = {};
        if (waveOutGetDevCaps(i, &c, sizeof(c)) == MMSYSERR_NOERROR) v.push_back({ i, c.szPname });
    }
    // nombres completos desde MMDevice (waveOut recorta a 31 caracteres)
    HRESULT hr = CoInitializeEx(nullptr, COINIT_MULTITHREADED);
    const bool inited = SUCCEEDED(hr);
    IMMDeviceEnumerator* en = nullptr;
    if (SUCCEEDED(CoCreateInstance(__uuidof(MMDeviceEnumerator), nullptr, CLSCTX_ALL, __uuidof(IMMDeviceEnumerator), (void**)&en)) && en) {
        IMMDeviceCollection* col = nullptr;
        if (SUCCEEDED(en->EnumAudioEndpoints(eRender, DEVICE_STATE_ACTIVE, &col)) && col) {
            UINT cnt = 0;
            col->GetCount(&cnt);
            static const PROPERTYKEY kFriendly = { { 0xa45c254e, 0xdf1c, 0x4efd, { 0x80, 0x20, 0x67, 0xd1, 0x46, 0xa8, 0x50, 0xe0 } }, 14 };
            std::vector<bool> used(v.size(), false);
            for (UINT i = 0; i < cnt; i++) {
                IMMDevice* d = nullptr;
                if (FAILED(col->Item(i, &d)) || !d) continue;
                IPropertyStore* ps = nullptr;
                if (SUCCEEDED(d->OpenPropertyStore(STGM_READ, &ps)) && ps) {
                    PROPVARIANT pv;
                    PropVariantInit(&pv);
                    if (SUCCEEDED(ps->GetValue(kFriendly, &pv)) && pv.vt == VT_LPWSTR && pv.pwszVal) {
                        std::wstring full = pv.pwszVal;
                        for (size_t k = 0; k < v.size(); k++)
                            if (!used[k] && !v[k].name.empty() && full.compare(0, v[k].name.size(), v[k].name) == 0) {
                                v[k].name = full; used[k] = true; break;
                            }
                    }
                    PropVariantClear(&pv);
                    ps->Release();
                }
                d->Release();
            }
            col->Release();
        }
        en->Release();
    }
    if (inited) CoUninitialize();
    return v;
}

static UINT resolveOutput() {   // WAVE_MAPPER si no hay eleccion o el dispositivo ya no existe
    std::wstring name;
    { Lock l(&g_hwoCS); name = g_outName; }
    if (name.empty()) return WAVE_MAPPER;
    for (auto& d : listOutputs()) if (d.name == name) return d.id;
    return WAVE_MAPPER;
}

static void saveOutName() {
    std::wstring name;
    { Lock l(&g_hwoCS); name = g_outName; }
    HKEY k;
    if (RegCreateKeyEx(HKEY_CURRENT_USER, L"Software\\MiauPlayer", 0, nullptr, 0, KEY_SET_VALUE, nullptr, &k, nullptr) == ERROR_SUCCESS) {
        RegSetValueEx(k, L"AudioDevice", 0, REG_SZ, (const BYTE*)name.c_str(), (DWORD)((name.size() + 1) * sizeof(wchar_t)));
        RegCloseKey(k);
    }
}

static void loadOutName() {
    HKEY k;
    if (RegOpenKeyEx(HKEY_CURRENT_USER, L"Software\\MiauPlayer", 0, KEY_QUERY_VALUE, &k) == ERROR_SUCCESS) {
        wchar_t buf[512] = L"";
        DWORD sz = sizeof(buf) - sizeof(wchar_t), type = 0;
        if (RegQueryValueEx(k, L"AudioDevice", nullptr, &type, (BYTE*)buf, &sz) == ERROR_SUCCESS && type == REG_SZ) {
            Lock l(&g_hwoCS);
            g_outName = buf;
        }
        RegCloseKey(k);
    }
}

static DWORD WINAPI audioThread(LPVOID p) {
    AudioJob* job = (AudioJob*)p;
    const int gen = job->gen;
    Decoder dec;
    dec.videoEnded = &g_videoEnded;
    if (!dec.open(utf8(job->path), 0, 2, true)) {   // 0 = frecuencia original del archivo
        PostMessage(g_hwnd, WM_APP_ERROR, gen, 0);
        delete job;
        return 0;
    }
    if (dec.duration > 0) g_duration = dec.duration;
    PostMessage(g_hwnd, WM_APP_OPENED, gen, 0);

    // Salida: float de 32 bits en la frecuencia original (un archivo de 24 bits cabe exacto en un float, sin perdida).
    // Si el dispositivo no la acepta, se vuelve al formato clasico de 16 bits a 48 kHz.
    int rate = dec.outRate;
    bool useFloat = true;
    HANDLE ev = CreateEvent(nullptr, FALSE, FALSE, nullptr);
    HWAVEOUT h = nullptr;
    UINT outDev = resolveOutput();
    auto openWave = [&](bool fl, int r) {
        WAVEFORMATEX wf = {};
        wf.wFormatTag = fl ? WAVE_FORMAT_IEEE_FLOAT : WAVE_FORMAT_PCM; wf.nChannels = 2; wf.nSamplesPerSec = r;
        wf.wBitsPerSample = fl ? 32 : 16; wf.nBlockAlign = fl ? 8 : 4; wf.nAvgBytesPerSec = r * wf.nBlockAlign;
        if (waveOutOpen(&h, outDev, &wf, (DWORD_PTR)ev, 0, CALLBACK_EVENT) == MMSYSERR_NOERROR) return true;
        return outDev != WAVE_MAPPER && waveOutOpen(&h, WAVE_MAPPER, &wf, (DWORD_PTR)ev, 0, CALLBACK_EVENT) == MMSYSERR_NOERROR;
    };
    if (!openWave(true, rate)) {
        useFloat = false;
        rate = 48000;
        if (!dec.noAudio) dec.setOutRate(48000);
        if (!openWave(false, rate)) {
            PostMessage(g_hwnd, WM_APP_ERROR, gen, 1);
            CloseHandle(ev); delete job;
            return 0;
        }
    }
    { Lock l(&g_hwoCS); g_hwo = h; }

    const int bpf = useFloat ? 8 : 4;                                          // bytes por cuadro (estereo)
    const int FR = rate > 96000 ? 8192 : (rate > 48000 ? 4096 : 2048), NB = 4;   // mas cuadros por bloque a frecuencias altas
    std::vector<uint8_t> bufs[NB];
    WAVEHDR hdr[NB] = {};
    for (int i = 0; i < NB; i++) {
        bufs[i].assign((size_t)FR * bpf, 0);
        hdr[i].lpData = (LPSTR)bufs[i].data();
        hdr[i].dwBufferLength = FR * bpf;
        waveOutPrepareHeader(h, &hdr[i], sizeof(WAVEHDR));
        hdr[i].dwFlags |= WHDR_DONE;
    }

    std::vector<float> fifo;
    size_t fp = 0;
    bool decEOF = false;
    double base = job->start;
    if (base > 0) dec.seek(base);
    // posicion del medio segun lo reproducido: la velocidad cambia cuanto avanza el medio por muestra de salida
    struct Seg { double outStart, mediaStart, speed; };
    std::vector<Seg> segs;
    long long written = 0;
    Tempo tempo;
    tempo.rate = rate;
    tempo.set(g_live ? 1.0 : (double)g_speed.load());
    segs.push_back({ 0.0, base, tempo.cur });
    auto mediaAt = [&](double o) {
        size_t k = segs.size() - 1;
        while (k > 0 && segs[k].outStart > o) k--;
        return segs[k].mediaStart + (o - segs[k].outStart) / rate * segs[k].speed;
    };

    Biquad eq[3];
    float eqCache[3] = { 1e9f, 1e9f, 1e9f };
    bool eqOn = false;
    float curG = 0.f;   // entra con un fundido corto para evitar chasquidos
    float peak = 0;

    while (!g_stop) {
        double sr = g_seekReq.exchange(-1.0);
        if (sr >= 0) {
            waveOutReset(h);
            for (int i = 0; i < NB; i++)
                for (int k = 0; k < 200 && !(hdr[i].dwFlags & WHDR_DONE); k++) Sleep(1);
            dec.seek(sr);
            fifo.clear(); fp = 0; decEOF = false; base = sr;
            written = 0;
            tempo.restart(g_live ? 1.0 : (double)g_speed.load());
            segs.clear();
            segs.push_back({ 0.0, sr, tempo.cur });
            for (auto& b : eq) b.reset();
            if (g_state == ST_PAUSED) waveOutPause(h);
        }

        // cambio de velocidad pedido desde la interfaz
        {
            const double want = g_live ? 1.0 : (double)g_speed.load();
            if (fabs(want - tempo.cur) > 0.0005) {
                const double outAt = (double)written + (double)(fifo.size() - fp) / 2.0;   // desde aqui rige la nueva
                const double mAt = mediaAt(outAt);
                tempo.set(want);
                segs.push_back({ outAt, mAt, tempo.cur });
            }
        }

        // parámetros de DSP
        float eqNow[3] = { g_eqDb[0], g_eqDb[1], g_eqDb[2] };
        for (int b = 0; b < 3; b++)
            if (eqNow[b] != eqCache[b]) {
                eqCache[b] = eqNow[b];
                eq[b].set(b, b == 0 ? 120.0 : (b == 1 ? 1000.0 : 8000.0), eqNow[b], rate);
            }
        eqOn = eqNow[0] != 0 || eqNow[1] != 0 || eqNow[2] != 0;
        const float vol = g_muted.load() ? 0.f : g_vol01.load() * g_vol01.load();
        const float tgtG = powf(10.f, g_gainDb.load() / 20.f) * vol;
        const bool lim = g_limiter.load();

        for (int i = 0; i < NB; i++) {
            if (!(hdr[i].dwFlags & WHDR_DONE)) continue;
            uint8_t* dst = bufs[i].data();
            int got = 0;
            while (got < FR) {
                if (fp + 1 >= fifo.size()) {
                    fifo.clear(); fp = 0;
                    if (decEOF) break;
                    if (dec.more(fifo)) { tempo.process(fifo); continue; }
                    decEOF = true;
                    tempo.flush(fifo);                 // cola que queda dentro del filtro
                    if (fifo.size() >= 2) continue;
                    break;
                }
                float l = fifo[fp], r = fifo[fp + 1];
                fp += 2;
                if (eqOn) {
                    for (int b = 0; b < 3; b++) { l = eq[b].proc(0, l); r = eq[b].proc(1, r); }
                }
                curG += (tgtG - curG) * 0.0015f;
                if (fabsf(tgtG - curG) < 1e-4f) curG = tgtG;   // sin esto la ganancia quedaba ~0.99998 y no era exacta
                if (curG != 1.f) { l *= curG; r *= curG; }     // a ganancia 0 dB y volumen 100% la senal pasa intacta
                float a = std::max(fabsf(l), fabsf(r));
                if (a > peak) peak = a;
                if (lim) { l = softLimit(l); r = softLimit(r); }
                l = std::min(1.f, std::max(-1.f, l));
                r = std::min(1.f, std::max(-1.f, r));
                if (useFloat) {
                    ((float*)dst)[got * 2] = l;
                    ((float*)dst)[got * 2 + 1] = r;
                } else {
                    ((short*)dst)[got * 2] = (short)lrintf(l * 32767.f);
                    ((short*)dst)[got * 2 + 1] = (short)lrintf(r * 32767.f);
                }
                got++;
            }
            if (got > 0) {
                hdr[i].dwBufferLength = got * bpf;
                waveOutWrite(h, &hdr[i], sizeof(WAVEHDR));
                written += got;
            }
        }
        float pn = g_peakNow.load();
        if (peak > pn) g_peakNow = peak;
        peak = 0;

        MMTIME mt = {};
        mt.wType = TIME_SAMPLES;
        if (waveOutGetPosition(h, &mt, sizeof(mt)) == MMSYSERR_NOERROR) {
            double played = mt.wType == TIME_SAMPLES ? (double)mt.u.sample
                          : mt.wType == TIME_BYTES ? mt.u.cb / (double)bpf : 0.0;
            LARGE_INTEGER q; QueryPerformanceCounter(&q);
            g_posSec = mediaAt(played);
            while (segs.size() > 1 && segs[1].outStart <= played) segs.erase(segs.begin());
            g_posQpc = q.QuadPart;
        }

        int inflight = 0;
        for (int i = 0; i < NB; i++) if (!(hdr[i].dwFlags & WHDR_DONE)) inflight++;
        if (decEOF && fp + 1 >= fifo.size() && inflight == 0) {
            if (gen == g_gen) PostMessage(g_hwnd, WM_APP_END, gen, 0);
            break;
        }
        WaitForSingleObject(ev, 20);
    }

    waveOutReset(h);
    for (int i = 0; i < NB; i++) {
        for (int k = 0; k < 200 && !(hdr[i].dwFlags & WHDR_DONE); k++) Sleep(1);
        waveOutUnprepareHeader(h, &hdr[i], sizeof(WAVEHDR));
    }
    { Lock l(&g_hwoCS); g_hwo = nullptr; waveOutClose(h); }
    CloseHandle(ev);
    delete job;
    return 0;
}

// ------------------------------------------------------------ hilo de video
static double clockNow() {   // reloj de reproduccion (segundos), interpolado entre lecturas del audio
    double p = g_posSec.load();
    if (g_state == ST_PLAYING) {
        LARGE_INTEGER q, f;
        QueryPerformanceCounter(&q);
        QueryPerformanceFrequency(&f);
        long long t0 = g_posQpc.load();
        if (t0 > 0) {
            double d = (double)(q.QuadPart - t0) / (double)f.QuadPart;
            if (d > 0 && d < 0.1) p += d * (g_live ? 1.0f : g_speed.load());
        }
    }
    return p;
}

static DWORD WINAPI videoThread(LPVOID pp) {
    AudioJob* job = (AudioJob*)pp;
    const int gen = job->gen;
    AVFormatContext* fmt = nullptr;
    AVCodecContext* cc = nullptr;
    SwsContext* sws = nullptr;
    AVPacket* pkt = nullptr;
    AVFrame* frm = nullptr;
    std::vector<uint8_t> dst;
    do {
        if (openInput(&fmt, utf8(job->path)) < 0) { fmt = nullptr; break; }
        if (avformat_find_stream_info(fmt, nullptr) < 0) break;
        const AVCodec* codec = nullptr;
        int vi = av_find_best_stream(fmt, AVMEDIA_TYPE_VIDEO, -1, -1, &codec, 0);
        if (vi < 0 || !codec) break;
        AVStream* st = fmt->streams[vi];
        if (st->disposition & AV_DISPOSITION_ATTACHED_PIC) break;   // portada de un mp3, no es video
        cc = avcodec_alloc_context3(codec);
        if (!cc || avcodec_parameters_to_context(cc, st->codecpar) < 0) break;
        cc->thread_count = 0;
        cc->thread_type = FF_THREAD_FRAME | FF_THREAD_SLICE;
        if (avcodec_open2(cc, codec, nullptr) < 0) break;
        pkt = av_packet_alloc();
        frm = av_frame_alloc();
        const double startOffset = fmt->start_time != AV_NOPTS_VALUE ? fmt->start_time / (double)AV_TIME_BASE : 0.0;
        const double tb = av_q2d(st->time_base);
        double fps = av_q2d(av_guess_frame_rate(fmt, st, nullptr));
        double frameDur = fps > 1 ? 1.0 / fps : 1.0 / 25.0;

        g_hasVideo = true;
        PostMessage(g_hwnd, WM_APP_VIDEOINFO, gen, 1);

        int serial = g_vSeekSerial.load();
        if (job->start > 0.05) {
            av_seek_frame(fmt, -1, (int64_t)((job->start + startOffset) * AV_TIME_BASE), AVSEEK_FLAG_BACKWARD);
            avcodec_flush_buffers(cc);
        }
        double target = job->start > 0.05 ? job->start : -1;   // descartar cuadros anteriores
        double lastPts = 0;
        bool eof = false;

        while (!g_stop && gen == g_gen) {
            if (serial != g_vSeekSerial.load()) {
                serial = g_vSeekSerial.load();
                target = g_vSeekTarget.load();
                av_seek_frame(fmt, -1, (int64_t)((target + startOffset) * AV_TIME_BASE), AVSEEK_FLAG_BACKWARD);
                avcodec_flush_buffers(cc);
                eof = false;
                g_videoEnded = false;
            }
            int r = avcodec_receive_frame(cc, frm);
            if (r == AVERROR(EAGAIN)) {
                if (eof) { g_videoEnded = true; Sleep(15); continue; }
                int rr = av_read_frame(fmt, pkt);
                if (rr < 0) { eof = true; avcodec_send_packet(cc, nullptr); continue; }
                if (pkt->stream_index == vi) avcodec_send_packet(cc, pkt);
                av_packet_unref(pkt);
                continue;
            }
            if (r == AVERROR_EOF) { eof = true; g_videoEnded = true; Sleep(15); continue; }
            if (r < 0) continue;

            double pts = frm->best_effort_timestamp != AV_NOPTS_VALUE
                       ? frm->best_effort_timestamp * tb - startOffset : lastPts + frameDur;
            lastPts = pts;
            if (target >= 0) {
                if (pts < target - 0.03) { av_frame_unref(frm); continue; }
                target = -1;
            }
            // esperar el momento de mostrar el cuadro
            bool abort = false;
            if (!g_live) for (;;) {   // en vivo no se espera: cada cuadro se muestra en cuanto llega
                if (g_stop || gen != g_gen || serial != g_vSeekSerial.load()) { abort = true; break; }
                double wait = pts - clockNow();
                if (wait <= 0.004) break;
                Sleep(wait > 0.03 ? 10 : (DWORD)std::max(1.0, wait * 1000.0 - 2));
            }
            if (g_stop || gen != g_gen) abort = true;
            if (abort) { av_frame_unref(frm); continue; }
            if (!g_live && clockNow() - pts > 0.25) { av_frame_unref(frm); continue; }   // muy atrasado: saltar

            int aw = g_vidAreaW.load(), ah = g_vidAreaH.load();
            if (aw < 16 || ah < 16) { av_frame_unref(frm); continue; }
            AVRational sar = av_guess_sample_aspect_ratio(fmt, st, frm);
            double dar = (double)frm->width / frm->height * (sar.num > 0 && sar.den > 0 ? (double)sar.num / sar.den : 1.0);
            int dw = aw, dh = (int)(aw / dar + 0.5);
            if (dh > ah) { dh = ah; dw = (int)(ah * dar + 0.5); }
            dw = std::max(2, dw & ~1); dh = std::max(2, dh & ~1);
            {   // guarda este cuadro para poder reescalarlo (pausa, pantalla completa, cambio de tamano) o capturarlo
                Lock l(&g_vidCS);
                if (!g_lastFrame) g_lastFrame = av_frame_alloc();
                if (g_lastFrame) { av_frame_unref(g_lastFrame); av_frame_ref(g_lastFrame, frm); g_lastDar = dar; }
            }
            sws = sws_getCachedContext(sws, frm->width, frm->height, (AVPixelFormat)frm->format, dw, dh,
                                       AV_PIX_FMT_BGRA, SWS_BILINEAR, nullptr, nullptr, nullptr);
            if (sws) {
                dst.resize((size_t)dw * dh * 4);
                uint8_t* dp[1] = { dst.data() };
                int ds[1] = { dw * 4 };
                sws_scale(sws, frm->data, frm->linesize, 0, frm->height, dp, ds);
                {
                    Lock l(&g_vidCS);
                    g_vidPix.swap(dst);
                    g_vidW = dw; g_vidH = dh;
                }
                InvalidateRect(g_hwnd, &g_videoRect, FALSE);
            }
            av_frame_unref(frm);
        }
    } while (0);
    if (sws) sws_freeContext(sws);
    if (frm) av_frame_free(&frm);
    if (pkt) av_packet_free(&pkt);
    if (cc) avcodec_free_context(&cc);
    if (fmt) avformat_close_input(&fmt);
    delete job;
    return 0;
}

static void stopAudio(bool keepVideo = false) {
    g_stop = true;
    if (g_audioThread) {
        WaitForSingleObject(g_audioThread, 5000);
        CloseHandle(g_audioThread);
        g_audioThread = nullptr;
    }
    if (g_videoThread) {
        WaitForSingleObject(g_videoThread, 5000);
        CloseHandle(g_videoThread);
        g_videoThread = nullptr;
    }
    g_stop = false;
    g_state = ST_STOPPED;
    g_posSec = 0;
    g_posQpc = 0;
    if (!keepVideo) {   // con keepVideo el ultimo fotograma queda a la vista (fin natural de un video sin mas pistas)
        g_hasVideo = false;
        g_videoEnded = false;
        Lock l(&g_vidCS); g_vidPix.clear(); g_vidW = g_vidH = 0; if (g_lastFrame) av_frame_unref(g_lastFrame);
    }
}

static void startAudio(const std::wstring& path, double start) {
    stopAudio();
    ++g_gen;
    g_seekReq = -1.0;
    g_posSec = start;
    g_posQpc = 0;
    g_state = ST_PLAYING;
    AudioJob* j = new AudioJob{ path, start, g_gen.load() };
    g_audioThread = CreateThread(nullptr, 0, audioThread, j, 0, nullptr);
    if (g_audioThread) SetThreadPriority(g_audioThread, THREAD_PRIORITY_ABOVE_NORMAL);
    else { delete j; g_state = ST_STOPPED; return; }
    AudioJob* v = new AudioJob{ path, start, g_gen.load() };
    g_videoThread = CreateThread(nullptr, 0, videoThread, v, 0, nullptr);
    if (!g_videoThread) delete v;
}

// ------------------------------------------------------------ lista y control
static std::wstring baseName(const std::wstring& p) {
    if (isUrl(p)) {   // ruta de red: se muestra sin usuario ni contrasena
        const size_t st = p.find(L"://") + 3;
        const size_t slash = p.find(L'/', st);
        const size_t at = p.rfind(L'@', slash == std::wstring::npos ? p.size() : slash);
        if (at != std::wstring::npos && at >= st) return p.substr(0, st) + p.substr(at + 1);
        return p;
    }
    size_t i = p.find_last_of(L"\\/");
    return i == std::wstring::npos ? p : p.substr(i + 1);
}

static std::wstring g_titleName;   // nombre del archivo actual (se dibuja en la barra de titulo)

static void updateTitle() {
    std::wstring t = L"MiauPlayer";
    g_titleName.clear();
    if (g_cur >= 0 && g_cur < (int)g_playlist.size()) {
        g_titleName = baseName(g_playlist[g_cur]);
        t = g_titleName + L" - MiauPlayer";
    }
    SetWindowText(g_hwnd, t.c_str());
    InvalidateRect(g_hwnd, nullptr, FALSE);
    if (g_list) InvalidateRect(g_list, nullptr, TRUE);
}

static void relayout();

static void playIndex(int i, double start = 0) {
    if (i < 0 || i >= (int)g_playlist.size()) return;
    g_cur = i;
    g_duration = 0;
    g_status.clear();
    SendMessage(g_list, LB_SETCURSEL, i, 0);
    g_live = isUrl(g_playlist[i]);
    if (g_live) { ++g_waveGen; { Lock l(&g_waveCS); g_peaks.clear(); g_waveDone = true; } }
    else startWave(g_playlist[i]);
    startAudio(g_playlist[i], start);
    updateTitle();
    relayout();
    InvalidateRect(g_hwnd, nullptr, FALSE);
}

static void stopAll() {
    stopAudio();
    g_posSec = 0;
    relayout();
    InvalidateRect(g_hwnd, nullptr, FALSE);
}

static void togglePause() {
    int s = g_state;
    if (s == ST_PLAYING) {
        Lock l(&g_hwoCS);
        if (g_hwo) { waveOutPause(g_hwo); g_state = ST_PAUSED; }
    } else if (s == ST_PAUSED) {
        Lock l(&g_hwoCS);
        if (g_hwo) { waveOutRestart(g_hwo); g_state = ST_PLAYING; }
    } else if (!g_playlist.empty()) {
        playIndex(g_cur >= 0 ? g_cur : 0);
    }
}

static void seekTo(double sec);
static void chooseOutput(const std::wstring& name) {
    { Lock l(&g_hwoCS); g_outName = name; }
    saveOutName();
    const int st = g_state;
    if (st == ST_STOPPED || g_cur < 0) return;
    const double pos = g_posSec;
    playIndex(g_cur, pos);          // reabre la pista en la nueva salida, en el mismo punto
    if (st == ST_PAUSED) { g_state = ST_PAUSED; seekTo(pos); }
}

static void nextTrack(int d, bool natural = false) {
    if (g_playlist.empty()) return;
    int n = g_cur + d;
    if (n < 0) n = 0;
    if (n >= (int)g_playlist.size()) {
        if (natural && g_hasVideo) {   // termino el ultimo video: se queda el ultimo fotograma en pantalla
            stopAudio(true);
            g_posSec = 0;
            relayout();
            InvalidateRect(g_hwnd, nullptr, FALSE);
            return;
        }
        stopAll();
        return;
    }
    playIndex(n);
}

static void seekTo(double sec) {
    if (g_state == ST_STOPPED || g_live) return;
    double d = g_duration;
    if (sec < 0) sec = 0;
    if (d > 0 && sec > d - 0.05) sec = d - 0.05;
    g_seekReq = sec;
    g_posSec = sec;
    g_vSeekTarget = sec;
    ++g_vSeekSerial;
}

static bool isAudioExt(const std::wstring& name) {
    size_t i = name.find_last_of(L'.');
    if (i == std::wstring::npos) return false;
    std::wstring e = name.substr(i + 1);
    for (auto& c : e) c = (wchar_t)towlower(c);
    static const wchar_t* exts[] = { L"mp3", L"mp2", L"mp1", L"aac", L"m4a", L"m4b", L"mp4", L"flac", L"ogg", L"oga",
        L"opus", L"wav", L"aif", L"aiff", L"aifc", L"ape", L"tta", L"wv", L"mpc", L"wma", L"asf", L"ac3", L"eac3",
        L"dts", L"mka", L"caf", L"dsf", L"dff", L"amr", L"au", L"w64", L"tak", L"mlp", L"thd", L"3gp", L"mov", L"wave",
        L"mkv", L"webm", L"avi", L"wmv", L"flv", L"mpg", L"mpeg", L"m4v", L"ts", L"m2ts", L"mts", L"vob", L"ogv", L"divx", L"rm", L"rmvb" };
    for (auto x : exts) if (e == x) return true;
    return false;
}

static void scanDir(const std::wstring& dir, std::vector<std::wstring>& out) {
    WIN32_FIND_DATAW fd;
    HANDLE h = FindFirstFileW((dir + L"\\*").c_str(), &fd);
    if (h == INVALID_HANDLE_VALUE) return;
    std::vector<std::wstring> dirs, files;
    do {
        std::wstring n = fd.cFileName;
        if (n == L"." || n == L"..") continue;
        if (fd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) dirs.push_back(dir + L"\\" + n);
        else if (isAudioExt(n)) files.push_back(dir + L"\\" + n);
    } while (FindNextFileW(h, &fd));
    FindClose(h);
    std::sort(files.begin(), files.end());
    std::sort(dirs.begin(), dirs.end());
    for (auto& f : files) out.push_back(f);
    for (auto& d : dirs) scanDir(d, out);
}

static int addPaths(const std::vector<std::wstring>& paths, bool autoplay = true) {
    std::vector<std::wstring> add;
    for (auto& p : paths) {
        if (isUrl(p)) { add.push_back(p); continue; }
        DWORD a = GetFileAttributesW(p.c_str());
        if (a == INVALID_FILE_ATTRIBUTES) continue;
        if (a & FILE_ATTRIBUTE_DIRECTORY) scanDir(p, add);
        else add.push_back(p);
    }
    if (add.empty()) return -1;
    int first = (int)g_playlist.size();
    for (auto& f : add) {
        g_playlist.push_back(f);
        SendMessage(g_list, LB_ADDSTRING, 0, (LPARAM)baseName(f).c_str());
    }
    if (autoplay && g_state == ST_STOPPED) playIndex(first);
    return first;
}

// ---- archivos que llegan desde fuera (menu contextual, "Abrir con", doble clic): instancia unica
static int g_openFirst = -1;   // primer archivo del lote "abrir"; suena cuando el lote termina de llegar
static void handleExternalPaths(const std::vector<std::wstring>& paths, bool play) {
    if (paths.empty()) return;
    if (!play) { addPaths(paths, true); return; }               // "Anadir a la lista": no interrumpe lo que suena
    int first = addPaths(paths, false);                         // "Abrir": se agregan y suena el primero del lote
    if (first >= 0) {
        if (g_openFirst < 0) g_openFirst = first;
        SetTimer(g_hwnd, 2, 450, nullptr);                      // espera a los demas archivos de la seleccion
    }
}

static void doOpenFiles() {
    std::vector<wchar_t> buf(65536, 0);
    OPENFILENAMEW o = {};
    o.lStructSize = sizeof(o);
    o.hwndOwner = g_hwnd;
    o.lpstrFile = buf.data();
    o.nMaxFile = (DWORD)buf.size();
    const std::wstring flt_ = makeFilter({ { TX(L"Audio y video"), L"*.mp3;*.flac;*.ogg;*.opus;*.wav;*.m4a;*.aac;*.wma;*.ape;*.wv;*.aiff;*.aif;*.mpc;*.tta;*.ac3;*.dts;*.dsf;*.dff;*.mka;*.mp2;*.mp4;*.mkv;*.webm;*.avi;*.mov;*.wmv;*.flv;*.mpg;*.mpeg;*.m4v;*.ts;*.m2ts;*.vob;*.ogv;*.3gp" }, { TX(L"Todos los archivos"), L"*.*" } });
    o.lpstrFilter = flt_.c_str();
    o.Flags = OFN_EXPLORER | OFN_ALLOWMULTISELECT | OFN_FILEMUSTEXIST | OFN_PATHMUSTEXIST;
    if (!GetOpenFileNameW(&o)) return;
    std::vector<std::wstring> paths;
    std::wstring first = buf.data();
    wchar_t* p = buf.data() + first.size() + 1;
    if (*p == 0) paths.push_back(first);
    else while (*p) { std::wstring f = p; paths.push_back(first + L"\\" + f); p += f.size() + 1; }
    addPaths(paths);
}

static void doOpenFolder() {
    BROWSEINFOW bi = {};
    bi.hwndOwner = g_hwnd;
    bi.lpszTitle = TX(L"Elige una carpeta de música");
    bi.ulFlags = BIF_RETURNONLYFSDIRS | BIF_NEWDIALOGSTYLE;
    LPITEMIDLIST pidl = SHBrowseForFolderW(&bi);
    if (!pidl) return;
    wchar_t path[MAX_PATH];
    if (SHGetPathFromIDListW(pidl, path)) addPaths({ path });
    CoTaskMemFree(pidl);
}

// ------------------------------------------------------------ interfaz
using Gdiplus::Graphics;
using Gdiplus::GraphicsPath;
using Gdiplus::SolidBrush;
using Gdiplus::Pen;
using Gdiplus::PointF;

// Paleta (estetica oscura, botones claros, onda roja)
static const COLORREF C_BG = RGB(45, 49, 56);          // fondo de la ventana
static const COLORREF C_TITLE = RGB(35, 38, 44);       // barra de titulo
static const COLORREF C_BAND = RGB(194, 199, 205);     // franja de botones
static const COLORREF C_TEXT = RGB(232, 234, 237);
static const COLORREF C_WAVEBG = RGB(37, 40, 46);
static const COLORREF C_WAVE_PLAYED = RGB(236, 98, 92);
static const COLORREF C_WAVE_REST = RGB(190, 68, 64);
static const COLORREF C_WAVE_CLIP = RGB(255, 214, 102);
static const COLORREF C_PANEL = RGB(246, 247, 249);

static const int CAP_H = 44;       // alto de la barra de titulo
static const int BAND_H = 62;      // alto de la franja de botones
static const int MARGIN = 14;
static const int CAP_BTN_W = 46;

static HFONT g_font = nullptr, g_fontBold = nullptr;
static HBRUSH g_brBg = nullptr;
static HICON g_iconSmall = nullptr;
static RECT g_panelRect = { 0, 0, 0, 0 };
static int g_capHot = -1, g_capDown = -1;
static HWND g_hotBtn = nullptr;
static ULONG_PTR g_gdipToken = 0;
static int g_videoAreaH = 0;
static HDC g_bbDC = nullptr;
static HBITMAP g_bbBmp = nullptr, g_bbOld = nullptr;
static int g_bbW = 0, g_bbH = 0;

static Gdiplus::Color Col(COLORREF c, int a = 255) {
    return Gdiplus::Color((BYTE)a, GetRValue(c), GetGValue(c), GetBValue(c));
}

static void rrPath(GraphicsPath& p, float x, float y, float w, float h, float r) {
    float d = r * 2;
    if (d > w) d = w;
    if (d > h) d = h;
    p.AddArc(x, y, d, d, 180, 90);
    p.AddArc(x + w - d, y, d, d, 270, 90);
    p.AddArc(x + w - d, y + h - d, d, d, 0, 90);
    p.AddArc(x, y + h - d, d, d, 90, 90);
    p.CloseFigure();
}

static void fillRect(HDC dc, const RECT& r, COLORREF c) {
    HBRUSH b = CreateSolidBrush(c);
    FillRect(dc, &r, b);
    DeleteObject(b);
}

enum { ICO_FILE, ICO_FOLDER, ICO_PREV, ICO_NEXT, ICO_STOP, ICO_PLAY, ICO_LIST, ICO_SPK, ICO_PAUSE, ICO_SAVE, ICO_IMPORT, ICO_EXPORT, ICO_INFO, ICO_CLOUD, ICO_FSEXIT, ICO_FSENTER, ICO_CAMERA, ICO_GEAR };

static void drawIcon(Graphics& g, int kind, float x, float y, float s, COLORREF col) {
    SolidBrush b(Col(col));
    switch (kind) {
    case ICO_FILE: {
        PointF pts[5] = { { x + s * 0.18f, y + s * 0.02f }, { x + s * 0.60f, y + s * 0.02f }, { x + s * 0.84f, y + s * 0.26f },
                          { x + s * 0.84f, y + s * 0.98f }, { x + s * 0.18f, y + s * 0.98f } };
        g.FillPolygon(&b, pts, 5);
        PointF fold[3] = { { x + s * 0.60f, y + s * 0.02f }, { x + s * 0.60f, y + s * 0.26f }, { x + s * 0.84f, y + s * 0.26f } };
        SolidBrush lt(Gdiplus::Color(120, 255, 255, 255));
        g.FillPolygon(&lt, fold, 3);
        break;
    }
    case ICO_FOLDER: {
        GraphicsPath tab, body;
        rrPath(tab, x, y + s * 0.12f, s * 0.46f, s * 0.34f, 2.f);
        rrPath(body, x, y + s * 0.28f, s, s * 0.62f, 2.5f);
        g.FillPath(&b, &tab);
        g.FillPath(&b, &body);
        break;
    }
    case ICO_PREV: {
        GraphicsPath bar;
        rrPath(bar, x + s * 0.14f, y + s * 0.16f, s * 0.14f, s * 0.68f, 1.f);
        g.FillPath(&b, &bar);
        PointF tri[3] = { { x + s * 0.88f, y + s * 0.16f }, { x + s * 0.36f, y + s * 0.5f }, { x + s * 0.88f, y + s * 0.84f } };
        g.FillPolygon(&b, tri, 3);
        break;
    }
    case ICO_NEXT: {
        GraphicsPath bar;
        rrPath(bar, x + s * 0.72f, y + s * 0.16f, s * 0.14f, s * 0.68f, 1.f);
        g.FillPath(&b, &bar);
        PointF tri[3] = { { x + s * 0.12f, y + s * 0.16f }, { x + s * 0.64f, y + s * 0.5f }, { x + s * 0.12f, y + s * 0.84f } };
        g.FillPolygon(&b, tri, 3);
        break;
    }
    case ICO_STOP: {
        GraphicsPath sq;
        rrPath(sq, x + s * 0.16f, y + s * 0.16f, s * 0.68f, s * 0.68f, 2.f);
        g.FillPath(&b, &sq);
        break;
    }
    case ICO_GEAR: {   // engranaje: anillo + 8 dientes
        const float cx = x + s * 0.5f, cy = y + s * 0.5f, R = s * 0.45f, r = s * 0.29f;
        for (int i = 0; i < 8; i++) {
            g.TranslateTransform(cx, cy);
            g.RotateTransform(i * 45.f);
            GraphicsPath tooth;
            rrPath(tooth, -s * 0.075f, -R, s * 0.15f, s * 0.22f, 1.f);
            g.FillPath(&b, &tooth);
            g.ResetTransform();
        }
        Pen gp(Col(col), s * 0.14f);
        g.DrawEllipse(&gp, cx - r, cy - r, r * 2, r * 2);
        break;
    }
    case ICO_CAMERA: {   // camara fotografica (contorno)
        Pen cp(Col(col), s * 0.09f);
        cp.SetLineJoin(Gdiplus::LineJoinRound);
        GraphicsPath body;
        rrPath(body, x + s * 0.08f, y + s * 0.28f, s * 0.84f, s * 0.56f, s * 0.10f);
        g.DrawPath(&cp, &body);
        GraphicsPath bump;
        rrPath(bump, x + s * 0.32f, y + s * 0.14f, s * 0.36f, s * 0.15f, s * 0.04f);
        g.FillPath(&b, &bump);
        g.DrawEllipse(&cp, x + s * 0.33f, y + s * 0.39f, s * 0.34f, s * 0.34f);
        break;
    }
    case ICO_FSENTER: {   // cuatro esquinas apuntando hacia afuera
        Pen fp(Col(col), s * 0.09f);
        fp.SetStartCap(Gdiplus::LineCapRound); fp.SetEndCap(Gdiplus::LineCapRound); fp.SetLineJoin(Gdiplus::LineJoinRound);
        PointF a1[3] = { { x + s * 0.40f, y + s * 0.12f }, { x + s * 0.12f, y + s * 0.12f }, { x + s * 0.12f, y + s * 0.40f } };
        PointF a2[3] = { { x + s * 0.60f, y + s * 0.12f }, { x + s * 0.88f, y + s * 0.12f }, { x + s * 0.88f, y + s * 0.40f } };
        PointF a3[3] = { { x + s * 0.12f, y + s * 0.60f }, { x + s * 0.12f, y + s * 0.88f }, { x + s * 0.40f, y + s * 0.88f } };
        PointF a4[3] = { { x + s * 0.88f, y + s * 0.60f }, { x + s * 0.88f, y + s * 0.88f }, { x + s * 0.60f, y + s * 0.88f } };
        g.DrawLines(&fp, a1, 3); g.DrawLines(&fp, a2, 3); g.DrawLines(&fp, a3, 3); g.DrawLines(&fp, a4, 3);
        break;
    }
    case ICO_FSEXIT: {   // cuatro esquinas apuntando hacia adentro
        Pen fp(Col(col), s * 0.09f);
        fp.SetStartCap(Gdiplus::LineCapRound); fp.SetEndCap(Gdiplus::LineCapRound); fp.SetLineJoin(Gdiplus::LineJoinRound);
        PointF a1[3] = { { x + s * 0.12f, y + s * 0.38f }, { x + s * 0.38f, y + s * 0.38f }, { x + s * 0.38f, y + s * 0.12f } };
        PointF a2[3] = { { x + s * 0.62f, y + s * 0.12f }, { x + s * 0.62f, y + s * 0.38f }, { x + s * 0.88f, y + s * 0.38f } };
        PointF a3[3] = { { x + s * 0.12f, y + s * 0.62f }, { x + s * 0.38f, y + s * 0.62f }, { x + s * 0.38f, y + s * 0.88f } };
        PointF a4[3] = { { x + s * 0.62f, y + s * 0.88f }, { x + s * 0.62f, y + s * 0.62f }, { x + s * 0.88f, y + s * 0.62f } };
        g.DrawLines(&fp, a1, 3); g.DrawLines(&fp, a2, 3); g.DrawLines(&fp, a3, 3); g.DrawLines(&fp, a4, 3);
        break;
    }
    case ICO_INFO: {
        Pen ip(Col(col), s * 0.10f);
        g.DrawEllipse(&ip, x + s * 0.08f, y + s * 0.08f, s * 0.84f, s * 0.84f);
        g.FillEllipse(&b, x + s * 0.445f, y + s * 0.25f, s * 0.11f, s * 0.11f);
        GraphicsPath bar;
        rrPath(bar, x + s * 0.445f, y + s * 0.43f, s * 0.11f, s * 0.31f, s * 0.04f);
        g.FillPath(&b, &bar);
        break;
    }
    case ICO_CLOUD: {
        GraphicsPath cl;
        cl.SetFillMode(Gdiplus::FillModeWinding);
        cl.AddEllipse(x + s * 0.06f, y + s * 0.42f, s * 0.36f, s * 0.36f);
        cl.AddEllipse(x + s * 0.24f, y + s * 0.18f, s * 0.44f, s * 0.44f);
        cl.AddEllipse(x + s * 0.52f, y + s * 0.36f, s * 0.40f, s * 0.42f);
        GraphicsPath base;
        rrPath(base, x + s * 0.14f, y + s * 0.54f, s * 0.72f, s * 0.26f, s * 0.13f);
        g.FillPath(&b, &cl);
        g.FillPath(&b, &base);
        break;
    }
    case ICO_IMPORT:
    case ICO_EXPORT: {
        Pen tp(Col(col), s * 0.11f);
        tp.SetStartCap(Gdiplus::LineCapRound); tp.SetEndCap(Gdiplus::LineCapRound); tp.SetLineJoin(Gdiplus::LineJoinRound);
        PointF tray[4] = { { x + s * 0.14f, y + s * 0.54f }, { x + s * 0.14f, y + s * 0.86f }, { x + s * 0.86f, y + s * 0.86f }, { x + s * 0.86f, y + s * 0.54f } };
        g.DrawLines(&tp, tray, 4);
        const bool in = kind == ICO_IMPORT;
        g.DrawLine(&tp, x + s * 0.5f, y + s * (in ? 0.10f : 0.66f), x + s * 0.5f, y + s * (in ? 0.66f : 0.10f));
        PointF head[3] = { { x + s * 0.28f, y + s * (in ? 0.46f : 0.30f) }, { x + s * 0.5f, y + s * (in ? 0.68f : 0.08f) }, { x + s * 0.72f, y + s * (in ? 0.46f : 0.30f) } };
        g.DrawLines(&tp, head, 3);
        break;
    }
    case ICO_SAVE: {
        GraphicsPath body;
        rrPath(body, x + s * 0.10f, y + s * 0.10f, s * 0.80f, s * 0.80f, s * 0.12f);
        g.FillPath(&b, &body);
        SolidBrush wb(Col(RGB(246, 247, 249)));
        GraphicsPath lab, shp;
        rrPath(lab, x + s * 0.27f, y + s * 0.52f, s * 0.46f, s * 0.38f, 1.f);
        rrPath(shp, x + s * 0.28f, y + s * 0.10f, s * 0.38f, s * 0.26f, 1.f);
        g.FillPath(&wb, &lab);
        g.FillPath(&wb, &shp);
        g.FillRectangle(&b, x + s * 0.52f, y + s * 0.15f, s * 0.08f, s * 0.15f);
        break;
    }
    case ICO_PAUSE: {
        GraphicsPath b1, b2;
        rrPath(b1, x + s * 0.20f, y + s * 0.14f, s * 0.22f, s * 0.72f, 1.5f);
        rrPath(b2, x + s * 0.58f, y + s * 0.14f, s * 0.22f, s * 0.72f, 1.5f);
        g.FillPath(&b, &b1);
        g.FillPath(&b, &b2);
        break;
    }
    case ICO_SPK: {
        GraphicsPath box;
        rrPath(box, x + s * 0.06f, y + s * 0.36f, s * 0.24f, s * 0.28f, 1.f);
        g.FillPath(&b, &box);
        PointF cone[4] = { { x + s * 0.28f, y + s * 0.36f }, { x + s * 0.54f, y + s * 0.14f }, { x + s * 0.54f, y + s * 0.86f }, { x + s * 0.28f, y + s * 0.64f } };
        g.FillPolygon(&b, cone, 4);
        Pen wp(Col(col), s * 0.09f);
        g.DrawArc(&wp, x + s * 0.42f, y + s * 0.30f, s * 0.30f, s * 0.40f, -55.f, 110.f);
        g.DrawArc(&wp, x + s * 0.44f, y + s * 0.12f, s * 0.46f, s * 0.76f, -55.f, 110.f);
        break;
    }
    case ICO_LIST: {
        for (int i = 0; i < 3; i++) {
            float yy = y + s * (0.20f + 0.28f * i);
            GraphicsPath dot, bar;
            rrPath(dot, x + s * 0.06f, yy, s * 0.14f, s * 0.14f, 1.f);
            rrPath(bar, x + s * 0.32f, yy, s * 0.62f, s * 0.14f, 1.f);
            g.FillPath(&b, &dot);
            g.FillPath(&b, &bar);
        }
        break;
    }
    case ICO_PLAY: {
        PointF tri[3] = { { x + s * 0.22f, y + s * 0.10f }, { x + s * 0.92f, y + s * 0.5f }, { x + s * 0.22f, y + s * 0.90f } };
        g.FillPolygon(&b, tri, 3);
        break;
    }
    }
}

// ------------------------------------------------------------ botones (dibujados a medida)
// ------------------------------------------------------------ apariencia: franja metalica oscura y botones "neon"
static int g_tbFreeX0 = 0, g_tbFreeX1 = 0;   // hueco libre de la franja (aqui se dibujan las pistas de circuito)

static COLORREF lerpC(COLORREF a, COLORREF b, float t) {
    return RGB((int)(GetRValue(a) + (GetRValue(b) - GetRValue(a)) * t), (int)(GetGValue(a) + (GetGValue(b) - GetGValue(a)) * t),
               (int)(GetBValue(a) + (GetBValue(b) - GetBValue(a)) * t));
}
// color de la franja en la fila yy (0 = borde superior): reborde claro arriba, cuerpo gris oscuro, reborde azulado abajo
static COLORREF bandColorAt(int yy) {
    struct Stop { int y; COLORREF c; };
    static const Stop st[] = { { 0, RGB(112, 116, 130) }, { 2, RGB(96, 100, 114) }, { 4, RGB(74, 77, 86) }, { 10, RGB(62, 65, 70) },
                               { 30, RGB(50, 53, 57) }, { 50, RGB(43, 46, 50) }, { 57, RGB(48, 57, 66) }, { 60, RGB(66, 80, 96) }, { 61, RGB(27, 30, 35) } };
    const int n = (int)(sizeof(st) / sizeof(st[0]));
    if (yy <= st[0].y) return st[0].c;
    for (int i = 1; i < n; i++)
        if (yy <= st[i].y) return lerpC(st[i - 1].c, st[i].c, (float)(yy - st[i - 1].y) / (float)(st[i].y - st[i - 1].y));
    return st[n - 1].c;
}

// pistas de circuito decorativas en el hueco libre de la franja
static void paintCircuits(HDC dc, int bandTop) {
    const int x0 = g_tbFreeX0, x1 = g_tbFreeX1;
    if (x1 - x0 < 90) return;
    const float ox = (float)x0, oy = (float)bandTop + 9.f, W = (float)(x1 - x0), H = (float)BAND_H - 20.f;
    Graphics g(dc);
    g.SetSmoothingMode(Gdiplus::SmoothingModeAntiAlias);
    Pen lp(Gdiplus::Color(120, 98, 103, 112), 1.2f);
    lp.SetLineJoin(Gdiplus::LineJoinRound);
    struct Poly { int n; float p[14]; };
    static const Poly polys[] = {
        { 6, { 0.00f, 0.30f, 0.25f, 0.30f, 0.35f, 0.15f, 0.70f, 0.15f, 0.80f, 0.30f, 1.00f, 0.30f } },
        { 6, { 0.00f, 0.62f, 0.18f, 0.62f, 0.28f, 0.80f, 0.60f, 0.80f, 0.70f, 0.62f, 1.00f, 0.62f } },
        { 3, { 0.20f, 0.08f, 0.20f, 0.45f, 0.32f, 0.55f } },
        { 4, { 0.55f, 0.95f, 0.55f, 0.62f, 0.65f, 0.50f, 0.90f, 0.50f } },
        { 2, { 0.40f, 0.30f, 0.40f, 0.52f } },
    };
    for (const Poly& po : polys) {
        std::vector<PointF> pts;
        for (int i = 0; i < po.n; i++) pts.push_back(PointF(ox + po.p[2 * i] * W, oy + po.p[2 * i + 1] * H));
        g.DrawLines(&lp, pts.data(), (INT)pts.size());
    }
    static const float vias[][2] = { { 0.00f, 0.30f }, { 0.00f, 0.62f }, { 0.32f, 0.55f }, { 0.90f, 0.50f }, { 0.40f, 0.52f }, { 0.20f, 0.08f }, { 1.00f, 0.30f }, { 1.00f, 0.62f } };
    Pen vp(Gdiplus::Color(130, 108, 113, 122), 1.2f);
    for (auto& v : vias) g.DrawEllipse(&vp, ox + v[0] * W - 2.6f, oy + v[1] * H - 2.6f, 5.2f, 5.2f);
    g.Flush();
}

struct Neon { COLORREF t1, t2, b1, b2, icon, text; };   // borde: mitad de arriba t1->t2, mitad de abajo b1->b2
static bool neonFor(int id, Neon& n) {
    const COLORREF txtLilac = RGB(206, 200, 240), txtWhite = RGB(234, 234, 242);
    switch (id) {
    case ID_OPEN:     n = { RGB(80, 205, 225), RGB(190, 140, 225), RGB(110, 195, 220), RGB(205, 150, 210), RGB(60, 205, 230), txtLilac }; return true;
    case ID_FOLDER:   n = { RGB(80, 205, 225), RGB(195, 140, 220), RGB(110, 195, 220), RGB(210, 150, 205), RGB(172, 156, 242), txtLilac }; return true;
    case ID_PREV:     n = { RGB(50, 205, 195), RGB(150, 60, 205), RGB(210, 60, 195), RGB(60, 195, 220), RGB(165, 112, 238), txtLilac }; return true;
    case ID_PLAY:     n = { RGB(60, 215, 195), RGB(130, 60, 215), RGB(195, 60, 195), RGB(60, 190, 220), RGB(172, 122, 248), txtLilac }; return true;
    case ID_STOP:     n = { RGB(75, 195, 215), RGB(155, 100, 190), RGB(60, 135, 180), RGB(125, 70, 165), RGB(238, 78, 78), txtLilac }; return true;
    case ID_NEXT:     n = { RGB(60, 125, 195), RGB(155, 70, 175), RGB(115, 175, 220), RGB(195, 85, 205), RGB(192, 124, 238), txtLilac }; return true;
    case ID_SPEED:    n = { RGB(232, 176, 190), RGB(190, 230, 220), RGB(232, 176, 190), RGB(190, 230, 220), txtWhite, txtWhite }; return true;
    case ID_LISTBTN:  n = { RGB(100, 190, 220), RGB(180, 120, 230), RGB(135, 155, 220), RGB(205, 112, 220), RGB(166, 154, 244), txtLilac }; return true;
    case ID_OUTBTN:   n = { RGB(115, 145, 190), RGB(200, 190, 230), RGB(85, 105, 145), RGB(155, 145, 195), RGB(172, 142, 238), txtLilac }; return true;
    case ID_MUTE:     n = { RGB(24, 210, 198), RGB(24, 210, 198), RGB(24, 200, 190), RGB(24, 200, 190), txtWhite, txtWhite }; return true;
    case ID_RESET:    n = { RGB(190, 72, 205), RGB(228, 118, 232), RGB(205, 90, 215), RGB(235, 130, 235), txtWhite, txtWhite }; return true;
    case ID_SPDRESET: n = { RGB(190, 72, 205), RGB(228, 118, 232), RGB(205, 90, 215), RGB(235, 130, 235), txtWhite, txtWhite }; return true;
    case ID_SNAP:     n = { RGB(36, 190, 178), RGB(36, 190, 178), RGB(30, 175, 165), RGB(30, 175, 165), RGB(48, 205, 192), txtWhite }; return true;
    case ID_SNAPDIR:  n = { RGB(20, 215, 200), RGB(20, 215, 200), RGB(16, 195, 182), RGB(16, 195, 182), RGB(8, 215, 200), txtWhite }; return true;
    case ID_SNAPOPT:  n = { RGB(198, 92, 215), RGB(176, 96, 215), RGB(176, 90, 205), RGB(200, 110, 215), RGB(205, 105, 225), txtWhite }; return true;
    case ID_NETURL:   n = { RGB(214, 150, 232), RGB(196, 130, 232), RGB(200, 125, 225), RGB(220, 150, 232), RGB(222, 150, 240), txtWhite }; return true;
    case ID_ABOUT:    n = { RGB(70, 100, 235), RGB(100, 120, 240), RGB(60, 90, 220), RGB(90, 110, 235), RGB(100, 130, 248), txtWhite }; return true;
    }
    return false;
}

static void drawNeon(HDC dc, const DRAWITEMSTRUCT* di, RECT r, int icon, bool iconOnly, const wchar_t* txt, bool down, bool hot, bool toolbar, const Neon& n) {
    // fondo: el mismo degradado de la franja (o el fondo de la ventana)
    if (toolbar) {
        POINT pt = { 0, 0 };
        MapWindowPoints(di->hwndItem, g_hwnd, &pt, 1);
        for (int yy = r.top; yy < r.bottom; yy++) {
            RECT row = { r.left, yy, r.right, yy + 1 };
            fillRect(dc, row, bandColorAt(pt.y + yy - CAP_H));
        }
    } else {
        fillRect(dc, r, C_BG);
    }
    const float inset = (r.bottom - r.top) <= 32 ? 2.5f : 3.5f;
    const float x = r.left + inset, y = r.top + inset + (down ? 1.f : 0.f);
    const float w = (float)(r.right - r.left) - 2 * inset, h = (float)(r.bottom - r.top) - 2 * inset;
    Graphics g(dc);
    g.SetSmoothingMode(Gdiplus::SmoothingModeAntiAlias);
    GraphicsPath p;
    rrPath(p, x, y, w, h, 10.f);
    // brillo suave alrededor (promedio de los colores del borde)
    const int ar = (GetRValue(n.t1) + GetRValue(n.t2) + GetRValue(n.b1) + GetRValue(n.b2)) / 4;
    const int ag = (GetGValue(n.t1) + GetGValue(n.t2) + GetGValue(n.b1) + GetGValue(n.b2)) / 4;
    const int ab = (GetBValue(n.t1) + GetBValue(n.t2) + GetBValue(n.b1) + GetBValue(n.b2)) / 4;
    const float boost = hot ? 1.9f : 1.f;
    { Pen gp(Gdiplus::Color((BYTE)std::min(255.f, 22 * boost), ar, ag, ab), 6.f); g.DrawPath(&gp, &p); }
    { Pen gp(Gdiplus::Color((BYTE)std::min(255.f, 40 * boost), ar, ag, ab), 4.2f); g.DrawPath(&gp, &p); }
    // cara oscura con un leve degradado vertical
    {
        const int d = down ? -8 : (hot ? 8 : 0);
        Gdiplus::LinearGradientBrush fb(PointF(x, y), PointF(x, y + h), Gdiplus::Color(255, 50 + d, 53 + d, 59 + d), Gdiplus::Color(255, 31 + d, 33 + d, 39 + d));
        g.FillPath(&fb, &p);
    }
    // borde en degradado: mitad de arriba t1->t2, mitad de abajo b1->b2
    {
        const float bw = hot ? 2.4f : 2.f;
        Gdiplus::LinearGradientBrush bt(PointF(x, 0), PointF(x + w, 0), Col(n.t1), Col(n.t2));
        Gdiplus::LinearGradientBrush bb(PointF(x, 0), PointF(x + w, 0), Col(n.b1), Col(n.b2));
        Pen pt(&bt, bw), pb(&bb, bw);
        g.SetClip(Gdiplus::RectF(x - 6, y - 6, w + 12, h / 2.f + 6));
        g.DrawPath(&pt, &p);
        g.SetClip(Gdiplus::RectF(x - 6, y + h / 2.f, w + 12, h / 2.f + 6));
        g.DrawPath(&pb, &p);
        g.ResetClip();
    }
    const float s = h >= 30 ? 18.f : 16.f;
    const float cy = y + h / 2.f;
    if (iconOnly) drawIcon(g, icon, x + (w - s) / 2, cy - s / 2, s, n.icon);
    else if (icon >= 0) drawIcon(g, icon, x + 12, cy - s / 2, s, n.icon);
    g.Flush();
    if (!iconOnly) {
        RECT tr = { (LONG)x, (LONG)y, (LONG)(x + w), (LONG)(y + h) };
        if (icon >= 0) tr.left += 12 + (int)s + 8;
        SelectObject(dc, g_font);
        SetBkMode(dc, TRANSPARENT);
        SetTextColor(dc, n.text);
        DrawText(dc, txt, -1, &tr, (icon >= 0 ? DT_LEFT : DT_CENTER) | DT_VCENTER | DT_SINGLELINE);
    }
}

static void drawButton(const DRAWITEMSTRUCT* di) {
    HDC dc = di->hDC;
    RECT r = di->rcItem;
    const int id = (int)di->CtlID;
    const bool down = (di->itemState & ODS_SELECTED) != 0;
    const bool hot = g_hotBtn == di->hwndItem;
    const bool toolbar = (id >= ID_OPEN && id <= ID_NEXT) || id == ID_LISTBTN || id == ID_OUTBTN || id == ID_SPEED;
    wchar_t txt[64] = L"";
    GetWindowText(di->hwndItem, txt, 64);
    const bool hdrBtn = id == ID_PLSAVE || id == ID_PLIMP || id == ID_PLEXP;
    { Neon n0; if (!neonFor(id, n0)) fillRect(dc, r, toolbar ? C_BAND : (hdrBtn ? RGB(226, 229, 234) : C_BG)); }
    const float x = r.left + 0.5f, y = r.top + 0.5f, w = (float)(r.right - r.left) - 1.f, h = (float)(r.bottom - r.top) - 1.f;

    if (hdrBtn) {   // botones planos de la cabecera de playlists
        Graphics g(dc);
        g.SetSmoothingMode(Gdiplus::SmoothingModeAntiAlias);
        if (hot || down) {
            GraphicsPath hp;
            rrPath(hp, x + 1, y + 1, w - 2, h - 2, 7.f);
            SolidBrush hb(Col(down ? RGB(196, 201, 208) : RGB(211, 215, 221)));
            g.FillPath(&hb, &hp);
        }
        const int ic = id == ID_PLIMP ? ICO_IMPORT : id == ID_PLEXP ? ICO_EXPORT : ICO_SAVE;
        const float is = 18.f;
        drawIcon(g, ic, x + (w - is) / 2, y + (h - is) / 2, is, id == ID_PLSAVE ? RGB(214, 48, 60) : RGB(58, 63, 71));
        g.Flush();
        return;
    }

    if (id == ID_LIMIT || id == ID_PEAKON) {   // casillas
        const bool checked = id == ID_LIMIT ? g_limiter.load() : g_peakOn.load();
        {
            Graphics g(dc);
            g.SetSmoothingMode(Gdiplus::SmoothingModeAntiAlias);
            const float bs = 20.f, by = y + (h - bs) / 2;
            GraphicsPath p;
            rrPath(p, x + 1, by, bs, bs, 5.f);
            if (checked) {
                SolidBrush fb(Col(RGB(205, 120, 118)));
                g.FillPath(&fb, &p);
                Pen ck(Col(RGB(255, 255, 255)), 2.2f);
                g.DrawLine(&ck, x + 6, by + 10.5f, x + 10, by + 14.5f);
                g.DrawLine(&ck, x + 10, by + 14.5f, x + 16.5f, by + 6.5f);
            } else {
                Pen pen(Col(RGB(200, 204, 210)), 1.6f);
                g.DrawPath(&pen, &p);
            }
            g.Flush();
        }
        RECT tr = { r.left + 32, r.top, r.right, r.bottom };
        SelectObject(dc, g_font);
        SetBkMode(dc, TRANSPARENT);
        SetTextColor(dc, C_TEXT);
        DrawText(dc, txt, -1, &tr, DT_LEFT | DT_VCENTER | DT_SINGLELINE);
        return;
    }

    COLORREF face = toolbar ? (down ? RGB(205, 210, 216) : hot ? RGB(248, 249, 250) : RGB(237, 239, 242))
                            : (down ? RGB(188, 193, 200) : hot ? RGB(238, 240, 243) : RGB(223, 227, 231));
    int icon = -1;
    switch (id) {
    case ID_OPEN: icon = ICO_FILE; break;
    case ID_FOLDER: icon = ICO_FOLDER; break;
    case ID_PREV: icon = ICO_PREV; break;
    case ID_NEXT: icon = ICO_NEXT; break;
    case ID_STOP: icon = ICO_STOP; break;
    case ID_LISTBTN: icon = ICO_LIST; break;
    case ID_OUTBTN: icon = ICO_SPK; break;
    case ID_PLAY: icon = g_state == ST_PLAYING ? ICO_PAUSE : ICO_PLAY; break;
    case ID_PLSAVE: icon = ICO_SAVE; break;
    case ID_ABOUT: icon = ICO_INFO; break;
    case ID_NETURL: icon = ICO_CLOUD; break;
    case ID_SNAP: icon = ICO_CAMERA; break;
    case ID_SNAPDIR: icon = ICO_FOLDER; break;
    case ID_SNAPOPT: icon = ICO_GEAR; break;
    }
    COLORREF ink = RGB(58, 63, 71);
    if (id == ID_SNAP || id == ID_SNAPDIR) ink = RGB(16, 178, 164);   // turquesa: captura y su carpeta van juntas
    else if (id == ID_ABOUT) ink = RGB(59, 130, 246);                // azul llamativo (sin exagerar) para invitar a hacer clic
    const bool iconOnly = id == ID_PREV || id == ID_NEXT || id == ID_LISTBTN || id == ID_OUTBTN || id == ID_FOLDER || id == ID_PLAY || id == ID_PLSAVE || id == ID_ABOUT || id == ID_NETURL || id == ID_SNAP || id == ID_SNAPDIR || id == ID_SNAPOPT ||
                          (g_tbCompact && (id == ID_OPEN || id == ID_STOP));
    const float s = 18.f;
    { Neon nn; if (neonFor(id, nn)) { drawNeon(dc, di, r, icon, iconOnly, txt, down, hot, toolbar, nn); return; } }
    {
        Graphics g(dc);
        g.SetSmoothingMode(Gdiplus::SmoothingModeAntiAlias);
        if (!down) {
            GraphicsPath sp;
            rrPath(sp, x, y + 2, w, h - 2, 9.f);
            SolidBrush sb(Col(RGB(0, 0, 0), toolbar ? 55 : 80));
            g.FillPath(&sb, &sp);
        }
        GraphicsPath p;
        rrPath(p, x, y + (down ? 1.f : 0.f), w, h - 3, 9.f);
        SolidBrush fb(Col(face));
        g.FillPath(&fb, &p);
        Pen bp(Col(toolbar ? RGB(168, 174, 182) : RGB(130, 136, 144)), 1.f);
        g.DrawPath(&bp, &p);
        float cy = y + (h - 3) / 2 + (down ? 1.f : 0.f);
        if (iconOnly) drawIcon(g, icon, x + (w - s) / 2, cy - s / 2, s, ink);
        else if (icon >= 0) drawIcon(g, icon, x + 14, cy - s / 2, s, ink);
        g.Flush();
    }
    if (!iconOnly) {
        RECT tr = r;
        if (icon >= 0) tr.left += 14 + (int)s + 8; else tr.left += 4;
        tr.bottom -= 3;
        if (down) OffsetRect(&tr, 0, 1);
        SelectObject(dc, g_font);
        SetBkMode(dc, TRANSPARENT);
        SetTextColor(dc, RGB(43, 47, 54));
        DrawText(dc, txt, -1, &tr, (icon >= 0 ? DT_LEFT : DT_CENTER) | DT_VCENTER | DT_SINGLELINE);
    }
}

static LRESULT CALLBACK BtnProc(HWND h, UINT m, WPARAM w, LPARAM l, UINT_PTR, DWORD_PTR) {
    // drawButton() ya pinta todo el rectangulo; si Windows ademas lo borra con el color de boton (claro), al estirar la
    // ventana se ven cuadros blancos hasta que llega el repintado.
    if (m == WM_ERASEBKGND) return 1;
    if (m == WM_MOUSEMOVE && g_hotBtn != h) {
        HWND old = g_hotBtn;
        g_hotBtn = h;
        if (old) InvalidateRect(old, nullptr, FALSE);
        InvalidateRect(h, nullptr, FALSE);
        TRACKMOUSEEVENT te = { sizeof(te), TME_LEAVE, h, 0 };
        TrackMouseEvent(&te);
    } else if (m == WM_MOUSELEAVE) {
        if (g_hotBtn == h) { g_hotBtn = nullptr; InvalidateRect(h, nullptr, FALSE); }
    }
    return DefSubclassProc(h, m, w, l);
}

// ------------------------------------------------------------ sliders (dibujo a medida)
static LRESULT sliderDraw(NMCUSTOMDRAW* cd) {
    HWND s = cd->hdr.hwndFrom;
    HDC dc = cd->hdc;
    if (cd->dwDrawStage == CDDS_PREPAINT) {
        RECT rc;
        GetClientRect(s, &rc);
        fillRect(dc, rc, C_BG);
        return CDRF_NOTIFYITEMDRAW;
    }
    if (cd->dwDrawStage == CDDS_ITEMPREPAINT) {
        if (cd->dwItemSpec == TBCD_TICS) return CDRF_SKIPDEFAULT;
        const bool isVol = s == g_vol;
        if (cd->dwItemSpec == TBCD_CHANNEL) {
            RECT ch = cd->rc, tr;
            SendMessage(s, TBM_GETTHUMBRECT, 0, (LPARAM)&tr);
            float cy = (ch.top + ch.bottom) / 2.f;
            float x = (float)ch.left, w = (float)(ch.right - ch.left), hh = 8.f;
            Graphics g(dc);
            g.SetSmoothingMode(Gdiplus::SmoothingModeAntiAlias);
            GraphicsPath p;
            rrPath(p, x, cy - hh / 2, w, hh, 4.f);
            SolidBrush tb(Col(RGB(27, 30, 35)));
            g.FillPath(&tb, &p);
            if (isVol) {
                float fx = (tr.left + tr.right) / 2.f - x;
                if (fx > 2) {
                    GraphicsPath fp;
                    rrPath(fp, x, cy - hh / 2, fx, hh, 4.f);
                    SolidBrush fb(Col(RGB(236, 98, 92)));   // relleno rojo (distinto de la perilla clara)
                    g.FillPath(&fb, &fp);
                }
            }
            g.Flush();
            return CDRF_SKIPDEFAULT;
        }
        if (cd->dwItemSpec == TBCD_THUMB) {
            RECT t = cd->rc;
            Graphics g(dc);
            g.SetSmoothingMode(Gdiplus::SmoothingModeAntiAlias);
            GraphicsPath p;
            rrPath(p, t.left - 1.f + 0.5f, t.top + 0.5f, (float)(t.right - t.left) + 2.f - 1.f, (float)(t.bottom - t.top) - 1.f, 6.f);
            SolidBrush fb(Col(isVol ? RGB(250, 251, 252) : RGB(186, 130, 128)));
            g.FillPath(&fb, &p);
            Pen bp(Col(isVol ? RGB(27, 30, 35) : RGB(118, 76, 75)), 1.4f);
            g.DrawPath(&bp, &p);
            g.Flush();
            return CDRF_SKIPDEFAULT;
        }
    }
    return CDRF_DODEFAULT;
}

// Dibujo completo del slider en un DC (fondo + canal + perilla). Lo usa el subclass en WM_PAINT, asi no
// depende del dibujo personalizado del trackbar, que dejaba de pintarse al redimensionar la ventana.
static void drawSliderInto(HDC dc, HWND s) {
    RECT rc;
    GetClientRect(s, &rc);
    fillRect(dc, rc, C_BG);
    RECT ch = {}, tr = {};
    SendMessage(s, TBM_GETCHANNELRECT, 0, (LPARAM)&ch);
    SendMessage(s, TBM_GETTHUMBRECT, 0, (LPARAM)&tr);
    const bool isVol = s == g_vol;
    float cy = (ch.top + ch.bottom) / 2.f;
    float x = (float)ch.left, w = (float)(ch.right - ch.left), hh = 8.f;
    Graphics g(dc);
    g.SetSmoothingMode(Gdiplus::SmoothingModeAntiAlias);
    GraphicsPath p;
    rrPath(p, x, cy - hh / 2, w, hh, 4.f);
    SolidBrush tb(Col(RGB(27, 30, 35)));
    g.FillPath(&tb, &p);
    if (isVol) {
        float fx = (tr.left + tr.right) / 2.f - x;
        if (fx > 2) {
            GraphicsPath fp;
            rrPath(fp, x, cy - hh / 2, fx, hh, 4.f);
            SolidBrush fb(Col(RGB(236, 98, 92)));
            g.FillPath(&fb, &fp);
        }
    }
    if (isVol) {   // marcas del volumen: encima de la barra (las de la ganancia van debajo, ver paintTicks)
        const int mn = (int)SendMessage(s, TBM_GETRANGEMIN, 0, 0), mx = (int)SendMessage(s, TBM_GETRANGEMAX, 0, 0);
        const int step = (int)(INT_PTR)GetProp(s, L"tick");
        if (step > 0 && mx > mn) {
            Pen tkp(Col(RGB(250, 251, 252)), 1.f);
            const float tw = 11.f, y2 = std::max(6.f, cy - 8.f), y1 = y2 - 5.f;
            for (int v = mn; v <= mx; v += step) {
                float tx = rc.left + tw / 2 + (float)(v - mn) / (mx - mn) * ((float)(rc.right - rc.left) - tw);
                g.DrawLine(&tkp, tx, y1, tx, y2);
            }
        }
    }
    GraphicsPath tp;
    rrPath(tp, tr.left - 1.f + 0.5f, tr.top + 0.5f, (float)(tr.right - tr.left) + 2.f - 1.f, (float)(tr.bottom - tr.top) - 1.f, 6.f);
    SolidBrush thb(Col(isVol ? RGB(250, 251, 252) : RGB(186, 130, 128)));
    g.FillPath(&thb, &tp);
    Pen bp(Col(isVol ? RGB(27, 30, 35) : RGB(118, 76, 75)), 1.4f);
    g.DrawPath(&bp, &tp);
    g.Flush();
}

static LRESULT CALLBACK SliderProc(HWND h, UINT m, WPARAM w, LPARAM l, UINT_PTR, DWORD_PTR) {
    if (m == WM_ERASEBKGND) return 1;
    // Rueda sobre volumen y ganancia: hacia arriba sube, hacia abajo baja (igual que la rueda sobre el resto de la ventana).
    // El control de Windows hace lo contrario, por eso se maneja aqui.
    if (m == WM_MOUSEWHEEL && (h == g_vol || h == g_gain)) {
        static int accV = 0, accG = 0;   // acumula giros finos de ruedas de alta resolucion
        int& acc = h == g_vol ? accV : accG;
        acc += GET_WHEEL_DELTA_WPARAM(w);
        const int notches = acc / WHEEL_DELTA;
        if (notches == 0) return 0;
        acc -= notches * WHEEL_DELTA;
        const int mn = (int)SendMessage(h, TBM_GETRANGEMIN, 0, 0), mx = (int)SendMessage(h, TBM_GETRANGEMAX, 0, 0);
        const int step = h == g_vol ? 5 : 2;   // volumen: 5 %, ganancia: 1 dB
        const int pos = std::max(mn, std::min(mx, (int)SendMessage(h, TBM_GETPOS, 0, 0) + notches * step));
        SendMessage(h, TBM_SETPOS, TRUE, pos);
        SendMessage(GetParent(h), WM_HSCROLL, MAKEWPARAM(TB_THUMBPOSITION, pos), (LPARAM)h);
        return 0;
    }
    // Clic en la linea de la ganancia o del volumen (fuera de la perilla): va a la marca mas cercana al clic, sin saltos de pagina.
    if (m == WM_LBUTTONDOWN && (h == g_gain || h == g_vol)) {
        RECT tr;
        SendMessage(h, TBM_GETTHUMBRECT, 0, (LPARAM)&tr);
        const POINT p = { GET_X_LPARAM(l), GET_Y_LPARAM(l) };
        if (!PtInRect(&tr, p)) {
            RECT rc;
            GetClientRect(h, &rc);
            const int mn = (int)SendMessage(h, TBM_GETRANGEMIN, 0, 0), mx = (int)SendMessage(h, TBM_GETRANGEMAX, 0, 0);
            const int step = std::max(1, (int)(INT_PTR)GetProp(h, L"tick"));
            const float tw = 11.f;   // mismo ancho de perilla que usa paintTicks() para dibujar las marcas
            const float frac = std::max(0.f, std::min(1.f, ((float)p.x - tw / 2) / std::max(1.f, (float)rc.right - tw)));
            const float raw = mn + frac * (mx - mn);
            const int pos = std::max(mn, std::min(mx, mn + (int)lrintf((raw - mn) / step) * step));
            SetFocus(h);
            SendMessage(h, TBM_SETPOS, TRUE, pos);
            SendMessage(GetParent(h), WM_HSCROLL, MAKEWPARAM(TB_THUMBPOSITION, pos), (LPARAM)h);
            return 0;
        }
    }
    if (m == WM_PAINT || m == WM_PRINTCLIENT) {
        PAINTSTRUCT ps;
        HDC dc = m == WM_PAINT ? BeginPaint(h, &ps) : (HDC)w;
        RECT rc;
        GetClientRect(h, &rc);
        const int W = std::max((int)rc.right, 1), H = std::max((int)rc.bottom, 1);
        HDC mem = CreateCompatibleDC(dc);
        HBITMAP bmp = CreateCompatibleBitmap(dc, W, H);
        HGDIOBJ old = SelectObject(mem, bmp);
        drawSliderInto(mem, h);
        BitBlt(dc, 0, 0, W, H, mem, 0, 0, SRCCOPY);
        SelectObject(mem, old);
        DeleteObject(bmp);
        DeleteDC(mem);
        if (m == WM_PAINT) EndPaint(h, &ps);
        return 0;
    }
    return DefSubclassProc(h, m, w, l);
}

// ------------------------------------------------------------ lista (dibujo a medida)
// ------------------------------------------------------------ playlists guardadas (.txt en la carpeta "playlists")
static std::wstring exeDir() {
    wchar_t b[MAX_PATH * 2];
    DWORD n = GetModuleFileNameW(nullptr, b, MAX_PATH * 2);
    std::wstring p(b, n);
    size_t i = p.find_last_of(L"\\/");
    return i == std::wstring::npos ? L"." : p.substr(0, i);
}

// true si el programa corre instalado como paquete MSIX (Microsoft Store): su carpeta es de solo lectura y las
// asociaciones de archivos las declara el manifiesto del paquete
static bool isPackaged() {
    static int v = -1;
    if (v < 0) {
        typedef LONG (WINAPI *Fn)(UINT32*, PWSTR);
        Fn f = (Fn)GetProcAddress(GetModuleHandleW(L"kernel32.dll"), "GetCurrentPackageFullName");
        UINT32 n = 0;
        v = (f && f(&n, nullptr) == ERROR_INSUFFICIENT_BUFFER) ? 1 : 0;   // sin paquete devuelve APPMODEL_ERROR_NO_PACKAGE
    }
    return v == 1;
}

static std::wstring plDir() {   // <carpeta del programa>\playlists (si no se puede escribir ahi: %APPDATA%\MiauPlayer\playlists)
    static std::wstring d;
    if (!d.empty()) return d;
    if (isPackaged()) {   // en el paquete MSIX no se puede escribir junto al ejecutable: Documentos\MiauPlayer\playlist
        wchar_t dp[MAX_PATH];
        if (SUCCEEDED(SHGetFolderPathW(nullptr, CSIDL_PERSONAL, nullptr, 0, dp))) {
            std::wstring b = std::wstring(dp) + L"\\MiauPlayer";
            CreateDirectoryW(b.c_str(), nullptr);
            b += L"\\playlist";
            CreateDirectoryW(b.c_str(), nullptr);
            return d = b;
        }
    }
    const std::wstring a = exeDir() + L"\\playlist";
    CreateDirectoryW(a.c_str(), nullptr);
    HANDLE h = CreateFileW((a + L"\\.escritura").c_str(), GENERIC_WRITE, 0, nullptr, CREATE_ALWAYS,
                           FILE_ATTRIBUTE_TEMPORARY | FILE_FLAG_DELETE_ON_CLOSE, nullptr);
    if (h != INVALID_HANDLE_VALUE) { CloseHandle(h); return d = a; }
    wchar_t ap[MAX_PATH];
    if (SUCCEEDED(SHGetFolderPathW(nullptr, CSIDL_APPDATA, nullptr, 0, ap))) {
        std::wstring b = std::wstring(ap) + L"\\MiauPlayer";
        CreateDirectoryW(b.c_str(), nullptr);
        b += L"\\playlist";
        CreateDirectoryW(b.c_str(), nullptr);
        return d = b;
    }
    return d = a;
}

static std::wstring plPath(const std::wstring& name) { return plDir() + L"\\" + name + L".txt"; }

static std::wstring sanitizeName(std::wstring n) {
    for (auto& c : n) if (c < 32 || wcschr(L"<>:\"/\\|?*", c)) c = L'_';
    size_t a = n.find_first_not_of(L" .");
    if (a == std::wstring::npos) return L"";
    size_t b = n.find_last_not_of(L" .");
    n = n.substr(a, b - a + 1);
    if (n.size() > 80) n.resize(80);
    return n;
}

static bool readPlaylistFile(const std::wstring& path, std::vector<std::wstring>& out);
static bool writeLinesFile(const std::wstring& path, const std::vector<std::wstring>& lines);

// Orden manual de las listas guardadas: playlist\orden.lst (un nombre por linea). Las listas que no estan en el archivo
// (nuevas) van al final, en orden alfabetico.
static void applySavedOrder() {
    std::vector<std::wstring> ord, res;
    if (!readPlaylistFile(plDir() + L"\\orden.lst", ord) || ord.empty()) return;
    for (auto& o : ord)
        for (auto it = g_savedNames.begin(); it != g_savedNames.end(); ++it)
            if (_wcsicmp(it->c_str(), o.c_str()) == 0) { res.push_back(*it); g_savedNames.erase(it); break; }
    res.insert(res.end(), g_savedNames.begin(), g_savedNames.end());
    g_savedNames = res;
}
static void saveSavedOrder() { writeLinesFile(plDir() + L"\\orden.lst", g_savedNames); }
static void fillSavedListBox() {
    if (!g_plList) return;
    SendMessage(g_plList, LB_RESETCONTENT, 0, 0);
    for (auto& n : g_savedNames) SendMessage(g_plList, LB_ADDSTRING, 0, (LPARAM)n.c_str());
    InvalidateRect(g_plList, nullptr, TRUE);
}

static void refreshSavedLists() {
    g_savedNames.clear();
    WIN32_FIND_DATAW fd;
    HANDLE h = FindFirstFileW((plDir() + L"\\*.txt").c_str(), &fd);
    if (h != INVALID_HANDLE_VALUE) {
        do {
            if (fd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) continue;
            std::wstring n = fd.cFileName;
            g_savedNames.push_back(n.substr(0, n.size() - 4));
        } while (FindNextFileW(h, &fd));
        FindClose(h);
    }
    std::sort(g_savedNames.begin(), g_savedNames.end(), [](const std::wstring& a, const std::wstring& b) { return _wcsicmp(a.c_str(), b.c_str()) < 0; });
    applySavedOrder();
    fillSavedListBox();
}

static bool writeLinesFile(const std::wstring& path, const std::vector<std::wstring>& lines) {   // una ruta por linea, UTF-8
    std::wstring text;
    for (auto& f : lines) text += f + L"\r\n";
    int n = WideCharToMultiByte(CP_UTF8, 0, text.c_str(), (int)text.size(), nullptr, 0, nullptr, nullptr);
    std::string u(n, 0);
    if (n > 0) WideCharToMultiByte(CP_UTF8, 0, text.c_str(), (int)text.size(), &u[0], n, nullptr, nullptr);
    HANDLE h = CreateFileW(path.c_str(), GENERIC_WRITE, 0, nullptr, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr);
    if (h == INVALID_HANDLE_VALUE) return false;
    DWORD wr = 0;
    static const unsigned char bom[3] = { 0xEF, 0xBB, 0xBF };
    bool ok = WriteFile(h, bom, 3, &wr, nullptr) && (u.empty() || WriteFile(h, u.data(), (DWORD)u.size(), &wr, nullptr));
    CloseHandle(h);
    return ok;
}

// Rutas portables: lo que cuelga de la carpeta del usuario se guarda como %USERPROFILE%\... (asi la lista sirve en otra
// computadora u otro usuario, por ejemplo con Dropbox en C:\Users\<usuario>\Dropbox). Al cargar se expanden las variables.
static std::wstring toPortablePath(const std::wstring& p) {
    if (isUrl(p)) return p;
    wchar_t up[MAX_PATH] = L"";
    const DWORD n = GetEnvironmentVariableW(L"USERPROFILE", up, MAX_PATH);
    if (n == 0 || n >= MAX_PATH) return p;
    std::wstring pre = up;
    while (!pre.empty() && pre.back() == L'\\') pre.pop_back();
    if (p.size() > pre.size() + 1 && _wcsnicmp(p.c_str(), pre.c_str(), pre.size()) == 0 && p[pre.size()] == L'\\')
        return L"%USERPROFILE%" + p.substr(pre.size());
    return p;
}
static std::wstring expandPortablePath(const std::wstring& p) {
    if (isUrl(p) || p.find(L'%') == std::wstring::npos) return p;
    wchar_t out[MAX_PATH * 4];
    const DWORD n = ExpandEnvironmentStringsW(p.c_str(), out, MAX_PATH * 4);
    return (n > 0 && n <= MAX_PATH * 4) ? std::wstring(out) : p;
}
static std::vector<std::wstring> portableLines(const std::vector<std::wstring>& v) {
    std::vector<std::wstring> o;
    for (auto& x : v) o.push_back(toPortablePath(x));
    return o;
}
static bool writePlaylistFile(const std::wstring& name) { return writeLinesFile(plPath(name), portableLines(g_playlist)); }

static bool readPlaylistFile(const std::wstring& path, std::vector<std::wstring>& out) {
    HANDLE h = CreateFileW(path.c_str(), GENERIC_READ, FILE_SHARE_READ, nullptr, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr);
    if (h == INVALID_HANDLE_VALUE) return false;
    DWORD sz = GetFileSize(h, nullptr), rd = 0;
    std::string u(sz == INVALID_FILE_SIZE ? 0 : sz, 0);
    if (!u.empty()) ReadFile(h, &u[0], sz, &rd, nullptr);
    CloseHandle(h);
    size_t off = (u.size() >= 3 && (unsigned char)u[0] == 0xEF && (unsigned char)u[1] == 0xBB && (unsigned char)u[2] == 0xBF) ? 3 : 0;
    int n = MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, u.data() + off, (int)(u.size() - off), nullptr, 0);
    UINT cp = CP_UTF8;
    if (n <= 0 && u.size() > off) { cp = CP_ACP; n = MultiByteToWideChar(cp, 0, u.data() + off, (int)(u.size() - off), nullptr, 0); }
    std::wstring w(n > 0 ? n : 0, 0);
    if (n > 0) MultiByteToWideChar(cp, 0, u.data() + off, (int)(u.size() - off), &w[0], n);
    size_t i = 0;
    while (i <= w.size()) {
        size_t j = w.find(L'\n', i);
        if (j == std::wstring::npos) j = w.size();
        std::wstring line = w.substr(i, j - i);
        while (!line.empty() && (line.back() == L'\r' || line.back() == L' ')) line.pop_back();
        if (!line.empty() && line[0] != L'#') out.push_back(line);
        i = j + 1;
    }
    return true;
}

// ---- dialogo simple para pedir un nombre (plantilla en memoria, sin recursos)
static std::wstring g_dlgText, g_dlgLabel, g_dlgTitle;
static int g_dlgMax = 80;
static INT_PTR CALLBACK PromptProc(HWND d, UINT m, WPARAM w, LPARAM) {
    switch (m) {
    case WM_INITDIALOG: {
        SetWindowText(d, g_dlgTitle.c_str());
        SetDlgItemText(d, 100, g_dlgLabel.c_str());
        SetDlgItemText(d, 101, g_dlgText.c_str());
        SendDlgItemMessage(d, 101, EM_SETLIMITTEXT, g_dlgMax, 0);
        SendDlgItemMessage(d, 101, EM_SETSEL, 0, -1);
        RECT pr, dr;
        GetWindowRect(g_hwnd, &pr);
        GetWindowRect(d, &dr);
        SetWindowPos(d, nullptr, pr.left + ((pr.right - pr.left) - (dr.right - dr.left)) / 2,
                     pr.top + ((pr.bottom - pr.top) - (dr.bottom - dr.top)) / 2, 0, 0, SWP_NOSIZE | SWP_NOZORDER);
        SetFocus(GetDlgItem(d, 101));
        return FALSE;
    }
    case WM_COMMAND:
        if (LOWORD(w) == IDOK) { std::vector<wchar_t> b(g_dlgMax + 2, 0); GetDlgItemText(d, 101, b.data(), g_dlgMax + 1); g_dlgText = b.data(); EndDialog(d, 1); return TRUE; }
        if (LOWORD(w) == IDCANCEL) { EndDialog(d, 0); return TRUE; }
        break;
    }
    return FALSE;
}
static void dlgAlign(std::vector<BYTE>& b) { while (b.size() % 4) b.push_back(0); }
static void dlgW(std::vector<BYTE>& b, WORD v) { b.push_back((BYTE)(v & 0xFF)); b.push_back((BYTE)(v >> 8)); }
static void dlgD(std::vector<BYTE>& b, DWORD v) { dlgW(b, LOWORD(v)); dlgW(b, HIWORD(v)); }
static void dlgS(std::vector<BYTE>& b, const wchar_t* s) { do { dlgW(b, (WORD)*s); } while (*s++); }
static void dlgItem(std::vector<BYTE>& b, DWORD style, short x, short y, short cx, short cy, WORD id, WORD cls, const wchar_t* text) {
    dlgAlign(b);
    dlgD(b, style | WS_CHILD | WS_VISIBLE);
    dlgD(b, 0);
    dlgW(b, (WORD)x); dlgW(b, (WORD)y); dlgW(b, (WORD)cx); dlgW(b, (WORD)cy); dlgW(b, id);
    dlgW(b, 0xFFFF); dlgW(b, cls);
    dlgS(b, text);
    dlgW(b, 0);
}
static bool promptName(const wchar_t* title, const wchar_t* label, std::wstring& io, int maxLen = 80,
                       const wchar_t* okText = TX(L"Aceptar"), int width = 230, int extraH = 0) {
    g_dlgTitle = title; g_dlgLabel = label; g_dlgText = io; g_dlgMax = maxLen;
    std::vector<BYTE> b;
    dlgD(b, DS_MODALFRAME | DS_SETFONT | WS_POPUP | WS_CAPTION | WS_SYSMENU);
    dlgD(b, 0);
    dlgW(b, 4);                                   // 4 controles
    dlgW(b, 0); dlgW(b, 0); dlgW(b, (WORD)width); dlgW(b, (WORD)(72 + extraH));
    dlgW(b, 0); dlgW(b, 0);                       // sin menu, clase por defecto
    dlgS(b, L"");                                 // titulo (se fija en WM_INITDIALOG)
    dlgW(b, 9); dlgS(b, L"Segoe UI");
    dlgItem(b, SS_LEFT, 12, 10, (short)(width - 24), (short)(10 + extraH), 100, 0x0082, L"");
    dlgItem(b, ES_AUTOHSCROLL | WS_BORDER | WS_TABSTOP, 12, (short)(24 + extraH), (short)(width - 24), 14, 101, 0x0081, L"");
    dlgItem(b, BS_DEFPUSHBUTTON | WS_TABSTOP, (short)(width - 134), (short)(46 + extraH), 64, 14, IDOK, 0x0080, okText);
    dlgItem(b, BS_PUSHBUTTON | WS_TABSTOP, (short)(width - 64), (short)(46 + extraH), 52, 14, IDCANCEL, 0x0080, TX(L"Cancelar"));
    INT_PTR r = DialogBoxIndirectParamW(GetModuleHandle(nullptr), (LPCDLGTEMPLATE)b.data(), g_hwnd, PromptProc, 0);
    if (r != 1) return false;
    io = g_dlgText;
    return true;
}

static void loadPlaylist(const std::wstring& name) {
    std::vector<std::wstring> lines, ok;
    if (!readPlaylistFile(plPath(name), lines)) {
        MessageBoxW(g_hwnd, TX(L"No se pudo leer el archivo de la playlist."), L"MiauPlayer", MB_OK | MB_ICONWARNING);
        refreshSavedLists();
        return;
    }
    int missing = 0;
    for (auto& p0 : lines) {
        const std::wstring p = expandPortablePath(p0);
        if (isUrl(p) || GetFileAttributesW(p.c_str()) != INVALID_FILE_ATTRIBUTES) ok.push_back(p);
        else missing++;
    }
    if (ok.empty()) {
        MessageBoxW(g_hwnd, TX(L"No se encontr\u00f3 ninguno de los archivos de esta playlist (\u00bfse movieron o borraron?)."), L"MiauPlayer", MB_OK | MB_ICONWARNING);
        return;
    }
    stopAudio();
    g_cur = -1;
    g_playlist = ok;
    SendMessage(g_list, LB_RESETCONTENT, 0, 0);
    for (auto& f : g_playlist) SendMessage(g_list, LB_ADDSTRING, 0, (LPARAM)baseName(f).c_str());
    g_loadedPl = name;
    g_listFloat = false;
    InvalidateRect(g_plList, nullptr, TRUE);
    playIndex(0);
    if (missing) g_status = L"(" + std::to_wstring(missing) + TX(L" archivo(s) de la playlist no se encontraron)");
}

// ---- captura del fotograma actual: PNG en Imagenes\MiauPlayer
static std::wstring g_capDir;        // carpeta elegida por el usuario ("" = Imagenes\MiauPlayer)
static std::wstring g_lastCapture;  // ultima captura guardada (para abrir la carpeta con el archivo seleccionado)

static std::wstring defaultCaptureDir() {
    wchar_t pics[MAX_PATH] = L"";
    if (FAILED(SHGetFolderPathW(nullptr, CSIDL_MYPICTURES, nullptr, 0, pics)) || !pics[0]) wcscpy(pics, exeDir().c_str());
    return std::wstring(pics) + L"\\MiauPlayer";
}
static std::wstring captureDir() { return g_capDir.empty() ? defaultCaptureDir() : g_capDir; }

static void loadCaptureDir() {
    HKEY k;
    if (RegOpenKeyExW(HKEY_CURRENT_USER, L"Software\\MiauPlayer", 0, KEY_QUERY_VALUE, &k) == ERROR_SUCCESS) {
        wchar_t b[MAX_PATH * 2] = L"";
        DWORD sz = sizeof(b) - sizeof(wchar_t), ty = 0;
        if (RegQueryValueExW(k, L"CaptureDir", nullptr, &ty, (BYTE*)b, &sz) == ERROR_SUCCESS && ty == REG_SZ) g_capDir = b;
        RegCloseKey(k);
    }
}
static void saveCaptureDir() {
    HKEY k;
    if (RegCreateKeyExW(HKEY_CURRENT_USER, L"Software\\MiauPlayer", 0, nullptr, 0, KEY_SET_VALUE, nullptr, &k, nullptr) != ERROR_SUCCESS) return;
    if (g_capDir.empty()) RegDeleteValueW(k, L"CaptureDir");
    else RegSetValueExW(k, L"CaptureDir", 0, REG_SZ, (const BYTE*)g_capDir.c_str(), (DWORD)((g_capDir.size() + 1) * sizeof(wchar_t)));
    RegCloseKey(k);
}

static int CALLBACK browseCb(HWND h, UINT m, LPARAM, LPARAM data) {
    if (m == BFFM_INITIALIZED && data) SendMessage(h, BFFM_SETSELECTIONW, TRUE, data);
    return 0;
}
static void changeCaptureDir() {
    const std::wstring cur = captureDir();
    wchar_t disp[MAX_PATH] = L"";
    BROWSEINFOW bi = {};
    bi.hwndOwner = g_hwnd;
    bi.pszDisplayName = disp;
    bi.lpszTitle = TX(L"Elige la carpeta donde se guardar\u00e1n las capturas");
    bi.ulFlags = BIF_RETURNONLYFSDIRS | BIF_NEWDIALOGSTYLE;
    bi.lpfn = browseCb;
    bi.lParam = (LPARAM)cur.c_str();
    PIDLIST_ABSOLUTE pidl = SHBrowseForFolderW(&bi);
    if (!pidl) return;
    wchar_t path[MAX_PATH] = L"";
    if (SHGetPathFromIDListW(pidl, path) && path[0]) {
        g_capDir = path;
        saveCaptureDir();
        g_lastCapture.clear();
    }
    CoTaskMemFree(pidl);
}

static void openCaptureFolder() {   // abre la carpeta; si hay una captura reciente en ella, la deja seleccionada
    const std::wstring dir = captureDir();
    SHCreateDirectoryExW(nullptr, dir.c_str(), nullptr);
    if (!g_lastCapture.empty() && GetFileAttributesW(g_lastCapture.c_str()) != INVALID_FILE_ATTRIBUTES &&
        _wcsnicmp(g_lastCapture.c_str(), dir.c_str(), dir.size()) == 0) {
        const std::wstring args = L"/select,\"" + g_lastCapture + L"\"";
        ShellExecuteW(nullptr, L"open", L"explorer.exe", args.c_str(), nullptr, SW_SHOWNORMAL);
    } else {
        ShellExecuteW(nullptr, L"open", dir.c_str(), nullptr, nullptr, SW_SHOWNORMAL);
    }
}

// ---- idioma y asociacion de archivos (menu Configuracion)
static bool regGetDword(HKEY root, const wchar_t* sub, const wchar_t* name, DWORD& v) {
    HKEY k;
    if (RegOpenKeyExW(root, sub, 0, KEY_QUERY_VALUE | KEY_WOW64_64KEY, &k) != ERROR_SUCCESS) return false;
    DWORD sz = sizeof(v), ty = 0;
    const bool ok = RegQueryValueExW(k, name, nullptr, &ty, (BYTE*)&v, &sz) == ERROR_SUCCESS && ty == REG_DWORD;
    RegCloseKey(k);
    return ok;
}
static void regSetStr(HKEY root, const std::wstring& sub, const wchar_t* name, const std::wstring& val) {
    HKEY k;
    if (RegCreateKeyExW(root, sub.c_str(), 0, nullptr, 0, KEY_SET_VALUE | KEY_WOW64_64KEY, nullptr, &k, nullptr) != ERROR_SUCCESS) return;
    RegSetValueExW(k, name, 0, REG_SZ, (const BYTE*)val.c_str(), (DWORD)((val.size() + 1) * sizeof(wchar_t)));
    RegCloseKey(k);
}
static void regSetDword(HKEY root, const wchar_t* sub, const wchar_t* name, DWORD v) {
    HKEY k;
    if (RegCreateKeyExW(root, sub, 0, nullptr, 0, KEY_SET_VALUE | KEY_WOW64_64KEY, nullptr, &k, nullptr) != ERROR_SUCCESS) return;
    RegSetValueExW(k, name, 0, REG_DWORD, (const BYTE*)&v, sizeof(v));
    RegCloseKey(k);
}

static int g_langPending = -1;   // idioma elegido en el menu (se aplica al reiniciar); -1 = sin cambios

// guarda el idioma para el usuario actual; initLanguage() lo lee al abrir el programa (HKCU antes que HKLM)
static void setLanguagePref(int lng) {
    regSetStr(HKEY_CURRENT_USER, L"Software\\MiauPlayer", L"Language", lng == LNG_EN ? L"en" : lng == LNG_PT ? L"pt" : L"es");
    g_langPending = lng;
    const int old = g_lang;
    g_lang = lng;   // el aviso sale ya en el idioma elegido
    const std::wstring msg = TX(L"El idioma cambiar\u00e1 la pr\u00f3xima vez que abras MiauPlayer.");
    g_lang = old;
    MessageBoxW(g_hwnd, msg.c_str(), L"MiauPlayer", MB_OK | MB_ICONINFORMATION);
}

// Las 10 extensiones que ofrece el instalador (mp3, mp4, ogg, wma, wmv, aac, m4a, mpg, mov, mkv)
static const struct { const wchar_t* ext; bool video; } kAssocExts[] = {
    { L"mp3", false }, { L"mp4", true }, { L"ogg", false }, { L"wma", false }, { L"wmv", true },
    { L"aac", false }, { L"m4a", false }, { L"mpg", true }, { L"mov", true }, { L"mkv", true } };

// Registra MiauPlayer para el usuario actual (no necesita permisos de administrador) y abre la pantalla de Windows donde
// el usuario confirma la eleccion: desde Windows 10 un programa no puede ponerse como predeterminado por su cuenta.
static void associateFileTypes() {
    if (isPackaged()) {   // en el paquete las asociaciones ya estan declaradas: solo se abre la pagina de apps predeterminadas
        ShellExecuteW(nullptr, L"open", L"ms-settings:defaultapps", nullptr, nullptr, SW_SHOWNORMAL);
        return;
    }
    wchar_t ex[MAX_PATH * 2];
    const DWORD en = GetModuleFileNameW(nullptr, ex, MAX_PATH * 2);
    const std::wstring exe(ex, en);
    const std::wstring cmd = L"\"" + exe + L"\" \"%1\"", icon = L"\"" + exe + L"\",-2";
    const HKEY u = HKEY_CURRENT_USER;
    const wchar_t* ids[2] = { L"MiauPlayer.Audio", L"MiauPlayer.Video" };
    const wchar_t* desc[2] = { TX(L"Archivo de audio (MiauPlayer)"), TX(L"Archivo de video (MiauPlayer)") };
    for (int i = 0; i < 2; i++) {
        const std::wstring base = std::wstring(L"Software\\Classes\\") + ids[i];
        regSetStr(u, base, nullptr, desc[i]);
        regSetStr(u, base + L"\\DefaultIcon", nullptr, icon);
        regSetStr(u, base + L"\\shell\\open\\command", nullptr, cmd);
    }
    regSetStr(u, L"Software\\MiauPlayer\\Capabilities", L"ApplicationName", L"MiauPlayer");
    regSetStr(u, L"Software\\MiauPlayer\\Capabilities", L"ApplicationDescription", TX(L"Reproductor de audio y video minimalista"));
    for (const auto& a : kAssocExts) {
        const std::wstring dot = std::wstring(L".") + a.ext;
        regSetStr(u, L"Software\\MiauPlayer\\Capabilities\\FileAssociations", dot.c_str(), ids[a.video]);
        regSetStr(u, L"Software\\Classes\\" + dot + L"\\OpenWithProgids", ids[a.video], L"");
    }
    regSetStr(u, L"Software\\RegisteredApplications", L"MiauPlayer", L"Software\\MiauPlayer\\Capabilities");
    SHChangeNotify(SHCNE_ASSOCCHANGED, SHCNF_IDLIST, nullptr, nullptr);
    ShellExecuteW(nullptr, L"open", L"ms-settings:defaultapps?registeredAppUser=MiauPlayer", nullptr, nullptr, SW_SHOWNORMAL);
}

// Primer arranque: ofrece usar MiauPlayer para esas 10 extensiones. No pregunta si el instalador ya lo hizo
// (marca AssocAsked en HKLM) ni si ya se pregunt\u00f3 antes (marca en HKCU).
static void askAssociationFirstRun() {
    if (isPackaged()) return;   // Windows ya ofrece a MiauPlayer para esos tipos (manifiesto del paquete)
    DWORD v = 0;
    if (regGetDword(HKEY_CURRENT_USER, L"Software\\MiauPlayer", L"AssocAsked", v) ||
        regGetDword(HKEY_LOCAL_MACHINE, L"Software\\MiauPlayer", L"AssocAsked", v)) return;
    regSetDword(HKEY_CURRENT_USER, L"Software\\MiauPlayer", L"AssocAsked", 1);   // se pregunta una sola vez
    if (MessageBoxW(g_hwnd,
                    TX(L"\u00bfQuieres abrir tus archivos de audio y video con MiauPlayer?\n\n"
                       L"Tipos de archivo: .mp3  .mp4  .ogg  .wma  .wmv  .aac  .m4a  .mpg  .mov  .mkv\n\n"
                       L"Windows te pedir\u00e1 confirmar la elecci\u00f3n. Podr\u00e1s cambiarlo despu\u00e9s en Configuraci\u00f3n (engranaje) > Asociar tipos de archivo."),
                    L"MiauPlayer", MB_YESNO | MB_ICONQUESTION) == IDYES)
        associateFileTypes();
}

static void showCaptureOptions() {
    HMENU m = CreatePopupMenu();
    std::wstring cur = captureDir();
    if (cur.size() > 56) cur = L"\u2026" + cur.substr(cur.size() - 55);
    AppendMenu(m, MF_STRING | MF_GRAYED, 1, (std::wstring(TX(L"Carpeta actual: ")) + cur).c_str());
    AppendMenu(m, MF_SEPARATOR, 0, nullptr);
    AppendMenu(m, MF_STRING, 2, TX(L"Cambiar carpeta de capturas\u2026"));
    if (!g_capDir.empty()) AppendMenu(m, MF_STRING, 3, TX(L"Usar la carpeta predeterminada"));
    AppendMenu(m, MF_SEPARATOR, 0, nullptr);
    {
        const int sel = g_langPending >= 0 ? g_langPending : g_lang;
        HMENU lm = CreatePopupMenu();
        AppendMenu(lm, MF_STRING | (sel == LNG_ES ? MF_CHECKED : 0), 10, L"Espa\u00f1ol");
        AppendMenu(lm, MF_STRING | (sel == LNG_EN ? MF_CHECKED : 0), 11, L"English");
        AppendMenu(lm, MF_STRING | (sel == LNG_PT ? MF_CHECKED : 0), 12, L"Portugu\u00eas");
        AppendMenu(m, MF_POPUP, (UINT_PTR)lm, TX(L"Idioma"));   // DestroyMenu(m) tambien libera el submenu
    }
    AppendMenu(m, MF_STRING, 4, TX(L"Asociar tipos de archivo\u2026"));
    RECT br;
    GetWindowRect(GetDlgItem(g_hwnd, ID_SNAPOPT), &br);
    SetForegroundWindow(g_hwnd);
    const int cmd = TrackPopupMenu(m, TPM_RETURNCMD | TPM_LEFTALIGN | TPM_TOPALIGN, br.left, br.bottom + 2, 0, g_hwnd, nullptr);
    DestroyMenu(m);
    { POINT cp; GetCursorPos(&cp); if (PtInRect(&br, cp)) g_optMenuClosedOnBtn = GetTickCount(); }   // segundo clic sobre el boton = solo cerrar
    if (cmd == 2) changeCaptureDir();
    else if (cmd == 3) { g_capDir.clear(); saveCaptureDir(); g_lastCapture.clear(); }
    else if (cmd == 4) associateFileTypes();
    else if (cmd >= 10 && cmd <= 12) setLanguagePref(cmd - 10);   // 10 = es, 11 = en, 12 = pt (ver LNG_*)
}

static bool pngEncoderClsid(CLSID* out) {
    UINT num = 0, size = 0;
    Gdiplus::GetImageEncodersSize(&num, &size);
    if (!size) return false;
    std::vector<BYTE> buf(size);
    Gdiplus::ImageCodecInfo* enc = (Gdiplus::ImageCodecInfo*)buf.data();
    Gdiplus::GetImageEncoders(num, size, enc);
    for (UINT i = 0; i < num; i++)
        if (wcscmp(enc[i].MimeType, L"image/png") == 0) { *out = enc[i].Clsid; return true; }
    return false;
}

static void captureFrame() {
    auto note = [](const std::wstring& t, const std::wstring& toast) {
        g_status = t;
        SetTimer(g_hwnd, 3, 5000, nullptr);   // el aviso desaparece solo
        g_toast = toast;                      // y un aviso corto sobre el video (se ve en pantalla completa)
        g_toastUntil = GetTickCount() + 2600;
        InvalidateRect(g_hwnd, nullptr, FALSE);
    };
    AVFrame* f = nullptr;
    double dar = 1.0;
    if (g_hasVideo) {
        Lock l(&g_vidCS);
        if (g_lastFrame && g_lastFrame->data[0] && g_lastFrame->width > 0) { f = av_frame_clone(g_lastFrame); dar = g_lastDar; }
    }
    if (!f) { note(TX(L"No hay video para capturar"), TX(L"No hay video para capturar")); return; }
    const int h = f->height, w = std::max(2, (int)(h * dar + 0.5));   // alto original, ancho segun el aspecto real
    SwsContext* sws = sws_getContext(f->width, f->height, (AVPixelFormat)f->format, w, h, AV_PIX_FMT_BGRA, SWS_BICUBIC, nullptr, nullptr, nullptr);
    std::vector<uint8_t> px((size_t)w * h * 4);
    bool ok = false;
    if (sws) {
        uint8_t* dp[1] = { px.data() };
        int ds[1] = { w * 4 };
        sws_scale(sws, f->data, f->linesize, 0, f->height, dp, ds);
        sws_freeContext(sws);
        const std::wstring dir = captureDir();
        SHCreateDirectoryExW(nullptr, dir.c_str(), nullptr);
        std::wstring base = L"MiauPlayer";
        if (g_cur >= 0 && g_cur < (int)g_playlist.size()) {
            base = isUrl(g_playlist[g_cur]) ? L"stream" : baseName(g_playlist[g_cur]);
            const size_t dot = base.find_last_of(L'.');
            if (dot != std::wstring::npos && dot > 0 && base.size() - dot <= 5) base.resize(dot);
        }
        for (auto& c : base) if (c < 32 || wcschr(L"<>:\"/\\|?*", c)) c = L'_';
        if (base.size() > 50) base.resize(50);
        SYSTEMTIME st;
        GetLocalTime(&st);
        const int sec = (int)g_posSec.load();
        wchar_t nm[160];
        swprintf(nm, 160, L"%ls_%02d-%02d-%02d_%04d%02d%02d-%02d%02d%02d.png", base.c_str(), sec / 3600, (sec / 60) % 60, sec % 60,
                 st.wYear, st.wMonth, st.wDay, st.wHour, st.wMinute, st.wSecond);
        const std::wstring path = dir + L"\\" + nm;
        CLSID png;
        if (pngEncoderClsid(&png)) {
            Gdiplus::Bitmap bmp(w, h, w * 4, PixelFormat32bppRGB, px.data());
            ok = bmp.Save(path.c_str(), &png, nullptr) == Gdiplus::Ok;
        }
        if (ok) { g_lastCapture = path; note(std::wstring(TX(L"Fotograma guardado: ")) + nm, TX(L"Fotograma guardado")); }
    }
    av_frame_free(&f);
    if (!ok) note(TX(L"No se pudo guardar el fotograma"), TX(L"No se pudo guardar el fotograma"));
}

static void doOpenNetUrl() {
    std::wstring url = L"rtsp://";
    if (!promptName(TX(L"Abrir ruta de red"),
                    (std::wstring(TX(L"Direcci\u00f3n de la transmisi\u00f3n (rtsp://, rtsps://, http://, https://, rtmp://, udp://\u2026):\r\n")) + TX(L"Ejemplo: rtsp://usuario:clave@192.168.1.10/stream")).c_str(),
                    url, 1000, TX(L"Reproducir"), 340, 12)) return;
    while (!url.empty() && iswspace(url.back())) url.pop_back();
    size_t a = 0;
    while (a < url.size() && iswspace(url[a])) a++;
    url = url.substr(a);
    if (!isUrl(url)) {
        MessageBoxW(g_hwnd, TX(L"La direcci\u00f3n debe empezar con el protocolo, por ejemplo rtsp://, http://, https:// o rtmp://"),
                    L"MiauPlayer", MB_OK | MB_ICONWARNING);
        return;
    }
    g_listFloat = false;
    const int idx = (int)g_playlist.size();
    g_playlist.push_back(url);
    SendMessage(g_list, LB_ADDSTRING, 0, (LPARAM)baseName(url).c_str());
    playIndex(idx);
}

static void doSavePlaylist() {
    if (g_playlist.empty()) {
        MessageBoxW(g_hwnd, TX(L"La lista actual est\u00e1 vac\u00eda. Agrega canciones y vuelve a guardar."), L"MiauPlayer", MB_OK | MB_ICONINFORMATION);
        return;
    }
    std::wstring name = g_loadedPl;
    if (!promptName(TX(L"Guardar playlist"), TX(L"Nombre de la playlist:"), name)) return;
    name = sanitizeName(name);
    if (name.empty()) {
        MessageBoxW(g_hwnd, TX(L"El nombre no es v\u00e1lido."), L"MiauPlayer", MB_OK | MB_ICONWARNING);
        return;
    }
    if (GetFileAttributesW(plPath(name).c_str()) != INVALID_FILE_ATTRIBUTES &&
        MessageBoxW(g_hwnd, TX(L"Ya existe una playlist con ese nombre. \u00bfReemplazarla?"), L"MiauPlayer", MB_YESNO | MB_ICONQUESTION) != IDYES)
        return;
    if (!writePlaylistFile(name)) {
        MessageBoxW(g_hwnd, (TX(L"No se pudo guardar el archivo en:\n") + plDir()).c_str(), L"MiauPlayer", MB_OK | MB_ICONERROR);
        return;
    }
    g_loadedPl = name;
    refreshSavedLists();
    for (size_t i = 0; i < g_savedNames.size(); i++)
        if (_wcsicmp(g_savedNames[i].c_str(), name.c_str()) == 0) { SendMessage(g_plList, LB_SETCURSEL, i, 0); break; }
}

static void doImportPlaylist() {
    wchar_t buf[MAX_PATH] = L"";
    OPENFILENAMEW o = {};
    o.lStructSize = sizeof(o);
    o.hwndOwner = g_hwnd;
    const std::wstring flt_ = makeFilter({ { TX(L"Playlists de texto (*.txt)"), L"*.txt" }, { TX(L"Todos los archivos"), L"*.*" } });
    o.lpstrFilter = flt_.c_str();
    o.lpstrFile = buf;
    o.nMaxFile = MAX_PATH;
    o.lpstrTitle = TX(L"Importar playlist (.txt)");
    o.Flags = OFN_FILEMUSTEXIST | OFN_PATHMUSTEXIST | OFN_HIDEREADONLY;
    if (!GetOpenFileNameW(&o)) return;
    std::vector<std::wstring> lines;
    if (!readPlaylistFile(buf, lines) || lines.empty()) {
        MessageBoxW(g_hwnd, TX(L"El archivo no contiene rutas de canciones (una por l\u00ednea)."), L"MiauPlayer", MB_OK | MB_ICONWARNING);
        return;
    }
    std::wstring name = baseName(buf);
    if (name.size() > 4 && _wcsicmp(name.c_str() + name.size() - 4, L".txt") == 0) name.resize(name.size() - 4);
    name = sanitizeName(name);
    if (name.empty()) name = TX(L"Importada");
    if (GetFileAttributesW(plPath(name).c_str()) != INVALID_FILE_ATTRIBUTES &&
        MessageBoxW(g_hwnd, (TX(L"Ya existe una playlist llamada \u201c") + name + TX(L"\u201d. \u00bfReemplazarla?")).c_str(), L"MiauPlayer", MB_YESNO | MB_ICONQUESTION) != IDYES)
        return;
    if (!writeLinesFile(plPath(name), lines)) {
        MessageBoxW(g_hwnd, (TX(L"No se pudo guardar el archivo en:\n") + plDir()).c_str(), L"MiauPlayer", MB_OK | MB_ICONERROR);
        return;
    }
    refreshSavedLists();
    for (size_t i = 0; i < g_savedNames.size(); i++)
        if (_wcsicmp(g_savedNames[i].c_str(), name.c_str()) == 0) { SendMessage(g_plList, LB_SETCURSEL, i, 0); break; }
    loadPlaylist(name);   // queda en la columna derecha, se carga a la izquierda y empieza a sonar
}

static void doExportCurrent() {   // exporta la lista actual (columna izquierda) a un .txt
    if (g_playlist.empty()) {
        MessageBoxW(g_hwnd, TX(L"La lista actual est\u00e1 vac\u00eda, no hay nada que exportar."), L"MiauPlayer", MB_OK | MB_ICONINFORMATION);
        return;
    }
    wchar_t buf[MAX_PATH] = L"";
    wcsncpy(buf, ((g_loadedPl.empty() ? std::wstring(L"playlist") : g_loadedPl) + L".txt").c_str(), MAX_PATH - 1);
    OPENFILENAMEW o = {};
    o.lStructSize = sizeof(o);
    o.hwndOwner = g_hwnd;
    const std::wstring flt_ = makeFilter({ { TX(L"Texto (*.txt)"), L"*.txt" }, { TX(L"Todos los archivos"), L"*.*" } });
    o.lpstrFilter = flt_.c_str();
    o.lpstrFile = buf;
    o.nMaxFile = MAX_PATH;
    o.lpstrDefExt = L"txt";
    o.lpstrTitle = TX(L"Exportar la lista actual a .txt");
    o.Flags = OFN_OVERWRITEPROMPT | OFN_PATHMUSTEXIST;
    if (GetSaveFileNameW(&o) && !writeLinesFile(buf, portableLines(g_playlist)))
        MessageBoxW(g_hwnd, TX(L"No se pudo exportar el archivo."), L"MiauPlayer", MB_OK | MB_ICONERROR);
}

static void exportPlaylist(const std::wstring& name) {
    wchar_t buf[MAX_PATH] = L"";
    wcsncpy(buf, (name + L".txt").c_str(), MAX_PATH - 1);
    OPENFILENAMEW o = {};
    o.lStructSize = sizeof(o);
    o.hwndOwner = g_hwnd;
    const std::wstring flt_ = makeFilter({ { TX(L"Texto (*.txt)"), L"*.txt" }, { TX(L"Todos los archivos"), L"*.*" } });
    o.lpstrFilter = flt_.c_str();
    o.lpstrFile = buf;
    o.nMaxFile = MAX_PATH;
    o.lpstrDefExt = L"txt";
    o.lpstrTitle = TX(L"Exportar playlist");
    o.Flags = OFN_OVERWRITEPROMPT | OFN_PATHMUSTEXIST;
    if (GetSaveFileNameW(&o) && !CopyFileW(plPath(name).c_str(), buf, FALSE))
        MessageBoxW(g_hwnd, TX(L"No se pudo exportar el archivo."), L"MiauPlayer", MB_OK | MB_ICONERROR);
}

static void deletePlaylist(const std::wstring& name) {
    if (MessageBoxW(g_hwnd, (TX(L"\u00bfEliminar la playlist \u201c") + name + TX(L"\u201d?\nSe borrar\u00e1 su archivo .txt.")).c_str(), L"MiauPlayer",
                    MB_YESNO | MB_ICONQUESTION) != IDYES) return;
    DeleteFileW(plPath(name).c_str());
    if (g_loadedPl == name) g_loadedPl.clear();
    refreshSavedLists();
}

static void showPlMenu(int idx, POINT pt) {
    if (idx < 0 || idx >= (int)g_savedNames.size()) return;
    const std::wstring name = g_savedNames[idx];
    HMENU m = CreatePopupMenu();
    AppendMenu(m, MF_STRING, 1, TX(L"Reproducir"));
    AppendMenu(m, MF_STRING, 2, TX(L"Exportar a .txt\u2026"));
    AppendMenu(m, MF_SEPARATOR, 0, nullptr);
    AppendMenu(m, MF_STRING, 3, TX(L"Eliminar"));
    SetForegroundWindow(g_hwnd);
    const int cmd = TrackPopupMenu(m, TPM_RETURNCMD | TPM_RIGHTBUTTON, pt.x, pt.y, 0, g_hwnd, nullptr);
    DestroyMenu(m);
    if (cmd == 1) loadPlaylist(name);
    else if (cmd == 2) exportPlaylist(name);
    else if (cmd == 3) deletePlaylist(name);
}

static void drawDropLine(const DRAWITEMSTRUCT* di);   // definida mas abajo, junto al arrastre de elementos
static void drawPlItem(const DRAWITEMSTRUCT* di) {
    if ((int)di->itemID < 0) return;
    HDC dc = di->hDC;
    RECT r = di->rcItem;
    fillRect(dc, r, C_PANEL);
    const bool sel = (di->itemState & ODS_SELECTED) != 0;
    wchar_t txt[256] = L"";
    SendMessage(di->hwndItem, LB_GETTEXT, di->itemID, (LPARAM)txt);
    const bool loaded = !g_loadedPl.empty() && _wcsicmp(g_loadedPl.c_str(), txt) == 0;
    {
        Graphics g(dc);
        g.SetSmoothingMode(Gdiplus::SmoothingModeAntiAlias);
        if (loaded || sel) {
            GraphicsPath p;
            rrPath(p, r.left + 6.5f, r.top + 2.5f, (float)(r.right - r.left) - 13.f, (float)(r.bottom - r.top) - 5.f, 8.f);
            if (loaded) {
                Gdiplus::LinearGradientBrush lg(PointF((float)r.left, (float)r.top), PointF((float)r.left, (float)r.bottom),
                                                Col(RGB(240, 218, 218)), Col(RGB(222, 184, 184)));
                g.FillPath(&lg, &p);
                Pen bp(Col(RGB(206, 164, 164)), 1.f);
                g.DrawPath(&bp, &p);
            } else {
                SolidBrush sb(Col(RGB(230, 233, 237)));
                g.FillPath(&sb, &p);
            }
        }
        const float cy = (r.top + r.bottom) / 2.f;
        drawIcon(g, loaded ? ICO_PLAY : ICO_LIST, r.left + 18.f, cy - 7.f, 14.f, RGB(58, 63, 71));
        g.Flush();
    }
    RECT tr = r;
    tr.left += 42;
    tr.right -= 12;
    SelectObject(dc, loaded ? g_fontBold : g_font);
    SetBkMode(dc, TRANSPARENT);
    SetTextColor(dc, RGB(43, 47, 54));
    DrawText(dc, txt, -1, &tr, DT_LEFT | DT_VCENTER | DT_SINGLELINE | DT_END_ELLIPSIS | DT_NOPREFIX);
    drawDropLine(di);
}

// ---- editar la lista actual (clic derecho sobre las canciones)
static void clearWaveData() {
    ++g_waveGen;   // invalida el analisis en curso
    Lock l(&g_waveCS);
    g_peaks.clear();
    g_waveDone = false;
}

static void removeTrack(int idx) {
    if (idx < 0 || idx >= (int)g_playlist.size()) return;
    const bool wasCur = idx == g_cur;
    const bool wasPlaying = wasCur && g_state == ST_PLAYING;
    if (wasCur) { stopAudio(); clearWaveData(); g_duration = 0; g_status.clear(); }
    g_playlist.erase(g_playlist.begin() + idx);
    SendMessage(g_list, LB_DELETESTRING, idx, 0);
    if (wasCur) g_cur = -1;
    else if (idx < g_cur) g_cur--;
    const int n = (int)g_playlist.size();
    if (n > 0) SendMessage(g_list, LB_SETCURSEL, std::min(idx, n - 1), 0);
    if (wasPlaying && idx < n) { playIndex(idx); return; }   // si sonaba, sigue con la siguiente
    updateTitle();
    relayout();
    InvalidateRect(g_hwnd, nullptr, FALSE);
}

static void clearPlaylist() {
    if (g_playlist.empty()) return;
    stopAudio();
    clearWaveData();
    g_duration = 0;
    g_posSec = 0;
    g_status.clear();
    g_playlist.clear();
    SendMessage(g_list, LB_RESETCONTENT, 0, 0);
    g_cur = -1;
    g_loadedPl.clear();
    if (g_plList) InvalidateRect(g_plList, nullptr, TRUE);
    updateTitle();
    relayout();
    InvalidateRect(g_hwnd, nullptr, FALSE);
}

// ---- arrastrar con el mouse para cambiar el orden (lista actual y listas guardadas)
struct DragState { int from = -1, gap = -1; bool pressed = false, active = false; POINT start = { 0, 0 }; };
static DragState g_dr;
static HWND g_drList = nullptr;
enum { DRG_NONE = 0, DRG_BUSY, DRG_DONE, DRG_CLICK };   // resultado de dragHandle: no es suyo / lo atendio / soltado tras arrastrar / clic sin arrastrar

// posicion de insercion (0..n) segun la altura y del cursor dentro de la lista
static int dragGapAt(HWND h, int y) {
    const int n = (int)SendMessage(h, LB_GETCOUNT, 0, 0);
    const int ih = std::max(1, (int)SendMessage(h, LB_GETITEMHEIGHT, 0, 0));
    const int top = (int)SendMessage(h, LB_GETTOPINDEX, 0, 0);
    const int i = y >= 0 ? y / ih : -((-y + ih - 1) / ih);
    const int frac = y - i * ih;
    return std::max(0, std::min(n, top + i + (frac * 2 >= ih ? 1 : 0)));
}

static void dragReset(HWND h) {
    g_dr = DragState();
    g_drList = nullptr;
    KillTimer(h, 77);
    InvalidateRect(h, nullptr, FALSE);
}

// apply(desde, hasta): mueve el elemento "desde" para que quede en la posicion "hasta"
static int dragHandle(HWND h, UINT m, WPARAM w, LPARAM l, void (*apply)(int, int)) {
    // Esta funcion se llama desde el subclass de la lista con TODOS sus mensajes. Los LB_* que enviamos aqui vuelven a
    // pasar por el subclass, asi que solo se atienden los mensajes del arrastre; con cualquier otro se sale sin enviar
    // nada (si no, LB_GETCOUNT se llamaria a si mismo sin fin y la pila se desborda).
    if (m != WM_LBUTTONDOWN && m != WM_MOUSEMOVE && m != WM_TIMER && m != WM_LBUTTONUP && m != WM_KEYDOWN && m != WM_CAPTURECHANGED)
        return DRG_NONE;
    const int n = (int)SendMessage(h, LB_GETCOUNT, 0, 0);
    switch (m) {
    case WM_LBUTTONDOWN: {
        const DWORD it = (DWORD)SendMessage(h, LB_ITEMFROMPOINT, 0, l);
        const int idx = LOWORD(it);
        if (HIWORD(it) != 0 || idx < 0 || idx >= n) return DRG_NONE;   // clic fuera de los elementos
        SetFocus(h);
        SetCapture(h);
        SendMessage(h, LB_SETCURSEL, idx, 0);
        g_dr = DragState();
        g_dr.from = idx;
        g_dr.pressed = true;
        g_dr.start = { GET_X_LPARAM(l), GET_Y_LPARAM(l) };
        g_drList = h;
        return DRG_BUSY;
    }
    case WM_MOUSEMOVE: {
        if (g_drList != h || !g_dr.pressed) return DRG_NONE;
        const int x = GET_X_LPARAM(l), y = GET_Y_LPARAM(l);
        if (!g_dr.active) {
            if (abs(x - g_dr.start.x) < 6 && abs(y - g_dr.start.y) < 6) return DRG_BUSY;   // todavia es un clic
            g_dr.active = true;
            SetTimer(h, 77, 60, nullptr);   // desplaza la lista si el cursor queda en un borde
        }
        const int gap = dragGapAt(h, y);
        if (gap != g_dr.gap) { g_dr.gap = gap; InvalidateRect(h, nullptr, FALSE); }
        return DRG_BUSY;
    }
    case WM_TIMER: {
        if (w != 77 || g_drList != h || !g_dr.active) return DRG_NONE;
        POINT p;
        GetCursorPos(&p);
        ScreenToClient(h, &p);
        RECT rc;
        GetClientRect(h, &rc);
        const int top = (int)SendMessage(h, LB_GETTOPINDEX, 0, 0);
        if (p.y < 14 && top > 0) SendMessage(h, LB_SETTOPINDEX, top - 1, 0);
        else if (p.y > rc.bottom - 14) SendMessage(h, LB_SETTOPINDEX, top + 1, 0);
        g_dr.gap = dragGapAt(h, p.y);
        InvalidateRect(h, nullptr, FALSE);
        return DRG_BUSY;
    }
    case WM_LBUTTONUP: {
        if (g_drList != h || !g_dr.pressed) return DRG_NONE;
        const bool was = g_dr.active;
        const int from = g_dr.from, gap = g_dr.gap;
        dragReset(h);
        ReleaseCapture();
        if (!was) return DRG_CLICK;
        const int to = gap > from ? gap - 1 : gap;
        if (gap >= 0 && from >= 0 && from < n && to >= 0 && to < n && to != from) apply(from, to);
        return DRG_DONE;
    }
    case WM_KEYDOWN:
        if (w == VK_ESCAPE && g_drList == h && g_dr.pressed) { dragReset(h); ReleaseCapture(); return DRG_BUSY; }   // Esc cancela
        return DRG_NONE;
    case WM_CAPTURECHANGED:
        if (g_drList == h && g_dr.pressed && (HWND)l != h) dragReset(h);
        return DRG_NONE;
    }
    return DRG_NONE;
}

// linea roja que marca donde caera el elemento arrastrado
static void drawDropLine(const DRAWITEMSTRUCT* di) {
    if (!g_dr.active || g_drList != di->hwndItem) return;
    const int n = (int)SendMessage(di->hwndItem, LB_GETCOUNT, 0, 0);
    const int id = (int)di->itemID;
    int y = -1;
    if (g_dr.gap == id) y = di->rcItem.top;
    else if (g_dr.gap == n && id == n - 1) y = di->rcItem.bottom - 2;
    if (y < 0) return;
    RECT r = { di->rcItem.left + 8, y, di->rcItem.right - 8, y + 2 };
    fillRect(di->hDC, r, C_WAVE_PLAYED);
}

static void movePlaylistItem(int from, int to) {
    const int n = (int)g_playlist.size();
    if (from < 0 || from >= n || to < 0 || to >= n || from == to) return;
    const std::wstring s = g_playlist[from];
    g_playlist.erase(g_playlist.begin() + from);
    g_playlist.insert(g_playlist.begin() + to, s);
    if (g_cur == from) g_cur = to;                                   // la que suena sigue siendo la misma
    else if (from < g_cur && to >= g_cur) g_cur--;
    else if (from > g_cur && to <= g_cur) g_cur++;
    const int top = (int)SendMessage(g_list, LB_GETTOPINDEX, 0, 0);
    SendMessage(g_list, WM_SETREDRAW, FALSE, 0);
    SendMessage(g_list, LB_RESETCONTENT, 0, 0);
    for (auto& f : g_playlist) SendMessage(g_list, LB_ADDSTRING, 0, (LPARAM)baseName(f).c_str());
    SendMessage(g_list, LB_SETTOPINDEX, top, 0);
    SendMessage(g_list, LB_SETCURSEL, to, 0);
    SendMessage(g_list, WM_SETREDRAW, TRUE, 0);
    InvalidateRect(g_list, nullptr, TRUE);
}

static void moveSavedItem(int from, int to) {
    const int n = (int)g_savedNames.size();
    if (from < 0 || from >= n || to < 0 || to >= n || from == to) return;
    const std::wstring s = g_savedNames[from];
    g_savedNames.erase(g_savedNames.begin() + from);
    g_savedNames.insert(g_savedNames.begin() + to, s);
    saveSavedOrder();
    const int top = (int)SendMessage(g_plList, LB_GETTOPINDEX, 0, 0);
    SendMessage(g_plList, WM_SETREDRAW, FALSE, 0);
    fillSavedListBox();
    SendMessage(g_plList, LB_SETTOPINDEX, top, 0);
    SendMessage(g_plList, LB_SETCURSEL, to, 0);
    SendMessage(g_plList, WM_SETREDRAW, TRUE, 0);
    InvalidateRect(g_plList, nullptr, TRUE);
}

static void drawListItem(const DRAWITEMSTRUCT* di) {
    if ((int)di->itemID < 0) return;
    HDC dc = di->hDC;
    RECT r = di->rcItem;
    fillRect(dc, r, C_PANEL);
    const bool cur = (int)di->itemID == g_cur;
    const bool sel = (di->itemState & ODS_SELECTED) != 0;
    wchar_t txt[512] = L"";
    SendMessage(di->hwndItem, LB_GETTEXT, di->itemID, (LPARAM)txt);
    {
        Graphics g(dc);
        g.SetSmoothingMode(Gdiplus::SmoothingModeAntiAlias);
        if (cur || sel) {
            GraphicsPath p;
            rrPath(p, r.left + 6.5f, r.top + 2.5f, (float)(r.right - r.left) - 13.f, (float)(r.bottom - r.top) - 5.f, 8.f);
            if (cur) {
                Gdiplus::LinearGradientBrush lg(PointF((float)r.left, (float)r.top), PointF((float)r.left, (float)r.bottom),
                                                Col(RGB(240, 218, 218)), Col(RGB(222, 184, 184)));
                g.FillPath(&lg, &p);
                Pen bp(Col(RGB(206, 164, 164)), 1.f);
                g.DrawPath(&bp, &p);
            } else {
                SolidBrush sb(Col(RGB(230, 233, 237)));
                g.FillPath(&sb, &p);
            }
        }
        if (cur) drawIcon(g, ICO_PLAY, r.left + 20.f, (r.top + r.bottom) / 2.f - 7.f, 14.f, RGB(58, 63, 71));
        g.Flush();
    }
    RECT tr = r;
    tr.left += cur ? 46 : 20;
    tr.right -= 10;
    SelectObject(dc, cur ? g_fontBold : g_font);
    SetBkMode(dc, TRANSPARENT);
    SetTextColor(dc, RGB(43, 47, 54));
    DrawText(dc, txt, -1, &tr, DT_LEFT | DT_VCENTER | DT_SINGLELINE | DT_END_ELLIPSIS | DT_NOPREFIX);
    drawDropLine(di);
}

// ------------------------------------------------------------ textos, etiquetas, volumen
static void updateLabels() {
    float g = g_gainDb;
    wchar_t b[128];
    swprintf(b, 128, TX(L"Ganancia: %+.1f dB  (%.0f%%)"), g, powf(10.f, g / 20.f) * 100.f);
    SetWindowText(g_gainTxt, b);
    static const wchar_t* names[3] = { TX(L"Graves"), TX(L"Medios"), TX(L"Agudos") };
    for (int i = 0; i < 3; i++) {
        swprintf(b, 128, L"%ls %+.0f dB", names[i], (float)g_eqDb[i]);
        SetWindowText(g_eqTxt[i], b);
    }
    swprintf(b, 128, TX(L"Volumen: %.0f%%%ls"), g_vol01.load() * 100.f, g_muted.load() ? TX(L"  (silencio)") : L"");
    SetWindowText(g_volTxt, b);
    SetWindowText(g_mute, g_muted.load() ? TX(L"Activar") : TX(L"Silencio"));
}

static void setVolume(float v) {
    v = std::min(1.f, std::max(0.f, v));
    g_vol01 = v;
    if (v > 0) g_muted = false;
    SendMessage(g_vol, TBM_SETPOS, TRUE, (LPARAM)lrintf(v * 100.f));
    InvalidateRect(g_vol, nullptr, FALSE);
    updateLabels();
}

// ------------------------------------------------------------ distribucion
// Reescala el ultimo cuadro al tamano actual del area de video. El hilo de video solo entrega cuadros nuevos mientras
// reproduce; en pausa (o tras cambiar a pantalla completa / cambiar el tamano de la ventana) el cuadro viejo se quedaba
// del tamano anterior hasta el siguiente cuadro.
static void rescaleLastFrame() {
    const int aw = g_vidAreaW.load(), ah = g_vidAreaH.load();
    if (aw < 16 || ah < 16) return;
    AVFrame* f = nullptr;
    double dar = 1.0;
    {
        Lock l(&g_vidCS);
        if (g_lastFrame && g_lastFrame->data[0] && g_lastFrame->width > 0) { f = av_frame_clone(g_lastFrame); dar = g_lastDar; }
    }
    if (!f) return;
    int dw = aw, dh = (int)(aw / dar + 0.5);
    if (dh > ah) { dh = ah; dw = (int)(ah * dar + 0.5); }
    dw = std::max(2, dw & ~1); dh = std::max(2, dh & ~1);
    {
        Lock l(&g_vidCS);
        if (g_vidW == dw && g_vidH == dh) { av_frame_free(&f); return; }   // ya esta al tamano correcto
    }
    SwsContext* sws = sws_getContext(f->width, f->height, (AVPixelFormat)f->format, dw, dh, AV_PIX_FMT_BGRA, SWS_BILINEAR, nullptr, nullptr, nullptr);
    if (sws) {
        std::vector<uint8_t> px((size_t)dw * dh * 4);
        uint8_t* dp[1] = { px.data() };
        int ds[1] = { dw * 4 };
        sws_scale(sws, f->data, f->linesize, 0, f->height, dp, ds);
        sws_freeContext(sws);
        Lock l(&g_vidCS);
        g_vidPix.swap(px);
        g_vidW = dw; g_vidH = dh;
    }
    av_frame_free(&f);
}

// Mover un control sin copiar los pixeles viejos (esa copia dejaba textos duplicados y bloques blancos al estirar)
static void moveCtl(HWND c, int x, int y, int w, int h, BOOL = TRUE) {
    SetWindowPos(c, nullptr, x, y, w, h, SWP_NOZORDER | SWP_NOACTIVATE | SWP_NOCOPYBITS);
}

static bool g_childrenHidden = false;   // pantalla completa: todos los controles ocultos
static void layout(int w, int h) {
    if (w <= 0 || h <= 0) return;
    const int M = MARGIN;
    if (g_fullscreen) {
        g_childrenHidden = true;
        EnumChildWindows(g_hwnd, [](HWND c, LPARAM) -> BOOL { ShowWindow(c, SW_HIDE); return TRUE; }, 0);
        g_videoRect = { 0, 0, w, h };
        g_waveRect = { 0, 0, 0, 0 };
        g_panelRect = { 0, 0, 0, 0 };
        g_spdRect = { 0, 0, 0, 0 };
        g_vidAreaW = w; g_vidAreaH = h;
        if (g_hasVideo) rescaleLastFrame();
        return;
    }
    // Solo se muestran todos los controles al salir de pantalla completa. Mostrarlos siempre y volver a ocultar algunos
    // (listas, panel de velocidad) en cada cambio de tamano dejaba a esas listas dibujadas encima de los demas controles.
    if (g_childrenHidden) {
        g_childrenHidden = false;
        EnumChildWindows(g_hwnd, [](HWND c, LPARAM) -> BOOL { ShowWindow(c, SW_SHOW); return TRUE; }, 0);
    }
    const bool video = g_hasVideo.load();
    if (!video) g_listFloat = false;
    const int ww = std::max(300, w - 2 * M);

    // franja de botones: tamano normal; solo si la ventana es estrecha se compacta (Abrir y Stop quedan solo con
    // icono) y, si aun asi no cabe, todos los botones se encogen proporcionalmente
    {
        const int bh = 40, by = CAP_H + (BAND_H - bh) / 2;
        const int rightFull = 52 + (video ? 8 + 52 : 0), gapLR = 16;
        const int needFull = 548 + gapLR + rightFull, needCompact = 472 + gapLR + rightFull;
        double f = 1.0;
        g_tbCompact = false;
        if (w - 2 * M < needFull) {
            g_tbCompact = true;
            f = std::max(0.6, std::min(1.0, (double)(w - 2 * M) / needCompact));
        }
        auto sc = [&](int v) { return std::max(2, (int)lrint(v * f)); };
        int x = M;
        struct B { int id, w, gap; } bs[] = { { ID_OPEN, g_tbCompact ? 52 : 90, 8 }, { ID_FOLDER, 52, 20 }, { ID_PREV, 52, 8 },
                                              { ID_PLAY, 60, 8 }, { ID_STOP, g_tbCompact ? 52 : 90, 8 }, { ID_NEXT, 52, 20 } };
        for (auto& bb : bs) { const int bw = sc(bb.w); moveCtl(GetDlgItem(g_hwnd, bb.id), x, by, bw, bh, TRUE); x += bw + sc(bb.gap); }
        const int spdX = x;
        moveCtl(GetDlgItem(g_hwnd, ID_SPEED), spdX, by, sc(80), bh, TRUE);
        if (g_spdOpen) {   // panel de velocidad bajo la barra de botones
            const int pw = std::min(340, w - 2 * M), px = std::max(M, std::min(spdX, w - M - pw));
            const int top = CAP_H + BAND_H + 6;
            g_spdRect = { px, top, px + pw, top + 100 };
            moveCtl(GetDlgItem(g_hwnd, ID_SPDRESET), px + pw - 14 - 56, top + 6, 56, 28, TRUE);
            moveCtl(g_spd, px + 14, top + 38, pw - 28, 28, TRUE);
            ShowWindow(GetDlgItem(g_hwnd, ID_SPDRESET), SW_SHOW);
            ShowWindow(g_spd, SW_SHOW);
            BringWindowToTop(GetDlgItem(g_hwnd, ID_SPDRESET));
            BringWindowToTop(g_spd);
        } else {
            g_spdRect = { 0, 0, 0, 0 };
            ShowWindow(GetDlgItem(g_hwnd, ID_SPDRESET), SW_HIDE);
            ShowWindow(g_spd, SW_HIDE);
        }
        const int outW = sc(52), lstW = sc(52);
        g_tbFreeX0 = spdX + sc(80) + 18;
        g_tbFreeX1 = (video ? (w - M - outW - sc(8) - lstW) : (w - M - outW)) - 18;
        moveCtl(GetDlgItem(g_hwnd, ID_OUTBTN), w - M - outW, by, outW, bh, TRUE);
        HWND lb = GetDlgItem(g_hwnd, ID_LISTBTN);
        moveCtl(lb, w - M - outW - sc(8) - lstW, by, lstW, bh, TRUE);
        ShowWindow(lb, video ? SW_SHOW : SW_HIDE);
    }

    int y = CAP_H + BAND_H + 10;
    const int rowH = 38;
    const int lowerH = 26 + rowH * 2 + 32 + 62;
    const int waveH = video ? 64 : 112;   // 75% de 150
    if (video) {
        int vh = std::max(100, h - y - M - (waveH + 8) - lowerH);
        g_videoRect = { M, y, M + ww, y + vh };
        g_vidAreaW = ww; g_vidAreaH = vh;
        g_videoAreaH = vh;
        y += vh + 8;
    } else {
        g_videoRect = { 0, 0, 0, 0 };
        g_videoAreaH = 0;
    }
    g_waveRect = { M, y, M + ww, y + waveH };
    y += waveH + 6;
    moveCtl(g_time, M + 2, y, ww, 20, TRUE);
    y += 26;
    moveCtl(g_volTxt, M + 2, y, 190, 22, TRUE);
    moveCtl(g_vol, M + 200, y - 6, std::max(100, ww - 200 - 112), 28, TRUE);
    moveCtl(g_mute, M + ww - 98, y - 4, 98, 30, TRUE);
    y += rowH;
    moveCtl(g_gainTxt, M + 2, y, 190, 22, TRUE);
    moveCtl(g_gain, M + 200, y - 6, std::max(100, ww - 200 - 112), 28, TRUE);
    moveCtl(GetDlgItem(g_hwnd, ID_RESET), M + ww - 98, y - 4, 98, 30, TRUE);
    y += rowH;
    moveCtl(g_limit, M, y, 190, 26, TRUE);
    moveCtl(g_peakChk, M + 200, y, 66, 26, TRUE);
    {
        const int ibw = 34, ibh = 28, gp = 6;
        auto bx = [&](int i) { return M + ww - (i + 1) * ibw - i * gp; };   // i = 0 es el de mas a la derecha
        moveCtl(GetDlgItem(g_hwnd, ID_ABOUT), bx(0), y - 1, ibw, ibh, TRUE);
        moveCtl(GetDlgItem(g_hwnd, ID_NETURL), bx(1), y - 1, ibw, ibh, TRUE);
        moveCtl(GetDlgItem(g_hwnd, ID_SNAPOPT), bx(2), y - 1, ibw, ibh, TRUE);
        moveCtl(GetDlgItem(g_hwnd, ID_SNAPDIR), bx(3), y - 1, ibw, ibh, TRUE);
        moveCtl(GetDlgItem(g_hwnd, ID_SNAP), bx(4), y - 1, ibw, ibh, TRUE);
        moveCtl(g_peakTxt, M + 270, y + 2, std::max(10, ww - 270 - (5 * ibw + 4 * gp + 10)), 22, TRUE);
    }
    y += 34;
    int cw = ww / 3;
    for (int i = 0; i < 3; i++) {
        moveCtl(g_eqTxt[i], M + 2 + i * cw, y, cw - 8, 20, TRUE);
        moveCtl(g_eq[i], M + i * cw, y + 20, cw - 8, 28, TRUE);
    }
    y += 62;
    g_plMid = 0;
    g_panelFloating = video && g_listFloat;
    if (video && !g_listFloat) {
        ShowWindow(g_list, SW_HIDE);
        ShowWindow(g_plList, SW_HIDE);
        ShowWindow(g_plSave, SW_HIDE); ShowWindow(g_plImp, SW_HIDE); ShowWindow(g_plExp, SW_HIDE);
        g_panelRect = { 0, 0, 0, 0 };
    } else {
        RECT pr;
        if (video) {   // cuadro flotante sobre el video: lista actual + listas guardadas, pegado a la derecha
            const int fw = std::min(640, std::max(420, ww - 16));
            const int top = (int)g_videoRect.top + 8, bot = std::max(top + 190, (int)g_videoRect.bottom - 8);
            pr = { (LONG)g_videoRect.right - 8 - fw, (LONG)top, (LONG)g_videoRect.right - 8, (LONG)bot };
        } else {
            pr = { M, y, M + ww, std::max(y + 110, h - M) };
        }
        g_panelRect = pr;
        const int hdr = 36;
        const int pw = pr.right - pr.left, lh = pr.bottom - pr.top - hdr;
        const int mid = pr.left + pw / 2;
        g_plMid = mid;
        const int lw = mid - pr.left - 1, rw = pr.right - mid - 1;
        moveCtl(g_list, pr.left, pr.top + hdr, lw, lh, TRUE);
        SetWindowRgn(g_list, CreateRoundRectRgn(0, -24, lw + 25, lh + 1, 24, 24), TRUE);       // redondea abajo-izquierda
        moveCtl(g_plList, mid + 1, pr.top + hdr, rw, lh, TRUE);
        SetWindowRgn(g_plList, CreateRoundRectRgn(-24, -24, rw + 1, lh + 1, 24, 24), TRUE);    // redondea abajo-derecha
        {
            const int bsz = 28, byy = pr.top + 4;   // importar | exportar | guardar, al final de la cabecera izquierda
            int bx = mid - 10 - bsz;
            moveCtl(g_plSave, bx, byy, bsz, bsz, TRUE); bx -= bsz + 2;
            moveCtl(g_plExp, bx, byy, bsz, bsz, TRUE);  bx -= bsz + 2;
            moveCtl(g_plImp, bx, byy, bsz, bsz, TRUE);
        }
        ShowWindow(g_list, SW_SHOW); ShowWindow(g_plList, SW_SHOW);
        ShowWindow(g_plSave, SW_SHOW); ShowWindow(g_plImp, SW_SHOW); ShowWindow(g_plExp, SW_SHOW);
        BringWindowToTop(g_list); BringWindowToTop(g_plList);
        BringWindowToTop(g_plSave); BringWindowToTop(g_plImp); BringWindowToTop(g_plExp);
    }
    if (g_hasVideo) rescaleLastFrame();
    // repintar ventana y TODOS los controles hijos: al mover/redimensionar controles Windows copia pixeles
    // viejos (barras sin dibujar, botones y textos duplicados) si solo se invalida la ventana padre
    // Mientras se estira la ventana con el mouse se pintan los hijos de inmediato (RDW_UPDATENOW): si no, Windows los
    // deja "en blanco" unos instantes hasta que le toca repintarlos.
    RedrawWindow(g_hwnd, nullptr, nullptr, RDW_INVALIDATE | RDW_ERASE | RDW_ALLCHILDREN | (g_liveSize ? RDW_UPDATENOW : 0));
}

static void showOutputMenu() {
    std::vector<OutDev> devs = listOutputs();
    std::wstring cur;
    { Lock l(&g_hwoCS); cur = g_outName; }
    HMENU m = CreatePopupMenu();
    AppendMenu(m, MF_STRING | (cur.empty() ? MF_CHECKED : 0), 5000, TX(L"Predeterminado del sistema"));
    AppendMenu(m, MF_SEPARATOR, 0, nullptr);
    bool found = cur.empty();
    for (size_t i = 0; i < devs.size(); i++) {
        const bool sel = !cur.empty() && devs[i].name == cur;
        if (sel) found = true;
        AppendMenu(m, MF_STRING | (sel ? MF_CHECKED : 0), 5001 + (UINT)i, devs[i].name.c_str());
    }
    if (!found) AppendMenu(m, MF_STRING | MF_CHECKED | MF_GRAYED, 4999, (cur + TX(L" (no disponible)")).c_str());
    RECT br;
    GetWindowRect(GetDlgItem(g_hwnd, ID_OUTBTN), &br);
    SetForegroundWindow(g_hwnd);
    int cmd = TrackPopupMenu(m, TPM_RETURNCMD | TPM_RIGHTALIGN | TPM_TOPALIGN, br.right, br.bottom + 2, 0, g_hwnd, nullptr);
    DestroyMenu(m);
    { POINT cp; GetCursorPos(&cp); if (PtInRect(&br, cp)) g_outMenuClosedOnBtn = GetTickCount(); }   // clic encima del boton = solo cerrar
    if (cmd == 5000) chooseOutput(L"");
    else if (cmd >= 5001 && cmd - 5001 < (int)devs.size()) chooseOutput(devs[cmd - 5001].name);
}

static void updateSpeedUI() {
    wchar_t b[48];
    swprintf(b, 48, L"%.2fx", g_speed.load());
    SetWindowText(GetDlgItem(g_hwnd, ID_SPEED), b);
    InvalidateRect(GetDlgItem(g_hwnd, ID_SPEED), nullptr, FALSE);
    if (g_spdRect.right > g_spdRect.left) InvalidateRect(g_hwnd, &g_spdRect, FALSE);
}

static void speedFromSlider() {
    int p = (int)SendMessage(g_spd, TBM_GETPOS, 0, 0);
    float sp;
    if (abs(p) <= 2) { sp = 1.f; if (p != 0) SendMessage(g_spd, TBM_SETPOS, TRUE, 0); }   // iman en 1x
    else sp = powf(3.f, p / 100.f);
    g_speed = sp;
    updateSpeedUI();
}

static void resetSpeed() {
    SendMessage(g_spd, TBM_SETPOS, TRUE, 0);
    g_speed = 1.f;
    updateSpeedUI();
}

static void relayout() {
    RECT rc;
    GetClientRect(g_hwnd, &rc);
    layout(rc.right, rc.bottom);
}

static void toggleFullscreen() {
    if (!g_fullscreen) {
        if (!g_hasVideo) return;
        g_savedStyle = GetWindowLong(g_hwnd, GWL_STYLE);
        GetWindowPlacement(g_hwnd, &g_savedPlacement);
        MONITORINFO mi = { sizeof(mi) };
        GetMonitorInfo(MonitorFromWindow(g_hwnd, MONITOR_DEFAULTTONEAREST), &mi);
        g_fullscreen = true;
        g_fsVisible = true; g_fsLastMove = GetTickCount(); g_fsDrag = 0; g_fsHot = 0; g_fsVolHover = false; g_fsLastPos = { -1, -1 };
        SetWindowLong(g_hwnd, GWL_STYLE, (g_savedStyle & ~WS_OVERLAPPEDWINDOW) | WS_POPUP);
        SetWindowPos(g_hwnd, HWND_TOP, mi.rcMonitor.left, mi.rcMonitor.top, mi.rcMonitor.right - mi.rcMonitor.left,
                     mi.rcMonitor.bottom - mi.rcMonitor.top, SWP_FRAMECHANGED | SWP_NOOWNERZORDER);
    } else {
        g_fullscreen = false;
        SetWindowLong(g_hwnd, GWL_STYLE, g_savedStyle);
        SetWindowPlacement(g_hwnd, &g_savedPlacement);
        SetWindowPos(g_hwnd, nullptr, 0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE | SWP_NOZORDER | SWP_NOOWNERZORDER | SWP_FRAMECHANGED);
    }
    relayout();
    SetFocus(g_hwnd);
    InvalidateRect(g_hwnd, nullptr, TRUE);
}

// Teclas comunes (ventana y lista). Devuelve true si la tecla fue atendida.
static bool handleKey(WPARAM w, bool inList) {
    bool ctrl = (GetKeyState(VK_CONTROL) & 0x8000) != 0;
    switch (w) {
    case VK_SPACE: togglePause(); return true;
    case VK_LEFT: seekTo(g_posSec - (ctrl ? 30 : 5)); return true;
    case VK_RIGHT: seekTo(g_posSec + (ctrl ? 30 : 5)); return true;
    case VK_UP: if (inList && !ctrl) return false; setVolume(g_vol01.load() + 0.05f); return true;
    case VK_DOWN: if (inList && !ctrl) return false; setVolume(g_vol01.load() - 0.05f); return true;
    case 'M': g_muted = !g_muted.load(); updateLabels(); return true;
    case 'S': captureFrame(); return true;   // captura el fotograma actual
    case VK_F11: case 'F': toggleFullscreen(); return true;
    case VK_ESCAPE:
        if (g_fullscreen) { toggleFullscreen(); return true; }
        if (g_spdOpen) { g_spdOpen = false; relayout(); return true; }
        if (g_listFloat) { g_listFloat = false; relayout(); return true; }
        return false;
    case VK_PRIOR: nextTrack(-1); return true;
    case VK_NEXT: nextTrack(1); return true;
    }
    return false;
}

// ------------------------------------------------------------ creacion de controles
static HWND mkBtn(HWND parent, const wchar_t* t, int id, int x, int w) {
    HWND b = CreateWindow(L"BUTTON", t, WS_CHILD | WS_VISIBLE | WS_TABSTOP | BS_OWNERDRAW, x, 10, w, 30, parent,
                          (HMENU)(INT_PTR)id, nullptr, nullptr);
    SetWindowSubclass(b, BtnProc, 1, 0);
    return b;
}

static HWND mkSlider(HWND parent, int id, int lo, int hi, int pos, int tick) {
    HWND t = CreateWindow(TRACKBAR_CLASS, L"", WS_CHILD | WS_VISIBLE | WS_TABSTOP | TBS_NOTICKS | TBS_HORZ, 0, 0, 100, 34,
                          parent, (HMENU)(INT_PTR)id, nullptr, nullptr);
    SendMessage(t, TBM_SETRANGE, TRUE, MAKELONG(lo, hi));
    SendMessage(t, TBM_SETPOS, TRUE, pos);
    SetProp(t, L"tick", (HANDLE)(INT_PTR)tick);
    SetWindowSubclass(t, SliderProc, 2, 0);
    return t;
}

static HWND mkStatic(HWND parent, int id) {
    return CreateWindow(L"STATIC", L"", WS_CHILD | WS_VISIBLE, 0, 0, 100, 20, parent, (HMENU)(INT_PTR)id, nullptr, nullptr);
}

// ------------------------------------------------------------ pintura
static void paintVideo(HDC hdc) {
    RECT rc = g_videoRect;
    int W = rc.right - rc.left, H = rc.bottom - rc.top;
    if (W <= 0 || H <= 0) return;
    HDC dc = CreateCompatibleDC(hdc);
    HBITMAP bmp = CreateCompatibleBitmap(hdc, W, H);
    HGDIOBJ old = SelectObject(dc, bmp);
    RECT r0 = { 0, 0, W, H };
    FillRect(dc, &r0, (HBRUSH)GetStockObject(BLACK_BRUSH));
    {
        Lock l(&g_vidCS);
        if (!g_vidPix.empty() && g_vidW > 0 && g_vidH > 0) {
            BITMAPINFO bi = {};
            bi.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
            bi.bmiHeader.biWidth = g_vidW;
            bi.bmiHeader.biHeight = -g_vidH;   // de arriba hacia abajo
            bi.bmiHeader.biPlanes = 1;
            bi.bmiHeader.biBitCount = 32;
            bi.bmiHeader.biCompression = BI_RGB;
            SetDIBitsToDevice(dc, (W - g_vidW) / 2, (H - g_vidH) / 2, g_vidW, g_vidH, 0, 0, 0, g_vidH,
                              g_vidPix.data(), &bi, DIB_RGB_COLORS);
        }
    }
    BitBlt(hdc, rc.left, rc.top, W, H, dc, 0, 0, SRCCOPY);
    SelectObject(dc, old);
    DeleteObject(bmp);
    DeleteDC(dc);
}

static void paintWave(HDC hdc) {
    RECT rc = g_waveRect;
    int W = rc.right - rc.left, H = rc.bottom - rc.top;
    if (W <= 0 || H <= 0) return;
    HDC dc = CreateCompatibleDC(hdc);
    HBITMAP bmp = CreateCompatibleBitmap(hdc, W, H);
    HGDIOBJ old = SelectObject(dc, bmp);
    RECT r0 = { 0, 0, W, H };
    fillRect(dc, r0, C_BG);                                   // esquinas: color de la ventana
    HRGN rr = CreateRoundRectRgn(0, 0, W + 1, H + 1, 22, 22);
    SelectClipRgn(dc, rr);
    fillRect(dc, r0, C_WAVEBG);

    double dur = g_duration;
    double pos = g_dragging ? g_dragFrac * dur : g_posSec.load();
    bool drewWave = false;
    int playX = 0;
    {
        Lock l(&g_waveCS);
        size_t n = g_peaks.size();
        double total = dur > 0 ? dur : n * SEC_PER_PEAK;
        if (total > 0 && n > 0) {
            drewWave = true;
            float gain = powf(10.f, g_gainDb.load() / 20.f);
            const float volAmp = g_vol01.load() * g_vol01.load();
            // solo visual: para temas muy fuertes (picos >= 0 dBFS, ganancia original alta en mp3gain) la altura se
            // normaliza por el pico del tema, asi nunca toca los extremos del contenedor; el audio no se altera
            float tpk = 1.f;
            for (size_t i = 0; i < n; i++) tpk = std::max(tpk, g_peaks[i]);
            int mid = H / 2;
            HPEN pPlay = CreatePen(PS_SOLID, 1, C_WAVE_PLAYED);
            HPEN pRest = CreatePen(PS_SOLID, 1, C_WAVE_REST);
            HPEN pClip = CreatePen(PS_SOLID, 1, C_WAVE_CLIP);
            playX = (int)(pos / total * W);
            for (int x = 0; x < W; x++) {
                size_t i0 = (size_t)((double)x / W * total / SEC_PER_PEAK);
                size_t i1 = (size_t)((double)(x + 1) / W * total / SEC_PER_PEAK);
                if (i0 >= n) break;
                if (i1 <= i0) i1 = i0 + 1;
                if (i1 > n) i1 = n;
                float m = 0;
                for (size_t i = i0; i < i1; i++) m = std::max(m, g_peaks[i]);
                float a = m * gain;
                bool clip = a * volAmp > 1.0f;
                // hasta 0 dBFS la altura crece lineal (90-100% => ~63-70% del alto); por encima (picos de AAC,
                // ganancia) sube muy poco y nunca llena todo el contenedor (tope ~90%)
                const float ad = a / tpk;
                const float fh = ad <= 1.f ? ad * WAVE_REF : WAVE_REF + (0.90f - WAVE_REF) * (1.f - expf(-(ad - 1.f) * 2.5f));
                int hh = (int)(fh * (mid - 3));
                SelectObject(dc, clip ? pClip : (x <= playX ? pPlay : pRest));
                MoveToEx(dc, x, mid - hh, nullptr);
                LineTo(dc, x, mid + hh + 1);
            }
            DeleteObject(pPlay); DeleteObject(pRest); DeleteObject(pClip);
        } else {
            SetBkMode(dc, TRANSPARENT);
            SetTextColor(dc, g_live ? RGB(236, 98, 92) : RGB(150, 160, 175));
            SelectObject(dc, g_font);
            const wchar_t* msg = g_live ? TX(L"\u25CF  EN VIVO  \u2014  transmisi\u00f3n de red") : g_playlist.empty() ? TX(L"Arrastra archivos o carpetas aqui, o usa Abrir / Carpeta")
                               : g_waveDone ? TX(L"Sin forma de onda") : TX(L"Analizando forma de onda...");
            DrawText(dc, msg, -1, &r0, DT_CENTER | DT_VCENTER | DT_SINGLELINE);
        }
    }
    if (drewWave) {
        Graphics g(dc);
        g.SetSmoothingMode(Gdiplus::SmoothingModeAntiAlias);
        // capsula de progreso translucida
        float cw = (float)std::max(18, playX + 2), ch = 26.f;
        GraphicsPath cap;
        rrPath(cap, 3.f, H / 2.f - ch / 2, std::min(cw, (float)W - 6.f), ch, ch / 2);
        SolidBrush cb(Gdiplus::Color(105, 255, 255, 255));
        g.FillPath(&cb, &cap);
        // cabezal
        Pen hp(Col(RGB(255, 255, 255)), 2.f);
        float px = (float)std::min(std::max(playX, 1), W - 2);
        g.DrawLine(&hp, px, 0.f, px, (float)H);
        g.Flush();
    }
    SelectClipRgn(dc, nullptr);
    DeleteObject(rr);
    BitBlt(hdc, rc.left, rc.top, W, H, dc, 0, 0, SRCCOPY);
    SelectObject(dc, old);
    DeleteObject(bmp);
    DeleteDC(dc);
}

static RECT capBtnRect(int i, int W) {   // 0 = minimizar, 1 = maximizar, 2 = cerrar
    return { W - CAP_BTN_W * (3 - i), 0, W - CAP_BTN_W * (2 - i), CAP_H };
}

static int capButtonAt(int x, int y, int W) {
    if (g_fullscreen || y < 0 || y >= CAP_H) return -1;
    for (int i = 0; i < 3; i++) {
        RECT r = capBtnRect(i, W);
        if (x >= r.left && x < r.right) return i;
    }
    return -1;
}

static void paintCaption(HDC dc, HWND hwnd, int W) {
    RECT cr = { 0, 0, W, CAP_H };
    fillRect(dc, cr, C_TITLE);
    if (g_iconSmall) DrawIconEx(dc, 16, (CAP_H - 20) / 2, g_iconSmall, 20, 20, 0, nullptr, DI_NORMAL);
    SetBkMode(dc, TRANSPARENT);
    SelectObject(dc, g_font);
    SetTextColor(dc, RGB(214, 217, 222));
    RECT lr = { 44, 0, 260, CAP_H };
    DrawText(dc, L"MiauPlayer", -1, &lr, DT_LEFT | DT_VCENTER | DT_SINGLELINE);
    if (!g_titleName.empty()) {
        SelectObject(dc, g_fontBold);
        SetTextColor(dc, RGB(245, 246, 248));
        RECT tr = { 250, 0, W - CAP_BTN_W * 3 - 20, CAP_H };
        DrawText(dc, g_titleName.c_str(), -1, &tr, DT_CENTER | DT_VCENTER | DT_SINGLELINE | DT_END_ELLIPSIS | DT_NOPREFIX);
    }
    Graphics g(dc);
    g.SetSmoothingMode(Gdiplus::SmoothingModeAntiAlias);
    for (int i = 0; i < 3; i++) {
        RECT r = capBtnRect(i, W);
        if (g_capHot == i) fillRect(dc, r, i == 2 ? RGB(196, 43, 28) : RGB(58, 62, 70));
        float cx = (r.left + r.right) / 2.f, cy = CAP_H / 2.f;
        Pen p(Col(RGB(232, 234, 237)), 1.2f);
        if (i == 0) g.DrawLine(&p, cx - 5, cy, cx + 5, cy);
        else if (i == 1) {
            if (IsZoomed(hwnd)) {
                g.DrawRectangle(&p, cx - 5.f, cy - 2.f, 7.f, 7.f);
                g.DrawLine(&p, cx - 2, cy - 5, cx + 5, cy - 5);
                g.DrawLine(&p, cx + 5, cy - 5, cx + 5, cy + 2);
            } else g.DrawRectangle(&p, cx - 5.f, cy - 5.f, 10.f, 10.f);
        } else {
            g.DrawLine(&p, cx - 5, cy - 5, cx + 5, cy + 5);
            g.DrawLine(&p, cx + 5, cy - 5, cx - 5, cy + 5);
        }
    }
    g.Flush();
}

// marcas bajo cada slider (se dibujan en la ventana, en la franja que queda debajo del control)
static void paintTicks(HDC dc, HWND hwnd) {
    HWND sl[6] = { g_vol, g_gain, g_eq[0], g_eq[1], g_eq[2], g_spd };
    Graphics g(dc);
    g.SetSmoothingMode(Gdiplus::SmoothingModeAntiAlias);
    for (HWND s : sl) {
        if (!s || !IsWindowVisible(s)) continue;
        if (s == g_vol) continue;   // las del volumen las dibuja el propio slider, encima de la barra
        // las marcas llevan el color de la perilla (los mismos de drawSliderInto): volumen blanca, ganancia rosada
        Pen p(Col(s == g_vol ? RGB(250, 251, 252) : s == g_gain ? RGB(186, 130, 128) : RGB(122, 128, 138)), 1.f);
        RECT wr;
        GetWindowRect(s, &wr);
        MapWindowPoints(nullptr, hwnd, (POINT*)&wr, 2);
        int mn = (int)SendMessage(s, TBM_GETRANGEMIN, 0, 0), mx = (int)SendMessage(s, TBM_GETRANGEMAX, 0, 0);
        int step = (int)(INT_PTR)GetProp(s, L"tick");
        if (step <= 0 || mx <= mn) continue;
        const float tw = 11.f;   // ancho de la perilla
        for (int v = mn; v <= mx; v += step) {
            float x = wr.left + tw / 2 + (float)(v - mn) / (mx - mn) * ((float)(wr.right - wr.left) - tw);
            g.DrawLine(&p, x, (float)wr.bottom + 1.f, x, (float)wr.bottom + 6.f);
        }
    }
    g.Flush();
}

// ------------------------------------------------------------ controles flotantes en pantalla completa
// Reproducir/pausa a la izquierda, tiempo y barra al centro, volumen y salir a la derecha.
// Se ocultan (junto con el cursor) si el mouse no se mueve durante 3 segundos.

struct FsL { RECT bar, play, elapsed, seek, total, vol, volSlider, volZone, exitB, cap; };
static FsL fsLayout(int W, int H) {
    FsL l;
    const int cy = H - 46;
    l.bar = { 0, H - 130, W, H };
    l.play = { 24, cy - 24, 72, cy + 24 };
    l.elapsed = { 84, cy - 12, 140, cy + 12 };
    const int exL = W - 24 - 48;
    l.exitB = { exL, cy - 24, exL + 48, cy + 24 };
    const int capL = exL - 6 - 48;                       // captura de fotograma, antes de "salir"
    l.cap = { capL, cy - 24, capL + 48, cy + 24 };
    const int vL = capL - 6 - 48;
    l.vol = { vL, cy - 24, vL + 48, cy + 24 };
    l.volSlider = { vL - 104, cy - 14, vL - 4, cy + 14 };
    l.volZone = { vL - 104, cy - 24, vL + 48, cy + 24 };
    l.total = { vL - 12 - 70, cy - 12, vL - 12, cy + 12 };
    l.seek = { 150, cy - 14, (int)l.total.left - 14, cy + 14 };
    return l;
}

static double fsDuration() {
    double d = g_duration;
    if (d <= 0) { Lock lk(&g_waveCS); d = g_peaks.size() * SEC_PER_PEAK; }
    return d;
}

static void fsInvalidateBar() {
    RECT cr;
    GetClientRect(g_hwnd, &cr);
    RECT r = { 0, cr.bottom - 140, cr.right, cr.bottom };
    InvalidateRect(g_hwnd, &r, FALSE);
}

static void fsShow() {
    g_fsLastMove = GetTickCount();
    if (!g_fsVisible) {
        g_fsVisible = true;
        SetCursor(LoadCursor(nullptr, IDC_ARROW));
        fsInvalidateBar();
    }
}

static void paintFsControls(HDC dc, int W, int H) {
    const FsL L = fsLayout(W, H);
    const bool live = g_live.load();
    Graphics g(dc);
    g.SetSmoothingMode(Gdiplus::SmoothingModeAntiAlias);
    {   // degradado oscuro abajo: da contraste sin tapar el video
        Gdiplus::LinearGradientBrush gb(Gdiplus::Point(0, L.bar.top), Gdiplus::Point(0, H), Gdiplus::Color(0, 0, 0, 0), Gdiplus::Color(190, 0, 0, 0));
        g.FillRectangle(&gb, 0, (int)L.bar.top, W, H - (int)L.bar.top);
    }
    auto hoverBg = [&](const RECT& r, int id) {
        if (g_fsHot != id) return;
        SolidBrush hb(Gdiplus::Color(46, 255, 255, 255));
        g.FillEllipse(&hb, (float)r.left + 2, (float)r.top + 2, (float)(r.right - r.left) - 4, (float)(r.bottom - r.top) - 4);
    };
    const float is = 26.f;
    auto iconAt = [&](const RECT& r, int ico) {
        drawIcon(g, ico, r.left + ((r.right - r.left) - is) / 2, r.top + ((r.bottom - r.top) - is) / 2, is, RGB(255, 255, 255));
    };
    hoverBg(L.play, 1);
    iconAt(L.play, g_state == ST_PLAYING ? ICO_PAUSE : ICO_PLAY);
    hoverBg(L.vol, 3);
    iconAt(L.vol, ICO_SPK);
    if (g_muted.load() || g_vol01.load() <= 0.001f) {   // barra diagonal = silenciado
        Pen mp(Gdiplus::Color(255, 255, 255, 255), 2.6f);
        mp.SetStartCap(Gdiplus::LineCapRound); mp.SetEndCap(Gdiplus::LineCapRound);
        const float x = L.vol.left + ((L.vol.right - L.vol.left) - is) / 2, y = L.vol.top + ((L.vol.bottom - L.vol.top) - is) / 2;
        g.DrawLine(&mp, x + is * 0.14f, y + is * 0.86f, x + is * 0.90f, y + is * 0.10f);
    }
    hoverBg(L.cap, 6);
    iconAt(L.cap, ICO_CAMERA);
    hoverBg(L.exitB, 4);
    iconAt(L.exitB, ICO_FSEXIT);

    // barra de tiempo
    const double dur = fsDuration();
    const double pos = g_fsDrag == 1 ? g_dragFrac * dur : (double)g_posSec.load();
    const double frac = (!live && dur > 0) ? std::min(1.0, std::max(0.0, pos / dur)) : 0.0;
    {
        const float ty = (L.seek.top + L.seek.bottom) / 2.f;
        const bool hot = g_fsHot == 2 || g_fsDrag == 1;
        const float th = hot ? 6.f : 4.f;
        const float x0 = (float)L.seek.left, x1 = (float)L.seek.right;
        GraphicsPath tp;
        rrPath(tp, x0, ty - th / 2, x1 - x0, th, th / 2);
        SolidBrush tb(Gdiplus::Color(live ? 60 : 105, 255, 255, 255));
        g.FillPath(&tb, &tp);
        if (!live && frac > 0) {
            const float pw = std::max(th, (float)(x1 - x0) * (float)frac);
            GraphicsPath pp;
            rrPath(pp, x0, ty - th / 2, pw, th, th / 2);
            SolidBrush pb(Gdiplus::Color(255, 236, 98, 92));
            g.FillPath(&pb, &pp);
        }
        if (!live) {
            const float r = hot ? 8.f : 6.f, cx = x0 + (x1 - x0) * (float)frac;
            SolidBrush th2(Gdiplus::Color(255, 255, 255, 255));
            g.FillEllipse(&th2, cx - r, ty - r, r * 2, r * 2);
        }
    }
    // volumen: el deslizador aparece al pasar el mouse sobre el parlante
    if (g_fsVolHover || g_fsDrag == 2) {
        const float ty = (L.volSlider.top + L.volSlider.bottom) / 2.f;
        const float x0 = (float)L.volSlider.left + 6, x1 = (float)L.volSlider.right - 6;
        const float v = g_muted.load() ? 0.f : g_vol01.load();
        GraphicsPath tp;
        rrPath(tp, x0, ty - 2.f, x1 - x0, 4.f, 2.f);
        SolidBrush tb(Gdiplus::Color(105, 255, 255, 255));
        g.FillPath(&tb, &tp);
        if (v > 0.01f) {
            GraphicsPath pp;
            rrPath(pp, x0, ty - 2.f, std::max(4.f, (x1 - x0) * v), 4.f, 2.f);
            SolidBrush pb(Gdiplus::Color(255, 255, 255, 255));
            g.FillPath(&pb, &pp);
        }
        SolidBrush th2(Gdiplus::Color(255, 255, 255, 255));
        g.FillEllipse(&th2, x0 + (x1 - x0) * v - 6.f, ty - 6.f, 12.f, 12.f);
    }
    g.Flush();

    // tiempos (con una sombra de 1 px para leerse sobre imagenes claras)
    SetBkMode(dc, TRANSPARENT);
    SelectObject(dc, g_font);
    auto txt = [&](RECT r, const std::wstring& t, UINT fmt, COLORREF c) {
        RECT sh = r;
        OffsetRect(&sh, 1, 1);
        SetTextColor(dc, RGB(0, 0, 0));
        DrawText(dc, t.c_str(), -1, &sh, fmt | DT_SINGLELINE | DT_VCENTER | DT_NOPREFIX);
        SetTextColor(dc, c);
        DrawText(dc, t.c_str(), -1, &r, fmt | DT_SINGLELINE | DT_VCENTER | DT_NOPREFIX);
    };
    txt(L.elapsed, fmtTime(pos), DT_LEFT, RGB(255, 255, 255));
    if (!g_fsVolHover && g_fsDrag != 2) {
        if (live) txt(L.total, TX(L"\u25CF EN VIVO"), DT_RIGHT, RGB(255, 110, 104));
        else txt(L.total, dur > 0 ? fmtTime(dur) : L"--:--", DT_RIGHT, RGB(255, 255, 255));
    }
}

static void fsMouseMove(HWND h, POINT p) {
    RECT cr;
    GetClientRect(h, &cr);
    const FsL L = fsLayout(cr.right, cr.bottom);
    if (p.x != g_fsLastPos.x || p.y != g_fsLastPos.y) { g_fsLastPos = p; fsShow(); }
    const bool volz = g_fsVisible && PtInRect(&L.volZone, p);
    const bool volHover = volz || g_fsDrag == 2;
    int hot = 0;
    if (g_fsVisible) {
        if (PtInRect(&L.play, p)) hot = 1;
        else if (PtInRect(&L.seek, p)) hot = 2;
        else if (PtInRect(&L.vol, p)) hot = 3;
        else if (PtInRect(&L.cap, p)) hot = 6;
        else if (PtInRect(&L.exitB, p)) hot = 4;
        else if (volHover && PtInRect(&L.volSlider, p)) hot = 5;
    }
    if (hot != g_fsHot || volHover != g_fsVolHover) { g_fsHot = hot; g_fsVolHover = volHover; fsInvalidateBar(); }
    if (g_fsDrag == 1) {
        g_dragFrac = std::min(1.0, std::max(0.0, (double)(p.x - L.seek.left) / std::max(1, (int)(L.seek.right - L.seek.left))));
        fsInvalidateBar();
    } else if (g_fsDrag == 2) {
        const float a = (float)L.volSlider.left + 6, b = (float)L.volSlider.right - 6;
        setVolume((p.x - a) / (b - a));
        fsInvalidateBar();
    }
    TRACKMOUSEEVENT te = { sizeof(te), TME_LEAVE, h, 0 };
    TrackMouseEvent(&te);
}

// devuelve true si el clic cayo sobre los controles
static bool fsMouseDown(HWND h, POINT p) {
    if (!g_fsVisible) { fsShow(); return false; }
    RECT cr;
    GetClientRect(h, &cr);
    const FsL L = fsLayout(cr.right, cr.bottom);
    fsShow();
    if (PtInRect(&L.play, p)) { togglePause(); fsInvalidateBar(); return true; }
    if (PtInRect(&L.cap, p)) { captureFrame(); return true; }
    if (PtInRect(&L.exitB, p)) { toggleFullscreen(); return true; }
    if (PtInRect(&L.vol, p)) { g_muted = !g_muted.load(); updateLabels(); fsInvalidateBar(); return true; }
    if ((g_fsVolHover) && PtInRect(&L.volSlider, p)) {
        g_fsDrag = 2;
        SetCapture(h);
        const float a = (float)L.volSlider.left + 6, b = (float)L.volSlider.right - 6;
        setVolume((p.x - a) / (b - a));
        fsInvalidateBar();
        return true;
    }
    if (PtInRect(&L.seek, p) && !g_live) {
        g_fsDrag = 1;
        SetCapture(h);
        g_dragFrac = std::min(1.0, std::max(0.0, (double)(p.x - L.seek.left) / std::max(1, (int)(L.seek.right - L.seek.left))));
        fsInvalidateBar();
        return true;
    }
    return PtInRect(&L.bar, p) != 0;   // el resto de la franja inferior tambien absorbe el clic
}

static void fsMouseUp() {
    if (g_fsDrag == 1) {
        g_fsDrag = 0;
        ReleaseCapture();
        seekTo(g_dragFrac * fsDuration());
    } else if (g_fsDrag == 2) {
        g_fsDrag = 0;
        ReleaseCapture();
    }
    g_fsLastMove = GetTickCount();
    fsInvalidateBar();
}

// llamado por el temporizador: oculta controles y cursor tras 3 s sin mover el mouse
static void fsTick() {
    if (g_toastUntil && (int)(GetTickCount() - g_toastUntil) > 0) { g_toastUntil = 0; InvalidateRect(g_hwnd, nullptr, FALSE); }
    const bool want = g_fsDrag != 0 || (GetTickCount() - g_fsLastMove) < kFsHideMs;
    if (want != g_fsVisible) {
        g_fsVisible = want;
        g_fsHot = 0;
        g_fsVolHover = false;
        InvalidateRect(g_hwnd, nullptr, FALSE);
        if (!want) {
            POINT cp;
            GetCursorPos(&cp);
            if (WindowFromPoint(cp) == g_hwnd) SetCursor(nullptr);
        }
    } else if (g_fsVisible) {
        fsInvalidateBar();   // mantener al dia el tiempo y la barra aunque el video este en pausa
    }
}

// ---- boton "ampliar" sobre el video cuando no esta a pantalla completa
static bool g_expHot = false;
static bool expandBtnVisible() { return g_hasVideo && !g_fullscreen && !g_listFloat && g_videoRect.right > g_videoRect.left; }
static RECT expandBtnRect() {
    return { g_videoRect.right - 12 - 40, g_videoRect.bottom - 12 - 40, g_videoRect.right - 12, g_videoRect.bottom - 12 };
}
static void paintExpandBtn(HDC dc) {
    const RECT r = expandBtnRect();
    Graphics g(dc);
    g.SetSmoothingMode(Gdiplus::SmoothingModeAntiAlias);
    GraphicsPath p;
    rrPath(p, r.left + 0.5f, r.top + 0.5f, 39.f, 39.f, 10.f);
    SolidBrush bg(Gdiplus::Color(g_expHot ? 150 : 90, 0, 0, 0));   // fondo oscuro translucido para leerse sobre cualquier imagen
    g.FillPath(&bg, &p);
    drawIcon(g, ICO_FSENTER, r.left + 9.f, r.top + 9.f, 22.f, RGB(255, 255, 255));
    g.Flush();
}

static void paintToast(HDC dc, int W, int H) {   // aviso breve (por ejemplo "Fotograma guardado") sobre el video
    if (!g_toastUntil || (int)(g_toastUntil - GetTickCount()) <= 0 || g_toast.empty()) return;
    SelectObject(dc, g_fontBold);
    SIZE ts = {};
    GetTextExtentPoint32W(dc, g_toast.c_str(), (int)g_toast.size(), &ts);
    const int bw = ts.cx + 44, bh = 40, bx = (W - bw) / 2, by = H - 190;
    {
        Graphics g(dc);
        g.SetSmoothingMode(Gdiplus::SmoothingModeAntiAlias);
        GraphicsPath p;
        rrPath(p, (float)bx, (float)by, (float)bw, (float)bh, 12.f);
        SolidBrush bb(Gdiplus::Color(185, 0, 0, 0));
        g.FillPath(&bb, &p);
        g.Flush();
    }
    SetBkMode(dc, TRANSPARENT);
    SetTextColor(dc, RGB(255, 255, 255));
    RECT r = { bx, by, bx + bw, by + bh };
    DrawText(dc, g_toast.c_str(), -1, &r, DT_CENTER | DT_VCENTER | DT_SINGLELINE);
}

static void paintAll(HDC dc, HWND hwnd, int W, int H) {
    if (g_fullscreen) {
        RECT all = { 0, 0, W, H };
        fillRect(dc, all, RGB(0, 0, 0));
        paintVideo(dc);
        if (g_fsVisible) paintFsControls(dc, W, H);
        paintToast(dc, W, H);
        return;
    }
    RECT all = { 0, 0, W, H };
    fillRect(dc, all, C_BG);
    paintCaption(dc, hwnd, W);
    for (int i = 0; i < BAND_H; i++) {   // franja metalica oscura
        RECT row = { 0, CAP_H + i, W, CAP_H + i + 1 };
        fillRect(dc, row, bandColorAt(i));
    }
    paintCircuits(dc, CAP_H);
    paintVideo(dc);
    if (expandBtnVisible()) paintExpandBtn(dc);
    paintWave(dc);
    if (g_spdRect.right > g_spdRect.left) {
        const RECT& pr = g_spdRect;
        float x = pr.left + 0.5f, y = pr.top + 0.5f, w = (float)(pr.right - pr.left) - 1.f, h = (float)(pr.bottom - pr.top) - 1.f;
        {
            Graphics g(dc);
            g.SetSmoothingMode(Gdiplus::SmoothingModeAntiAlias);
            GraphicsPath p;
            rrPath(p, x, y, w, h, 12.f);
            SolidBrush pb(Col(C_BG));
            g.FillPath(&pb, &p);
            Pen bp(Col(RGB(96, 102, 112)), 1.2f);
            g.DrawPath(&bp, &p);
            g.Flush();
        }
        wchar_t b[64];
        const float sp = g_speed.load();
        if (fabsf(sp - 1.f) < 0.0005f) swprintf(b, 64, TX(L"Velocidad: 1.00x  (normal)"));
        else swprintf(b, 64, TX(L"Velocidad: %.2fx"), sp);
        SetBkMode(dc, TRANSPARENT);
        SelectObject(dc, g_font);
        SetTextColor(dc, C_TEXT);
        RECT lr = { pr.left + 16, pr.top + 8, pr.right - 80, pr.top + 34 };
        DrawText(dc, b, -1, &lr, DT_LEFT | DT_VCENTER | DT_SINGLELINE);
        SetTextColor(dc, RGB(150, 156, 164));
        const int sx0 = pr.left + 14, sx1 = pr.right - 14, cy = pr.top + 74;
        RECT c0 = { sx0, cy, sx0 + 70, cy + 20 }, c1 = { (sx0 + sx1) / 2 - 35, cy, (sx0 + sx1) / 2 + 35, cy + 20 }, c2 = { sx1 - 70, cy, sx1, cy + 20 };
        DrawText(dc, L"0.33x", -1, &c0, DT_LEFT | DT_SINGLELINE);
        DrawText(dc, L"1x", -1, &c1, DT_CENTER | DT_SINGLELINE);
        DrawText(dc, L"3x", -1, &c2, DT_RIGHT | DT_SINGLELINE);
    }
    paintTicks(dc, hwnd);
    if (g_panelRect.right > g_panelRect.left) {
        const RECT& pr = g_panelRect;
        float x = (float)pr.left + 0.5f, y = (float)pr.top + 0.5f, w = (float)(pr.right - pr.left) - 1.f, h = (float)(pr.bottom - pr.top) - 1.f;
        {
            Graphics g(dc);
            g.SetSmoothingMode(Gdiplus::SmoothingModeAntiAlias);
            GraphicsPath p;
            rrPath(p, x, y, w, h, 12.f);
            if (g_panelFloating) {   // sombra suave para que se despegue del video
                for (int i = 0; i < 4; i++) {
                    GraphicsPath sp;
                    rrPath(sp, x - 3.f + i, y + 1.f + i, w + 6.f - 2 * i, h + 8.f - 2 * i, 14.f);
                    SolidBrush sb(Gdiplus::Color(28 + i * 8, 0, 0, 0));
                    g.FillPath(&sb, &sp);
                }
            }
            SolidBrush pb(Col(C_PANEL));
            g.FillPath(&pb, &p);
            // cabecera
            GraphicsPath hp;
            const float hh = 36.f;
            hp.AddArc(x, y, 24.f, 24.f, 180, 90);
            hp.AddArc(x + w - 24.f, y, 24.f, 24.f, 270, 90);
            hp.AddLine(x + w, y + hh, x, y + hh);
            hp.CloseFigure();
            SolidBrush hb(Col(RGB(226, 229, 234)));
            g.FillPath(&hb, &hp);
            Pen sep(Col(RGB(205, 209, 215)), 1.f);
            g.DrawLine(&sep, x, y + hh, x + w, y + hh);
            g.Flush();
        }
        RECT hr = { pr.left + 18, pr.top, (g_plMid > 0 ? g_plMid - 106 : pr.right - 10), pr.top + 36 };
        SetBkMode(dc, TRANSPARENT);
        SelectObject(dc, g_fontBold);
        SetTextColor(dc, RGB(43, 47, 54));
        DrawText(dc, L"Playlist", -1, &hr, DT_LEFT | DT_VCENTER | DT_SINGLELINE);
        if (g_plMid > 0) {
            RECT rr2 = { g_plMid + 16, pr.top, pr.right - 10, pr.top + 36 };
            DrawText(dc, TX(L"Listas Guardadas"), -1, &rr2, DT_LEFT | DT_VCENTER | DT_SINGLELINE | DT_END_ELLIPSIS);
            RECT dv = { g_plMid - 1, pr.top + 1, g_plMid + 1, pr.bottom - 1 };   // division entre columnas
            fillRect(dc, dv, RGB(205, 209, 215));
        }
    }
}

static void ensureBackBuffer(HDC ref, int W, int H) {
    if (g_bbDC && W <= g_bbW && H <= g_bbH) return;
    if (g_bbDC) { SelectObject(g_bbDC, g_bbOld); DeleteObject(g_bbBmp); DeleteDC(g_bbDC); }
    g_bbW = std::max(W, 1); g_bbH = std::max(H, 1);
    g_bbDC = CreateCompatibleDC(ref);
    g_bbBmp = CreateCompatibleBitmap(ref, g_bbW, g_bbH);
    g_bbOld = (HBITMAP)SelectObject(g_bbDC, g_bbBmp);
}

// ---- teclas multimedia del teclado (siguiente, anterior, reproducir/pausa, detener)
// Se registran como atajos globales solo mientras hay algo sonando o en pausa: asi funcionan aunque MiauPlayer este en
// segundo plano, y cuando esta detenido las teclas vuelven a otras aplicaciones (navegador, Spotify...).
#ifndef MOD_NOREPEAT
#define MOD_NOREPEAT 0x4000
#endif
enum { HK_PLAYPAUSE = 0x4D41, HK_NEXT, HK_PREV, HK_STOP };
static bool g_mediaKeysOn = false;
static void updateMediaKeys() {
    const bool want = g_state != ST_STOPPED;
    if (want == g_mediaKeysOn) return;
    g_mediaKeysOn = want;
    if (want) {
        RegisterHotKey(g_hwnd, HK_PLAYPAUSE, MOD_NOREPEAT, VK_MEDIA_PLAY_PAUSE);   // si otra app ya la tiene, no se registra
        RegisterHotKey(g_hwnd, HK_NEXT, MOD_NOREPEAT, VK_MEDIA_NEXT_TRACK);
        RegisterHotKey(g_hwnd, HK_PREV, MOD_NOREPEAT, VK_MEDIA_PREV_TRACK);
        RegisterHotKey(g_hwnd, HK_STOP, MOD_NOREPEAT, VK_MEDIA_STOP);
    } else {
        UnregisterHotKey(g_hwnd, HK_PLAYPAUSE);
        UnregisterHotKey(g_hwnd, HK_NEXT);
        UnregisterHotKey(g_hwnd, HK_PREV);
        UnregisterHotKey(g_hwnd, HK_STOP);
    }
}

static void onTimer() {
    updateMediaKeys();
    // texto de tiempo
    double dur = g_duration;
    double pos = g_dragging ? g_dragFrac * dur : g_posSec.load();
    std::wstring t = g_live ? fmtTime(pos) + TX(L"   \u25CF EN VIVO") : fmtTime(pos) + L" / " + (dur > 0 ? fmtTime(dur) : L"--:--");
    const wchar_t* st = g_state == ST_PLAYING ? TX(L"  [Reproduciendo]") : g_state == ST_PAUSED ? TX(L"  [Pausa]") : TX(L"  [Detenido]");
    t += st;
    if (!g_status.empty()) t += L"   " + g_status;
    wchar_t cur[512];
    GetWindowText(g_time, cur, 512);
    if (t != cur) SetWindowText(g_time, t.c_str());

    { static int lastSt = -1; if (g_state != lastSt) { lastSt = g_state; InvalidateRect(GetDlgItem(g_hwnd, ID_PLAY), nullptr, FALSE); } }

    // medidor de pico
    float pn = g_peakNow.exchange(0.f);
    g_peakHold = std::max(pn, g_peakHold * 0.92f);
    wchar_t b[160];
    if (!g_peakOn.load()) b[0] = 0;
    else if (g_peakHold > 0.0001f) {
        float db = 20.f * log10f(g_peakHold);
        if (g_peakHold > 1.0f) swprintf(b, 160, g_limiter.load() ? TX(L"Pico: %+.1f dBFS  - SATURA (el limitador suaviza)") : TX(L"Pico: %+.1f dBFS  - SATURA"), db);
        else swprintf(b, 160, TX(L"Pico: %+.1f dBFS"), db);
    } else swprintf(b, 160, TX(L"Pico: -inf dBFS"));
    wchar_t old[160];
    GetWindowText(g_peakTxt, old, 160);
    if (wcscmp(b, old) != 0) SetWindowText(g_peakTxt, b);

    InvalidateRect(g_hwnd, &g_waveRect, FALSE);

    if (g_fullscreen) fsTick();

    // pantalla completa sin video por unos segundos (p. ej. siguiente pista solo audio): salir
    static int noVideoTicks = 0;
    if (g_fullscreen && !g_hasVideo) { if (++noVideoTicks > 75) { noVideoTicks = 0; toggleFullscreen(); } }
    else noVideoTicks = 0;
}

// ------------------------------------------------------------ ventana "Acerca de"
#define APP_VERSION L"1.7.9"
static const wchar_t* kAboutClass = L"MiauAbout";
static const wchar_t* kSiteUrl = L"https://ia21.cc";
static const wchar_t* kKofiUrl = L"https://ko-fi.com/miauapps";
static const int AB_W = 500, AB_H = 498;

static struct {
    bool done = false, hoverSite = false, hoverClose = false, hoverCopy = false, hoverKofi = false, copied = false;
    HFONT fTitle = nullptr, fLink = nullptr;
} g_ab;

static std::wstring asciiToWide(const char* s) {
    if (!s) return L"";
    int n = MultiByteToWideChar(CP_UTF8, 0, s, -1, nullptr, 0);
    std::wstring w(n > 0 ? n - 1 : 0, 0);
    if (n > 1) MultiByteToWideChar(CP_UTF8, 0, s, -1, &w[0], n);
    return w;
}

static std::wstring buildDateText() {
    char mon[4] = "";
    int d = 0, y = 0;
    sscanf(__DATE__, "%3s %d %d", mon, &d, &y);
    static const char* ms = "JanFebMarAprMayJunJulAugSepOctNovDec";
    int mo = 0;
    for (int i = 0; i < 12; i++) if (strncmp(mon, ms + 3 * i, 3) == 0) mo = i + 1;
    wchar_t b[32];
    swprintf(b, 32, L"%02d/%02d/%04d", d, mo, y);
    return b;
}

static std::wstring windowsText() {
    typedef LONG(WINAPI * RtlGetVersionFn)(OSVERSIONINFOEXW*);
    OSVERSIONINFOEXW vi = {};
    vi.dwOSVersionInfoSize = sizeof(vi);
    HMODULE nt = GetModuleHandleW(L"ntdll.dll");
    RtlGetVersionFn fn = nt ? (RtlGetVersionFn)(void*)GetProcAddress(nt, "RtlGetVersion") : nullptr;
    if (!fn || fn(&vi) != 0) return L"Windows";
    wchar_t b[96];
    const wchar_t* name = vi.dwMajorVersion >= 10 ? (vi.dwBuildNumber >= 22000 ? L"Windows 11" : L"Windows 10") : L"Windows";
    swprintf(b, 96, TX(L"%ls (compilaci\u00f3n %lu)"), name, (unsigned long)vi.dwBuildNumber);
    return b;
}

static std::wstring aboutInfoText() {
    return std::wstring(L"MiauPlayer ") + APP_VERSION + TX(L" (compilaci\u00f3n ") + buildDateText() + L")\r\n" +
           TX(L"Elaborado por IA21\r\n") + kSiteUrl + L"\r\n" +
           L"FFmpeg " + asciiToWide(av_version_info()) + L" (" + asciiToWide(avutil_license()) + L")\r\n" + windowsText();
}

static void aboutCopyInfo(HWND h) {
    const std::wstring t = aboutInfoText();
    if (!OpenClipboard(h)) return;
    EmptyClipboard();
    HGLOBAL mem = GlobalAlloc(GMEM_MOVEABLE, (t.size() + 1) * sizeof(wchar_t));
    if (mem) {
        wchar_t* d = (wchar_t*)GlobalLock(mem);
        memcpy(d, t.c_str(), (t.size() + 1) * sizeof(wchar_t));
        GlobalUnlock(mem);
        SetClipboardData(CF_UNICODETEXT, mem);
    }
    CloseClipboard();
    g_ab.copied = true;
    SetTimer(h, 1, 1800, nullptr);
    InvalidateRect(h, nullptr, FALSE);
}

// logo de Ko-fi incluido como recurso (RCDATA 2); se copia a un bitmap propio para no depender del stream
static Gdiplus::Bitmap* kofiLogo() {
    static Gdiplus::Bitmap* bmp = nullptr;
    if (bmp) return bmp;
    HRSRC r = FindResource(nullptr, MAKEINTRESOURCE(2), RT_RCDATA);
    if (!r) return nullptr;
    HGLOBAL hg = LoadResource(nullptr, r);
    const DWORD sz = SizeofResource(nullptr, r);
    const void* data = hg ? LockResource(hg) : nullptr;
    if (!data || !sz) return nullptr;
    HGLOBAL copy = GlobalAlloc(GMEM_MOVEABLE, sz);
    if (!copy) return nullptr;
    memcpy(GlobalLock(copy), data, sz);
    GlobalUnlock(copy);
    IStream* st = nullptr;
    if (FAILED(CreateStreamOnHGlobal(copy, TRUE, &st)) || !st) { GlobalFree(copy); return nullptr; }
    Gdiplus::Bitmap* src = Gdiplus::Bitmap::FromStream(st);
    if (src && src->GetLastStatus() == Gdiplus::Ok)
        bmp = src->Clone(0, 0, (INT)src->GetWidth(), (INT)src->GetHeight(), PixelFormat32bppARGB);
    delete src;
    st->Release();
    return bmp;
}

static void aboutRects(RECT& site, RECT& copy, RECT& close, RECT& kofi) {
    kofi = { 28, 340, AB_W - 28, 340 + 46 };
    site = { 172, 156 + 30 - 3, 172 + 130, 156 + 30 + 21 };
    copy = { 28, AB_H - 62, 28 + 190, AB_H - 62 + 34 };
    close = { AB_W - 28 - 120, AB_H - 62, AB_W - 28, AB_H - 62 + 34 };
}

static void aboutButton(Graphics& g, const RECT& r, bool hot) {
    GraphicsPath p;
    rrPath(p, r.left + 0.5f, r.top + 0.5f, (float)(r.right - r.left) - 1.f, (float)(r.bottom - r.top) - 1.f, 9.f);
    SolidBrush b(Col(hot ? RGB(240, 242, 245) : RGB(226, 229, 234)));
    g.FillPath(&b, &p);
}

static void paintAbout(HWND h) {
    PAINTSTRUCT ps;
    HDC hdc = BeginPaint(h, &ps);
    RECT rc;
    GetClientRect(h, &rc);
    const int W = rc.right, H = rc.bottom;
    HDC dc = CreateCompatibleDC(hdc);
    HBITMAP bmp = CreateCompatibleBitmap(hdc, W, H);
    HGDIOBJ old = SelectObject(dc, bmp);
    fillRect(dc, rc, C_BG);
    RECT band = { 0, 0, W, 118 };
    fillRect(dc, band, C_TITLE);
    HICON ic = (HICON)LoadImage(GetModuleHandle(nullptr), MAKEINTRESOURCE(1), IMAGE_ICON, 64, 64, 0);
    if (ic) { DrawIconEx(dc, 28, 27, ic, 64, 64, 0, nullptr, DI_NORMAL); DestroyIcon(ic); }
    SetBkMode(dc, TRANSPARENT);
    SelectObject(dc, g_ab.fTitle);
    SetTextColor(dc, RGB(255, 255, 255));
    RECT r1 = { 110, 22, W - 20, 64 };
    DrawText(dc, L"MiauPlayer", -1, &r1, DT_LEFT | DT_VCENTER | DT_SINGLELINE);
    SelectObject(dc, g_font);
    SetTextColor(dc, RGB(236, 98, 92));
    RECT r2 = { 112, 64, W - 20, 84 };
    const std::wstring ver = std::wstring(TX(L"Versi\u00f3n ")) + APP_VERSION + TX(L"  \u00b7  compilada el ") + buildDateText();
    DrawText(dc, ver.c_str(), -1, &r2, DT_LEFT | DT_VCENTER | DT_SINGLELINE);
    SetTextColor(dc, RGB(170, 176, 186));
    RECT r3 = { 112, 86, W - 20, 106 };
    DrawText(dc, TX(L"Reproductor de audio y video minimalista"), -1, &r3, DT_LEFT | DT_VCENTER | DT_SINGLELINE);

    struct Row { const wchar_t* k; std::wstring v; bool link; };
    const Row rows[] = {
        { TX(L"Elaborado por"), L"IA21", false },
        { TX(L"Sitio web"), L"ia21.cc", true },
        { TX(L"Motor multimedia"), L"FFmpeg " + asciiToWide(av_version_info()), false },
        { TX(L"Licencia de FFmpeg"), asciiToWide(avutil_license()), false },
        { TX(L"Formatos"), TX(L"Todos los que soporta FFmpeg, y rutas de red"), false },
        { TX(L"Sistema"), windowsText(), false },
    };
    int y = 156;
    for (const Row& rw : rows) {
        RECT lk = { 28, y, 168, y + 22 }, vl = { 172, y, W - 20, y + 22 };
        SetTextColor(dc, RGB(150, 156, 164));
        SelectObject(dc, g_font);
        DrawText(dc, rw.k, -1, &lk, DT_LEFT | DT_VCENTER | DT_SINGLELINE);
        if (rw.link) {
            SelectObject(dc, g_ab.hoverSite ? g_ab.fLink : g_font);
            SetTextColor(dc, g_ab.hoverSite ? RGB(255, 140, 134) : RGB(236, 98, 92));
        } else {
            SelectObject(dc, g_font);
            SetTextColor(dc, C_TEXT);
        }
        DrawText(dc, rw.v.c_str(), -1, &vl, DT_LEFT | DT_VCENTER | DT_SINGLELINE | DT_END_ELLIPSIS);
        y += 30;
    }
    RECT sep = { 28, AB_H - 100, W - 28, AB_H - 99 };
    fillRect(dc, sep, RGB(64, 69, 78));
    SelectObject(dc, g_font);
    SetTextColor(dc, RGB(130, 136, 146));
    RECT note = { 28, AB_H - 94, W - 28, AB_H - 70 };
    DrawText(dc, TX(L"Usa FFmpeg (ffmpeg.org). Se ofrece sin garant\u00eda de ning\u00fan tipo."), -1, &note, DT_LEFT | DT_VCENTER | DT_SINGLELINE);

    RECT site, copy, close, kofi;
    aboutRects(site, copy, close, kofi);
    {   // boton de apoyo: logo de Ko-fi + frase, enlazado a la cuenta de Ko-fi
        Graphics g(dc);
        g.SetSmoothingMode(Gdiplus::SmoothingModeAntiAlias);
        g.SetInterpolationMode(Gdiplus::InterpolationModeHighQualityBicubic);
        GraphicsPath kp;
        rrPath(kp, kofi.left + 0.5f, kofi.top + 0.5f, (float)(kofi.right - kofi.left) - 1.f, (float)(kofi.bottom - kofi.top) - 1.f, 12.f);
        SolidBrush kb(Col(g_ab.hoverKofi ? RGB(255, 243, 242) : RGB(255, 255, 255)));
        g.FillPath(&kb, &kp);
        Pen kpn(Col(RGB(255, 94, 91)), g_ab.hoverKofi ? 2.2f : 1.5f);
        g.DrawPath(&kpn, &kp);
        Gdiplus::Bitmap* logo = kofiLogo();
        const std::wstring ktxt = TX(L"Si te gust\u00f3 MiauPlayer, inv\u00edtame un caf\u00e9");
        SelectObject(dc, g_fontBold);
        SIZE ts = {};
        GetTextExtentPoint32W(dc, ktxt.c_str(), (int)ktxt.size(), &ts);
        const float lh = 26.f, lw = logo ? lh * logo->GetWidth() / (float)logo->GetHeight() : 0.f;
        const float gw = lw + (logo ? 14.f : 0.f) + (float)ts.cx;
        const float sx = kofi.left + ((kofi.right - kofi.left) - gw) / 2.f, cy = (kofi.top + kofi.bottom) / 2.f;
        if (logo) g.DrawImage(logo, Gdiplus::RectF(sx, cy - lh / 2.f, lw, lh));
        g.Flush();
        SetTextColor(dc, RGB(43, 47, 54));
        RECT kt = { (LONG)(sx + lw + (logo ? 14.f : 0.f)), kofi.top, kofi.right - 8, kofi.bottom };
        DrawText(dc, ktxt.c_str(), -1, &kt, DT_LEFT | DT_VCENTER | DT_SINGLELINE);
    }
    {
        Graphics g(dc);
        g.SetSmoothingMode(Gdiplus::SmoothingModeAntiAlias);
        aboutButton(g, copy, g_ab.hoverCopy);
        aboutButton(g, close, g_ab.hoverClose);
        g.Flush();
    }
    SetTextColor(dc, RGB(43, 47, 54));
    SelectObject(dc, g_fontBold);
    DrawText(dc, g_ab.copied ? TX(L"\u00a1Copiado!") : TX(L"Copiar informaci\u00f3n"), -1, &copy, DT_CENTER | DT_VCENTER | DT_SINGLELINE);
    DrawText(dc, TX(L"Cerrar"), -1, &close, DT_CENTER | DT_VCENTER | DT_SINGLELINE);

    BitBlt(hdc, 0, 0, W, H, dc, 0, 0, SRCCOPY);
    SelectObject(dc, old);
    DeleteObject(bmp);
    DeleteDC(dc);
    EndPaint(h, &ps);
}

static LRESULT CALLBACK AboutProc(HWND h, UINT m, WPARAM w, LPARAM l) {
    RECT site, copy, close, kofi;
    aboutRects(site, copy, close, kofi);
    switch (m) {
    case WM_ERASEBKGND: return 1;
    case WM_PAINT: paintAbout(h); return 0;
    case WM_MOUSEMOVE: {
        POINT p = { (short)LOWORD(l), (short)HIWORD(l) };
        const bool hs = PtInRect(&site, p) != 0, hc = PtInRect(&copy, p) != 0, hx = PtInRect(&close, p) != 0, hk = PtInRect(&kofi, p) != 0;
        if (hs != g_ab.hoverSite || hc != g_ab.hoverCopy || hx != g_ab.hoverClose || hk != g_ab.hoverKofi) {
            g_ab.hoverSite = hs; g_ab.hoverCopy = hc; g_ab.hoverClose = hx; g_ab.hoverKofi = hk;
            InvalidateRect(h, nullptr, FALSE);
        }
        TRACKMOUSEEVENT te = { sizeof(te), TME_LEAVE, h, 0 };
        TrackMouseEvent(&te);
        return 0;
    }
    case WM_MOUSELEAVE:
        g_ab.hoverSite = g_ab.hoverCopy = g_ab.hoverClose = g_ab.hoverKofi = false;
        InvalidateRect(h, nullptr, FALSE);
        return 0;
    case WM_SETCURSOR:
        if (g_ab.hoverSite || g_ab.hoverCopy || g_ab.hoverClose || g_ab.hoverKofi) { SetCursor(LoadCursor(nullptr, IDC_HAND)); return TRUE; }
        break;
    case WM_LBUTTONUP: {
        POINT p = { (short)LOWORD(l), (short)HIWORD(l) };
        if (PtInRect(&site, p)) ShellExecuteW(h, L"open", kSiteUrl, nullptr, nullptr, SW_SHOWNORMAL);
        else if (PtInRect(&kofi, p)) ShellExecuteW(h, L"open", kKofiUrl, nullptr, nullptr, SW_SHOWNORMAL);
        else if (PtInRect(&copy, p)) aboutCopyInfo(h);
        else if (PtInRect(&close, p)) g_ab.done = true;
        return 0;
    }
    case WM_KEYDOWN:
        if (w == VK_ESCAPE || w == VK_RETURN) { g_ab.done = true; return 0; }
        if (w == 'C' && (GetKeyState(VK_CONTROL) & 0x8000)) { aboutCopyInfo(h); return 0; }
        break;
    case WM_TIMER:
        if (w == 1) { KillTimer(h, 1); g_ab.copied = false; InvalidateRect(h, nullptr, FALSE); }
        return 0;
    case WM_CLOSE: g_ab.done = true; return 0;
    }
    return DefWindowProc(h, m, w, l);
}

static void showAbout() {
    static bool registered = false;
    HINSTANCE hi = GetModuleHandle(nullptr);
    if (!registered) {
        WNDCLASSEX wc = { sizeof(wc) };
        wc.lpfnWndProc = AboutProc;
        wc.hInstance = hi;
        wc.hCursor = LoadCursor(nullptr, IDC_ARROW);
        wc.lpszClassName = kAboutClass;
        wc.hIcon = g_iconSmall;
        RegisterClassEx(&wc);
        registered = true;
    }
    HWND owner = g_hwnd;
    const DWORD st = WS_POPUP | WS_CAPTION | WS_SYSMENU;
    RECT r = { 0, 0, AB_W, AB_H };
    AdjustWindowRectEx(&r, st, FALSE, WS_EX_DLGMODALFRAME);
    const int w = r.right - r.left, hgt = r.bottom - r.top;
    RECT pr, wa;
    GetWindowRect(owner, &pr);
    SystemParametersInfo(SPI_GETWORKAREA, 0, &wa, 0);
    int x = pr.left + ((pr.right - pr.left) - w) / 2, y = pr.top + ((pr.bottom - pr.top) - hgt) / 2;
    x = std::max((int)wa.left, std::min(x, (int)wa.right - w));
    y = std::max((int)wa.top, std::min(y, (int)wa.bottom - hgt));
    g_ab = {};
    g_ab.fTitle = CreateFont(-34, 0, 0, 0, FW_BOLD, FALSE, FALSE, FALSE, DEFAULT_CHARSET, 0, 0, CLEARTYPE_QUALITY, 0, L"Segoe UI");
    g_ab.fLink = CreateFont(-15, 0, 0, 0, FW_NORMAL, FALSE, TRUE, FALSE, DEFAULT_CHARSET, 0, 0, CLEARTYPE_QUALITY, 0, L"Segoe UI");
    EnableWindow(owner, FALSE);
    HWND d = CreateWindowEx(WS_EX_DLGMODALFRAME, kAboutClass, TX(L"Acerca de MiauPlayer"), st, x, y, w, hgt, owner, nullptr, hi, nullptr);
    if (d) {
        BOOL dark = TRUE;
        DwmSetWindowAttribute(d, 20, &dark, sizeof(dark));
        ShowWindow(d, SW_SHOW);
        UpdateWindow(d);
        SetFocus(d);
        MSG msg;
        while (!g_ab.done && GetMessage(&msg, nullptr, 0, 0) > 0) {
            TranslateMessage(&msg);
            DispatchMessage(&msg);
        }
    }
    EnableWindow(owner, TRUE);
    if (d) DestroyWindow(d);
    SetForegroundWindow(owner);
    DeleteObject(g_ab.fTitle);
    DeleteObject(g_ab.fLink);
    g_ab = {};
}

// ---- clic derecho sobre una pista: abrir su carpeta e informacion tecnica
static void openTrackLocation(int idx) {
    if (idx < 0 || idx >= (int)g_playlist.size() || isUrl(g_playlist[idx])) return;
    const std::wstring& p = g_playlist[idx];
    if (GetFileAttributesW(p.c_str()) == INVALID_FILE_ATTRIBUTES) {
        MessageBoxW(g_hwnd, TX(L"El archivo ya no existe en esa ubicaci\u00f3n."), L"MiauPlayer", MB_OK);
        return;
    }
    // API del shell: abre la carpeta y selecciona el archivo tambien cuando el Explorador aun no esta abierto
    CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED);
    bool ok = false;
    if (PIDLIST_ABSOLUTE pidl = ILCreateFromPathW(p.c_str())) {
        ok = SUCCEEDED(SHOpenFolderAndSelectItems(pidl, 0, nullptr, 0));
        ILFree(pidl);
    }
    if (!ok) {
        const std::wstring args = L"/select,\"" + p + L"\"";
        ShellExecuteW(nullptr, L"open", L"explorer.exe", args.c_str(), nullptr, SW_SHOWNORMAL);
    }
}

static std::wstring upperAscii(const char* s) {
    std::wstring r = asciiToWide(s);
    for (auto& c : r) c = (wchar_t)towupper(c);
    return r;
}

// Tipo de archivo, bitrate y frecuencia de muestreo, leidos con FFmpeg (sin reproducir nada)
static void showTrackInfo(int idx) {
    if (idx < 0 || idx >= (int)g_playlist.size() || isUrl(g_playlist[idx])) return;
    const std::wstring path = g_playlist[idx];
    const wchar_t* title = TX(L"Informaci\u00f3n de la pista");
    AVFormatContext* fmt = nullptr;
    if (openInput(&fmt, utf8(path)) < 0 || !fmt) {
        MessageBoxW(g_hwnd, TX(L"No se pudo leer la informaci\u00f3n de este archivo."), title, MB_OK);
        return;
    }
    avformat_find_stream_info(fmt, nullptr);
    const int ai = av_find_best_stream(fmt, AVMEDIA_TYPE_AUDIO, -1, -1, nullptr, 0);
    int vi = av_find_best_stream(fmt, AVMEDIA_TYPE_VIDEO, -1, -1, nullptr, 0);
    if (vi >= 0 && (fmt->streams[vi]->disposition & AV_DISPOSITION_ATTACHED_PIC)) vi = -1;   // la portada de un mp3 no es video
    const AVCodecParameters* ap = ai >= 0 ? fmt->streams[ai]->codecpar : nullptr;
    const AVCodecParameters* vp = vi >= 0 ? fmt->streams[vi]->codecpar : nullptr;

    // tipo: extension del archivo y codecs
    std::wstring ext;
    const size_t dot = path.find_last_of(L'.');
    if (dot != std::wstring::npos && path.size() - dot <= 6) { ext = path.substr(dot + 1); for (auto& c : ext) c = (wchar_t)towupper(c); }
    std::wstring type = ext.empty() ? upperAscii(fmt->iformat ? fmt->iformat->name : "") : ext;
    std::wstring codecs;
    if (vp) codecs += std::wstring(TX(L"video")) + L": " + upperAscii(avcodec_get_name(vp->codec_id));
    if (ap) codecs += std::wstring(codecs.empty() ? L"" : L", ") + TX(L"audio") + L": " + upperAscii(avcodec_get_name(ap->codec_id));
    if (!codecs.empty()) type += L"  (" + codecs + L")";

    // bitrate: el del audio; si no se conoce, el del contenedor; si tampoco, tamano / duracion
    long long br = ap ? ap->bit_rate : 0;
    long long total = fmt->bit_rate;
    if (total <= 0 && fmt->duration > 0) {
        WIN32_FILE_ATTRIBUTE_DATA fa;
        if (GetFileAttributesExW(path.c_str(), GetFileExInfoStandard, &fa)) {
            const long long size = ((long long)fa.nFileSizeHigh << 32) | fa.nFileSizeLow;
            total = (long long)((double)size * 8.0 / ((double)fmt->duration / AV_TIME_BASE));
        }
    }
    if (br <= 0 && !vp) br = total;
    std::wstring brText = br > 0 ? std::to_wstring((br + 500) / 1000) + L" kbps" : std::wstring(TX(L"desconocido"));
    if (vp && total > 0) brText += std::wstring(L"  (") + TX(L"total del archivo") + L": " + std::to_wstring((total + 500) / 1000) + L" kbps)";

    const std::wstring hz = ap && ap->sample_rate > 0 ? std::to_wstring(ap->sample_rate) + L" Hz" : std::wstring(TX(L"desconocido"));

    // profundidad de bits: la del original si el formato la declara; en formatos con perdida no tiene sentido
    std::wstring depth = TX(L"desconocido");
    if (ap) {
        int bits = ap->bits_per_raw_sample > 0 ? ap->bits_per_raw_sample : ap->bits_per_coded_sample;
        const AVCodecDescriptor* cd = avcodec_descriptor_get(ap->codec_id);
        const bool lossy = cd && (cd->props & AV_CODEC_PROP_LOSSY) && !(cd->props & AV_CODEC_PROP_LOSSLESS);
        if (bits > 0) depth = std::to_wstring(bits) + L" bits";
        else if (lossy) depth = TX(L"no aplica (formato con p\u00e9rdida)");
        else {
            const AVSampleFormat sf = (AVSampleFormat)ap->format;
            const int b = sf == AV_SAMPLE_FMT_NONE ? 0 : av_get_bytes_per_sample(sf) * 8;
            if (b > 0) depth = std::to_wstring(b) + ((sf == AV_SAMPLE_FMT_FLT || sf == AV_SAMPLE_FMT_FLTP || sf == AV_SAMPLE_FMT_DBL || sf == AV_SAMPLE_FMT_DBLP) ? std::wstring(L" ") + std::wstring(TX(L"bits (flotante)")) : std::wstring(L" bits"));
        }
        if (bits > 0 && (ap->format == AV_SAMPLE_FMT_FLT || ap->format == AV_SAMPLE_FMT_FLTP) && ap->codec_id >= AV_CODEC_ID_PCM_F32BE && ap->codec_id <= AV_CODEC_ID_PCM_F64LE)
            depth = std::to_wstring(bits) + L" " + TX(L"bits (flotante)");
    }
    avformat_close_input(&fmt);

    const std::wstring text = std::wstring(TX(L"Archivo")) + L": " + baseName(path) + L"\n\n" +
                              TX(L"Tipo") + L": " + type + L"\n" +
                              L"Bitrate: " + brText + L"\n" +
                              TX(L"Frecuencia") + L": " + hz + L"\n" +
                              TX(L"Profundidad de bits") + L": " + depth;
    MessageBoxW(g_hwnd, text.c_str(), title, MB_OK);   // sin icono: asi Windows no reproduce sonido de alerta
}

static LRESULT CALLBACK ListProc(HWND h, UINT m, WPARAM w, LPARAM l, UINT_PTR, DWORD_PTR) {
    {   // arrastrar para reordenar (un clic simple conserva el comportamiento de siempre)
        const int d = dragHandle(h, m, w, l, movePlaylistItem);
        if (d == DRG_BUSY || d == DRG_DONE) return 0;
    }
    if (m == WM_KEYDOWN && w == VK_DELETE) {   // Supr = quitar la cancion seleccionada
        removeTrack((int)SendMessage(h, LB_GETCURSEL, 0, 0));
        return 0;
    }
    if (m == WM_RBUTTONUP) {                   // clic derecho: quitar de la lista / limpiar toda la lista
        if (g_playlist.empty()) return 0;
        const DWORD it = (DWORD)SendMessage(h, LB_ITEMFROMPOINT, 0, l);
        const int idx = LOWORD(it);
        const bool onItem = HIWORD(it) == 0 && idx >= 0 && idx < (int)g_playlist.size();
        if (onItem) SendMessage(h, LB_SETCURSEL, idx, 0);
        HMENU mn = CreatePopupMenu();
        if (onItem) {
            const UINT noNet = isUrl(g_playlist[idx]) ? MF_GRAYED : 0;   // rutas de red: no hay archivo local
            AppendMenu(mn, MF_STRING, 1, TX(L"Reproducir"));
            AppendMenu(mn, MF_STRING | noNet, 4, TX(L"Abrir ubicaci\u00f3n del archivo"));
            AppendMenu(mn, MF_STRING | noNet, 5, TX(L"Informaci\u00f3n de la pista"));
            AppendMenu(mn, MF_SEPARATOR, 0, nullptr);
            AppendMenu(mn, MF_STRING, 2, TX(L"Quitar de la lista"));
            AppendMenu(mn, MF_SEPARATOR, 0, nullptr);
        }
        AppendMenu(mn, MF_STRING, 3, TX(L"Limpiar toda la lista"));
        POINT p = { (short)LOWORD(l), (short)HIWORD(l) };
        ClientToScreen(h, &p);
        SetForegroundWindow(g_hwnd);
        const int cmd = TrackPopupMenu(mn, TPM_RETURNCMD | TPM_RIGHTBUTTON, p.x, p.y, 0, g_hwnd, nullptr);
        DestroyMenu(mn);
        if (cmd == 1) { g_listFloat = false; playIndex(idx); }
        else if (cmd == 2) removeTrack(idx);
        else if (cmd == 3) clearPlaylist();
        else if (cmd == 4) openTrackLocation(idx);
        else if (cmd == 5) showTrackInfo(idx);
        return 0;
    }
    if (m == WM_KEYDOWN && handleKey(w, true)) return 0;
    if (m == WM_CHAR && (w == VK_SPACE || w == 'm' || w == 'M' || w == 'f' || w == 'F')) return 0;
    if (m == WM_ERASEBKGND) {   // zona vacia bajo el ultimo elemento
        RECT rc;
        GetClientRect(h, &rc);
        fillRect((HDC)w, rc, C_PANEL);
        return 1;
    }
    return DefSubclassProc(h, m, w, l);
}

static LRESULT CALLBACK PlProc(HWND h, UINT m, WPARAM w, LPARAM l, UINT_PTR, DWORD_PTR) {
    if (m == WM_ERASEBKGND) {
        RECT rc;
        GetClientRect(h, &rc);
        fillRect((HDC)w, rc, C_PANEL);
        return 1;
    }
    {   // arrastrar para reordenar las listas guardadas; el clic sin arrastrar sigue cargando la lista
        const int d = dragHandle(h, m, w, l, moveSavedItem);
        if (d == DRG_BUSY || d == DRG_DONE) return 0;
    }
    if (m == WM_LBUTTONUP) {
        LRESULT r = DefSubclassProc(h, m, w, l);
        const DWORD it = (DWORD)SendMessage(h, LB_ITEMFROMPOINT, 0, l);
        const int idx = LOWORD(it);
        if (HIWORD(it) == 0 && idx >= 0 && idx < (int)g_savedNames.size()) {
            loadPlaylist(g_savedNames[idx]);   // clic => cargar en la columna izquierda y reproducir la primera
        }
        return r;
    }
    if (m == WM_RBUTTONUP) {
        const DWORD it = (DWORD)SendMessage(h, LB_ITEMFROMPOINT, 0, l);
        const int idx = LOWORD(it);
        if (HIWORD(it) == 0 && idx >= 0 && idx < (int)g_savedNames.size()) {
            SendMessage(h, LB_SETCURSEL, idx, 0);
            POINT p = { (short)LOWORD(l), (short)HIWORD(l) };
            ClientToScreen(h, &p);
            showPlMenu(idx, p);
        }
        return 0;
    }
    if (m == WM_KEYDOWN) {
        const int idx = (int)SendMessage(h, LB_GETCURSEL, 0, 0);
        if (w == VK_RETURN && idx >= 0 && idx < (int)g_savedNames.size()) { loadPlaylist(g_savedNames[idx]); return 0; }
        if (w == VK_DELETE && idx >= 0 && idx < (int)g_savedNames.size()) { deletePlaylist(g_savedNames[idx]); return 0; }
        if (handleKey(w, true)) return 0;
    }
    if (m == WM_CHAR && (w == VK_SPACE || w == 'm' || w == 'M' || w == 'f' || w == 'F')) return 0;
    if (m == WM_PAINT) {
        LRESULT r = DefSubclassProc(h, m, w, l);
        if (SendMessage(h, LB_GETCOUNT, 0, 0) == 0) {   // columna vacia: pista
            HDC dc = GetDC(h);
            RECT rc;
            GetClientRect(h, &rc);
            rc.left += 16; rc.right -= 16; rc.top += 22;
            SelectObject(dc, g_font);
            SetBkMode(dc, TRANSPARENT);
            SetTextColor(dc, RGB(120, 126, 136));
            DrawText(dc, TX(L"A\u00fan no hay playlists guardadas.\nPulsa el bot\u00f3n de guardar para crear una con la lista actual."),
                     -1, &rc, DT_CENTER | DT_WORDBREAK | DT_NOPREFIX);
            ReleaseDC(h, dc);
        }
        return r;
    }
    return DefSubclassProc(h, m, w, l);
}

static bool isSlider(HWND h) { return h == g_gain || h == g_vol || h == g_spd || h == g_eq[0] || h == g_eq[1] || h == g_eq[2]; }

static void applyWindowChrome(HWND h) {
    BOOL dark = TRUE;
    DwmSetWindowAttribute(h, 20, &dark, sizeof(dark));            // DWMWA_USE_IMMERSIVE_DARK_MODE
    int corner = 2;                                               // DWMWCP_ROUND
    DwmSetWindowAttribute(h, 33, &corner, sizeof(corner));        // DWMWA_WINDOW_CORNER_PREFERENCE
    COLORREF border = RGB(70, 75, 84);
    DwmSetWindowAttribute(h, 34, &border, sizeof(border));        // DWMWA_BORDER_COLOR
    MARGINS mg = { 1, 1, 1, 1 };                                  // conserva la sombra sin marco clasico
    DwmExtendFrameIntoClientArea(h, &mg);
}

static LRESULT CALLBACK WndProc(HWND h, UINT m, WPARAM w, LPARAM l) {
    switch (m) {
    case WM_CREATE: {
        g_hwnd = h;
        int x = 10;
        mkBtn(h, TX(L"Abrir"), ID_OPEN, x, 90);
        mkBtn(h, TX(L"Carpeta"), ID_FOLDER, x, 106);
        mkBtn(h, L"|<", ID_PREV, x, 52);
        mkBtn(h, TX(L"Play/Pausa"), ID_PLAY, x, 116);
        mkBtn(h, TX(L"Stop"), ID_STOP, x, 90);
        mkBtn(h, L">|", ID_NEXT, x, 52);
        g_time = mkStatic(h, ID_TIME);
        g_volTxt = mkStatic(h, ID_VOLTXT);
        g_vol = mkSlider(h, ID_VOL, 0, 100, 100, 10);
        g_mute = mkBtn(h, TX(L"Silencio"), ID_MUTE, 0, 98);
        g_gainTxt = mkStatic(h, ID_GAINTXT);
        g_gain = mkSlider(h, ID_GAIN, -24, 20, 0, 4);        // medios dB: -12..+10 dB
        mkBtn(h, L"0 dB", ID_RESET, 0, 98);
        g_limit = mkBtn(h, TX(L"Limitador suave"), ID_LIMIT, 0, 190);
        g_peakChk = mkBtn(h, TX(L"Pico"), ID_PEAKON, 0, 76);
        mkBtn(h, TX(L"Lista"), ID_LISTBTN, 0, 52);
        mkBtn(h, TX(L"Salida"), ID_OUTBTN, 0, 52);
        mkBtn(h, TX(L"Acerca de"), ID_ABOUT, 0, 38);
        mkBtn(h, TX(L"Capturar fotograma"), ID_SNAP, 0, 34);
        mkBtn(h, TX(L"Abrir carpeta de capturas"), ID_SNAPDIR, 0, 34);
        mkBtn(h, TX(L"Configuración"), ID_SNAPOPT, 0, 34);
        mkBtn(h, TX(L"Abrir ruta de red"), ID_NETURL, 0, 38);
        g_plImp = mkBtn(h, TX(L"Importar"), ID_PLIMP, 0, 28);
        g_plExp = mkBtn(h, TX(L"Exportar"), ID_PLEXP, 0, 28);
        g_plSave = mkBtn(h, TX(L"Guardar playlist"), ID_PLSAVE, 0, 28);
        mkBtn(h, L"1.00x", ID_SPEED, 0, 80);
        mkBtn(h, L"1x", ID_SPDRESET, 0, 56);
        g_spd = mkSlider(h, ID_SPD, -100, 100, 0, 50);   // escala logaritmica: 3^(pos/100) => 0.33x .. 3x
        ShowWindow(g_spd, SW_HIDE);
        ShowWindow(GetDlgItem(h, ID_SPDRESET), SW_HIDE);
        // ayudas emergentes para los botones que solo tienen icono
        {
            HWND tip = CreateWindowEx(WS_EX_TOPMOST, TOOLTIPS_CLASS, nullptr, WS_POPUP | TTS_ALWAYSTIP, 0, 0, 0, 0, h, nullptr, nullptr, nullptr);
            struct TipDef { int id; const wchar_t* t; } tt[] = { { ID_OPEN, TX(L"Abrir archivos") }, { ID_STOP, TX(L"Detener") }, { ID_PREV, TX(L"Anterior") }, { ID_NEXT, TX(L"Siguiente") },
                { ID_PLSAVE, TX(L"Guardar la lista actual como playlist") }, { ID_SNAP, TX(L"Capturar fotograma") }, { ID_SNAPDIR, TX(L"Abrir carpeta de capturas") }, { ID_SNAPOPT, TX(L"Configuración") }, { ID_ABOUT, TX(L"Acerca de MiauPlayer") }, { ID_NETURL, TX(L"Abrir ruta de red (RTSP, HTTP\u2026)") }, { ID_PLIMP, TX(L"Importar playlist (.txt)") }, { ID_PLEXP, TX(L"Exportar la lista actual a .txt") }, { ID_FOLDER, TX(L"Abrir carpeta") }, { ID_PLAY, TX(L"Reproducir / Pausa") },
                { ID_LISTBTN, TX(L"Lista de reproducci\u00f3n") }, { ID_OUTBTN, TX(L"Salida de audio") }, { ID_SPEED, TX(L"Velocidad de reproducci\u00f3n") } };
            if (tip)
                for (auto& e : tt) {
                    TOOLINFO ti = { sizeof(ti) };
                    ti.uFlags = TTF_IDISHWND | TTF_SUBCLASS;
                    ti.hwnd = h;
                    ti.uId = (UINT_PTR)GetDlgItem(h, e.id);
                    ti.lpszText = (LPWSTR)e.t;
                    SendMessage(tip, TTM_ADDTOOL, 0, (LPARAM)&ti);
                }
        }
        g_peakTxt = mkStatic(h, ID_PEAKTXT);
        int ids[3] = { ID_BASS, ID_MID, ID_TREBLE };
        for (int i = 0; i < 3; i++) {
            g_eqTxt[i] = mkStatic(h, ID_EQTXT0 + i);
            g_eq[i] = mkSlider(h, ids[i], -12, 12, 0, 3);
            g_eqDb[i] = 0.f;
        }
        g_list = CreateWindowEx(0, L"LISTBOX", L"", WS_CHILD | WS_VISIBLE | WS_VSCROLL | WS_TABSTOP | LBS_NOTIFY |
                                LBS_NOINTEGRALHEIGHT | LBS_OWNERDRAWFIXED | LBS_HASSTRINGS,
                                0, 0, 100, 100, h, (HMENU)ID_LIST, nullptr, nullptr);
        SetWindowSubclass(g_list, ListProc, 1, 0);
        g_plList = CreateWindowEx(0, L"LISTBOX", L"", WS_CHILD | WS_VISIBLE | WS_VSCROLL | WS_TABSTOP | LBS_NOTIFY |
                                  LBS_NOINTEGRALHEIGHT | LBS_OWNERDRAWFIXED | LBS_HASSTRINGS,
                                  0, 0, 100, 100, h, (HMENU)ID_PLLIST, nullptr, nullptr);
        SetWindowSubclass(g_plList, PlProc, 1, 0);
        EnumChildWindows(h, [](HWND c, LPARAM lp) -> BOOL { SendMessage(c, WM_SETFONT, lp, TRUE); return TRUE; }, (LPARAM)g_font);
        refreshSavedLists();   // lee la carpeta "playlists" y llena la columna derecha
        updateLabels();
        DragAcceptFiles(h, TRUE);
        applyWindowChrome(h);
        SetWindowPos(h, nullptr, 0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE | SWP_NOZORDER | SWP_FRAMECHANGED);
        SetTimer(h, 1, 40, nullptr);
        return 0;
    }
    case WM_NCCALCSIZE:
        if (w) {   // sin marco clasico: el area cliente ocupa toda la ventana
            if (IsZoomed(h) && !g_fullscreen) {
                NCCALCSIZE_PARAMS* p = (NCCALCSIZE_PARAMS*)l;
                MONITORINFO mi = { sizeof(mi) };
                if (GetMonitorInfo(MonitorFromRect(&p->rgrc[0], MONITOR_DEFAULTTONEAREST), &mi)) p->rgrc[0] = mi.rcWork;
                APPBARDATA abd = { sizeof(abd) };
                if (SHAppBarMessage(ABM_GETSTATE, &abd) & ABS_AUTOHIDE) p->rgrc[0].bottom -= 1;   // deja asomar la barra de tareas
            }
            return 0;
        }
        break;
    case WM_NCHITTEST: {
        if (g_fullscreen) return HTCLIENT;
        POINT pt = { GET_X_LPARAM(l), GET_Y_LPARAM(l) };
        ScreenToClient(h, &pt);
        RECT rc;
        GetClientRect(h, &rc);
        if (!IsZoomed(h)) {
            const int B = 6;
            bool L = pt.x < B, R = pt.x >= rc.right - B, T = pt.y < B, Bt = pt.y >= rc.bottom - B;
            if (T && L) return HTTOPLEFT;
            if (T && R) return HTTOPRIGHT;
            if (Bt && L) return HTBOTTOMLEFT;
            if (Bt && R) return HTBOTTOMRIGHT;
            if (L) return HTLEFT;
            if (R) return HTRIGHT;
            if (T) return HTTOP;
            if (Bt) return HTBOTTOM;
        }
        if (pt.y < CAP_H) return capButtonAt(pt.x, pt.y, rc.right) >= 0 ? HTCLIENT : HTCAPTION;
        return HTCLIENT;
    }
    case WM_NCACTIVATE:
        return DefWindowProc(h, m, w, -1);   // evita que Windows repinte un marco clasico
    case WM_SETTEXT: {
        LONG st = GetWindowLong(h, GWL_STYLE);
        SetWindowLong(h, GWL_STYLE, st & ~WS_VISIBLE);   // WM_SETTEXT no debe pintar la barra clasica
        LRESULT r = DefWindowProc(h, m, w, l);
        SetWindowLong(h, GWL_STYLE, st);
        return r;
    }
    case WM_SIZE:
        layout(LOWORD(l), HIWORD(l));
        InvalidateRect(h, nullptr, FALSE);
        return 0;
    case WM_ENTERSIZEMOVE:
        g_liveSize = true;
        // mientras se estira, Windows pinta la ventana y todos sus controles en un solo bufer (sin cuadros blancos)
        SetWindowLong(h, GWL_EXSTYLE, GetWindowLong(h, GWL_EXSTYLE) | WS_EX_COMPOSITED);
        return 0;
    case WM_EXITSIZEMOVE:
        g_liveSize = false;
        SetWindowLong(h, GWL_EXSTYLE, GetWindowLong(h, GWL_EXSTYLE) & ~WS_EX_COMPOSITED);
        RedrawWindow(h, nullptr, nullptr, RDW_INVALIDATE | RDW_ALLCHILDREN | RDW_UPDATENOW);
        return 0;
    case WM_CTLCOLORBTN:   // fondo oscuro para los botones propios (por defecto Windows usa el color claro de boton)
        return (LRESULT)g_brBg;
    case WM_GETMINMAXINFO: {
        MINMAXINFO* mi = (MINMAXINFO*)l;
        mi->ptMinTrackSize.x = 600;
        mi->ptMinTrackSize.y = 600;
        return 0;
    }
    case WM_ERASEBKGND:
        return 1;
    case WM_CTLCOLORSTATIC: {
        HDC dc = (HDC)w;
        bool clip = (HWND)l == g_peakTxt && g_peakHold > 1.0f;
        SetTextColor(dc, clip ? RGB(255, 196, 90) : C_TEXT);
        SetBkColor(dc, C_BG);
        return (LRESULT)g_brBg;
    }
    case WM_MEASUREITEM: {
        MEASUREITEMSTRUCT* mi = (MEASUREITEMSTRUCT*)l;
        if (mi->CtlType == ODT_LISTBOX) { mi->itemHeight = 34; return TRUE; }
        break;
    }
    case WM_DRAWITEM: {
        const DRAWITEMSTRUCT* di = (const DRAWITEMSTRUCT*)l;
        if (di->CtlType == ODT_BUTTON) drawButton(di);
        else if (di->CtlType == ODT_LISTBOX) { if (di->CtlID == ID_PLLIST) drawPlItem(di); else drawListItem(di); }
        return TRUE;
    }
    case WM_NOTIFY: {
        NMHDR* nh = (NMHDR*)l;
        if (nh->code == NM_CUSTOMDRAW && isSlider(nh->hwndFrom)) return sliderDraw((NMCUSTOMDRAW*)l);
        break;
    }
    case WM_PAINT: {
        PAINTSTRUCT ps;
        HDC hdc = BeginPaint(h, &ps);
        RECT cr;
        GetClientRect(h, &cr);
        ensureBackBuffer(hdc, cr.right, cr.bottom);
        HRGN clip = CreateRectRgnIndirect(&ps.rcPaint);
        SelectClipRgn(g_bbDC, clip);
        DeleteObject(clip);
        paintAll(g_bbDC, h, cr.right, cr.bottom);
        SelectClipRgn(g_bbDC, nullptr);
        BitBlt(hdc, ps.rcPaint.left, ps.rcPaint.top, ps.rcPaint.right - ps.rcPaint.left, ps.rcPaint.bottom - ps.rcPaint.top,
               g_bbDC, ps.rcPaint.left, ps.rcPaint.top, SRCCOPY);
        EndPaint(h, &ps);
        return 0;
    }
    case WM_TIMER:
        if (w == 3) { KillTimer(h, 3); g_status.clear(); InvalidateRect(h, nullptr, FALSE); return 0; }
        if (w == 4) { KillTimer(h, 4); askAssociationFirstRun(); return 0; }   // primer arranque: ofrece asociar archivos
        if (w == 2) {
            KillTimer(h, 2);
            if (g_openFirst >= 0 && g_openFirst < (int)g_playlist.size()) { g_listFloat = false; playIndex(g_openFirst); }
            g_openFirst = -1;
            return 0;
        }
        onTimer();
        return 0;
    case WM_COPYDATA: {   // 1 = abrir y reproducir, 2 = anadir a la lista, 3 = solo traer la ventana al frente
        COPYDATASTRUCT* cd = (COPYDATASTRUCT*)l;
        if (!cd) return FALSE;
        std::vector<std::wstring> paths;
        if (cd->lpData && cd->cbData >= sizeof(wchar_t)) {
            std::wstring str((const wchar_t*)cd->lpData, cd->cbData / sizeof(wchar_t));
            size_t i = 0;
            while (i <= str.size()) {
                size_t j = str.find(L'\n', i);
                if (j == std::wstring::npos) j = str.size();
                std::wstring one = str.substr(i, j - i);
                while (!one.empty() && one.back() == 0) one.pop_back();
                if (!one.empty()) paths.push_back(one);
                i = j + 1;
            }
        }
        if (cd->dwData == 1 || cd->dwData == 2) handleExternalPaths(paths, cd->dwData == 1);
        if (cd->dwData == 1 || cd->dwData == 3) {
            if (IsIconic(h)) ShowWindow(h, SW_RESTORE);
            SetForegroundWindow(h);
        }
        return TRUE;
    }
    case WM_KEYDOWN:
        if (g_fullscreen) fsShow();   // cualquier tecla vuelve a mostrar los controles
        if (handleKey(w, false)) return 0;
        break;
    case WM_MOUSEWHEEL:
        if (g_fullscreen) fsShow();
        setVolume(g_vol01.load() + (GET_WHEEL_DELTA_WPARAM(w) > 0 ? 0.05f : -0.05f));
        return 0;
    case WM_LBUTTONDBLCLK: {
        POINT p = { (short)LOWORD(l), (short)HIWORD(l) };
        if (g_fullscreen) {
            RECT cr;
            GetClientRect(h, &cr);
            const FsL L = fsLayout(cr.right, cr.bottom);
            if (g_fsVisible && PtInRect(&L.bar, p)) { fsMouseDown(h, p); return 0; }   // en los controles no sale
            toggleFullscreen();
            return 0;
        }
        if (g_hasVideo && PtInRect(&g_videoRect, p)) toggleFullscreen();
        return 0;
    }
    case WM_SETCURSOR:
        if (g_fullscreen && LOWORD(l) == HTCLIENT && !g_fsVisible) { SetCursor(nullptr); return TRUE; }   // cursor oculto
        if (LOWORD(l) == HTCLIENT && expandBtnVisible()) {   // manita sobre el boton ampliar
            POINT cp;
            GetCursorPos(&cp);
            ScreenToClient(h, &cp);
            const RECT er = expandBtnRect();
            if (PtInRect(&er, cp)) { SetCursor(LoadCursor(nullptr, IDC_HAND)); return TRUE; }
        }
        break;
    case WM_APP_VIDEOINFO:
        if ((int)w == g_gen && !g_fullscreen) {
            relayout();
            if (g_videoAreaH < 300 && !IsZoomed(h)) {   // hacer la ventana mas alta para ver el video
                RECT wr;
                GetWindowRect(h, &wr);
                MONITORINFO mi = { sizeof(mi) };
                GetMonitorInfo(MonitorFromWindow(h, MONITOR_DEFAULTTONEAREST), &mi);
                int nh = std::min((int)(wr.bottom - wr.top) + (300 - g_videoAreaH), (int)(mi.rcWork.bottom - mi.rcWork.top));
                int top = std::min((int)wr.top, (int)mi.rcWork.bottom - nh);
                SetWindowPos(h, nullptr, wr.left, std::max(top, (int)mi.rcWork.top), wr.right - wr.left, nh, SWP_NOZORDER | SWP_NOCOPYBITS);
            }
        } else if ((int)w == g_gen) relayout();
        return 0;
    case WM_LBUTTONDOWN: {
        POINT p = { (short)LOWORD(l), (short)HIWORD(l) };
        if (g_fullscreen) { fsMouseDown(h, p); return 0; }
        if (expandBtnVisible()) {   // boton "ampliar" sobre el video
            const RECT er = expandBtnRect();
            if (PtInRect(&er, p)) { toggleFullscreen(); return 0; }
        }
        RECT cr;
        GetClientRect(h, &cr);
        if (g_spdOpen && !PtInRect(&g_spdRect, p)) { g_spdOpen = false; relayout(); }
        if (g_listFloat && !PtInRect(&g_panelRect, p)) { g_listFloat = false; relayout(); }
        int cb = capButtonAt(p.x, p.y, cr.right);
        if (cb >= 0) { g_capDown = cb; SetCapture(h); return 0; }
        if (PtInRect(&g_waveRect, p) && g_state != ST_STOPPED && !g_live) {
            g_dragging = true;
            SetCapture(h);
            g_dragFrac = std::min(1.0, std::max(0.0, (double)(p.x - g_waveRect.left) / (g_waveRect.right - g_waveRect.left)));
        }
        return 0;
    }
    case WM_MOUSEMOVE: {
        POINT p = { (short)LOWORD(l), (short)HIWORD(l) };
        if (g_fullscreen) { fsMouseMove(h, p); return 0; }
        {
            const RECT er = expandBtnRect();
            const bool eh = expandBtnVisible() && PtInRect(&er, p);
            if (eh != g_expHot) { g_expHot = eh; InvalidateRect(h, &er, FALSE); }
            if (eh) { TRACKMOUSEEVENT te0 = { sizeof(te0), TME_LEAVE, h, 0 }; TrackMouseEvent(&te0); }
        }
        RECT cr;
        GetClientRect(h, &cr);
        int hot = capButtonAt(p.x, p.y, cr.right);
        if (hot != g_capHot) {
            g_capHot = hot;
            RECT cap = { 0, 0, cr.right, CAP_H };
            InvalidateRect(h, &cap, FALSE);
            TRACKMOUSEEVENT te = { sizeof(te), TME_LEAVE, h, 0 };
            TrackMouseEvent(&te);
        }
        if (g_dragging) {
            g_dragFrac = std::min(1.0, std::max(0.0, (double)(p.x - g_waveRect.left) / (g_waveRect.right - g_waveRect.left)));
        }
        return 0;
    }
    case WM_MOUSELEAVE:
        if (g_fullscreen) { if (g_fsHot || g_fsVolHover) { g_fsHot = 0; g_fsVolHover = false; fsInvalidateBar(); } return 0; }
        if (g_expHot) { g_expHot = false; const RECT er = expandBtnRect(); InvalidateRect(h, &er, FALSE); }
        if (g_capHot != -1) { g_capHot = -1; RECT cap = { 0, 0, 4000, CAP_H }; InvalidateRect(h, &cap, FALSE); }
        return 0;
    case WM_LBUTTONUP: {
        if (g_fullscreen || g_fsDrag) { fsMouseUp(); return 0; }
        if (g_capDown >= 0) {
            int down = g_capDown;
            g_capDown = -1;
            ReleaseCapture();
            POINT p = { (short)LOWORD(l), (short)HIWORD(l) };
            RECT cr;
            GetClientRect(h, &cr);
            if (capButtonAt(p.x, p.y, cr.right) == down) {
                if (down == 0) ShowWindow(h, SW_MINIMIZE);
                else if (down == 1) ShowWindow(h, IsZoomed(h) ? SW_RESTORE : SW_MAXIMIZE);
                else PostMessage(h, WM_CLOSE, 0, 0);
            }
            return 0;
        }
        if (g_dragging) {
            g_dragging = false;
            ReleaseCapture();
            double d = g_duration;
            if (d <= 0) {
                Lock lk(&g_waveCS);
                d = g_peaks.size() * SEC_PER_PEAK;
            }
            seekTo(g_dragFrac * d);
        }
        return 0;
    }
    case WM_HSCROLL: {
        HWND src = (HWND)l;
        if (src == g_gain) {
            g_gainDb = (float)SendMessage(g_gain, TBM_GETPOS, 0, 0) / 2.f;
        } else if (src == g_vol) {
            g_vol01 = (float)SendMessage(g_vol, TBM_GETPOS, 0, 0) / 100.f;
            if (g_vol01.load() > 0) g_muted = false;
            InvalidateRect(g_vol, nullptr, FALSE);
        } else if (src == g_spd) {
            speedFromSlider();
        } else {
            for (int i = 0; i < 3; i++)
                if (src == g_eq[i]) g_eqDb[i] = (float)SendMessage(g_eq[i], TBM_GETPOS, 0, 0);
        }
        updateLabels();
        return 0;
    }
    case WM_HOTKEY:   // teclas multimedia (atajos globales)
        switch (w) {
        case HK_PLAYPAUSE: togglePause(); break;
        case HK_NEXT: nextTrack(1); break;
        case HK_PREV: nextTrack(-1); break;
        case HK_STOP: stopAll(); break;
        }
        return 0;
    case WM_APPCOMMAND: {   // teclas multimedia cuando la ventana tiene el foco (y adelantar/retroceder)
        switch (GET_APPCOMMAND_LPARAM(l)) {
        case APPCOMMAND_MEDIA_NEXTTRACK: nextTrack(1); return TRUE;
        case APPCOMMAND_MEDIA_PREVIOUSTRACK: nextTrack(-1); return TRUE;
        case APPCOMMAND_MEDIA_PLAY_PAUSE: togglePause(); return TRUE;
        case APPCOMMAND_MEDIA_PLAY: if (g_state != ST_PLAYING) togglePause(); return TRUE;
        case APPCOMMAND_MEDIA_PAUSE: if (g_state == ST_PLAYING) togglePause(); return TRUE;
        case APPCOMMAND_MEDIA_STOP: stopAll(); return TRUE;
        case APPCOMMAND_MEDIA_FAST_FORWARD: seekTo(g_posSec.load() + 10.0); return TRUE;
        case APPCOMMAND_MEDIA_REWIND: seekTo(std::max(0.0, g_posSec.load() - 10.0)); return TRUE;
        }
        break;   // volumen y demas: que los maneje Windows
    }
    case WM_COMMAND:
        switch (LOWORD(w)) {
        case ID_OPEN: doOpenFiles(); break;
        case ID_FOLDER: doOpenFolder(); break;
        case ID_PREV: nextTrack(-1); break;
        case ID_NEXT: nextTrack(1); break;
        case ID_PLAY: togglePause(); break;
        case ID_STOP: stopAll(); break;
        case ID_MUTE: g_muted = !g_muted.load(); updateLabels(); break;
        case ID_RESET:
            g_gainDb = 0.f;
            SendMessage(g_gain, TBM_SETPOS, TRUE, 0);
            for (int i = 0; i < 3; i++) { g_eqDb[i] = 0.f; SendMessage(g_eq[i], TBM_SETPOS, TRUE, 0); }
            updateLabels();
            break;
        case ID_LIMIT: g_limiter = !g_limiter.load(); InvalidateRect(g_limit, nullptr, FALSE); break;
        case ID_OUTBTN: if (GetTickCount() - g_outMenuClosedOnBtn > 500) showOutputMenu(); break;
        case ID_SPEED: g_spdOpen = !g_spdOpen; relayout(); break;
        case ID_SPDRESET: resetSpeed(); break;
        case ID_PLSAVE: doSavePlaylist(); break;
        case ID_ABOUT: showAbout(); break;
        case ID_NETURL: doOpenNetUrl(); break;
        case ID_SNAP: captureFrame(); break;
        case ID_SNAPDIR: openCaptureFolder(); break;
        case ID_SNAPOPT: if (GetTickCount() - g_optMenuClosedOnBtn > 500) showCaptureOptions(); break;
        case ID_PLIMP: doImportPlaylist(); break;
        case ID_PLEXP: doExportCurrent(); break;
        case ID_LISTBTN: g_listFloat = !g_listFloat; relayout(); break;
        case ID_PEAKON: g_peakOn = !g_peakOn.load(); InvalidateRect(g_peakChk, nullptr, FALSE); break;
        case ID_LIST:
            if (HIWORD(w) == LBN_DBLCLK) {
                int i = (int)SendMessage(g_list, LB_GETCURSEL, 0, 0);
                if (i >= 0) { g_listFloat = false; playIndex(i); }
            }
            break;
        }
        if (LOWORD(w) != ID_LIST && LOWORD(w) != ID_PLLIST && !g_fullscreen) SetFocus(g_hasVideo ? g_hwnd : g_list);
        return 0;
    case WM_DROPFILES: {
        HDROP d = (HDROP)w;
        UINT n = DragQueryFileW(d, 0xFFFFFFFF, nullptr, 0);
        std::vector<std::wstring> paths;
        for (UINT i = 0; i < n; i++) {
            wchar_t b[MAX_PATH * 2];
            if (DragQueryFileW(d, i, b, MAX_PATH * 2)) paths.push_back(b);
        }
        DragFinish(d);
        addPaths(paths);
        return 0;
    }
    case WM_APP_OPENED:
        return 0;
    case WM_APP_END:
        if ((int)w == g_gen) nextTrack(1, true);
        return 0;
    case WM_APP_ERROR:
        if ((int)w == g_gen) {
            g_status = l == 1 ? TX(L"No se pudo abrir el dispositivo de audio")
                     : g_live ? TX(L"No se pudo conectar con la transmisi\u00f3n (revisa la direcci\u00f3n, el usuario y la contrase\u00f1a)")
                     : TX(L"No se pudo abrir/decodificar el archivo");
            int cur = g_cur;
            if (l != 1 && !g_live && cur + 1 < (int)g_playlist.size()) { stopAudio(); playIndex(cur + 1); g_status = TX(L"(se omitio un archivo no valido)"); }
            else stopAudio();
        }
        return 0;
    case WM_DESTROY:
        KillTimer(h, 1);
        if (g_mediaKeysOn) { UnregisterHotKey(h, HK_PLAYPAUSE); UnregisterHotKey(h, HK_NEXT); UnregisterHotKey(h, HK_PREV); UnregisterHotKey(h, HK_STOP); }
        stopAudio();
        ++g_waveGen;
        PostQuitMessage(0);
        return 0;
    }
    return DefWindowProc(h, m, w, l);
}

int WINAPI wWinMain(HINSTANCE hi, HINSTANCE, PWSTR, int show) {
    // linea de comandos:  MiauPlayer.exe [/add] ruta1 [ruta2 ...]   (/add = anadir a la lista sin interrumpir)
    bool addMode = false;
    std::wstring langArg;
    std::vector<std::wstring> cmdPaths;
    {
        int argc = 0;
        LPWSTR* argv = CommandLineToArgvW(GetCommandLineW(), &argc);
        if (argv) {
            for (int i = 1; i < argc; i++) {
                if (_wcsnicmp(argv[i], L"/lang=", 6) == 0) { langArg = argv[i] + 6; continue; }
                if (i == 1 && (_wcsicmp(argv[i], L"/add") == 0 || _wcsicmp(argv[i], L"--add") == 0)) { addMode = true; continue; }
                if (isUrl(argv[i])) { cmdPaths.push_back(argv[i]); continue; }
                wchar_t full[MAX_PATH * 4];
                DWORD n = GetFullPathNameW(argv[i], MAX_PATH * 4, full, nullptr);
                cmdPaths.push_back(n > 0 && n < MAX_PATH * 4 ? std::wstring(full) : std::wstring(argv[i]));
            }
            LocalFree(argv);
        }
    }
    initLanguage(langArg);
    loadCaptureDir();
    // instancia unica: si ya hay un MiauPlayer abierto, le pasamos los archivos y salimos
    HANDLE singleMutex = CreateMutexW(nullptr, FALSE, L"Local\\MiauPlayer.SingleInstance");
    if (singleMutex && GetLastError() == ERROR_ALREADY_EXISTS) {
        HWND other = nullptr;
        for (int i = 0; i < 80 && !other; i++) {   // hasta ~6 s por si la otra instancia aun esta arrancando
            other = FindWindowW(L"MiauPlayer", nullptr);
            if (!other) Sleep(75);
        }
        if (other) {
            std::wstring joined;
            for (auto& pth : cmdPaths) joined += pth + L"\n";
            COPYDATASTRUCT cd = {};
            cd.dwData = cmdPaths.empty() ? 3 : (addMode ? 2 : 1);
            cd.cbData = (DWORD)((joined.size() + 1) * sizeof(wchar_t));
            cd.lpData = (PVOID)joined.c_str();
            AllowSetForegroundWindow(ASFW_ANY);
            DWORD_PTR res = 0;
            SendMessageTimeout(other, WM_COPYDATA, 0, (LPARAM)&cd, SMTO_ABORTIFHUNG, 5000, &res);
            return 0;
        }
    }
    av_log_set_level(AV_LOG_QUIET);
    InitializeCriticalSection(&g_hwoCS);
    loadOutName();
    InitializeCriticalSection(&g_waveCS);
    InitializeCriticalSection(&g_vidCS);
    OleInitialize(nullptr);
    INITCOMMONCONTROLSEX ic = { sizeof(ic), ICC_BAR_CLASSES | ICC_WIN95_CLASSES };
    InitCommonControlsEx(&ic);
    Gdiplus::GdiplusStartupInput gin;
    Gdiplus::GdiplusStartup(&g_gdipToken, &gin, nullptr);

    g_font = CreateFont(-15, 0, 0, 0, FW_NORMAL, 0, 0, 0, DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
                        CLEARTYPE_QUALITY, DEFAULT_PITCH | FF_DONTCARE, L"Segoe UI");
    g_fontBold = CreateFont(-15, 0, 0, 0, FW_SEMIBOLD, 0, 0, 0, DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
                            CLEARTYPE_QUALITY, DEFAULT_PITCH | FF_DONTCARE, L"Segoe UI");
    g_brBg = CreateSolidBrush(C_BG);
    g_iconSmall = (HICON)LoadImage(hi, MAKEINTRESOURCE(1), IMAGE_ICON, 20, 20, 0);

    WNDCLASSEX wc = {};
    wc.cbSize = sizeof(wc);
    wc.style = CS_DBLCLKS;
    wc.lpfnWndProc = WndProc;
    wc.hInstance = hi;
    wc.hCursor = LoadCursor(nullptr, IDC_ARROW);
    wc.hbrBackground = g_brBg;
    wc.lpszClassName = L"MiauPlayer";
    wc.hIcon = (HICON)LoadImage(hi, MAKEINTRESOURCE(1), IMAGE_ICON, 32, 32, 0);
    wc.hIconSm = (HICON)LoadImage(hi, MAKEINTRESOURCE(1), IMAGE_ICON, 16, 16, 0);
    RegisterClassEx(&wc);

    RECT wa = { 0, 0, 1024, 728 };
    SystemParametersInfo(SPI_GETWORKAREA, 0, &wa, 0);
    const int winW = std::min(780, (int)(wa.right - wa.left) - 16);
    const int winH = std::min(660, (int)(wa.bottom - wa.top) - 16);
    const int winX = wa.left + ((wa.right - wa.left) - winW) / 2;
    const int winY = wa.top + ((wa.bottom - wa.top) - winH) / 2;
    HWND hw = CreateWindowEx(0, L"MiauPlayer", L"MiauPlayer", WS_OVERLAPPEDWINDOW | WS_CLIPCHILDREN,
                             winX, winY, winW, winH, nullptr, nullptr, hi, nullptr);
    ShowWindow(hw, show);
    UpdateWindow(hw);

    handleExternalPaths(cmdPaths, !addMode);   // archivos recibidos al abrir el programa
    if (!addMode) SetTimer(hw, 4, 1500, nullptr);   // 1,5 s despues de abrir: pregunta (una sola vez) por la asociacion de archivos

    MSG msg;
    while (GetMessage(&msg, nullptr, 0, 0)) {
        TranslateMessage(&msg);
        DispatchMessage(&msg);
    }
    Gdiplus::GdiplusShutdown(g_gdipToken);
    OleUninitialize();
    return 0;
}
