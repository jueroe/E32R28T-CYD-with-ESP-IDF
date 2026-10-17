# E32R28T CYD – Raumthermometer, -hygrometer und Luftdruckmesser

ESP-IDF-Projekt auf Basis des "Cheap Yellow Display"
(QDtech / LCDWiki **2.8inch ESP32-32E Display**, SKU **E32R28T**).

Ursprünglich als reiner Display-/Touch-Test entstanden; das Display, der Touch,
die Backlight-Dimmung und die RGB-LED sind inzwischen auf diesem Gerät
verifiziert. Das Projekt wird zum Raumthermometer ausgebaut.

## Zielbild der Anwendung

| Funktion | Vorgabe |
|---|---|
| Sensor | BME280 (Temperatur, relative Luftfeuchte, Luftdruck) über I²C – **läuft** |
| Uhrzeit | NTP, Anzeigeblock `hh:mm:ss` + Wochentag (Mo./Di. …) + `dd.mm.yy`, sekundengenau |
| Temperatur | große Ziffern für die ganzen °C, Komma und eine Nachkommastelle, das `°` nur ein Viertel so groß |
| Luftfeuchte | darunter, etwa halb so große Schrift, Format `xx,x %rF` |
| Hintergrund | JPEG-Wallpapers nach Jahreszeit (Frühling/Sommer/Herbst/Winter) und Tageszeit (Morgen/Tag/Abend/Nacht), je Slot 3 Bilder zufällig, insgesamt 48 |
| Schnittstelle | HTTP-Endpunkt, der Temperatur und Luftfeuchte als JSON liefert (später Abruf durch eine Wetterstation) |
| Ausrichtung | Hochformat, USB-Anschluss unten |

Die Wallpaper liegen am PC unter `backgrounds/<jahreszeit>/<tageszeit>/`, je drei
JPEG-Dateien pro Ordner (zusammen 715 KB). Sie kommen in die
littlefs-Partition `storage`, siehe [Flash-Aufteilung](#flash-aufteilung).

## Hardware

| Baustein | Details |
|---|---|
| Modul | ESP32-32E, ESP32-D0WD-V3, 240 MHz, 520 KB SRAM, kein PSRAM, 4 MB QSPI-Flash |
| Display | 2.8" TN-TFT, 240×320, **ILI9341V**, 4-Draht-SPI, Weiß-LED-Backlight an GPIO21 |
| Touch | **XPT2046** (resistiv) an eigenem SPI-Bus, PENIRQ an GPIO36 |
| Sonstiges | RGB-LED (R/G/B = 22/16/17, Low-aktiv), BOOT-Taster an GPIO0, microSD, Lautsprecher |

Alle verwendeten Pins stehen gesammelt in
[board_e32r28t.h](main/board_e32r28t.h); Quelle sind die Pin-Tabelle der
LCDWiki-Produktseite (https://www.lcdwiki.com/2.8inch_ESP32-32E_Display) und die
Gerätebeschreibung `cyd-e32r28t` des Tactility-Projekts.

| Funktion | GPIO |
|---|---|
| LCD SCLK / MOSI / MISO | 14 / 13 / 12 |
| LCD CS / DC | 15 / 2 |
| LCD Reset | an EN des ESP32 (deshalb `reset_gpio_num = -1`, Software-Reset) |
| LCD Backlight | 21 (High = ein) |
| Touch SCLK / MOSI / MISO / CS / IRQ | 25 / 32 / 39 / 33 / 36 |
| RGB-LED R / G / B | 22 / 16 / 17 (Low = ein) |
| BOOT-Taste | 0 |
| microSD (nicht genutzt) | CS 5, SCK 18, MISO 19, MOSI 23 |
| Audio (nicht genutzt) | Enable 4 (Low = ein), Ausgang 26 |
| BME280 (I²C) | **SDA = 23, SCL = 27** (Stecker P3), 3V3/GND von P4 |

### Stecker am Board

| Stecker | Belegung | Nutzung hier |
|---|---|---|
| P4 „Expand Pin" (4-polig) | 1 = 3V3, 2 = IO35, 3 = **NC**, 4 = GND | 3V3 + GND für den Sensor |
| P3 „SPI peripheral" (4-polig) | 1 = IO23, 2 = IO19, 3 = IO18, 4 = IO27, 6 = GND | SDA = IO23, SCL = IO27 |

Aus dem Originalschaltplan (Rev. V1.0): Pin 3 von P4 ist wirklich nicht
verbunden, könnte also als Durchführung für ein zusätzliches Signal dienen.
**IO35 (P4 Pin 2) ist aber unbrauchbar für I²C**: GPIO34…39 sind reine
Eingänge ohne Ausgangstreiber und ohne interne Pull-ups – I²C braucht auf SDA
und SCL beide Richtungen. Deshalb liegen beide Leitungen auf P3. Nebenwirkung:
IO18/19/23/27 sind die microSD-Leitungen, der SD-Slot ist damit nicht mehr
nutzbar.

## Flash-Aufteilung

4 MB Flash, custom Partitionstabelle in [partitions.csv](partitions.csv)
(`CONFIG_PARTITION_TABLE_CUSTOM=y`):

| Partition | Typ | Offset | Größe | Inhalt |
|---|---|---|---|---|
| (bootloader) | – | 0x1000 | 32 KB | vom Bootloader belegt |
| (Partitionstabelle) | – | 0x8000 | 4 KB | diese Tabelle |
| `nvs` | data/nvs | 0x9000 | 24 KB | Einstellungen, WLAN-Kalibrierung |
| `phy_init` | data/phy | 0xF000 | 4 KB | RF-Kalibrierdaten |
| `factory` | app/factory | 0x10000 | **3008 KB** | Anwendung |
| `storage` | data/spiffs | 0x300000 | **1 MB** | littlefs: 48 Wallpaper, später Logs |

Bewusste Entscheidungen:

* **3 MB App**: WiFi, NTP, HTTP-Server, LVGL und JPEG-Decoder brauchen deutlich
  mehr als die 1 MB der Standardtabelle (aktueller Teststand: 192 KB, also 94 %
  der Partition frei).
* **1 MB littlefs** ist auf diesem Gerät bereits voll: die 48 vorhandenen
  Wallpaper belegen 992 KiB inklusive littlefs-Overhead (Details unter
  [Wallpaper-Ablage](#wallpaper-ablage)). Für mehr Bilder die App-Partition
  verkleinern (z. B. 2 MB App + 1,94 MB `storage`).
* **kein OTA-Slot**: Für zwei App-Images à 3 MB ist kein Platz. Updates laufen
  über USB.
* Der Subtyp der littlefs-Partition ist `spiffs` (generischer Datentyp) – das
  erwartet die Komponente `joltwallet/littlefs`. Die Partition wird beim ersten
  Mount automatisch formatiert (`format_if_mount_failed`).

Prüfen lässt sich die Tabelle ohne Hardware:

```powershell
.\idf partition-table        # erzeugt build/partition_table/partition-table.bin und zeigt sie an
```

Nach einer Änderung der Tabelle einmal `.\idf flash` ausführen – die Tabelle
wird dabei an 0x8000 mitgeschrieben. Ein `erase-flash` ist nicht nötig, solange
keine Partition ihre Lage ändert; die neue `storage`-Partition liegt in einem
Bereich, der vorher unbenutzt war.

## Was das Testprogramm macht

1. Backlight aus, RGB-LED initialisieren, danach **Selbsttest der LED**: rot,
   grün, blau, weiß nacheinander für je 400 ms (steht so im Log), BOOT-Taste als
   Eingang konfigurieren
2. ILI9341 über SPI2 hochfahren (40 MHz) und in den Querformat-Modus 320×240 bringen
   (ESP-IDF 6 nennt die beiden freien Busse SPI1/SPI2; die Header verwenden die
   weiterhin gültigen Aliase `SPI2_HOST`/`SPI3_HOST`, im Monitor erscheint
   deshalb „SPI1" für das Display und „SPI2" für den Touch)
3. Testbild aus einem kompletten Framebuffer (150 KB) in Streifen auf das Display schreiben
4. Backlight auf 100 % dimmen (LEDC-PWM, 20 kHz)
5. XPT2046 über SPI3 initialisieren; schlägt das fehl, läuft das Programm ohne Touch weiter
6. littlefs-Partition `storage` einhängen (beim ersten Mal formatieren), Belegung
   und Dateianzahl loggen
7. BME280 über I²C einlesen (Adresse 0x76, 16× Oversampling); der Init kehrt
   erst zurück, wenn eine vollständige Messung vorliegt
8. Endlosschleife:
   * Statuszeile mit Laufzeit, Frames/s und freiem Heap aktualisieren
   * alle 2 s eine Messung loggen: `BME280: 23.97 C, 45.8 %rH, 965.5 hPa`
   * grüner Balken wandert alle 5 s einmal über die Breite (Animation/Bandbreite sichtbar)
   * beim Berühren: magenta Punkt an die Tippstelle malen, Roh- und Bildschirmwerte loggen
   * **BOOT** drücken: Testbild neu zeichnen (löscht alle Punkte)
   * LED: grün = Programm läuft, blau = solange neu gezeichnet wird

### Bildschirmaufbau (320×240 Querformat)

| Bereich | Inhalt |
|---|---|
| y 0–19 | Titelbalken: Boardname, ESP-IDF-Version |
| y 24–119 | Farbreihen: Weiß, Gelb, Cyan, Grün, Magenta, Rot, Blau, Schwarz |
| y 124–147 | Verläufe: Schwarz→Weiß, Schwarz→Rot, Schwarz→Blau |
| y 152–199 | Geometrie: Rahmen, Diagonalen, Kreis, Text „GEOMETRY / 1PX" |
| y 204–239 | Statuszeile und laufender Balken |

## Wallpaper-Ablage

Die JPEGs liegen im Projektordner [data/](data). Genau dieser Ordner wird beim
Bauen in ein littlefs-Image gepackt und mit `flash` in die Partition `storage`
geschrieben. Kopiere deine Verzeichnisstruktur also so hinein:

```
E32R28T_erste_Tests\
└─ data\                        <- Inhalt = Wurzel des Dateisystems
   └─ backgrounds\
      ├─ spring\  ├─ morning\   1.jpg  2.jpg  3.jpg
      │           ├─ day\       …
      │           ├─ evening\   …
      │           └─ night\     …
      ├─ summer\  … wie spring
      ├─ autumn\  … wie spring
      └─ winter\  … wie spring
```

Damit ergeben sich auf dem Gerät diese Pfade (Präfix `STORAGE_BASE_PATH` aus
[storage.h](main/storage.h)):

```
/littlefs/backgrounds/spring/morning/1.jpg
/littlefs/backgrounds/summer/day/2.jpg
…
```

Also einfach den kompletten Ordner `backgrounds` nach `data\` kopieren:

```powershell
Copy-Item -Recurse "C:\...\backgrounds" "C:\Users\joe\Documents\ESP-IDF\Projects\E32R28T_erste_Tests\data\"
```

Dann bauen und flashen – das Image wird automatisch neu erzeugt und ab 0x300000
geschrieben:

```powershell
.\idf -p COM8 flash monitor
```

Erwartete Ausgabe (Ist-Stand mit 48 Dateien):

```
I (2584) storage: littlefs mounted at /littlefs: 1024 KiB total, 32 KiB free
I (2634) storage: /littlefs/backgrounds/autumn/day: 3 files, 53200 bytes
I (2674) storage: /littlefs/backgrounds/autumn/evening: 3 files, 40490 bytes
…   (16 Ordnerzeilen, je 3 Dateien)
I (4544) storage: 48 files in total (48 JPEG), 732455 bytes
```

Hinweise:

* Der Image-Build läuft über `littlefs_create_partition_image(storage ../data
  FLASH_IN_PROJECT)` in [main/CMakeLists.txt](main/CMakeLists.txt); dabei wird
  einmalig ein kleines Python-Venv (`build/littlefs_py_venv`) mit
  `littlefs-python` angelegt.
* Das Image ist fest 1 MB groß (`--fs-size=0x100000`, Blockgröße 4096).
* **Platzbedarf (gemessen):** 715 KiB Dateien + 277 KiB littlefs-Overhead =
  992 KiB belegt, 32 KiB frei. Der Overhead kommt von den Metadatenpaaren
  (jedes der 21 Verzeichnisse belegt 2 × 4 KiB = 8 KiB, zusammen 168 KiB) und
  der Blockrundung der Dateien (~109 KiB). Ein viertes Bild pro Slot passt
  **nicht** mehr hinein – dafür müsste `storage` in
  [partitions.csv](partitions.csv) vergrößert und die App-Partition entsprechend
  verkleinert werden (Tabelle danach neu flashen).
* Dateinamen dürfen bis 127 Zeichen lang sein: `CONFIG_LITTLEFS_OBJ_NAME_LEN=128`
  in [sdkconfig.defaults](sdkconfig.defaults), weil der längste vorhandene Name
  73 Zeichen hat (Vorgabe der Komponente ist 64). Der Image-Build übernimmt den
  Wert automatisch als `--name-max`.
* Jedes `flash` schreibt die komplette Partition neu (ca. 6 s), auch wenn sich
  nur die App geändert hat. Wer das nicht will, flasht die App gezielt mit
  `.\idf app-flash` (Tabelle und Image bleiben dann unangetastet).

## Voraussetzungen

* ESP-IDF **v6.1** – auf diesem Rechner installiert unter `C:\esp\v6.1\esp-idf`
  (Espressif Installation Manager, Tools in `C:\Espressif\tools`)
* VS Code mit der ESP-IDF-Erweiterung (`espressif.esp-idf-extension`), Setup
  `v6.1` ist in `.vscode/settings.json` schon ausgewählt
* USB-Datenkabel (Type-C) – das Board wird als USB-Seriell-Adapter erkannt
  (auf diesem Rechner waren zum Zeitpunkt des Erstellens **keine** Ports außer
  zwei Bluetooth-Ports vorhanden, also bitte Board anstecken)

## Bauen, Flashen, Monitor

### In VS Code

1. Ordner `E32R28T_erste_Tests` öffnen (falls nicht schon offen)
2. Unten in der Statusleiste: Ziel `esp32`, seriellen Port auswählen (z. B. COM3)
3. Bausteinsymbol = Build, Blitzsymbol = Flash, Stecker/Seriell-Symbol = Monitor
   (oder „Build, Flash and Monitor" über die Kommandopalette)

### IntelliSense in VS Code

Die Include-Pfade kommen aus `.vscode/c_cpp_properties.json`. Wichtig sind zwei
Felder:

* `compilerPath` – der Xtensa-Compiler, aus dessen eingebautem Suchpfad die
  Systemheader (`stdio.h`, `string.h`, …) kommen. Ohne diesen Eintrag meldet VS
  Code für jede Systemheader-Zeile „cannot open source file ...".
* `compileCommands` – `${workspaceFolder}/build/compile_commands.json`. Daraus
  zieht IntelliSense die rund 95 `-I`-Pfade und alle `-D`-Schalter des echten
  ESP-IDF-Builds, also auch `sdkconfig.h` mit den `CONFIG_*`-Werten. Die Datei
  entsteht beim Build automatisch; **vor dem ersten Build gibt es sie noch
  nicht**.

Wenn die Meldungen nach einem Toolchain-Update wieder auftauchen, zeigt
`compilerPath` auf einen nicht mehr existierenden Pfad – dann in
`.vscode/c_cpp_properties.json` den aktuellen Pfad eintragen (Ordner
`C:\Espressif\tools\xtensa-esp-elf\`) und in VS Code
„C/C++: Reset IntelliSense Database" ausführen.

Alternative: die ESP-IDF-Erweiterung hat in `.vscode/settings.json` schon
Einstellungen für `clangd` hinterlegt (inklusive `--compile-commands-dir`). Wer
statt des C/C++-Parsers lieber clangd nutzt, installiert die Extension
`llvm-vs-code-extensions.vscode-clangd` und setzt
`"C_Cpp.intelliSenseEngine": "disabled"`, damit sich beide nicht in die Quere
kommen.

### Auf der Kommandozeile

`idf.py` liegt bei einer EIM-Installation **nicht** im PATH; es funktioniert nur
in einer Shell, in der das Aktivierungsskript geladen wurde. Dafür gibt es drei
Wege:

**1. Wrapper im Projekt (empfohlen, funktioniert in jedem Fenster)**

```powershell
cd C:\Users\joe\Documents\ESP-IDF\Projects\E32R28T_erste_Tests
.\idf build
.\idf -p COM8 flash monitor     # COMx = Port des Boards
# Monitor beenden: Strg + ]
```

[idf.cmd](idf.cmd) setzt `IDF_PATH`, `IDF_TOOLS_PATH`, `IDF_PYTHON_ENV_PATH`
und den Toolchain-Pfad für diesen Aufruf und reicht alle Argumente an `idf.py`
weiter. Eine `.cmd`-Datei ist von der PowerShell-ExecutionPolicy nicht
betroffen, deshalb funktioniert das auch bei `Restricted` (Standard auf diesem
Rechner). Nach einem Wechsel der ESP-IDF-Version über den EIM müssen die Pfade
oben in der Datei angepasst werden.

**2. ESP-IDF-Umgebung in der aktuellen Sitzung aktivieren**

```powershell
Set-ExecutionPolicy -Scope CurrentUser -ExecutionPolicy RemoteSigned   # einmalig
. C:\Espressif\tools\Microsoft.v6.1.PowerShell_profile.ps1
idf.py build
```

Ohne die Policy-Änderung bricht das Laden des Profils mit
„Die Ausführung von Skripts ist auf diesem System deaktiviert" ab. Danach ist
`idf.py` als Alias verfügbar – allerdings nur in diesem Fenster.

**3. Nur für einen Aufruf, ohne Policy-Änderung**

```powershell
powershell -ExecutionPolicy Bypass -NoExit -Command ". 'C:\Espressif\tools\Microsoft.v6.1.PowerShell_profile.ps1'"
```

Öffnet ein neues Fenster mit vollständiger ESP-IDF-Umgebung.

Der erste Build lädt die verwalteten Komponenten automatisch aus der ESP
Component Registry nach `managed_components/`:

* `espressif/esp_lcd_ili9341` 2.1.0 (ILI9341-Treiber für `esp_lcd`)
* `atanisoft/esp_lcd_touch_xpt2046` 1.0.6 + `espressif/esp_lcd_touch` 1.2.1
* `joltwallet/littlefs` 1.23.0 (Dateisystem der Wallpaper-Partition)
* `espressif/bme280` 0.1.1 + `espressif/i2c_bus` 1.5.2 (Sensor am I²C-Bus; die
  i2c_bus-Version nutzt ab ESP-IDF 5.3 den neuen `i2c_master`-Treiber, der alte
  `driver/i2c.h` existiert in IDF 6 nicht mehr)

Zum Testen ohne Hardware genügt `.\idf build` – es ist bereits erfolgreich
durchgelaufen (App-Binärdatei 192 KB in einer 3008-KB-Partition, Flash 4 MB,
CPU 240 MHz).

## Projektstruktur

| Datei | Zweck |
|---|---|
| [main/main.c](main/main.c) | Ablauf: Backlight, LCD-Init, Testbild, Statuszeile, Touch-Auswertung |
| [main/board_e32r28t.h](main/board_e32r28t.h) | Alle Pins, Auflösung, SPI-Takt, Farb- und Touch-Kalibrierwerte |
| [main/gfx.c](main/gfx.c), [main/gfx.h](main/gfx.h) | Mini-Grafikbibliothek: Canvas, Rechtecke, Linien, Kreis, Text, Flush |
| [main/font5x7.c](main/font5x7.c), [main/font5x7.h](main/font5x7.h) | 5×7-Pixel-Font (A–Z, 0–9, Satzzeichen), generiert |
| [main/touch.c](main/touch.c), [main/touch.h](main/touch.h) | XPT2046: SPI-Bus, `esp_lcd_touch`, Rohwert → Bildschirmkoordinate |
| [main/storage.c](main/storage.c), [main/storage.h](main/storage.h) | littlefs: Mount der `storage`-Partition, Belegung, Dateizählung |
| [main/sensor.c](main/sensor.c), [main/sensor.h](main/sensor.h) | BME280: I²C-Bus, Sensor-Init, Temperatur/Feuchte/Druck |
| [data/](data) | Inhalt der littlefs-Partition (Wallpaper), wird beim Bauen zum Image |
| [main/idf_component.yml](main/idf_component.yml) | Abhängigkeiten aus der Component Registry |
| [tools/gen_font5x7.py](tools/gen_font5x7.py) | Erzeugt den Font neu (Glyphen als ASCII-Art, mit Probedruck) |
| [sdkconfig.defaults](sdkconfig.defaults) | 4 MB Flash, 240 MHz CPU, Ziel esp32, eigene Partitionstabelle, littlefs-Namenslänge 128, Z-Druckschwelle des Touch-Treibers |
| [partitions.csv](partitions.csv) | Partitionstabelle: nvs, phy_init, 3 MB App, 1 MB littlefs |
| [idf.cmd](idf.cmd) | Wrapper: `.\idf build` / `.\idf -p COM8 flash monitor` ohne ESP-IDF-Profil |

## Erwartete Ausgabe auf dem seriellen Monitor

```
I (xxx) cyd: --- E32R28T CYD first tests, ESP-IDF v6.1.0 ---
I (xxx) cyd: heap: 293000 bytes free, largest free block 290000 bytes
I (xxx) cyd: RGB LED: red
I (xxx) cyd: RGB LED: green
I (xxx) cyd: RGB LED: blue
I (xxx) cyd: RGB LED: white (all three)
I (xxx) cyd: RGB LED: off
I (xxx) cyd: SPI2 for ILI9341: SCLK=14 MOSI=13 MISO=12 CS=15 DC=2 RST=EN
I (xxx) cyd: ILI9341 ready, 320x240 landscape, 40 MHz SPI clock, BGR element order
I (xxx) gfx: canvas 320x240: framebuffer 153600 bytes, bounce buffer 15360 bytes
I (xxx) cyd: test pattern drawn, backlight on - press BOOT (GPIO0) to redraw
I (xxx) touch: SPI3 for XPT2046: SCK=25 MOSI=32 MISO=39 CS=33
I (2464) touch: XPT2046 ready, raw axes 300..3805 / 215..3712, swap=1 mirror=1/1
I (2474) storage: littlefs mounted at /littlefs: 1024 KiB total, 1008 KiB free
I (4464) storage: 0 files in total (0 JPEG), 0 bytes
I (4464) sensor: I2C0 for BME280: SDA=23 SCL=27, 100 kHz, address 0x76
I (4464) i2c_bus: I2C Bus V2 uses the externally initialized bus handle
I (4864) sensor: BME280 ready, first reading: 23.88 C, 45.8 %rH, 965.5 hPa
I (4864) cyd: 123624 free heap bytes after initialisation
I (4864) cyd: BME280: 23.88 C, 45.8 %rH, 965.5 hPa
I (6874) cyd: BME280: 23.97 C, 45.8 %rH, 965.4 hPa
I (xxx) cyd: TOUCH: RAW 1234/2345 -> 120/160  Z 900     (beim Berühren, max. 5/s)
```

## Fehlersuche

| Symptom | Ursache / Abhilfe |
|---|---|
| Kein Log auf dem Monitor | Falscher COM-Port, USB-Kabel ohne Datenleitungen, BOOT gedrückt halten und EN/RESET tippen |
| „cannot open source file stdio.h" (IntelliSense) | `.vscode/c_cpp_properties.json` prüfen: `compilerPath` muss auf die vorhandene `xtensa-esp32-elf-gcc.exe` zeigen und `compileCommands` auf die gebaute `build/compile_commands.json` (erst nach einem Build vorhanden). Danach „C/C++: Reset IntelliSense Database" |
| `idf.py` wird nicht erkannt (`CommandNotFoundException`) | `idf.py` liegt bei EIM-Installationen nicht im PATH. Entweder `.\idf build` (Wrapper im Projekt) oder die ESP-IDF-Umgebung laden, siehe [Auf der Kommandozeile](#auf-der-kommandozeile) |
| „Die Ausführung von Skripts ist auf diesem System deaktiviert" | PowerShell-ExecutionPolicy ist `Restricted` (Windows-Standard). Entweder den Wrapper `.\idf` nutzen oder einmalig `Set-ExecutionPolicy -Scope CurrentUser -ExecutionPolicy RemoteSigned` ausführen |
| `partition 'storage' not found` / littlefs meldet `ESP_ERR_NOT_FOUND` | Partitionstabelle im Flash ist noch die alte: `.\idf -p COM8 flash` (schreibt sie an 0x8000 mit). Prüfen mit `.\idf partition-table` |
| `storage: 0 files in total` obwohl Wallpaper vorhanden | Dateien liegen nicht unter `data/` (z. B. eine Ebene zu tief, etwa `data/backgrounds/backgrounds/…`). Prüfen mit `python -m littlefs list build\storage.bin --block-size 4096` im Ordner `build\littlefs_py_venv\Scripts\` |
| littlefs-Image zu groß für die Partition | Weniger/kleinere JPEGs, oder `storage` in [partitions.csv](partitions.csv) vergrößern (App-Partition entsprechend verkleinern) und Tabelle neu flashen |
| Dateiname wird nicht angelegt / `LFS_ERR_NAMETOOLONG` | Name länger als `CONFIG_LITTLEFS_OBJ_NAME_LEN - 1` (aktuell 127 Zeichen), oder das Image wurde vor der Änderung gebaut: `.\idf fullclean` und neu bauen |
| BME280 meldet `no BME280 at 0x76` | SDA/SCL vertauscht oder nicht kontaktiert (SDA = IO23/P3-Pin1, SCL = IO27/P3-Pin4), 3V3/GND fehlen, oder die Adressbrücke ist nicht auf 0x76 (SDO an GND). Pull-ups: Breakout hat 10 kΩ, sonst 4,7 kΩ nach 3V3 ergänzen |
| BME280 liefert unsinnige Werte | Sensor kurz vorher verlötet/angefasst – Selbstaufheizung; 1–2 Minuten warten. Druck wird in hPa ausgegeben (965,5 hPa entspricht ca. 400 m Höhe) |
| `Could not open COM8, the port is busy` beim Flashen | Ein noch laufender Monitor hält den Port: im Monitor `Strg+]`, sonst das Fenster schließen, dann erneut flashen |
| Bildschirm bleibt dunkel, Log endet mit `cyd: LCD bring-up failed` | Fehlerzeile darüber lesen (SPI/Registrierung). Backlight an GPIO21 prüfen – manche Board-Revisionen nutzen GPIO27 (`BOARD_LCD_PIN_BACKLIGHT`) |
| Bild ist da, aber wie ein Negativ | `BOARD_LCD_INVERT_COLORS` auf `1` setzen |
| Rot und Blau vertauscht | `BOARD_LCD_BGR` auf `0` setzen |
| Bild seitenverkehrt | `BOARD_LCD_MIRROR_X` umschalten (bzw. `BOARD_LCD_MIRROR_Y`) |
| Bild um 90° verdreht | `BOARD_LCD_LANDSCAPE` auf `0` setzen (Hochformat) |
| Streifen, Flackern, Farbrauschen | `BOARD_LCD_PIXEL_CLOCK_HZ` auf `20 * 1000 * 1000` reduzieren |
| `not enough memory for the 320x240 canvas` | Heap-Optionen/BT/WiFi abgeschaltet lassen; der Framebuffer braucht 150 KB + 15 KB Puffer |
| Touch reagiert nicht | `BOARD_TOUCH_ENABLE 1` prüfen, Log der Init-Zeilen ansehen; der Punkt hängt am MISO-Pin GPIO39 |
| Punkt erscheint nicht unter dem Finger | Touch neu kalibrieren, Anleitung im Abschnitt [Touch-Kalibrierung](#touch-kalibrierung) |
| Touch-Koordinaten springen | `BOARD_TOUCH_Z_THRESHOLD` **und** `CONFIG_XPT2046_Z_THRESHOLD` (in `sdkconfig`, Default im Treiber: 400) erhöhen – beide müssen passen. Gemessen auf diesem Board: Z zwischen ~430 (unten links, der schwächste Punkt) und ~2700 (oben rechts); eingestellt sind 250, weil das unberührte Panel nahe 0 liest (Sollte das nicht stimmen, macht sich das durch Punkte ohne Berührung bemerkbar, dann hochsetzen) |
| RGB-LED leuchtet nur grün | Ist im Betrieb so gewollt (grün = läuft, blau = Neuzeichnen). Ob alle drei Kanäle funktionieren, zeigt der Selbsttest direkt nach dem Start: rot, grün, blau, weiß nacheinander. Fehlt eine Farbe dort, stimmt der Pin nicht (`BOARD_LED_PIN_*`; andere CYD-Revisionen nutzen GPIO4 für rot) oder die LED/der Vorwiderstand ist defekt |

### Touch-Kalibrierung

Die Werte in [board_e32r28t.h](main/board_e32r28t.h) sind auf diesem Board
bereits eingemessen: der Touchscreen ist gegenüber dem Display um 90° gedreht,
und beide Rohachsen zählen verkehrt herum, deshalb sind `BOARD_TOUCH_SWAP_XY`
und `BOARD_TOUCH_MIRROR_X/Y` alle `1`.

Gemessene Rohwerte an den Bildecken (Querformat 320×240), zweite, genauere
Messreihe:

| Ecke | RAW x/y |
|---|---|
| oben links | 3791 / 3714 |
| oben rechts | 3808 / 218 |
| unten links | 283 / 3704 |
| unten rechts | 323 / 214 |

Also wächst `raw_x` nach oben (300 = Unterkante, 3805 = Oberkante) und `raw_y`
nach links (215 = rechte Kante, 3712 = linke Kante). Wegen des Tauschs
kalibrieren die `..._X`-Werte die Bildschirm-**Y**-Achse und die `..._Y`-Werte
die Bildschirm-**X**-Achse.

Zum Nachkalibrieren:

1. An jeder Ecke des Bildschirms drücken und im Monitor die Zeile
   `cyd: TOUCH: RAW xxxx/yyyy` ablesen (max. 5 Ausgaben pro Sekunde).
2. Die äußersten Werte in `BOARD_TOUCH_RAW_MIN/MAX_X/Y` eintragen (kleiner Wert
   in `MIN`, großer Wert in `MAX`), gerne ein paar Zähler nach außen gerundet –
   Werte außerhalb des Fensters werden begrenzt, die Randpixel bleiben also
   erreichbar.
3. Falls die Achsen doch vertauscht sind: `BOARD_TOUCH_SWAP_XY` umschalten;
   falls eine Achse spiegelverkehrt läuft: das passende `BOARD_TOUCH_MIRROR_*`
   umschalten.

Mit den eingestellten Werten ergeben sich für die vier Eckberührungen und die
Bildschirmmitte (Mittelwert der vier Eckmesswerte):

| Berührung | RAW x/y | → x/y | Sollwert | Abweichung |
|---|---|---|---|---|
| oben links | 3791 / 3714 | 0 / 1 | 0 / 0 | 1 px |
| oben rechts | 3808 / 218 | 319 / 0 | 319 / 0 | 0 px |
| unten links | 283 / 3704 | 1 / 239 | 0 / 239 | 1 px |
| unten rechts | 323 / 214 | 319 / 238 | 319 / 239 | 1 px |
| Mitte | 2051 / 1962 | 160 / 120 | 160 / 120 | 0 px |

Die letzte Messreihe lag gegenüber der ersten rund 200 Zähler weiter außen (die
Ecken wurden genauer getroffen); die erste Messreihe würde mit diesen Werten
7–15 px neben der Ecke landen. Typische Streuung beim wiederholten Treffen
derselben Ecke: einige Dutzend Zähler, also 1–3 px. Eine noch genauere
Zuordnung würde eine affine 2×2-Abbildung in `touch.c` erfordern, die die
Querkopplung zwischen den Achsen mitrechnet.

Änderungen an `main/main.c`, `main/gfx.c` usw. brauchen nur einen neuen Build;
nach Änderungen an `main/idf_component.yml` lädt der Component Manager die
Abhängigkeiten selbst nach. `sdkconfig.defaults` wirkt nur bei neu erzeugter
`sdkconfig` (Ziel neu setzen oder `sdkconfig` löschen).

## Nächste Schritte

1. ~~Partitionierung~~ – erledigt, siehe [Flash-Aufteilung](#flash-aufteilung)
2. ~~littlefs + Wallpaper~~ – erledigt, siehe [Wallpaper-Ablage](#wallpaper-ablage)
3. ~~BME280~~ – erledigt: SDA = IO23, SCL = IO27, Adresse 0x76
4. NTP-Zeit über WLAN holen (`esp_sntp`), Zeitzone Europe/Berlin
5. Anzeige im Hochformat: große Temperaturziffern, kleines `°`, Feuchte darunter,
   Zeile mit Uhrzeit/Datum, Wallpaper im Hintergrund
6. HTTP-Server mit JSON-Endpunkt für Temperatur/Luftfeuchte
7. Bildauswahl: Jahreszeit aus dem Datum, Tageszeit aus der Uhrzeit, Zufall aus
   den drei JPEGs pro Slot, JPEG-Decoder (z. B. `espressif/esp_jpeg`)

Optional, nicht für das Raumthermometer nötig:

* microSD-Karte über SPI3 (CS=5) – Achtung: derselbe Bus wie der Touch
* Lautsprecher (GPIO26) mit I2S oder LEDC-PWM, Verstärker über GPIO4
