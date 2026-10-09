// Kanal des Testers (Konzept §4): der Tester lauscht auf einer benannten Pipe,
// die DLL in der geprüften Anwendung verbindet sich.
//
// Rahmen: 4 Byte Länge (little endian) + ein JSON-Objekt in UTF-8.

#pragma once

#include <QByteArray>
#include <QHash>
#include <QLocalServer>
#include <QString>

#include <functional>

class QLocalSocket;

namespace sichttest
{
    class KanalServer
    {
    public:
        KanalServer();
        ~KanalServer();
        KanalServer(const KanalServer&) = delete;
        KanalServer& operator=(const KanalServer&) = delete;

        // "sichttest-<Benutzer>" — unter Windows die Pipe \\.\pipe\sichttest-<Benutzer>.
        // Die DLL bildet denselben Namen (Sitzung.cpp, kanalName).
        static QString kanalName();

        // Je Benutzer lauscht nur ein Tester: das sichert ein benannter Mutex mit
        // dem Namen des Kanals. QLocalServer allein ließe einen zweiten zu
        // (gemessen 2026-10-09).
        bool lausche(const QString& name);
        void schliesse();
        bool lauscht() const { return m_server.isListening(); }
        QString fehler() const { return m_fehler; }

        static void sende(QLocalSocket* s, const QByteArray& json);

        std::function<void(QLocalSocket*)> beiVerbunden;
        std::function<void(QLocalSocket*, const QByteArray&)> beiRahmen;
        std::function<void(QLocalSocket*)> beiGetrennt;

    private:
        void nimmAn();
        void lies(QLocalSocket* s);

        QHash<QLocalSocket*, QByteArray> m_puffer;
        QString m_fehler;
        void* m_mutex = nullptr;   // HANDLE, nur Windows
        QLocalServer m_server;
    };
}
