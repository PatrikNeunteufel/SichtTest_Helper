#include "Ergebnis.hpp"

#include "Protokoll.hpp"

#include <QDir>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QSet>
#include <QSqlDatabase>
#include <QSqlError>
#include <QSqlQuery>
#include <QVariant>

namespace sichttest::ergebnis
{
    namespace
    {
        const QString kVerbindung = QStringLiteral("sichttest_ergebnisse");

        // Bestandszeilen tragen den vollen Pfad des Testlogs (Comm Studio bis 2026-10-10). Sie
        // werden einmalig auf den Dateinamen gekürzt. Tragen zwei Zeilen danach denselben Namen
        // (die Ablage wurde einmal kopiert und weiterbenutzt), bleibt die jüngere.
        void stelleUm(QSqlDatabase& db)
        {
            struct Zeile { int id; QString logDatei; };
            QList<Zeile> zeilen;
            bool noetig = false;
            {
                QSqlQuery q(db);
                if (!q.exec(QStringLiteral("SELECT lauf_id, log_datei FROM testlauf ORDER BY lauf_id DESC"))) return;
                while (q.next())
                {
                    zeilen.append({ q.value(0).toInt(), q.value(1).toString() });
                    noetig = noetig || zeilen.last().logDatei.contains(QLatin1Char('/'))
                             || zeilen.last().logDatei.contains(QLatin1Char('\\'));
                }
            }
            if (!noetig) return;
            db.transaction();
            QSet<QString> vergeben;
            QList<Zeile> bleiben;
            for (const Zeile& z : std::as_const(zeilen))
            {
                const QString name = QFileInfo(z.logDatei).fileName();
                if (!vergeben.contains(name.toLower()))
                {
                    vergeben.insert(name.toLower());
                    bleiben.append({ z.id, name });
                    continue;
                }
                QSqlQuery weg(db);
                weg.prepare(QStringLiteral("DELETE FROM testschritt WHERE lauf_id = ?"));
                weg.addBindValue(z.id);
                weg.exec();
                weg.prepare(QStringLiteral("DELETE FROM testlauf WHERE lauf_id = ?"));
                weg.addBindValue(z.id);
                weg.exec();
            }
            for (const Zeile& z : std::as_const(bleiben))
            {
                QSqlQuery um(db);
                um.prepare(QStringLiteral("UPDATE testlauf SET log_datei = ? WHERE lauf_id = ? AND log_datei <> ?"));
                um.addBindValue(z.logDatei);
                um.addBindValue(z.id);
                um.addBindValue(z.logDatei);
                um.exec();
            }
            db.commit();
        }

        // Liefert eine offene Verbindung auf die DB der Ablage oder setzt *fehler.
        QSqlDatabase oeffne(const QString& ablage, QString* fehler)
        {
            QSqlDatabase db = QSqlDatabase::contains(kVerbindung)
                ? QSqlDatabase::database(kVerbindung, false)
                : QSqlDatabase::addDatabase(QStringLiteral("QSQLITE"), kVerbindung);
            const QString pfad = dbPfad(ablage);
            if (db.isOpen() && db.databaseName() != pfad) db.close();   // andere Ablage
            if (db.isOpen()) return db;

            QDir().mkpath(ablage);
            db.setDatabaseName(pfad);
            if (!db.open())
            {
                if (fehler) *fehler = QStringLiteral("Ergebnis-DB lässt sich nicht öffnen (%1): %2")
                                          .arg(QDir::toNativeSeparators(pfad), db.lastError().text());
                return {};
            }
            // Das Schema des Comm Studio, Wort für Wort; was dazukam, als stilles ALTER
            // (scheitert, wenn die Spalte schon da ist).
            QSqlQuery q(db);
            q.exec(QStringLiteral(
                "CREATE TABLE IF NOT EXISTS testlauf ("
                " lauf_id INTEGER PRIMARY KEY AUTOINCREMENT,"
                " protokoll TEXT,"
                " protokoll_datei TEXT,"
                " log_datei TEXT NOT NULL UNIQUE,"
                " build_exe TEXT,"
                " build_timestamp TEXT,"
                " started TEXT,"
                " finished TEXT,"
                " pass INTEGER, fail INTEGER, skip INTEGER, open INTEGER)"));
            q.exec(QStringLiteral("ALTER TABLE testlauf ADD COLUMN pass_remark INTEGER"));
            q.exec(QStringLiteral(
                "CREATE TABLE IF NOT EXISTS protokoll ("
                " datei TEXT PRIMARY KEY,"
                " titel TEXT,"
                " status TEXT,"
                " registriert TEXT)"));
            q.exec(QStringLiteral(
                "CREATE TABLE IF NOT EXISTS testschritt ("
                " lauf_id INTEGER NOT NULL,"
                " step_id TEXT NOT NULL,"
                " section TEXT,"
                " title TEXT,"
                " result TEXT,"
                " remark TEXT,"
                " screenshots TEXT,"
                " history TEXT,"
                " PRIMARY KEY (lauf_id, step_id),"
                " FOREIGN KEY (lauf_id) REFERENCES testlauf(lauf_id))"));
            q.exec(QStringLiteral("ALTER TABLE testschritt ADD COLUMN history TEXT"));
            // Der Build am Urteil (Teil A): je Schritt, leer bei Urteilen ohne eigenen Stempel.
            q.exec(QStringLiteral("ALTER TABLE testschritt ADD COLUMN build_exe TEXT"));
            q.exec(QStringLiteral("ALTER TABLE testschritt ADD COLUMN build_timestamp TEXT"));
            stelleUm(db);
            return db;
        }

        QString alsText(const QJsonValue& feld)
        {
            return QString::fromUtf8(QJsonDocument(feld.toArray()).toJson(QJsonDocument::Compact));
        }
    }

    QString dbPfad(const QString& ablage)
    {
        return QDir(ablage).filePath(QStringLiteral("testergebnisse.sqlite"));
    }

    QString spiegele(const QString& logPfad, const QJsonObject& log)
    {
        QString fehler;
        QSqlDatabase db = oeffne(QFileInfo(logPfad).absolutePath(), &fehler);
        if (!db.isOpen()) return fehler;

        const QJsonObject build = log.value(QStringLiteral("build")).toObject();
        const QJsonObject summe = log.value(QStringLiteral("summary")).toObject();
        const QString name = QFileInfo(logPfad).fileName();

        // Gesucht wird über den Namen, ohne Groß-/Kleinschreibung — nicht per LIKE, der Name
        // enthält `_`.
        int laufId = -1;
        {
            QSqlQuery sel(db);
            if (sel.exec(QStringLiteral("SELECT lauf_id, log_datei FROM testlauf")))
                while (sel.next())
                    if (QFileInfo(sel.value(1).toString()).fileName().compare(name, Qt::CaseInsensitive) == 0)
                    {
                        laufId = sel.value(0).toInt();
                        break;
                    }
        }
        db.transaction();
        QSqlQuery q(db);
        if (laufId > 0)
            q.prepare(QStringLiteral(
                "UPDATE testlauf SET protokoll=?, protokoll_datei=?, build_exe=?, build_timestamp=?, started=?,"
                " finished=?, pass=?, pass_remark=?, fail=?, skip=?, open=?, log_datei=? WHERE lauf_id=?"));
        else
            q.prepare(QStringLiteral(
                "INSERT INTO testlauf (protokoll, protokoll_datei, build_exe, build_timestamp, started, finished,"
                " pass, pass_remark, fail, skip, open, log_datei) VALUES (?,?,?,?,?,?,?,?,?,?,?,?)"));
        q.addBindValue(log.value(QStringLiteral("protocol")).toString());
        q.addBindValue(log.value(QStringLiteral("protocol_file")).toString());
        q.addBindValue(build.value(QStringLiteral("exe")).toString());
        q.addBindValue(build.value(QStringLiteral("exe_timestamp")).toString());
        q.addBindValue(log.value(QStringLiteral("started")).toString());
        q.addBindValue(log.value(QStringLiteral("finished")).toString());
        q.addBindValue(summe.value(QStringLiteral("pass")).toInt());
        q.addBindValue(summe.value(QStringLiteral("pass_remark")).toInt());
        q.addBindValue(summe.value(QStringLiteral("fail")).toInt());
        q.addBindValue(summe.value(QStringLiteral("skip")).toInt());
        q.addBindValue(summe.value(QStringLiteral("open")).toInt());
        q.addBindValue(name);
        if (laufId > 0) q.addBindValue(laufId);
        if (!q.exec())
        {
            fehler = q.lastError().text();
            db.rollback();
            return fehler;
        }
        if (laufId <= 0) laufId = q.lastInsertId().toInt();

        QSqlQuery weg(db);
        weg.prepare(QStringLiteral("DELETE FROM testschritt WHERE lauf_id = ?"));
        weg.addBindValue(laufId);
        weg.exec();
        QSqlQuery ein(db);
        ein.prepare(QStringLiteral(
            "INSERT INTO testschritt (lauf_id, step_id, section, title, result, remark, screenshots, history,"
            " build_exe, build_timestamp) VALUES (?,?,?,?,?,?,?,?,?,?)"));
        int n = 0;
        const QJsonArray schritte = log.value(QStringLiteral("steps")).toArray();
        for (const QJsonValue& v : schritte)
        {
            const QJsonObject s = v.toObject();
            ++n;
            if (istVorbereitung(s)) continue;   // ohne Urteil, gehört nicht in die Historie
            const QJsonObject b = s.value(QStringLiteral("build")).toObject();
            ein.addBindValue(laufId);
            ein.addBindValue(s.value(QStringLiteral("id")).toString(QStringLiteral("step%1").arg(n)));
            ein.addBindValue(s.value(QStringLiteral("section")).toString());
            ein.addBindValue(s.value(QStringLiteral("title")).toString());
            ein.addBindValue(s.value(QStringLiteral("result")).toString());
            ein.addBindValue(s.value(QStringLiteral("remark")).toString());
            ein.addBindValue(alsText(s.value(QStringLiteral("screenshots"))));
            ein.addBindValue(alsText(s.value(QStringLiteral("history"))));
            ein.addBindValue(b.value(QStringLiteral("exe")).toString());
            ein.addBindValue(b.value(QStringLiteral("exe_timestamp")).toString());
            if (!ein.exec())
            {
                fehler = ein.lastError().text();
                db.rollback();
                return fehler;
            }
        }
        db.commit();
        return {};
    }

    QList<LaufZeile> laeufe(const QString& ablage, const QString& protokoll)
    {
        QList<LaufZeile> zeilen;
        if (!QFileInfo::exists(dbPfad(ablage))) return zeilen;
        QSqlDatabase db = oeffne(ablage, nullptr);
        if (!db.isOpen()) return zeilen;
        QSqlQuery q(db);
        q.prepare(QStringLiteral(
            "SELECT started, finished, build_timestamp, log_datei, pass, pass_remark, fail, skip, open"
            " FROM testlauf WHERE protokoll = ? ORDER BY started DESC"));
        q.addBindValue(protokoll.isNull() ? QString::fromLatin1("") : protokoll);
        if (!q.exec()) return zeilen;
        while (q.next())
        {
            LaufZeile z;
            z.gestartet = q.value(0).toString();
            z.beendet = q.value(1).toString();
            z.buildStempel = q.value(2).toString();
            z.logDatei = q.value(3).toString();
            z.pass = q.value(4).toInt();
            z.passBefund = q.value(5).toInt();
            z.fail = q.value(6).toInt();
            z.skip = q.value(7).toInt();
            z.offen = q.value(8).toInt();
            zeilen.append(z);
        }
        return zeilen;
    }

    QList<SchrittSumme> schrittSummen(const QString& ablage)
    {
        QList<SchrittSumme> summen;
        if (!QFileInfo::exists(dbPfad(ablage))) return summen;
        QSqlDatabase db = oeffne(ablage, nullptr);
        if (!db.isOpen()) return summen;
        QSqlQuery q(db);
        if (!q.exec(QStringLiteral(
                "SELECT step_id, SUM(result IN ('pass','pass_remark')), SUM(result = 'fail')"
                " FROM testschritt GROUP BY step_id")))
            return summen;
        while (q.next()) summen.append({ q.value(0).toString(), q.value(1).toInt(), q.value(2).toInt() });
        return summen;
    }

    QHash<QString, LetztesErgebnis> letzteErgebnisse(const QString& ablage)
    {
        QHash<QString, LetztesErgebnis> letzte;
        if (!QFileInfo::exists(dbPfad(ablage))) return letzte;
        QSqlDatabase db = oeffne(ablage, nullptr);
        if (!db.isOpen()) return letzte;
        QSqlQuery q(db);
        // Spätere Läufe überschreiben frühere. Der Build am Urteil gilt, sonst der des Laufs.
        if (!q.exec(QStringLiteral(
                "SELECT s.step_id, s.result, l.started,"
                " CASE WHEN s.build_timestamp IS NULL OR s.build_timestamp = '' THEN l.build_timestamp"
                "      ELSE s.build_timestamp END"
                " FROM testschritt s JOIN testlauf l ON l.lauf_id = s.lauf_id"
                " WHERE s.result IN ('pass','fail','pass_remark') ORDER BY l.started ASC")))
            return letzte;
        while (q.next())
            letzte.insert(q.value(0).toString(), { q.value(1).toString(), q.value(2).toString(), q.value(3).toString() });
        return letzte;
    }

    void schliesse()
    {
        if (!QSqlDatabase::contains(kVerbindung)) return;
        {
            QSqlDatabase db = QSqlDatabase::database(kVerbindung, false);
            db.close();
        }
        QSqlDatabase::removeDatabase(kVerbindung);
    }
}
