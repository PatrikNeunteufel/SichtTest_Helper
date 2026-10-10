// Ergebnis-DB (Konzept §14.1 Teil B): je Ablage eine kleine SQLite-Datei
// `testergebnisse.sqlite`, in die jeder Lauf bei jedem Schreiben des Testlogs
// gespiegelt wird. Das Testlog bleibt die Wahrheit; die DB ist der Index über
// alle Läufe und Builds — für die Statistik und den Nachtest-Indikator.
//
// Das Schema ist das des Comm Studio (testlauf, testschritt, protokoll). Zwei
// Dinge sind anders: `log_datei` trägt den Dateinamen ohne Ordner, und
// `testschritt` hat zwei ergänzte Spalten für den Build am Urteil.

#pragma once

#include <QHash>
#include <QJsonObject>
#include <QList>
#include <QString>

namespace sichttest::ergebnis
{
    QString dbPfad(const QString& ablage);

    // Lauf und Schritte aus dem Testlog spiegeln (Upsert über den Dateinamen des
    // Testlogs, Schritte ersetzt). Vorbereitungen werden nicht gespiegelt.
    // Rückgabe leer = gut, sonst der Fehlertext.
    QString spiegele(const QString& logPfad, const QJsonObject& log);

    struct LaufZeile
    {
        QString gestartet, beendet, buildStempel, logDatei;
        int pass = 0, passBefund = 0, fail = 0, skip = 0, offen = 0;
    };
    // Alle Läufe einer Liste (nach ihrem Titel), neueste zuerst.
    QList<LaufZeile> laeufe(const QString& ablage, const QString& protokoll);

    struct SchrittSumme
    {
        QString id;
        int paesse = 0;   // pass und pass_remark
        int fails = 0;
    };
    // Je Schritt-Kennung über alle Läufe und Listen der Ablage.
    QList<SchrittSumme> schrittSummen(const QString& ablage);

    struct LetztesErgebnis
    {
        QString ergebnis;       // pass, pass_remark oder fail — skip ist nicht verifiziert
        QString gestartet;      // Start des Laufs, der es geliefert hat
        QString buildStempel;   // Build am Urteil, sonst der des Laufs
    };
    // Das letzte verifizierte Ergebnis je Schritt-Kennung, über alle Listen der Ablage.
    QHash<QString, LetztesErgebnis> letzteErgebnisse(const QString& ablage);

    // Die Verbindung schließen (Selbsttest: die Datei wird sonst festgehalten).
    void schliesse();
}
