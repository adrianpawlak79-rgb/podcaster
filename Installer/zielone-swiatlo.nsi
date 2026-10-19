; Instalator "Zielone swiatlo" (NSIS 3, budowany na Linuksie: makensis).
;
; Audyt 14.10: "niezawodny instalator z modelem" przed 1.0. Program szuka
; libvosk.dll i modelu vosk-model-small-pl-0.22 OBOK .exe (VoskEngine.h) —
; bez nich prompter jedzie na liczniku sylab. Dotad paczki mialy sam .exe.
;
; Instalacja do folderu uzytkownika, bez uprawnien administratora (bez okna
; UAC — dla poczatkujacego to "czy na pewno?" bez odpowiedzi). Nagrania i
; ustawienia leza gdzie indziej (Muzyka, AppData) — odinstalowanie ich nie
; rusza.
;
; Pliki skladane przez Tools/instalator.sh do katalogu ZRODLO:
;   Zielone swiatlo.exe, libvosk.dll + 3 DLL MinGW, vosk-model-small-pl-0.22\
;
;   makensis -DZRODLO=... -DWERSJA=0.4.0 -DWYNIK=... Installer/zielone-swiatlo.nsi

Unicode true
SetCompressor /SOLID lzma

!ifndef ZRODLO
  !error "Podaj -DZRODLO=katalog z plikami (Tools/instalator.sh)"
!endif
!ifndef WERSJA
  !define WERSJA "0.0.0"
!endif
!ifndef WYNIK
  !define WYNIK "Zielone-swiatlo-${WERSJA}-instalator.exe"
!endif

!define NAZWA "Zielone światło"
!define KLUCZ "Software\Microsoft\Windows\CurrentVersion\Uninstall\ZieloneSwiatlo"

Name "${NAZWA}"
OutFile "${WYNIK}"
InstallDir "$LOCALAPPDATA\Programs\Zielone swiatlo"
InstallDirRegKey HKCU "Software\ZieloneSwiatlo" "Folder"
RequestExecutionLevel user
BrandingText "${NAZWA} ${WERSJA}"

!include "MUI2.nsh"

!define MUI_ABORTWARNING
!define MUI_WELCOMEPAGE_TITLE "Instalacja programu ${NAZWA}"
!define MUI_WELCOMEPAGE_TEXT "Program do nagrywania głosu: podcastów, audiobooków i lektorki.$\r$\n$\r$\nInstalacja nie wymaga uprawnień administratora. Twoje nagrania i ustawienia nie są ruszane — także przy aktualizacji i odinstalowaniu."
!insertmacro MUI_PAGE_WELCOME
!insertmacro MUI_PAGE_DIRECTORY
!insertmacro MUI_PAGE_INSTFILES
!define MUI_FINISHPAGE_RUN "$INSTDIR\Zielone swiatlo.exe"
!define MUI_FINISHPAGE_RUN_TEXT "Uruchom ${NAZWA}"
!insertmacro MUI_PAGE_FINISH

!insertmacro MUI_UNPAGE_CONFIRM
!insertmacro MUI_UNPAGE_INSTFILES

!insertmacro MUI_LANGUAGE "Polish"

VIProductVersion "${WERSJA}.0"
VIAddVersionKey /LANG=${LANG_POLISH} "ProductName" "${NAZWA}"
VIAddVersionKey /LANG=${LANG_POLISH} "FileDescription" "Instalator ${NAZWA}"
VIAddVersionKey /LANG=${LANG_POLISH} "FileVersion" "${WERSJA}"
VIAddVersionKey /LANG=${LANG_POLISH} "ProductVersion" "${WERSJA}"
VIAddVersionKey /LANG=${LANG_POLISH} "LegalCopyright" "Adrian Pawlak"

Section "Program" SecProgram
  SectionIn RO
  SetOutPath "$INSTDIR"

  ; Aktualizacja w trakcie dzialania programu: plik .exe jest zablokowany.
  ; Zamiast tajemniczego bledu zapisu — jedno zdanie i ponowna proba.
  retry:
  ClearErrors
  File "${ZRODLO}\Zielone swiatlo.exe"
  IfErrors 0 +3
    MessageBox MB_RETRYCANCEL|MB_ICONEXCLAMATION "Zamknij program ${NAZWA} i kliknij Ponów." IDRETRY retry
    Abort

  ; rozpoznawanie mowy (VoskEngine.h): biblioteka i model obok programu
  File "${ZRODLO}\libvosk.dll"
  File "${ZRODLO}\libgcc_s_seh-1.dll"
  File "${ZRODLO}\libstdc++-6.dll"
  File "${ZRODLO}\libwinpthread-1.dll"
  File /r "${ZRODLO}\vosk-model-small-pl-0.22"
  File "${ZRODLO}\LICENCJE.txt"

  CreateDirectory "$SMPROGRAMS\${NAZWA}"
  CreateShortcut "$SMPROGRAMS\${NAZWA}\${NAZWA}.lnk" "$INSTDIR\Zielone swiatlo.exe"
  CreateShortcut "$SMPROGRAMS\${NAZWA}\Odinstaluj.lnk" "$INSTDIR\Odinstaluj.exe"
  CreateShortcut "$DESKTOP\${NAZWA}.lnk" "$INSTDIR\Zielone swiatlo.exe"

  WriteUninstaller "$INSTDIR\Odinstaluj.exe"
  WriteRegStr HKCU "Software\ZieloneSwiatlo" "Folder" "$INSTDIR"
  WriteRegStr HKCU "${KLUCZ}" "DisplayName" "${NAZWA}"
  WriteRegStr HKCU "${KLUCZ}" "DisplayVersion" "${WERSJA}"
  WriteRegStr HKCU "${KLUCZ}" "Publisher" "Adrian Pawlak"
  WriteRegStr HKCU "${KLUCZ}" "DisplayIcon" "$INSTDIR\Zielone swiatlo.exe"
  WriteRegStr HKCU "${KLUCZ}" "InstallLocation" "$INSTDIR"
  WriteRegStr HKCU "${KLUCZ}" "UninstallString" '"$INSTDIR\Odinstaluj.exe"'
  WriteRegDWORD HKCU "${KLUCZ}" "NoModify" 1
  WriteRegDWORD HKCU "${KLUCZ}" "NoRepair" 1
SectionEnd

Section "Uninstall"
  ; tylko to, co instalator polozyl — nagrania (Muzyka) i ustawienia zostaja
  Delete "$INSTDIR\Zielone swiatlo.exe"
  Delete "$INSTDIR\libvosk.dll"
  Delete "$INSTDIR\libgcc_s_seh-1.dll"
  Delete "$INSTDIR\libstdc++-6.dll"
  Delete "$INSTDIR\libwinpthread-1.dll"
  Delete "$INSTDIR\LICENCJE.txt"
  RMDir /r "$INSTDIR\vosk-model-small-pl-0.22"
  Delete "$INSTDIR\Odinstaluj.exe"
  RMDir "$INSTDIR"

  Delete "$SMPROGRAMS\${NAZWA}\${NAZWA}.lnk"
  Delete "$SMPROGRAMS\${NAZWA}\Odinstaluj.lnk"
  RMDir "$SMPROGRAMS\${NAZWA}"
  Delete "$DESKTOP\${NAZWA}.lnk"

  DeleteRegKey HKCU "${KLUCZ}"
  DeleteRegKey HKCU "Software\ZieloneSwiatlo"
SectionEnd
