# Hinweise für die KI-Unterstützung in diesem Projekt

Diese Datei wird von Copilot Chat automatisch gelesen, wenn sie unter
`.github/copilot-instructions.md` im Arbeitsverzeichnis liegt. Sie hält fest, was
in diesem Projekt bereits teuer gelernt wurde – bitte einhalten, statt es neu
auszuprobieren. Für neue Projekte: Datei kopieren und den Abschnitt
„Projekt-spezifisch" anpassen.

## Umgebung

- ESP-IDF **v6.1**, Ziel meist `esp32`; Wrapper im Projekt: `.\idf build`,
  `.\idf -p COM<port> flash`, `.\idf -p COM<port> monitor`
- Tools unter `C:\Espressif\tools`, IDF unter `C:\esp\v6.1\esp-idf`
- PowerShell mit ExecutionPolicy `Restricted` → immer der Wrapper `.\idf`,
  nicht `idf.py` direkt
- Der Monitor blockiert den seriellen Port. Vor dem Flashen die Monitor-Session
  beenden, sonst `could not open port … PermissionError`
- `Select-String`-Filter liefert Exitcode 1 – für „fertig" ist die Zeile
  `Project build complete` maßgeblich, nicht der Exitcode

## Build-System

- `MINIMAL_BUILD`: jedes benutzte Component muss in `REQUIRES`
  (`main/CMakeLists.txt`) **und** in `main/idf_component.yml` stehen
- **Änderungen an `sdkconfig.defaults` wirken nur, wenn keine `sdkconfig`
  existiert.** Bei bestehendem Projekt die Option direkt in `sdkconfig` ändern,
  sonst bleibt sie aus (Beispiel: `CONFIG_ESP_WIFI_SOFTAP_SUPPORT`)
- Kein `cJSON` in IDF 6.1 – JSON mit `snprintf` bauen oder Felder per `strstr`
  extrahieren
- Warnungen sind Fehler (`-Werror`): `snprintf` mit möglicher Kürzung vermeiden,
  dafür `strlcpy` verwenden

## Eigene Header

- **Nie** Namen von System-/Component-Headern verwenden: kein `main/time.h`
  (verdeckt `<time.h>`), kein `main/mdns.h` (verdeckt die mDNS-Komponente).
  Deshalb heißen die Module hier `clock` und `discovery`
- Header ohne ESP-IDF-Abhängigkeit halten, wenn sie von datenorientierten
  Headern gebraucht werden (Muster: `gfx_color.h`)

## Speicher (ESP32 mit 520 KB SRAM)

- DRAM-Segment für Daten: 192 KB, davon ~41 KB statisch
- Voller Framebuffer 240×320 = 150 KB passt **nicht** neben WiFi → Anzeige
  bandweise über `gfx_canvas_set_origin()`, `LAYOUT_BAND_HEIGHT` muss
  `BOARD_SCREEN_HEIGHT` teilen
- Heap nach der Initialisierung und nach jedem neuen Server/Task **loggen** –
  Messwerte statt Schätzungen (`esp_heap_caps_get_free_size` /
  `_get_largest_free_block`)
- `stb_truetype` scheitert **still**, wenn kein zusammenhängender Block frei ist:
  Chunk-Größen in `stb_truetype.h` bleiben klein (128/64/32), Allokationsfehler
  werden protokolliert

## Netzwerk

- `nvs_flash_init()` genau einmal, früh, über `netcfg_init()`
- **Port 80 darf nur einen Server haben**: vor dem Start des zweiten
  `httpd_stop()` des ersten abwarten (`webserver_stop()`), sonst antwortet der
  falsche Server
- Access Point braucht `CONFIG_ESP_WIFI_SOFTAP_SUPPORT=y`; Scan im AP-Betrieb
  nur mit `WIFI_MODE_APSTA`
- DHCP-Hostname max. 32 Zeichen, nur `a–z`, `0–9`, `-`
- HTTPS zu Fremdservern: `esp_crt_bundle_attach` +
  `CONFIG_MBEDTLS_CERTIFICATE_BUNDLE_CROSS_SIGNED_VERIFY=y`,
  `CONFIG_MBEDTLS_SSL_IN_CONTENT_LEN=16384`
- mDNS über die Komponente `espressif/mdns`; Hostname aus `netcfg_hostname()`

## Zahlen und Text

- `CONFIG_NEWLIB_NANO_FORMAT` kann `%f` unbrauchbar machen → Dezimalzahlen aus
  Ganzzahlen bauen (`/10`, `%10`), wie in `infotile.c` und `webserver.c`
- JSON immer mit Punkt, die Anzeige mit Komma
- Text aus Formularen maskieren: `html_escape()` / `json_escape()` aus
  `textutil.h`

## Arbeitsweise

- Testschalter (`APP_FORCE_CONFIG`, `APP_TEST_CONFIG_AFTER_CONNECT_S`) stehen im
  Repository auf **0**; wenn sie für einen Test auf 1 gesetzt werden, müssen sie
  danach zurück – ein vergessener Schalter hat schon zweimal eine
  Fehlersuche ausgelöst
- Nach jeder Änderung: bauen, flashen, im Monitor prüfen, Ergebnis berichten
- Board-spezifische Werte (Pins, Spiegelung, Kalibrierung) gehören in
  `main/board_<board>.h`, nie in die Logik
- Alles, was ein Mensch einstellen will, gehört in `layout.h` / `connect.h` /
  `netcfg`, nicht als Konstante in eine `.c`

## Projekt-spezifisch (hier: E32R28T CYD)

- Display ILI9341V 240×320 **Hochformat**, gespiegelt (`BOARD_LCD_MIRROR_X/Y`),
  `BOARD_LCD_BGR=1`
- Touch XPT2046 an eigenem SPI-Bus, Achsen getauscht (`BOARD_TOUCH_SWAP_XY`)
- BME280 an I²C: SDA=IO23, SCL=IO27, Adresse 0x76
- RGB-LED R/G/B = 22/16/17, **low-aktiv**, über LEDC-PWM
- Partitionen: 3 MB App, 1 MB littlefs (`storage`) für die Wallpaper

## Wo die Details stehen

- [README.md](../README.md) – vollständiges Handbuch inkl. Fehlersuche
- [PORTING.md](../PORTING.md) – was für die Folgeprojekte übernommen wird
