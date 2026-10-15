# Lightspill

Bildschirm- und Sound-Sync für Philips-Hue-Lampen unter Linux

<p align="center">
  <img src="assets/logo.svg" alt="Lightspill-Logo" width="192" height="192">
</p>

[English version](README.md)

Lightspill lässt deine Philips-Hue-Lampen in Echtzeit deinem Bildschirm folgen, wie es die offizielle Hue-Sync-App unter Windows und macOS tut. Die Lampen blitzen bei hellen Szenen auf und auf Wunsch auch beim Sound deines Computers. Sollen sie dem Bildschirm nicht folgen, zeigen sie stattdessen statische Farben und Szenen.

Es ist ein Fork von [Huenicorn](https://gitlab.com/openjowelsofts/huenicorn) von OpenJowel, siehe [Herkunft und Änderungen](#herkunft-und-änderungen).

| ![Screenshot](screenshots/WebUI.png) |
|:--:|
| *Die Lichtsteuerung* |

> Lightspill ist ein unabhängiges Projekt und steht in keiner Verbindung zu Signify oder Philips, wird von ihnen weder unterstützt noch gesponsert. Philips Hue und Hue Sync sind Marken von Signify Holding.

## Funktionen

* **Bildschirm-Sync**: Jede Lampe folgt dem Bildschirmbereich, den du ihr zuweist, mit bis zu 60 Hz über die Hue-Entertainment-API
* **Aufnahme auf der GPU**: Die Bilder bleiben im Grafikspeicher (DMA-BUF), die Lampenfarben werden auf der GPU berechnet, mit automatischem Ausweichen auf die CPU
* **Highlight-Boost**: Die Lampen blitzen bei plötzlich hellen Momenten auf (Explosionen, Blitze), auf Wunsch auch bei lauten Momenten im Sound des Computers
* **Statische Szenen**: eine Farbe für alle Lampen, eine Farbe pro Lampe, eingebaute oder eigene gespeicherte Szenen, mit einem Klick umgeschaltet
* **Vorschau**: eine 12-sekündige Testsequenz zum Einstellen von Glättung, Helligkeit und Highlight-Boost
* **Desktop-Anwendung**: Fenster und Tray-Icon statt eines Browser-Tabs, mit einem Einrichtungsassistenten, der sich mit der Bridge koppelt
* **Deutsch und Englisch**, nach der Systemsprache

## Voraussetzungen

* Linux mit **Wayland**-Sitzung und Pipewire. X11 wird nicht unterstützt
* Ein xdg-desktop-portal mit Bildschirmaufnahme: KDE Plasma, GNOME, Hyprland, Sway und andere wlroots-Compositoren bringen eines mit
* Eine Philips Hue Bridge (v2 oder Pro) mit einem **Entertainment-Bereich**, eingerichtet in der Hue-App
* Für die Desktop-Anwendung: Qt 6 mit Qt WebEngine. Das Tray-Icon braucht einen Desktop mit Systemabschnitt (Tray)

### Getestet auf

Lightspill wird unter CachyOS mit KDE Plasma (Wayland) und einer NVIDIA-Grafikkarte (proprietärer Treiber) entwickelt und getestet. Der GPU-Pfad nutzt nur herstellerneutrale EGL-Erweiterungen und sollte mit AMD- und Intel-Grafik (Mesa) funktionieren, das ist aber noch nicht getestet. Ohne nutzbare GPU werden die Bilder auf der CPU verarbeitet. Rückmeldungen zu anderen Systemen sind willkommen.

## Installation

### Arch Linux und Ableger (CachyOS, EndeavourOS, Manjaro …)

`dist/arch/PKGBUILD` baut ein Paket und installiert es:

```bash
cd dist/arch
makepkg -si
systemctl --user enable --now lightspill.service
```

Das Paket enthält:

* `lightspill`: den Dienst, der die Lampen steuert, mit seiner systemd-User-Unit
* `lightspill-gui`: die Desktop-Anwendung, im Startmenü als „Lightspill“
* einen Autostart-Eintrag, der beim Login das Tray-Icon startet

### Aus dem Quellcode bauen

Abhängigkeiten unter Arch-basierten Distributionen:

```bash
sudo pacman -S git cmake make gcc pkgconf curl opencv mbedtls pipewire glib2 nlohmann-json glm libglvnd libdrm qt6-base qt6-svg qt6-webengine
```

Lightspill braucht **Mbed TLS 4.x** und **[cpp-httplib](https://github.com/yhirose/cpp-httplib) 0.46 oder neuer**. Liefert deine Distribution ältere Versionen, baue sie selbst. Für cpp-httplib:

```bash
git clone --branch v0.46.0 https://github.com/yhirose/cpp-httplib.git
cmake -S cpp-httplib -B cpp-httplib/build
sudo cmake --install cpp-httplib/build
```

Dann Lightspill bauen:

```bash
cmake -S . -B build -DCMAKE_INSTALL_PREFIX=/usr
cmake --build build -j
sudo cmake --install build
```

Optionen:

* `-DBUILD_GUI=OFF` baut ohne Desktop-Anwendung (ohne Qt). Die Lichtsteuerung läuft dann im Browser unter [127.0.0.1:8215](http://127.0.0.1:8215)
* `-DBUILD_TESTS=ON` baut die Tests, auszuführen mit `ctest --test-dir build`. `TestGpuDownscale` schickt ein bekanntes Bild als DMA-BUF durch die GPU und vergleicht es mit dem exakten CPU-Mittelwert; dafür braucht es `mesa` für `libgbm`

## Benutzung

### Erster Start

Starte „Lightspill“ aus dem Startmenü. Solange keine Bridge gekoppelt ist, führt ein Einrichtungsassistent durch:

1. **Bridge finden**: Sie wird automatisch gesucht (über den Discovery-Dienst von Philips, braucht Internet), oder du gibst ihre IP-Adresse ein
2. **Koppeln**: Drücke den runden Knopf oben auf der Bridge. Die App fragt alle 2 Sekunden nach und koppelt sich, sobald der Knopf gedrückt ist
3. **Entertainment-Bereich**: Der Assistent prüft, ob einer existiert, und erklärt sonst, wie du ihn in der Hue-App anlegst
4. **Bildschirmauswahl**: Der Desktop fragt, welchen Bildschirm Lightspill aufnehmen darf. Erlaube, dass die Auswahl gespeichert wird, damit die Frage nicht bei jedem Start kommt

Danach in der Lichtsteuerung die Lampen von „Verfügbar“ nach „Aktiv“ ziehen, eine Lampe auswählen und den Bildschirmbereich anpassen, dem sie folgt, und das Profil speichern.

### Desktop-Anwendung und Dienst

Die Lampen steuert ein Dienst (systemd-User-Unit `lightspill.service`). Die Desktop-Anwendung zeigt seine Oberfläche:

* Linksklick auf das Tray-Icon: Fenster öffnen oder schließen
* Rechtsklick: Sync an/aus, statische Szenen, Vorschau, Dienst starten (wenn er nicht läuft)

Das Fenster zu schließen lässt das Tray-Icon stehen. Das Tray-Icon zu schließen lässt die Lampen weiterlaufen: Sie steuert allein der Dienst. Ein erneuter Start holt das laufende Fenster nach vorn.

Ausgaben des Dienstes:

```bash
journalctl --user -u lightspill.service -f
```

### Lichtsteuerung

* **Bildschirmbereiche**: der Teil des Bildschirms, dem jede aktive Lampe folgt, mit einer Gammakorrektur pro Lampe
* **Sync** (Kopfzeile): wechselt zwischen Bildschirm und statischer Szene. Ohne Sync pausieren Bildschirmaufnahme und Sound-Analyse
* **Statische Szene**: eine Farbe für alle Lampen, eine Farbe pro Lampe, eingebaute Szenen, deren Farben von links nach rechts über die Lampen verteilt werden, und eigene gespeicherte Szenen. Eine Auswahl schaltet Sync aus
* **Einstellungen**: Helligkeit, Übergangsglättung (Vorlagen „Gaming“ und „Film“), Highlight-Boost (Vorlagen von „Dezent“ bis „Intensiv“), Reaktion auf Sound, Aufnahmeauflösung und Bildwiederholrate
* **Vorschau**: spielt Farbschnitte, Blitze links, rechts und beidseitig, einen langsamen Verlauf und einen Schnitt von Schwarz auf Weiß mit den aktuellen Einstellungen ab

Jedes Bedienelement erklärt sich in einem Tooltip.

### Verarbeitung auf der GPU

Der Compositor liefert die Bilder als DMA-BUF im Grafikspeicher. Lightspill importiert sie ohne Kopie über EGL und berechnet die Lampenfarben auf der GPU: Ein Shader verkleinert das Bild auf die Subsample-Auflösung, zwei weitere Durchläufe mitteln den Bereich jeder Lampe, und zurückgelesen wird nur ein Pixel pro Lampe. Das Log zeigt den aktiven Pfad:

```
[gpu] Using EGL device /dev/dri/renderD128 (NVIDIA)
Capture path: GPU (DMA-BUF, modifier 0x0)
```

Scheitert der Import, weicht Lightspill auf Shared Memory und die CPU aus (`Capture path: CPU (shared memory)`). `LIGHTSPILL_DISABLE_GPU=1` erzwingt den CPU-Pfad, z. B. zum Vergleichen.

## Konfiguration

Die Konfiguration liegt in `~/.config/lightspill/`. Eine Konfiguration von Huenicorn oder eines früheren Namens von Lightspill wird beim ersten Start übernommen.

* `config.json`: Bridge, Zugangsdaten und Einstellungen
* `profile.json`: die Bildschirmbereiche der Lampen für einen Entertainment-Bereich

### config.json

Die meisten Werte änderst du in der Lichtsteuerung. Von Hand lassen sich alle bearbeiten, solange Lightspill nicht läuft.

* **boundBackendIP**: (String) Adresse, auf der die Lichtsteuerung lauscht. Standard `127.0.0.1` (nur lokaler Zugriff). Mit `0.0.0.0` ist sie von anderen Geräten erreichbar, ohne jede Authentifizierung
* **bridgeAddress**: (String) Adresse der Hue Bridge
* **credentials**: **username** und **clientkey** (Strings), beim Koppeln erzeugt
* **profileName**: (String) Name des Lampenprofils
* **refreshRate**: (Unsigned) Aufnahme- und Streamingrate. Höchstens 60 Hz: Die Bridge gibt Farben mit etwa 25 Hz an die Lampen weiter, Philips empfiehlt 50–60 Hz Streamingrate
* **restoreToken**: (String) merkt sich die Bildschirmauswahl
* **restServerPort**: (Unsigned) Port der Lichtsteuerung, standardmäßig 8215
* **subsampleWidth**: (Unsigned) Breite des verkleinerten Bildes, auf dem die Bereiche gemittelt werden
* **brightness**: (Float) Helligkeit aller Lampen, von 0 bis 1
* **transitionSmoothing**: (Float) wie weich die Lampen zu einer neuen Farbe übergehen, von 0 (sofort) bis 0,97
* **highlightBoost**: (Float) Stärke der Lichtblitze bei plötzlicher Aufhellung, von 0 (aus) bis 1. Steigt die ungeglättete Helligkeit im Bereich einer Lampe von einem Bild zum nächsten um mehr als 5 %, löst das aus, voll ab 30 %; der Blitz klingt mit einer Zeitkonstante von 250 ms ab
* **audioReactive**: (Boolean) ob laute Momente im Sound des Computers den Highlight-Boost ebenfalls auslösen. Lightspill hört dafür über Pipewire den Monitor des Standard-Ausgabegeräts mit (kein Mikrofon). Ein Puffer mehr als 6 dB über dem Mittel der letzten 0,5 s löst aus, voll ab 12 dB; Sound unter −50 dBFS nie. Linker und rechter Kanal werden nach ihrer Position über die Lampen verteilt
* **syncEnabled**: (Boolean) ob die Lampen dem Bildschirm folgen oder die statischen Farben zeigen
* **staticColors**: (Objekt) statische Farbe je Kanal-ID, als `#rrggbb`
* **scenes**: (Objekt) gespeicherte Szenen: Name → Farben wie in `staticColors`
* **language**: (String) `auto` (Systemsprache, Englisch, wenn sie weder Deutsch noch Englisch ist), `en` oder `de`

### profile.json

* **entertainmentConfigurationId**: (String) UUID des Entertainment-Bereichs
* **channels**: (Array) die Lampen des Bereichs, jeweils mit **channelId**, **active**, **devices**, **gammaFactor** und **uvs** (obere linke Ecke **uvA** und untere rechte Ecke **uvB** ihres Bildschirmbereichs, von 0 bis 1)

## Fehlerbehebung

### Es wird kein Entertainment-Bereich angezeigt

Die Zugangsdaten sind womöglich ungültig geworden. Beende Lightspill, lösche `~/.config/lightspill/config.json` und starte es neu: Der Einrichtungsassistent koppelt sich erneut mit der Bridge.

### Einen anderen Bildschirm wählen

Beende Lightspill, entferne die Zeile mit `restoreToken` aus `~/.config/lightspill/config.json` und starte es neu: Der Desktop fragt erneut nach dem Bildschirm.

### Die Lampen reagieren nicht

* Womöglich streamt eine andere Anwendung an den Entertainment-Bereich (die Hue-App oder Hue Sync auf einem anderen Computer): Die Bridge nimmt nur einen Stream gleichzeitig an. Lightspill versucht es alle 5 Sekunden erneut
* Prüfe das Log mit `journalctl --user -u lightspill.service`

## Herkunft und Änderungen

Lightspill ist ein Fork von [Huenicorn](https://gitlab.com/openjowelsofts/huenicorn) 1.2.0 von OpenJowel und seinen Mitwirkenden, dessen Historie es beibehält. Die wichtigsten Änderungen seit dem Fork:

* Die Bilder werden aus DMA-BUFs auf der GPU verarbeitet, mit der CPU als Ausweichweg; die CPU-Last sank von etwa 45 % auf etwa 3 % eines Kerns
* Highlight-Boost aus Bild und Sound, Helligkeit, Glättungsvorlagen, statische Farben und Szenen, Vorschausequenz
* Desktop-Anwendung mit Tray-Icon, Einrichtungsassistent, neue Oberfläche, deutsche Übersetzung
* Streaming auf das begrenzt, was die Bridge verarbeitet, Verbindungsaufbau im Hintergrund, viele Fehlerbehebungen
* Unterstützung für X11, andere Plattformen und die CPU-Interpolationsarten wurde entfernt, um den Code schlank zu halten

## Lizenz

Lightspill ist freie Software unter der GNU General Public License Version 3, wie Huenicorn. Siehe [LICENSE.txt](LICENSE.txt). Du darfst es nutzen, untersuchen, weitergeben und verändern; veränderte Versionen, die du weitergibst, müssen unter derselben Lizenz stehen und ihren Quellcode mitliefern.

## Danksagungen

* [Huenicorn](https://gitlab.com/openjowelsofts/huenicorn) von OpenJowel und seinen Mitwirkenden
* [Beispiel für Bildschirmaufnahme über das XDG-Portal](https://codeberg.org/metamuffin/xdg-dp-start/src/branch/master/xdg-dp-start.c)
* [Pipewire-Dokumentation](https://docs.pipewire.org/page_tutorial5.html)
* [Umrechnung von RGB nach xy (D65)](https://gist.github.com/popcorn245/30afa0f98eea1c2fd34d)
* [Huestacean](https://github.com/BradyBrenot/huestacean)
