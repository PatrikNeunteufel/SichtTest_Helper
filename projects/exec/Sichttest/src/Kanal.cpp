#include "Kanal.hpp"

#include <QLocalSocket>
#include <QtEndian>

#ifdef Q_OS_WIN
#  include <qt_windows.h>
#endif

namespace sichttest
{
    namespace
    {
        constexpr quint32 kMaxRahmen = 16u * 1024u * 1024u;
    }

    KanalServer::KanalServer()
    {
        // Nur der angemeldete Benutzer darf sich verbinden.
        m_server.setSocketOptions(QLocalServer::UserAccessOption);
        QObject::connect(&m_server, &QLocalServer::newConnection, &m_server, [this] { nimmAn(); });
    }

    KanalServer::~KanalServer()
    {
        schliesse();
    }

    QString KanalServer::kanalName()
    {
        QString benutzer = qEnvironmentVariable("USERNAME", qEnvironmentVariable("USER"));
        benutzer.replace(QLatin1Char('\\'), QLatin1Char('_'));
        return QStringLiteral("sichttest-") + benutzer;
    }

    bool KanalServer::lausche(const QString& name)
    {
        schliesse();
#ifdef Q_OS_WIN
        // Global\: Pipes gelten für den ganzen Rechner, nicht nur für eine Anmeldesitzung.
        const QString mutexName = QStringLiteral("Global\\") + name;
        m_mutex = CreateMutexW(nullptr, FALSE, reinterpret_cast<const wchar_t*>(mutexName.utf16()));
        if (!m_mutex || GetLastError() == ERROR_ALREADY_EXISTS)
        {
            schliesse();
            m_fehler = QStringLiteral("ein anderer Tester lauscht schon");
            return false;
        }
#endif
        if (!m_server.listen(name))
        {
            const QString grund = m_server.errorString();
            schliesse();
            m_fehler = grund;
            return false;
        }
        m_fehler.clear();
        return true;
    }

    void KanalServer::schliesse()
    {
        m_server.close();
#ifdef Q_OS_WIN
        if (m_mutex) CloseHandle(m_mutex);
#endif
        m_mutex = nullptr;
    }

    void KanalServer::sende(QLocalSocket* s, const QByteArray& json)
    {
        char laenge[4];
        qToLittleEndian<quint32>(static_cast<quint32>(json.size()), laenge);
        s->write(laenge, 4);
        s->write(json);
    }

    void KanalServer::nimmAn()
    {
        while (QLocalSocket* s = m_server.nextPendingConnection())
        {
            m_puffer.insert(s, {});
            QObject::connect(s, &QLocalSocket::readyRead, &m_server, [this, s] { lies(s); });
            QObject::connect(s, &QLocalSocket::disconnected, &m_server, [this, s] {
                if (m_puffer.remove(s) && beiGetrennt) beiGetrennt(s);
                s->deleteLater();
            });
            if (beiVerbunden) beiVerbunden(s);
            lies(s);
        }
    }

    void KanalServer::lies(QLocalSocket* s)
    {
        if (!m_puffer.contains(s)) return;
        m_puffer[s] += s->readAll();
        // Ein Rückruf darf die Verbindung beenden — dann ist der Puffer weg.
        while (m_puffer.contains(s))
        {
            QByteArray& p = m_puffer[s];
            if (p.size() < 4) return;
            const quint32 n = qFromLittleEndian<quint32>(p.constData());
            if (n > kMaxRahmen)
            {
                s->abort();
                return;
            }
            if (p.size() < 4 + qsizetype(n)) return;
            const QByteArray json = p.mid(4, n);
            p.remove(0, 4 + qsizetype(n));
            if (beiRahmen) beiRahmen(s, json);
        }
    }
}
