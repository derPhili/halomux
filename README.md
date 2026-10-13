# Huenicorn

Ein freier Bildschirm-Synchronisierer für Philips Hue™ unter GNU/Linux

<p align="center">
  <img src="assets/logo.svg" alt="Logo" width="256" height="256">
</p>

## Beschreibung

Huenicorn ist ein freier Ambilight-Treiber für GNU/Linux.
Mit anderen Worten: Huenicorn überträgt die farbige Atmosphäre deines Computerbildschirms in Echtzeit auf deine Hue-Lampen.
Über eine einfache Weboberfläche weist du einzelnen Lampen bestimmte Bildschirmbereiche zu und speicherst die Einstellungen für spätere Sitzungen.

### Screenshot

| ![Screenshot](screenshots/HuenicornFullWebUI.png) |
|:--:|
| *Bildunterschrift: Oberfläche des Huenicorn Light Managers* |

## Projektstatus

Huenicorn 1.2.0 ist verfügbar.

### Neu in dieser Version

- Neue Funktionen
  - Unterstützung für Gamescope
  - Weiche Farbübergänge (einstellbar in den erweiterten Einstellungen der Oberfläche)

- Korrekturen und Verbesserungen
  - Unterstützung für MbedTLS 4.x
  - Aktualisierung des restoreToken korrigiert
  - Serialisierung der Konfiguration korrigiert
  - Busy-Loop im xdg-portal behoben

## Erste Schritte

Bevor du Huenicorn verwendest, musst du über die offizielle App von Philips einen Entertainment-Bereich auf deiner Hue Bridge anlegen.

### Voraussetzungen

* GNU/Linux-System mit einer grafischen Sitzung unter X11 oder Wayland
* Philips Hue Bridge mit eingebundenen Lampen

### Abhängigkeiten

* [X.Org](https://xorg.freedesktop.org) oder [Wayland](https://wayland.freedesktop.org)
* [OpenCV](https://github.com/opencv/opencv)
* [Mbed-TLS](https://github.com/Mbed-TLS/mbedtls)
* [Curl](https://curl.se)

#### Installation der Abhängigkeiten

<details>

<summary>ArchLinux</summary>

```bash
# Benötigte Abhängigkeiten
sudo pacman -S git cmake make gcc curl opencv mbedtls

# Für X11-Unterstützung
sudo pacman -S xorg-server

# Für Wayland-Unterstützung
sudo pacman -S wayland glib2 pipewire
```

</details>

<details>

<summary>Fedora</summary>

```bash
# Benötigte Abhängigkeiten
sudo dnf install -y git cmake gcc gcc-c++ opencv-devel curl-devel mbedtls-devel

# Für X11-Unterstützung
sudo dnf install -y libXrandr-devel

# Für Wayland-Unterstützung
sudo dnf install -y pipewire-devel glib2-devel
```

</details>

<details>

<summary>OpenSUSE Tumbleweed</summary>

Diese Abhängigkeiten mussten unter OpenSUSE Tumbleweed 20231011 installiert werden, um Huenicorn zu bauen und auszuführen:

```bash
sudo zypper install opencv-devel libopencv408 python311-jsonschema

# Für Wayland-Unterstützung
sudo zypper install pipewire-devel glib2-devel

```

Zusätzlich musst du Mbed-TLS aus den Quellen bauen (siehe Links oben).
Folge den Bauanleitungen in den jeweiligen README-Dateien und kopiere die Ergebnisse an die passende Stelle, da das nicht immer automatisch geschieht (üblicherweise /usr/local/lib64/ für Bibliotheken (LD_LIBRARY_PATH prüfen) bzw. /usr/local/include/ für Header).

</details>

<details>

<summary>Ubuntu 26.04 LTS und neuer</summary>

```bash
# Benötigte Abhängigkeiten
sudo apt-get install build-essential cmake libopencv-dev libcurl4-openssl-dev libmbedtls-dev

# Für X11-Unterstützung
sudo apt-get install libx11-dev libxext-dev libxrandr-dev

# Für Wayland-Unterstützung
sudo apt-get install libglib2.0-dev libpipewire-0.3-dev wayland-utils
```

</details>

<details>
<summary>Ubuntu 22.04 – 24.04</summary>

```bash
# Manche Ubuntu-Installationen benötigen das Universe-Repository
sudo add-apt-repository universe
sudo apt-get update

# Benötigte Abhängigkeiten
sudo apt-get install build-essential cmake libopencv-dev libcurl4-openssl-dev libmbedtls-dev

# Für X11-Unterstützung
sudo apt-get install libx11-dev libxext-dev libxrandr-dev

# Für Wayland-Unterstützung
sudo apt-get install libglib2.0-dev libpipewire-0.3-dev wayland-utils

# Huenicorn benötigt GCC/G++ 12 oder neuer.
# Ubuntu 22.04 liefert standardmäßig GCC 11 aus, installiere daher bei Bedarf GCC 12 oder neuer.
sudo apt install gcc-12 g++-12
# Im Kapitel „Huenicorn bauen“ ist dann statt ```cmake ..``` dieser Befehl nötig:
cmake -DCMAKE_C_COMPILER=gcc-12 -DCMAKE_CXX_COMPILER=g++-12 ..
```

Ubuntu-Versionen vor 24.04 werden offiziell nicht unterstützt.
</details>

<details>
<summary>NixOS</summary>

Das Repository enthält einen Nix Flake. Im Wurzelverzeichnis eines Checkouts:

```bash
nix run .
```
</details>

### Huenicorn bauen

```bash
git clone http://192.168.0.250:3000/philip/huenicorn.git
cd huenicorn
mkdir build && cd build
# Standardkonfiguration
cmake ..
# oder: ohne automatisches Nachladen fehlender Abhängigkeiten aus dem Internet (glm, nlohmann_json, httplib)
cmake -DHUENICORN_FETCH_DEPS=OFF ..
make
```

Das Ergebnis ist eine eigenständige ausführbare Datei namens „huenicorn“.

### Programm ausführen

* Starte die Datei „huenicorn“ auf die Art, die dir am liebsten ist (im Terminal, wenn du Textausgaben sehen möchtest).
* Öffne deinen Browser unter [127.0.0.1:8215](http://127.0.0.1:8215).
  Bei der Ersteinrichtung und solange noch kein Lampenprofil gespeichert ist, öffnet sich der Browser automatisch.
  (Der Port lässt sich in der Konfiguration ändern. Standard ist 8215.)

#### Als systemd-Dienst

Huenicorn kann als systemd-User-Dienst mit der grafischen Sitzung starten. Die Unit liegt unter `dist/systemd/huenicorn.service` und startet `~/.local/bin/huenicorn`. Die Ersteinrichtung und die erste Bildschirmauswahl unter Wayland sollten vorher einmal von Hand erledigt sein.

Installieren und aktivieren:

```bash
install -Dm755 build/huenicorn ~/.local/bin/huenicorn
install -Dm644 dist/systemd/huenicorn.service ~/.config/systemd/user/huenicorn.service
systemctl --user daemon-reload
systemctl --user enable --now huenicorn.service
```

Nach einem neuen Build die installierte Datei ersetzen und den Dienst neu starten:

```bash
install -Dm755 build/huenicorn ~/.local/bin/huenicorn
systemctl --user restart huenicorn.service
```

Ausgaben ansehen:

```bash
journalctl --user -u huenicorn.service -f
```

#### Ersteinrichtung

Beim ersten Start öffnet Huenicorn die Einrichtungsseite in deinem Browser. Ein Schritt-für-Schritt-Assistent hilft dir, deine Hue Bridge im Netzwerk zu finden und einen Benutzer darauf anzulegen.

Nach erfolgreichem Abschluss der Einrichtung setzt Huenicorn seinen normalen Ablauf fort.

Lade die Webseite anschließend neu, um zur Oberfläche des Light Managers zu gelangen.

#### Normaler Ablauf

<details>
  <summary>Zusätzlicher Schritt unter Wayland</summary>
  <br/>

  Beim Start fordert dich der Grabber über ein Bildschirmfreigabe-Portal auf, eine Quelle auszuwählen. Wähle den gewünschten Bildschirm und bestätige mit „Teilen“ (bzw. „Share“, je nach Sprache des Desktops).
</details>
<br/>

Huenicorn wählt automatisch den passenden Screen-Grabber für die grafische Sitzung.

Beim Start sucht Huenicorn nach einem gespeicherten Profil und beginnt mit dem Streamen der Farben.

Um Lampen bestimmten Bildschirmbereichen zuzuweisen, öffne die Weboberfläche und ziehe die verfügbaren Lampen per Drag-and-Drop aus der linken in die rechte Liste.

Ist eine synchronisierte Lampe in der Liste ausgewählt, kannst du ihren Bildschirmbereich im Abschnitt zur Bildschirmaufteilung anpassen.

Bist du mit der Aufteilung zufrieden, speichere sie als Profil. Es wird bei späteren Starts automatisch geladen.

Solange kein Profil gespeichert ist, öffnet sich die Lampenverwaltung beim Start automatisch.

### Konfigurationsdateien

Die Konfigurationsdateien liegen in deinem Home-Verzeichnis unter ~/.config/huenicorn/.
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
* **interpolation**: (Unsigned) Kennung der gewünschten Interpolationsfunktion für das Subsampling
  * Nearest = 0: Am günstigsten. Geringe Qualität bei feinen Details. Kann die Lampen flackern lassen
  * Cubic = 1: Langsam und hart (für Tests beibehalten)
  * Area = 2: (Empfohlen) Weich und gibt den Gesamtinhalt gut wieder
* **profileName**: (String) Name des aktuellen, benutzerdefinierten Lampenprofils
* **refreshRate**: (Unsigned) Frequenz von Bildschirmaufnahme und Lampenaktualisierung. Höchstens 60 Hz: Die Bridge gibt Farben mit etwa 25 Hz an die Lampen weiter, Philips empfiehlt 50–60 Hz Streamingrate. Höhere Werte werden beim Start und in der Weboberfläche auf 60 Hz begrenzt
* **restoreToken**: (String) Token, um die Einstellungen der letzten Sitzung zu merken (nur Wayland)
* **restServerPort**: (Unsigned) Port, auf dem die Weboberfläche antwortet
* **subsampleWidth**: (Unsigned) Breite des verarbeiteten, verkleinerten Bildes
* **transitionSmoothing**: (Float) Stärke der weichen Farbübergänge, zwischen 0 (aus) und 0,97

Ein Beispiel:

```json
{
  "boundBackendIP": "127.0.0.1",
  "bridgeAddress": "192.168.0.10",
  "credentials": {
    "clientkey": "01234567890ABCDEF0123456789ABCDE",
    "username": "AbCdEfGhIjKlMnOpQrStUvWxYz-0123456789012"
  },
  "interpolation": 2,
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

Huenicorn lässt sich über die Weboberfläche oder durch ein Beenden-Signal (z. B. Strg+C oder SIGTERM) beenden.

## Fehlerbehebung

### In der Weboberfläche sehe ich keine Entertainment-Bereiche bzw. kann keinen auswählen

Das passiert, wenn die Zugangsdaten aus irgendeinem Grund ungültig geworden sind. Meist hilft Folgendes:

* Huenicorn beenden, falls es läuft
* Die Datei ```~/.config/huenicorn/config.json``` löschen (oder umbenennen)
* Huenicorn neu starten
* Die Ersteinrichtung erneut durchlaufen und beim Schritt zu den Zugangsdaten auf die Schaltfläche „I don't have credentials yet“ klicken (die Oberfläche ist englisch)
* Den Anweisungen folgen (dazu gehört das Drücken der Taste auf der Bridge) und die Einrichtung abschließen

Danach sollte Huenicorn die Entertainment-Bereiche wieder anzeigen und Farben streamen.

### Meine Lampen zeigen einen Regenbogen-Verlauf statt der Bildschirmfarben

#### Ursache

Huenicorn konnte keinen Grabber für die Desktop-Sitzung laden. Es lädt dann den „Dummy-Grabber“, damit die Verwaltungsoberfläche trotzdem erreichbar ist und die Kommunikation mit der Bridge funktioniert.

#### Lösung

Prüfe, ob Abhängigkeiten für deinen Sitzungstyp fehlen.

Falls du den aktuellen Sitzungstyp nicht kennst, gib folgenden Befehl ein:

```bash
echo $XDG_SESSION_TYPE
```

Sieh dann im Abschnitt zur Installation der Abhängigkeiten für deine Distribution nach.

Die Ausgabe von CMake zeigt, welche Grabber gebaut werden können:

```cmake
cmake ..
...
[cmake] Able to build X11 Grabber !
[cmake] Able to build Pipewire Grabber !
...
```

Wayland benötigt den ```Pipewire```-Grabber.

### Unter Wayland erscheint das Portal zur Bildschirmauswahl nicht mehr, ich möchte die Auswahl aber ändern

1. Huenicorn beenden, falls es läuft
2. Die Datei `~/.config/huenicorn/config.json` öffnen und die Zeile mit `restoreToken` entfernen
3. Huenicorn starten
4. Das Portal zur Bildschirmauswahl erscheint wieder

### Absturz nach der Bildschirmauswahl unter Wayland

Es gibt einen bekannten Fehler, der je nach Build-Parametern auftritt. Seit 1.0.9 ist er „halb gelöst“, kann unter bestimmten Umständen aber weiterhin auftreten.

Dann kann es helfen, Huenicorn mit anderen Optimierungsoptionen neu zu bauen.

Leere den Inhalt des Verzeichnisses ```build``` (falls vorhanden).

Konfiguriere das Projekt mit folgender Zeile statt ~~```cmake ..```~~ neu:

```bash
cmake -DCMAKE_BUILD_TYPE=<CMakeBuildType> -DCMAKE_C_COMPILER=gcc -DCMAKE_CXX_COMPILER=g++ ..
make
```

```<CMakeBuildType>``` muss durch einen dieser Werte ersetzt werden:

* Release
* MinSizeRel
* Debug
* RelWithDebInfo

Starte Huenicorn dann erneut und prüfe, ob es nun über die Bildschirmauswahl hinauskommt.

### Huenicorn stürzt weiterhin sofort ab

Prüfe in der Datei ```~/.config/huenicorn/config.json``` folgende Werte:

* ```subsampleWidth```: Muss größer als 0 und ein Teiler deiner Bildschirmauflösung sein.
* ```refreshRate```: Muss zwischen 1 und 60 bzw. der Bildwiederholrate deines Bildschirms liegen (der kleinere Wert gilt).

## Website

Weitere Informationen und Neuigkeiten zum ursprünglichen Projekt gibt es auf [Huenicorn.org](http://huenicorn.org), der offiziellen Website.

## Versionsgeschichte

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

Dieses Repository ist ein privater Fork von Huenicorn und wird auf einer lokalen Forgejo-Instanz gepflegt.

## Lizenz

Huenicorn steht unter der GNU GPLv3. Details findest du in der Datei [LICENSE.txt](LICENSE.txt).

## Danksagungen

* [XDG portal screencast](https://codeberg.org/metamuffin/xdg-dp-start/src/branch/master/xdg-dp-start.c)
* [Pipewire-Dokumentation](https://docs.pipewire.org/page_tutorial5.html)
* [RGB-D65-Umrechnung](https://gist.github.com/popcorn245/30afa0f98eea1c2fd34d)
* [Huestacean](https://github.com/BradyBrenot/huestacean)
