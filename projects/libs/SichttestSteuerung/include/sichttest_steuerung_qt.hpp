// sichttest_steuerung_qt.hpp — Zusatz für Qt-Anwendungen (Konzept §3, §8).
//
// Stellt die Rückrufe im GUI-Thread zu und nimmt QString und QJsonObject.
// Wird in der Anwendung übersetzt, also mit deren Qt und deren Compiler;
// braucht nur QtCore und kein moc.
//
//     if (QCoreApplication::arguments().contains("--testing")) {
//         m_steuerung = sichttest::Steuerung::lade("LumiViz", version, &grund);
//         if (m_steuerung) {
//             m_steuerung->aktion("panel_zeigen", "Holt ein Panel nach vorn", "name=<Titel>",
//                 [this](const QJsonObject& arg) -> sichttest::Ergebnis {
//                     return zeigePanel(arg["name"].toString())
//                          ? sichttest::ok() : sichttest::fehler("kein Panel dieses Namens");
//                 });
//             m_steuerung->verbinde();
//         }
//     }
//
// lade() gehört in den GUI-Thread: dort laufen danach alle Aktionen.

#ifndef SICHTTEST_STEUERUNG_QT_HPP
#define SICHTTEST_STEUERUNG_QT_HPP

#include "sichttest_steuerung.hpp"

#include <QByteArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QObject>
#include <QString>

namespace sichttest
{
    inline Ergebnis ok(const QString& text) { return { STS_OK, text.toStdString() }; }
    inline Ergebnis fehler(const QString& text) { return { STS_FEHLER, text.toStdString() }; }
    inline Ergebnis ungueltig(const QString& text) { return { STS_UNGUELTIG, text.toStdString() }; }

    class Steuerung
    {
    public:
        using Aktion = std::function<Ergebnis(const QJsonObject& argumente)>;

        // Liefert leer, wenn die DLL fehlt oder die Fassung nicht passt; grund nennt
        // dann einen Satz dazu, und die Anwendung läuft ohne Steuerung weiter.
        // dllPfad: voller Pfad; ohne ihn SichttestSteuerung<S>.dll im Ordner der Exe.
        static std::unique_ptr<Steuerung> lade(const QString& anwendung, const QString& version,
                                               QString* grund = nullptr, const QString& dllPfad = {},
                                               const QString& projektDatei = {})
        {
            std::string warum;
            const std::wstring pfad = dllPfad.toStdWString();
            const QByteArray datei = projektDatei.toUtf8();
            std::unique_ptr<Sitzung> sitzung = Sitzung::lade(anwendung.toUtf8().constData(),
                                                             version.toUtf8().constData(), &warum,
                                                             dllPfad.isEmpty() ? nullptr : pfad.c_str(),
                                                             projektDatei.isEmpty() ? nullptr : datei.constData());
            if (!sitzung)
            {
                if (grund) *grund = QString::fromStdString(warum);
                return nullptr;
            }
            std::unique_ptr<Steuerung> s(new Steuerung);
            s->m_sitzung = std::move(sitzung);
            // Der Wecker kommt aus dem Lesethread der DLL; gepumpt wird im Thread von m_kontext.
            Steuerung* self = s.get();
            s->m_sitzung->setzeWecker([self] {
                QMetaObject::invokeMethod(&self->m_kontext, [self] { self->m_sitzung->pumpe(); },
                                          Qt::QueuedConnection);
            });
            return s;
        }

        Steuerung(const Steuerung&) = delete;
        Steuerung& operator=(const Steuerung&) = delete;

        // Vor oder nach verbinde(). false: Name ungültig oder schon vergeben (letzterFehler()).
        bool aktion(const QString& name, const QString& beschreibung, const QString& parameter, Aktion a,
                    uint32_t schalter = 0)
        {
            return m_sitzung->aktion(
                name.toUtf8().constData(), beschreibung.toUtf8().constData(), parameter.toUtf8().constData(),
                [a = std::move(a)](const char* argumente_json) -> Ergebnis {
                    const QJsonDocument d = QJsonDocument::fromJson(QByteArray(argumente_json));
                    if (!d.isObject()) return ungueltig("Die Argumente sind kein JSON-Objekt.");
                    return a(d.object());
                },
                schalter);
        }

        // Ab hier kann die Anwendung gesteuert werden. Kehrt sofort zurück.
        bool verbinde() { return m_sitzung->verbinde(); }
        bool melde(const QString& text) { return m_sitzung->melde(text.toUtf8().constData()); }
        // STS_GETRENNT, STS_VERBINDET, STS_VERBUNDEN oder STS_ABGELEHNT
        int zustand() const { return m_sitzung->zustand(); }
        // Grund des letzten Fehlschlags; bleibt stehen, bis ein neuer ihn ersetzt (ein Erfolg leert nicht).
        QString letzterFehler() const { return QString::fromStdString(m_sitzung->letzterFehler()); }
        // Ab S 1.1. Einen benannten Zustand melden, den die Anwendung herstellt und wieder
        // abräumt (etwa eine Test-Datenbank). Jederzeit, auch vor verbinde().
        bool meldeZustand(const QString& name, bool steht, const QString& text = {})
        {
            return m_sitzung->meldeZustand(name.toUtf8().constData(), steht, text.toUtf8().constData());
        }
        // Ab S 1.1. Läuft im GUI-Thread, wenn eine bestehende Verbindung zum Tester endet;
        // geordnet: der Tester wurde geschlossen (sonst Abriss).
        void beiEnde(std::function<void(bool geordnet)> ende) { m_sitzung->beiEnde(std::move(ende)); }
        // Fassung der geladenen DLL, etwa für das Log der Anwendung.
        Fassung fassung() const { return m_sitzung->fassung(); }

    private:
        Steuerung() = default;

        // Reihenfolge zählt: die Sitzung endet zuerst (kein Wecker mehr), dann der Kontext.
        QObject m_kontext;
        std::unique_ptr<Sitzung> m_sitzung;
    };
}

#endif // SICHTTEST_STEUERUNG_QT_HPP
