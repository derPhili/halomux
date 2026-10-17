#pragma once

#include <QHash>
#include <QLocale>
#include <QString>


namespace Halomux::Gui::Texts
{
  /**
   * @brief Language the texts are shown in
   *
   * @return QString& "en" or "de"
   */
  inline QString& language()
  {
    static QString s_language = "en";
    return s_language;
  }


  /**
   * @brief Sets the language from the setting shared with the web UI
   *
   * @param setting "auto" follows the system language, English if it's neither German nor English
   * @return true The language changed
   */
  inline bool setLanguage(const QString& setting)
  {
    QString resolved = setting;
    if(resolved != "en" && resolved != "de"){
      const QStringList uiLanguages = QLocale::system().uiLanguages();
      resolved = !uiLanguages.isEmpty() && uiLanguages.first().startsWith("de") ? "de" : "en";
    }

    const bool changed = resolved != language();
    language() = resolved;
    return changed;
  }


  /**
   * @brief Translates an English text into the current language
   *
   * @param english Text in English
   * @return QString Translated text, or the English one if there is no translation
   */
  inline QString tr(const QString& english)
  {
    static const QHash<QString, QString> s_german = {
      {"Open", "Öffnen"},
      {"Sync with screen and sound", "Sync mit Bildschirm und Sound"},
      {"Static scene", "Statische Szene"},
      {"My scenes", "Meine Szenen"},
      {"Preview settings", "Vorschau"},
      {"Stop preview", "Vorschau stoppen"},
      {"Start service", "Dienst starten"},
      {"Close tray icon", "Tray-Symbol schließen"},
      {"Sync on", "Sync an"},
      {"Service not running", "Dienst läuft nicht"},
      {"The service isn't running", "Der Dienst läuft nicht"},
      {"Start it from the tray menu. This window connects as soon as it answers.", "Starte ihn über das Tray-Menü. Dieses Fenster verbindet sich, sobald er antwortet."},
      // Built-in scenes
      {"Warm white", "Warmweiß"},
      {"Daylight", "Tageslicht"},
      {"Sunset", "Sonnenuntergang"},
      {"Ocean", "Ozean"},
      {"Forest", "Wald"},
      {"Neon", "Neon"},
      {"Galaxy", "Galaxie"},
      {"Fire", "Feuer"},
      {"Candlelight", "Kerzenlicht"},
      {"Off", "Aus"}
    };

    if(language() != "de"){
      return english;
    }

    return s_german.value(english, english);
  }
}
