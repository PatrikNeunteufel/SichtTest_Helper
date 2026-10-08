#include "Protokoll.hpp"

#include <QDateTime>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QHash>
#include <QJsonDocument>
#include <QRegularExpression>
#include <QSet>

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
            QJsonObject s;
            s.insert(QStringLiteral("id"), id);
            s.insert(QStringLiteral("section"), sektion);
            s.insert(QStringLiteral("title"), titel);
            s.insert(QStringLiteral("text"), rest);
            s.insert(QStringLiteral("result"), ergebnisAusHaken(haken));
            s.insert(QStringLiteral("remark"), QString());
            s.insert(QStringLiteral("screenshots"), QJsonArray());
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
        macheIdsEindeutig(p.schritte);
        return p;
    }

    Protokoll ausJson(const QJsonObject& wurzel, const QString& pfad)
    {
        Protokoll p;
        p.pfad = pfad;
        p.titel = wurzel.value(QStringLiteral("title")).toString(QFileInfo(pfad).completeBaseName());
        p.beschreibung = wurzel.value(QStringLiteral("description")).toString();
        p.exe = wurzel.value(QStringLiteral("exe")).toString(findeExe(p.beschreibung));
        const QJsonArray schritte = wurzel.value(QStringLiteral("steps")).toArray();
        for (const QJsonValue& v : schritte)
        {
            QJsonObject s = v.toObject();
            if (!s.contains(QStringLiteral("result")))
                s.insert(QStringLiteral("result"), QStringLiteral("open"));
            if (!s.contains(QStringLiteral("remark")))
                s.insert(QStringLiteral("remark"), QString());
            if (!s.contains(QStringLiteral("screenshots")))
                s.insert(QStringLiteral("screenshots"), QJsonArray());
            p.schritte.append(s);
        }
        macheIdsEindeutig(p.schritte);
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
        for (int n = 0; n < protokoll.schritte.size(); ++n)
        {
            QJsonObject s = protokoll.schritte.at(n).toObject();
            const auto it = alt.constFind(s.value(QStringLiteral("id")).toString());
            if (it == alt.constEnd()) continue;
            for (const char* feld : { "result", "remark", "screenshots", "rated" })
                if (it->contains(QLatin1String(feld)))
                    s.insert(QLatin1String(feld), it->value(QLatin1String(feld)));
            protokoll.schritte.replace(n, s);
        }
    }

    Zaehler zaehle(const QJsonArray& schritte)
    {
        Zaehler z;
        for (const QJsonValue& v : schritte)
        {
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
        return QStringLiteral("○");
    }

    QJsonObject alsLog(const Protokoll& protokoll, const QString& gestartet,
                       const QString& exeZeit)
    {
        const Zaehler z = zaehle(protokoll.schritte);
        QJsonObject build;
        build.insert(QStringLiteral("exe"), protokoll.exe);
        build.insert(QStringLiteral("exe_timestamp"), exeZeit);
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

    QStringList findeProtokolle(const QString& ordner)
    {
        QStringList gefunden;
        const QDir d(ordner);
        const QStringList namen = d.entryList(
            { QStringLiteral("*.testprotokoll.json"), QStringLiteral("*.md") },
            QDir::Files, QDir::Name | QDir::Reversed);
        for (const QString& name : namen)
        {
            const QString pfad = d.filePath(name);
            if (!lade(pfad).schritte.isEmpty()) gefunden.append(pfad);
        }
        return gefunden;
    }
}
