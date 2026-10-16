# Monitorwahl pro Lampe – Umsetzungsplan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Lightspill erfasst mehrere Monitore gleichzeitig. Jede Lampe folgt einem Bereich auf einem davon. Die App zeigt die Monitore mit Namen und lässt sie zur Laufzeit neu wählen.

**Architecture:**
- Das Portal liefert einen Stream pro angehaktem Monitor. Der `PipewireGrabber` verarbeitet sie alle auf einem gemeinsamen Pipewire-Core, mit einem `pw_stream` pro Monitor.
- Kanäle speichern eine Monitor-Referenz (logische Geometrie plus Wayland-Name). Die Runtime löst sie bei jedem Monitorwechsel auf einen Index auf.
- Eine Neuwahl baut in einem Hintergrund-Thread einen neuen Grabber und tauscht ihn erst nach Bestätigung aus.
- **Abweichung vom Spec:** Die Neuwahl ist kein `reselectMonitors()` im Grabber, sondern ein neuer Grabber, den die Runtime austauscht. Der Grabber-Konstruktor *ist* der Portal-Ablauf, und der Portal-Thread endet, sobald der fd da ist. Damit bleibt der alte Grabber ohne Sonderlogik bis zur Bestätigung aktiv.

**Tech Stack:** C++20, Pipewire, xdg-desktop-portal (GDBus), EGL/GLES, wayland-client (`wl_output` v4, `zxdg_output_manager_v1`), cpp-httplib, nlohmann_json, Vanilla-JS-Web-UI (SVG).

**Spec:** `docs/superpowers/specs/2026-10-16-multi-monitor-design.md`

## Global Constraints

- **Profile:** Bestehende Profile ohne `monitor`-Feld laden ohne Verlust. Ihre Kanäle gehören zum ersten Stream der Sitzung.
- **Kein unnötiger Systemdialog:** Er erscheint nur bei fehlendem `restoreToken` oder nach „Monitore…“.
- **Ein Monitor:** Die Oberfläche sieht aus wie heute.
- **Ungenutzte Monitore:** Ein Monitor ohne aktive Lampe ist pausiert (`pw_stream_set_active(false)`).
- **GPU-Pfad:**
  - `LIGHTSPILL_DISABLE_GPU=1` erzwingt weiter den CPU-Pfad.
  - Der GPU-Pfad bleibt der bevorzugte.
- **Texte:**
  - Neue UI-Texte stehen auf Englisch im Code und bekommen einen deutschen Eintrag in `webroot/I18n.js`.
  - Jedes neue Bedienelement hat einen erklärenden `title`.
- **Commits:**
  - Jeder Task ist ein Commit auf `develop`.
  - Identität ist die repo-lokale (noreply).
  - Trailer: `Co-Authored-By: Claude Opus 5.5 <noreply@anthropic.com>`.
- **Tests:**
  - Gebaut in einem eigenen Verzeichnis: `cmake -S . -B /tmp/claude-1000/-home-philip-git-huenicorn/176c1386-40e0-410a-947a-50f4efb7a981/scratchpad/build-tests -DBUILD_TESTS=ON`.
  - Ausgeführt mit `ctest --test-dir …/build-tests --output-on-failure`.
  - Alle bestehenden Tests bleiben grün.
- **Live-Tests:**
  - Der Dienst wird gestoppt, bevor ein Test-Binary läuft (Port 8215, Bridge-Stream).
  - Ein Test-Binary aus der Shell löst den KDE-Portaldialog aus. Er wird **nie** stellvertretend bestätigt; der Nutzer bestätigt ihn selbst.
  - Danach wird `lightspill.service` wieder gestartet.
- **Testsystem:**
  - DP-3 mit 2560×1440 an Position 1920,0
  - DP-1 mit 1920×1080 an Position 0,249
  - Skalierung 1

## Review Focus

1. **Monitore mit negativen Koordinaten** (links oder über dem Hauptmonitor): Desktop-Grenzen, Editor und Panning müssen mit `x < 0` und `y < 0` rechnen. Getestet in Task 1 (`desktopBounds`, `desktopCenterX`).
2. **HiDPI:** Logische Größe und Pixelauflösung unterscheiden sich, zum Beispiel bei Skalierung 1,5.
   - Zuordnung und Editor nutzen die logische Geometrie.
   - Die Subsample-Breite nutzt die Pixelbreite.

   Getestet in Task 1 (`subsampleWidthFor`, `nameMonitors`).
3. **Portal ohne `position`/`size`** (ältere oder wlroots-Portale):
   - Die Monitore werden nach Stream-Reihenfolge nebeneinandergelegt, mit der Pixelgröße als Geometrie.
   - Namen fallen auf „Monitor N · B×H“ zurück.

   Getestet in Task 1 (`fallbackGeometry`, `nameMonitors`).
4. **Panning und Szenen-Verteilung:** Beides läuft über die Desktop-x-Position, nicht über die x-Position auf dem Monitor. Eine Lampe ganz rechts auf dem linken Monitor liegt links von einer Lampe ganz links auf dem rechten Monitor. Getestet in Task 1 (`desktopCenterX`), verwendet in Task 5.
5. **Weniger Monitore nach einer Neuwahl, oder ein Monitor ist weg:**
   - Betroffene Kanäle werden „fehlend“ und dunkel.
   - Es gibt keinen Indexfehler.
   - Regionen mit veraltetem Index erreichen den neuen Grabber nicht.

   Getestet in Task 1 (`findMonitor` → `nullopt`) und Task 5 (`TestChannelMonitors`).

---

### Task 1: Monitor-Modell und reine Hilfsfunktionen

**Files:**
- Create: `include/Huenicorn/Grabber/Monitors.hpp`, `src/Grabber/Monitors.cpp`
- Create: `tests/src/TestMonitors.cpp`
- Modify: `tests/AddTests.cmake` (neues Target `TestMonitors` mit `src/Grabber/Monitors.cpp`, `src/Core/Logger.cpp`, ASan/UBSan wie `TestBridgeDiscovery`, `add_test(NAME Monitors …)`)
- Modify: `CMakeLists.txt` (`src/Grabber/Monitors.cpp` zu den App-Quellen)

**Interfaces:**
- Produces (namespace `Huenicorn::Grabber`):
  ```cpp
  struct MonitorGeometry { int x{0}; int y{0}; int width{0}; int height{0}; bool operator==(const MonitorGeometry&) const = default; };
  struct MonitorRef { MonitorGeometry geometry; std::string name; bool operator==(const MonitorRef&) const = default; };   // persisted per channel
  struct WaylandOutput { std::string name; std::string description; MonitorGeometry geometry; };   // from the compositor
  struct MonitorInfo {
    MonitorGeometry geometry;          // logical desktop coordinates
    int pixelWidth{0}; int pixelHeight{0};
    std::string name;                  // "DP-3", empty if unknown
    std::string description;           // "LG Electronics 27GL850", or "Monitor 2 · 2560×1440"
    bool gpu{false}; std::string processingDetails;
    bool active{false};                // stream running (has regions, not paused)
  };
  using Monitors = std::vector<MonitorInfo>;

  std::optional<size_t> findMonitor(const MonitorRef& ref, const Monitors& monitors);
  MonitorRef monitorRef(const MonitorInfo& monitor);
  void nameMonitors(Monitors& monitors, const std::vector<WaylandOutput>& outputs);
  std::vector<MonitorGeometry> fallbackGeometry(const std::vector<std::pair<int, int>>& pixelSizes);
  MonitorGeometry desktopBounds(const Monitors& monitors);
  float desktopCenterX(const Imaging::UVs& uvs, const MonitorGeometry& monitor, const MonitorGeometry& bounds);   // 0 = left edge of the desktop, 1 = right
  int subsampleWidthFor(unsigned configuredWidth, int referencePixelWidth, int monitorPixelWidth);
  size_t defaultMonitorForNewLamp(const std::vector<size_t>& monitorsOfActiveLamps, size_t monitorCount);
  ```
- **Regeln:**
  - `findMonitor` sucht in dieser Reihenfolge und nimmt jeweils den ersten Treffer:
    1. exakte Geometrie
    2. gleicher nicht leerer `name`
    3. gleiche Breite und Höhe
  - `nameMonitors` ordnet über gleiche logische Geometrie zu. Die `description` kommt vom Output, gekürzt um ein angehängtes ` (NAME)`, wie KWin es anhängt.
  - Ohne Treffer gilt: `description = "Monitor " + (i+1) + " · " + pixelWidth + "×" + pixelHeight` und `name = ""`.
  - `fallbackGeometry` legt die Monitore links beginnend bei x=0 nebeneinander, alle mit y=0.
  - `subsampleWidthFor` ergibt `round(configured * monitor / reference)`, begrenzt auf `[1, monitorPixelWidth]`.
  - `defaultMonitorForNewLamp` gibt den häufigsten Index zurück, bei Gleichstand den kleinsten, und 0, wenn die Liste leer ist.

- [ ] **Step 1: Write the failing tests** in `tests/src/TestMonitors.cpp`. Der Stil folgt `TestBridgeDiscovery.cpp`: eigene `check()`-Funktion, Rückgabewert 1 bei Fehler. Testdaten sind das Testsystem plus Sonderfälle.
  - **findMonitor**, mit `DP1{0,249,1920,1080}` und `DP3{1920,0,2560,1440}`:
    - `findMonitor({{1920,0,2560,1440},"DP-3"}, {DP1, DP3}) == 1`
    - Geometrie verschoben, Name gleich: `findMonitor({{0,0,2560,1440},"DP-3"}, …) == 1`
    - Name unbekannt, Größe gleich: `findMonitor({{5000,0,1920,1080},"HDMI-A-1"}, …) == 0`
    - nichts passt: `findMonitor({{0,0,3840,2160},"DP-9"}, …) == std::nullopt`
  - **nameMonitors**:
    - Output `{"DP-3","LG Electronics 27GL850 (DP-3)",{1920,0,2560,1440}}` ergibt die description `"LG Electronics 27GL850"` und den name `"DP-3"`.
    - Ein Monitor ohne passenden Output ergibt `"Monitor 2 · 1920×1080"`.
    - HiDPI: logisch `{0,0,1707,960}`, pixel 2560×1440, Output-Geometrie `{0,0,1707,960}` → passt.
  - **fallbackGeometry**: `{{2560,1440},{1920,1080}}` ergibt `{0,0,2560,1440}` und `{2560,0,1920,1080}`.
  - **desktopBounds**:
    - Testsystem: `{0,0,4480,1440}`
    - Mit `{-1920,0,1920,1080}` und `{0,0,2560,1440}`: `{-1920,0,4480,1440}`
  - **desktopCenterX**:
    - Lampe `uvs{{0.6,0},{1,1}}` auf DP1 (x 1536…1920, Mitte 1728) bei Bounds `{0,0,4480,1440}`: Ergebnis ≈ 1728/4480, Toleranz 1e-4.
    - Lampe `{{0,0},{0.4,1}}` auf DP3: Mitte 1920+512=2432, also ≈ 2432/4480, größer als der erste Wert.
  - **subsampleWidthFor**: `(256, 2560, 1920) == 192`; `(256, 2560, 2560) == 256`; `(5000, 2560, 1920) == 1920`; `(0, 2560, 1920) == 1`.
  - **defaultMonitorForNewLamp**: `({1,1,0}, 2) == 1`; `({0,1}, 2) == 0`; `({}, 2) == 0`.

- [ ] **Step 2: Run the test and check that it fails.** `cmake --build …/build-tests --target TestMonitors` scheitert, weil `Monitors.hpp` fehlt.
- [ ] **Step 3: Implement** `Monitors.hpp` und `Monitors.cpp` nach den Interfaces und Regeln oben.
- [ ] **Step 4: Run the tests.** Erwartet: `ctest … -R Monitors` PASS, alle anderen Tests ebenfalls PASS.
- [ ] **Step 5: Commit** mit der Nachricht „Add the monitor model and its matching rules“.

### Task 2: Wayland-Ausgaben lesen

**Files:**
- Create: `include/Huenicorn/Grabber/GnuLinux/WaylandOutputs.hpp`, `src/Grabber/GnuLinux/WaylandOutputs.cpp`
- Modify: `CMakeLists.txt`:
  - `pkg_check_modules(WAYLAND REQUIRED wayland-client)` und `pkg_get_variable(WAYLAND_PROTOCOLS_DIR wayland-protocols pkgdatadir)`.
  - Der `wayland-scanner` erzeugt `xdg-output-unstable-v1-client-protocol.h` und `…-protocol.c` aus `${WAYLAND_PROTOCOLS_DIR}/unstable/xdg-output/xdg-output-unstable-v1.xml` ins Build-Verzeichnis.
  - Beide kommen zur App, `WAYLAND_LIBRARIES` wird gelinkt.
- Modify: `dist/arch/PKGBUILD`, um `wayland` zu `depends` und `wayland-protocols` zu `makedepends` hinzuzufügen. Vorher `git update-index --no-skip-worktree dist/arch/PKGBUILD`, danach wieder `--skip-worktree`.
- Modify: `README.md` und `README.de.md`, Abhängigkeitsliste um `wayland wayland-protocols` ergänzen.

**Interfaces:**
- Consumes: `WaylandOutput`, `MonitorGeometry` aus Task 1.
- Produces: `std::vector<Huenicorn::Grabber::WaylandOutput> listWaylandOutputs();`
  - Verbindet sich mit `wl_display_connect(nullptr)`, bindet `wl_output` (min(version, 4)) und `zxdg_output_manager_v1` (min(version, 3)).
  - Danach zwei Roundtrips: Globals, dann Output-Events.
  - Die Felder kommen aus diesen Events:
    - `name` aus `wl_output.name`
    - `description` aus `wl_output.description`; fehlt es (Version < 4), aus `zxdg_output_v1.name`/`description`
    - Geometrie aus `zxdg_output_v1.logical_position`/`logical_size`
  - Danach wird getrennt.
  - Ohne Display oder ohne xdg-output: leere Liste und ein `Logger::warn`.

- [ ] **Step 1: Implement** `listWaylandOutputs()`. Das ist reiner Glue-Code ohne Unit-Test, der Nachweis läuft live in Step 3.
- [ ] **Step 2: Build** mit `cmake --build build -j`, ohne Warnungen.
- [ ] **Step 3: Verify live.** Ein Wegwerf-Aufruf in `tests/src/TestGrabber.cpp`, der jedes Output loggt, mit `cmake --build …/build-tests --target TestGrabber`.
  - Erwartet: zwei Zeilen mit `DP-3 … 1920,0 2560x1440` und `DP-1 … 0,249 1920x1080`, verglichen mit `kscreen-doctor -o`.
  - Den Wegwerf-Aufruf danach entfernen, falls er nicht als nützliche Ausgabe von TestGrabber bleibt.
- [ ] **Step 4: Commit** mit der Nachricht „Read the monitors' names from the compositor“.

### Task 3: Portal mit mehreren Streams

**Files:**
- Modify: `include/Huenicorn/Grabber/GnuLinux/Pipewire/XdgDesktopPortal.hpp`
- Modify: `src/Grabber/GnuLinux/Pipewire/XdgDesktopPortal.cpp`:
  - `selectSource` um 486-525
  - `onStartResponseReceivedCallback` um 335-382

**Interfaces:**
- Produces in `XdgDesktopPortal`:
  ```cpp
  struct PortalStream { uint32_t node{0}; std::optional<Grabber::MonitorGeometry> geometry; };
  // in Capture: replaces pwNode
  std::vector<PortalStream> streams;
  bool askForMonitors{false};                 // ignore the saved restore token, so the dialog opens
  std::optional<std::string> restoreToken;    // received token, saved by the grabber once capture works
  ```
- **Verhalten:**
  - `multiple` wird `TRUE`.
  - Mit `askForMonitors` wird kein `restore_token` mitgeschickt.
  - Der Start-Callback liest alle Streams: `position` und `size` aus dem Properties-Vardict, beide `(ii)`; fehlt eins, ist die Geometrie `nullopt`. Er schreibt den Token nur noch nach `capture->restoreToken`, nicht mehr in die Config.
  - Die Fehlermeldung „received more than one stream“ entfällt. Null Streams gelten als Fehler: `resolveCapture(capture, false)`.

- [ ] **Step 1: Implement.** Alle bisherigen Nutzer von `capture->pwNode` bekommen in Task 4 `streams`. Bis dahin baut Task 3 nur, wenn `PipewireGrabber` übergangsweise `streams.front().node` nimmt und den Token nach erfolgreichem Start selbst speichert. Das gehört in diesen Commit, damit jeder Commit baut.
- [ ] **Step 2: Build and verify live.**
  - Dienst stoppen, `build/lightspill` starten.
  - Erwartet: kein Dialog, weil der Token noch für einen Monitor gilt, und im Log `Capture path: GPU`.
  - Danach den Dienst wieder starten.
- [ ] **Step 3: Commit** mit der Nachricht „Let the portal return several monitors“.

### Task 4: Grabber mit einem Stream pro Monitor

**Files:**
- Modify: `include/Huenicorn/Grabber/IGrabber.hpp`
- Modify: `include/Huenicorn/Grabber/GnuLinux/Pipewire/PipewireGrabber.hpp`, `src/Grabber/GnuLinux/Pipewire/PipewireGrabber.cpp`
- Modify: `include/Huenicorn/Platform/IAdapter.hpp:98` und die GnuLinux-Adapter-Quelle: `getGrabber(Core::Config*, bool askForMonitors = false)` gibt jetzt `std::shared_ptr<Grabber::IGrabber>` zurück.
- Modify: `include/Huenicorn/Core/Runtime.hpp:187` und alle `m_grabber`-Nutzer in `src/Core/Runtime.cpp` und `src/Core/CoreService.cpp`: mechanische Anpassung auf den neuen Typ. Das echte Monitor-Mapping kommt in Task 5.
- Modify: `tests/src/TestGrabber.cpp` auf die neue Region-Struktur.

**Interfaces:**
- Consumes: `Capture::streams`, `Capture::restoreToken`, `askForMonitors` (Task 3); `MonitorInfo`, `nameMonitors`, `fallbackGeometry`, `subsampleWidthFor` (Task 1); `listWaylandOutputs` (Task 2).
- Produces in `IGrabber`:
  ```cpp
  struct ChannelRegion { uint8_t channelId; size_t monitor; Imaging::UVs uvs; bool operator==(const ChannelRegion&) const = default; };
  using ChannelRegions = std::vector<ChannelRegion>;      // replaces the pair list
  virtual Monitors monitors() const = 0;                  // stable order = portal stream order
  ```
- **Bestehende Methoden mit neuer Bedeutung:**
  - `displayResolution()` liefert die Pixelauflösung des breitesten Monitors.
  - `displayRefreshRate()` liefert das Maximum aller Monitore.
  - `processingInfo()`:
    - `gpu` ist nur wahr, wenn alle aktiven Streams GPU nutzen.
    - Läuft ein aktiver Stream über die CPU, enthält `details` dessen `cpuReason` mit der Monitor-`description` davor.
    - Ohne aktive Streams gilt der Zustand des ersten Streams.
  - `setChannelRegions` verteilt die Regionen auf die Streams. Ein Stream ohne Regionen und jeder Stream bei Pause wird über `pw_stream_set_active` auf dem Pipewire-Loop deaktiviert (`pw_loop_invoke` wie `_onUpdatePaused`). Regionen mit `monitor >= streams.size()` werden ignoriert.
  - `grabChannelColors` fügt die Farben aller Streams zusammen. Rückgabe `true`, sobald ein Stream mit Regionen schon Farben hat.
  - `setFrameRateLimit` gilt für jeden Stream.
- **Konstruktor:** `PipewireGrabber(Core::Config* config, bool askForMonitors = false)`. Er speichert `capture.restoreToken` erst nach erfolgreichem Pipewire-Start in die Config (`config->setRestoreToken`, Log wie bisher).
- **Struktur, intern:**
  - `PipewireData` behält Loop, Context, Core, EGL (`gpu`, `dmaBufFormats`), Renegotiate-Event und Config.
  - Neu ist `struct StreamData { PipewireData* owner; size_t index; pw_stream* stream; spa_hook listener; spa_video_info format{}; std::atomic<unsigned> displayMaxFramerate{0}; bool dmaBufFailed{false}; bool usingDmaBuf{false}; std::optional<uint64_t> loggedModifier; std::atomic<bool> lastFrameOnGpu{false}; std::string cpuReason; ChannelRegions regions; ChannelColors colors; bool hasColors{false}; bool active{true}; std::optional<MonitorGeometry> portalGeometry; };`, gehalten in `std::vector<std::unique_ptr<StreamData>>`.
  - Regionen, Farben und `cpuReason` stehen unter dem bestehenden Mutex.
  - Die Stream-Callbacks bekommen `StreamData*`.
  - `_onRenegotiate` und die Format-Updates laufen über alle Streams.
  - `screenDataReadyPromise` wird erfüllt, wenn **jeder** Stream ein Format hat (Zähler).
  - Die Subsample-Breite pro Stream kommt aus `subsampleWidthFor(config->subsampleWidth(), breitesterPixelWidth, streamPixelWidth)`.
  - Das Log zeigt pro Stream einmal den Pfad mit Monitor-Name, etwa `Capture path (DP-3): GPU (DMA-BUF, modifier 0x…)`.
  - Die Monitor-Namen setzt `monitors()` aus `listWaylandOutputs()`. Das wird einmal im Konstruktor nach Formatfixierung gelesen und gecacht.
  - Ohne Portal-Geometrie gilt `fallbackGeometry`.

- [ ] **Step 1: Refactor.** Ziel: ein Stream verhält sich exakt wie vorher.
- [ ] **Step 2: Build and run tests.** Erwartet: alle Unit-Tests PASS, `TestGpuDownscale` PASS.
- [ ] **Step 3: Verify live with one monitor.** Dienst stoppen, `build/lightspill` starten. Erwartet:
  - kein Dialog
  - Lampen folgen DP-3 wie vorher
  - `perf stat -e task-clock -p <pid> -- sleep 20` liegt bei etwa 2–3,5 % eines Kerns, wie vorher
- [ ] **Step 4: Verify live with two monitors.**
  - `restoreToken` aus der Config vorübergehend entfernen (vorher sichern). Der Nutzer hakt im Dialog beide Monitore an.
  - Erwartet: zwei `Capture path`-Zeilen. `TestGrabber` bzw. ein Debug-Log zeigt pro Stream Farben, sobald dort eine Region liegt.
  - Danach den gesicherten Token zurückschreiben, falls der Nutzer bei einem Monitor bleiben will. Sonst den neuen behalten.
- [ ] **Step 5: Verify live with `LIGHTSPILL_DISABLE_GPU=1`.** Erwartet: beide Streams `CPU (shared memory)`, Farben korrekt.
- [ ] **Step 6: Commit** mit der Nachricht „Capture every selected monitor in its own stream“.

### Task 5: Monitor pro Kanal in Runtime und Profil

**Files:**
- Modify: `include/Huenicorn/Hue/Api/Channel.hpp`: neues Feld `std::optional<Grabber::MonitorRef> monitor;` plus transient `std::optional<size_t> monitorIndex;`. `nullopt` bedeutet „Monitor fehlt“.
- Modify: `include/Huenicorn/Serialization/Channel.hpp`:
  - `to_json` schreibt `"monitor": {"x","y","width","height","name"}`, wenn gesetzt.
  - Neu ist `std::optional<Grabber::MonitorRef> monitorRefFromJson(const Json& jsonChannel)`. Es liefert `nullopt` bei fehlendem oder fehlerhaftem Feld.
- Modify: `src/Core/Runtime.cpp`:
  - `_initChannels` (204-240) liest `monitor`.
  - Region-Aufbau (337-347) und Pan (409).
  - Neu ist `void _resolveChannelMonitors()`:
    - Gerufen nach `_initChannels` und nach jedem Grabber-Tausch.
    - Kanäle ohne `monitor` bekommen `monitorRef(monitors[0])` (Migration).
    - Danach gilt `monitorIndex = findMonitor(...)`.
  - Es gibt einen Cache `Grabber::Monitors m_monitors`.
- Modify: `src/Core/CoreService.cpp`:
  - Szenen-Verteilung (513) nutzt `desktopCenterX`.
  - `setChannelActivity`: Eine neu aktive Lampe ohne Monitor-Referenz bekommt `defaultMonitorForNewLamp`.
  - Neu ist `std::optional<Imaging::UVs> setChannelMonitor(uint8_t channelId, size_t monitor, const Imaging::UVs& uvs)`. Es klemmt die UVs auf [0,1], setzt `monitor = monitorRef(m_monitors[monitor])` und `monitorIndex`, und gibt `nullopt` bei unbekanntem Kanal oder Index zurück.
- Create: `tests/src/TestChannelMonitors.cpp` mit Target `TestChannelMonitors` (Quellen `Monitors.cpp`, `Channel.cpp`, `Logger.cpp`) in `tests/AddTests.cmake`.

**Interfaces:**
- Consumes: `findMonitor`, `monitorRef`, `desktopBounds`, `desktopCenterX`, `defaultMonitorForNewLamp` (Task 1); `IGrabber::monitors()`, `ChannelRegion` (Task 4).
- Produces:
  - `CoreService::setChannelMonitor(...)` (oben).
  - `CoreService::monitors() -> Grabber::Monitors`.
  - `Channel::monitorIndex` für die API in Task 7.
- **Verhalten im Loop:**
  - Aktive Kanäle mit `monitorIndex` liefern `ChannelRegion{id, *monitorIndex, uvs}`.
  - Aktive Kanäle ohne Index werden schwarz gestreamt, wie ein Kanal ohne Farbe, und nie an den Grabber gegeben.
  - Der Pan nutzt `desktopCenterX(uvs, m_monitors[idx].geometry, desktopBounds(m_monitors))`. Ohne Index ist er 0,5.

- [ ] **Step 1: Write the failing tests** in `TestChannelMonitors.cpp`:
  - Ein JSON-Kanal ohne `monitor` ergibt `monitorRefFromJson(...) == std::nullopt`.
  - Ein JSON-Kanal mit `{"x":1920,"y":0,"width":2560,"height":1440,"name":"DP-3"}` ergibt `MonitorRef{{1920,0,2560,1440},"DP-3"}`.
  - Ein fehlerhaftes Feld (`"monitor": 3`) ergibt `nullopt` und keine Exception.
  - Roundtrip: Ein `Channel` mit Monitor wird über `Json(channel)` serialisiert und mit `monitorRefFromJson` gelesen; Ergebnis gleich.
- [ ] **Step 2: Run the test and check that it fails.** Erwartet: Compile-Fehler, weil `monitorRefFromJson` fehlt.
- [ ] **Step 3: Implement** alles oben Genannte.
- [ ] **Step 4: Run the tests.** Erwartet: alle Tests PASS.
- [ ] **Step 5: Verify live.**
  - Mit dem bestehenden Profil starten. Danach `POST /api/saveProfile`, dann `jq '.channels[].monitor' ~/.config/lightspill/profile.json` (Pfad laut `profileName`).
  - Erwartet: beide Lampen haben DP-3 als Monitor, die Lampen verhalten sich wie vorher.
- [ ] **Step 6: Commit** mit der Nachricht „Remember the monitor each lamp follows“.

### Task 6: Monitore zur Laufzeit neu wählen

**Files:**
- Modify: `include/Huenicorn/Core/Runtime.hpp`, `src/Core/Runtime.cpp`
- Modify: `include/Huenicorn/Core/CoreService.hpp`, `src/Core/CoreService.cpp`

**Interfaces:**
- Consumes: `Platform::adapter.getGrabber(config, true)` (Task 4); `_resolveChannelMonitors()` (Task 5).
- Produces:
  ```cpp
  enum class MonitorSelection { Idle, Pending, Cancelled };
  bool CoreService::reselectMonitors();             // false if a selection is already pending
  MonitorSelection CoreService::monitorSelection() const;
  ```
- **Verhalten:**
  - **Grabber-Zugriff:**
    - `Runtime::m_grabber` wird `std::shared_ptr<Grabber::IGrabber>`, geschützt durch `std::mutex m_grabberMutex`.
    - Neuer Zugriff `std::shared_ptr<Grabber::IGrabber> Runtime::grabber() const`.
    - Alle Aufrufer holen sich einmal pro Aufruf bzw. Loop-Durchlauf eine Kopie.
  - **Ablauf von `reselectMonitors()`:**
    - Der Zustand wird `Pending`.
    - Ein `std::jthread` baut `getGrabber(&m_config, true)`.
  - **Bei Erfolg:**
    - Unter dem Mutex wird getauscht.
    - `m_channelRegions.clear()` erzwingt neue Regionen.
    - Der Pausenzustand wird übertragen (`setPaused(m_capturePaused)`), ebenso `setFrameRateLimit(refreshRate)`.
    - `_resolveChannelMonitors()` läuft unter `m_channelsMutex`.
    - Der Zustand wird `Idle`.
  - **Bei Abbruch oder Exception (`GrabberCancelled`, `GrabberUnavailable`):**
    - Der alte Grabber läuft weiter.
    - Der Zustand wird `Cancelled`; der nächste `reselectMonitors()`-Aufruf setzt ihn zurück.
  - **Ende:** `Runtime::stop()` bricht eine laufende Wahl ab. Dafür wird der Grabber-Konstruktor über `GCancellable` abbrechbar gemacht, falls nötig; sonst wartet `stop()` auf das Ende des Threads.
  - **Kein zweiter Systemdialog:** Ein zweiter Aufruf während `Pending` gibt `false` zurück.

- [ ] **Step 1: Implement.**
- [ ] **Step 2: Build and run tests.** Erwartet: alle Tests PASS.
- [ ] **Step 3: Verify live.** Ausgelöst wird per `curl -X POST http://127.0.0.1:8215/api/reselectMonitors` (die Route kommt in Task 7; bis dahin vorübergehend einen Debug-Aufruf nutzen oder Task 6 und 7 zusammen verifizieren).
  1. Den Dialog bricht der Nutzer ab. Erwartet: Die Lampen laufen ohne Unterbrechung weiter, der Zustand ist `Cancelled`.
  2. Erneut auslösen, der Nutzer hakt beide Monitore an. Erwartet: Wechsel ohne Neustart, neuer Token in der Config.
  3. Dienst neu starten. Erwartet: kein Dialog, beide Monitore wieder da.
- [ ] **Step 4: Commit** mit der Nachricht „Choose the monitors again without a restart“.

### Task 7: HTTP-API

**Files:**
- Modify: `src/Network/Http/Server/WebUIBackend.cpp`: `displayInfo` 112-141, Kanal-Routen 79-93, neue Routen
- Modify: `include/Huenicorn/Serialization/Channel.hpp` (Web-Variante) oder die Kanal-Routen direkt

**Interfaces:**
- Consumes: `CoreService::monitors()`, `setChannelMonitor` (Task 5); `reselectMonitors()`, `monitorSelection()` (Task 6).
- **Produces (Wire-Format):**
  - `GET /api/displayInfo` erhält zusätzlich:
    - `"monitors": [{"index":0,"name":"DP-3","description":"LG Electronics 27GL850","x":1920,"y":0,"width":2560,"height":1440,"pixelWidth":2560,"pixelHeight":1440,"gpu":true,"active":true}, …]`
    - `"monitorSelection": "idle" | "pending" | "cancelled"`
  - `GET /api/channels` und `GET /api/channel/:id`: Jeder Kanal erhält `"monitor": <index> | null` und `"monitorMissing": <bool>`. `monitorMissing` ist `active && monitor == null`.
  - `PUT /api/setChannelMonitor/:channelId`:
    - Body `{"monitor":1,"uvs":{"min":{"x":0.1,"y":0.1},"max":{"x":0.4,"y":0.9}}}`, im selben UV-JSON-Format wie das Profil.
    - Antwort 200 mit dem Kanal-JSON (wie `/api/channel/:id`).
    - Antwort 400 bei ungültigem Body oder Index.
  - `POST /api/reselectMonitors`:
    - Antwort 200 `{"started":true}`.
    - Bei laufender Wahl `{"started":false}`.

- [ ] **Step 1: Implement.**
- [ ] **Step 2: Verify with curl** gegen den laufenden Test-Build:
  - `curl -s 127.0.0.1:8215/api/displayInfo | jq '.monitors, .monitorSelection'` zeigt zwei Monitore mit Namen.
  - `curl -s -X PUT -d '{"monitor":1,"uvs":{"min":{"x":0,"y":0},"max":{"x":0.5,"y":1}}}' 127.0.0.1:8215/api/setChannelMonitor/<id> | jq .monitor` gibt `1` aus.
  - Mit Index 7: HTTP 400.
- [ ] **Step 3: Commit** mit der Nachricht „Serve the monitors and the lamps' monitors to the light manager“.

### Task 8: Light Manager – Kopfzeile und Desktop-Editor

**Files:**
- Modify: `webroot/index.html`: Kopfzeile 10-25, Bereichs-Karte 28-70
- Modify: `webroot/ScreenWidget.js`, `webroot/WebUI.js`, `webroot/I18n.js`, `webroot/style.css`

**Interfaces:**
- Consumes: API aus Task 7.
- **Kopfzeile:**
  - Neues Element `<span id="monitorInfo">` mit dem Text `Source: ` plus die `description` der aktiven Monitore, verbunden mit ` · `. Ohne aktive Monitore stehen dort alle Monitore.
  - Neuer Knopf `<button id="reselectMonitorsButton">Monitors…</button>`.
  - Während `pending` ist er deaktiviert und zeigt `Waiting for the screen selection…`.
  - Bei `cancelled` erscheint kurz der Hinweis `Selection cancelled, nothing changed`.
  - Polling über das bestehende `displayInfo`-Polling.
- **Editor (`ScreenWidget`):**
  - Eine äußere SVG-`viewBox` aus den Desktop-Grenzen (min/max über alle Monitore, negative Koordinaten erlaubt). Darin ein verschachteltes `<svg>` je Monitor mit bisherigem Raster, Live-Farb-Rechtecken und Beschriftung (`description`, unten rechts).
  - Monitore mit `active:false` werden gedimmt gezeichnet.
  - Die Region der gewählten Lampe wird aus `(monitor, uvs)` in Desktop-Koordinaten umgerechnet.
  - **Ziehen der ganzen Region:**
    - Beim Loslassen bestimmt die Mitte den Ziel-Monitor.
    - Liegt sie außerhalb aller Monitore, gilt der nächstgelegene Monitor (Abstand Mitte zu Rechteck).
    - Die Region wird ohne Größenänderung in den Monitor geschoben. Ist sie größer als der Monitor, wird sie auf den Monitor gekürzt.
    - Danach folgt `PUT /api/setChannelMonitor`.
  - **Eckgriffe:** Sie bleiben auf ihrem Monitor begrenzt und nutzen weiter `setChannelUV`.
  - **Ein Monitor:** Das Bild ist identisch zu heute.
- **Kanal-Hinweise:**
  - Kanäle mit `monitorMissing` zeigen in der Lampenliste `Monitor not captured – lamp is off`.
  - Tooltip: `The monitor this lamp followed isn't captured. Choose it again with "Monitors…", or drag the region onto a captured monitor.`
- **Neue Texte** (englisch, mit deutscher Übersetzung in `I18n.js`):

  | Englisch | Deutsch |
  |---|---|
  | `Source: ` | `Quelle: ` |
  | `Monitors…` | `Monitore…` |
  | Tooltip: `Opens the system dialog to choose which monitors Lightspill captures. Tick every monitor a lamp should follow. Until you confirm, everything keeps running as before.` | `Öffnet den Systemdialog, in dem du wählst, welche Monitore Lightspill erfasst. Hake alle Monitore an, denen Lampen folgen sollen. Bis du bestätigst, läuft alles weiter wie bisher.` |
  | `Waiting for the screen selection…` | `Warte auf die Bildschirmauswahl…` |
  | `Selection cancelled, nothing changed` | `Auswahl abgebrochen, nichts geändert` |
  | `Monitor not captured – lamp is off` | `Monitor nicht erfasst – Lampe aus` |
  | Kanal-Tooltip (oben) | `Der Monitor, dem diese Lampe gefolgt ist, wird nicht erfasst. Wähle ihn über „Monitore…“ erneut oder zieh den Bereich auf einen erfassten Monitor.` |
  | Tooltip der Bereichs-Karte, erweitert um: `With several monitors, drag a region onto another monitor to make the lamp follow it.` | `Bei mehreren Monitoren: Zieh einen Bereich auf einen anderen Monitor, damit die Lampe ihm folgt.` |

- [ ] **Step 1: Implement.**
- [ ] **Step 2: Verify in the app** (`lightspill-gui` vom Build bzw. DevTools-Capture, nie Screenshot des Desktops):
  1. Beide Monitore erscheinen in der realen Anordnung: DP-1 links und 249 px tiefer, DP-3 rechts.
  2. Die Kopfzeile zeigt beide Namen.
  3. Lampe „links“ auf DP-1 ziehen. Erwartet: Sie folgt DP-1. Im Profil steht nach dem Speichern `DP-1`.
  4. Alle Lampen auf DP-3 ziehen. Erwartet: DP-1 gedimmt, `perf stat` wie mit einem Monitor.
  5. Beide genutzt. Erwartet: `perf stat` etwa +3 % eines Kerns.
  6. Sprache Deutsch: alle neuen Texte übersetzt (Skript wie beim letzten Review: alle `t()`/`title`-Texte gegen `I18n.js`).
  7. Fensterbreite 600 px: Der Editor passt ohne horizontales Scrollen.
- [ ] **Step 3: Commit** mit der Nachricht „Show the whole desktop in the region editor“.

### Task 9: Einrichtung, Dokumentation, Abschluss

**Files:**
- Modify: `webroot/setup.html`/`webroot/Setup.js` (letzter Schritt, Text), `webroot/I18n.js`
- Modify: `README.md`, `README.de.md`: Abschnitt „Light manager“ und „First start“, Konfigurationsfeld `monitor` in `profile.json`, Troubleshooting „Choosing another screen“ ersetzt durch den Knopf „Monitors…“

**Texte:**
- **Setup**, englisch: `The desktop asks which screens Lightspill may capture. Tick every monitor a lamp should follow, and allow it to be remembered.`
- **Setup**, deutsch: `Der Desktop fragt, welche Bildschirme Lightspill erfassen darf. Hake alle Monitore an, denen Lampen folgen sollen, und erlaube, dass die Auswahl gespeichert wird.`
- **README, Profilfeld:**
  - englisch: `**monitor**: (object) the monitor the lamp follows: its position and size on the desktop (**x**, **y**, **width**, **height**) and its connector **name**. A lamp without it follows the first captured monitor`
  - deutsch: sinngemäß.

- [ ] **Step 1: Implement** die Texte.
- [ ] **Step 2: Clean build and run all tests.** Frischer Build mit Tests und GUI: keine Warnungen, alle Tests PASS.
- [ ] **Step 3: Final live run** gegen die Checkliste aus dem Spec, Abschnitt „Tests“, Punkte 1–7.
- [ ] **Step 4: Commit** mit der Nachricht „Explain choosing monitors in the setup and the README“.
- [ ] **Step 5: Ask the user** ob in `master` gemergt, auf Forgejo gepusht und das Paket gebaut werden soll.
