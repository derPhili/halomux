# Philip's Hue Desktop Sync

Ein freier Bildschirm-Synchronisierer für Philips Hue™ unter GNU/Linux

<p align="center">
  <img src="assets/logo.svg" alt="Logo" width="256" height="256">
</p>

## Beschreibung

Philip's Hue Desktop Sync ist ein Ambilight-Treiber für GNU/Linux.
Er überträgt die farbige Atmosphäre deines Computerbildschirms in Echtzeit auf deine Hue-Lampen.
Über eine einfache Weboberfläche weist du einzelnen Lampen bestimmte Bildschirmbereiche zu und speicherst die Einstellungen für spätere Sitzungen.

### Screenshot

| ![Screenshot](screenshots/WebUI.png) |
|:--:|
| *Bildunterschrift: Weboberfläche von Philip's Hue Desktop Sync* |

## Projektstatus

Privater Fork von [Huenicorn](http://huenicorn.org) 1.2.0 (OpenJowel), zugeschnitten auf genau ein System:

* CachyOS (Arch-basiert)
* KDE Plasma unter Wayland (KWin, xdg-desktop-portal-kde)
* Bildschirmaufnahme über Pipewire
* NVIDIA GeForce GTX 1080 (proprietärer Treiber), Verkleinern der Bilder auf der GPU
* Philips Hue Bridge mit Entertainment-Bereich

Andere Plattformen, X11, Gamescope und NixOS werden nicht unterstützt.

## Erste Schritte

Bevor du Hue Desktop Sync verwendest, musst du über die offizielle App von Philips einen Entertainment-Bereich auf deiner Hue Bridge anlegen.

### Abhängigkeiten

```bash
sudo pacman -S git cmake make gcc pkgconf curl opencv mbedtls pipewire glib2 nlohmann-json glm libglvnd libdrm mesa
```

`libglvnd` liefert EGL und GLES (die Implementierung kommt vom NVIDIA-Treiber), `mesa` nur `libgbm` für den GPU-Test.

Benötigt wird Mbed TLS 4.x.

[cpp-httplib](https://github.com/yhirose/cpp-httplib) (0.46 oder neuer) ist auf diesem System von Hand nach `/usr/local` installiert:

```bash
git clone --branch v0.46.0 https://github.com/yhirose/cpp-httplib.git
cmake -S cpp-httplib -B cpp-httplib/build
sudo cmake --install cpp-httplib/build
```

### Hue Desktop Sync bauen

```bash
git clone http://192.168.0.250:3000/philip/huenicorn.git
cd huenicorn
cmake -S . -B build
cmake --build build -j
```

Das Ergebnis ist die eigenständige ausführbare Datei `build/hue-desktop-sync` (die Weboberfläche ist eingebettet).

Tests (optional; `TestGpuDownscale` schickt ein bekanntes Bild als DMA-BUF durch die GPU und vergleicht es mit dem exakten CPU-Mittelwert):

```bash
cmake -S . -B build -DBUILD_TESTS=ON
cmake --build build -j
ctest --test-dir build
```

### Programm ausführen

* Starte die Datei „hue-desktop-sync“ auf die Art, die dir am liebsten ist (im Terminal, wenn du Textausgaben sehen möchtest).
* Öffne deinen Browser unter [127.0.0.1:8215](http://127.0.0.1:8215).
  Bei der Ersteinrichtung und solange noch kein Lampenprofil gespeichert ist, öffnet sich der Browser automatisch.
  (Der Port lässt sich in der Konfiguration ändern. Standard ist 8215.)

#### Bildverarbeitung auf der GPU

KWin liefert die Bilder als DMA-BUF direkt im Grafikspeicher. Hue Desktop Sync importiert sie ohne Kopie per EGL und rechnet die Lampenfarben vollständig auf der GPU aus:

1. Ein GLES-Shader mittelt jeden Bildblock exakt auf die eingestellte Subsample-Auflösung (z. B. 160×90).
2. Zwei weitere Durchläufe mitteln den Bereich jeder Lampe: erst pro Zeile, dann über die Zeilen. So läuft keine lange Schleife in einem einzelnen GPU-Thread, auf die der Treiber aktiv warten müsste.
3. Zurückgelesen wird nur ein Pixel pro Lampe.

Die Lampenbereiche werden auf CPU und GPU identisch in Pixel umgerechnet (`ImageProcessing::subImageRect`), beide Pfade liefern dieselben Farben. Im Log steht der aktive Pfad:

```
[gpu] Using EGL device /dev/dri/renderD128 (NVIDIA)
Capture path: GPU (DMA-BUF, modifier 0x0)
```

Klappt der Import nicht, wechselt Hue Desktop Sync automatisch auf Shared Memory und verkleinert auf der CPU (`Capture path: CPU (shared memory)`). Mit `HUE_DESKTOP_SYNC_DISABLE_GPU=1` lässt sich der CPU-Pfad erzwingen, z. B. zum Vergleichen.

#### Als systemd-Dienst

Hue Desktop Sync kann als systemd-User-Dienst mit der grafischen Sitzung starten. Die Unit liegt unter `dist/systemd/hue-desktop-sync.service` und startet `~/.local/bin/hue-desktop-sync`. Die Ersteinrichtung und die erste Bildschirmauswahl unter Wayland sollten vorher einmal von Hand erledigt sein.

Installieren und aktivieren:

```bash
install -Dm755 build/hue-desktop-sync ~/.local/bin/hue-desktop-sync
install -Dm644 dist/systemd/hue-desktop-sync.service ~/.config/systemd/user/hue-desktop-sync.service
systemctl --user daemon-reload
systemctl --user enable --now hue-desktop-sync.service
```

Nach einem neuen Build die installierte Datei ersetzen und den Dienst neu starten:

```bash
install -Dm755 build/hue-desktop-sync ~/.local/bin/hue-desktop-sync
systemctl --user restart hue-desktop-sync.service
```

Ausgaben ansehen:

```bash
journalctl --user -u hue-desktop-sync.service -f
```

#### Ersteinrichtung

Beim ersten Start öffnet Hue Desktop Sync die Einrichtungsseite in deinem Browser. Ein Schritt-für-Schritt-Assistent hilft dir, deine Hue Bridge im Netzwerk zu finden und einen Benutzer darauf anzulegen.

Nach erfolgreichem Abschluss der Einrichtung setzt Hue Desktop Sync seinen normalen Ablauf fort.

Lade die Webseite anschließend neu, um zur Weboberfläche zu gelangen.

#### Normaler Ablauf

Beim ersten Start fordert KDE über das Bildschirmfreigabe-Portal auf, eine Quelle auszuwählen. Wähle den gewünschten Bildschirm und bestätige mit „Teilen“. Die Auswahl wird als `restoreToken` gespeichert, spätere Starts kommen ohne Dialog aus.

Beim Start sucht Hue Desktop Sync nach einem gespeicherten Profil und beginnt mit dem Streamen der Farben.

Um Lampen bestimmten Bildschirmbereichen zuzuweisen, öffne die Weboberfläche und ziehe die Lampen unter „Lamps“ per Drag-and-Drop von „Available“ nach „Active“. Jede aktive Lampe zeigt dort und in ihrem Bildschirmbereich live ihre aktuelle Farbe; die Kopfzeile zeigt, ob GPU oder CPU rechnet.

Ist eine synchronisierte Lampe in der Liste ausgewählt, kannst du ihren Bildschirmbereich im Abschnitt zur Bildschirmaufteilung anpassen.

Bist du mit der Aufteilung zufrieden, speichere sie als Profil. Es wird bei späteren Starts automatisch geladen.

Solange kein Profil gespeichert ist, öffnet sich die Lampenverwaltung beim Start automatisch.

### Konfigurationsdateien

Die Konfigurationsdateien liegen in deinem Home-Verzeichnis unter ~/.config/hue-desktop-sync/.
config.json enthält die Konfiguration rund um die Bridge.
profile.json enthält die gespeicherte Kanalaufteilung für eine Entertainment-Konfiguration.

Beide Dateien sind im JSON-Format.

#### config.json

Diese Datei enthält Parameter, die nach der Ersteinrichtung von Hand geändert werden können.

* **boundBackendIP**: (String) IP-Adresse, an die der Dienst gebunden wird. Standard ist `127.0.0.1` (nur lokaler Zugriff). Mit `0.0.0.0` ist die Weboberfläche auch von anderen Geräten erreichbar – bedenke dabei, dass sie keine Authentifizierung hat
* **bridgeAddress**: (String) Adresse der Philips Hue Bridge
* **credentials**:
  * **clientkey**: (String) Schlüssel aus 32 Hexadezimalzeichen, erzeugt bei der Ersteinrichtung
  * **username**: (String) Benutzername (Base64 oder UUID), erzeugt bei der Ersteinrichtung
* **profileName**: (String) Name des aktuellen, benutzerdefinierten Lampenprofils
* **refreshRate**: (Unsigned) Frequenz von Bildschirmaufnahme und Lampenaktualisierung. Höchstens 60 Hz: Die Bridge gibt Farben mit etwa 25 Hz an die Lampen weiter, Philips empfiehlt 50–60 Hz Streamingrate. Höhere Werte werden beim Start und in der Weboberfläche auf 60 Hz begrenzt
* **restoreToken**: (String) Token, um die Einstellungen der letzten Sitzung zu merken (nur Wayland)
* **restServerPort**: (Unsigned) Port, auf dem die Weboberfläche antwortet
* **subsampleWidth**: (Unsigned) Breite des verarbeiteten, verkleinerten Bildes
* **brightness**: (Float) Globale Helligkeit aller Lampen, zwischen 0 und 1 (in der Weboberfläche 0–100 %). Wirkt nach Gamma und Glättung wie ein Dimmer
* **audioReactive**: (Boolean) Ob zusätzlich der PC-Sound Lichtblitze auslöst (Schalter „React to sound“). Hue Desktop Sync hört dafür über Pipewire den Monitor des Standard-Ausgabegeräts mit (kein Mikrofon) und erkennt Transienten: Liegt die Energie eines Audiopuffers mehr als 6 dB über dem Mittel der letzten ~0,5 s, löst das aus, ab 12 dB voll. Gleichmäßig laute Musik löst daher nicht ständig aus, Leises unter −50 dBFS nie. Linker und rechter Kanal werden nach der horizontalen Position des Lampenbereichs verteilt. Bild und Sound wirken zusammen, der stärkere Auslöser gewinnt; die Stärke kommt von `highlightBoost`
* **highlightBoost**: (Float) Stärke der Lichtblitze bei plötzlich heller werdenden Bildbereichen (Explosionen, Blitze, Schnitte in helle Szenen), zwischen 0 (aus) und 1 (in der Weboberfläche 0–100 %). Steigt die ungeglättete Helligkeit im Bereich einer Lampe von einem Bild zum nächsten um mehr als 5 %, geht die Lampe sofort einen Teil des Weges zur vollen Helligkeit (bei einem Sprung ab 30 % genau um den eingestellten Anteil) und klingt mit einer Zeitkonstante von 250 ms ab. Verdunkeln und reine Farbwechsel lösen nichts aus. Wirkt pro Lampe und nach der Glättung. Voreinstellungen in der Weboberfläche: Subtle (20 %), Moderate (40 %), High (60 %), Intense (85 %)
* **transitionSmoothing**: (Float) Stärke der weichen Farbübergänge, zwischen 0 (aus) und 0,97. In der Weboberfläche gibt es neben dem Slider die Voreinstellungen „Gaming“ (10 %, Lampen folgen fast sofort) und „Movie“ (60 %, glättet harte Schnitte und Blitze)

Ein Beispiel:

```json
{
  "boundBackendIP": "127.0.0.1",
  "bridgeAddress": "192.168.0.10",
  "credentials": {
    "clientkey": "01234567890ABCDEF0123456789ABCDE",
    "username": "AbCdEfGhIjKlMnOpQrStUvWxYz-0123456789012"
  },
  "profileName": "profile",
  "refreshRate": 60,
  "restoreToken": "aBcDeFgH_iJkLmNoPqRsT1",
  "restServerPort": 8215,
  "subsampleWidth": 32,
  "transitionSmoothing": 0.0
}
```

#### profile.json

Diese Datei enthält die Liste der Kanäle einer Entertainment-Konfiguration.

* **channels**: (Array) Liste der Kanäle der Entertainment-Konfiguration
  * **active**: (Boolean) Ob der Kanal aktiv ist
  * **channelId**: (Unsigned) Index des Kanals in der Entertainment-Konfiguration
  * **devices**: (Array) Liste der Geräte des Kanals
    * **id**: (String) UUID des Geräts
    * **name**: (String) Vom Benutzer vergebener Name des Geräts
  * **gammaFactor**: (Float) Helligkeitskorrektur der Lampe
  * **uvs**: (Object) Koordinatenpaar des verarbeiteten Bildausschnitts
    * **uvA**: (Object) Koordinate der oberen linken Ecke
      * **x**: (Float) x-Koordinate
      * **y**: (Float) y-Koordinate
    * **uvB**: (Object) Koordinate der unteren rechten Ecke
      * **x**: (Float) x-Koordinate
      * **y**: (Float) y-Koordinate
* **entertainmentConfigurationId**: (String) UUID der zugehörigen Entertainment-Konfiguration

#### Beenden

Hue Desktop Sync lässt sich über die Weboberfläche oder durch ein Beenden-Signal (z. B. Strg+C oder SIGTERM) beenden.

## Fehlerbehebung

### In der Weboberfläche sehe ich keine Entertainment-Bereiche bzw. kann keinen auswählen

Das passiert, wenn die Zugangsdaten aus irgendeinem Grund ungültig geworden sind. Meist hilft Folgendes:

* Hue Desktop Sync beenden, falls es läuft
* Die Datei ```~/.config/hue-desktop-sync/config.json``` löschen (oder umbenennen)
* Hue Desktop Sync neu starten
* Die Ersteinrichtung erneut durchlaufen und beim Schritt zu den Zugangsdaten auf die Schaltfläche „I don't have credentials yet“ klicken (die Oberfläche ist englisch)
* Den Anweisungen folgen (dazu gehört das Drücken der Taste auf der Bridge) und die Einrichtung abschließen

Danach sollte Hue Desktop Sync die Entertainment-Bereiche wieder anzeigen und Farben streamen.

### Das Portal zur Bildschirmauswahl erscheint nicht mehr, ich möchte die Auswahl aber ändern

1. Hue Desktop Sync beenden, falls es läuft
2. Die Datei `~/.config/hue-desktop-sync/config.json` öffnen und die Zeile mit `restoreToken` entfernen
3. Hue Desktop Sync starten
4. Das Portal zur Bildschirmauswahl erscheint wieder

### Absturz nach der Bildschirmauswahl

Es gibt einen bekannten Fehler, der je nach Build-Parametern auftritt. Seit 1.0.9 ist er „halb gelöst“, kann unter bestimmten Umständen aber weiterhin auftreten.

Dann kann es helfen, Hue Desktop Sync mit anderen Optimierungsoptionen neu zu bauen.

Lösche dazu das Verzeichnis ```build``` (falls vorhanden).

Konfiguriere das Projekt mit einem expliziten Build-Typ neu:

```bash
cmake -S . -B build -DCMAKE_BUILD_TYPE=<CMakeBuildType>
cmake --build build -j
```

```<CMakeBuildType>``` muss durch einen dieser Werte ersetzt werden:

* Release
* MinSizeRel
* Debug
* RelWithDebInfo

Starte Hue Desktop Sync dann erneut und prüfe, ob es nun über die Bildschirmauswahl hinauskommt.

### Hue Desktop Sync stürzt weiterhin sofort ab

Prüfe in der Datei ```~/.config/hue-desktop-sync/config.json``` folgende Werte:

* ```subsampleWidth```: Muss größer als 0 und ein Teiler deiner Bildschirmauflösung sein.
* ```refreshRate```: Muss zwischen 1 und 60 bzw. der Bildwiederholrate deines Bildschirms liegen (der kleinere Wert gilt).

## Website

Weitere Informationen und Neuigkeiten zum ursprünglichen Projekt gibt es auf [Huenicorn.org](http://huenicorn.org), der offiziellen Website.

## Versionsgeschichte (Huenicorn)

* 1.2.0 (aktuell)
  * Unterstützung für Gamescope, weiche Farbübergänge, Unterstützung für MbedTLS 4.0
* 1.1.0
  * Bessere Helligkeit/Sättigung der Farben, Unterstützung der Bridge Pro, automatische Bildschirmauswahl unter Wayland
* 1.0.12
  * Umfassendes Refactoring für weniger Abhängigkeiten und einen klareren Ablauf
* 1.0.11
  * Webroot in die ausführbare Datei eingebettet, mehrere Bridges bei der Einrichtung
* 1.0.10
  * Standardport geändert, Farbberechnung korrigiert, kleinere Fehler behoben
* 1.0.9
  * Kleiner Schritt zur Behebung des Absturzes unter Wayland
* 1.0.8
  * Umfassende Umstrukturierung des Codes und Einbindung des Logos
* 1.0.7
  * Fehlerbehebungen für Wayland-Sitzungen
* 1.0.6
  * Bibliotheken ersetzt und bessere Compiler-Unterstützung
* 1.0.5
  * Auswahl der Subsampling-Interpolation in den erweiterten Einstellungen der Weboberfläche
* 1.0.4
  * Unterstützung für Wayland-Sitzungen
* 1.0.3
  * Umstieg auf Mbed-TLS 3.4
* 1.0.2
  * Leistungsverbesserung
* 1.0.1
  * Höhere Zuverlässigkeit und bessere Dokumentation
* 1.0.0
  * Erste stabile Version
* 0.0.0 (Legacy)
  * Erste (langsame) Implementierung über die HTTP-API von Hue

## Autoren

OpenJowel (ursprünglicher Autor)

Philip's Hue Desktop Sync ist ein privater Fork von Huenicorn und wird auf einer lokalen Forgejo-Instanz gepflegt.

## Lizenz

Philip's Hue Desktop Sync steht wie Huenicorn unter der GNU GPLv3. Details findest du in der Datei [LICENSE.txt](LICENSE.txt).

## Danksagungen

* [XDG portal screencast](https://codeberg.org/metamuffin/xdg-dp-start/src/branch/master/xdg-dp-start.c)
* [Pipewire-Dokumentation](https://docs.pipewire.org/page_tutorial5.html)
* [RGB-D65-Umrechnung](https://gist.github.com/popcorn245/30afa0f98eea1c2fd34d)
* [Huestacean](https://github.com/BradyBrenot/huestacean)
