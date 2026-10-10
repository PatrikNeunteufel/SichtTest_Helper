// Sichttest — Protokoll und Testlog (S93).
//
// Ein Protokoll ist eine Liste von Schritten. Es kommt aus einer Markdown-
// Checkliste (`- [ ] **A1 Titel:** Text`, Abschnitte als `## …`) oder aus einer
// `*.testprotokoll.json` (Format des UART-Testers: title, description, steps
// mit id / section / title / text).
//
// Ein Punkt kann Aktionen tragen, die das Werkzeug in der geprüften Anwendung
// auslöst (docs/Konzept_Steuerung.md §6): in Markdown als Code-Stück
// `aktion: name schlüssel=wert`, in JSON als "aktionen". Ein Punkt, dessen
// Titel mit »Vorbereiten« beginnt (JSON: "kind": "prep"), ist eine
// Vorbereitung: er bekommt kein Urteil und zählt nicht mit.
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
        QJsonArray schritte;   // id, section, title, text, result, remark, screenshots;
                               // dazu kind ("prep" = Vorbereitung, ohne Urteil), aktionen[]
                               // (aus der Liste) und actions[] (was im Lauf ausgelöst wurde)
        QString    anwendung;  // Name, unter dem sich die Anwendung meldet; leer = die einzige verbundene
        QJsonArray nachbereitung; // Aktionen für das Ende des Laufs (nur JSON)
        QJsonArray nachbereitungLog; // was davon im Lauf ausgelöst wurde (Testlog: teardown_actions)
        QJsonObject steuerung; // mit welcher Anwendung und DLL der Lauf lief (Testlog: steuerung, Konzept §5)
        QJsonObject build;     // gegen welche Exe der Lauf lief: exe, exe_timestamp (ISO) (Testlog: build)
        QJsonObject zustaende; // was die Anwendung zuletzt gemeldet hat: name -> { steht, text } (Testlog: zustaende)
        QJsonObject setup;     // Felder des Comm Studio, wie sie in der Liste stehen; gehen
        QJsonObject testDb;    // unverändert ins Testlog (test_db.active bleibt aus altem Lauf)
        QStringList hinweise;  // beim Laden bemerkt: Verweis auf unbekannte Kennung, Verweis im Kreis
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

    // --- Aktionen (Konzept §6) ----------------------------------------------
    // Eine Aktion ist ein Objekt { "aktion": Name, "mit": { Schlüssel: Wert }, "text"?, "frist"? }.
    // Ein Name mit @ davor verweist auf die Aktionen des Punkts mit dieser Kennung.

    // Inhalt eines Code-Stücks `aktion: <name> [wert] [schlüssel=wert …]` lesen (ohne
    // das führende "aktion:"). Werte mit Leerzeichen in "…"; Werte ohne Schlüssel
    // kommen als Argument "wert" an.
    QJsonObject leseAktion(const QString& stueck);

    bool istVorbereitung(const QJsonObject& schritt);
    // Ein Urteil aus einem fortgesetzten Lauf, dessen Schritt die Liste nicht mehr kennt. Es
    // bleibt im Testlog, zählt aber nicht und lässt sich nicht neu bewerten.
    bool istVerwaist(const QJsonObject& schritt);
    // Aktionen eines Punkts, Verweise aufgelöst. Was sich nicht auflösen lässt, steht
    // mit "unbekannt": true in der Reihe und wird wie eine unbekannte Aktion behandelt.
    QJsonArray aktionenVon(const Protokoll& protokoll, int idx, QStringList* hinweise = nullptr);
    // Vorbereitung, die zu einem Punkt gehört: die seines Abschnitts, sonst die der
    // ganzen Liste (vor dem ersten Abschnitt). -1 = keine.
    int vorbereitungVon(const Protokoll& protokoll, int idx);

    // Datei nach Endung laden; leere Schrittliste = nichts Brauchbares gefunden.
    Protokoll lade(const QString& pfad, QString* fehler = nullptr);

    // Ergebnis, Bemerkung und Bilder eines früheren Laufs je Schritt-id übernehmen.
    void uebernimmLauf(Protokoll& protokoll, const QJsonObject& log);

    Zaehler zaehle(const QJsonArray& schritte);
    QString zeichen(const QString& ergebnis);

    QJsonObject alsLog(const Protokoll& protokoll, const QString& gestartet);
    QString alsReport(const Protokoll& protokoll, const QString& gestartet,
                      const QString& exeZeit);

    // Ablage der Läufe eines Protokolls: was das Projekt für seinen Ordner nennt
    // (Projekt.hpp), sonst <Ordner des Protokolls>/sichttest-logs.
    QString logOrdner(const QString& protokollPfad);
    QString logStamm(const QString& protokollPfad);
    QString neuesterLauf(const QString& protokollPfad);
    // Ein Lauf ohne offene Schritte (summary.open des Testlogs) wird nicht fortgesetzt.
    bool istAbgeschlossen(const QJsonObject& log);

    // Protokolle eines Ordners: *.testprotokoll.json und jede *.md mit Schritten;
    // mit Muster nur die Dateien, die darauf passen.
    QStringList findeProtokolle(const QString& ordner, const QString& muster = {});
}
