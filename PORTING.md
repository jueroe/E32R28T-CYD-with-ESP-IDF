# PORTING – dieses Projekt als Vorlage für die nächsten

Diese Datei ist für die zwei geplanten Folgeprojekte gedacht:

1. **Außensensor** – SHT31 an einem ESP32-C3 super mini, stellt Temperatur und
   Feuchte als HTTP/JSON bereit (Gegenstück zum Feld `remote` hier)
2. **Wetterstation** – JC8012P4A1 mit ESP32-P4 + ESP32-C6-MINI-1U (32 MB PSRAM,
   16 MB Flash), zeigt die Werte aller Räume an

Sie beschreibt, **was übernommen wird**, **was angepasst werden muss** und
**welche Fallen schon bezahlt sind**. Das Rad muss nicht zweimal erfunden werden.

Verwandte Dateien:

| Datei | Zweck |
|---|---|
| [README.md](README.md) | das vollständige Handbuch dieses Projekts (RAM, Layout, Fehlersuche) |
| [.github/copilot-instructions.md](.github/copilot-instructions.md) | Regeln für die KI-Unterstützung – **in das neue Repo kopieren** |
| `main/board_e32r28t.h` | Beispiel für die Trennung „alle Pins in einem Header" |

---

## 1. Was übertragbar ist

| Datei | Rolle | Anpassung im neuen Projekt |
|---|---|---|
| `main/netcfg.c/.h` | WLAN, Standort, Gerätename, Außensensor-IP im NVS; `netcfg_hostname()` | keine (nur `connect.h`-Defaults setzen) |
| `main/configmode.c/.h` | Access Point, Einrichtungsseite, Speichern + Neustart | TFT-Teile entfernen, wenn es kein Display gibt |
| `main/webserver.c/.h` | `GET /api/values` als JSON, `/` als Browserseite | Wertequelle tauschen (SHT31 statt BME280) |
| `main/discovery.c/.h` | mDNS `<name>.local`, `_http._tcp` | keine |
| `main/textutil.c/.h` | HTML-/JSON-Maskierung | keine |
| `main/sensor.c/.h` | Muster: Sensor abstrahieren + **letzten Messwert cachen** (`sensor_last()`) | Treiber ersetzen, Cache-Muster behalten |
| `main/gfx.c/.h`, `main/ttf.c/.h`, `main/fonts/`, `main/tjpgd/` | Zeichnen, TrueType, JPEG | nur bei Geräten mit Display |
| `main/layout.h` | alle Koordinaten/Maße an einer Stelle | neue Werte, Aufbau übernehmen |
| `main/clock.c/.h` | NTP + Zeitzone, Modulname bewusst **nicht** `time` | keine |
| `main/sun.c/.h`, `main/wallpaper.c/.h`, `main/statusled.c/.h` | Sonnenzeiten, Bildauswahl, Ambilight | nur bei Bedarf |

Nicht übertragbar (board- und projektgebunden): `board_e32r28t.h`, `partitions.csv`,
`sdkconfig.defaults`, die Wallpaper in `data/`.

---

## 2. Ziel A: SHT31-Außensensor (ESP32-C3 super mini)

Der einfachste Fall – kein Display, keine Bilder, nur Sensor + Netzwerk.

1. Neues ESP-IDF-Projekt, `netcfg`, `configmode` (ohne TFT), `webserver`,
   `discovery`, `textutil` übernehmen
2. `sensor.c` gegen SHT31 tauschen (I²C, Adresse 0x44 oder 0x45, Kommando 0x2400
   „measure, high repeatability", danach ~15 ms warten und 6 Bytes lesen;
   Umrechnung `-45 + 175 * raw / 65535` bzw. `100 * raw / 65535`)
3. JSON-Schema **identisch** zu diesem Gerät halten (`temperature_c`,
   `humidity_rh`, `valid`, `age_s`), damit die Wetterstation später beide Geräte
   gleich behandeln kann. Druck einfach weglassen
4. Konfigurationsmodus ohne Anzeige: Der Modus startet dann nur über den
   fehlenden Netzzugang, nicht per Touch. Alternativ den BOOT-Taster (GPIO9) mit
   `configmode_request()` verbinden – 5 Zeilen, sehr praktisch beim Einrichten
5. Stromverbrauch: Der C3 kann WiFi-Modem-Sleep nutzen
   (`CONFIG_ESP_WIFI_SLP_IRAM_OPT`, `esp_wifi_set_ps(WIFI_PS_MIN_MODEM)`), wenn er
   nur alle paar Minuten antwortet – aber Achtung, dann antwortet der Webserver
   träger

**Randbedingungen C3:** Single-Core (RISC-V), ~400 KB SRAM, kein PSRAM. Kein
Bandrendering nötig, kein RAM-Sparzwang wie auf dem E32R28T. Für den
Konfigurationsmodus `CONFIG_ESP_WIFI_SOFTAP_SUPPORT=y` **direkt in der
`sdkconfig`** setzen (siehe Falle 3).

## 3. Ziel B: Wetterstation (ESP32-P4 + ESP32-C6)

Hier ändert sich die Netzwerkseite grundlegend, die Anzeigeseite wird einfacher.

1. **Zuerst das Board zum Laufen bringen**: Espressif-Beispiel für den P4 bzw.
   das Board-SDK als Ausgangspunkt, Panel über `esp_lcd` (beim P4 meist
   MIPI-DSI, `esp_lcd_mipi_dsi`) – Board-Datenblatt prüfen, nicht raten
2. **Netzwerk über den C6**: Der P4 hat kein eigenes WLAN. Dafür kommen
   `esp_hosted` (Firmware auf dem C6, Anbindung SDIO/SPI) und `esp_wifi_remote`
   ( stumpfe `esp_wifi_*`-API auf dem P4) dazu. Die `esp_netif`-Aufrufe bleiben
   gleich, aber **`wifi.c` muss angepasst werden**: Modus wechseln, Scan,
   Access Point und `esp_wifi_sta_get_ap_info()` laufen über die Remote-Schicht.
   Diesen Schritt zuerst isoliert testen (verbinden, IP, Scan), bevor die
   Einrichtungsseite portiert wird
3. **Anzeige**: Bandrendering ist hier unnötig – 32 MB PSRAM erlauben einen
   vollen Framebuffer. In `gfx.c` genügt `origin_y = 0` und
   `LAYOUT_BAND_HEIGHT = BOARD_SCREEN_HEIGHT`; die Bandlogik bleibt als Option
   erhalten. Danach: doppelt gepufferte Frames, Bilder vorab dekodieren
4. **Räume anzeigen**: Die Messwerte der anderen Geräte per
   `http://<ip-oder-name>.local/api/values` holen (Muster: `sun_fetch()` in
   `main/sun.c`, nur HTTP statt HTTPS) und in einer Tabelle/Kachel darstellen.
   Geräteliste als Tabelle in einer eigenen Konfigurationsdatei oder im NVS
5. **Schriften**: Auf dem P4 lohnt es, die TTF-Rasterisierung einmal pro
   Textänderung in einen PSRAM-Puffer zu rendern und nur bei Änderung neu – bei
   800×1280 ist das der Unterschied zwischen flüssig und ruckelig

---

## 4. Die schon bezahlten Fallen

Diese Liste ist der eigentliche Wert dieses Projekts – jede Zeile hat einmal
einen Abend gekostet.

1. **`stb_truetype` fordert still zu viel Heap.** Der Zeichen-Cache reservierte
   56 KB pro Glyph; nach WiFi/TLS war kein zusammenhängender Block mehr frei und
   die Glyphen blieben **ohne Fehlermeldung** leer. Lösung: Chunk-Größe in
   `stb_truetype.h` auf 128/64/32 senken, Allokationsfehler protokollieren,
   Selbsttest „Textpixel pro Band zählen".
2. **HTTPS auf api.sunrise-sunset.org scheitert mit `-0x3000`.** Das ist
   `MBEDTLS_ERR_X509_FATAL_ERROR` wegen des cross-signierten GTS-Root-R4 →
   `CONFIG_MBEDTLS_CERTIFICATE_BUNDLE_CROSS_SIGNED_VERIFY=y`. Außerdem reichen
   4 KB TLS-Puffer nicht: `CONFIG_MBEDTLS_SSL_IN_CONTENT_LEN=16384`.
3. **`sdkconfig.defaults` wirkt nur, wenn keine `sdkconfig` existiert.** Bei
   bestehendem Projekt muss die Option **direkt in der `sdkconfig`** geändert
   werden – sonst bleibt z. B. `CONFIG_ESP_WIFI_SOFTAP_SUPPORT=n` und der Linker
   meldet `undefined reference to esp_netif_create_default_wifi_ap`.
4. **Zwei HTTP-Server auf Port 80 kollidieren lautlos.** Beide binden mit
   `SO_REUSEADDR` erfolgreich, und die Anfrage landet beim falschen Server.
   Vor dem Start des zweiten Servers `httpd_stop()` des ersten abwarten.
5. **Header-Namen:** ein `main/time.h` verdeckt `<time.h>`, ein `main/mdns.h` den
   Header der mDNS-Komponente. Deshalb heißen die Module hier `clock` und
   `discovery`.
6. **`esp_wifi_scan_start()` braucht ein gestartetes Interface** – im
   Access-Point-Betrieb also Modus `WIFI_MODE_APSTA`, sonst schlägt der Scan fehl.
   Der Scan während laufendem AP wirft Clients kurz raus (Kanalwechsel).
7. **Der DHCP-Hostname braucht die richtige Länge**: maximal 32 Zeichen,
   `esp_netif_set_hostname()` gibt sonst einen Fehler zurück, der leicht
   übersehen wird.
8. **Resistiver Touch (XPT2046):** Rohwerte kalibrieren, Achsen können vertauscht
   oder gespiegelt sein (`BOARD_TOUCH_SWAP_XY`, `_MIRROR_X/Y`), und die
   Z-Druckschwelle ist boardspezifisch (`CONFIG_XPT2046_Z_THRESHOLD`).
9. **`nvs_flash_init()` genau einmal** und früh – NVS wird von WiFi, unseren
   Einstellungen und der Feldstärke-Kalibrierung gemeinsam genutzt.
10. **Konfigurationsmodus endet im Neustart.** Ein Neustart ist der einzige
    Zustand, der garantiert sauber ist, wenn der Netzwerkteil in einer eigenen
    Task läuft. Dazu der NVS-Merker „übersprungen", sonst hängt ein Gerät ohne
    Netz bei jedem Start in der Einrichtung.
11. **`%.1f` in `printf` ist riskant**, wenn `CONFIG_NEWLIB_NANO_FORMAT` aktiv
    ist. Zahlen mit Zehnteln deshalb aus Ganzzahlen bauen
    (`wert/10`, `wert%10`) – so macht es `infotile.c`.
12. **Flash-Größe prüfen, bevor OTA geplant wird**: 4 MB reichen nicht für zwei
    App-Images, deshalb hier keine OTA-Partition. Beim P4 mit 16 MB ist OTA
    dagegen problemlos.

---

## 5. Checkliste für ein neues Projekt

- [ ] `sdkconfig.defaults` früh festlegen (Flash-Größe, Partitionen, CPU-Takt)
- [ ] Pins in `board_<name>.h` sammeln, nichts hart im Code
- [ ] `netcfg` + `webserver` + `discovery` zuerst – damit ist das Gerät ohne
      Neuflashen bedienbar, was jede weitere Iteration beschleunigt
- [ ] JSON-Schema über alle Geräte gleich halten
- [ ] `.github/copilot-instructions.md` aus diesem Repo kopieren
- [ ] README mit „Fehlersuche"-Tabelle beginnen, sobald der erste Abend verloren
      ging – nicht am Ende

## 6. Diese Datei im Chat nutzen

Im neuen Projekt-Chat genügt ein Verweis, z. B.:

> Lies `C:\Users\joe\Documents\ESP-IDF\Projects\E32R28T_erste_Tests\PORTING.md`
> und lege danach das Projekt für den SHT31-Außensensor an.

Besser ist, `PORTING.md` und `.github/copilot-instructions.md` gleich in das neue
Repository zu kopieren – dann liegen sie im Arbeitsverzeichnis und werden ohne
Pfadangabe gefunden.
