# Monitorwahl pro Lampe

Stand: 2026-10-16

## Absicht

Lightspill ist ein Open-Source-Projekt. Der Maintainer nutzt selbst nur seinen Hauptmonitor, andere Nutzer haben aber mehrere Monitore und Lampen, die zu verschiedenen Monitoren gehören. Ein Beispiel: Ein Play hinter dem linken Monitor soll dem linken Bild folgen, ein Play hinter dem rechten dem rechten.

Heute erfasst Lightspill genau einen Monitor. Welcher das ist, sieht man in der App nicht. Ändern lässt er sich nur, indem man `restoreToken` aus der Konfiguration löscht und den Dienst neu startet.

## Ziele

1. Die App zeigt, welche Monitore gerade erfasst werden, mit Namen.
2. Jede Lampe folgt einem Bereich auf einem beliebigen der erfassten Monitore. Mehrere Monitore sind gleichzeitig Quelle.
3. Die Zuordnung lässt sich zur Laufzeit ändern, ohne Neustart des Dienstes.
4. Die Monitore lassen sich zur Laufzeit neu wählen („Monitore…“), ohne Neustart des Dienstes.

## Nicht-Ziele

- Mehrere Entertainment-Bereiche gleichzeitig. Die Bridge streamt nur einen Bereich zur Zeit.
- Bereiche, die über zwei Monitore reichen. Ein Bereich liegt immer auf genau einem Monitor.
- Monitorwahl im Tray-Menü.
- Erfassung einzelner Fenster.
- X11.

## Anti-Ziele (wäre ein Fehlschlag, auch wenn es technisch läuft)

- Nutzer mit einem Monitor merken eine Verschlechterung: anderes Aussehen, mehr CPU-Last, zusätzliche Dialoge.
- Bestehende Profile verlieren ihre Bereiche.
- Der Systemdialog „Bildschirmübertragung angefordert“ erscheint bei normalen Starts oder bei einem Wechsel zwischen bereits freigegebenen Monitoren.
- Die CPU-Last steigt durch Monitore, denen keine Lampe folgt.

## Rahmenbedingungen

- **Wayland-Portal.** Eine App darf Monitore nicht selbst auswählen. Die Auswahl trifft der Nutzer im Systemdialog des xdg-desktop-portal. Mit `multiple = true` kann er dort mehrere Monitore anhaken. Das Portal liefert dann einen Stream pro Monitor, mit den Eigenschaften `position` und `size` in logischen Desktop-Koordinaten, und gibt einen gemeinsamen `restore_token` zurück.
- **Alle Streams einer Sitzung** teilen sich einen Pipewire-Remote, also einen Dateideskriptor aus `OpenPipeWireRemote`.
- **Testsystem:** KDE Plasma 6 unter Wayland mit Portal-Version 6 und zwei Monitoren:
  - DP-3 mit 2560×1440 an Position 1920,0
  - DP-1 mit 1920×1080 an Position 0,249

  `WAYLAND_DISPLAY` ist in der Umgebung des systemd-User-Dienstes gesetzt.
- **Kompositor-Verhalten:** Er schickt nur bei Bildänderungen neue Frames. Ein ruhender Monitor liefert also lange keine Frames.
- **Weiter gültig:**
  - Der GPU-Pfad (DMA-BUF, EGL) mit CPU-Fallback.
  - Die Begrenzung der Bildrate (`setFrameRateLimit`).
  - Das Pausieren bei ausgeschaltetem Sync.
- **Die Web-Oberfläche muss auch im Browser funktionieren,** also ohne die Desktop-App, wenn mit `-DBUILD_GUI=OFF` gebaut wird.

## Entschieden mit dem Maintainer

- **Monitorwahl pro Lampe:** Jede aktive Lampe hat ihren eigenen Monitor.
- **Darstellung „ganzer Desktop“** (Entwurf B):
  - Der Bereichs-Editor zeigt alle erfassten Monitore so angeordnet und in dem Größenverhältnis wie im System.
  - Jede Lampe ist ein Rechteck auf einem Monitor.
  - Wird das Rechteck auf einen anderen Monitor geschoben, folgt die Lampe diesem Monitor.
- **Kopfzeile:** Sie zeigt die erfassten Monitore mit Namen, zum Beispiel „Quelle: LG 27GL850 · Dell U2419H“. Daneben öffnet der Knopf „Monitore…“ den Systemdialog neu.
- **Freigabe für alle zusammen:** Im Systemdialog hakt man alle gewünschten Monitore auf einmal an. Ein Wechsel zwischen freigegebenen Monitoren fragt nicht nach.
- **Bereich über der Kante:** Wird ein Bereich über die Monitorkante gezogen, landet er auf dem Monitor, auf dem seine Mitte liegt. Dort wird er in den Monitor hineingeschoben, also geclampt.
- **Nicht genutzte Monitore:** Ein Monitor, dem keine aktive Lampe folgt, wird nicht verarbeitet. Sein Stream ist inaktiv.
- **Abgesteckter Monitor:** Lampen, deren Monitor nicht mehr erfasst wird, gehen aus. Die Oberfläche zeigt bei der Lampe einen Hinweis, warum.
- **Bestehende Profile:** Bereiche ohne Monitorangabe gehören zu dem Monitor, der bisher Quelle war. Das ist der erste Stream der wiederhergestellten Sitzung. Bei einem Monitor sieht alles aus wie heute.

## Meine Entscheidungen (vom Maintainer prüfen)

1. **Monitornamen kommen aus dem Dienst, nicht aus der Desktop-App.**
   - Der Dienst liest die Monitore über das Wayland-Protokoll: `wl_output` ab Version 4 liefert `name` und `description`, `zxdg_output_manager_v1` die logische Position und Größe.
   - Ein Stream wird dem Monitor mit gleicher logischer Position und Größe zugeordnet.
   - Vorteil: Das funktioniert auch im Browser ohne Desktop-App.
   - Neue Abhängigkeit: `wayland-client` (plus `wayland-scanner` für das xdg-output-Protokoll, falls nicht in `wayland-protocols` vorhanden).
   - Ohne Wayland-Verbindung oder ohne Treffer heißt ein Monitor „Monitor N · B×H“.
2. **Ein Monitor wird über seine logische Geometrie identifiziert.**
   - Im Profil steht pro Kanal `monitor: {"x":…, "y":…, "width":…, "height":…}`.
   - Beim Laden wird so gesucht, bis etwas passt:
     1. exakte Geometrie
     2. gleicher Wayland-Name (`name`, z. B. `DP-3`, wird mitgespeichert)
     3. gleiche Größe
   - Passt nichts, gilt die Lampe als „Monitor fehlt“.
3. **Erfassungsauflösung pro Monitor.**
   - `subsampleWidth` gilt weiter für den größten erfassten Monitor.
   - Die anderen Monitore verwenden dasselbe Verhältnis von Breite zu Monitorbreite, gerundet.
   - Die Einstellung in der Oberfläche bleibt eine einzige.
4. **Bildrate:**
   - `displayRefreshRate()` ist das Maximum der erfassten Monitore.
   - Das Limit aus `refreshRate` gilt für jeden Stream.
5. **„Monitore…“ ohne Risiko:**
   - Der Dienst startet eine neue Portal-Sitzung ohne `restore_token`, während die alte weiterläuft.
   - Bestätigt der Nutzer, ersetzt die neue Sitzung die alte und ihr Token wird gespeichert.
   - Bricht er ab, bleibt alles wie vorher.
6. **Auch die Ersteinrichtung** fragt mit `multiple = true`. Der Assistent sagt im Schritt „Bildschirmauswahl“, dass man alle Monitore anhaken kann, deren Bild Lampen folgen sollen.
7. **Ruhende Monitore:** Eine Lampe behält die letzte Farbe ihres Monitors, bis dieser einen neuen Frame liefert. `grabChannelColors()` liefert `true`, sobald mindestens ein genutzter Monitor schon einen Frame hatte. Lampen eines Monitors ohne bisherigen Frame werden wie heute übersprungen, bis einer kommt.
8. **Kopfzeile bei unterschiedlichen Verarbeitungspfaden:** Läuft ein Stream über die CPU und ein anderer über die GPU, zeigt die Kopfzeile „CPU“ und nennt in der Tooltip-Erklärung den Monitor.
9. **Neue Lampe:** Eine Lampe, die neu aktiv wird, kommt auf den Monitor, dem die meisten anderen Lampen folgen. Gibt es noch keine, auf den ersten.

## Architektur-Skizze

Diese Skizze ist eine Richtung, keine Vorgabe. Der Builder darf abweichen, wenn er es begründet.

- **Portal** (`XdgDesktopPortal`):
  - `multiple = TRUE`.
  - `onStartResponseReceivedCallback` liest alle Streams: Node-ID, `position`, `size`.
  - `Capture` hält eine Liste statt `pwNode`.
- **Grabber** (`PipewireGrabber`):
  - Ein Pipewire-Core auf dem gemeinsamen fd, darauf ein `pw_stream` pro Monitor.
  - Jeder Stream hat eigenen Zustand: Format, DMA-BUF-Fähigkeit, Pfad.
  - Der EGL-Kontext und der `EglDownscaler` können geteilt werden. Sie laufen alle im selben Pipewire-Thread.
  - Streams ohne Regionen werden mit `pw_stream_set_active(false)` pausiert.
- **`IGrabber`:**
  - `ChannelRegions` bekommt pro Kanal einen Monitor-Index.
  - Neu ist `monitors()` mit Index, Name, Beschreibung, logischer Geometrie, Pixelauflösung, Bildrate und Verarbeitungspfad.
  - `displayResolution()` gibt es weiter, als Auflösung des größten Monitors, für bestehende Aufrufer wie `subsampleResolutionCandidates()`.
  - Neu ist `reselectMonitors()` für Ziel 4.
- **Runtime:** Der Grabber bleibt ein Objekt. „Monitore…“ läuft über `reselectMonitors()` im Grabber, nicht über ein Neuerzeugen in der Runtime.
- **Channel / Profil:** Neues Feld `monitor` (siehe Entscheidung 2). Ein fehlendes Feld bedeutet: erster Monitor.
- **HTTP-API:**
  - `GET /api/displayInfo` bekommt `monitors: [...]`.
  - `PUT /api/setChannelUV/:channelId` nimmt optional einen Monitor an.
  - Neu ist `POST /api/reselectMonitors`. Seine Antwort kommt sofort; den Ausgang zeigt das Polling.
- **Web-Oberfläche** (`ScreenWidget.js`, `index.html`, `WebUI.js`, `I18n.js`, `style.css`):
  - Der Editor zeigt alle Monitore nebeneinander, mit Rechtecken pro Monitor.
  - Ein Rechteck ist frei ziehbar. Beim Loslassen wird es dem Monitor unter seiner Mitte zugeordnet und dort geclampt.
  - Die Griffe sind auf den Monitor begrenzt.
  - Raster und Live-Farben gibt es pro Monitor.
  - Neue Texte kommen auf Deutsch und Englisch, mit Tooltips, wie überall.
- **Einrichtungsassistent:** Der Text im letzten Schritt wird angepasst.
- **README** (de/en): Neue Abschnitte zur Monitorwahl und zu den Konfigurationsfeldern.

## Tests

- **Unit-Tests (C++):**
  - Zuordnung Stream zu Wayland-Ausgabe.
  - Wiederfinden des Monitors aus dem Profil, alle drei Stufen aus Entscheidung 2.
  - Profil-Migration ohne `monitor`-Feld.
  - Subsample-Breite pro Monitor.
- **Live auf dem Testsystem:**
  1. Ein Monitor mit bestehendem Profil: kein Dialog, alles wie vorher.
  2. „Monitore…“, beide Monitore anhaken: Die Kopfzeile zeigt beide Namen.
  3. Eine Lampe auf DP-1 ziehen: Sie folgt DP-1. Die CPU-Last steigt mit beiden genutzten Monitoren um etwa einen Monitor-Anteil (rund 3 %).
  4. Alle Lampen auf einen Monitor: Der andere Stream ist pausiert, die CPU-Last ist wie bei einem Monitor.
  5. „Monitore…“ und dann Abbrechen: Alles läuft weiter wie vorher.
  6. Dienst neu starten: kein Dialog, beide Monitore wieder da.
  7. Mit `LIGHTSPILL_DISABLE_GPU=1`: Der CPU-Pfad läuft mit zwei Monitoren.
- **Nicht testbar hier:** einen Monitor im Betrieb abstecken. Das wird über einen Unit-Test der Zuordnung plus Code-Review abgedeckt.

## Offen für den Builder

- Ob der EGL-Kontext geteilt oder pro Stream angelegt wird.
- Wie die Monitore im Editor skaliert werden (Abstände, Mindestgröße kleiner Monitore).
- Die Wire-Formate der API im Detail.
