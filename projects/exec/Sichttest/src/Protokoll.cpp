#include "Protokoll.hpp"

#include "Projekt.hpp"

#include <QDateTime>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QHash>
#include <QJsonDocument>
#include <QRegularExpression>
#include <QSet>

#include <functional>

namespace sichttest
{
    namespace
    {
        QString ergebnisAusHaken(const QString& haken)
        {
            if (haken == QLatin1String("x") || haken == QLatin1String("X")) return QStringLiteral("pass");
            if (haken == QLatin1String("!")) return QStringLiteral("fail");
            if (haken == QLatin1String("-")) return QStringLiteral("skip");
            return QStringLiteral("open");
        }

        // Kopf eines Punkts zerlegen: `**A1 Titel:** Rest` → id, Titel, Rest.
        void zerlegeKopf(const QString& kopf, QString& id, QString& titel, QString& rest)
        {
            static const QRegularExpression fett(QStringLiteral(R"(^\*\*(.+?)\*\*\s*(.*)$)"));
            static const QRegularExpression mitId(QStringLiteral(R"(^([A-Za-z]{1,3}\d+[a-z]?)\s+(.*)$)"));

            QString name;
            const auto f = fett.match(kopf);
            if (f.hasMatch())
            {
                name = f.captured(1).trimmed();
                rest = f.captured(2).trimmed();
            }
            else
            {
                // Ohne Fettdruck: der Anfang bis zum ersten Satzende ist der Titel.
                const int ende = kopf.indexOf(QRegularExpression(QStringLiteral(R"([.:?!](\s|$))")));
                name = (ende > 0 && ende < 90) ? kopf.left(ende) : kopf.left(70);
                rest = kopf;
            }
            while (name.endsWith(QLatin1Char(':'))) name.chop(1);

            const auto i = mitId.match(name);
            if (i.hasMatch())
            {
                id = i.captured(1);
                titel = i.captured(2).trimmed();
            }
            else
            {
                id.clear();
                titel = name;
            }
        }

        // Erste Exe, die in der Beschreibung in Backticks steht.
        QString findeExe(const QString& text)
        {
            static const QRegularExpression re(QStringLiteral(R"(`([^`\n]+\.exe)`)"),
                                                QRegularExpression::CaseInsensitiveOption);
            const auto m = re.match(text);
            return m.hasMatch() ? m.captured(1).trimmed() : QString();
        }

        void macheIdsEindeutig(QJsonArray& schritte)
        {
            QSet<QString> gesehen;
            int laufend = 0;
            for (int n = 0; n < schritte.size(); ++n)
            {
                QJsonObject s = schritte.at(n).toObject();
                QString id = s.value(QStringLiteral("id")).toString();
                ++laufend;
                if (id.isEmpty()) id = QStringLiteral("P%1").arg(laufend);
                QString frei = id;
                for (int k = 2; gesehen.contains(frei); ++k)
                    frei = QStringLiteral("%1-%2").arg(id).arg(k);
                gesehen.insert(frei);
                s.insert(QStringLiteral("id"), frei);
                schritte.replace(n, s);
            }
        }

        QString zeile(const QJsonObject& s)
        {
            return QStringLiteral("**%1** %2")
                .arg(s.value(QStringLiteral("id")).toString(),
                     s.value(QStringLiteral("title")).toString());
        }

        // Die Code-Stücke `aktion: …` aus dem Text eines Punkts nehmen (§6.1): im
        // Fenster stehen sie nicht, in jedem anderen Betrachter als unauffälliger Code.
        QString nimmAktionenHeraus(QString text, QJsonArray& aktionen)
        {
            static const QRegularExpression stueck(QStringLiteral(R"(`\s*aktion:\s*([^`]*)`)"));
            static const QRegularExpression luecke(QStringLiteral(R"([ \t]{2,})"));
            auto it = stueck.globalMatch(text);
            while (it.hasNext())
            {
                const QJsonObject a = leseAktion(it.next().captured(1));
                if (!a.isEmpty()) aktionen.append(a);
            }
            text.remove(stueck);
            text.replace(luecke, QStringLiteral(" "));
            return text.trimmed();
        }

        void macheZurVorbereitung(QJsonObject& s)
        {
            s.insert(QStringLiteral("kind"), QStringLiteral("prep"));
            s.insert(QStringLiteral("result"), QStringLiteral("prep"));
        }

        int findeSchritt(const Protokoll& p, const QString& id)
        {
            for (int n = 0; n < p.schritte.size(); ++n)
                if (p.schritte.at(n).toObject().value(QStringLiteral("id")).toString() == id) return n;
            return -1;
        }

        void sammleAktionen(const Protokoll& p, int idx, QStringList& weg, QJsonArray& aus, QStringList* hinweise)
        {
            const QJsonObject s = p.schritte.at(idx).toObject();
            const QString id = s.value(QStringLiteral("id")).toString();
            weg.append(id);
            const QJsonArray aktionen = s.value(QStringLiteral("aktionen")).toArray();
            for (const QJsonValue& v : aktionen)
            {
                QJsonObject a = v.toObject();
                const QString name = a.value(QStringLiteral("aktion")).toString();
                if (!name.startsWith(QLatin1Char('@')))
                {
                    aus.append(a);
                    continue;
                }
                const QString zielId = name.mid(1);
                const int ziel = findeSchritt(p, zielId);
                if (ziel >= 0 && !weg.contains(zielId))
                {
                    sammleAktionen(p, ziel, weg, aus, hinweise);
                    continue;
                }
                if (hinweise)
                    hinweise->append(ziel < 0
                        ? QStringLiteral("%1: Verweis %2 nennt eine Kennung, die es in der Liste nicht gibt").arg(id, name)
                        : QStringLiteral("%1: Verweis %2 läuft im Kreis").arg(id, name));
                a.insert(QStringLiteral("unbekannt"), true);
                aus.append(a);
            }
            weg.removeLast();
        }

        void pruefeVerweise(Protokoll& p)
        {
            for (int n = 0; n < p.schritte.size(); ++n) aktionenVon(p, n, &p.hinweise);
            p.hinweise.removeDuplicates();
        }
    }

    QJsonObject leseAktion(const QString& stueck)
    {
        // In Teile schneiden: Leerraum trennt, außer in "…"; das erste = außerhalb
        // von Anführungszeichen trennt Schlüssel und Wert.
        struct Teil { QString text; int gleich = -1; };
        QList<Teil> teile;
        Teil t;
        bool zitat = false;
        bool begonnen = false;
        auto schliesse = [&]() {
            if (begonnen) teile.append(t);
            t = {};
            begonnen = false;
        };
        for (const QChar c : stueck)
        {
            if (c == QLatin1Char('"')) { zitat = !zitat; begonnen = true; continue; }
            if (c.isSpace() && !zitat) { schliesse(); continue; }
            if (c == QLatin1Char('=') && !zitat && t.gleich < 0) t.gleich = int(t.text.size());
            t.text += c;
            begonnen = true;
        }
        schliesse();
        if (teile.isEmpty() || teile.first().text.isEmpty()) return {};

        QJsonObject mit;
        QStringList lose;
        for (int n = 1; n < teile.size(); ++n)
        {
            const Teil& e = teile.at(n);
            if (e.gleich > 0) mit.insert(e.text.left(e.gleich), e.text.mid(e.gleich + 1));
            else lose.append(e.text);
        }
        if (!lose.isEmpty()) mit.insert(QStringLiteral("wert"), lose.join(QLatin1Char(' ')));
        QJsonObject a;
        a.insert(QStringLiteral("aktion"), teile.first().text);
        a.insert(QStringLiteral("mit"), mit);
        return a;
    }

    bool istVerwaist(const QJsonObject& schritt)
    {
        return schritt.value(QStringLiteral("verwaist")).toBool();
    }

    bool istVorbereitung(const QJsonObject& schritt)
    {
        return schritt.value(QStringLiteral("kind")).toString() == QLatin1String("prep");
    }

    QJsonArray aktionenVon(const Protokoll& protokoll, int idx, QStringList* hinweise)
    {
        QJsonArray aus;
        if (idx < 0 || idx >= protokoll.schritte.size()) return aus;
        QStringList weg;
        sammleAktionen(protokoll, idx, weg, aus, hinweise);
        return aus;
    }

    int vorbereitungVon(const Protokoll& protokoll, int idx)
    {
        if (idx < 0 || idx >= protokoll.schritte.size()) return -1;
        const QString sektion = protokoll.schritte.at(idx).toObject().value(QStringLiteral("section")).toString();
        int fuerAlle = -1;
        for (int n = 0; n < protokoll.schritte.size(); ++n)
        {
            const QJsonObject s = protokoll.schritte.at(n).toObject();
            if (!istVorbereitung(s)) continue;
            const QString sek = s.value(QStringLiteral("section")).toString();
            if (sek == sektion && !sektion.isEmpty()) return n;
            if (sek.isEmpty() && fuerAlle < 0) fuerAlle = n;
        }
        return fuerAlle;
    }

    Protokoll ausMarkdown(const QString& text, const QString& pfad)
    {
        static const QRegularExpression punkt(QStringLiteral(R"(^\s*[-*]\s+\[( |x|X|!|-)\]\s+(.*)$)"));
        static const QRegularExpression abschnitt(QStringLiteral(R"(^(#{2,})\s+(.*)$)"));
        static const QRegularExpression haupt(QStringLiteral(R"(^#\s+(.*)$)"));

        Protokoll p;
        p.pfad = pfad;
        p.titel = QFileInfo(pfad).completeBaseName();

        QString sektion;
        QStringList vorspann;
        bool offen = false;       // ein Punkt wird gerade gesammelt
        bool leerzeile = false;
        QString haken, kopf;

        auto schliesse = [&]() {
            if (!offen) return;
            offen = false;
            QString id, titel, rest;
            zerlegeKopf(kopf.trimmed(), id, titel, rest);
            QJsonArray aktionen;
            rest = nimmAktionenHeraus(rest, aktionen);
            QJsonObject s;
            s.insert(QStringLiteral("id"), id);
            s.insert(QStringLiteral("section"), sektion);
            s.insert(QStringLiteral("title"), titel);
            s.insert(QStringLiteral("text"), rest);
            s.insert(QStringLiteral("result"), ergebnisAusHaken(haken));
            s.insert(QStringLiteral("remark"), QString());
            s.insert(QStringLiteral("screenshots"), QJsonArray());
            if (!aktionen.isEmpty()) s.insert(QStringLiteral("aktionen"), aktionen);
            if (titel.startsWith(QLatin1String("Vorbereiten"), Qt::CaseInsensitive)) macheZurVorbereitung(s);
            p.schritte.append(s);
        };

        const QStringList zeilen = text.split(QLatin1Char('\n'));
        for (QString z : zeilen)
        {
            if (z.endsWith(QLatin1Char('\r'))) z.chop(1);

            const auto mp = punkt.match(z);
            if (mp.hasMatch())
            {
                schliesse();
                offen = true;
                leerzeile = false;
                haken = mp.captured(1);
                kopf = mp.captured(2);
                continue;
            }
            const auto ma = abschnitt.match(z);
            if (ma.hasMatch())
            {
                schliesse();
                sektion = ma.captured(2).trimmed();
                continue;
            }
            const auto mh = haupt.match(z);
            if (mh.hasMatch())
            {
                schliesse();
                if (p.schritte.isEmpty()) p.titel = mh.captured(1).trimmed();
                continue;
            }
            if (z.trimmed().isEmpty())
            {
                leerzeile = offen;
                if (!offen && sektion.isEmpty()) vorspann.append(QString());
                continue;
            }
            const bool eingerueckt = z.at(0).isSpace();
            if (offen && eingerueckt)
            {
                // Fortsetzung des Punkts; ein Absatz bleibt ein Absatz.
                kopf += (leerzeile ? QStringLiteral("\n\n") : QStringLiteral(" ")) + z.trimmed();
                leerzeile = false;
                continue;
            }
            schliesse();
            if (sektion.isEmpty() && z.trimmed() != QLatin1String("---"))
                vorspann.append(z);
        }
        schliesse();

        p.beschreibung = vorspann.join(QLatin1Char('\n')).trimmed();
        p.exe = findeExe(p.beschreibung);
        static const QRegularExpression anwendung(QStringLiteral(R"(\*\*Anwendung:\*\*\s*([^\s·`*]+))"));
        p.anwendung = anwendung.match(p.beschreibung).captured(1);
        macheIdsEindeutig(p.schritte);
        pruefeVerweise(p);
        return p;
    }

    Protokoll ausJson(const QJsonObject& wurzel, const QString& pfad)
    {
        Protokoll p;
        p.pfad = pfad;
        p.titel = wurzel.value(QStringLiteral("title")).toString(QFileInfo(pfad).completeBaseName());
        p.beschreibung = wurzel.value(QStringLiteral("description")).toString();
        p.exe = wurzel.value(QStringLiteral("exe")).toString(findeExe(p.beschreibung));
        p.anwendung = wurzel.value(QStringLiteral("anwendung")).toString();
        p.nachbereitung = wurzel.value(QStringLiteral("nachbereitung")).toArray();

        // Was für die ganze Liste herzurichten ist, wird ein Punkt vor dem ersten Abschnitt.
        // Die Felder des Comm Studio (§6.2) stehen dort immer als Text; eine Aktion lösen
        // sie nur aus, wenn die Projektdatei unter "abbildung" eine nennt.
        QJsonArray vorbereitung;
        QStringList texte;
        auto bilde = [](const char* feld, const QString& text, const std::function<QJsonValue(const QString&)>& wert) {
            QString aktion;
            QString wenn;
            QStringList schluessel;
            if (!abbildung(projekt(), QLatin1String(feld), aktion, schluessel, &wenn)) return QJsonObject();
            // "fortgesetzt" ist vorbelegt: kein Feld der Liste, der Tester füllt es beim Aufruf (§7).
            QJsonObject mit;
            for (const QString& k : std::as_const(schluessel))
                mit.insert(k, k == QLatin1String("fortgesetzt") ? QJsonValue(false) : wert(k));
            QJsonObject a{ { QStringLiteral("aktion"), aktion }, { QStringLiteral("mit"), mit },
                           { QStringLiteral("text"), text }, { QStringLiteral("feld"), QLatin1String(feld) } };
            if (!wenn.isEmpty()) a.insert(QStringLiteral("wenn"), wenn);
            return a;
        };

        // Die Test-DB zuerst: die Tabs aus "setup" fragen beim Öffnen schon die Datenbank ab.
        p.testDb = wurzel.value(QStringLiteral("test_db")).toObject();
        if (!p.testDb.isEmpty())
        {
            const QString text = QStringLiteral("Test-DB »%1« einrichten")
                                     .arg(p.testDb.value(QStringLiteral("name")).toString());
            texte.append(QStringLiteral("- ") + text);
            const QJsonObject a = bilde("test_db", text, [&p](const QString& k) { return p.testDb.value(k); });
            if (!a.isEmpty()) vorbereitung.append(a);
            const QJsonObject ende = bilde("test_db.ende", QStringLiteral("Test-DB abbauen"),
                                           [](const QString&) { return QJsonValue(); });
            if (!ende.isEmpty()) p.nachbereitung.append(ende);
        }
        p.setup = wurzel.value(QStringLiteral("setup")).toObject();
        if (p.setup.value(QStringLiteral("close_all_tabs")).toBool())
        {
            const QString text = QStringLiteral("Alle offenen Tabs und das aktive Projekt schließen (ohne Speichern-Nachfragen)");
            texte.append(QStringLiteral("- ") + text);
            const QJsonObject a = bilde("setup.close_all_tabs", text, [](const QString&) { return QJsonValue(); });
            if (!a.isEmpty()) vorbereitung.append(a);
        }
        const QJsonArray oeffnen = p.setup.value(QStringLiteral("open")).toArray();
        for (const QJsonValue& v : oeffnen)
        {
            const QString text = QStringLiteral("Öffnen: %1").arg(v.toString());
            texte.append(QStringLiteral("- ") + text);
            const QJsonObject a = bilde("setup.open", text, [&v](const QString&) { return v; });
            if (!a.isEmpty()) vorbereitung.append(a);
        }
        const QJsonArray eigene = wurzel.value(QStringLiteral("vorbereitung")).toArray();
        for (const QJsonValue& v : eigene)
        {
            const QString t = v.toObject().value(QStringLiteral("text")).toString();
            if (!t.isEmpty()) texte.append(QStringLiteral("- ") + t);
            vorbereitung.append(v);
        }
        if (!texte.isEmpty() || !vorbereitung.isEmpty())
        {
            QJsonObject s;
            s.insert(QStringLiteral("id"), QStringLiteral("V0"));
            s.insert(QStringLiteral("section"), QString());
            s.insert(QStringLiteral("title"), QStringLiteral("Vorbereiten"));
            s.insert(QStringLiteral("text"), texte.join(QLatin1Char('\n')));
            s.insert(QStringLiteral("remark"), QString());
            s.insert(QStringLiteral("screenshots"), QJsonArray());
            s.insert(QStringLiteral("aktionen"), vorbereitung);
            macheZurVorbereitung(s);
            p.schritte.append(s);
        }

        const QJsonArray schritte = wurzel.value(QStringLiteral("steps")).toArray();
        for (const QJsonValue& v : schritte)
        {
            QJsonObject s = v.toObject();
            if (istVorbereitung(s)) macheZurVorbereitung(s);
            // "tab" und "restart" des Comm Studio: mit Abbildung eine Aktion, sonst nur Anzeige.
            QJsonArray aktionen = s.value(QStringLiteral("aktionen")).toArray();
            const QString tab = s.value(QStringLiteral("tab")).toString();
            if (!tab.isEmpty())
            {
                const QJsonObject a = bilde("tab", QStringLiteral("Tab »%1« nach vorn holen").arg(tab),
                                            [&tab](const QString&) { return QJsonValue(tab); });
                if (!a.isEmpty()) aktionen.prepend(a);
            }
            if (s.value(QStringLiteral("restart")).toBool())
            {
                const QJsonObject a = bilde("restart", QStringLiteral("Anwendung neu starten"),
                                            [](const QString&) { return QJsonValue(); });
                if (!a.isEmpty()) aktionen.append(a);
            }
            if (!aktionen.isEmpty()) s.insert(QStringLiteral("aktionen"), aktionen);
            if (!s.contains(QStringLiteral("result")))
                s.insert(QStringLiteral("result"), QStringLiteral("open"));
            if (!s.contains(QStringLiteral("remark")))
                s.insert(QStringLiteral("remark"), QString());
            if (!s.contains(QStringLiteral("screenshots")))
                s.insert(QStringLiteral("screenshots"), QJsonArray());
            p.schritte.append(s);
        }
        macheIdsEindeutig(p.schritte);
        pruefeVerweise(p);
        return p;
    }

    Protokoll lade(const QString& pfad, QString* fehler)
    {
        QFile f(pfad);
        if (!f.open(QIODevice::ReadOnly))
        {
            if (fehler) *fehler = QStringLiteral("Datei nicht lesbar: %1").arg(pfad);
            return {};
        }
        const QByteArray roh = f.readAll();
        Protokoll p;
        if (pfad.endsWith(QLatin1String(".json"), Qt::CaseInsensitive))
        {
            QJsonParseError pe;
            const QJsonDocument doc = QJsonDocument::fromJson(roh, &pe);
            if (pe.error != QJsonParseError::NoError)
            {
                if (fehler) *fehler = QStringLiteral("%1: %2").arg(pfad, pe.errorString());
                return {};
            }
            p = ausJson(doc.object(), pfad);
        }
        else
        {
            p = ausMarkdown(QString::fromUtf8(roh), pfad);
        }
        if (p.schritte.isEmpty() && fehler)
            *fehler = QStringLiteral("Keine Schritte in %1 gefunden.").arg(pfad);
        return p;
    }

    void uebernimmLauf(Protokoll& protokoll, const QJsonObject& log)
    {
        QHash<QString, QJsonObject> alt;
        const QJsonArray schritte = log.value(QStringLiteral("steps")).toArray();
        for (const QJsonValue& v : schritte)
        {
            const QJsonObject s = v.toObject();
            alt.insert(s.value(QStringLiteral("id")).toString(), s);
        }
        // Der Build hängt am Urteil (§14.1): ein Urteil ohne eigenen Stempel — aus einem Lauf
        // vor v0.4.0 oder aus dem Comm Studio — bekommt den, den der Lauf bis hierher nannte.
        const QJsonObject laufBuild = log.value(QStringLiteral("build")).toObject();
        const bool laufBuildDa = !laufBuild.value(QStringLiteral("exe")).toString().isEmpty()
                                 || !laufBuild.value(QStringLiteral("exe_timestamp")).toString().isEmpty();
        auto stempele = [&](QJsonObject& s) {
            const QString r = s.value(QStringLiteral("result")).toString();
            const bool urteil = r == QLatin1String("pass") || r == QLatin1String("pass_remark")
                                || r == QLatin1String("fail") || r == QLatin1String("skip");
            if (urteil && laufBuildDa && !s.contains(QStringLiteral("build")))
                s.insert(QStringLiteral("build"), laufBuild);
        };
        QSet<QString> inDerListe;
        for (int n = 0; n < protokoll.schritte.size(); ++n)
        {
            QJsonObject s = protokoll.schritte.at(n).toObject();
            inDerListe.insert(s.value(QStringLiteral("id")).toString());
            const auto it = alt.constFind(s.value(QStringLiteral("id")).toString());
            if (it == alt.constEnd()) continue;
            // history: das Befund-Archiv des Comm Studio — wird mitgeführt, damit alte Läufe ganz bleiben.
            for (const char* feld : { "result", "remark", "screenshots", "rated", "actions", "history", "build" })
                if (it->contains(QLatin1String(feld)))
                    s.insert(QLatin1String(feld), it->value(QLatin1String(feld)));
            if (istVorbereitung(s)) s.insert(QStringLiteral("result"), QStringLiteral("prep"));
            else stempele(s);
            protokoll.schritte.replace(n, s);
        }
        // Verwaiste Urteile: der Schritt steht nicht mehr in der Liste, sein Befund bleibt.
        for (const QJsonValue& v : schritte)
        {
            QJsonObject s = v.toObject();
            if (inDerListe.contains(s.value(QStringLiteral("id")).toString())) continue;
            const QString r = s.value(QStringLiteral("result")).toString();
            const bool urteil = !r.isEmpty() && r != QLatin1String("open") && r != QLatin1String("prep");
            const bool beleg = !s.value(QStringLiteral("remark")).toString().trimmed().isEmpty()
                               || !s.value(QStringLiteral("screenshots")).toArray().isEmpty()
                               || !s.value(QStringLiteral("history")).toArray().isEmpty();
            if (!urteil && !beleg) continue;
            if (r.isEmpty() || r == QLatin1String("prep")) s.insert(QStringLiteral("result"), QStringLiteral("open"));
            stempele(s);
            s.remove(QStringLiteral("aktionen"));
            s.remove(QStringLiteral("kind"));
            s.insert(QStringLiteral("verwaist"), true);
            s.insert(QStringLiteral("section"), QStringLiteral("Nicht mehr in der Liste"));
            protokoll.schritte.append(s);
        }
        protokoll.zustaende = log.value(QStringLiteral("zustaende")).toObject();
        protokoll.nachbereitungLog = log.value(QStringLiteral("teardown_actions")).toArray();
        protokoll.steuerung = log.value(QStringLiteral("steuerung")).toObject();
        protokoll.build = log.value(QStringLiteral("build")).toObject();
        const QJsonValue aktiv = log.value(QStringLiteral("test_db")).toObject().value(QStringLiteral("active"));
        if (!aktiv.isUndefined() && !protokoll.testDb.isEmpty())
            protokoll.testDb.insert(QStringLiteral("active"), aktiv);
    }

    Zaehler zaehle(const QJsonArray& schritte)
    {
        Zaehler z;
        for (const QJsonValue& v : schritte)
        {
            if (istVorbereitung(v.toObject())) continue;   // ohne Urteil, zählt nicht
            if (istVerwaist(v.toObject())) continue;       // nicht mehr in der Liste, zählt nicht
            const QString r = v.toObject().value(QStringLiteral("result")).toString();
            if (r == QLatin1String("pass")) ++z.pass;
            else if (r == QLatin1String("pass_remark")) ++z.passBefund;
            else if (r == QLatin1String("fail")) ++z.fail;
            else if (r == QLatin1String("skip")) ++z.skip;
            else ++z.offen;
        }
        return z;
    }

    QString zeichen(const QString& ergebnis)
    {
        if (ergebnis == QLatin1String("pass"))        return QStringLiteral("✓");
        if (ergebnis == QLatin1String("pass_remark")) return QStringLiteral("✓⚠");
        if (ergebnis == QLatin1String("fail"))        return QStringLiteral("✗");
        if (ergebnis == QLatin1String("skip"))        return QStringLiteral("↷");
        if (ergebnis == QLatin1String("prep"))        return QStringLiteral("▷");
        return QStringLiteral("○");
    }

    QJsonObject alsLog(const Protokoll& protokoll, const QString& gestartet)
    {
        const Zaehler z = zaehle(protokoll.schritte);
        QJsonObject build;
        build.insert(QStringLiteral("exe"), protokoll.build.value(QStringLiteral("exe")).toString());
        build.insert(QStringLiteral("exe_timestamp"),
                     protokoll.build.value(QStringLiteral("exe_timestamp")).toString());
        QJsonObject summe;
        summe.insert(QStringLiteral("pass"), z.pass);
        summe.insert(QStringLiteral("pass_remark"), z.passBefund);
        summe.insert(QStringLiteral("fail"), z.fail);
        summe.insert(QStringLiteral("skip"), z.skip);
        summe.insert(QStringLiteral("open"), z.offen);

        QJsonObject log;
        log.insert(QStringLiteral("protocol"), protokoll.titel);
        log.insert(QStringLiteral("protocol_file"), protokoll.pfad);
        log.insert(QStringLiteral("build"), build);
        log.insert(QStringLiteral("started"), gestartet);
        log.insert(QStringLiteral("finished"),
                   z.offen == 0 ? QDateTime::currentDateTime().toString(Qt::ISODate) : QString());
        log.insert(QStringLiteral("steps"), protokoll.schritte);
        log.insert(QStringLiteral("summary"), summe);
        if (!protokoll.setup.isEmpty()) log.insert(QStringLiteral("setup"), protokoll.setup);
        if (!protokoll.testDb.isEmpty()) log.insert(QStringLiteral("test_db"), protokoll.testDb);
        if (!protokoll.nachbereitungLog.isEmpty())
            log.insert(QStringLiteral("teardown_actions"), protokoll.nachbereitungLog);
        if (!protokoll.steuerung.isEmpty()) log.insert(QStringLiteral("steuerung"), protokoll.steuerung);
        if (!protokoll.zustaende.isEmpty()) log.insert(QStringLiteral("zustaende"), protokoll.zustaende);
        return log;
    }

    QString alsReport(const Protokoll& protokoll, const QString& gestartet,
                      const QString& exeZeit)
    {
        const Zaehler z = zaehle(protokoll.schritte);
        QString r;
        r += QStringLiteral("# Sichttest-Report — %1\n\n").arg(protokoll.titel);
        r += QStringLiteral("- Protokoll: `%1`\n").arg(protokoll.pfad);
        r += QStringLiteral("- Lauf seit: %1 · Stand: %2\n")
                 .arg(gestartet, QDateTime::currentDateTime().toString(Qt::ISODate));
        if (!protokoll.exe.isEmpty())
            r += QStringLiteral("- Exe: `%1` (%2)\n")
                     .arg(protokoll.exe, exeZeit.isEmpty() ? QStringLiteral("nicht gefunden") : exeZeit);
        r += QStringLiteral("- Ergebnis: ✓ %1 · ✓⚠ %2 · ✗ %3 · ↷ %4 · ○ %5\n")
                 .arg(z.pass).arg(z.passBefund).arg(z.fail).arg(z.skip).arg(z.offen);

        // Ausführlich: Fail und Pass mit Befund, samt Bemerkung und Bildern.
        auto block = [&](const QString& ergebnis, const QString& kopf) {
            bool erster = true;
            for (const QJsonValue& v : protokoll.schritte)
            {
                const QJsonObject s = v.toObject();
                if (s.value(QStringLiteral("result")).toString() != ergebnis) continue;
                if (erster) { r += QStringLiteral("\n## %1\n").arg(kopf); erster = false; }
                r += QStringLiteral("\n### %1 %2\n\n")
                         .arg(s.value(QStringLiteral("id")).toString(),
                              s.value(QStringLiteral("title")).toString());
                const QString sektion = s.value(QStringLiteral("section")).toString();
                if (!sektion.isEmpty()) r += QStringLiteral("*%1*\n\n").arg(sektion);
                const QString bemerkung = s.value(QStringLiteral("remark")).toString().trimmed();
                r += (bemerkung.isEmpty() ? QStringLiteral("(keine Bemerkung)") : bemerkung)
                     + QStringLiteral("\n");
                const QJsonArray bilder = s.value(QStringLiteral("screenshots")).toArray();
                for (const QJsonValue& b : bilder)
                    r += QStringLiteral("\n![%1](%1)\n").arg(b.toString());
            }
        };
        block(QStringLiteral("fail"), QStringLiteral("✗ Fail"));
        block(QStringLiteral("pass_remark"), QStringLiteral("✓⚠ Pass mit Befund"));

        // Knapp: der Rest als Liste; eine Bemerkung steht dahinter.
        auto liste = [&](const QString& ergebnis, const QString& kopf) {
            bool erster = true;
            for (const QJsonValue& v : protokoll.schritte)
            {
                const QJsonObject s = v.toObject();
                if (s.value(QStringLiteral("result")).toString() != ergebnis) continue;
                if (erster) { r += QStringLiteral("\n## %1\n\n").arg(kopf); erster = false; }
                r += QStringLiteral("- ") + zeile(s);
                const QString bemerkung = s.value(QStringLiteral("remark")).toString().trimmed();
                if (!bemerkung.isEmpty())
                    r += QStringLiteral(" — ") + QString(bemerkung).replace(QLatin1Char('\n'), QLatin1Char(' '));
                const QJsonArray bilder = s.value(QStringLiteral("screenshots")).toArray();
                for (const QJsonValue& b : bilder)
                    r += QStringLiteral(" · `%1`").arg(b.toString());
                r += QLatin1Char('\n');
            }
        };
        liste(QStringLiteral("skip"), QStringLiteral("↷ Übersprungen"));
        liste(QStringLiteral("open"), QStringLiteral("○ Offen"));
        liste(QStringLiteral("pass"), QStringLiteral("✓ Pass"));
        return r;
    }

    QString logOrdner(const QString& protokollPfad)
    {
        const QString ablage = ablageVon(projekt(), protokollPfad);
        if (!ablage.isEmpty()) return ablage;
        return QFileInfo(protokollPfad).absolutePath() + QStringLiteral("/sichttest-logs");
    }

    QString logStamm(const QString& protokollPfad)
    {
        QString name = QFileInfo(protokollPfad).fileName();
        for (const char* endung : { ".testprotokoll.json", ".json", ".md" })
            if (name.endsWith(QLatin1String(endung), Qt::CaseInsensitive))
            {
                name.chop(int(qstrlen(endung)));
                break;
            }
        return name;
    }

    QString neuesterLauf(const QString& protokollPfad)
    {
        const QDir ordner(logOrdner(protokollPfad));
        const QStringList logs = ordner.entryList(
            { logStamm(protokollPfad) + QStringLiteral("_*.testlog.json") },
            QDir::Files, QDir::Name | QDir::Reversed);
        return logs.isEmpty() ? QString() : ordner.filePath(logs.first());
    }

    bool istAbgeschlossen(const QJsonObject& log)
    {
        return log.value(QStringLiteral("summary")).toObject().value(QStringLiteral("open")).toInt() == 0;
    }

    QStringList findeProtokolle(const QString& ordner, const QString& muster)
    {
        QStringList gefunden;
        const QDir d(ordner);
        const QStringList namen = d.entryList(
            muster.isEmpty() ? QStringList{ QStringLiteral("*.testprotokoll.json"), QStringLiteral("*.md") }
                             : QStringList{ muster },
            QDir::Files, QDir::Name | QDir::Reversed);
        for (const QString& name : namen)
        {
            const QString pfad = d.filePath(name);
            if (!lade(pfad).schritte.isEmpty()) gefunden.append(pfad);
        }
        return gefunden;
    }
}
