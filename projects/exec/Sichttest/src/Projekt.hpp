// Sichttest — die Projektdatei `sichttest.projekt.json` (docs/Konzept_Steuerung.md §7).
//
// Eine Datei je Anwendung, im Repo der Anwendung. Sie trägt, was der Tester über
// das Projekt wissen muss und was sich ändern kann, ohne dass jemand übersetzt:
// wo die Listen liegen, wohin die Läufe gehen, wie die Anwendung gestartet wird
// und welche Aktion ein Feld des JSON-Formats auslöst. Pfade sind relativ zur Datei.
//
// Ohne Projektdatei verhält sich das Werkzeug wie bisher.
#pragma once

#include <QJsonObject>
#include <QList>
#include <QString>
#include <QStringList>

namespace sichttest
{
    struct Projekt
    {
        struct Liste
        {
            QString pfad;      // Ordner mit Listen oder eine einzelne Liste, absolut
            QString ablage;    // absolut; leer = <Ordner der Liste>/sichttest-logs
            QString muster;    // Dateimuster; leer = jede *.md mit Schritten und *.testprotokoll.json
        };

        QString      pfad;             // die Projektdatei; leer = kein Projekt
        QString      ordner;           // ihr Ordner
        int          schema = 1;
        QString      anwendung;        // Name, unter dem sich die Anwendung meldet
        QString      startExe;         // absolut; leer = der Mensch startet sie
        QStringList  startArgumente;
        QList<Liste> listen;
        QJsonObject  gewichtung;       // gelesen, noch ohne Wirkung (§14 Stufe 4)
        QJsonObject  abbildung;        // Feld des JSON-Formats → "aktion schlüssel …"

        bool gueltig() const { return !pfad.isEmpty(); }
    };

    Projekt ladeProjekt(const QString& pfad, QString* fehler = nullptr);

    // Von einem Ordner aufwärts die erste `sichttest.projekt.json`; leer = keine.
    QString findeProjektDatei(const QString& ordner);

    // Das Projekt, mit dem Listen gerade gelesen und Läufe abgelegt werden.
    const Projekt& projekt();
    void setzeProjekt(const Projekt& projekt);

    // Alle Listen des Projekts. Ein genannter Ordner, den es nicht gibt, wird übergangen.
    QStringList protokolleDesProjekts(const Projekt& projekt);
    // Ablage einer Liste laut Projekt; leer = das Projekt nennt keine.
    QString ablageVon(const Projekt& projekt, const QString& protokollPfad);

    // Abbildung eines Felds ("tab", "setup.open", …) auf eine Aktion: aus
    // "tab_zeigen titel" wird aktion = tab_zeigen, schluessel = { titel }.
    bool abbildung(const Projekt& projekt, const QString& feld, QString& aktion, QStringList& schluessel);
}
