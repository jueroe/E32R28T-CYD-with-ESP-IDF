# E32R28T CYD – erste Tests mit dem Display

ESP-IDF-Projekt für die erste Inbetriebnahme des "Cheap Yellow Display"
(QDtech / LCDWiki **2.8inch ESP32-32E Display**, SKU **E32R28T**).

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

## Was das Testprogramm macht

1. Backlight aus, RGB-LED initialisieren, danach **Selbsttest der LED**: rot,
   grün, blau, weiß nacheinander für je 400 ms (steht so im Log), BOOT-Taste als
   Eingang konfigurieren
2. ILI9341 über SPI2 hochfahren (40 MHz) und in den Querformat-Modus 320×240 bringen
3. Testbild aus einem kompletten Framebuffer (150 KB) in Streifen auf das Display schreiben
4. Backlight auf 100 % dimmen (LEDC-PWM, 20 kHz)
5. XPT2046 über SPI3 initialisieren; schlägt das fehl, läuft das Programm ohne Touch weiter
6. Endlosschleife:
   * Statuszeile mit Laufzeit, Frames/s und freiem Heap aktualisieren
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

### Auf der Kommandozeile (PowerShell)

```powershell
# ESP-IDF-Umgebung aktivieren (dieselbe Umgebung, die auch VS Code nutzt)
. C:\Espressif\tools\Microsoft.v6.1.PowerShell_profile.ps1

cd C:\Users\joe\Documents\ESP-IDF\Projects\E32R28T_erste_Tests
idf.py build
idf.py -p COM3 flash monitor     # COMx = Port des Boards
# Monitor beenden: Strg + ]
```

Der erste Build lädt die verwalteten Komponenten automatisch aus der ESP
Component Registry nach `managed_components/`:

* `espressif/esp_lcd_ili9341` 2.1.0 (ILI9341-Treiber für `esp_lcd`)
* `atanisoft/esp_lcd_touch_xpt2046` 1.0.6 + `espressif/esp_lcd_touch` 1.2.1

Zum Testen ohne Hardware genügt `idf.py build` – es ist bereits erfolgreich
durchgelaufen (App-Binärdatei ca. 191 KB, Flash 4 MB, CPU 240 MHz).

## Projektstruktur

| Datei | Zweck |
|---|---|
| [main/main.c](main/main.c) | Ablauf: Backlight, LCD-Init, Testbild, Statuszeile, Touch-Auswertung |
| [main/board_e32r28t.h](main/board_e32r28t.h) | Alle Pins, Auflösung, SPI-Takt, Farb- und Touch-Kalibrierwerte |
| [main/gfx.c](main/gfx.c), [main/gfx.h](main/gfx.h) | Mini-Grafikbibliothek: Canvas, Rechtecke, Linien, Kreis, Text, Flush |
| [main/font5x7.c](main/font5x7.c), [main/font5x7.h](main/font5x7.h) | 5×7-Pixel-Font (A–Z, 0–9, Satzzeichen), generiert |
| [main/touch.c](main/touch.c), [main/touch.h](main/touch.h) | XPT2046: SPI-Bus, `esp_lcd_touch`, Rohwert → Bildschirmkoordinate |
| [main/idf_component.yml](main/idf_component.yml) | Abhängigkeiten aus der Component Registry |
| [tools/gen_font5x7.py](tools/gen_font5x7.py) | Erzeugt den Font neu (Glyphen als ASCII-Art, mit Probedruck) |
| [sdkconfig.defaults](sdkconfig.defaults) | 4 MB Flash, 240 MHz CPU, Ziel esp32, Z-Druckschwelle des Touch-Treibers |

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
I (xxx) touch: XPT2046 ready, raw axes 200..3900 / 200..3900, swap=0 mirror=0/0
I (xxx) cyd: 124000 free heap bytes after initialisation
I (xxx) cyd: TOUCH: RAW 1234/2345 -> 120/160  Z 900     (beim Berühren, max. 5/s)
```

## Fehlersuche

| Symptom | Ursache / Abhilfe |
|---|---|
| Kein Log auf dem Monitor | Falscher COM-Port, USB-Kabel ohne Datenleitungen, BOOT gedrückt halten und EN/RESET tippen |
| „cannot open source file stdio.h" (IntelliSense) | `.vscode/c_cpp_properties.json` prüfen: `compilerPath` muss auf die vorhandene `xtensa-esp32-elf-gcc.exe` zeigen und `compileCommands` auf die gebaute `build/compile_commands.json` (erst nach einem Build vorhanden). Danach „C/C++: Reset IntelliSense Database" |
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

* LVGL 9 über `espressif/esp_lvgl_port` einbinden (der Canvas in `gfx.c` kann
  dann durch einen LVGL-Zeichenpuffer ersetzt werden)
* microSD-Karte über SPI3 (CS=5) anbinden – Achtung: derselbe Bus wie der Touch
* Lautsprecher (GPIO26) mit I2S oder LEDC-PWM ansteuern, Verstärker über GPIO4 freigeben
* WLAN/BLE: dafür `CONFIG_BT_ENABLED`/WLAN zuschalten und Heap-Verbrauch beachten
