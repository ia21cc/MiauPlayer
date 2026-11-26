Unicode true
!include "MUI2.nsh"
!include "nsDialogs.nsh"
!include "LogicLib.nsh"
!include "x64.nsh"

!define APP "MiauPlayer"
!define VERSION "1.7.25"
!define UNKEY "Software\Microsoft\Windows\CurrentVersion\Uninstall\MiauPlayer"
Name "${APP}"
OutFile "MiauPlayer-Setup-${VERSION}.exe"
InstallDir "$PROGRAMFILES64\${APP}"
InstallDirRegKey HKLM "Software\MiauPlayer" "InstallDir"
RequestExecutionLevel admin
SetCompressor /SOLID lzma
SetCompressorDictSize 64
BrandingText "MiauPlayer ${VERSION}"
VIProductVersion "1.7.25.0"
VIAddVersionKey /LANG=1034 "ProductName" "MiauPlayer"
VIAddVersionKey /LANG=1034 "FileDescription" "Instalador de MiauPlayer"
VIAddVersionKey /LANG=1034 "FileVersion" "${VERSION}"
VIAddVersionKey /LANG=1034 "LegalCopyright" "MiauPlayer"

!define MUI_ICON "app.ico"
!define MUI_UNICON "app.ico"
!define MUI_ABORTWARNING
!define MUI_WELCOMEPAGE_TEXT "$(LS_WELCOME)"

Var H0
Var S0
Var H1
Var S1
Var H2
Var S2
Var H3
Var S3
Var H4
Var S4
Var H5
Var S5
Var H6
Var S6
Var H7
Var S7
Var H8
Var S8
Var H9
Var S9
Var Dialog
Var HDesk
Var SDesk

!insertmacro MUI_PAGE_WELCOME
!insertmacro MUI_PAGE_DIRECTORY
Page custom AssocPage AssocLeave
!insertmacro MUI_PAGE_INSTFILES
!define MUI_FINISHPAGE_RUN
!define MUI_FINISHPAGE_TEXT "$(LS_FIN_TEXT)"
!define MUI_FINISHPAGE_RUN_TEXT "$(LS_RUN)"
!define MUI_FINISHPAGE_RUN_FUNCTION LaunchApp
!define MUI_FINISHPAGE_SHOWREADME ""
!define MUI_FINISHPAGE_SHOWREADME_TEXT "$(LS_DEFAPPS)"
!define MUI_FINISHPAGE_SHOWREADME_FUNCTION OpenDefaultApps
!define MUI_FINISHPAGE_SHOWREADME_NOTCHECKED
!insertmacro MUI_PAGE_FINISH
!insertmacro MUI_UNPAGE_CONFIRM
!insertmacro MUI_UNPAGE_INSTFILES
!insertmacro MUI_LANGUAGE "English"
!insertmacro MUI_LANGUAGE "Spanish"
!insertmacro MUI_LANGUAGE "PortugueseBR"

LangString LS_WELCOME ${LANG_ENGLISH} "This wizard will install MiauPlayer on your computer.$\r$\n$\r$\nOne of the steps lets you choose which file types to open with MiauPlayer.$\r$\n$\r$\nMiauPlayer will use the language of this installer. You can change it later in the program, under Settings (gear icon).$\r$\n$\r$\nClick Next to continue."
LangString LS_WELCOME ${LANG_SPANISH} "Este asistente instalará MiauPlayer en tu equipo.$\r$\n$\r$\nEn uno de los pasos podrás elegir qué tipos de archivo quieres abrir con MiauPlayer.$\r$\n$\r$\nMiauPlayer usará el idioma de este instalador. Podrás cambiarlo después en el programa, en Configuración (icono del engranaje).$\r$\n$\r$\nHaz clic en Siguiente para continuar."
LangString LS_WELCOME ${LANG_PORTUGUESEBR} "Este assistente instalará o MiauPlayer no seu computador.$\r$\n$\r$\nEm uma das etapas você poderá escolher quais tipos de arquivo abrir com o MiauPlayer.$\r$\n$\r$\nO MiauPlayer usará o idioma deste instalador. Você poderá alterá-lo depois no programa, em Configurações (ícone de engrenagem).$\r$\n$\r$\nClique em Avançar para continuar."
LangString LS_FIN_TEXT ${LANG_ENGLISH} "MiauPlayer was installed successfully. Windows will ask you to confirm the associated file types."
LangString LS_FIN_TEXT ${LANG_SPANISH} "MiauPlayer se instaló correctamente. Windows te pedirá confirmar los tipos de archivo asociados."
LangString LS_FIN_TEXT ${LANG_PORTUGUESEBR} "O MiauPlayer foi instalado com sucesso. O Windows pedirá que você confirme os tipos de arquivo associados."
LangString LS_RUN ${LANG_ENGLISH} "Open MiauPlayer"
LangString LS_RUN ${LANG_SPANISH} "Abrir MiauPlayer"
LangString LS_RUN ${LANG_PORTUGUESEBR} "Abrir o MiauPlayer"
LangString LS_DEFAPPS ${LANG_ENGLISH} "Open default apps"
LangString LS_DEFAPPS ${LANG_SPANISH} "Abrir aplicaciones predeterminadas"
LangString LS_DEFAPPS ${LANG_PORTUGUESEBR} "Abrir aplicativos padrão"
LangString LS_ASSOC_TITLE ${LANG_ENGLISH} "Associate file types"
LangString LS_ASSOC_TITLE ${LANG_SPANISH} "Asociar tipos de archivo"
LangString LS_ASSOC_TITLE ${LANG_PORTUGUESEBR} "Associar tipos de arquivo"
LangString LS_ASSOC_SUB ${LANG_ENGLISH} "Choose which files to open with MiauPlayer when you double-click them."
LangString LS_ASSOC_SUB ${LANG_SPANISH} "Elige qué archivos quieres abrir con MiauPlayer al hacerles doble clic."
LangString LS_ASSOC_SUB ${LANG_PORTUGUESEBR} "Escolha quais arquivos abrir com o MiauPlayer ao clicar duas vezes neles."
LangString LS_ASSOC_LABEL ${LANG_ENGLISH} "Check the file types you want to open with MiauPlayer. You can change this later in Windows Settings."
LangString LS_ASSOC_LABEL ${LANG_SPANISH} "Marca los tipos de archivo que quieres abrir con MiauPlayer. Puedes cambiarlo después en Configuración de Windows."
LangString LS_ASSOC_LABEL ${LANG_PORTUGUESEBR} "Marque os tipos de arquivo que deseja abrir com o MiauPlayer. Você pode alterar isso depois nas Configurações do Windows."
LangString LS_SELALL ${LANG_ENGLISH} "Select all"
LangString LS_SELALL ${LANG_SPANISH} "Seleccionar todo"
LangString LS_SELALL ${LANG_PORTUGUESEBR} "Selecionar tudo"
LangString LS_SELNONE ${LANG_ENGLISH} "Clear all"
LangString LS_SELNONE ${LANG_SPANISH} "Quitar todo"
LangString LS_SELNONE ${LANG_PORTUGUESEBR} "Limpar tudo"
LangString LS_DESKTOP ${LANG_ENGLISH} "Create a desktop shortcut"
LangString LS_DESKTOP ${LANG_SPANISH} "Crear acceso directo en el escritorio"
LangString LS_DESKTOP ${LANG_PORTUGUESEBR} "Criar atalho na área de trabalho"
LangString LS_X64 ${LANG_ENGLISH} "MiauPlayer requires 64-bit Windows."
LangString LS_X64 ${LANG_SPANISH} "MiauPlayer requiere Windows de 64 bits."
LangString LS_X64 ${LANG_PORTUGUESEBR} "O MiauPlayer requer Windows de 64 bits."
LangString LS_ADDLIST ${LANG_ENGLISH} "Add to MiauPlayer playlist"
LangString LS_ADDLIST ${LANG_SPANISH} "Añadir a la lista MiauPlayer"
LangString LS_ADDLIST ${LANG_PORTUGUESEBR} "Adicionar à lista do MiauPlayer"
LangString LS_AUDIOFILE ${LANG_ENGLISH} "Audio file (MiauPlayer)"
LangString LS_AUDIOFILE ${LANG_SPANISH} "Archivo de audio (MiauPlayer)"
LangString LS_AUDIOFILE ${LANG_PORTUGUESEBR} "Arquivo de áudio (MiauPlayer)"
LangString LS_VIDEOFILE ${LANG_ENGLISH} "Video file (MiauPlayer)"
LangString LS_VIDEOFILE ${LANG_SPANISH} "Archivo de video (MiauPlayer)"
LangString LS_VIDEOFILE ${LANG_PORTUGUESEBR} "Arquivo de vídeo (MiauPlayer)"
LangString LS_APPDESC ${LANG_ENGLISH} "Audio and video player with waveform."
LangString LS_APPDESC ${LANG_SPANISH} "Reproductor de audio y video con forma de onda."
LangString LS_APPDESC ${LANG_PORTUGUESEBR} "Reprodutor de áudio e vídeo com forma de onda."
LangString LS_DELPL ${LANG_ENGLISH} "Do you also want to delete your saved playlists?"
LangString LS_DELPL ${LANG_SPANISH} "¿Quieres eliminar también tus listas de reproducción guardadas?"
LangString LS_DELPL ${LANG_PORTUGUESEBR} "Deseja excluir também suas playlists salvas?"


!macro RegExt EXT PROGID
  WriteRegStr HKLM "Software\Classes\.${EXT}\OpenWithProgids" "${PROGID}" ""
  WriteRegStr HKLM "Software\MiauPlayer\Capabilities\FileAssociations" ".${EXT}" "${PROGID}"
  WriteRegStr HKLM "Software\Classes\Applications\MiauPlayer.exe\SupportedTypes" ".${EXT}" ""
  WriteRegStr HKLM "Software\Classes\SystemFileAssociations\.${EXT}\shell\MiauPlayer.Add" "" "$(LS_ADDLIST)"
  WriteRegStr HKLM "Software\Classes\SystemFileAssociations\.${EXT}\shell\MiauPlayer.Add" "MultiSelectModel" "Player"
  WriteRegStr HKLM "Software\Classes\SystemFileAssociations\.${EXT}\shell\MiauPlayer.Add" "Icon" '"$INSTDIR\MiauPlayer.exe",0'
  WriteRegStr HKLM "Software\Classes\SystemFileAssociations\.${EXT}\shell\MiauPlayer.Add\command" "" '"$INSTDIR\MiauPlayer.exe" /add "%1"'
!macroend
!macro AssocExt EXT PROGID
  ReadRegStr $0 HKLM "Software\Classes\.${EXT}" ""
  ${If} $0 != "${PROGID}"
    ${If} $0 != ""
      WriteRegStr HKLM "Software\Classes\.${EXT}" "MiauPlayer.backup" "$0"
    ${EndIf}
    WriteRegStr HKLM "Software\Classes\.${EXT}" "" "${PROGID}"
  ${EndIf}
!macroend
!macro UnExt EXT PROGID
  ReadRegStr $0 HKLM "Software\Classes\.${EXT}" ""
  ${If} $0 == "${PROGID}"
    ReadRegStr $1 HKLM "Software\Classes\.${EXT}" "MiauPlayer.backup"
    ${If} $1 != ""
      WriteRegStr HKLM "Software\Classes\.${EXT}" "" "$1"
    ${Else}
      DeleteRegValue HKLM "Software\Classes\.${EXT}" ""
    ${EndIf}
  ${EndIf}
  DeleteRegValue HKLM "Software\Classes\.${EXT}" "MiauPlayer.backup"
  DeleteRegValue HKLM "Software\Classes\.${EXT}\OpenWithProgids" "${PROGID}"
  DeleteRegKey HKLM "Software\Classes\SystemFileAssociations\.${EXT}\shell\MiauPlayer.Add"
!macroend

Function CloseRunning
  FindWindow $0 "MiauPlayer"
  ${If} $0 != 0
    SendMessage $0 16 0 0
    Sleep 1200
  ${EndIf}
FunctionEnd
Function un.CloseRunning
  FindWindow $0 "MiauPlayer"
  ${If} $0 != 0
    SendMessage $0 16 0 0
    Sleep 1200
  ${EndIf}
FunctionEnd

Function .onInit
  ${IfNot} ${RunningX64}
    MessageBox MB_OK|MB_ICONSTOP "$(LS_X64)"
    Abort
  ${EndIf}
  SetRegView 64
  StrCpy $S0 1
  StrCpy $S1 1
  StrCpy $S2 1
  StrCpy $S3 1
  StrCpy $S4 1
  StrCpy $S5 1
  StrCpy $S6 1
  StrCpy $S7 1
  StrCpy $S8 1
  StrCpy $S9 1
  StrCpy $SDesk 1
  ${If} ${Silent}
    StrCpy $S0 0
    StrCpy $S1 0
    StrCpy $S2 0
    StrCpy $S3 0
    StrCpy $S4 0
    StrCpy $S5 0
    StrCpy $S6 0
    StrCpy $S7 0
    StrCpy $S8 0
    StrCpy $S9 0
  ${EndIf}
FunctionEnd
Function un.onInit
  SetRegView 64
FunctionEnd

Function LaunchApp
  Exec '"$WINDIR\explorer.exe" "$INSTDIR\MiauPlayer.exe"'
FunctionEnd
Function OpenDefaultApps
  ExecShell "open" "ms-settings:defaultapps"
FunctionEnd

Function AssocPage
  !insertmacro MUI_HEADER_TEXT "$(LS_ASSOC_TITLE)" "$(LS_ASSOC_SUB)"
  nsDialogs::Create 1018
  Pop $Dialog
  ${If} $Dialog == error
    Abort
  ${EndIf}
  ${NSD_CreateLabel} 0 0 100% 24u "$(LS_ASSOC_LABEL)"
  Pop $0
  ${NSD_CreateCheckbox} 10u 24u 90u 11u ".mp3"
  Pop $H0
  ${NSD_SetState} $H0 $S0
  ${NSD_CreateCheckbox} 10u 36u 90u 11u ".mp4"
  Pop $H1
  ${NSD_SetState} $H1 $S1
  ${NSD_CreateCheckbox} 10u 48u 90u 11u ".ogg"
  Pop $H2
  ${NSD_SetState} $H2 $S2
  ${NSD_CreateCheckbox} 10u 60u 90u 11u ".wma"
  Pop $H3
  ${NSD_SetState} $H3 $S3
  ${NSD_CreateCheckbox} 10u 72u 90u 11u ".wmv"
  Pop $H4
  ${NSD_SetState} $H4 $S4
  ${NSD_CreateCheckbox} 110u 24u 90u 11u ".aac"
  Pop $H5
  ${NSD_SetState} $H5 $S5
  ${NSD_CreateCheckbox} 110u 36u 90u 11u ".m4a"
  Pop $H6
  ${NSD_SetState} $H6 $S6
  ${NSD_CreateCheckbox} 110u 48u 90u 11u ".mpg"
  Pop $H7
  ${NSD_SetState} $H7 $S7
  ${NSD_CreateCheckbox} 110u 60u 90u 11u ".mov"
  Pop $H8
  ${NSD_SetState} $H8 $S8
  ${NSD_CreateCheckbox} 110u 72u 90u 11u ".mkv"
  Pop $H9
  ${NSD_SetState} $H9 $S9
  ${NSD_CreateButton} 10u 90u 70u 13u "$(LS_SELALL)"
  Pop $0
  ${NSD_OnClick} $0 SelAll
  ${NSD_CreateButton} 86u 90u 70u 13u "$(LS_SELNONE)"
  Pop $0
  ${NSD_OnClick} $0 SelNone
  ${NSD_CreateCheckbox} 10u 110u 100% 11u "$(LS_DESKTOP)"
  Pop $HDesk
  ${NSD_SetState} $HDesk $SDesk
  nsDialogs::Show
FunctionEnd
Function SelAll
  ${NSD_SetState} $H0 ${BST_CHECKED}
  ${NSD_SetState} $H1 ${BST_CHECKED}
  ${NSD_SetState} $H2 ${BST_CHECKED}
  ${NSD_SetState} $H3 ${BST_CHECKED}
  ${NSD_SetState} $H4 ${BST_CHECKED}
  ${NSD_SetState} $H5 ${BST_CHECKED}
  ${NSD_SetState} $H6 ${BST_CHECKED}
  ${NSD_SetState} $H7 ${BST_CHECKED}
  ${NSD_SetState} $H8 ${BST_CHECKED}
  ${NSD_SetState} $H9 ${BST_CHECKED}
FunctionEnd
Function SelNone
  ${NSD_SetState} $H0 ${BST_UNCHECKED}
  ${NSD_SetState} $H1 ${BST_UNCHECKED}
  ${NSD_SetState} $H2 ${BST_UNCHECKED}
  ${NSD_SetState} $H3 ${BST_UNCHECKED}
  ${NSD_SetState} $H4 ${BST_UNCHECKED}
  ${NSD_SetState} $H5 ${BST_UNCHECKED}
  ${NSD_SetState} $H6 ${BST_UNCHECKED}
  ${NSD_SetState} $H7 ${BST_UNCHECKED}
  ${NSD_SetState} $H8 ${BST_UNCHECKED}
  ${NSD_SetState} $H9 ${BST_UNCHECKED}
FunctionEnd
Function AssocLeave
  ${NSD_GetState} $H0 $S0
  ${NSD_GetState} $H1 $S1
  ${NSD_GetState} $H2 $S2
  ${NSD_GetState} $H3 $S3
  ${NSD_GetState} $H4 $S4
  ${NSD_GetState} $H5 $S5
  ${NSD_GetState} $H6 $S6
  ${NSD_GetState} $H7 $S7
  ${NSD_GetState} $H8 $S8
  ${NSD_GetState} $H9 $S9
  ${NSD_GetState} $HDesk $SDesk
FunctionEnd

Section "MiauPlayer" SecMain
  SectionIn RO
  SetShellVarContext all
  SetRegView 64
  Call CloseRunning
  SetOutPath "$INSTDIR"
  File "dist\MiauPlayer.exe"
  File "dist\*.dll"
  File "dist\FFMPEG-LICENSE.txt"
  File "dist\LEEME.txt"
  CreateDirectory "$INSTDIR\playlist"
  nsExec::Exec 'icacls "$INSTDIR\playlist" /grant *S-1-5-32-545:(OI)(CI)M'
  Pop $0
  WriteUninstaller "$INSTDIR\Uninstall.exe"
  CreateShortcut "$SMPROGRAMS\MiauPlayer.lnk" "$INSTDIR\MiauPlayer.exe"
  ${If} $SDesk == 1
    CreateShortcut "$DESKTOP\MiauPlayer.lnk" "$INSTDIR\MiauPlayer.exe"
  ${EndIf}
  ; programa en Aplicaciones instaladas
  WriteRegStr HKLM "Software\MiauPlayer" "InstallDir" "$INSTDIR"
  ${If} $LANGUAGE == ${LANG_SPANISH}
    WriteRegStr HKLM "Software\MiauPlayer" "Language" "es"
  ${ElseIf} $LANGUAGE == ${LANG_PORTUGUESEBR}
    WriteRegStr HKLM "Software\MiauPlayer" "Language" "pt"
  ${Else}
    WriteRegStr HKLM "Software\MiauPlayer" "Language" "en"
  ${EndIf}
  WriteRegStr HKLM "${UNKEY}" "DisplayName" "MiauPlayer"
  WriteRegStr HKLM "${UNKEY}" "DisplayVersion" "${VERSION}"
  WriteRegStr HKLM "${UNKEY}" "Publisher" "IA21"
  WriteRegStr HKLM "${UNKEY}" "DisplayIcon" '"$INSTDIR\MiauPlayer.exe"'
  WriteRegStr HKLM "${UNKEY}" "InstallLocation" "$INSTDIR"
  WriteRegStr HKLM "${UNKEY}" "UninstallString" '"$INSTDIR\Uninstall.exe"'
  WriteRegStr HKLM "${UNKEY}" "QuietUninstallString" '"$INSTDIR\Uninstall.exe" /S'
  WriteRegDWORD HKLM "${UNKEY}" "NoModify" 1
  WriteRegDWORD HKLM "${UNKEY}" "NoRepair" 1
  WriteRegDWORD HKLM "${UNKEY}" "EstimatedSize" 200000
  ; ProgIDs y "Abrir con"
  WriteRegStr HKLM "Software\Classes\MiauPlayer.Audio" "" "$(LS_AUDIOFILE)"
  WriteRegStr HKLM "Software\Classes\MiauPlayer.Audio" "FriendlyTypeName" "$(LS_AUDIOFILE)"
  WriteRegStr HKLM "Software\Classes\MiauPlayer.Audio\DefaultIcon" "" '"$INSTDIR\MiauPlayer.exe",-2'
  WriteRegStr HKLM "Software\Classes\MiauPlayer.Audio\shell\open\command" "" '"$INSTDIR\MiauPlayer.exe" "%1"'
  WriteRegStr HKLM "Software\Classes\MiauPlayer.Audio\shell\open" "MultiSelectModel" "Player"
  WriteRegStr HKLM "Software\Classes\MiauPlayer.Video" "" "$(LS_VIDEOFILE)"
  WriteRegStr HKLM "Software\Classes\MiauPlayer.Video" "FriendlyTypeName" "$(LS_VIDEOFILE)"
  WriteRegStr HKLM "Software\Classes\MiauPlayer.Video\DefaultIcon" "" '"$INSTDIR\MiauPlayer.exe",-2'
  WriteRegStr HKLM "Software\Classes\MiauPlayer.Video\shell\open\command" "" '"$INSTDIR\MiauPlayer.exe" "%1"'
  WriteRegStr HKLM "Software\Classes\MiauPlayer.Video\shell\open" "MultiSelectModel" "Player"
  WriteRegStr HKLM "Software\Classes\Applications\MiauPlayer.exe" "FriendlyAppName" "MiauPlayer"
  WriteRegStr HKLM "Software\Classes\Applications\MiauPlayer.exe\DefaultIcon" "" '"$INSTDIR\MiauPlayer.exe",0'
  WriteRegStr HKLM "Software\Classes\Applications\MiauPlayer.exe\shell\open\command" "" '"$INSTDIR\MiauPlayer.exe" "%1"'
  WriteRegStr HKLM "Software\Classes\Applications\MiauPlayer.exe\shell\open" "MultiSelectModel" "Player"
  WriteRegStr HKLM "Software\MiauPlayer\Capabilities" "ApplicationName" "MiauPlayer"
  WriteRegStr HKLM "Software\MiauPlayer\Capabilities" "ApplicationDescription" "$(LS_APPDESC)"
  WriteRegStr HKLM "Software\RegisteredApplications" "MiauPlayer" "Software\MiauPlayer\Capabilities"
  ; menu contextual "Anadir a la lista" y "Abrir con" para todos los formatos de audio/video conocidos
  !insertmacro RegExt "mp3" "MiauPlayer.Audio"
  !insertmacro RegExt "mp2" "MiauPlayer.Audio"
  !insertmacro RegExt "mp1" "MiauPlayer.Audio"
  !insertmacro RegExt "aac" "MiauPlayer.Audio"
  !insertmacro RegExt "m4a" "MiauPlayer.Audio"
  !insertmacro RegExt "m4b" "MiauPlayer.Audio"
  !insertmacro RegExt "flac" "MiauPlayer.Audio"
  !insertmacro RegExt "ogg" "MiauPlayer.Audio"
  !insertmacro RegExt "oga" "MiauPlayer.Audio"
  !insertmacro RegExt "opus" "MiauPlayer.Audio"
  !insertmacro RegExt "wav" "MiauPlayer.Audio"
  !insertmacro RegExt "aif" "MiauPlayer.Audio"
  !insertmacro RegExt "aiff" "MiauPlayer.Audio"
  !insertmacro RegExt "aifc" "MiauPlayer.Audio"
  !insertmacro RegExt "ape" "MiauPlayer.Audio"
  !insertmacro RegExt "tta" "MiauPlayer.Audio"
  !insertmacro RegExt "wv" "MiauPlayer.Audio"
  !insertmacro RegExt "mpc" "MiauPlayer.Audio"
  !insertmacro RegExt "wma" "MiauPlayer.Audio"
  !insertmacro RegExt "ac3" "MiauPlayer.Audio"
  !insertmacro RegExt "eac3" "MiauPlayer.Audio"
  !insertmacro RegExt "dts" "MiauPlayer.Audio"
  !insertmacro RegExt "mka" "MiauPlayer.Audio"
  !insertmacro RegExt "caf" "MiauPlayer.Audio"
  !insertmacro RegExt "dsf" "MiauPlayer.Audio"
  !insertmacro RegExt "dff" "MiauPlayer.Audio"
  !insertmacro RegExt "amr" "MiauPlayer.Audio"
  !insertmacro RegExt "au" "MiauPlayer.Audio"
  !insertmacro RegExt "w64" "MiauPlayer.Audio"
  !insertmacro RegExt "wave" "MiauPlayer.Audio"
  !insertmacro RegExt "mp4" "MiauPlayer.Video"
  !insertmacro RegExt "mov" "MiauPlayer.Video"
  !insertmacro RegExt "mkv" "MiauPlayer.Video"
  !insertmacro RegExt "wmv" "MiauPlayer.Video"
  !insertmacro RegExt "asf" "MiauPlayer.Video"
  !insertmacro RegExt "avi" "MiauPlayer.Video"
  !insertmacro RegExt "webm" "MiauPlayer.Video"
  !insertmacro RegExt "mpg" "MiauPlayer.Video"
  !insertmacro RegExt "mpeg" "MiauPlayer.Video"
  !insertmacro RegExt "m4v" "MiauPlayer.Video"
  !insertmacro RegExt "flv" "MiauPlayer.Video"
  !insertmacro RegExt "3gp" "MiauPlayer.Video"
  !insertmacro RegExt "ogv" "MiauPlayer.Video"
  !insertmacro RegExt "vob" "MiauPlayer.Video"
  !insertmacro RegExt "m2ts" "MiauPlayer.Video"
  !insertmacro RegExt "mts" "MiauPlayer.Video"
  !insertmacro RegExt "divx" "MiauPlayer.Video"
  !insertmacro RegExt "rm" "MiauPlayer.Video"
  !insertmacro RegExt "rmvb" "MiauPlayer.Video"
  ; tambien para cualquier tipo que Windows clasifique como audio o video
  WriteRegStr HKLM "Software\Classes\SystemFileAssociations\audio\shell\MiauPlayer.Add" "" "$(LS_ADDLIST)"
  WriteRegStr HKLM "Software\Classes\SystemFileAssociations\audio\shell\MiauPlayer.Add" "MultiSelectModel" "Player"
  WriteRegStr HKLM "Software\Classes\SystemFileAssociations\audio\shell\MiauPlayer.Add" "Icon" '"$INSTDIR\MiauPlayer.exe",0'
  WriteRegStr HKLM "Software\Classes\SystemFileAssociations\audio\shell\MiauPlayer.Add\command" "" '"$INSTDIR\MiauPlayer.exe" /add "%1"'
  WriteRegStr HKLM "Software\Classes\SystemFileAssociations\video\shell\MiauPlayer.Add" "" "$(LS_ADDLIST)"
  WriteRegStr HKLM "Software\Classes\SystemFileAssociations\video\shell\MiauPlayer.Add" "MultiSelectModel" "Player"
  WriteRegStr HKLM "Software\Classes\SystemFileAssociations\video\shell\MiauPlayer.Add" "Icon" '"$INSTDIR\MiauPlayer.exe",0'
  WriteRegStr HKLM "Software\Classes\SystemFileAssociations\video\shell\MiauPlayer.Add\command" "" '"$INSTDIR\MiauPlayer.exe" /add "%1"'
  ; asociaciones elegidas
  ${If} $S0 == 1
    !insertmacro AssocExt "mp3" "MiauPlayer.Audio"
  ${EndIf}
  ${If} $S1 == 1
    !insertmacro AssocExt "mp4" "MiauPlayer.Video"
  ${EndIf}
  ${If} $S2 == 1
    !insertmacro AssocExt "ogg" "MiauPlayer.Audio"
  ${EndIf}
  ${If} $S3 == 1
    !insertmacro AssocExt "wma" "MiauPlayer.Audio"
  ${EndIf}
  ${If} $S4 == 1
    !insertmacro AssocExt "wmv" "MiauPlayer.Video"
  ${EndIf}
  ${If} $S5 == 1
    !insertmacro AssocExt "aac" "MiauPlayer.Audio"
  ${EndIf}
  ${If} $S6 == 1
    !insertmacro AssocExt "m4a" "MiauPlayer.Audio"
  ${EndIf}
  ${If} $S7 == 1
    !insertmacro AssocExt "mpg" "MiauPlayer.Video"
  ${EndIf}
  ${If} $S8 == 1
    !insertmacro AssocExt "mov" "MiauPlayer.Video"
  ${EndIf}
  ${If} $S9 == 1
    !insertmacro AssocExt "mkv" "MiauPlayer.Video"
  ${EndIf}
  ${IfNot} ${Silent}
    WriteRegDWORD HKLM "Software\MiauPlayer" "AssocAsked" 1
  ${EndIf}
  System::Call 'shell32::SHChangeNotify(i 0x08000000, i 0, p 0, p 0)'
SectionEnd

Section "Uninstall"
  SetShellVarContext all
  SetRegView 64
  Call un.CloseRunning
  !insertmacro UnExt "mp3" "MiauPlayer.Audio"
  !insertmacro UnExt "mp2" "MiauPlayer.Audio"
  !insertmacro UnExt "mp1" "MiauPlayer.Audio"
  !insertmacro UnExt "aac" "MiauPlayer.Audio"
  !insertmacro UnExt "m4a" "MiauPlayer.Audio"
  !insertmacro UnExt "m4b" "MiauPlayer.Audio"
  !insertmacro UnExt "flac" "MiauPlayer.Audio"
  !insertmacro UnExt "ogg" "MiauPlayer.Audio"
  !insertmacro UnExt "oga" "MiauPlayer.Audio"
  !insertmacro UnExt "opus" "MiauPlayer.Audio"
  !insertmacro UnExt "wav" "MiauPlayer.Audio"
  !insertmacro UnExt "aif" "MiauPlayer.Audio"
  !insertmacro UnExt "aiff" "MiauPlayer.Audio"
  !insertmacro UnExt "aifc" "MiauPlayer.Audio"
  !insertmacro UnExt "ape" "MiauPlayer.Audio"
  !insertmacro UnExt "tta" "MiauPlayer.Audio"
  !insertmacro UnExt "wv" "MiauPlayer.Audio"
  !insertmacro UnExt "mpc" "MiauPlayer.Audio"
  !insertmacro UnExt "wma" "MiauPlayer.Audio"
  !insertmacro UnExt "ac3" "MiauPlayer.Audio"
  !insertmacro UnExt "eac3" "MiauPlayer.Audio"
  !insertmacro UnExt "dts" "MiauPlayer.Audio"
  !insertmacro UnExt "mka" "MiauPlayer.Audio"
  !insertmacro UnExt "caf" "MiauPlayer.Audio"
  !insertmacro UnExt "dsf" "MiauPlayer.Audio"
  !insertmacro UnExt "dff" "MiauPlayer.Audio"
  !insertmacro UnExt "amr" "MiauPlayer.Audio"
  !insertmacro UnExt "au" "MiauPlayer.Audio"
  !insertmacro UnExt "w64" "MiauPlayer.Audio"
  !insertmacro UnExt "wave" "MiauPlayer.Audio"
  !insertmacro UnExt "mp4" "MiauPlayer.Video"
  !insertmacro UnExt "mov" "MiauPlayer.Video"
  !insertmacro UnExt "mkv" "MiauPlayer.Video"
  !insertmacro UnExt "wmv" "MiauPlayer.Video"
  !insertmacro UnExt "asf" "MiauPlayer.Video"
  !insertmacro UnExt "avi" "MiauPlayer.Video"
  !insertmacro UnExt "webm" "MiauPlayer.Video"
  !insertmacro UnExt "mpg" "MiauPlayer.Video"
  !insertmacro UnExt "mpeg" "MiauPlayer.Video"
  !insertmacro UnExt "m4v" "MiauPlayer.Video"
  !insertmacro UnExt "flv" "MiauPlayer.Video"
  !insertmacro UnExt "3gp" "MiauPlayer.Video"
  !insertmacro UnExt "ogv" "MiauPlayer.Video"
  !insertmacro UnExt "vob" "MiauPlayer.Video"
  !insertmacro UnExt "m2ts" "MiauPlayer.Video"
  !insertmacro UnExt "mts" "MiauPlayer.Video"
  !insertmacro UnExt "divx" "MiauPlayer.Video"
  !insertmacro UnExt "rm" "MiauPlayer.Video"
  !insertmacro UnExt "rmvb" "MiauPlayer.Video"
  DeleteRegKey HKLM "Software\Classes\SystemFileAssociations\audio\shell\MiauPlayer.Add"
  DeleteRegKey HKLM "Software\Classes\SystemFileAssociations\video\shell\MiauPlayer.Add"
  DeleteRegKey HKLM "Software\Classes\MiauPlayer.Audio"
  DeleteRegKey HKLM "Software\Classes\MiauPlayer.Video"
  DeleteRegKey HKLM "Software\Classes\Applications\MiauPlayer.exe"
  DeleteRegValue HKLM "Software\RegisteredApplications" "MiauPlayer"
  DeleteRegKey HKLM "Software\MiauPlayer"
  ; registro por usuario creado por la app (Configuracion > Asociar tipos de archivo)
  DeleteRegValue HKCU "Software\Classes\.mp3\OpenWithProgids" "MiauPlayer.Audio"
  DeleteRegValue HKCU "Software\Classes\.mp4\OpenWithProgids" "MiauPlayer.Video"
  DeleteRegValue HKCU "Software\Classes\.ogg\OpenWithProgids" "MiauPlayer.Audio"
  DeleteRegValue HKCU "Software\Classes\.wma\OpenWithProgids" "MiauPlayer.Audio"
  DeleteRegValue HKCU "Software\Classes\.wmv\OpenWithProgids" "MiauPlayer.Video"
  DeleteRegValue HKCU "Software\Classes\.aac\OpenWithProgids" "MiauPlayer.Audio"
  DeleteRegValue HKCU "Software\Classes\.m4a\OpenWithProgids" "MiauPlayer.Audio"
  DeleteRegValue HKCU "Software\Classes\.mpg\OpenWithProgids" "MiauPlayer.Video"
  DeleteRegValue HKCU "Software\Classes\.mov\OpenWithProgids" "MiauPlayer.Video"
  DeleteRegValue HKCU "Software\Classes\.mkv\OpenWithProgids" "MiauPlayer.Video"
  DeleteRegKey HKCU "Software\Classes\MiauPlayer.Audio"
  DeleteRegKey HKCU "Software\Classes\MiauPlayer.Video"
  DeleteRegKey HKCU "Software\MiauPlayer\Capabilities"
  DeleteRegValue HKCU "Software\RegisteredApplications" "MiauPlayer"
  DeleteRegKey HKLM "${UNKEY}"
  Delete "$SMPROGRAMS\MiauPlayer.lnk"
  Delete "$DESKTOP\MiauPlayer.lnk"
  Delete "$INSTDIR\MiauPlayer.exe"
  Delete "$INSTDIR\*.dll"
  Delete "$INSTDIR\FFMPEG-LICENSE.txt"
  Delete "$INSTDIR\LEEME.txt"
  Delete "$INSTDIR\Uninstall.exe"
  IfFileExists "$INSTDIR\playlist\*.txt" 0 +3
  MessageBox MB_YESNO|MB_ICONQUESTION "$(LS_DELPL)" /SD IDNO IDNO +2
  RMDir /r "$INSTDIR\playlist"
  RMDir "$INSTDIR\playlist"
  RMDir "$INSTDIR"
  System::Call 'shell32::SHChangeNotify(i 0x08000000, i 0, p 0, p 0)'
SectionEnd
