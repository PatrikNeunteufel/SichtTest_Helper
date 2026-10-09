// Steuerung — die Seite des Testers im Protokoll P (Konzept §4, §5).
//
// Nimmt Anwendungen an, die sich über die DLL melden (hallo → willkommen oder
// abgelehnt), kennt deren Aktionen und schickt Aufrufe. Je Verbindung läuft
// ein Aufruf zur Zeit; weitere warten in der Reihe.

#pragma once

#include "Kanal.hpp"

#include <QJsonArray>
#include <QJsonObject>
#include <QList>
#include <QObject>
#include <QString>

#include <functional>

class QProcess;
class QTextStream;
class QTimer;

namespace sichttest
{
    class Steuerung
    {
    public:
        struct Anwendung
        {
            QString name;          // sts_konfig.anwendung — der Name, den Listen nennen
            QString version;
            QString produkt;       // Version der DLL (Tag des Repos)
            QString s;             // Fassung der Schnittstelle S, "1.0"
            QString exe;
            QString projektDatei;
            qint64 pid = 0;
            int p = 0;             // gewählte große Fassung von P
            QJsonArray aktionen;   // je Aktion: name, beschreibung, parameter, schalter[]
        };

        // status: ok, fehler, unbekannt, ungueltig (von der Anwendung) —
        //         frist, getrennt (vom Tester)
        using Fertig = std::function<void(const QString& status, const QString& text)>;

        Steuerung();
        ~Steuerung();

        bool lausche(const QString& kanal);
        bool lauscht() const { return m_kanal.lauscht(); }
        QString fehler() const { return m_kanal.fehler(); }

        // Große Fassungen von P, die der Tester spricht. Nur der Selbsttest ändert sie.
        void setzeFassungen(int pMin, int pMax);

        const Anwendung* anwendung(const QString& name) const;
        bool kennt(const QString& anwendung, const QString& aktion) const;

        // fristMs <= 0: keine Frist (Aktionen mit wartet_auf_mensch).
        void rufe(const QString& anwendung, const QString& aktion, const QJsonObject& argumente,
                  const QString& basis, int fristMs, Fertig fertig);

        // Eine Zeile für die Statuszeile des Fensters.
        QString zustandsText() const;
        QString letzteAblehnung() const { return m_ablehnung; }

        std::function<void()> beiAenderung;
        std::function<void(const QString& anwendung, const QString& text)> beiMeldung;

    private:
        struct Auftrag
        {
            qint64 id = 0;
            QByteArray nachricht;
            int fristMs = 0;
            Fertig fertig;
        };
        struct Verbindung
        {
            Anwendung app;
            bool begruesst = false;
            bool laeuft = false;       // der vorderste Auftrag ist geschickt
            QList<Auftrag> reihe;
            QTimer* frist = nullptr;
        };

        void nimm(QLocalSocket* s, const QByteArray& json);
        void begruesse(QLocalSocket* s, const QJsonObject& hallo);
        void trenne(QLocalSocket* s);
        void starteNaechsten(QLocalSocket* s);
        void beende(QLocalSocket* s, const QString& status, const QString& text);
        QLocalSocket* findeVerbindung(const QString& anwendung) const;
        void geaendert() const { if (beiAenderung) beiAenderung(); }

        QObject m_kontext;
        QHash<QLocalSocket*, Verbindung> m_verbindungen;
        QList<QLocalSocket*> m_reihenfolge;   // älteste zuerst
        QString m_ablehnung;
        qint64 m_naechsteId = 1;
        int m_pMin;
        int m_pMax;
        KanalServer m_kanal;
    };

    // Teil von --selbsttest: startet die Gegenprobe (ohne Qt), die die DLL lädt
    // und sich als Anwendung meldet. Gibt die Zahl der gescheiterten Prüfungen zurück.
    int selbsttestSteuerung(QTextStream& aus);

    // Für den Selbsttest: die Gegenprobe aus dem Build-Baum starten, verbunden mit
    // dem genannten Kanal. false = sie ist nicht gebaut.
    bool starteGegenprobe(QProcess& p, const QString& szenario, const QString& kanal);
}
