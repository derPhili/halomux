// Translation of the web UI. English texts are the source: the page is written in English and
// German translations are looked up by the English text. Dynamic texts go through t().
class I18n
{
  /**
   * Loads the language setting and translates the page
   */
  static init()
  {
    return RequestUtils.get("/api/language")
    .then((data) => {I18n.setting = data.language;})
    .catch((error) => {log(error);})
    .finally(() => {
      I18n.language = I18n.resolve(I18n.setting);
      document.documentElement.lang = I18n.language;
      I18n.translatePage(document.body);
      document.body.classList.remove("i18nPending");
    });
  }


  /**
   * Language to show for a setting: "auto" follows the system, English if it's neither German nor English
   */
  static resolve(setting)
  {
    if(setting in I18n.Translations || setting == "en"){
      return setting;
    }

    let systemLanguage = (navigator.languages && navigator.languages[0]) || navigator.language || "en";
    let language = systemLanguage.toLowerCase().split("-")[0];
    return language in I18n.Translations ? language : "en";
  }


  /**
   * Translates text and parameters such as {name}
   */
  static t(text, parameters = {})
  {
    let translations = I18n.Translations[I18n.language];
    let translated = (translations && translations[text]) || text;
    return translated.replace(/\{(\w+)\}/g, (match, name) => (name in parameters ? parameters[name] : match));
  }


  /**
   * Translates the texts, tooltips, placeholders and empty list notices written in the HTML
   */
  static translatePage(root)
  {
    if(!I18n.Translations[I18n.language]){
      return;
    }

    let walker = document.createTreeWalker(root, NodeFilter.SHOW_TEXT);
    let textNodes = [];
    while(walker.nextNode()){
      textNodes.push(walker.currentNode);
    }

    for(let node of textNodes){
      let text = node.nodeValue.trim();
      if(text && !["SCRIPT", "STYLE"].includes(node.parentNode.nodeName)){
        node.nodeValue = node.nodeValue.replace(text, I18n.t(text));
      }
    }

    for(let attribute of ["title", "placeholder", "data-empty"]){
      for(let element of root.querySelectorAll(`[${attribute}]`)){
        element.setAttribute(attribute, I18n.t(element.getAttribute(attribute)));
      }
    }
  }


  /**
   * Saves the language setting and reloads the page in that language
   */
  static setLanguage(setting)
  {
    RequestUtils.put("/api/setLanguage", JSON.stringify(setting))
    .then(() => {location.reload();})
    .catch((error) => {log(error);});
  }
}


// "auto", "en" or "de"
I18n.setting = "auto";
I18n.language = "en";

I18n.Translations = {
  de : {
    // Header
    "Sync on: the lamps follow the screen (and the sound, if enabled).\nSync off: the lamps show the static scene and screen capture pauses.":
      "Sync an: Die Lampen folgen dem Bildschirm (und dem Sound, falls aktiviert).\nSync aus: Die Lampen zeigen die statische Szene, die Bildschirmaufnahme pausiert.",
    "Sync" : "Sync",
    "Sync off" : "Sync aus",
    "Where captured frames are processed.\nGPU (green): frames stay in video memory (DMA-BUF), lowest CPU load.\nCPU (orange): fallback through shared memory.":
      "Wo die aufgenommenen Bilder verarbeitet werden.\nGPU (grün): Die Bilder bleiben im Grafikspeicher (DMA-BUF), geringste CPU-Last.\nCPU (orange): Ausweichweg über Shared Memory.",
    "Capture paused · static scene" : "Aufnahme pausiert · statische Szene",
    "The monitors whose picture the lamps follow" : "Die Monitore, deren Bild die Lampen folgen",
    "Source: " : "Quelle: ",
    "Monitors…" : "Monitore…",
    "Opens the system dialog to choose which monitors Lightspill captures. Tick every monitor a lamp should follow. Until you confirm, everything keeps running as before.":
      "Öffnet den Systemdialog, in dem du wählst, welche Monitore Lightspill erfasst. Hake alle Monitore an, denen Lampen folgen sollen. Bis du bestätigst, läuft alles weiter wie bisher.",
    "Waiting for the screen selection…" : "Warte auf die Bildschirmauswahl…",
    "Selection cancelled, nothing changed" : "Auswahl abgebrochen, nichts geändert",

    // Screen regions
    "Which part of the screen each lamp follows. With several monitors, drag a region onto another monitor to make the lamp follow it.":
      "Welchem Bildschirmbereich jede Lampe folgt. Bei mehreren Monitoren: Zieh einen Bereich auf einen anderen Monitor, damit die Lampe ihm folgt.",
    "Drag to move the region. With several monitors, drop it onto another monitor to make the lamp follow it":
      "Ziehen, um den Bereich zu verschieben. Bei mehreren Monitoren: Auf einen anderen Monitor ziehen, damit die Lampe ihm folgt",
    "{monitor}: captured, a lamp follows it" : "{monitor}: wird erfasst, eine Lampe folgt ihm",
    "{monitor}: no lamp follows it, so it isn't captured" : "{monitor}: Keine Lampe folgt ihm, daher wird er nicht erfasst",
    "Monitor not captured – lamp is off" : "Monitor nicht erfasst – Lampe aus",
    "The monitor this lamp followed isn't captured. Choose it again with \"Monitors…\", or drag the region onto a captured monitor.":
      "Der Monitor, dem diese Lampe gefolgt ist, wird nicht erfasst. Wähle ihn über „Monitore…“ erneut oder zieh den Bereich auf einen erfassten Monitor.",
    "Screen regions" : "Bildschirmbereiche",
    "Select an active lamp, then drag the corners of its region" : "Aktive Lampe auswählen, dann die Ecken ihres Bereichs ziehen",
    "Each colored rectangle is the screen region a lamp follows, filled with the lamp's current color. Select an active lamp in the list to edit its region.":
      "Jedes farbige Rechteck ist der Bildschirmbereich, dem eine Lampe folgt, gefüllt mit ihrer aktuellen Farbe. Wähle eine aktive Lampe in der Liste, um ihren Bereich zu bearbeiten.",
    "Drag to move the top left corner of the region" : "Ziehen, um die obere linke Ecke des Bereichs zu verschieben",
    "Drag to move the top right corner of the region" : "Ziehen, um die obere rechte Ecke des Bereichs zu verschieben",
    "Drag to move the bottom left corner of the region" : "Ziehen, um die untere linke Ecke des Bereichs zu verschieben",
    "Drag to move the bottom right corner of the region" : "Ziehen, um die untere rechte Ecke des Bereichs zu verschieben",
    "Gamma correction" : "Gammakorrektur",
    "Gamma correction of this lamp: to the right, dim colors get brighter; to the left, darker. In the middle, unchanged.":
      "Gammakorrektur dieser Lampe: Nach rechts werden dunkle Farben heller, nach links dunkler. In der Mitte unverändert.",
    "{lamp}: the screen region this lamp follows, filled with its current color. Select the lamp in the list to edit it":
      "{lamp}: der Bildschirmbereich, dem diese Lampe folgt, gefüllt mit ihrer aktuellen Farbe. Wähle die Lampe in der Liste, um ihn zu bearbeiten",
    "There are no lamps in this entertainment area. Add them in the Hue app." : "In diesem Entertainment-Bereich gibt es keine Lampen. Füge sie in der Hue-App hinzu.",
    "Drag a lamp from Available to Active to drive it." : "Ziehe eine Lampe von „Verfügbar“ nach „Aktiv“, um sie zu steuern.",
    "Select an active lamp to edit its region" : "Wähle eine aktive Lampe, um ihren Bereich zu bearbeiten",

    // Static scene
    "Colors the lamps show while sync is off" : "Farben, die die Lampen zeigen, solange Sync aus ist",
    "Static scene" : "Statische Szene",
    "Shown while sync is off" : "Wird gezeigt, solange Sync aus ist",
    "Pick one to turn sync off" : "Auswahl schaltet Sync aus",
    "Showing now" : "Wird gerade gezeigt",
    "Pick colors yourself" : "Farben selbst wählen",
    "Color" : "Farbe",
    "One color for all lamps" : "Eine Farbe für alle Lampen",
    "Full color" : "Vollfarbe",
    "A separate color for each active lamp" : "Eine eigene Farbe für jede aktive Lampe",
    "Per lamp" : "Pro Lampe",
    "Opens a color picker. The chosen color shows on all lamps and turns sync off" : "Öffnet einen Farbwähler. Die gewählte Farbe erscheint auf allen Lampen und schaltet Sync aus",
    "Custom color" : "Eigene Farbe",
    "Built-in scenes: a left and a right color, spread over the lamps from left to right" : "Eingebaute Szenen: eine linke und eine rechte Farbe, von links nach rechts über die Lampen verteilt",
    "Presets" : "Vorlagen",
    "Scenes you saved. Click one to show it, × deletes it" : "Deine gespeicherten Szenen. Klick zeigt sie, × löscht sie",
    "My scenes" : "Meine Szenen",
    "Save current colors as…" : "Aktuelle Farben speichern als …",
    "Name for the current lamp colors. A scene with the same name is replaced" : "Name für die aktuellen Lampenfarben. Eine Szene mit gleichem Namen wird ersetzt",
    "Saves the current colors of the active lamps under this name" : "Speichert die aktuellen Farben der aktiven Lampen unter diesem Namen",
    "Save" : "Speichern",
    "No active lamp" : "Keine aktive Lampe",
    "No saved scene yet" : "Noch keine Szene gespeichert",
    "{name}: shows this color on all lamps and turns sync off" : "{name}: zeigt diese Farbe auf allen Lampen und schaltet Sync aus",
    "Color of {lamp} while sync is off. Picking one turns sync off" : "Farbe von {lamp}, solange Sync aus ist. Eine Auswahl schaltet Sync aus",
    "{name}: {color} on all lamps. Turns sync off" : "{name}: {color} auf allen Lampen. Schaltet Sync aus",
    "{name}: from {left} on the leftmost lamp to {right} on the rightmost. Turns sync off" : "{name}: von {left} auf der linken Lampe bis {right} auf der rechten. Schaltet Sync aus",
    "{name}: shows your saved colors on the lamps it covers. Turns sync off" : "{name}: zeigt deine gespeicherten Farben auf den betroffenen Lampen. Schaltet Sync aus",
    "Deletes the saved scene \"{name}\", after asking" : "Löscht die gespeicherte Szene „{name}“ nach Rückfrage",
    "Delete the scene \"{name}\"?" : "Szene „{name}“ löschen?",

    // Colors and built-in scenes
    "Red" : "Rot",
    "Orange" : "Orange",
    "Yellow" : "Gelb",
    "Green" : "Grün",
    "Cyan" : "Cyan",
    "Blue" : "Blau",
    "Purple" : "Lila",
    "Pink" : "Pink",
    "Warm white" : "Warmweiß",
    "Cool white" : "Kaltweiß",
    "Daylight" : "Tageslicht",
    "Sunset" : "Sonnenuntergang",
    "Ocean" : "Ozean",
    "Forest" : "Wald",
    "Neon" : "Neon",
    "Galaxy" : "Galaxie",
    "Fire" : "Feuer",
    "Candlelight" : "Kerzenlicht",
    "Off" : "Aus",

    // Lamps
    "Entertainment area of the Hue bridge to stream to, as set up in the Hue app" : "Entertainment-Bereich der Hue Bridge, an den gestreamt wird, wie in der Hue-App eingerichtet",
    "Entertainment area" : "Entertainment-Bereich",
    "Switches to another entertainment area of the bridge, with its own lamps and saved regions" : "Wechselt zu einem anderen Entertainment-Bereich der Bridge, mit eigenen Lampen und gespeicherten Bereichen",
    "Switching configuration..." : "Bereich wird gewechselt …",
    "Lamps of the entertainment area" : "Lampen des Entertainment-Bereichs",
    "Lamps" : "Lampen",
    "Drag between lists" : "Zwischen den Listen ziehen",
    "Lamps driven by this application. Click one to edit its screen region" : "Lampen, die diese Anwendung steuert. Klick bearbeitet ihren Bildschirmbereich",
    "Active" : "Aktiv",
    "Lamps of the area left alone. Drag one to Active to drive it" : "Lampen des Bereichs, die unberührt bleiben. Nach „Aktiv“ ziehen, um sie zu steuern",
    "Available" : "Verfügbar",
    "{lamp}: the dot shows its current color. Click to edit its screen region, drag to Available to stop driving it" : "{lamp}: Der Punkt zeigt die aktuelle Farbe. Klick bearbeitet den Bildschirmbereich, Ziehen nach „Verfügbar“ beendet die Steuerung",
    "{lamp}: not driven. Drag to Active to have it follow the screen" : "{lamp}: nicht gesteuert. Nach „Aktiv“ ziehen, damit sie dem Bildschirm folgt",

    // Settings
    "How the lamps react. Changes apply immediately and are kept" : "Wie die Lampen reagieren. Änderungen wirken sofort und bleiben gespeichert",
    "Settings" : "Einstellungen",
    "Plays a 12 s test sequence on the lamps instead of the screen: color cuts, flashes, a slow fade and a black to white cut, through the current settings. Click again to stop it":
      "Spielt eine 12-s-Testsequenz statt des Bildschirms auf den Lampen ab: Farbschnitte, Blitze, einen langsamen Verlauf und einen Schnitt von Schwarz auf Weiß, mit den aktuellen Einstellungen. Erneuter Klick stoppt sie",
    "Preview" : "Vorschau",
    "Stop preview · {left} s" : "Vorschau stoppen · {left} s",
    "Stops the preview: the lamps go back to the screen or the static scene" : "Stoppt die Vorschau: Die Lampen kehren zum Bildschirm oder zur statischen Szene zurück",
    "Brightness and how fast colors change" : "Helligkeit und wie schnell Farben wechseln",
    "Light" : "Licht",
    "Scales the brightness of all lamps, in sync and with static scenes. 100% = full brightness" : "Skaliert die Helligkeit aller Lampen, bei Sync und bei statischen Szenen. 100 % = volle Helligkeit",
    "Brightness" : "Helligkeit",
    "Eases each lamp toward a new color instead of jumping to it. Higher = calmer, but the lamps lag behind the picture" : "Lässt jede Lampe weich zur neuen Farbe übergehen statt zu springen. Höher = ruhiger, aber die Lampen hinken dem Bild hinterher",
    "Transition smoothing" : "Übergangsglättung",
    "10%: lights follow the screen almost instantly (90% after ~20 ms at 50 Hz)" : "10 %: Die Lampen folgen dem Bildschirm fast sofort (90 % nach ~20 ms bei 50 Hz)",
    "Gaming" : "Gaming",
    "60%: smooths hard cuts and flashes (90% after ~90 ms at 50 Hz)" : "60 %: glättet harte Schnitte und Blitze (90 % nach ~90 ms bei 50 Hz)",
    "Movie" : "Film",
    "Short brightness flashes on sudden bright moments" : "Kurze Lichtblitze bei plötzlich hellen Momenten",
    "Highlight boost" : "Highlight-Boost",
    "Lamps flash brighter when their screen region suddenly gets brighter (explosions, lightning, cuts to bright scenes), then fade back within about a second. 0% = off":
      "Die Lampen blitzen heller auf, wenn ihr Bildschirmbereich plötzlich heller wird (Explosionen, Blitze, Schnitte in helle Szenen), und klingen in etwa einer Sekunde ab. 0 % = aus",
    "Strength" : "Stärke",
    "20% of the remaining brightness on a full jump" : "20 % der verbleibenden Helligkeit bei einem vollen Sprung",
    "Subtle" : "Dezent",
    "40% of the remaining brightness on a full jump" : "40 % der verbleibenden Helligkeit bei einem vollen Sprung",
    "Moderate" : "Mittel",
    "60% of the remaining brightness on a full jump" : "60 % der verbleibenden Helligkeit bei einem vollen Sprung",
    "High" : "Hoch",
    "85% of the remaining brightness on a full jump" : "85 % der verbleibenden Helligkeit bei einem vollen Sprung",
    "Intense" : "Intensiv",
    "Sudden loudness jumps in the PC sound (explosions, shots, beats) also trigger the highlight boost: the left channel for left lamps, the right one for right lamps. Steady loud music doesn't keep triggering":
      "Plötzliche Lautstärkesprünge im PC-Sound (Explosionen, Schüsse, Beats) lösen ebenfalls den Highlight-Boost aus: der linke Kanal für linke Lampen, der rechte für rechte. Gleichmäßig laute Musik löst nicht ständig aus",
    "React to sound" : "Auf Sound reagieren",
    "How the screen is captured" : "Wie der Bildschirm aufgenommen wird",
    "Capture" : "Aufnahme",
    "Resolution the screen is reduced to before averaging the regions. Smaller is cheaper and averages more; larger follows small details" : "Auflösung, auf die der Bildschirm vor dem Mitteln der Bereiche verkleinert wird. Kleiner spart Rechenzeit und mittelt stärker, größer folgt kleinen Details",
    "Subsample resolution" : "Subsample-Auflösung",
    "How many times per second colors are captured and sent to the bridge. At most 60 Hz: the bridge doesn't process more" : "Wie oft pro Sekunde Farben aufgenommen und an die Bridge gesendet werden. Höchstens 60 Hz, mehr verarbeitet die Bridge nicht",
    "Refresh rate" : "Bildwiederholrate",
    "(max {max} Hz: {limitedBy}. Display: {display} Hz, bridge: {bridge} Hz)" : "(max. {max} Hz: {limitedBy}. Bildschirm: {display} Hz, Bridge: {bridge} Hz)",
    "Hue bridge limit" : "Limit der Hue Bridge",
    "display refresh rate" : "Bildwiederholrate des Bildschirms",

    // Footer and dialogs
    "Saves the lamps' screen regions, gamma and activity, loaded on every start. Settings and scenes are saved automatically" : "Speichert Bildschirmbereiche, Gamma und Aktivität der Lampen, geladen bei jedem Start. Einstellungen und Szenen werden automatisch gespeichert",
    "Save profile" : "Profil speichern",
    "Stops the service: the lamps go back to their state from before. The desktop application can start it again" : "Beendet den Dienst: Die Lampen kehren in ihren vorherigen Zustand zurück. Die Desktop-Anwendung kann ihn wieder starten",
    "Shut down" : "Beenden",
    "Do you really want to stop Lightspill?" : "Lightspill wirklich beenden?",
    "Stops the service now" : "Beendet den Dienst jetzt",
    "Keeps the service running" : "Lässt den Dienst weiterlaufen",
    "Cancel" : "Abbrechen",
    "Lightspill is now off" : "Lightspill ist jetzt beendet",
    "Language of the application. System language follows the desktop, English if it's neither German nor English" : "Sprache der Anwendung. „Systemsprache“ folgt dem Desktop, Englisch, wenn er weder Deutsch noch Englisch ist",
    "System language" : "Systemsprache",

    // Setup
    "Welcome" : "Willkommen",
    "Lightspill makes your Philips Hue lamps follow your screen. The setup takes about two minutes. You need:" : "Lightspill lässt deine Philips-Hue-Lampen deinem Bildschirm folgen. Die Einrichtung dauert etwa zwei Minuten. Du brauchst:",
    "A Hue bridge in the same network as this computer" : "Eine Hue Bridge im selben Netzwerk wie dieser Computer",
    "Access to the bridge, to press its button" : "Zugang zur Bridge, um ihren Knopf zu drücken",
    "The Hue app, to set up an entertainment area with your lamps" : "Die Hue-App, um einen Entertainment-Bereich mit deinen Lampen einzurichten",
    "AI-assisted project, no warranty, no liability" : "KI-gestütztes Projekt, keine Gewähr, keine Haftung",
    "Lightspill is developed with the help of AI: its changes since the fork of Huenicorn were largely written by an AI (Claude by Anthropic) and were tested only on a few setups." : "Lightspill wird mit Hilfe von KI entwickelt: Seine Änderungen seit dem Fork von Huenicorn hat überwiegend eine KI (Claude von Anthropic) geschrieben, getestet sind sie nur auf wenigen Systemen.",
    "The maintainer is not responsible for the code, its correctness or its security, nor for the consequences of its use." : "Der Maintainer ist nicht verantwortlich für den Code, seine Richtigkeit oder Sicherheit und nicht für die Folgen seiner Nutzung.",
    "Accept and continue" : "Zustimmen und fortfahren",
    "Use it at your own risk. It is provided \"as is\", without any warranty, and to the extent permitted by law the maintainer accepts no liability for any damage or other consequences of its use. There is no obligation to provide support, maintenance, updates or fixes." : "Die Nutzung erfolgt auf eigenes Risiko. Die Software wird ohne jede Gewährleistung bereitgestellt, und soweit gesetzlich zulässig übernimmt der Maintainer keine Haftung für Schäden oder sonstige Folgen ihrer Nutzung. Es besteht keine Pflicht zu Support, Wartung, Updates oder Fehlerbehebungen.",
    "I have read and understood this notice." : "Ich habe diesen Hinweis gelesen und verstanden.",
    "Confirm that you read the notice first" : "Bestätige zuerst, dass du den Hinweis gelesen hast",
    "Get started" : "Los geht's",
    "Cancel setup" : "Einrichtung abbrechen",
    "Step {step} of {count}" : "Schritt {step} von {count}",
    "Find your bridge" : "Bridge finden",
    "Pick your Hue bridge. If it isn't found, enter its IP address: the Hue app shows it under Settings → My Hue system → Bridge." : "Wähle deine Hue Bridge. Wird sie nicht gefunden, gib ihre IP-Adresse ein: Die Hue-App zeigt sie unter Einstellungen → Mein Hue-System → Bridge.",
    "Searching for bridges…" : "Suche nach Bridges …",
    "Bridge address" : "Adresse der Bridge",
    "Continue" : "Weiter",
    "Search again" : "Erneut suchen",
    "Back" : "Zurück",
    "Hue bridge {id}" : "Hue Bridge {id}",
    "No bridge answered in the local network, and the online search needs internet access. Enter the address instead." : "Im lokalen Netz hat keine Bridge geantwortet, und die Online-Suche braucht Internetzugang. Gib stattdessen die Adresse ein.",
    "No bridge was found. Enter its address instead." : "Es wurde keine Bridge gefunden. Gib stattdessen ihre Adresse ein.",
    "Pick a bridge or enter its address." : "Wähle eine Bridge oder gib ihre Adresse ein.",
    "No Hue bridge answers at {address}." : "Unter {address} antwortet keine Hue Bridge.",
    "Pair with the bridge" : "Mit der Bridge koppeln",
    "Press the round button on top of your Hue bridge now." : "Drücke jetzt den runden Knopf oben auf deiner Hue Bridge.",
    "The app pairs as soon as the button is pressed, you don't need to click anything here." : "Die App koppelt sich, sobald der Knopf gedrückt ist. Hier musst du nichts anklicken.",
    "Waiting for the button… {seconds} s" : "Warte auf den Knopfdruck … {seconds} s",
    "The button wasn't pressed in time. Press it, then try again." : "Der Knopf wurde nicht rechtzeitig gedrückt. Drück ihn und versuch es dann erneut.",
    "The bridge refused the pairing: {error}" : "Die Bridge hat die Kopplung abgelehnt: {error}",
    "Paired with the bridge ✓" : "Mit der Bridge gekoppelt ✓",
    "Try again" : "Erneut versuchen",
    "I already have credentials for this bridge" : "Ich habe schon Zugangsdaten für diese Bridge",
    "Username" : "Benutzername",
    "Client key" : "Client-Schlüssel",
    "Use these credentials" : "Diese Zugangsdaten verwenden",
    "Enter both the username and the client key." : "Gib Benutzername und Client-Schlüssel ein.",
    "The bridge doesn't accept these credentials." : "Die Bridge akzeptiert diese Zugangsdaten nicht.",
    "The lamps are driven through an entertainment area of the bridge, which you set up in the Hue app." : "Die Lampen werden über einen Entertainment-Bereich der Bridge gesteuert, den du in der Hue-App einrichtest.",
    "{count} lamps" : "{count} Lampen",
    "No entertainment area was found yet." : "Es wurde noch kein Entertainment-Bereich gefunden.",
    "Open the Hue app and go to Settings → Entertainment areas." : "Öffne die Hue-App und gehe zu Einstellungen → Entertainment-Bereiche.",
    "Add an area, for example \"PC\", and select the lamps around your screen." : "Füge einen Bereich hinzu, zum Beispiel „PC“, und wähle die Lampen rund um deinen Bildschirm.",
    "Place the lamps where they stand relative to the screen." : "Platziere die Lampen so, wie sie zum Bildschirm stehen.",
    "This page checks again every few seconds." : "Diese Seite prüft alle paar Sekunden erneut.",
    "Check again" : "Erneut prüfen",
    "Ready" : "Fertig",
    "Next, the desktop asks which screens Lightspill may capture. Tick every monitor a lamp should follow, and allow it to be remembered, so you aren't asked again on every start." : "Als Nächstes fragt der Desktop, welche Bildschirme Lightspill erfassen darf. Hake alle Monitore an, denen Lampen folgen sollen, und erlaube, dass die Auswahl gespeichert wird, damit die Frage nicht bei jedem Start kommt.",
    "Then, in the light manager:" : "Danach in der Lichtsteuerung:",
    "Drag your lamps from Available to Active." : "Ziehe deine Lampen von „Verfügbar“ nach „Aktiv“.",
    "Select a lamp and adjust the part of the screen it follows." : "Wähle eine Lampe und passe den Bildschirmbereich an, dem sie folgt.",
    "Save the profile." : "Speichere das Profil.",
    "Configuration file:" : "Konfigurationsdatei:",
    "Open the light manager" : "Zur Lichtsteuerung",
    "Starting the light manager…" : "Lichtsteuerung wird gestartet …",
    "Setup cancelled" : "Einrichtung abgebrochen",
    "Start the application again to continue the setup." : "Starte die Anwendung erneut, um die Einrichtung fortzusetzen."
  }
};

const t = I18n.t;
