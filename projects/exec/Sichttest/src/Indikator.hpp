// Nachtest-Indikator (Konzept §14.1 Teil C): wie dringend ein Schritt wieder zu
// prüfen ist. Aus drei Quellen — dem letzten verifizierten Ergebnis je Schritt
// (Ergebnis-DB der Ablage), der Hand-Schicht der Gewichtung (weight, changed,
// retest_steps) und der git-Schicht (jüngster Commit je Area).
//
// Die Rechnung ist die des Comm Studio (TestProtokollWindow.cpp:1020–1119).

#pragma once

#include "Ergebnis.hpp"
#include "Projekt.hpp"

#include <QHash>
#include <QJsonObject>
#include <QString>

namespace sichttest::indikator
{
    struct Gewichtung
    {
        bool aktiv = false;   // die Projektdatei nennt `gewichtung` (auch leer): es gibt einen Indikator
        QJsonObject hand;     // verschmolzen aus gewichtung.hand[]: areas, retest_steps, auto_weight
        QJsonObject git;      // verschmolzen aus gewichtung.git[]: areas
    };

    // Dateien der Projektdatei lesen und je Schicht verschmelzen: `areas` und
    // `retest_steps` je Schlüssel, alles andere überschrieben; die Reihenfolge der
    // Liste gilt. Eine Datei, die fehlt, wird übergangen.
    Gewichtung lade(const Projekt& projekt);

    constexpr double kRot = 0.7;    // ab hier dringend
    constexpr double kGelb = 0.3;   // ab hier empfohlen; darunter unkritisch

    struct Dringlichkeit
    {
        double u = -1.0;   // -1: kein Indikator für diesen Schritt
        QString warum;
    };

    Dringlichkeit bewerte(const QJsonObject& schritt, const Gewichtung& gewichtung,
                          const QHash<QString, ergebnis::LetztesErgebnis>& letzte);

    // "🔴", "🟡" oder leer.
    QString marke(double u);
}
