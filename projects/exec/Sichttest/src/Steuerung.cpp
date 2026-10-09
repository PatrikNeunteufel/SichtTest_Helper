#include "Steuerung.hpp"

#include <sichttest_steuerung.h>

#include <QCoreApplication>
#include <QDir>
#include <QElapsedTimer>
#include <QFile>
#include <QFileInfo>
#include <QJsonDocument>
#include <QLocalSocket>
#include <QProcess>
#include <QTextStream>
#include <QThread>
#include <QTimer>

#include <memory>

namespace sichttest
{
    namespace
    {
        QByteArray alsRahmen(const QJsonObject& o)
        {
            return QJsonDocument(o).toJson(QJsonDocument::Compact);
        }

        QJsonObject nachricht(const char* name)
        {
            return QJsonObject{ { QStringLiteral("nachricht"), QLatin1String(name) } };
        }
    }

    // Der Tester spricht die aktuelle große Fassung von P und die vorige (§5).
    Steuerung::Steuerung()
        : m_pMin(STS_P_MAJOR > 1 ? STS_P_MAJOR - 1 : 1)
        , m_pMax(STS_P_MAJOR)
    {
        m_kanal.beiVerbunden = [this](QLocalSocket* s) {
            m_verbindungen.insert(s, {});
            m_reihenfolge.append(s);
        };
        m_kanal.beiRahmen = [this](QLocalSocket* s, const QByteArray& json) { nimm(s, json); };
        m_kanal.beiGetrennt = [this](QLocalSocket* s) { trenne(s); };
    }

    Steuerung::~Steuerung()
    {
        m_kanal.beiVerbunden = nullptr;
        m_kanal.beiRahmen = nullptr;
        m_kanal.beiGetrennt = nullptr;
        for (QLocalSocket* s : std::as_const(m_reihenfolge))
        {
            KanalServer::sende(s, alsRahmen(nachricht("tschuess")));
            s->flush();
        }
    }

    bool Steuerung::lausche(const QString& kanal)
    {
        const bool gut = m_kanal.lausche(kanal);
        geaendert();
        return gut;
    }

    void Steuerung::setzeFassungen(int pMin, int pMax)
    {
        m_pMin = pMin;
        m_pMax = pMax;
    }

    QLocalSocket* Steuerung::findeVerbindung(const QString& anwendung) const
    {
        // Die jüngste zählt: nach einem Neustart kann die alte Verbindung noch kurz bestehen.
        for (auto it = m_reihenfolge.crbegin(); it != m_reihenfolge.crend(); ++it)
        {
            const Verbindung& v = m_verbindungen[*it];
            if (v.begruesst && (anwendung.isEmpty() || v.app.name == anwendung)) return *it;
        }
        return nullptr;
    }

    const Steuerung::Anwendung* Steuerung::anwendung(const QString& name) const
    {
        QLocalSocket* s = findeVerbindung(name);
        if (!s) return nullptr;
        return &m_verbindungen.find(s)->app;
    }

    QList<Steuerung::Anwendung> Steuerung::anwendungen() const
    {
        QList<Anwendung> alle;
        for (QLocalSocket* s : m_reihenfolge)
        {
            const Verbindung v = m_verbindungen.value(s);
            if (v.begruesst) alle.append(v.app);
        }
        return alle;
    }

    bool Steuerung::kennt(const QString& anwendungName, const QString& aktion) const
    {
        const Anwendung* a = anwendung(anwendungName);
        if (!a) return false;
        for (const QJsonValue& v : a->aktionen)
            if (v.toObject().value(QStringLiteral("name")).toString() == aktion) return true;
        return false;
    }

    void Steuerung::nimm(QLocalSocket* s, const QByteArray& json)
    {
        const QJsonObject o = QJsonDocument::fromJson(json).object();
        const QString art = o.value(QStringLiteral("nachricht")).toString();
        Verbindung& v = m_verbindungen[s];

        if (art == QLatin1String("hallo"))
        {
            if (!v.begruesst) begruesse(s, o);
            return;
        }
        if (!v.begruesst) return;

        if (art == QLatin1String("aktionen"))
        {
            // Nachgemeldetes ersetzt Gleichnamiges, sonst kommt es dazu.
            const QJsonArray neu = o.value(QStringLiteral("aktionen")).toArray();
            for (const QJsonValue& n : neu)
            {
                const QString name = n.toObject().value(QStringLiteral("name")).toString();
                bool ersetzt = false;
                for (qsizetype i = 0; i < v.app.aktionen.size() && !ersetzt; ++i)
                    if (v.app.aktionen.at(i).toObject().value(QStringLiteral("name")).toString() == name)
                    {
                        v.app.aktionen.replace(i, n);
                        ersetzt = true;
                    }
                if (!ersetzt) v.app.aktionen.append(n);
            }
            geaendert();
        }
        else if (art == QLatin1String("antwort"))
        {
            // Eine verspätete Antwort (nach der Frist) trägt eine alte Kennung und wird verworfen.
            if (v.laeuft && !v.reihe.isEmpty()
                && o.value(QStringLiteral("id")).toInteger(-1) == v.reihe.first().id)
                beende(s, o.value(QStringLiteral("status")).toString(), o.value(QStringLiteral("text")).toString());
        }
        else if (art == QLatin1String("meldung"))
        {
            if (beiMeldung) beiMeldung(v.app.name, o.value(QStringLiteral("text")).toString());
        }
        else if (art == QLatin1String("tschuess"))
        {
            // Das Ende der Verbindung folgt von selbst; gemerkt wird nur, dass es geordnet war.
            v.verabschiedet = true;
        }
        // Unbekanntes wird überlesen (§5).
    }

    void Steuerung::begruesse(QLocalSocket* s, const QJsonObject& hallo)
    {
        Verbindung& v = m_verbindungen[s];
        const QString name = hallo.value(QStringLiteral("anwendung")).toString();
        const QString version = hallo.value(QStringLiteral("version")).toString();
        const int appMin = hallo.value(QStringLiteral("p_min")).toInt();
        const int appMax = hallo.value(QStringLiteral("p_max")).toInt();
        const int p = qMin(appMax, m_pMax);
        if (p < qMax(appMin, m_pMin))
        {
            const QString grund = appMin > m_pMax
                ? QStringLiteral("Die Anwendung spricht Protokoll P %1, der Tester höchstens P %2 — bitte den "
                                 "Tester erneuern.").arg(appMin).arg(m_pMax)
                : QStringLiteral("Die Anwendung spricht höchstens Protokoll P %1, der Tester mindestens P %2 — "
                                 "bitte den Pin der Anwendung (sichttest.pin) heben.").arg(appMax).arg(m_pMin);
            m_ablehnung = QStringLiteral("%1 %2: %3").arg(name, version, grund);
            QJsonObject nein = nachricht("abgelehnt");
            nein.insert(QStringLiteral("grund"), grund);
            KanalServer::sende(s, alsRahmen(nein));
            geaendert();
            return;
        }

        v.app.name = name;
        v.app.version = version;
        v.app.produkt = hallo.value(QStringLiteral("produkt")).toString();
        v.app.s = hallo.value(QStringLiteral("s")).toString();
        v.app.exe = hallo.value(QStringLiteral("exe")).toString();
        v.app.projektDatei = hallo.value(QStringLiteral("projekt_datei")).toString();
        v.app.pid = hallo.value(QStringLiteral("pid")).toInteger();
        v.app.p = p;
        v.app.aktionen = hallo.value(QStringLiteral("aktionen")).toArray();
        v.begruesst = true;
        m_ablehnung.clear();

        QJsonObject ja = nachricht("willkommen");
        ja.insert(QStringLiteral("p"), p);
        ja.insert(QStringLiteral("tester"), QStringLiteral(STS_PRODUKT));
        KanalServer::sende(s, alsRahmen(ja));
        geaendert();
    }

    void Steuerung::trenne(QLocalSocket* s)
    {
        Verbindung v = m_verbindungen.take(s);
        m_reihenfolge.removeAll(s);
        delete v.frist;
        for (const Auftrag& a : std::as_const(v.reihe))
            if (a.fertig) a.fertig(QStringLiteral("getrennt"), QStringLiteral("Verbindung zur Anwendung beendet"));
        if (v.begruesst && beiEnde) beiEnde(v.app.name, v.verabschiedet);
        geaendert();
    }

    void Steuerung::rufe(const QString& anwendungName, const QString& aktion, const QJsonObject& argumente,
                         const QString& basis, int fristMs, Fertig fertig)
    {
        QLocalSocket* s = findeVerbindung(anwendungName);
        if (!s)
        {
            if (fertig) fertig(QStringLiteral("getrennt"), QStringLiteral("keine Anwendung verbunden"));
            return;
        }
        Auftrag a;
        a.id = m_naechsteId++;
        a.fristMs = fristMs;
        a.fertig = std::move(fertig);
        QJsonObject o = nachricht("aufruf");
        o.insert(QStringLiteral("id"), a.id);
        o.insert(QStringLiteral("aktion"), aktion);
        o.insert(QStringLiteral("argumente"), argumente);
        o.insert(QStringLiteral("basis"), basis);
        a.nachricht = alsRahmen(o);
        m_verbindungen[s].reihe.append(std::move(a));
        starteNaechsten(s);
    }

    void Steuerung::starteNaechsten(QLocalSocket* s)
    {
        const auto it = m_verbindungen.find(s);
        if (it == m_verbindungen.end() || it->laeuft || it->reihe.isEmpty()) return;
        it->laeuft = true;
        KanalServer::sende(s, it->reihe.first().nachricht);
        const int frist = it->reihe.first().fristMs;
        if (frist <= 0) return;
        if (!it->frist)
        {
            it->frist = new QTimer(&m_kontext);
            it->frist->setSingleShot(true);
            QObject::connect(it->frist, &QTimer::timeout, &m_kontext, [this, s] {
                const auto v = m_verbindungen.find(s);
                if (v == m_verbindungen.end() || !v->laeuft || v->reihe.isEmpty()) return;
                const int ms = v->reihe.first().fristMs;
                beende(s, QStringLiteral("frist"),
                       ms % 1000 == 0 ? QStringLiteral("keine Antwort nach %1 s").arg(ms / 1000)
                                      : QStringLiteral("keine Antwort nach %1 ms").arg(ms));
            });
        }
        it->frist->start(frist);
    }

    void Steuerung::beende(QLocalSocket* s, const QString& status, const QString& text)
    {
        const auto it = m_verbindungen.find(s);
        if (it == m_verbindungen.end() || it->reihe.isEmpty()) return;
        const Auftrag a = it->reihe.takeFirst();
        it->laeuft = false;
        if (it->frist) it->frist->stop();
        // Der Rückruf darf neue Aufrufe stellen oder die Verbindung verlieren — danach neu nachsehen.
        if (a.fertig) a.fertig(status, text);
        starteNaechsten(s);
    }

    QString Steuerung::zustandsText() const
    {
        if (!m_kanal.lauscht()) return QStringLiteral("Steuerung aus: %1").arg(m_kanal.fehler());
        QStringList teile;
        for (QLocalSocket* s : m_reihenfolge)
        {
            const Verbindung& v = m_verbindungen[s];
            if (v.begruesst)
                teile << QStringLiteral("%1 %2 verbunden (%3 Aktionen)")
                             .arg(v.app.name, v.app.version).arg(v.app.aktionen.size());
        }
        if (teile.isEmpty()) teile << QStringLiteral("lauscht, keine Anwendung verbunden");
        if (!m_ablehnung.isEmpty()) teile << QStringLiteral("abgelehnt — %1").arg(m_ablehnung);
        return QStringLiteral("Steuerung: ") + teile.join(QStringLiteral(" · "));
    }

    // ==========================================================================
    // Selbsttest
    // ==========================================================================

    bool gegenprobePfade(QString& exe, QString& dll)
    {
        // Die Teile liegen im Build-Baum nebeneinander: exec/<Target>/bin/<Konfig>/, libs/<Target>/bin/<Konfig>/.
        const QString hier = QCoreApplication::applicationDirPath();
        exe = hier;
        exe.replace(QLatin1String("/exec/Sichttest/"), QLatin1String("/exec/Gegenprobe/"));
        exe += QLatin1String("/Gegenprobe.exe");
        dll = hier;
        dll.replace(QLatin1String("/exec/Sichttest/"), QLatin1String("/libs/SichttestSteuerung/"));
        // Die große Fassung von S steht im Dateinamen (Konzept §5).
        dll += QStringLiteral("/SichttestSteuerung%1.dll").arg(STS_S_MAJOR);
        return QFileInfo::exists(exe) && QFileInfo::exists(dll);
    }

    bool starteGegenprobe(QProcess& p, const QString& szenario, const QString& kanal, const QString& projektDatei)
    {
        QString gegenprobe, dll;
        if (!gegenprobePfade(gegenprobe, dll)) return false;
        QProcessEnvironment env = QProcessEnvironment::systemEnvironment();
        env.insert(QStringLiteral("SICHTTEST_KANAL"), kanal);
        env.insert(QStringLiteral("SICHTTEST_EXE"), QStringLiteral("gibt-es-nicht.exe"));
        if (!projektDatei.isEmpty()) env.insert(QStringLiteral("GEGENPROBE_PROJEKT"), projektDatei);
        p.setProcessEnvironment(env);
        p.setProcessChannelMode(QProcess::MergedChannels);
        p.start(gegenprobe, { dll, szenario });
        return true;
    }

    int selbsttestSteuerung(QTextStream& aus)
    {
        int fehler = 0;
        auto pruefe = [&](bool gut, const QString& was) {
            aus << (gut ? "ok   " : "FAIL ") << was << "\n";
            if (!gut) ++fehler;
        };
        auto warteBis = [](const std::function<bool()>& fertig, int ms) {
            QElapsedTimer uhr;
            uhr.start();
            while (!fertig() && uhr.elapsed() < ms)
            {
                QCoreApplication::processEvents();
                QThread::msleep(5);
            }
            return fertig();
        };

        const QString hier = QCoreApplication::applicationDirPath();
        QString gegenprobe, dll;
        gegenprobePfade(gegenprobe, dll);
        pruefe(QFileInfo::exists(gegenprobe), QStringLiteral("Gegenprobe ist gebaut: %1").arg(gegenprobe));
        pruefe(QFileInfo::exists(dll), QStringLiteral("DLL ist gebaut: %1").arg(dll));
        if (!QFileInfo::exists(gegenprobe) || !QFileInfo::exists(dll)) return fehler;

        // Im Build-Baum: die Produktversion kommt als Define aus Solution.json, und die
        // Schablone der Datei VERSION im Paket nennt S und P wie der Kopf (Konzept §9).
        {
            QDir d(hier);
            for (int n = 0; n < 10 && !d.exists(QStringLiteral("Solution.json")); ++n)
                if (!d.cdUp()) break;
            QFile f(d.filePath(QStringLiteral("Solution.json")));
            if (f.open(QIODevice::ReadOnly))
            {
                const QString version = QJsonDocument::fromJson(f.readAll()).object()
                    .value(QStringLiteral("solution")).toObject().value(QStringLiteral("version")).toString();
                pruefe(version == QLatin1String(STS_PRODUKT),
                       QStringLiteral("STS_PRODUKT (%1) ist die Version aus Solution.json (%2)")
                           .arg(QLatin1String(STS_PRODUKT), version));
            }
            QFile v(d.filePath(QStringLiteral("packaging/VERSION.in")));
            if (v.open(QIODevice::ReadOnly))
            {
                const QStringList zeilen = QString::fromUtf8(v.readAll()).split(QLatin1Char('\n'));
                QStringList sauber;
                for (const QString& z : zeilen) sauber.append(z.trimmed());
                pruefe(sauber.contains(QStringLiteral("s=%1.%2").arg(STS_S_MAJOR).arg(STS_S_MINOR))
                       && sauber.contains(QStringLiteral("p=%1").arg(STS_P_MAJOR))
                       && sauber.contains(QStringLiteral("produkt=@VERSION@")),
                       QStringLiteral("packaging/VERSION.in nennt S %1.%2 und P %3 wie der Kopf")
                           .arg(STS_S_MAJOR).arg(STS_S_MINOR).arg(STS_P_MAJOR));
            }
        }

        // Eigener Name, damit der Test einen laufenden Tester nicht stört.
        const QString kanal = QStringLiteral("sichttest-selbsttest-%1").arg(QCoreApplication::applicationPid());
        auto starteExe = [&](QProcess& p, const QString& exe, const QStringList& argumente, const QString& kanalName) {
            QProcessEnvironment env = QProcessEnvironment::systemEnvironment();
            env.insert(QStringLiteral("SICHTTEST_KANAL"), kanalName);
            // Die DLL soll hier keinen Tester starten.
            env.insert(QStringLiteral("SICHTTEST_EXE"), hier + QStringLiteral("/gibt-es-nicht.exe"));
            p.setProcessEnvironment(env);
            p.setProcessChannelMode(QProcess::MergedChannels);
            p.start(exe, argumente);
        };
        auto starte = [&](QProcess& p, const QString& szenario, const QString& kanalName) {
            starteExe(p, gegenprobe, { dll, szenario }, kanalName);
        };
        auto warteAufEnde = [&](QProcess& p, int ms) {
            const bool beendet = warteBis([&] { return p.state() == QProcess::NotRunning; }, ms);
            if (!beendet) p.kill();
            p.waitForFinished(2000);
            const QString text = QString::fromUtf8(p.readAll());
            const QStringList zeilen = text.split(QLatin1Char('\n'), Qt::SkipEmptyParts);
            for (const QString& z : zeilen) aus << "     | " << z.trimmed() << "\n";
            return beendet && p.exitStatus() == QProcess::NormalExit ? text : QString();
        };

        // 1. Ohne Tester: Fassungen, Ablehnung durch S, Anmelden von Aktionen, kein Tester erreichbar.
        {
            QProcess p;
            starte(p, QStringLiteral("lokal"), kanal + QStringLiteral("-niemand"));
            const QString text = warteAufEnde(p, 15000);
            pruefe(!text.isEmpty() && p.exitCode() == 0,
                   QStringLiteral("Gegenprobe »lokal« meldet keine gescheiterte Prüfung (Exit %1)").arg(p.exitCode()));
        }

        // 2. Mit Tester.
        Steuerung st;
        QStringList meldungen;
        st.beiMeldung = [&](const QString&, const QString& text) { meldungen << text; };
        pruefe(st.lausche(kanal), QStringLiteral("Tester lauscht auf \\\\.\\pipe\\%1").arg(kanal));
        {
            Steuerung zweite;
            const bool lauscht = zweite.lausche(kanal);
            pruefe(!lauscht,
                   QStringLiteral("ein zweiter Tester lauscht nicht auf demselben Kanal (»%1«)").arg(zweite.fehler()));
        }

        const QString g = QStringLiteral("Gegenprobe");
        QProcess app;
        starte(app, QStringLiteral("anwendung"), kanal);
        pruefe(warteBis([&] { return st.anwendung(g) != nullptr; }, 10000),
               QStringLiteral("die Anwendung meldet sich mit hallo und wird begrüßt"));
        if (const Steuerung::Anwendung* a = st.anwendung(g))
        {
            pruefe(a->version == QLatin1String("1.2.3") && a->s == QLatin1String("1.0") && a->p == STS_P_MAJOR
                   && a->produkt == QLatin1String(STS_PRODUKT),
                   QStringLiteral("hallo trägt Version, S, P und Produkt"));
            pruefe(a->pid == app.processId() && QFileInfo(a->exe) == QFileInfo(gegenprobe),
                   QStringLiteral("hallo trägt Pid und Pfad der Exe"));
            QStringList schalterEnde;
            for (const QJsonValue& v : a->aktionen)
                if (v.toObject().value(QStringLiteral("name")).toString() == QLatin1String("ende"))
                    schalterEnde = v.toObject().value(QStringLiteral("schalter")).toVariant().toStringList();
            pruefe(a->aktionen.size() == 8 && st.kennt(g, QStringLiteral("echo"))
                   && schalterEnde == QStringList{ QStringLiteral("beendet_anwendung") },
                   QStringLiteral("acht Aktionen gemeldet, Schalter als Wörter (gezählt %1)").arg(a->aktionen.size()));
        }

        struct Ergebnis { QString status; QString text; bool da = false; };
        auto rufe = [&](const QString& anwendung, const QString& aktion, const QJsonObject& argumente,
                        int fristMs) {
            auto e = std::make_shared<Ergebnis>();
            st.rufe(anwendung, aktion, argumente, QStringLiteral("C:/basis"), fristMs,
                    [e](const QString& status, const QString& text) { e->status = status; e->text = text; e->da = true; });
            warteBis([&] { return e->da; }, 15000);
            return *e;
        };
        auto alsObjekt = [](const QString& text) { return QJsonDocument::fromJson(text.toUtf8()).object(); };

        const QJsonObject arg{ { QStringLiteral("text"), QStringLiteral("Grüße \"zitiert\"\n\\ Ende") },
                               { QStringLiteral("zahl"), 3 } };
        Ergebnis e = rufe(g, QStringLiteral("echo"), arg, 5000);
        pruefe(e.status == QLatin1String("ok") && alsObjekt(e.text) == arg,
               QStringLiteral("Aufruf: Argumente kommen unverändert im Rückruf an (Umlaute, Anführungszeichen)"));

        const QJsonObject gross{ { QStringLiteral("fuell"), QString(300000, QLatin1Char('x')) } };
        e = rufe(g, QStringLiteral("echo"), gross, 5000);
        pruefe(e.status == QLatin1String("ok") && alsObjekt(e.text) == gross,
               QStringLiteral("Aufruf und Antwort mit 300 kB"));

        e = rufe(g, QStringLiteral("gibt_es_nicht"), {}, 5000);
        pruefe(e.status == QLatin1String("unbekannt"), QStringLiteral("unbekannte Aktion: Status unbekannt (»%1«)").arg(e.text));
        e = rufe(g, QStringLiteral("scheitert"), {}, 5000);
        pruefe(e.status == QLatin1String("fehler") && e.text == QLatin1String("absichtlich gescheitert"),
               QStringLiteral("gescheiterte Aktion: Status fehler mit dem Text der Anwendung"));
        e = rufe(g, QStringLiteral("ungueltig"), {}, 5000);
        pruefe(e.status == QLatin1String("ungueltig"), QStringLiteral("unpassende Argumente: Status ungueltig"));

        e = rufe(g, QStringLiteral("pumpt"), {}, 5000);
        pruefe(e.status == QLatin1String("ok") && e.text == QLatin1String("0"),
               QStringLiteral("Wiedereintritt: sts_pumpe() im Rückruf führt nichts aus (liefert %1)").arg(e.text));

        e = rufe(g, QStringLiteral("schlaeft"), {}, 300);
        pruefe(e.status == QLatin1String("frist"), QStringLiteral("Fristablauf: »%1«").arg(e.text));
        e = rufe(g, QStringLiteral("echo"), arg, 5000);
        pruefe(e.status == QLatin1String("ok") && alsObjekt(e.text) == arg,
               QStringLiteral("die verspätete Antwort wird verworfen, der nächste Aufruf bekommt seine eigene"));

        e = rufe(g, QStringLiteral("meldet"), {}, 5000);
        pruefe(e.status == QLatin1String("ok") && meldungen.contains(QStringLiteral("Meldung aus der Aktion")),
               QStringLiteral("sts_melde() kommt als Meldung im Tester an"));

        e = rufe(g, QStringLiteral("spaet"), {}, 5000);
        pruefe(e.status == QLatin1String("ok")
               && warteBis([&] { return st.kennt(g, QStringLiteral("nachgemeldet")); }, 2000),
               QStringLiteral("eine nach dem Verbinden angemeldete Aktion wird nachgemeldet"));
        e = rufe(g, QStringLiteral("nachgemeldet"), arg, 5000);
        pruefe(e.status == QLatin1String("ok") && alsObjekt(e.text) == arg,
               QStringLiteral("die nachgemeldete Aktion lässt sich aufrufen"));

        e = rufe(g, QStringLiteral("ende"), {}, 5000);
        const QString ausgabe = warteAufEnde(app, 5000);
        pruefe(e.status == QLatin1String("ok") && !ausgabe.isEmpty() && app.exitCode() == 0,
               QStringLiteral("Aktion »ende«: Antwort kommt noch, die Anwendung endet von selbst (Exit %1)")
                   .arg(app.exitCode()));
        pruefe(warteBis([&] { return st.anwendung(g) == nullptr; }, 2000),
               QStringLiteral("der Tester bemerkt das Ende der Verbindung"));

        // 3. Die Köpfe für C++ und für Qt: je eine Anwendung, die nur den Kopf benutzt.
        {
            const QString k = QStringLiteral("GegenprobeKopf");
            QProcess p;
            starte(p, QStringLiteral("kopf"), kanal);
            pruefe(warteBis([&] { return st.anwendung(k) != nullptr; }, 10000),
                   QStringLiteral("Kopf für C++: die Anwendung meldet sich"));
            e = rufe(k, QStringLiteral("echo"), arg, 5000);
            pruefe(e.status == QLatin1String("ok") && alsObjekt(e.text) == arg,
                   QStringLiteral("Kopf für C++: ein Lambda als Aktion bekommt die Argumente"));
            e = rufe(k, QStringLiteral("wirft"), {}, 5000);
            pruefe(e.status == QLatin1String("fehler") && e.text.contains(QLatin1String("absichtlich")),
                   QStringLiteral("Kopf für C++: eine Ausnahme in der Aktion wird zum Status fehler (»%1«)").arg(e.text));
            e = rufe(k, QStringLiteral("ende"), {}, 5000);
            const QString text = warteAufEnde(p, 5000);
            pruefe(e.status == QLatin1String("ok") && !text.isEmpty() && p.exitCode() == 0,
                   QStringLiteral("Kopf für C++: Gegenprobe meldet keine gescheiterte Prüfung (Exit %1)").arg(p.exitCode()));
        }
        {
            QString gegenprobeQt = hier;
            gegenprobeQt.replace(QLatin1String("/exec/Sichttest/"), QLatin1String("/exec/GegenprobeQt/"));
            gegenprobeQt += QLatin1String("/GegenprobeQt.exe");
            pruefe(QFileInfo::exists(gegenprobeQt), QStringLiteral("GegenprobeQt ist gebaut: %1").arg(gegenprobeQt));
            const QString q = QStringLiteral("GegenprobeQt");
            QProcess p;
            starteExe(p, gegenprobeQt, { dll }, kanal);
            pruefe(warteBis([&] { return st.anwendung(q) != nullptr; }, 10000),
                   QStringLiteral("Kopf für Qt: die Anwendung meldet sich"));
            e = rufe(q, QStringLiteral("echo"), arg, 5000);
            pruefe(e.status == QLatin1String("ok") && alsObjekt(e.text) == arg,
                   QStringLiteral("Kopf für Qt: die Aktion bekommt die Argumente als QJsonObject"));
            e = rufe(q, QStringLiteral("faden"), {}, 5000);
            pruefe(e.status == QLatin1String("ok") && e.text == QLatin1String("gui"),
                   QStringLiteral("Kopf für Qt: die Aktion läuft im GUI-Thread (»%1«)").arg(e.text));
            e = rufe(q, QStringLiteral("wirft"), {}, 5000);
            pruefe(e.status == QLatin1String("fehler") && e.text.contains(QLatin1String("absichtlich")),
                   QStringLiteral("Kopf für Qt: eine Ausnahme in der Aktion wird zum Status fehler"));
            e = rufe(q, QStringLiteral("ende"), {}, 5000);
            const QString text = warteAufEnde(p, 5000);
            pruefe(e.status == QLatin1String("ok") && !text.isEmpty() && p.exitCode() == 0,
                   QStringLiteral("Kopf für Qt: Gegenprobe meldet keine gescheiterte Prüfung (Exit %1)").arg(p.exitCode()));
        }

        // 4. Ablehnung: der Tester gibt sich als zu neu aus (spricht nur P 2 und P 3).
        st.setzeFassungen(STS_P_MAJOR + 1, STS_P_MAJOR + 2);
        {
            QProcess p;
            starte(p, QStringLiteral("anwendung"), kanal);
            const QString text = warteAufEnde(p, 10000);
            pruefe(text.contains(QLatin1String("abgelehnt")) && p.exitCode() == 0,
                   QStringLiteral("zu alte Anwendung: die DLL meldet Zustand abgelehnt mit dem Grund"));
            pruefe(st.letzteAblehnung().contains(QLatin1String("Pin")) && st.anwendung(g) == nullptr,
                   QStringLiteral("der Tester nennt die Ablehnung und führt die Anwendung nicht als verbunden"));
        }
        return fehler;
    }
}
