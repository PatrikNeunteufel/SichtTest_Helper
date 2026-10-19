// Ergebnis-DB (Spezifikation §6.6): je Ablage eine kleine SQLite-Datei
// `testergebnisse.sqlite`, in die jeder Lauf bei jedem Schreiben des Testlogs
// gespiegelt wird. Das Testlog bleibt die Wahrheit; die DB ist der Index über
// alle Läufe und Builds — für die Statistik und den Nachtest-Indikator.
//
// Tabellen testlauf, testschritt, protokoll; Spalten werden nur ergänzt. Zwei
// Dinge gegenüber dem ursprünglichen Schema: `log_datei` trägt den Dateinamen ohne Ordner, und
// `testschritt` hat ergänzte Spalten für den Build am Urteil und seinen Zeitpunkt.

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

    // Testlogs der Ablage, die die DB noch nicht kennt (von einer älteren Version geschrieben),
    // nachträglich spiegeln. Die Testlogs selbst bleiben unberührt. Rückgabe: wie viele neu
    // dazukamen; *uebergangen zählt Dateien, die sich nicht als Testlog lesen ließen.
    int leseNach(const QString& ablage, int* uebergangen = nullptr, QString* fehler = nullptr);

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
        QString bewertet;       // Zeitpunkt des Urteils (rated_at); leer bei Urteilen ohne ihn
        // Wann getestet wurde: der Zeitpunkt des Urteils, sonst der Start des Laufs.
        const QString& wann() const { return bewertet.isEmpty() ? gestartet : bewertet; }
    };
    // Das letzte verifizierte Ergebnis je Schritt-Kennung, über alle Listen der Ablage —
    // das jüngste nach dem Zeitpunkt des Urteils, nicht nach dem Start seines Laufs.
    QHash<QString, LetztesErgebnis> letzteErgebnisse(const QString& ablage);

    // Die Verbindung schließen (Selbsttest: die Datei wird sonst festgehalten).
    void schliesse();
}
