#include "Projekt.hpp"

#include "Protokoll.hpp"

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>

namespace sichttest
{
    namespace
    {
        const QString kDateiName = QStringLiteral("sichttest.projekt.json");

        Projekt& aktuelles()
        {
            static Projekt p;
            return p;
        }
    }

    Projekt ladeProjekt(const QString& pfad, QString* fehler)
    {
        QFile f(pfad);
        if (!f.open(QIODevice::ReadOnly))
        {
            if (fehler) *fehler = QStringLiteral("Projektdatei nicht lesbar: %1").arg(pfad);
            return {};
        }
        QJsonParseError pe;
        const QJsonDocument doc = QJsonDocument::fromJson(f.readAll(), &pe);
        if (pe.error != QJsonParseError::NoError || !doc.isObject())
        {
            if (fehler) *fehler = QStringLiteral("%1: %2").arg(pfad, pe.errorString());
            return {};
        }
        const QJsonObject o = doc.object();
        const QFileInfo fi(pfad);
        const QDir d(fi.absolutePath());
        auto absolut = [&d](const QString& p) { return QDir::cleanPath(d.absoluteFilePath(p)); };

        // Unbekannte Schlüssel werden überlesen (§7).
        Projekt p;
        p.pfad = fi.absoluteFilePath();
        p.ordner = d.absolutePath();
        p.schema = o.value(QStringLiteral("schema")).toInt(1);
        p.anwendung = o.value(QStringLiteral("anwendung")).toString();
        const QJsonObject start = o.value(QStringLiteral("start")).toObject();
        if (!start.value(QStringLiteral("exe")).toString().isEmpty())
            p.startExe = absolut(start.value(QStringLiteral("exe")).toString());
        const QJsonArray argumente = start.value(QStringLiteral("argumente")).toArray();
        for (const QJsonValue& a : argumente) p.startArgumente.append(a.toString());
        const QJsonArray listen = o.value(QStringLiteral("listen")).toArray();
        for (const QJsonValue& v : listen)
        {
            const QJsonObject l = v.toObject();
            const QString ort = l.value(QStringLiteral("ordner")).toString();
            if (ort.isEmpty()) continue;
            Projekt::Liste liste;
            liste.pfad = absolut(ort);
            if (!l.value(QStringLiteral("ablage")).toString().isEmpty())
                liste.ablage = absolut(l.value(QStringLiteral("ablage")).toString());
            liste.muster = l.value(QStringLiteral("muster")).toString();
            p.listen.append(liste);
        }
        // Ohne "listen": der Ordner der Projektdatei, Ablage sichttest-logs.
        if (p.listen.isEmpty()) p.listen.append({ p.ordner, QString(), QString() });
        p.gewichtung = o.value(QStringLiteral("gewichtung")).toObject();
        p.abbildung = o.value(QStringLiteral("abbildung")).toObject();
        return p;
    }

    QString findeProjektDatei(const QString& ordner)
    {
        if (ordner.isEmpty()) return {};
        QDir d(ordner);
        for (int n = 0; n < 16; ++n)
        {
            if (d.exists(kDateiName)) return d.absoluteFilePath(kDateiName);
            if (!d.cdUp()) break;
        }
        return {};
    }

    const Projekt& projekt()
    {
        return aktuelles();
    }

    void setzeProjekt(const Projekt& projekt)
    {
        aktuelles() = projekt;
    }

    QStringList protokolleDesProjekts(const Projekt& projekt)
    {
        QStringList gefunden;
        for (const Projekt::Liste& l : projekt.listen)
        {
            const QFileInfo fi(l.pfad);
            QStringList pfade;
            if (fi.isDir()) pfade = findeProtokolle(l.pfad, l.muster);
            else if (fi.isFile() && !lade(l.pfad).schritte.isEmpty()) pfade.append(l.pfad);
            for (const QString& p : std::as_const(pfade))
                if (!gefunden.contains(p)) gefunden.append(p);
        }
        return gefunden;
    }

    QString ablageVon(const Projekt& projekt, const QString& protokollPfad)
    {
        if (!projekt.gueltig()) return {};
        const QFileInfo fi(protokollPfad);
        for (const Projekt::Liste& l : projekt.listen)
            if (QFileInfo(l.pfad) == fi || QFileInfo(l.pfad) == QFileInfo(fi.absolutePath())) return l.ablage;
        return {};
    }

    bool abbildung(const Projekt& projekt, const QString& feld, QString& aktion, QStringList& schluessel)
    {
        QStringList teile = projekt.abbildung.value(feld).toString().split(QLatin1Char(' '), Qt::SkipEmptyParts);
        if (teile.isEmpty()) return false;
        aktion = teile.takeFirst();
        schluessel = teile;
        return true;
    }
}
