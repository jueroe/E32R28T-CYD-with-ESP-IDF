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
| Uhrzeit | NTP über WLAN, Zeitzone Europe/Berlin – **läuft**, Anzeige vorerst `dd.mm. hh:mm` |
| Temperatur | große Ziffern für die ganzen °C, Komma und eine Nachkommastelle, das `°` nur ein Viertel so groß |
| Luftfeuchte | darunter, etwa halb so große Schrift, Format `xx,x %rF` |
| Luftdruck | darunter, `967 hPa` |
| Hintergrund | JPEG-Wallpapers nach Jahreszeit (Frühling/Sommer/Herbst/Winter) und Tageszeit (Morgen/Tag/Abend/Nacht), je Slot 3 Bilder zufällig, insgesamt 48 |
| Sonnenauf-/-untergang | Werte von sunrise-sunset.org für 48.4149 / 11.0997 – **nur Platzhalter**, Abruf folgt |
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
| Temperatur | obere Kachel, frei positionierbar (aktuell x 25, Grundlinie y 160, `°`-Ecke 215/40) | `xx,x` mit 120 px hohen Ziffern, Komma und Nachkommastelle halb so groß und unten bündig, `°` separat platziert – [bigdigit.c](main/bigdigit.c), Werte in [layout.h](main/layout.h) |
| Luftfeuchte, Luftdruck, Datum/Zeit | untere Kachel x 25…214, y 205…294 | drei zentrierte Zeilen `xx,x %rF`, `xxx hPa`, `dd.mm. hh:mm` – [infotile.c](main/infotile.c), Werte in [layout.h](main/layout.h) |

Die Temperatur wird mit der eingebetteten **TrueType-Schrift Open Sans** gezeichnet
([ttf.c](main/ttf.c), Renderer `stb_truetype`, Font liegt als `EMBED_FILES` im
Flash und wird ohne RAM-Kopie gelesen). Vorteil gegenüber den Bitmap-Fonts: jede
Größe, saubere Kantenglättung, echte Schriftformen – und derselbe Renderer deckt
auch Uhrzeit, Datum und Luftfeuchte ab.

**Wichtig:** `stb_truetype` holt seine Zwischenpuffer (Glyphen-Vertices, Kurven-
und Kantenlisten, Bitmap) aus dem Heap. Upstream fordert für die Vertices eines
120-px-Glyphen einen Block von **56 KB** am Stück an (2000 Einträge à 28 Byte) –
und eine fehlgeschlagene Allokation ist dort **stumm**: das Glyph bleibt einfach
leer. Genau das passierte, sobald WiFi und TLS den Heap zerstückelt hatten (nur
noch 43 KB zusammenhängend): Das Hintergrundbild blieb, alle Texte verschwanden.
Deshalb ist in [stb_truetype.h](main/stb_truetype.h) die Chunk-Größe auf 128/64/32
reduziert (stb hängt bei Bedarf weitere Chunks an), und jede Textausgabe wird
nach dem Zeichnen geprüft – ein Band ohne Textpixel erzeugt eine Fehlerzeile im
Log statt einer leeren Anzeige.

### Warum die Anzeige in Bändern gezeichnet wird

Ein vollständiger Framebuffer (240 × 320 × 2 Byte = **150 KB**) passt auf diesem
ESP32 nicht mehr neben den WiFi-Stack: das DRAM-Segment ist 192 KB groß, die
Bibliotheken belegen davon schon ~40 KB, und WiFi braucht weitere ~40 KB
Sende-/Empfangspuffer. Deshalb zeichnet die Anwendung **zwei Bänder à 160 Zeilen**
(77 KB Puffer) und schickt jedes Band direkt ans Display:

1. `wallpaper_decode()` liest das JPEG in den RAM (14–20 KB) und prüft die Größe
2. pro Band: `wallpaper_paint()` dekodiert das Bild mit **TJpgDec**
   ([main/tjpgd](main/tjpgd)) in voller Auflösung und übernimmt nur die Zeilen des
   Bandes – der Rest wird verworfen. Danach zeichnen `bigdigit`/`infotile` ihre
   Texte mit Bildschirmkoordinaten darüber (das Canvas schneidet alles außerhalb
   des Bandes ab), dann geht das Band per DMA an das Display.

TJpgDec kann nicht anhalten, also läuft pro Band ein vollständiger Decoder-Durchlauf
(das huffman-dekodierte Bild wird ja Zeile für Zeile geliefert). Bei 160 Zeilen
Bandhöhe sind das **zwei Durchläufe** pro Frame, ein Frame dauert damit rund
**275 ms** (davon ~110 ms Dekodieren pro Band). Kleinere Bänder sparen RAM,
kosten aber weitere Durchläufe: `LAYOUT_BAND_HEIGHT 80` = 38 KB und vier
Durchläufe.

### RAM-Budget (Ist-Stand)

| Posten | Größe |
|---|---|
| DRAM-Segment für Daten insgesamt | 192 KB |
| davon statisch (Bibliotheken, WiFi, LVGL-frei, App) | ~40 KB |
| Canvas 240×160 (ein Band) | 77 KB |
| JPEG im RAM (größtes Wallpaper) | ~20 KB |
| TJpgDec-Arbeitsspeicher | 4 KB |
| **frei nach der Initialisierung** | **~166 KB** |
| davon nimmt sich WiFi zur Laufzeit (Puffer, DHCP, lwIP) | ~40–60 KB |

Ein voller Framebuffer hätte 150 KB gekostet und wäre damit nicht mehr
unterzubringen gewesen. Für den HTTP-Server ist mit ~100 KB Rest reichlich Platz;
die HTTPS-Abfrage der Sonnenzeiten belegt während des Handshakes kurzzeitig
~25–30 KB und gibt sie danach wieder frei.

### Layout einstellen – [main/layout.h](main/layout.h)

Alle Werte der Anzeige stehen in einer Datei, geändert wird nur dort. Die
Koordinaten gelten im Hochformat: (0,0) oben links, x bis 239 nach rechts, y bis
319 nach unten.

| Makro | Bedeutung | aktuell |
|---|---|---|
| `LAYOUT_TEMP_X` | linke Kante der ersten großen Ziffer | 25 |
| `LAYOUT_TEMP_BASELINE_Y` | Grundlinie = Unterkante der großen Ziffern; Komma-Schweif und Nachkommastelle enden hier ebenfalls | 160 |
| `LAYOUT_TEMP_MAX_WIDTH` | Breite, auf die die Zeichengruppe höchstens gestaucht wird | 190 |
| `LAYOUT_TEMP_DIGIT_HEIGHT` | Höhe der beiden Ziffern vor dem Komma | 120 |
| `LAYOUT_TEMP_MIN_SQUEEZE` | stärkste horizontale Stauchung; `1.0` = nie stauchen, `0.5` = sehr schmal | 0.55 |
| `LAYOUT_TEMP_DECIMAL_SCALE` | Komma + Nachkommastelle relativ zur Ziffernhöhe | 0.5 |
| `LAYOUT_TEMP_DEGREE_X` | rechte Kante des `°` | 215 |
| `LAYOUT_TEMP_DEGREE_Y` | Oberkante des `°` | 40 |
| `LAYOUT_TEMP_DEGREE_SCALE` | `°` relativ zur Ziffernhöhe | 0.5 |

Die Positionen von Ziffernblock und Gradzeichen sind damit unabhängig: `x` und
Grundlinie gehören zum Zahlentext, `DEGREE_X/Y` bezeichnen die **obere rechte
Ecke der Tinte** des Gradzeichens. Beides lässt sich frei auf dem Bildschirm
platzieren, auch überlappend.

Für die untere Kachel gilt dasselbe Prinzip: ein Rechteck plus drei
Zeilenhöhen, der Block wird darin automatisch mittig gesetzt.

| Makro | Bedeutung | aktuell |
|---|---|---|
| `LAYOUT_INFO_X` / `LAYOUT_INFO_Y` | linke obere Ecke des Textbereichs | 25 / 205 |
| `LAYOUT_INFO_W` / `LAYOUT_INFO_H` | Breite und Höhe des Textbereichs | 190 / 90 |
| `LAYOUT_INFO_HUMIDITY_CAP` | Ziffernhöhe der Feuchtezeile `xx,x %rF` | 24 |
| `LAYOUT_INFO_PRESSURE_CAP` | Ziffernhöhe der Druckzeile `xxx hPa` | 18 |
| `LAYOUT_INFO_TIME_CAP` | Ziffernhöhe der Zeile `dd.mm. hh:mm` | 18 |
| `LAYOUT_INFO_GAP` | Abstand zwischen den Zeilen | 8 |
| `LAYOUT_INFO_MIN_SQUEEZE` | stärkste Stauchung je Zeile; greift sie nicht, wird die Zeile kleiner | 0.6 |

Dazu kommt `LAYOUT_BAND_HEIGHT` (160): die Höhe der Bänder, in denen gezeichnet
wird (siehe oben). Sie muss `BOARD_SCREEN_HEIGHT` ohne Rest teilen und bestimmt
Speicherbedarf **und** Rechenzeit: 160 Zeilen = 77 KB Puffer und zwei
Decoder-Durchläufe, 80 Zeilen = 38 KB und vier Durchläufe.

Jede Zeile wird waagerecht um `LAYOUT_INFO_X + LAYOUT_INFO_W/2` zentriert und
steht mit ihrer Unterkante auf der jeweiligen Grundlinie. Die Summe aus
Zeilenhöhen und Zwischenräumen wird senkrecht im Bereich zentriert – wird
`LAYOUT_INFO_H` geändert, wandert der Block automatisch mit.

Ablauf zum Ausprobieren: Wert ändern → `.\idf build` → `.\idf -p COM8 app-flash`
→ im Monitor die Zeilen ansehen:

```
bigdigit: "22,2" cap 120 px, scale x/y 0.060/0.082, from x=25 on baseline y=160, 187 px wide
bigdigit: degree: 27x25 px ink, top right corner at (215,40)
infotile: area x 25..214, y 205..294, centre line at x=120, block 76 px high from y=212
infotile:   line 1 "47,2 %rF": cap 24 px, baseline y=236, 130 px wide
infotile:   line 2 "967 hPa": cap 18 px, baseline y=262, 92 px wide
infotile:   line 3 "18.10. 13:44": cap 18 px, baseline y=288, 136 px wide
```

Regeln, die der Renderer anwendet:

* Die Geometrie-Zeilen erscheinen einmal pro Frame (nur beim ersten Band), nicht
  viermal.
* Die Ziffern-Gruppe wird horizontal gestaucht, bis sie in `MAX_WIDTH` passt –
  aber höchstens bis `MIN_SQUEEZE`. Ist selbst das zu wenig, verringert der
  Renderer die Ziffernhöhe, damit die Proportionen lesbar bleiben. Für die
  Infozeilen gilt dasselbe mit `LAYOUT_INFO_W` und `LAYOUT_INFO_MIN_SQUEEZE`.
* Komma und Nachkommastelle sitzen mit ihrer Unterkante auf der Grundlinie; der
  Komma-Schweif endet ebenfalls dort.
* Das Gradzeichen wird an `DEGREE_X/Y` ausgerichtet, egal wie groß es ist.
* Feuchte und Druck kommen mit einer bzw. drei Nachkommastellen aus dem Sensor
  (`47,1 %rF` mit deutschem Komma, `967 hPa`). Datum und Uhrzeit kommen aus der
  NTP-Uhr; solange die nicht synchronisiert ist, zeigen die Zeilen die
  Rückfallwerte `APP_FALLBACK_*` aus [main/main.c](main/main.c) (`01.01. 00:00`).

## WLAN, Uhrzeit und Sonnenzeiten – [main/connect.h](main/connect.h), [main/wifi.c](main/wifi.c), [main/clock.c](main/clock.c), [main/sun.c](main/sun.c)

Alles, was zur Installation gehört, steht in **`main/connect.h`**: SSID, Passwort,
Hostname, NTP-Server, Zeitzone und die Koordinaten für sunrise-sunset.org.

```c
#define CONNECT_WIFI_SSID       "FRITZ!Box 7590 j.oe"
#define CONNECT_WIFI_PASSWORD   "…"
#define CONNECT_SNTP_SERVER     "de.pool.ntp.org"
#define CONNECT_TZ              "CET-1CEST,M3.5.0,M10.5.0/3"   /* Europe/Berlin */
#define CONNECT_LATITUDE_TEXT   "48.41490736914154"
#define CONNECT_LONGITUDE_TEXT  "11.099721982616911"
```

`connect.h` enthält das WLAN-Passwort und steht deshalb in [.gitignore](.gitignore).
Im Repository liegt nur die Vorlage [main/connect.example.h](main/connect.example.h):

```powershell
Copy-Item main\connect.example.h main\connect.h   # einmalig, dann Werte eintragen
```

Ablauf beim Start (der Netzwerkteil läuft in einer eigenen Task, die Anzeige
startet also sofort):

1. `clock_init()` setzt die Zeitzone (`setenv("TZ", …)` + `tzset()`), damit
   `localtime_r()` Ortszeit liefert – inklusive Sommerzeit
2. `wifi_connect()`: NVS, `esp_netif`, Event-Loop, Station-Modus, dann warten auf
   die IP-Adresse. Nach einem Verbindungsabbruch wird neu verbunden; war das Gerät
   schon einmal verbunden, endlos (Router-Neustart braucht keinen Geräte-Neustart)
3. `clock_start_sntp()` + `clock_wait_for_sync()`: SNTP-Polling, ohne die Anzeige
   zu blockieren
4. `sun_fetch()` holt Sonnenauf-/-untergang, danach alle
   `CONNECT_SUN_REFRESH_S` (6 h) erneut
5. Die Uhrzeit erscheint in der unteren Kachel, sobald sie da ist – der
   Neuzeichen-Takt (alle 10 s) merkt die Änderung selbst. Bis dahin stehen die
   Rückfallwerte `APP_FALLBACK_*` aus [main.c](main/main.c) (`01.01. 00:00`)

### Woher die Sonnenzeiten kommen

Der Dateninput besteht aus **zwei Zahlen in `connect.h`** und einer daraus
gebauten Adresse:

```c
#define CONNECT_LATITUDE_TEXT   "48.41490736914154"   /* Breite, Nord positiv, Süd negativ */
#define CONNECT_LONGITUDE_TEXT  "11.099721982616911"  /* Länge, Ost positiv, West negativ */
#define CONNECT_SUN_API_URL \
    "https://api.sunrise-sunset.org/json?lat=" CONNECT_LATITUDE_TEXT \
    "&lng=" CONNECT_LONGITUDE_TEXT "&formatted=0"
```

Für einen anderen Aufstellort also nur die beiden Textwerte ändern (Dezimalgrad
mit Punkt, z. B. `"53.5511"` / `"9.9937"` für Hamburg) – URL und Abfrage folgen
automatisch. Werte mit zu vielen Nachkommastellen sind unkritisch, die API
rundet selbst.

Der Ablauf in [sun.c](main/sun.c):

1. HTTPS-GET auf `CONNECT_SUN_API_URL` (`esp_http_client`, Zertifikatsprüfung
   gegen das CA-Bundle im Flash)
2. Antwort ist JSON, daraus werden `status` und die beiden Felder `sunrise` und
   `sunset` gelesen, z. B. `"sunrise":"2026-10-18T05:38:23+00:00"` (UTC)
3. Umrechnung UTC → Ortszeit über `localtime_r()`, also inklusive Sommerzeit
4. Ablage mit `clock_set_sun_times()`; lesbar über `clock_sunrise()` /
   `clock_sunset()` (jeweils `hour`/`minute`/`valid`)

Weil die Antwort nur aus einer flachen Liste fester Felder besteht, extrahiert
`sun.c` die zwei Werte direkt aus dem Text, statt einen JSON-Parser
einzubinden (kein zusätzliches Modul, ~1 KB Flash). Kommt künftig mehr aus der
API, wäre ein richtiger Parser der nächste Schritt.

Bis zur ersten erfolgreichen Abfrage stehen in [clock.c](main/clock.c) die
Platzhalter 07:39 / 18:19 mit `valid = false`; nach dem Abruf sind sie gültig und
`clock_log_now()` protokolliert sie ohne den Zusatz „(placeholder)".

### HTTPS und Zertifikate

`api.sunrise-sunset.org` hängt bei Google Trust Services; das Zertifikat wird von
`WE1` ausgestellt, die Kette endet bei `GTS Root R4` – allerdings in der
**cross-signierten** Variante (ausgestellt von GlobalSign Root CA, die nicht im
CA-Bundle von ESP-IDF liegt). Deshalb steht in
[sdkconfig.defaults](sdkconfig.defaults):

```
CONFIG_MBEDTLS_CERTIFICATE_BUNDLE_CROSS_SIGNED_VERIFY=y
```

Ohne diese Option bricht der Handshake mit `esp-x509-crt-bundle: No matching
trusted root certificate found` ab (`mbedtls_ssl_handshake returned -0x3000`).
Ebenso wichtig: `CONFIG_MBEDTLS_SSL_IN_CONTENT_LEN` bleibt bei 16 KB, denn die
Zertifikatskette kommt als ein TLS-Record und passt nicht in 4 KB.

Die TLS-Sitzung braucht während der Abfrage etwa 25–30 KB Heap; danach ist der
Speicher wieder frei (die Anzeige läuft während der Abfrage weiter, weil sie in
der Thermometer-Task läuft).

## Hardware

| Baustein | Details |
|---|---|
| Modul | ESP32-32E, ESP32-D0WD-V3, 240 MHz, 520 KB SRAM, kein PSRAM, 4 MB QSPI-Flash |
| Display | 2.8" TN-TFT, 240×320, **ILI9341V**, 4-Draht-SPI, Weiß-LED-Backlight an GPIO21 |
| Touch | **XPT2046** (resistiv) an eigenem SPI-Bus, PENIRQ an GPIO36 |
| Sonstiges | RGB-LED (R/G/B = 22/16/17, Low-aktiv, jetzt PWM), BOOT-Taster an GPIO0, microSD, Lautsprecher |

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

* **3 MB App**: der WiFi-Stack allein bringt ~560 KB mit (aktueller Teststand:
  991 KB, also 68 % der Partition frei). Platz für HTTP-Server und TLS ist da.
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
   grün, blau, weiß nacheinander für je 400 ms (steht so im Log); anschließend
   übernimmt die LED-Task (siehe [RGB-LED als Ambilight](#rgb-led-als-ambilight-statusledh))
2. ILI9341 über SPI2 hochfahren (40 MHz) in den **Hochformat-Modus 240×320**
   (ESP-IDF 6 nennt die beiden freien Busse SPI1/SPI2; die Header verwenden die
   weiterhin gültigen Aliase `SPI2_HOST`/`SPI3_HOST`, im Monitor erscheint
   deshalb „SPI1" für das Display und „SPI2" für den Touch)
3. Canvas 240×160 anlegen (77 KB, DMA-fähig) – die Anzeige wird in zwei Bändern
   gezeichnet, siehe [Warum die Anzeige in Bändern gezeichnet wird](#warum-die-anzeige-in-bändern-gezeichnet-wird)
4. littlefs-Partition `storage` einhängen (beim ersten Mal formatieren), Belegung
   und Dateianzahl loggen
5. BME280 über I²C initialisieren (Adresse 0x76, 16× Oversampling); der Init
   kehrt erst zurück, wenn eine vollständige Messung vorliegt
6. XPT2046 initialisieren
7. Zeitzone setzen, **Netzwerk-Task starten** (WLAN + NTP, läuft im Hintergrund)
8. Backlight auf 100 %, LED grün
9. **Thermometer-Schleife**: Wallpaper lesen und prüfen, dann pro Band
   dekodieren (voller Auflösung), Temperatur als `xx,x°` in die obere Kachel
   zeichnen, darunter in der unteren Kachel Feuchte, Luftdruck und Datum/Uhrzeit,
   Band senden. Alle 10 s eine neue Messung; neu gezeichnet wird nur, wenn sich
   ein angezeigter Wert ändert (Zehntelgrad, Zehntel-Prozent Feuchte, ganze hPa,
   Minute).

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

### Welches Bild wann erscheint

Die Regeln stehen als `#define`-Block oben in [main/wallpaper.h](main/wallpaper.h)
und werden von `wallpaper_daytime_for()` / `wallpaper_season_from_month()`
angewendet:

| Wechsel | Auslöser |
|---|---|
| → `morning` | Sonnenaufgang (von sunrise-sunset.org, `clock_sunrise()`) |
| `morning` → `day` | `WALLPAPER_MORNING_END_HOUR` = **9:00**, im Winter `WALLPAPER_MORNING_END_HOUR_WINTER` = **10:00** |
| → `evening` | Sonnenuntergang (`clock_sunset()`) |
| → `night` | `WALLPAPER_NIGHT_START_HOUR` = **23:00** (und nach Mitternacht bis Sonnenaufgang) |

Die Jahreszeit folgt den meteorologischen Blöcken: Dezember–Februar Winter,
März–Mai Frühling, Juni–August Sommer, September–November Herbst.

Beim Wechsel des Slots (Jahreszeit oder Tageszeit) wird **eines der drei Bilder
zufällig** gewählt (`wallpaper_random_index()`); innerhalb eines Slots bleibt es
stehen. Geprüft wird im 10-s-Takt der Messschleife, ein Wechsel erfolgt also
innerhalb von 10 s nach dem auslösenden Zeitpunkt.

Sind die Sonnenzeiten noch nicht bekannt (kein WLAN, erste Sekunden nach dem
Start), gelten die Ersatzwerte `WALLPAPER_SUNRISE_FALLBACK_*` (07:00) und
`WALLPAPER_SUNSET_FALLBACK_*` (19:00). Solange die NTP-Uhr fehlt, rechnet die
Auswahl mit dem Rückfall-Datum (`01.01. 00:00` → Winter/Nacht); sobald die Zeit
da ist, springt der Hintergrund auf den richtigen Slot (im Log als
`cyd: background …` sichtbar).

## RGB-LED als Ambilight – [main/statusled.h](main/statusled.h)

Das Gerät hängt im Acrylrahmen etwa 15 mm vor einer weißen Wand; die RGB-LED
leuchtet diese Wand von hinten aus. Die drei Kanäle hängen jetzt an
**LEDC-PWM** (Timer 0, 20 kHz, 10 Bit – derselbe wie das Backlight, Kanäle 1–3),
deshalb sind Helligkeit und Farbverläufe stufenlos möglich.

| Modus | Verhalten |
|---|---|
| Nacht (`DAYTIME_NIGHT`) | **Regenbogen-Fade** als Ambilight: rot → gelb → grün → cyan → blau → magenta → rot, ein Umlauf pro `STATUSLED_AMBIENT_PERIOD_MS` (60 s), Aktualisierung alle 50 ms (1200 Stufen pro Umlauf) |
| alle anderen Tageszeiten | weiß mit `STATUSLED_IDLE_BRIGHTNESS` (30 % PWM-Tastgrad) |

```c
#define STATUSLED_AMBIENT_PERIOD_MS   60000   /* ein Farbumlauf im Nachtmodus */
#define STATUSLED_AMBIENT_BRIGHTNESS  100     /* Prozent */
#define STATUSLED_IDLE_BRIGHTNESS      30     /* Prozent, weiß */
```

Der Modus folgt der Tageszeit aus `wallpaper_update()` in [main/main.c](main/main.c);
im Log steht pro Wechsel `status LED: rainbow fade (night)` bzw.
`status LED: white, dimmed`. `statusled_set()` (Selbsttest, BOOT-Taste im
Testbild) schaltet die Automatik ab, bis wieder `statusled_ambilight()` aufgerufen
wird.

## Voraussetzungen

* ESP-IDF **v6.1** – auf diesem Rechner installiert unter `C:\esp\v6.1\esp-idf`
  (Espressif Installation Manager, Tools in `C:\Espressif\tools`)
* VS Code mit der ESP-IDF-Erweiterung (`espressif.esp-idf-extension`), Setup
  `v6.1` ist in `.vscode/settings.json` schon ausgewählt
* USB-Datenkabel (Type-C) – das Board wird als USB-Seriell-Adapter erkannt
  (auf diesem Rechner waren zum Zeitpunkt des Erstellens **keine** Ports außer
  zwei Bluetooth-Ports vorhanden, also bitte Board anstecken)
* 2,4-GHz-WLAN in Reichweite; SSID und Passwort in [main/connect.h](main/connect.h)
  (aus [connect.example.h](main/connect.example.h) kopieren)

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
* `espressif/esp_jpeg` – **nicht mehr nötig**: der JPEG-Decoder TJpgDec liegt als
  Quelltext unter [main/tjpgd](main/tjpgd) (R0.03, (C)ChaN 2021, freie Lizenz) und
  wird direkt angesprochen, weil nur er die Ausgabe bandweise wegwerfen kann.
  Konfiguration in [main/tjpgd/tjpgdcnf.h](main/tjpgd/tjpgdcnf.h)

Zum Testen ohne Hardware genügt `.\idf build` – es ist bereits erfolgreich
durchgelaufen (App-Binärdatei 966 KB inklusive 147 KB Schrift und WiFi-Stack in
einer 3008-KB-Partition, Flash 4 MB, CPU 240 MHz).

## Projektstruktur

| Datei | Zweck |
|---|---|
| [main/main.c](main/main.c) | Ablauf: Backlight, LCD-Init, littlefs, Sensor, Netzwerk-Task, Thermometer-Schleife mit Slot-Wechsel (`APP_SHOW_TESTSCREEN` schaltet das Testbild ein) |
| [main/board_e32r28t.h](main/board_e32r28t.h) | Alle Pins, Auflösung, Ausrichtung, SPI-Takt, Farb- und Touch-Kalibrierwerte |
| [main/connect.h](main/connect.h) | **Konfiguration der Installation**: WLAN, NTP, Zeitzone, Koordinaten (nicht im Repository, Vorlage: [connect.example.h](main/connect.example.h)) |
| [main/wifi.c](main/wifi.c), [main/wifi.h](main/wifi.h) | WLAN-Station: NVS, `esp_netif`, Event-Handler, Verbindungsaufbau mit Wiederholung |
| [main/clock.c](main/clock.c), [main/clock.h](main/clock.h) | Zeitzone, SNTP, Ortszeit, Ablage der Sonnenzeiten |
| [main/sun.c](main/sun.c), [main/sun.h](main/sun.h) | Sonnenauf-/-untergang von sunrise-sunset.org holen und umrechnen (HTTPS) |
| [main/wallpaper.c](main/wallpaper.c), [main/wallpaper.h](main/wallpaper.h) | JPEG lesen, bandweise mit TJpgDec in voller Auflösung malen, Slot-Auswahl (Jahreszeit/Tageszeit + Zufallsbild) |
| [main/tjpgd](main/tjpgd) | JPEG-Decoder TJpgDec R0.03 mit eigener Konfiguration (`tjpgdcnf.h`) |
| [main/bigdigit.c](main/bigdigit.c), [main/bigdigit.h](main/bigdigit.h) | Große Temperaturanzeige `xx,x°` mit dem TrueType-Renderer |
| [main/infotile.c](main/infotile.c), [main/infotile.h](main/infotile.h) | Untere Kachel: Feuchte, Luftdruck, Datum/Zeit als drei zentrierte Zeilen |
| [main/layout.h](main/layout.h) | Alle Layout-Werte der Anzeige (beide Kacheln, Ziffernhöhen, Komma-/Gradgröße, Bandhöhe) |
| [main/ttf.c](main/ttf.c), [main/ttf.h](main/ttf.h) | TrueType-Text: `stb_truetype`, Anti-Aliasing, Stauchung |
| [main/fonts/OpenSans-Regular.ttf](main/fonts) | Schrift (Apache 2.0), per `EMBED_FILES` im App-Image |
| [main/stb_truetype.h](main/stb_truetype.h) | TrueType-Rasterizer (public domain), mit einer lokalen Anpassung: kleinere Heap-Chunks (siehe oben) |
| [main/testscreen.c](main/testscreen.c), [main/testscreen.h](main/testscreen.h) | Hardware-Testbild, Statuszeile, Touch-Punkte, BOOT-Neudruck (separat entfernbar) |
| [main/statusled.c](main/statusled.c), [main/statusled.h](main/statusled.h) | RGB-LED über LEDC-PWM: Selbsttest, Regenbogen-Ambilight bei Nacht, sonst gedimmtes Weiß |
| [main/gfx.c](main/gfx.c), [main/gfx.h](main/gfx.h) | Mini-Grafikbibliothek: Canvas (auch als Band), Rechtecke, Linien, Kreis, Text, Flush |
| [main/gfx_color.h](main/gfx_color.h) | RGB565-Farbmakros, ohne ESP-IDF-Abhängigkeit (auch für Datentabellen nutzbar) |
| [main/font5x7.c](main/font5x7.c), [main/font5x7.h](main/font5x7.h) | 5×7-Pixel-Font (A–Z, 0–9, Satzzeichen), generiert |
| [main/touch.c](main/touch.c), [main/touch.h](main/touch.h) | XPT2046: SPI-Bus, `esp_lcd_touch`, Rohwert → Bildschirmkoordinate |
| [main/storage.c](main/storage.c), [main/storage.h](main/storage.h) | littlefs: Mount der `storage`-Partition, Belegung, Dateizählung |
| [main/sensor.c](main/sensor.c), [main/sensor.h](main/sensor.h) | BME280: I²C-Bus, Sensor-Init, Temperatur/Feuchte/Druck |
| [main/background.h](main/background.h) | Tabelle der 48 Wallpaper-Dateinamen und Textfarben je Jahreszeit/Tageszeit |
| [data/](data) | Inhalt der littlefs-Partition (Wallpaper), wird beim Bauen zum Image |
| [main/idf_component.yml](main/idf_component.yml) | Abhängigkeiten aus der Component Registry |
| [tools/gen_font5x7.py](tools/gen_font5x7.py) | Erzeugt den Font neu (Glyphen als ASCII-Art, mit Probedruck) |
| [sdkconfig.defaults](sdkconfig.defaults) | 4 MB Flash, 240 MHz CPU, Ziel esp32, eigene Partitionstabelle, littlefs-Namenslänge 128, Z-Druckschwelle des Touch-Treibers, WiFi-Puffer (RAM), TLS-Optionen für HTTPS |
| [partitions.csv](partitions.csv) | Partitionstabelle: nvs, phy_init, 3 MB App, 1 MB littlefs |
| [idf.cmd](idf.cmd) | Wrapper: `.\idf build` / `.\idf -p COM8 flash monitor` ohne ESP-IDF-Profil |

## Erwartete Ausgabe auf dem seriellen Monitor

```
I (560) cyd: --- E32R28T room thermometer, ESP-IDF v6.1 ---
I (570) cyd: heap: 272640 bytes free, largest free block 139264 bytes
I (600) statusled: RGB LED: red
I (1000) statusled: RGB LED: green
I (1410) statusled: RGB LED: blue
I (1810) statusled: RGB LED: white (all three)
I (2210) statusled: RGB LED: off
I (2570) cyd: SPI1 for ILI9341: SCLK=14 MOSI=13 MISO=12 CS=15 DC=2 RST=EN
I (2690) cyd: ILI9341 ready, 240x320 portrait, 40 MHz SPI clock, BGR element order
I (2690) gfx: canvas 240x160: 76800 bytes
I (2750) storage: littlefs mounted at /littlefs: 1024 KiB total, 36 KiB free
…   (16 Ordnerzeilen, je 3 Dateien)
I (4650) storage: 48 files in total (48 JPEG), 728964 bytes
I (4650) sensor: I2C0 for BME280: SDA=23 SCL=27, 100 kHz, address 0x76
I (5060) sensor: BME280 ready, first reading: 22.08 C, 47.0 %rH, 966.8 hPa
I (5060) touch: SPI2 for XPT2046: SCK=25 MOSI=32 MISO=39 CS=33
I (5060) touch: XPT2046 ready, raw axes 300..3805 / 215..3712, swap=0 mirror=1/0
I (5070) ttf: 147528 byte font loaded, ascent 2189 / descent -600 / gap 0 (font units)
I (5160) clock: time zone Europe/Berlin (CET-1CEST,M3.5.0,M10.5.0/3)
I (5180) cyd: 163112 free heap bytes after initialisation
I (5567) cyd: band 0..159: 5046 text pixels
I (5677) cyd: band 160..319: 875 text pixels
I (5881) wallpaper: /littlefs/backgrounds/summer/evening/ocean-sunset.jpg: 240x320, 14222 bytes JPEG, YCbCr
I (5990) bigdigit: "22,1" cap 120 px, scale x/y 0.060/0.082, from x=25 on baseline y=160, 187 px wide
I (6000) bigdigit: degree: 27x25 px ink, top right corner at (215,40)
I (6010) infotile: area x 25..214, y 205..294, centre line at x=120, block 76 px high from y=212
I (6010) infotile:   line 1 "47,0 %rF": cap 24 px, baseline y=236, 130 px wide
I (6020) infotile:   line 2 "967 hPa": cap 18 px, baseline y=262, 92 px wide
I (6030) infotile:   line 3 "01.01. 00:00": cap 18 px, baseline y=288, 136 px wide
I (6191) cyd: frame: 22,1 degC, text from (25,160), 187 px wide, degree at (215,40), decode 114 ms, total 421 ms
I (6990) wifi: connected to "FRITZ!Box 7590 j.oe", IP 192.168.123.122
I (7000) wifi: link to "FRITZ!Box 7590 j.oe": channel 1, RSSI -43 dBm, IP 192.168.123.122
I (7000) clock: NTP server de.pool.ntp.org, querying ...
I (8100) clock: NTP answer received, 18.10.2026 16:17:12 (Europe/Berlin)
I (8100) clock: clock synchronised after 1100 ms
I (8100) clock: time is 18.10.2026 16:17:12 (Europe/Berlin)
I (8100) clock: sun at 48.41490736914154: rise 07:39 (placeholder), set 18:19 (placeholder)
I (9557) sun: sunrise 07:38, sunset 18:23 (Europe/Berlin)
I (16221) cyd: 18.10.2026 16:17:22 BME280: 22.40 C, 47.2 %rH, 966.8 hPa, IP 192.168.123.122
I (15777) cyd: background 2/1/0 at 18.10. 17:37: …/backgrounds/autumn/day/wild-flowers-desert.jpg, text colour 0xFFFF
I (15777) cyd: status LED: white, dimmed
I (16471) cyd: frame: 22,4 degC, text from (25,160), 187 px wide, degree at (215,40), decode 27 ms, total 276 ms
```

Der erste Frame steht nach etwa 6 s (ohne auf das Netzwerk zu warten), die
Uhrzeit erscheint mit dem nächsten 10-s-Refresh nach dem NTP-Antwort, die
Sonnenzeiten nach der HTTPS-Abfrage. Beispiel für einen Start ohne WLAN:
`W (21520) clock: no NTP answer …`, danach läuft die Anzeige mit `01.01. 00:00`
weiter.

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
| Untere Zeilen zu groß/klein oder falsch verteilt | `LAYOUT_INFO_*_CAP`, `LAYOUT_INFO_GAP` und `LAYOUT_INFO_MIN_SQUEEZE` in [layout.h](main/layout.h) – Kontrolle über die `infotile:`-Zeilen im Log (Blockhöhe und Grundlinien je Zeile) |
| Infozeilen fehlen komplett | `infotile.c` muss in der `SRCS`-Liste von [main/CMakeLists.txt](main/CMakeLists.txt) stehen, sonst fehlt der Aufruf im Image |
| `band … has no text pixels` im Log | Die Textausgabe ist leer geblieben: dafür fehlte `stb_truetype` ein zusammenhängender Puffer. Prüfen, ob `LAYOUT_BAND_HEIGHT`/`LAYOUT_TEMP_DIGIT_HEIGHT` kleiner gestellt wurden bzw. ob der Heap zu knapp ist (Heap-Zeile im Log). Normalerweise darf diese Meldung nicht auftreten |
| Kein Text, Log zeigt `font could not be loaded` | `fonts/OpenSans-Regular.ttf` fehlt oder ist beschädigt; Datei prüfen und neu bauen (`EMBED_FILES` in [main/CMakeLists.txt](main/CMakeLists.txt)) |
| Texte verschwinden nach einigen Sekunden | War ein Fehler in `stb_truetype` (56-KB-Block nach Heap-Fragmentierung durch WiFi/TLS) – behoben durch die kleinere Chunk-Größe in [stb_truetype.h](main/stb_truetype.h). Tritt es wieder auf, steht jetzt `band … has no text pixels` im Log |
| `ttf: no memory for … bytes (free …, largest …)` | Der Renderer konnte einen Zwischenpuffer nicht anlegen (siehe oben) – die Anzeige bleibt in diesem Frame ohne Text |
| Touch trifft um 90° versetzt | Touch-Flags passend zur Anzeige: `BOARD_TOUCH_SWAP_XY` / `BOARD_TOUCH_MIRROR_X/Y` – siehe [Touch-Kalibrierung](#touch-kalibrierung) |
| Bildschirm bleibt dunkel, Log endet mit `cyd: LCD bring-up failed` | Fehlerzeile darüber lesen (SPI/Registrierung). Backlight an GPIO21 prüfen – manche Board-Revisionen nutzen GPIO27 (`BOARD_LCD_PIN_BACKLIGHT`) |
| Bild ist da, aber wie ein Negativ | `BOARD_LCD_INVERT_COLORS` auf `1` setzen |
| Rot und Blau vertauscht | `BOARD_LCD_BGR` auf `0` setzen |
| Bild seitenverkehrt | `BOARD_LCD_MIRROR_X` umschalten (bzw. `BOARD_LCD_MIRROR_Y`) |
| Bild um 90° verdreht | `BOARD_LCD_LANDSCAPE` auf `0` setzen (Hochformat) |
| Streifen, Flackern, Farbrauschen | `BOARD_LCD_PIXEL_CLOCK_HZ` auf `20 * 1000 * 1000` reduzieren |
| `not enough memory for the 240x160 band canvas` | Heap-Optionen/BT abgeschaltet lassen; ein Band braucht 77 KB. Bleibt es knapp: `LAYOUT_BAND_HEIGHT` in [layout.h](main/layout.h) halbieren (80 Zeilen = 38 KB, dafür vier Decoder-Durchläufe) |
| `cannot reach "…" (reason 201 …)` im Log | Access Point nicht gefunden: SSID in [connect.h](main/connect.h) prüfen (Groß-/Kleinschreibung, 2,4 GHz – der ESP32 kann kein 5-GHz-Band), Gerät weiter vom Router entfernt |
| `reason 15` beim Verbinden | Passwort falsch: `CONNECT_WIFI_PASSWORD` in [connect.h](main/connect.h) prüfen |
| `connect.h: No such file or directory` | [connect.h](main/connect.h) fehlt, weil sie nicht im Repository liegt: `Copy-Item main\connect.example.h main\connect.h` und Werte eintragen |
| Uhrzeit bleibt `01.01. 00:00` | Platzhalter aus [main.c](main/main.c): kein WLAN, kein NTP-Antwort innerhalb `CONNECT_SNTP_WAIT_MS`, oder Port 123/UDP von der Firewall blockiert. Testweise anderen Server in `CONNECT_SNTP_SERVER` eintragen (`pool.ntp.org`) |
| `sun: request failed: ESP_ERR_HTTP_CONNECT` mit `No matching trusted root certificate found` | `CONFIG_MBEDTLS_CERTIFICATE_BUNDLE_CROSS_SIGNED_VERIFY=y` fehlt (siehe [HTTPS und Zertifikate](#https-und-zertifikate)) |
| `sun: request failed: ESP_ERR_HTTP_CONNECT` ohne Zertifikatsmeldung | Kein Internetzugang/DNS, `CONNECT_SUN_TIMEOUT_MS` zu klein, oder Port 443 blockiert. Für Details `esp_log_level_set("esp-tls", ESP_LOG_DEBUG)` in [sun.c](main/sun.c) setzen und den Monitor ansehen |
| `sun: API did not answer with status OK` | Der Dienst meldet z. B. `INVALID_REQUEST`: Koordinaten in [connect.h](main/connect.h) prüfen (Punkt als Dezimaltrenner, Bereich −90…90 bzw. −180…180) |
| Sonnenzeiten bleiben bei `(placeholder)` | Die Abfrage läuft alle `CONNECT_SUN_REFRESH_S` (6 h); nach einem Fehlversuch dauert es also bis zu 6 h. Für einen Test den Wert kurz auf 60 setzen |
| `mbedtls_ssl_handshake returned -0x3000` (X509) | Siehe [HTTPS und Zertifikate](#https-und-zertifikate): 16-KB-TLS-Puffer und Cross-Signed-Option |
| Uhrzeit um 1–2 h falsch | Zeitzone: `CONNECT_TZ` in [connect.h](main/connect.h) muss `CET-1CEST,M3.5.0,M10.5.0/3` sein (posix-Regel: `CET` = UTC+1, `CEST` = UTC+2) |
| Wallpaper wirkt unscharf | Sollte nicht mehr vorkommen: seit dem Wechsel auf TJpgDec wird in voller Auflösung dekodiert. Bleibt es unscharf, stammt die geladene Datei aus einem älteren Image – `.\idf -p COM8 flash` schreibt auch die littlefs-Partition neu |
| `wallpaper: … : broken data` / `not a baseline JPEG` | Das JPEG ist beschädigt oder progressiv kodiert; TJpgDec versteht nur Baseline-JPEG. Datei neu speichern (z. B. mit „Progressive" aus) |
| `wallpaper: … work area too small` | `WALLPAPER_WORK_SIZE` in [wallpaper.c](main/wallpaper.c) erhöhen (4 KB reichen für `JD_FASTDECODE 1`) |
| Touch reagiert nicht | `BOARD_TOUCH_ENABLE 1` prüfen, Log der Init-Zeilen ansehen; der Punkt hängt am MISO-Pin GPIO39 |
| Punkt erscheint nicht unter dem Finger | Touch neu kalibrieren, Anleitung im Abschnitt [Touch-Kalibrierung](#touch-kalibrierung) |
| Touch-Koordinaten springen | `BOARD_TOUCH_Z_THRESHOLD` **und** `CONFIG_XPT2046_Z_THRESHOLD` (in `sdkconfig`, Default im Treiber: 400) erhöhen – beide müssen passen. Gemessen auf diesem Board: Z zwischen ~430 (unten links, der schwächste Punkt) und ~2700 (oben rechts); eingestellt sind 250, weil das unberührte Panel nahe 0 liest (Sollte das nicht stimmen, macht sich das durch Punkte ohne Berührung bemerkbar, dann hochsetzen) |
| RGB-LED macht im Betrieb nichts | Modus prüfen: `status LED: white, dimmed` heißt 30 % PWM – das ist bei Tageslicht am Rückwand-Abstrahl kaum zu sehen. Zum Testen `STATUSLED_IDLE_BRIGHTNESS` in [statusled.h](main/statusled.h) auf 100 setzen |
| RGB-LED zeigt keine Farben / nur eine Farbe | Der Selbsttest direkt nach dem Start (rot, grün, blau, weiß) zeigt, ob alle drei Kanäle funktionieren. Fehlt eine Farbe dort, stimmt der Pin nicht (`BOARD_LED_PIN_*`; andere CYD-Revisionen nutzen GPIO4 für rot) oder LED/Vorwiderstand ist defekt |

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
7. ~~Luftfeuchte, Luftdruck, Datum/Zeit in der unteren Glasfläche~~ – erledigt:
   `infotile.c`, drei zentrierte Zeilen im Bereich x 25…214 / y 205…294
8. ~~WLAN + NTP~~ – erledigt: `wifi.c`, `clock.c`, Konfiguration in
   `connect.h`, Zeitzone Europe/Berlin, SNTP im Hintergrund; bis zur ersten
   Antwort zeigen die Zeilen `APP_FALLBACK_*` aus [main/main.c](main/main.c)
9. ~~Sonnenauf-/-untergang von sunrise-sunset.org~~ – erledigt: `sun.c` holt die
   Werte per HTTPS, rechnet sie in Ortszeit um und legt sie in `clock.c` ab
   (`clock_sunrise()` / `clock_sunset()`), Auffrischung alle 6 h
10. ~~Bildauswahl automatisch~~ – erledigt: `wallpaper_daytime_for()` (Sonnenaufgang,
    09:00 bzw. 10:00 im Winter, Sonnenuntergang, 23:00) und
    `wallpaper_season_from_month()`; beim Slot-Wechsel wird eines der drei Bilder
    zufällig gewählt. Regeln und Zeiten stehen als `WALLPAPER_*` in
    [main/wallpaper.h](main/wallpaper.h) –    die Sonnenzeiten können dabei die Grenze zwischen Tag und Nacht genauer ziehen
    als feste Stunden
11. HTTP-Server mit JSON-Endpunkt für Temperatur/Luftfeuchte
12. Feinschliff: nur das betroffene Band neu zeichnen statt beider (das
    JPEG-Dekodieren von ~110 ms pro Band bliebe allerdings)

Optional, nicht für das Raumthermometer nötig:

* microSD-Karte über SPI3 (CS=5) – Achtung: derselbe Bus wie der Touch
* Lautsprecher (GPIO26) mit I2S oder LEDC-PWM, Verstärker über GPIO4
