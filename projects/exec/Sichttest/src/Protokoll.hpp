// Sichttest — Protokoll und Testlog (S93).
//
// Ein Protokoll ist eine Liste von Schritten. Es kommt aus einer Markdown-
// Checkliste (`- [ ] **A1 Titel:** Text`, Abschnitte als `## …`) oder aus einer
// `*.testprotokoll.json` (Format des UART-Testers: title, description, steps
// mit id / section / title / text).
//
// Das Testlog (`*.testlog.json`) hat das Format des UART-Testers und wird nach
// jeder Bewertung geschrieben; daneben liegt ein Report in Markdown, den ein
// Mensch und Claude lesen.
#pragma once

#include <QJsonArray>
#include <QJsonObject>
#include <QString>
#include <QStringList>

namespace sichttest
{
    struct Protokoll
    {
        QString    pfad;
        QString    titel;
        QString    beschreibung;
        QString    exe;        // aus der Beschreibung gelesen, kann leer sein
        QJsonArray schritte;   // id, section, title, text, result, remark, screenshots
    };

    struct Zaehler
    {
        int pass = 0;
        int passBefund = 0;
        int fail = 0;
        int skip = 0;
        int offen = 0;
    };

    // Markdown-Checkliste lesen. Haken der Datei werden übernommen:
    // [x] = pass, [!] = fail, [-] = skip.
    Protokoll ausMarkdown(const QString& text, const QString& pfad);
    Protokoll ausJson(const QJsonObject& wurzel, const QString& pfad);

    // Datei nach Endung laden; leere Schrittliste = nichts Brauchbares gefunden.
    Protokoll lade(const QString& pfad, QString* fehler = nullptr);

    // Ergebnis, Bemerkung und Bilder eines früheren Laufs je Schritt-id übernehmen.
    void uebernimmLauf(Protokoll& protokoll, const QJsonObject& log);

    Zaehler zaehle(const QJsonArray& schritte);
    QString zeichen(const QString& ergebnis);

    QJsonObject alsLog(const Protokoll& protokoll, const QString& gestartet,
                       const QString& exeZeit);
    QString alsReport(const Protokoll& protokoll, const QString& gestartet,
                      const QString& exeZeit);

    // Ablage der Läufe eines Protokolls: <Ordner des Protokolls>/sichttest-logs.
    QString logOrdner(const QString& protokollPfad);
    QString logStamm(const QString& protokollPfad);
    QString neuesterLauf(const QString& protokollPfad);

    // Protokolle eines Ordners: *.testprotokoll.json und jede *.md mit Schritten.
    QStringList findeProtokolle(const QString& ordner);
}
