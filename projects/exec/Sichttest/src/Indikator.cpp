#include "Indikator.hpp"

#include "Protokoll.hpp"

#include <QDate>
#include <QDateTime>
#include <QDir>
#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>

namespace sichttest::indikator
{
    namespace
    {
        QJsonObject liesJson(const QString& pfad)
        {
            QFile f(pfad);
            if (!f.open(QIODevice::ReadOnly)) return {};
            const QJsonDocument doc = QJsonDocument::fromJson(f.readAll());
            return doc.isObject() ? doc.object() : QJsonObject();
        }

        // Zwei Ebenen: Objekte (areas, retest_steps) werden je Schlüssel gemischt, alles
        // andere wird überschrieben.
        void mische(QJsonObject& ziel, const QJsonObject& quelle)
        {
            for (auto it = quelle.constBegin(); it != quelle.constEnd(); ++it)
            {
                if (it->isObject() && ziel.value(it.key()).isObject())
                {
                    QJsonObject gemischt = ziel.value(it.key()).toObject();
                    const QJsonObject dazu = it->toObject();
                    for (auto j = dazu.constBegin(); j != dazu.constEnd(); ++j) gemischt.insert(j.key(), j.value());
                    ziel.insert(it.key(), gemischt);
                }
                else
                    ziel.insert(it.key(), it.value());
            }
        }

        // Ein Zeitstempel ohne Zone gilt als Ortszeit; verglichen werden Zeitpunkte, nicht Texte.
        QDateTime zeit(const QString& iso)
        {
            return QDateTime::fromString(iso, Qt::ISODate);
        }
    }

    Gewichtung lade(const Projekt& projekt)
    {
        Gewichtung g;
        g.aktiv = projekt.gueltig() && projekt.gewichtungGenannt;
        if (!g.aktiv) return g;
        const QDir root(projekt.root);
        const QJsonArray hand = projekt.gewichtung.value(QStringLiteral("hand")).toArray();
        for (const QJsonValue& v : hand) mische(g.hand, liesJson(root.absoluteFilePath(v.toString())));
        const QJsonArray git = projekt.gewichtung.value(QStringLiteral("git")).toArray();
        for (const QJsonValue& v : git) mische(g.git, liesJson(root.absoluteFilePath(v.toString())));
        return g;
    }

    Dringlichkeit bewerte(const QJsonObject& schritt, const Gewichtung& gewichtung,
                          const QHash<QString, ergebnis::LetztesErgebnis>& letzte)
    {
        if (!gewichtung.aktiv || istVerwaist(schritt)) return {};
        // Eine Vorbereitung ist kein Testfall und kommt nie in die Ergebnis-DB.
        if (istVorbereitung(schritt)) return { 0.0, QStringLiteral("Vorbereitung, kein Testfall.") };

        const QString id = schritt.value(QStringLiteral("id")).toString();
        const QJsonObject nachtest = gewichtung.hand.value(QStringLiteral("retest_steps")).toObject();
        if (nachtest.contains(id))
            return { 1.0, QStringLiteral("Nachtest angewiesen: %1").arg(nachtest.value(id).toString()) };
        const auto it = letzte.constFind(id);
        if (it == letzte.constEnd())
            return { 1.0, QStringLiteral("noch nie verifiziert (kein Pass oder Fail in der Ergebnis-DB).") };
        if (it->ergebnis == QLatin1String("fail"))
            return { 1.0, QStringLiteral("letzter Test FAIL (%1) — Fix nachtesten.").arg(it->gestartet.left(10)) };

        // Letzter Pass: dringend nur, wenn eine Area des Schritts seither geändert wurde — das
        // höchste solche Gewicht zählt.
        Dringlichkeit d{ 0.0, {} };
        const QJsonObject areas = gewichtung.hand.value(QStringLiteral("areas")).toObject();
        const QJsonArray eigene = schritt.value(QStringLiteral("areas")).toArray();
        // Gegen diesen Build fiel der Pass: der Build am Urteil, sonst der des Laufs, sonst sein Start.
        const QString bezugText = it->buildStempel.isEmpty() ? it->gestartet : it->buildStempel;
        const QDateTime bezug = zeit(bezugText);
        const QDate getestet = QDate::fromString(it->gestartet.left(10), Qt::ISODate);
        for (const QJsonValue& av : eigene)
        {
            const QJsonObject a = areas.value(av.toString()).toObject();
            if (a.isEmpty()) continue;
            const QString geaendert = a.value(QStringLiteral("changed")).toString();
            const double w = a.value(QStringLiteral("weight")).toDouble();
            // Mit Uhrzeit gegen den Build des Urteils, mit reinem Datum tageweise gegen den Lauf.
            bool neuer = false;
            if (geaendert.contains(QLatin1Char('T')))
                neuer = zeit(geaendert).isValid() && bezug.isValid() && zeit(geaendert) > bezug;
            else
            {
                const QDate tag = QDate::fromString(geaendert, Qt::ISODate);
                neuer = tag.isValid() && getestet.isValid() && tag > getestet;
            }
            if (neuer && w > d.u)
            {
                d.u = w;
                d.warum = QStringLiteral("Änderung »%1« vom %2 (Gewicht %3) nach letztem Pass (%4): %5")
                              .arg(av.toString(), geaendert).arg(w)
                              .arg(it->gestartet.left(10), a.value(QStringLiteral("note")).toString());
            }
        }
        // git-Schicht: ein Commit, der die Pfade einer Area nach dem Build des letzten Pass berührt
        // hat, hebt auf auto_weight — nie höher als die Hand-Schicht es täte, nie senkend.
        const QJsonObject gitAreas = gewichtung.git.value(QStringLiteral("areas")).toObject();
        const double autoVorgabe = gewichtung.hand.value(QStringLiteral("auto_weight")).toDouble(0.5);
        for (const QJsonValue& av : eigene)
        {
            const QJsonObject ga = gitAreas.value(av.toString()).toObject();
            const QString geaendert = ga.value(QStringLiteral("changed")).toString();
            if (geaendert.isEmpty() || !zeit(geaendert).isValid() || !bezug.isValid() || !(zeit(geaendert) > bezug))
                continue;
            const double w = areas.value(av.toString()).toObject().value(QStringLiteral("auto_weight")).toDouble(autoVorgabe);
            if (w > d.u)
            {
                d.u = w;
                d.warum = QStringLiteral("Code-Änderung (git) »%1« vom %2 (Commit %3, z. B. %4) nach dem Build des letzten Pass.")
                              .arg(av.toString(), geaendert.left(16), ga.value(QStringLiteral("commit")).toString(),
                                   ga.value(QStringLiteral("file")).toString());
            }
        }
        if (d.u <= 0.0)
            d.warum = QStringLiteral("letzter Test Pass (%1), keine neuere Änderung an den Areas dieses Schritts.")
                          .arg(it->gestartet.left(10));
        return d;
    }

    QString marke(double u)
    {
        return u >= kRot ? QStringLiteral("🔴") : u >= kGelb ? QStringLiteral("🟡") : QString();
    }
}
