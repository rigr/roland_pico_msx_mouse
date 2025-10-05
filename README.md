# 🖱️ Roland S-750 USB-Maus-Adapter (Raspberry Pi Pico)

Dieses Projekt erlaubt es, eine moderne USB-Maus an den Roland S-750 Sampler anzuschließen.  
Der Raspberry Pi Pico agiert als USB-Host und emuliert das originale MU-1-Mausprotokoll.

## 🔧 Funktionen
- Unterstützt USB-Kabelmäuse und Funkmäuse (mit Dongle)
- Roland-kompatibles 4-Bit-Datenprotokoll (MSX-Mausstandard)
- Versorgung aus dem Roland S-750 (über +5V, Pin 5)
- Kompatibel mit jedem TinyUSB-tauglichen Pico-SDK

## 🧱 Aufbau
Siehe `main.cpp` für Pinbelegung und Anschlussplan.

## 🚀 Build auf GitHub
1. Fork dieses Repos oder lade es hoch.
2. Jeder Commit startet automatisch den Build.
3. Nach Abschluss unter “Actions → Build Pico Roland Mouse → Artifacts” die `.uf2` herunterladen.
4. Pico im BOOTSEL-Modus starten und `.uf2` kopieren.
