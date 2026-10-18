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
| Ausrichtung | **Hochformat 240×320**, von der ursprünglichen Querformat-Ansicht um 90° nach rechts gedreht: (0,0) oben links, x bis 239 nach rechts, y bis 319 nach unten |

Die Wallpaper liegen am PC unter `backgrounds/<jahreszeit>/<tageszeit>/`, je drei
JPEG-Dateien pro Ordner (zusammen 712 KB). Sie kommen in die
littlefs-Partition `storage`, siehe [Flash-Aufteilung](#flash-aufteilung).

## Anzeige-Layout (Hochformat)

Die Bilder enthalten bereits zwei Glasflächen (Glassmorph-Effekt) für die
Texte; die Anwendung zeichnet nur noch die Werte hinein.

| Element | Bereich | Umsetzung |
|---|---|---|
| Temperatur | frei positionierbar (aktuell x 25, Grundlinie y 175, `°`-Ecke 215/25) | `xx,x` mit 120 px hohen Ziffern, Komma und Nachkommastelle halb so groß und unten bündig, `°` separat platziert – [bigdigit.c](main/bigdigit.c), Werte in [layout.h](main/layout.h) |
| Luftfeuchte | folgt | geplant |
| Uhrzeit/Datum | folgt | geplant |

Die Temperatur wird mit der eingebetteten **TrueType-Schrift Open Sans** gezeichnet
([ttf.c](main/ttf.c), Renderer `stb_truetype`, Font liegt als `EMBED_FILES` im
Flash und wird ohne RAM-Kopie gelesen). Vorteil gegenüber den Bitmap-Fonts: jede
Größe, saubere Kantenglättung, echte Schriftformen – und derselbe Renderer deckt
später auch Uhrzeit, Datum und Luftfeuchte ab.

### Layout einstellen – [main/layout.h](main/layout.h)

Alle Werte der Anzeige stehen in einer Datei, geändert wird nur dort. Die
Koordinaten gelten im Hochformat: (0,0) oben links, x bis 239 nach rechts, y bis
319 nach unten.

| Makro | Bedeutung | aktuell |
|---|---|---|
| `LAYOUT_TEMP_X` | linke Kante der ersten großen Ziffer | 25 |
| `LAYOUT_TEMP_BASELINE_Y` | Grundlinie = Unterkante der großen Ziffern; Komma-Schweif und Nachkommastelle enden hier ebenfalls | 175 |
| `LAYOUT_TEMP_MAX_WIDTH` | Breite, auf die die Zeichengruppe höchstens gestaucht wird | 190 |
| `LAYOUT_TEMP_DIGIT_HEIGHT` | Höhe der beiden Ziffern vor dem Komma | 120 |
| `LAYOUT_TEMP_MIN_SQUEEZE` | stärkste horizontale Stauchung; `1.0` = nie stauchen, `0.5` = sehr schmal | 0.55 |
| `LAYOUT_TEMP_DECIMAL_SCALE` | Komma + Nachkommastelle relativ zur Ziffernhöhe | 0.5 |
| `LAYOUT_TEMP_DEGREE_X` | rechte Kante des `°` | 215 |
| `LAYOUT_TEMP_DEGREE_Y` | Oberkante des `°` | 25 |
| `LAYOUT_TEMP_DEGREE_SCALE` | `°` relativ zur Ziffernhöhe | 0.5 |

Die Positionen von Ziffernblock und Gradzeichen sind damit unabhängig: `x` und
Grundlinie gehören zum Zahlentext, `DEGREE_X/Y` bezeichnen die **obere rechte
Ecke der Tinte** des Gradzeichens. Beides lässt sich frei auf dem Bildschirm
platzieren, auch überlappend.

Ablauf zum Ausprobieren: Wert ändern → `.\idf build` → `.\idf -p COM8 app-flash`
→ im Monitor die Zeilen ansehen:

```
bigdigit: "21,8" cap 120 px, scale x/y 0.060/0.082, from x=25 on baseline y=175, 187 px wide
bigdigit: degree: 27x25 px ink, top right corner at (215,25)
```

Regeln, die der Renderer anwendet:

* Die Ziffern-Gruppe wird horizontal gestaucht, bis sie in `MAX_WIDTH` passt –
  aber höchstens bis `MIN_SQUEEZE`. Ist selbst das zu wenig, verringert der
  Renderer die Ziffernhöhe, damit die Proportionen lesbar bleiben.
* Komma und Nachkommastelle sitzen mit ihrer Unterkante auf der Grundlinie; der
  Komma-Schweif endet ebenfalls dort.
* Das Gradzeichen wird an `DEGREE_X/Y` ausgerichtet, egal wie groß es ist.

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

## Was das Programm macht

1. Backlight aus, RGB-LED initialisieren, danach **Selbsttest der LED**: rot,
   grün, blau, weiß nacheinander für je 400 ms (steht so im Log)
2. ILI9341 über SPI2 hochfahren (40 MHz) in den **Hochformat-Modus 240×320**
   (ESP-IDF 6 nennt die beiden freien Busse SPI1/SPI2; die Header verwenden die
   weiterhin gültigen Aliase `SPI2_HOST`/`SPI3_HOST`, im Monitor erscheint
   deshalb „SPI1" für das Display und „SPI2" für den Touch)
3. Canvas 240×320 anlegen (Framebuffer 150 KB, DMA-fähig)
4. littlefs-Partition `storage` einhängen (beim ersten Mal formatieren), Belegung
   und Dateianzahl loggen
5. BME280 über I²C initialisieren (Adresse 0x76, 16× Oversampling); der Init
   kehrt erst zurück, wenn eine vollständige Messung vorliegt
6. XPT2046 initialisieren
7. Backlight auf 100 %, LED grün
8. **Thermometer-Schleife**: Wallpaper dekodieren (JPEG → RGB565 direkt in den
   Canvas), Temperatur als `xx,x°` in die Kachel zeichnen, Frame senden. Alle
   10 s eine neue Messung; neu gezeichnet wird nur, wenn sich die Anzeige
   (Zehntelgrad) ändert.

Das alte Hardware-Testbild liegt getrennt in
[testscreen.c](main/testscreen.c)/[testscreen.h](main/testscreen.h) (Muster,
Statuszeile, Touch-Punkte, BOOT-Neudruck). Einschalten mit
`APP_SHOW_TESTSCREEN 1` in [main.c](main/main.c); zum Entfernen beide Dateien
und den einen Aufruf löschen.

### Bildschirmaufbau des Testbilds (Hochformat 240×320)

| Bereich | Inhalt |
|---|---|
| y 0–25 | Titelbalken: Boardname, ESP-IDF-Version |
| y 32–151 | Farbreihen: Weiß, Gelb, Cyan, Grün, Magenta, Rot, Blau, Schwarz |
| y 158–187 | Verläufe: Schwarz→Weiß, Schwarz→Rot, Schwarz→Blau |
| y 194–257 | Geometrie: Rahmen, Diagonalen, Kreis, Text „GEOMETRY 1PX" |
| y 264–319 | Statuszeile und laufender Balken |

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
I (2594) storage: littlefs mounted at /littlefs: 1024 KiB total, 36 KiB free
I (2644) storage: /littlefs/backgrounds/autumn/day: 3 files, 53200 bytes
I (2674) storage: /littlefs/backgrounds/autumn/evening: 3 files, 40490 bytes
…   (16 Ordnerzeilen, je 3 Dateien)
I (4444) storage: 48 files in total (48 JPEG), 728964 bytes
```

Hinweise:

* Der Image-Build läuft über `littlefs_create_partition_image(storage ../data
  FLASH_IN_PROJECT)` in [main/CMakeLists.txt](main/CMakeLists.txt); dabei wird
  einmalig ein kleines Python-Venv (`build/littlefs_py_venv`) mit
  `littlefs-python` angelegt.
* Das Image ist fest 1 MB groß (`--fs-size=0x100000`, Blockgröße 4096).
* **Platzbedarf (gemessen):** 712 KiB Dateien + 276 KiB littlefs-Overhead =
  988 KiB belegt, 36 KiB frei. Der Overhead kommt von den Metadatenpaaren
  (jedes der 21 Verzeichnisse belegt 2 × 4 KiB = 8 KiB, zusammen 168 KiB) und
  der Blockrundung der Dateien (~108 KiB). Ein viertes Bild pro Slot passt
  **nicht** mehr hinein – dafür müsste `storage` in
  [partitions.csv](partitions.csv) vergrößert und die App-Partition entsprechend
  verkleinert werden (Tabelle danach neu flashen).
* Dateinamen dürfen bis 127 Zeichen lang sein: `CONFIG_LITTLEFS_OBJ_NAME_LEN=128`
  in [sdkconfig.defaults](sdkconfig.defaults) (Vorgabe der Komponente ist 64).
  Der längste Name ist inzwischen auf 29 Zeichen gekürzt, der Wert bleibt aber
  großzügig – der Image-Build übernimmt ihn automatisch als `--name-max`.
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

**Header, die in keinem `.c` eingebunden sind** (`background.h` ist so ein Fall),
stehen nicht in der Compile-Datenbank. IntelliSense prüft sie dann nur mit dem
Projekt-Suchpfad `${workspaceFolder}/**`, nicht mit den ESP-IDF-Pfaden. Sobald
so ein Header einen anderen Header aus dem Projekt einbindet, der seinerseits
ESP-IDF-Header braucht (früher `gfx.h` → `esp_lcd_panel_ops.h`), meldet VS Code
einen Include-Fehler, obwohl der Compiler die Datei akzeptiert. Lösung: die
Abhängigkeit klein halten – reine Daten-/Farbtabellen binden nur
[gfx_color.h](main/gfx_color.h) ein, das außer `<stdint.h>` nichts braucht.

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
* `espressif/esp_jpeg` 1.3.1 (JPEG-Decoder für die Wallpaper)

Zum Testen ohne Hardware genügt `.\idf build` – es ist bereits erfolgreich
durchgelaufen (App-Binärdatei 434 KB inklusive 147 KB Schrift in einer
3008-KB-Partition, Flash 4 MB,
CPU 240 MHz).

## Projektstruktur

| Datei | Zweck |
|---|---|
| [main/main.c](main/main.c) | Ablauf: Backlight, LCD-Init, littlefs, Sensor, Thermometer-Schleife (`APP_SHOW_TESTSCREEN` schaltet das Testbild ein) |
| [main/board_e32r28t.h](main/board_e32r28t.h) | Alle Pins, Auflösung, Ausrichtung, SPI-Takt, Farb- und Touch-Kalibrierwerte |
| [main/wallpaper.c](main/wallpaper.c), [main/wallpaper.h](main/wallpaper.h) | JPEG → Canvas, Slot-Auswahl über `background.h`, Jahreszeit/Tageszeit aus Datum/Uhrzeit |
| [main/bigdigit.c](main/bigdigit.c), [main/bigdigit.h](main/bigdigit.h) | Große Temperaturanzeige `xx,x°` mit dem TrueType-Renderer |
| [main/layout.h](main/layout.h) | Alle Layout-Werte der Anzeige (Kachel, Ziffernhöhe, Komma-/Gradgröße) |
| [main/ttf.c](main/ttf.c), [main/ttf.h](main/ttf.h) | TrueType-Text: `stb_truetype`, Anti-Aliasing, Stauchung |
| [main/fonts/OpenSans-Regular.ttf](main/fonts) | Schrift (Apache 2.0), per `EMBED_FILES` im App-Image |
| [main/testscreen.c](main/testscreen.c), [main/testscreen.h](main/testscreen.h) | Hardware-Testbild, Statuszeile, Touch-Punkte, BOOT-Neudruck (separat entfernbar) |
| [main/statusled.c](main/statusled.c), [main/statusled.h](main/statusled.h) | RGB-LED inkl. Selbsttest |
| [main/gfx.c](main/gfx.c), [main/gfx.h](main/gfx.h) | Mini-Grafikbibliothek: Canvas, Rechtecke, Linien, Kreis, Text, Flush |
| [main/gfx_color.h](main/gfx_color.h) | RGB565-Farbmakros, ohne ESP-IDF-Abhängigkeit (auch für Datentabellen nutzbar) |
| [main/font5x7.c](main/font5x7.c), [main/font5x7.h](main/font5x7.h) | 5×7-Pixel-Font (A–Z, 0–9, Satzzeichen), generiert |
| [main/touch.c](main/touch.c), [main/touch.h](main/touch.h) | XPT2046: SPI-Bus, `esp_lcd_touch`, Rohwert → Bildschirmkoordinate |
| [main/storage.c](main/storage.c), [main/storage.h](main/storage.h) | littlefs: Mount der `storage`-Partition, Belegung, Dateizählung |
| [main/sensor.c](main/sensor.c), [main/sensor.h](main/sensor.h) | BME280: I²C-Bus, Sensor-Init, Temperatur/Feuchte/Druck |
| [main/background.h](main/background.h) | Tabelle der 48 Wallpaper-Dateinamen und Textfarben je Jahreszeit/Tageszeit |
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
I (2436) cyd: ILI9341 ready, 240x320 portrait, 40 MHz SPI clock, BGR element order
I (2436) gfx: canvas 240x320: framebuffer 153600 bytes, bounce buffer 11520 bytes
I (2546) storage: littlefs mounted at /littlefs: 1024 KiB total, 36 KiB free
…   (16 Ordnerzeilen, je 3 Dateien)
I (4446) storage: 48 files in total (48 JPEG), 728964 bytes
I (4446) sensor: I2C0 for BME280: SDA=23 SCL=27, 100 kHz, address 0x76
I (4456) i2c_bus: I2C Bus V2 uses the externally initialized bus handle
I (4846) sensor: BME280 ready, first reading: 21.08 C, 49.5 %rH, 967.2 hPa
I (4930) touch: XPT2046 ready, raw axes 300..3805 / 215..3712, swap=0 mirror=1/0
I (4940) ttf: 147528 byte font loaded, ascent 2189 / descent -600 / gap 0 (font units)
I (4950) cyd: 126580 free heap bytes after initialisation
I (4950) cyd: wallpaper slot 1/2/1: /littlefs/backgrounds/summer/evening/ocean-sunset.jpg, text colour 0x1F00
I (5070) wallpaper: …/ocean-sunset.jpg: 240x320, 14222 bytes JPEG -> RGB565
I (5080) bigdigit: "21,8" cap 120 px, scale x/y 0.060/0.082, from x=25 on baseline y=175, 187 px wide
I (5080) bigdigit: degree: 27x25 px ink, top right corner at (215,25)
I (5110) cyd: frame: 21,8 degC, text from (25,175), 187 px wide, degree at (215,25), decode 103 ms, total 148 ms
I (15110) cyd: BME280: 21.92 C, 47.1 %rH, 967.3 hPa
I (15220) bigdigit: "21,9" cap 120 px, scale x/y 0.060/0.082, from x=25 on baseline y=175, 187 px wide
```

## Fehlersuche

| Symptom | Ursache / Abhilfe |
|---|---|
| Kein Log auf dem Monitor | Falscher COM-Port, USB-Kabel ohne Datenleitungen, BOOT gedrückt halten und EN/RESET tippen |
| „cannot open source file stdio.h" (IntelliSense) | `.vscode/c_cpp_properties.json` prüfen: `compilerPath` muss auf die vorhandene `xtensa-esp32-elf-gcc.exe` zeigen und `compileCommands` auf die gebaute `build/compile_commands.json` (erst nach einem Build vorhanden). Danach „C/C++: Reset IntelliSense Database" |
| Include-Fehler in einem Header, obwohl der Build läuft | Der Header gehört zu keinem `.c` und wird von IntelliSense nur mit dem Projekt-Suchpfad geprüft. Keine ESP-IDF-Header darin einbinden – reine Tabellen nutzen [gfx_color.h](main/gfx_color.h) statt [gfx.h](main/gfx.h), Details im Abschnitt [IntelliSense in VS Code](#intellisense-in-vs-code) |
| `idf.py` wird nicht erkannt (`CommandNotFoundException`) | `idf.py` liegt bei EIM-Installationen nicht im PATH. Entweder `.\idf build` (Wrapper im Projekt) oder die ESP-IDF-Umgebung laden, siehe [Auf der Kommandozeile](#auf-der-kommandozeile) |
| „Die Ausführung von Skripts ist auf diesem System deaktiviert" | PowerShell-ExecutionPolicy ist `Restricted` (Windows-Standard). Entweder den Wrapper `.\idf` nutzen oder einmalig `Set-ExecutionPolicy -Scope CurrentUser -ExecutionPolicy RemoteSigned` ausführen |
| `partition 'storage' not found` / littlefs meldet `ESP_ERR_NOT_FOUND` | Partitionstabelle im Flash ist noch die alte: `.\idf -p COM8 flash` (schreibt sie an 0x8000 mit). Prüfen mit `.\idf partition-table` |
| `storage: 0 files in total` obwohl Wallpaper vorhanden | Dateien liegen nicht unter `data/` (z. B. eine Ebene zu tief, etwa `data/backgrounds/backgrounds/…`). Prüfen mit `python -m littlefs list build\storage.bin --block-size 4096` im Ordner `build\littlefs_py_venv\Scripts\` |
| littlefs-Image zu groß für die Partition | Weniger/kleinere JPEGs, oder `storage` in [partitions.csv](partitions.csv) vergrößern (App-Partition entsprechend verkleinern) und Tabelle neu flashen |
| Dateiname wird nicht angelegt / `LFS_ERR_NAMETOOLONG` | Name länger als `CONFIG_LITTLEFS_OBJ_NAME_LEN - 1` (aktuell 127 Zeichen), oder das Image wurde vor der Änderung gebaut: `.\idf fullclean` und neu bauen |
| BME280 meldet `no BME280 at 0x76` | SDA/SCL vertauscht oder nicht kontaktiert (SDA = IO23/P3-Pin1, SCL = IO27/P3-Pin4), 3V3/GND fehlen, oder die Adressbrücke ist nicht auf 0x76 (SDO an GND). Pull-ups: Breakout hat 10 kΩ, sonst 4,7 kΩ nach 3V3 ergänzen |
| BME280 liefert unsinnige Werte | Sensor kurz vorher verlötet/angefasst – Selbstaufheizung; 1–2 Minuten warten. Druck wird in hPa ausgegeben (965,5 hPa entspricht ca. 400 m Höhe) |
| `Could not open COM8, the port is busy` beim Flashen | Ein noch laufender Monitor hält den Port: im Monitor `Strg+]`, sonst das Fenster schließen, dann erneut flashen |
| `undefined reference to 'app_main'` | `main.c` fehlt in der `SRCS`-Liste von [main/CMakeLists.txt](main/CMakeLists.txt) – passiert, wenn die ESP-IDF-Erweiterung die Liste synchronisiert, während die Datei kurzzeitig gelöscht war. Datei wieder eintragen |
| Wallpaper erscheint, aber die Farben stimmen nicht (grün/lila) | Byte-Reihenfolge des JPEG-Ergebnisses: `flags.swap_color_bytes` in [wallpaper.c](main/wallpaper.c) umschalten |
| Bild steht auf dem Kopf | `BOARD_LCD_MIRROR_X` **und** `BOARD_LCD_MIRROR_Y` in [board_e32r28t.h](main/board_e32r28t.h) umschalten (180°-Drehung) – danach auch `BOARD_TOUCH_MIRROR_X/Y` umschalten, sonst passt der Touch nicht mehr |
| Schrift wirkt zu schmal/gestaucht | `LAYOUT_TEMP_MIN_SQUEEZE` in [layout.h](main/layout.h) erhöhen (1.0 = nie stauchen, Schrift wird stattdessen kleiner) |
| Schrift/° zu groß oder zu klein | `LAYOUT_TEMP_DIGIT_HEIGHT` bzw. `LAYOUT_TEMP_DEGREE_SCALE` in [layout.h](main/layout.h) – Kontrolle über die `bigdigit:`-Zeilen im Log |
| Text sitzt an der falschen Stelle | `LAYOUT_TEMP_X` / `LAYOUT_TEMP_BASELINE_Y` bzw. `LAYOUT_TEMP_DEGREE_X/Y` in [layout.h](main/layout.h) |
| Kein Text, Log zeigt `font could not be loaded` | `fonts/OpenSans-Regular.ttf` fehlt oder ist beschädigt; Datei prüfen und neu bauen (`EMBED_FILES` in [main/CMakeLists.txt](main/CMakeLists.txt)) |
| Touch trifft um 90° versetzt | Touch-Flags passend zur Anzeige: `BOARD_TOUCH_SWAP_XY` / `BOARD_TOUCH_MIRROR_X/Y` – siehe [Touch-Kalibrierung](#touch-kalibrierung) |
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
bereits eingemessen. Im **Hochformat** folgt `raw_x` der Bildschirm-X-Achse und
`raw_y` der Y-Achse, wobei `raw_y` verkehrt herum zählt; deshalb
`BOARD_TOUCH_SWAP_XY 0`, `BOARD_TOUCH_MIRROR_X 0`, `BOARD_TOUCH_MIRROR_Y 1`.

Gemessene Rohwerte an den Bildecken (damals im Querformat 320×240), zweite,
genauere Messreihe:

| Ecke | RAW x/y |
|---|---|
| oben links | 3791 / 3714 |
| oben rechts | 3808 / 218 |
| unten links | 283 / 3704 |
| unten rechts | 323 / 214 |

Damit gilt: `raw_x` von 300 bis 3805 und `raw_y` von 215 bis 3712 decken jeweils
eine Bildschirmkante ab. Zum Nachkalibrieren:

1. An jeder Ecke des Bildschirms drücken und im Monitor die Zeile
   `testscreen: TOUCH: RAW xxxx/yyyy` ablesen (Testbild mit
   `APP_SHOW_TESTSCREEN 1`).
2. Die äußersten Werte in `BOARD_TOUCH_RAW_MIN/MAX_X/Y` eintragen (kleiner Wert
   in `MIN`, großer Wert in `MAX`), gerne ein paar Zähler nach außen gerundet –
   Werte außerhalb des Fensters werden begrenzt, die Randpixel bleiben also
   erreichbar.
3. Falls die Achsen doch vertauscht sind: `BOARD_TOUCH_SWAP_XY` umschalten;
   falls eine Achse spiegelverkehrt läuft: das passende `BOARD_TOUCH_MIRROR_*`
   umschalten.

Im Hochformat ergeben die Ecken (Umrechnung mit denselben Werten):

| Berührung (Querformat gemessen) | RAW x/y | Hochformat x/y | erwartete Ecke |
|---|---|---|---|
| oben links | 3791 / 3714 | 238 / 0 | oben rechts |
| oben rechts | 3808 / 218 | 239 / 319 | unten rechts |
| unten links | 283 / 3704 | 0 / 1 | oben links |
| unten rechts | 323 / 214 | 1 / 319 | unten links |

Die 0/1-Abweichungen sind die übliche Streuung beim Antippen (1–3 px).

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
4. ~~Hochformat + Testbild ausgelagert~~ – erledigt: `testscreen.c`, Anzeige 240×320
5. ~~JPEG-Anzeige~~ – erledigt: `wallpaper.c`, `ocean-sunset.jpg` wird dekodiert
6. ~~Temperatur im Großformat~~ – erledigt: `bigdigit.c`, Kachel x 25…215 / y 25…175
7. Luftfeuchte `xx,x %rF` in der zweiten Glasfläche (halb so große Schrift)
8. NTP-Zeit über WLAN holen (`esp_sntp`), Zeitzone Europe/Berlin, Uhrzeit/Datum-Block
9. Bildauswahl automatisch: Jahreszeit aus dem Monat, Tageszeit aus der Stunde
   (Helfer `wallpaper_season_from_month()` / `wallpaper_daytime_from_hour()` sind
   schon vorhanden), Zufall aus den drei Bildern per `wallpaper_random_index()`
10. HTTP-Server mit JSON-Endpunkt für Temperatur/Luftfeuchte
11. Feinschliff: beim Neuzeichnen nur die Werte-Kachel aktualisieren statt des
    ganzen Frames (spart das JPEG-Dekodieren, aktuell 102 ms pro Frame)

Optional, nicht für das Raumthermometer nötig:

* microSD-Karte über SPI3 (CS=5) – Achtung: derselbe Bus wie der Touch
* Lautsprecher (GPIO26) mit I2S oder LEDC-PWM, Verstärker über GPIO4
