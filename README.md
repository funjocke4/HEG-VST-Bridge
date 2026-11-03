# HEG 32/64-Bit VST Bridge

**Version 0.1.0 – Windows 11, experimenteller Quellcode / IPC-Prototyp.**

Ziel: Alte, rechtmäßig vorhandene **32-Bit-VST2-DLL-Plugins** über einen eigenständigen
32-Bit-Prozess in einem späteren **64-Bit-VST3-Wrapper** in Cubase Artist 15 einsetzen.

## Architektur

```text
Cubase Artist 15 (64 Bit)
        |
        v
[HEG Bridge.vst3 (64 Bit)]  <-- noch zu entwickeln
        |
        | IPC: versioniertes Named-Pipe-Protokoll
        v
[HEGWorker32.exe (32 Bit)]
        |
        | VST2 ABI / LoadLibraryW
        v
[altes VST2-Plugin.dll (32 Bit)]
```

Der vorliegende Stand implementiert den **32-Bit-Worker** plus ein **64-Bit-Konsolen-Testprogramm**.
Es ist **noch kein VST3-Plugin** und kann somit **noch nicht in Cubase geladen werden**.

## Voraussetzungen

- Windows 11 x64 (Entwicklung/Test)
- Visual Studio 2022 Community mit „Desktopentwicklung mit C++“
- CMake 3.21 oder neuer
- Eigene, vertrauenswürdige 32-Bit-VST2-DLL (Effekt mit `processReplacing`, 1–2 Outputs)

## Build

Im Projektordner in PowerShell:

```powershell
.\build-windows.ps1
```

Oder manuell:

```powershell
cmake -S . -B build-x86 -G "Visual Studio 17 2022" -A Win32
cmake --build build-x86 --config Release
cmake -S . -B build-x64 -G "Visual Studio 17 2022" -A x64
cmake --build build-x64 --config Release
```

## Erster Test

Zwei PowerShell-Fenster öffnen:

Fenster 1:
```powershell
.\build-x86\Release\HEGWorker32.exe
```

Fenster 2:
```powershell
.\build-x64\Release\HEGController64.exe "C:\VST32\MeinAlterEffekt.dll"
```

Der Controller sendet ein synthetisches Stereo-Audiosignal (256 Samples) an den Worker und
zeigt den Status an. **Keine Tonwiedergabe und noch keine Cubase-Integration.**

## Funktionsumfang 0.1.0

- Minimaler Legacy-VST2-ABI-Adapter für ein 32-Bit-Effekt-Plugin
- Separater 32-Bit-Prozess, kontrolliert von einem x64-Controller
- Versioniertes IPC-Protokoll über Windows Named Pipes
- Load / Configure / Process / Unload / Quit
- Stereo-`float32`-Blöcke mit höchstens 512 Frames

## Grenzen & Sicherheit

- Einfache **synchrone** IPC-Kommunikation: **nicht echtzeitfähig**. Keinesfalls als fertige DAW-Audiobridge verwenden.
- Kein MIDI, keine VST-Instrumente, keine Plugin-GUI, keine Host-Automation, keine Zustandsverwaltung,
  keine Latenzkompensation oder Absturz-Wiederherstellung.
- Alte VST2-DLLs können beim Laden/Verarbeiten abstürzen; der Worker isoliert sie vom künftigen DAW-Prozess.
- Worker wird bewusst manuell gestartet, nicht automatisch mit Benutzerrechten für beliebige Pfade.
- Die Pipe ist zurzeit ohne benutzerdefinierte Zugriffssteuerung. Nur in einer vertrauenswürdigen Testumgebung verwenden.
- Der Worker lädt und **führt Code aus der angegebenen DLL aus**. Keine DLL unbekannter Herkunft testen.
- Nur Effekte, die die vereinfachte ABI und `processReplacing` tatsächlich unterstützen.
- `VSTPluginMain` und `main` werden geprüft. Einige alte Plugins benutzen abweichende Mechanismen.
- Das `AEffect`-Layout wurde für MSVC/Win32 vorbereitet, aber mangels Windows-Build **noch nicht validiert**.
- Fehlende Windows-Tests: der Code wurde bereitgestellt, aber hier **nicht kompiliert oder ausgeführt**.
- Keine fremden proprietären Plugins oder Steinberg-VST2-SDK-Dateien werden ausgeliefert.

## Nächste Entwicklungsphasen

1. x86/x64 unter Windows 11 kompilieren; echte alte Test-DLL prüfen und ABI/Host-Callback ergänzen.
2. `HEGBridge.vst3` aus dem offiziellen, MIT-lizenzierten Steinberg VST3-SDK
   (https://github.com/steinbergmedia/vst3sdk) erstellen und im VST3-Validator testen.
3. Pro Instanz eindeutige Pipe + Child-Prozess, Shared-Memory-Audio-Ringpuffer,
   lock-free/one-block pipelining und sauber gemeldete Latenz.
4. MIDI-Events, Instrumente, Parameter, Presets/Chunks und GUI-Fenster-Bridging.
5. Fehlerbehandlung, Plugin-Scanner, Plugin-Kompatibilitätsliste, Installer und Release.

## Kompatibilität

Cubase 15: VST2 kann optional aktiviert werden, aber nur unterstützte 64-Bit-VST2-Plugins.
Der geplante **VST3-Wrapper** erfordert daher *keine* VST2-Freischaltung in Cubase.
Siehe: https://helpcenter.steinberg.de/hc/de/articles/22554401894162-Verwendung-von-VST-2-Plug-ins-in-Cubase-Nuendo-14-und-15

## Lizenz

Projektcode MIT. Steinberg VST3 SDK ist separat unter MIT erhältlich; Copyright-Hinweise
beachten. VST2 SDK wird nicht mitgeliefert. Die Nutzung fremder Plugins unterliegt deren Lizenzen.

## Experimental: Windows-EXE auf Linux/openSUSE bauen (v0.1.1)

Benötigt CMake und die Cross-Compiler `i686-w64-mingw32-g++` und `x86_64-w64-mingw32-g++` aus den openSUSE-MinGW-Paketen.

```bash
bash build-linux-to-windows.sh
```

Die Pfade im Skript sind relativ zum Projekt. Dies ist ein **ungeprüfter Cross-Build-Pfad**, keine getestete Windows-Freigabe. Die finale Cubase-VST3-Integration fehlt weiterhin.
