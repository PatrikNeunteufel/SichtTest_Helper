// Sichttest — eigenständiges Fenster zum Abhaken einer Sichttest-Liste (S93).
//
// Vorbild ist das Abnahme-Protokoll des UART-Studios (TestProtokollWindow):
// Schritt für Schritt Pass / Pass mit Befund / Fail / Überspringen, dazu eine
// Bemerkung und Screenshots aus der Zwischenablage. Hier ohne die Kopplung an
// eine Anwendung — das Fenster läuft neben dem Programm, das geprüft wird.
//
// Aufruf:  Sichttest [datei|ordner]
//          Sichttest --pruefe <datei>     (Schritte auf die Standardausgabe)
//          Sichttest --schnapp <png> [datei|ordner]   (Bild des Fensters)
//          Sichttest --selbsttest <leerer ordner>     (Exit = gescheiterte Prüfungen)
//          Sichttest --steuerung [datei|ordner]       (so startet ihn die DLL der Anwendung)

#include "Projekt.hpp"
#include "Protokoll.hpp"
#include "Steuerung.hpp"

#include <QAction>
#include <QApplication>
#include <QHash>
#include <QSignalBlocker>
#include <QCheckBox>
#include <QClipboard>
#include <QCloseEvent>
#include <QComboBox>
#include <QDateTime>
#include <QDesktopServices>
#include <QDir>
#include <QDragEnterEvent>
#include <QElapsedTimer>
#include <QFile>
#include <QFileDialog>
#include <QFileInfo>
#include <QHBoxLayout>
#include <QImage>
#include <QImageReader>
#include <QJsonDocument>
#include <QKeyEvent>
#include <QLabel>
#include <QListWidget>
#include <QMessageBox>
#include <QMimeData>
#include <QPlainTextEdit>
#include <QProcess>
#include <QPushButton>
#include <QSaveFile>
#include <QSet>
#include <QSettings>
#include <QShortcut>
#include <QSplitter>
#include <QTextBrowser>
#include <QTextStream>
#include <QThread>
#include <QTimer>
#include <QUrl>
#include <QVBoxLayout>

#include <cstdio>

namespace
{
    using namespace sichttest;

    constexpr int kRolleSchritt = Qt::UserRole;

    QString farbe(const QString& ergebnis)
    {
        if (ergebnis == QLatin1String("pass"))        return QStringLiteral("#3fa34d");
        if (ergebnis == QLatin1String("pass_remark")) return QStringLiteral("#c9a227");
        if (ergebnis == QLatin1String("fail"))        return QStringLiteral("#d64545");
        if (ergebnis == QLatin1String("skip"))        return QStringLiteral("#8a8a8a");
        return QString();
    }

    // Exe-Angabe des Protokolls auflösen: absolut, sonst vom Ordner des
    // Protokolls aufwärts gesucht (die Listen nennen den Pfad ab Repo-Wurzel).
    QString findeExeDatei(const QString& angabe, const QString& protokollPfad)
    {
        if (angabe.isEmpty()) return {};
        const QString sauber = QDir::fromNativeSeparators(angabe);
        if (QFileInfo(sauber).isAbsolute())
            return QFileInfo::exists(sauber) ? sauber : QString();
        QDir d(QFileInfo(protokollPfad).absolutePath());
        for (int n = 0; n < 8; ++n)
        {
            if (d.exists(sauber)) return d.absoluteFilePath(sauber);
            if (!d.cdUp()) break;
        }
        return {};
    }

    QString findeHandoverOrdner()
    {
        for (const QString& start : { QDir::currentPath(), QCoreApplication::applicationDirPath() })
        {
            QDir d(start);
            for (int n = 0; n < 10; ++n)
            {
                if (d.exists(QStringLiteral(".claude/handover")))
                    return d.absoluteFilePath(QStringLiteral(".claude/handover"));
                if (!d.cdUp()) break;
            }
        }
        return {};
    }

    class Fenster : public QWidget
    {
    public:
        Fenster()
        {
            setWindowTitle(QStringLiteral("Sichttest"));
            setAcceptDrops(true);
            resize(1040, 680);
            baue();
        }

        void starte(const QString& ziel)
        {
            QSettings einstellungen;
            QString ordner;
            QString datei;
            if (!ziel.isEmpty())
            {
                const QFileInfo fi(ziel);
                if (fi.isDir()) ordner = fi.absoluteFilePath();
                else if (fi.fileName().endsWith(QLatin1String(".projekt.json"), Qt::CaseInsensitive))
                {
                    // Sichttest <projektdatei>: deren Listen (Konzept §7).
                    m_projektGenannt = fi.absoluteFilePath();
                    ordner = fi.absolutePath();
                }
                else { datei = fi.absoluteFilePath(); ordner = fi.absolutePath(); }
            }
            else
            {
                ordner = einstellungen.value(QStringLiteral("ordner")).toString();
                datei = einstellungen.value(QStringLiteral("protokoll")).toString();
                if (ordner.isEmpty() || !QFileInfo::exists(ordner))
                {
                    ordner = findeHandoverOrdner();
                    datei.clear();
                }
            }
            setzeOrdner(ordner, datei);

            // Der Tester lauscht immer (Konzept §4); der Selbsttest ruft starte() nicht
            // und nimmt einen eigenen Kanal.
            m_steuerung.lausche(KanalServer::kanalName());
        }

        // Selbsttest: eine kleine Liste im Ordner anlegen, sie über die Knöpfe
        // bewerten und nachsehen, was in Log und Report steht. Gibt die Zahl
        // der gescheiterten Prüfungen zurück.
        int selbsttest(const QString& ordner)
        {
            QTextStream aus(stdout);
            aus.setEncoding(QStringConverter::Utf8);
            int fehler = 0;
            auto pruefe = [&](bool gut, const QString& was) {
                aus << (gut ? "ok   " : "FAIL ") << was << "\n";
                if (!gut) ++fehler;
            };
            auto klicke = [this](const QString& text) {
                const auto knoepfe = findChildren<QPushButton*>();
                for (QPushButton* k : knoepfe)
                    if (k->text() == text) { k->click(); return true; }
                return false;
            };

            QDir d(ordner);
            d.mkpath(QStringLiteral("."));
            QDir(d.filePath(QStringLiteral("sichttest-logs"))).removeRecursively();
            const QString liste = d.filePath(QStringLiteral("Selbsttest.md"));
            {
                QFile f(liste);
                if (!f.open(QIODevice::WriteOnly)) { aus << "Ordner nicht beschreibbar\n"; return 1; }
                f.write("# Selbsttest\n\n## A. Erster\n\n- [ ] **A1 Eins:** Text eins,\n"
                        "      zweite Zeile.\n- [x] **A2 Zwei:** schon abgehakt.\n\n"
                        "## B. Zweiter\n\n- [ ] **B1 Drei:** Text drei.\n- [ ] ohne Fettdruck. Rest.\n");
            }
            setzeOrdner(d.absolutePath(), liste);
            pruefe(m_p.schritte.size() == 4, QStringLiteral("vier Schritte gelesen"));
            pruefe(m_idx == 0, QStringLiteral("erster offener Schritt gezeigt"));
            pruefe(schritt(1).value(QStringLiteral("result")).toString() == QLatin1String("pass"),
                   QStringLiteral("Haken [x] als Pass übernommen"));
            pruefe(schritt(0).value(QStringLiteral("text")).toString() == QLatin1String("Text eins, zweite Zeile."),
                   QStringLiteral("Fortsetzungszeile gehört zum Punkt"));
            pruefe(!QFileInfo::exists(m_logPfad), QStringLiteral("vor der ersten Bewertung liegt kein Log"));

            pruefe(klicke(QStringLiteral("✓ Pass")), QStringLiteral("Knopf Pass gefunden"));
            pruefe(schritt(0).value(QStringLiteral("result")).toString() == QLatin1String("pass")
                   && m_idx == 2, QStringLiteral("Pass gesetzt, weiter zum nächsten offenen (B1)"));

            m_bemerkung->setPlainText(QStringLiteral("flackert beim Wechsel"));
            QImage bild(40, 30, QImage::Format_RGB32);
            bild.fill(Qt::red);
            haengeBildAn(bild);
            klicke(QStringLiteral("✗ Fail"));
            pruefe(schritt(2).value(QStringLiteral("result")).toString() == QLatin1String("fail")
                   && m_idx == 3, QStringLiteral("Fail mit Bemerkung gesetzt, weiter zu P4"));
            const QString bildName = schritt(2).value(QStringLiteral("screenshots")).toArray().at(0).toString();
            pruefe(QFileInfo::exists(QDir(logOrdner(liste)).filePath(bildName)),
                   QStringLiteral("Screenshot liegt als Datei: %1").arg(bildName));

            pruefe(schritt(3).value(QStringLiteral("id")).toString() == QLatin1String("P4"),
                   QStringLiteral("Punkt ohne Kennung heißt P4"));
            klicke(QStringLiteral("↷ Überspringen"));
            pruefe(schritt(3).value(QStringLiteral("result")).toString() == QLatin1String("skip"),
                   QStringLiteral("Überspringen gesetzt"));

            const QJsonObject log = leseJson(m_logPfad);
            const QJsonObject summe = log.value(QStringLiteral("summary")).toObject();
            pruefe(summe.value(QStringLiteral("pass")).toInt() == 2
                   && summe.value(QStringLiteral("fail")).toInt() == 1
                   && summe.value(QStringLiteral("skip")).toInt() == 1
                   && summe.value(QStringLiteral("open")).toInt() == 0,
                   QStringLiteral("Summe im Log: 2 Pass, 1 Fail, 1 übersprungen, 0 offen"));
            pruefe(!log.value(QStringLiteral("finished")).toString().isEmpty(),
                   QStringLiteral("Lauf als beendet vermerkt"));
            QFile rf(reportPfad());
            const QString report = rf.open(QIODevice::ReadOnly) ? QString::fromUtf8(rf.readAll()) : QString();
            pruefe(report.contains(QStringLiteral("### B1 Drei"))
                   && report.contains(QStringLiteral("flackert beim Wechsel"))
                   && report.contains(bildName),
                   QStringLiteral("Report nennt den Fail mit Bemerkung und Bild"));

            // Wiederaufnahme: neu einlesen setzt denselben Lauf fort.
            const QString lauf = m_logPfad;
            setzeOrdner(d.absolutePath(), liste);
            pruefe(m_logPfad == lauf
                   && schritt(2).value(QStringLiteral("remark")).toString() == QLatin1String("flackert beim Wechsel"),
                   QStringLiteral("Neu-Einlesen setzt den Lauf fort, Bemerkung ist da"));
            QFile original(liste);
            pruefe(original.open(QIODevice::ReadOnly) && original.readAll().contains("- [ ] **A1 Eins:**"),
                   QStringLiteral("die Liste selbst ist unverändert"));

            selbsttestListen(d, pruefe);
#ifdef Q_OS_WIN
            fehler += selbsttestSteuerung(aus);
            selbsttestAktionen(d, pruefe, klicke);
            selbsttestProjekt(d, pruefe, klicke);
#endif

            aus << (fehler == 0 ? "Selbsttest bestanden" : "Selbsttest GESCHEITERT") << "\n";
            return fehler;
        }

    protected:
        void closeEvent(QCloseEvent* e) override
        {
            if (merkeBemerkung()) schreibe();
            // Beim Schließen eines begonnenen Laufs die Nachbereitung anbieten; läuft sie,
            // schließt das Fenster danach von selbst.
            if (!m_schliesstDanach && hatBewertung() && bieteNachbereitungAn())
            {
                m_schliesstDanach = true;
                e->ignore();
                return;
            }
            QSettings().setValue(QStringLiteral("geometrie"), saveGeometry());
            e->accept();
        }

        // Strg+V im Bemerkungsfeld hängt ein Bild aus der Zwischenablage an;
        // Text fügt das Feld weiter selbst ein.
        bool eventFilter(QObject* o, QEvent* e) override
        {
            if (o == m_bemerkung && e->type() == QEvent::KeyPress)
            {
                auto* k = static_cast<QKeyEvent*>(e);
                const QMimeData* md = QGuiApplication::clipboard()->mimeData();
                if (k->matches(QKeySequence::Paste) && md && md->hasImage() && !md->hasText())
                {
                    haengeAusZwischenablageAn();
                    return true;
                }
            }
            return QWidget::eventFilter(o, e);
        }

        void dragEnterEvent(QDragEnterEvent* e) override
        {
            if (e->mimeData()->hasUrls() && m_idx >= 0) e->acceptProposedAction();
        }

        void dropEvent(QDropEvent* e) override
        {
            const QList<QUrl> urls = e->mimeData()->urls();
            for (const QUrl& u : urls)
            {
                const QImage bild(u.toLocalFile());
                if (!bild.isNull()) haengeBildAn(bild);
            }
        }

    private:
        void baue()
        {
            auto* lay = new QVBoxLayout(this);

            auto* kopf = new QHBoxLayout();
            kopf->addWidget(new QLabel(QStringLiteral("Protokoll:"), this));
            m_combo = new QComboBox(this);
            m_combo->setFocusPolicy(Qt::StrongFocus);
            kopf->addWidget(m_combo, 1);
            auto* neuLesen = new QPushButton(QStringLiteral("↻"), this);
            neuLesen->setFixedWidth(32);
            neuLesen->setToolTip(QStringLiteral("Ordner und Protokoll neu einlesen — Bewertungen bleiben."));
            kopf->addWidget(neuLesen);
            auto* ordnerBtn = new QPushButton(QStringLiteral("Ordner…"), this);
            kopf->addWidget(ordnerBtn);
            auto* neuerLauf = new QPushButton(QStringLiteral("▶ Neuer Lauf"), this);
            neuerLauf->setToolTip(QStringLiteral(
                "Beginnt einen frischen Lauf für dieses Protokoll. Der bisherige Lauf bleibt als Datei liegen."));
            kopf->addWidget(neuerLauf);
            m_vorn = new QCheckBox(QStringLiteral("Im Vordergrund"), this);
            m_vorn->setToolTip(QStringLiteral("Hält das Fenster über dem Programm, das geprüft wird."));
            kopf->addWidget(m_vorn);
            lay->addLayout(kopf);

            m_kopfzeile = new QLabel(this);
            m_kopfzeile->setWordWrap(true);
            m_kopfzeile->setTextInteractionFlags(Qt::TextSelectableByMouse);
            lay->addWidget(m_kopfzeile);

            auto* split = new QSplitter(Qt::Horizontal, this);
            lay->addWidget(split, 1);

            auto* links = new QWidget(split);
            auto* linksLay = new QVBoxLayout(links);
            linksLay->setContentsMargins(0, 0, 0, 0);
            m_liste = new QListWidget(links);
            linksLay->addWidget(m_liste, 1);
            m_nurOffene = new QCheckBox(QStringLiteral("Nur offene und Fail zeigen"), links);
            linksLay->addWidget(m_nurOffene);

            auto* rechts = new QWidget(split);
            auto* col = new QVBoxLayout(rechts);
            col->setContentsMargins(8, 0, 0, 0);

            m_titel = new QLabel(rechts);
            m_titel->setWordWrap(true);
            m_titel->setStyleSheet(QStringLiteral("font-weight:bold; font-size:14pt;"));
            col->addWidget(m_titel);
            m_stand = new QLabel(rechts);
            col->addWidget(m_stand);

            m_text = new QTextBrowser(rechts);
            // Links `tab:<Titel>` und `sql:<Abfrage>` der Listen des Comm Studio lösen Aktionen aus.
            m_text->setOpenLinks(false);
            connect(m_text, &QTextBrowser::anchorClicked, this, [this](const QUrl& url) { folgeLink(url); });
            col->addWidget(m_text, 2);

            col->addWidget(new QLabel(
                QStringLiteral("Bemerkung (bei Fail: was war erwartet, was ist passiert?):"), rechts));
            m_bemerkung = new QPlainTextEdit(rechts);
            m_bemerkung->installEventFilter(this);
            col->addWidget(m_bemerkung, 1);

            auto* bildZeile = new QHBoxLayout();
            auto* bildBtn = new QPushButton(QStringLiteral("📋 Screenshot anhängen"), rechts);
            bildBtn->setToolTip(QStringLiteral(
                "Win+Shift+S, Bereich wählen, dann hier klicken (oder Strg+Shift+V, oder Strg+V im "
                "Bemerkungsfeld). Bilddateien lassen sich auch ins Fenster ziehen."));
            bildZeile->addWidget(bildBtn);
            m_bilder = new QListWidget(rechts);
            m_bilder->setFlow(QListView::LeftToRight);
            m_bilder->setFixedHeight(34);
            m_bilder->setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
            m_bilder->setContextMenuPolicy(Qt::ActionsContextMenu);
            auto* loesche = new QAction(QStringLiteral("Screenshot löschen"), m_bilder);
            loesche->setShortcut(QKeySequence::Delete);
            loesche->setShortcutContext(Qt::WidgetShortcut);
            m_bilder->addAction(loesche);
            bildZeile->addWidget(m_bilder, 1);
            col->addLayout(bildZeile);

            // Aktionen des Punkts (Konzept §6): laufen nie von selbst, nur auf den Knopf.
            auto* aktionsZeile = new QHBoxLayout();
            m_herstellen = new QPushButton(QStringLiteral("▶ Herstellen"), rechts);
            m_vorbereitungBtn = new QPushButton(QStringLiteral("↺ Vorbereitung"), rechts);
            m_vorbereitungBtn->setToolTip(QStringLiteral(
                "Wiederholt die Vorbereitung dieses Abschnitts — hast du etwas verstellt, lade die Vorlage einfach neu."));
            m_aktionStand = new QLabel(rechts);
            m_aktionStand->setWordWrap(true);
            m_aktionStand->setTextFormat(Qt::RichText);
            m_aktionStand->setSizePolicy(QSizePolicy::Ignored, QSizePolicy::Preferred);
            aktionsZeile->addWidget(m_herstellen);
            aktionsZeile->addWidget(m_vorbereitungBtn);
            aktionsZeile->addWidget(m_aktionStand, 1);
            col->addLayout(aktionsZeile);
            m_neuFrist.setSingleShot(true);

            auto* knoepfe = new QHBoxLayout();
            auto* zurueck = new QPushButton(QStringLiteral("◀ Zurück"), rechts);
            auto* weiter = new QPushButton(QStringLiteral("Weiter ▶"), rechts);
            auto* offenBtn = new QPushButton(QStringLiteral("○ Offen"), rechts);
            offenBtn->setToolTip(QStringLiteral("Nimmt die Bewertung zurück."));
            auto* skip = new QPushButton(QStringLiteral("↷ Überspringen"), rechts);
            auto* fail = new QPushButton(QStringLiteral("✗ Fail"), rechts);
            auto* passBefund = new QPushButton(QStringLiteral("✓⚠ Pass mit Befund"), rechts);
            passBefund->setToolTip(QStringLiteral(
                "Der Punkt selbst passt, dabei ist aber etwas aufgefallen — Bemerkung oder Screenshot ist Pflicht."));
            auto* pass = new QPushButton(QStringLiteral("✓ Pass"), rechts);
            pass->setStyleSheet(QStringLiteral("font-weight:bold;"));
            pass->setToolTip(QStringLiteral("Strg+1"));
            passBefund->setToolTip(passBefund->toolTip() + QStringLiteral(" Strg+2"));
            fail->setToolTip(QStringLiteral("Bemerkung oder Screenshot ist Pflicht. Strg+3"));
            skip->setToolTip(QStringLiteral("Strg+4"));
            knoepfe->addWidget(zurueck);
            knoepfe->addWidget(weiter);
            knoepfe->addStretch();
            // Eine Vorbereitung bekommt kein Urteil, sondern »Weiter →«.
            m_weiterPrep = new QPushButton(QStringLiteral("Weiter →"), rechts);
            m_weiterPrep->setVisible(false);
            knoepfe->addWidget(m_weiterPrep);
            m_urteil = { offenBtn, skip, fail, passBefund, pass };
            knoepfe->addWidget(offenBtn);
            knoepfe->addWidget(skip);
            knoepfe->addWidget(fail);
            knoepfe->addWidget(passBefund);
            knoepfe->addWidget(pass);
            col->addLayout(knoepfe);

            split->setStretchFactor(0, 2);
            split->setStretchFactor(1, 3);
            split->setSizes({ 420, 620 });

            auto* fuss = new QHBoxLayout();
            m_summe = new QLabel(this);
            m_summe->setSizePolicy(QSizePolicy::Ignored, QSizePolicy::Preferred);
            fuss->addWidget(m_summe, 1);
            m_exeBtn = new QPushButton(QStringLiteral("▶ Exe starten"), this);
            fuss->addWidget(m_exeBtn);
            auto* reportBtn = new QPushButton(QStringLiteral("Report öffnen"), this);
            fuss->addWidget(reportBtn);
            auto* pfadBtn = new QPushButton(QStringLiteral("Report-Pfad kopieren"), this);
            pfadBtn->setToolTip(QStringLiteral("Den Pfad des Reports in die Zwischenablage — zum Einfügen im Chat."));
            fuss->addWidget(pfadBtn);
            lay->addLayout(fuss);

            m_steuerungZeile = new QLabel(this);
            m_steuerungZeile->setSizePolicy(QSizePolicy::Ignored, QSizePolicy::Preferred);
            m_steuerungZeile->setStyleSheet(QStringLiteral("color:#8a8a8a;"));
            m_steuerungZeile->setTextInteractionFlags(Qt::TextSelectableByMouse);
            lay->addWidget(m_steuerungZeile);
            m_steuerung.beiAenderung = [this]() {
                bemerkeAnwendungen();
                zeigeSteuerung();
                aktualisiereAktionen();
                weiterNachNeustart();
            };
            m_steuerung.beiMeldung = [this](const QString& anwendung, const QString& text) {
                m_steuerungZeile->setText(QStringLiteral("%1 — %2: %3")
                                              .arg(m_steuerung.zustandsText(), anwendung, text));
            };
            connect(m_herstellen, &QPushButton::clicked, this, [this]() {
                if (m_lauf.aktiv) brichAb();
                else starteAktionen(m_idx, aktionenVon(m_p, m_idx));
            });
            connect(m_vorbereitungBtn, &QPushButton::clicked, this, [this]() {
                const int v = vorbereitungVon(m_p, m_idx);
                starteAktionen(v, aktionenVon(m_p, v));
            });
            connect(m_weiterPrep, &QPushButton::clicked, this, [this]() { gehe(+1); });
            connect(&m_neuFrist, &QTimer::timeout, this, [this]() {
                if (m_lauf.aktiv && m_lauf.wartetAufNeu)
                    beendeLauf(QStringLiteral("die Anwendung hat sich nach 60 s nicht wieder verbunden"));
            });

            connect(m_combo, &QComboBox::activated, this, [this](int i) {
                oeffne(m_combo->itemData(i).toString(), false);
            });
            connect(neuLesen, &QPushButton::clicked, this, [this]() {
                setzeOrdner(m_ordner, m_p.pfad);
            });
            connect(ordnerBtn, &QPushButton::clicked, this, [this]() {
                const QString o = QFileDialog::getExistingDirectory(
                    this, QStringLiteral("Ordner mit Sichttest-Listen"), m_ordner);
                if (!o.isEmpty()) setzeOrdner(o, {});
            });
            connect(neuerLauf, &QPushButton::clicked, this, [this]() {
                if (m_p.pfad.isEmpty()) return;
                if (QMessageBox::question(this, QStringLiteral("Neuer Lauf"),
                        QStringLiteral("Einen frischen Lauf beginnen? Der bisherige bleibt als Datei liegen."))
                    == QMessageBox::Yes)
                    oeffne(m_p.pfad, true);
            });
            connect(m_vorn, &QCheckBox::toggled, this, [this](bool an) {
                setWindowFlag(Qt::WindowStaysOnTopHint, an);
                show();
                QSettings().setValue(QStringLiteral("vorn"), an);
            });
            connect(m_nurOffene, &QCheckBox::toggled, this, [this]() { fuelleListe(); });
            connect(m_liste, &QListWidget::currentItemChanged, this,
                    [this](QListWidgetItem* neu, QListWidgetItem*) {
                if (m_fuellt || !neu) return;
                const QVariant v = neu->data(kRolleSchritt);
                if (v.isValid()) zeige(v.toInt());
            });
            connect(bildBtn, &QPushButton::clicked, this, [this]() { haengeAusZwischenablageAn(); });
            connect(loesche, &QAction::triggered, this, [this]() { loescheBild(); });
            connect(m_bilder, &QListWidget::itemDoubleClicked, this, [this](QListWidgetItem* it) {
                QDesktopServices::openUrl(QUrl::fromLocalFile(
                    QDir(logOrdner(m_p.pfad)).filePath(it->text())));
            });
            connect(zurueck, &QPushButton::clicked, this, [this]() { gehe(-1); });
            connect(weiter, &QPushButton::clicked, this, [this]() { gehe(+1); });
            connect(offenBtn, &QPushButton::clicked, this, [this]() { bewerte(QStringLiteral("open")); });
            connect(skip, &QPushButton::clicked, this, [this]() { bewerte(QStringLiteral("skip")); });
            connect(fail, &QPushButton::clicked, this, [this]() { bewerte(QStringLiteral("fail")); });
            connect(passBefund, &QPushButton::clicked, this, [this]() { bewerte(QStringLiteral("pass_remark")); });
            connect(pass, &QPushButton::clicked, this, [this]() { bewerte(QStringLiteral("pass")); });
            connect(m_exeBtn, &QPushButton::clicked, this, [this]() { starteAnwendung(); });
            m_startFrist.setSingleShot(true);
            connect(&m_startFrist, &QTimer::timeout, this, [this]() {
                if (m_steuerung.anwendung(m_p.anwendung)) return;
                m_aktionStand->setText(QStringLiteral(
                    "<span style='color:#d64545;'>gestartet, aber nicht verbunden — läuft die Anwendung schon "
                    "ohne <code>--testing</code>?</span>"));
            });
            connect(reportBtn, &QPushButton::clicked, this, [this]() {
                if (merkeBemerkung() || !QFileInfo::exists(reportPfad())) schreibe();
                QDesktopServices::openUrl(QUrl::fromLocalFile(reportPfad()));
            });
            connect(pfadBtn, &QPushButton::clicked, this, [this]() {
                if (merkeBemerkung() || !QFileInfo::exists(reportPfad())) schreibe();
                QGuiApplication::clipboard()->setText(QDir::toNativeSeparators(reportPfad()));
            });

            auto kuerzel = [this](const QString& tasten, const QString& ergebnis) {
                auto* s = new QShortcut(QKeySequence(tasten), this);
                connect(s, &QShortcut::activated, this, [this, ergebnis]() { bewerte(ergebnis); });
            };
            kuerzel(QStringLiteral("Ctrl+1"), QStringLiteral("pass"));
            kuerzel(QStringLiteral("Ctrl+2"), QStringLiteral("pass_remark"));
            kuerzel(QStringLiteral("Ctrl+3"), QStringLiteral("fail"));
            kuerzel(QStringLiteral("Ctrl+4"), QStringLiteral("skip"));
            auto* einfuegen = new QShortcut(QKeySequence(QStringLiteral("Ctrl+Shift+V")), this);
            connect(einfuegen, &QShortcut::activated, this, [this]() { haengeAusZwischenablageAn(); });

            const QSettings einstellungen;
            restoreGeometry(einstellungen.value(QStringLiteral("geometrie")).toByteArray());
            if (einstellungen.value(QStringLiteral("vorn")).toBool())
            {
                const QSignalBlocker ruhe(m_vorn);
                m_vorn->setChecked(true);
                setWindowFlag(Qt::WindowStaysOnTopHint, true);
            }
        }

        void setzeOrdner(const QString& ordner, const QString& vorwahl)
        {
            m_ordner = ordner;
            m_combo->clear();

            // Projektdatei (Konzept §7): ausdrücklich genannt, sonst vom Ordner aufwärts gesucht.
            // Mit ihr zählen ihre Listenordner und ihre Ablage, ohne sie der Ordner wie bisher.
            if (!m_projektGenannt.isEmpty() && QFileInfo(m_projektGenannt).absolutePath() != ordner)
                m_projektGenannt.clear();
            const QString projektDatei = !m_projektGenannt.isEmpty() ? m_projektGenannt : findeProjektDatei(ordner);
            QString projektFehler;
            setzeProjekt(projektDatei.isEmpty() ? Projekt() : ladeProjekt(projektDatei, &projektFehler));
            const Projekt& pr = projekt();

            const QStringList pfade = pr.gueltig() ? protokolleDesProjekts(pr)
                                    : ordner.isEmpty() ? QStringList() : findeProtokolle(ordner);
            for (const QString& p : pfade)
            {
                const Protokoll kurz = lade(p);
                const Zaehler z = zaehleMitLauf(kurz);
                m_combo->addItem(QStringLiteral("%1   ·   ○ %2  ✗ %3   (%4)")
                                     .arg(pr.gueltig() ? QDir(pr.ordner).relativeFilePath(p) : QFileInfo(p).fileName())
                                     .arg(z.offen).arg(z.fail)
                                     .arg(kurz.titel), p);
            }
            if (pfade.isEmpty())
            {
                m_p = {};
                m_idx = -1;
                fuelleListe();
                m_kopfzeile->setText(!projektFehler.isEmpty() ? projektFehler
                    : pr.gueltig()
                    ? QStringLiteral("Die Projektdatei %1 nennt keinen Ordner, in dem Listen liegen.")
                          .arg(QDir::toNativeSeparators(pr.pfad))
                    : ordner.isEmpty()
                    ? QStringLiteral("Kein Ordner gewählt — über »Ordner…« einen Ordner mit Sichttest-Listen öffnen.")
                    : QStringLiteral("In %1 liegt keine Liste mit Punkten der Form »- [ ] **A1 Titel:** Text«.")
                          .arg(QDir::toNativeSeparators(ordner)));
                aktualisiereAktionen();
                return;
            }
            QSettings().setValue(QStringLiteral("ordner"), ordner);
            int i = m_combo->findData(vorwahl);
            if (i < 0) i = 0;
            m_combo->setCurrentIndex(i);
            oeffne(m_combo->itemData(i).toString(), false);
        }

        static Zaehler zaehleMitLauf(Protokoll p)
        {
            const QString lauf = neuesterLauf(p.pfad);
            if (!lauf.isEmpty()) uebernimmLauf(p, leseJson(lauf));
            return zaehle(p.schritte);
        }

        static QJsonObject leseJson(const QString& pfad)
        {
            QFile f(pfad);
            if (!f.open(QIODevice::ReadOnly)) return {};
            return QJsonDocument::fromJson(f.readAll()).object();
        }

        void oeffne(const QString& pfad, bool neuerLauf)
        {
            if (merkeBemerkung()) schreibe();
            QString fehler;
            Protokoll p = lade(pfad, &fehler);
            if (p.schritte.isEmpty())
            {
                QMessageBox::warning(this, QStringLiteral("Sichttest"), fehler);
                return;
            }
            const QString lauf = neuerLauf ? QString() : neuesterLauf(pfad);
            if (!lauf.isEmpty())
            {
                const QJsonObject log = leseJson(lauf);
                uebernimmLauf(p, log);
                m_logPfad = lauf;
                m_gestartet = log.value(QStringLiteral("started")).toString();
            }
            else
            {
                m_logPfad = QStringLiteral("%1/%2_%3.testlog.json")
                    .arg(logOrdner(pfad), logStamm(pfad),
                         QDateTime::currentDateTime().toString(QStringLiteral("yyyyMMdd-HHmmss")));
                m_gestartet = QDateTime::currentDateTime().toString(Qt::ISODate);
            }
            // Nennt die Liste keine Anwendung und keine Exe, gelten die des Projekts.
            const Projekt& pr = projekt();
            if (p.anwendung.isEmpty()) p.anwendung = pr.anwendung;
            if (p.exe.isEmpty() && !pr.startExe.isEmpty()) p.exe = pr.startExe;
            m_p = p;
            m_gespeichert.clear();
            m_nachbereitet = false;
            m_exeDatei = findeExeDatei(m_p.exe, pfad);
            m_exeZeit = m_exeDatei.isEmpty() ? QString()
                : QFileInfo(m_exeDatei).lastModified().toString(QStringLiteral("dd.MM.yyyy HH:mm"));
            m_exeBtn->setVisible(!m_exeDatei.isEmpty());
            m_exeBtn->setToolTip(QDir::toNativeSeparators(m_exeDatei)
                                 + (pr.startArgumente.isEmpty() ? QString()
                                        : QLatin1Char(' ') + pr.startArgumente.join(QLatin1Char(' '))));
            QSettings().setValue(QStringLiteral("protokoll"), pfad);

            QString kopf = QStringLiteral("<b>%1</b>").arg(m_p.titel.toHtmlEscaped());
            if (pr.gueltig())
                kopf += QStringLiteral(" · Projekt %1").arg(QDir::toNativeSeparators(pr.pfad).toHtmlEscaped());
            if (!m_p.exe.isEmpty())
                kopf += m_exeDatei.isEmpty()
                    ? QStringLiteral(" · Exe nicht gefunden: %1").arg(m_p.exe.toHtmlEscaped())
                    : QStringLiteral(" · Exe vom %1").arg(m_exeZeit);
            kopf += lauf.isEmpty() ? QStringLiteral(" · neuer Lauf")
                                   : QStringLiteral(" · Lauf vom %1 fortgesetzt").arg(m_gestartet);
            for (const QString& h : std::as_const(m_p.hinweise))
                kopf += QStringLiteral("<br><span style='color:#d64545;'>⚠ %1</span>").arg(h.toHtmlEscaped());
            m_kopfzeile->setText(kopf);
            m_kopfzeile->setToolTip(m_p.beschreibung);

            m_lauf = {};
            m_neuFrist.stop();
            m_aktionStand->clear();
            m_idx = -1;
            fuelleListe();
            const int erster = naechsterOffene(-1);
            zeige(erster >= 0 ? mitVorbereitung(erster, -1) : 0);
            aktualisiereSumme();
            zeigeSteuerung();
        }

        QJsonObject schritt(int idx) const { return m_p.schritte.at(idx).toObject(); }

        bool sichtbar(const QJsonObject& s) const
        {
            if (!m_nurOffene->isChecked()) return true;
            const QString r = s.value(QStringLiteral("result")).toString();
            return r == QLatin1String("open") || r == QLatin1String("fail");
        }

        void fuelleListe()
        {
            m_fuellt = true;
            m_liste->clear();
            QString sektion;
            for (int n = 0; n < m_p.schritte.size(); ++n)
            {
                const QJsonObject s = schritt(n);
                if (!sichtbar(s) && n != m_idx) continue;
                const QString sek = s.value(QStringLiteral("section")).toString();
                if (sek != sektion)
                {
                    sektion = sek;
                    auto* kopf = new QListWidgetItem(sek, m_liste);
                    kopf->setFlags(Qt::NoItemFlags);
                    QFont f = kopf->font();
                    f.setBold(true);
                    kopf->setFont(f);
                    kopf->setForeground(palette().color(QPalette::Text));
                }
                auto* it = new QListWidgetItem(m_liste);
                it->setData(kRolleSchritt, n);
                beschrifte(it, s);
                if (n == m_idx) m_liste->setCurrentItem(it);
            }
            m_fuellt = false;
        }

        void beschrifte(QListWidgetItem* it, const QJsonObject& s)
        {
            const QString r = s.value(QStringLiteral("result")).toString();
            const bool bild = !s.value(QStringLiteral("screenshots")).toArray().isEmpty();
            const bool text = !s.value(QStringLiteral("remark")).toString().trimmed().isEmpty();
            it->setText(QStringLiteral("%1  %2  %3%4%5")
                .arg(zeichen(r), s.value(QStringLiteral("id")).toString(),
                     s.value(QStringLiteral("title")).toString(),
                     text ? QStringLiteral("  ✎") : QString(),
                     bild ? QStringLiteral("  🖼") : QString()));
            it->setToolTip(s.value(QStringLiteral("title")).toString());
            const QString f = farbe(r);
            it->setForeground(f.isEmpty() ? palette().color(QPalette::Text) : QColor(f));
        }

        QListWidgetItem* eintrag(int idx) const
        {
            for (int r = 0; r < m_liste->count(); ++r)
                if (m_liste->item(r)->data(kRolleSchritt).isValid()
                    && m_liste->item(r)->data(kRolleSchritt).toInt() == idx)
                    return m_liste->item(r);
            return nullptr;
        }

        void zeige(int idx)
        {
            if (idx < 0 || idx >= m_p.schritte.size()) return;
            if (m_idx != idx && merkeBemerkung()) schreibe();
            m_idx = idx;
            const QJsonObject s = schritt(idx);
            m_titel->setText(QStringLiteral("%1  %2")
                .arg(s.value(QStringLiteral("id")).toString(),
                     s.value(QStringLiteral("title")).toString()));
            m_text->setMarkdown(s.value(QStringLiteral("text")).toString());
            m_bemerkung->setPlainText(s.value(QStringLiteral("remark")).toString());
            zeigeStand(s);
            zeigeBilder(s);
            if (QListWidgetItem* it = eintrag(idx))
            {
                m_fuellt = true;
                m_liste->setCurrentItem(it);
                m_liste->scrollToItem(it);
                m_fuellt = false;
            }
            if (!m_lauf.aktiv) m_aktionStand->clear();
            aktualisiereAktionen();
        }

        void zeigeStand(const QJsonObject& s)
        {
            const QString r = s.value(QStringLiteral("result")).toString();
            static const QHash<QString, QString> namen = {
                { QStringLiteral("pass"), QStringLiteral("Pass") },
                { QStringLiteral("pass_remark"), QStringLiteral("Pass mit Befund") },
                { QStringLiteral("fail"), QStringLiteral("Fail") },
                { QStringLiteral("skip"), QStringLiteral("übersprungen") },
                { QStringLiteral("prep"), QStringLiteral("Vorbereitung, ohne Urteil") },
            };
            QString t = QStringLiteral("%1 · %2 von %3 · ")
                .arg(s.value(QStringLiteral("section")).toString())
                .arg(m_idx + 1).arg(m_p.schritte.size());
            t += zeichen(r) + QLatin1Char(' ') + namen.value(r, QStringLiteral("offen"));
            const QString wann = s.value(QStringLiteral("rated")).toString();
            if (!wann.isEmpty()) t += QStringLiteral(" (%1)").arg(wann);
            // Felder des Comm Studio, die zum Handgriff gehören.
            const QString tab = s.value(QStringLiteral("tab")).toString();
            if (!tab.isEmpty()) t += QStringLiteral(" · Tab: %1").arg(tab);
            if (s.value(QStringLiteral("restart")).toBool()) t += QStringLiteral(" · mit Neustart der Anwendung");
            m_stand->setText(t);
            const QString f = farbe(r);
            m_stand->setStyleSheet(f.isEmpty() ? QString() : QStringLiteral("color:%1;").arg(f));
        }

        void zeigeBilder(const QJsonObject& s)
        {
            m_bilder->clear();
            const QDir ordner(logOrdner(m_p.pfad));
            const QJsonArray bilder = s.value(QStringLiteral("screenshots")).toArray();
            for (const QJsonValue& b : bilder)
            {
                auto* it = new QListWidgetItem(b.toString(), m_bilder);
                it->setToolTip(QStringLiteral("<img src='%1' width='520'>")
                    .arg(QUrl::fromLocalFile(ordner.filePath(b.toString())).toString()));
            }
        }

        // Bemerkung des gezeigten Schritts übernehmen; true = sie hat sich geändert.
        bool merkeBemerkung()
        {
            if (m_idx < 0 || m_idx >= m_p.schritte.size()) return false;
            QJsonObject s = schritt(m_idx);
            const QString neu = m_bemerkung->toPlainText().trimmed();
            if (s.value(QStringLiteral("remark")).toString() == neu) return false;
            s.insert(QStringLiteral("remark"), neu);
            m_p.schritte.replace(m_idx, s);
            if (QListWidgetItem* it = eintrag(m_idx)) beschrifte(it, s);
            return true;
        }

        void bewerte(const QString& ergebnis)
        {
            if (m_idx < 0 || istVorbereitung(schritt(m_idx))) return;
            merkeBemerkung();
            QJsonObject s = schritt(m_idx);
            const bool beleg = !s.value(QStringLiteral("remark")).toString().isEmpty()
                               || !s.value(QStringLiteral("screenshots")).toArray().isEmpty();
            if (!beleg && (ergebnis == QLatin1String("fail") || ergebnis == QLatin1String("pass_remark")))
            {
                QMessageBox::information(this, QStringLiteral("Sichttest"),
                    QStringLiteral("Dazu gehört eine Bemerkung oder ein Screenshot — sonst lässt sich der Befund nicht beheben."));
                m_bemerkung->setFocus();
                return;
            }
            s.insert(QStringLiteral("result"), ergebnis);
            if (ergebnis == QLatin1String("open")) s.remove(QStringLiteral("rated"));
            else s.insert(QStringLiteral("rated"),
                          QDateTime::currentDateTime().toString(QStringLiteral("dd.MM. HH:mm")));
            m_p.schritte.replace(m_idx, s);
            schreibe();
            if (QListWidgetItem* it = eintrag(m_idx)) beschrifte(it, s);
            // FERTIG: die Nachbereitung der Liste anbieten (§6.2).
            if (zaehle(m_p.schritte).offen == 0) bieteNachbereitungAn();

            int weiter = ergebnis == QLatin1String("open") ? -1 : naechsterOffene(m_idx);
            if (weiter >= 0)
            {
                weiter = mitVorbereitung(weiter, m_idx);
                if (m_nurOffene->isChecked()) { m_idx = weiter; fuelleListe(); m_idx = -1; }
                zeige(weiter);
            }
            else
            {
                zeigeStand(s);
            }
        }

        int naechsterOffene(int nach) const
        {
            const int n = int(m_p.schritte.size());
            for (int k = 1; k <= n; ++k)
            {
                const int i = (nach + k + n) % n;
                if (schritt(i).value(QStringLiteral("result")).toString() == QLatin1String("open"))
                    return i;
            }
            return -1;
        }

        void gehe(int richtung)
        {
            // Über die sichtbaren Einträge der Liste, damit der Filter gilt.
            QListWidgetItem* jetzt = eintrag(m_idx);
            int r = jetzt ? m_liste->row(jetzt) : -1;
            for (r += richtung; r >= 0 && r < m_liste->count(); r += richtung)
                if (m_liste->item(r)->data(kRolleSchritt).isValid())
                {
                    zeige(m_liste->item(r)->data(kRolleSchritt).toInt());
                    return;
                }
        }

        QString laufStamm() const
        {
            QString name = QFileInfo(m_logPfad).fileName();
            name.chop(int(qstrlen(".testlog.json")));
            return name;
        }

        QString reportPfad() const
        {
            return QDir(logOrdner(m_p.pfad)).filePath(laufStamm() + QStringLiteral(".report.md"));
        }

        void haengeAusZwischenablageAn()
        {
            const QImage bild = QGuiApplication::clipboard()->image();
            if (bild.isNull())
            {
                QMessageBox::information(this, QStringLiteral("Screenshot"),
                    QStringLiteral("In der Zwischenablage liegt kein Bild. Win+Shift+S, Bereich wählen, dann noch einmal."));
                return;
            }
            haengeBildAn(bild);
        }

        void haengeBildAn(const QImage& bild)
        {
            if (m_idx < 0) return;
            merkeBemerkung();
            QJsonObject s = schritt(m_idx);
            QDir ordner(logOrdner(m_p.pfad));
            ordner.mkpath(QStringLiteral("."));
            QString name;
            for (int n = 1; ; ++n)
            {
                name = QStringLiteral("%1_%2_%3.png")
                    .arg(laufStamm(), s.value(QStringLiteral("id")).toString()).arg(n);
                if (!ordner.exists(name)) break;
            }
            if (!bild.save(ordner.filePath(name), "PNG"))
            {
                QMessageBox::warning(this, QStringLiteral("Screenshot"),
                    QStringLiteral("Das Bild ließ sich nicht speichern: %1").arg(ordner.filePath(name)));
                return;
            }
            QJsonArray bilder = s.value(QStringLiteral("screenshots")).toArray();
            bilder.append(name);
            s.insert(QStringLiteral("screenshots"), bilder);
            m_p.schritte.replace(m_idx, s);
            schreibe();
            zeigeBilder(s);
            if (QListWidgetItem* it = eintrag(m_idx)) beschrifte(it, s);
        }

        void loescheBild()
        {
            QListWidgetItem* gewaehlt = m_bilder->currentItem();
            if (!gewaehlt || m_idx < 0) return;
            const QString name = gewaehlt->text();
            QJsonObject s = schritt(m_idx);
            QJsonArray bilder = s.value(QStringLiteral("screenshots")).toArray();
            for (int n = 0; n < bilder.size(); ++n)
                if (bilder.at(n).toString() == name) { bilder.removeAt(n); break; }
            s.insert(QStringLiteral("screenshots"), bilder);
            m_p.schritte.replace(m_idx, s);
            QFile::remove(QDir(logOrdner(m_p.pfad)).filePath(name));
            schreibe();
            zeigeBilder(s);
            if (QListWidgetItem* it = eintrag(m_idx)) beschrifte(it, s);
        }

        void schreibe()
        {
            if (m_p.pfad.isEmpty()) return;
            QDir().mkpath(logOrdner(m_p.pfad));
            bool gut = true;
            {
                QSaveFile f(m_logPfad);
                gut = f.open(QIODevice::WriteOnly)
                      && f.write(QJsonDocument(alsLog(m_p, m_gestartet, m_exeZeit)).toJson()) >= 0
                      && f.commit();
            }
            {
                QSaveFile f(reportPfad());
                gut = f.open(QIODevice::WriteOnly)
                      && f.write(alsReport(m_p, m_gestartet, m_exeZeit).toUtf8()) >= 0
                      && f.commit() && gut;
            }
            m_gespeichert = gut ? QDateTime::currentDateTime().toString(QStringLiteral("HH:mm:ss"))
                                : QStringLiteral("FEHLER beim Schreiben");
            aktualisiereSumme();
        }

        void aktualisiereSumme()
        {
            const Zaehler z = zaehle(m_p.schritte);
            QString t = QStringLiteral("✓ %1 · ✓⚠ %2 · ✗ %3 · ↷ %4 · ○ %5")
                .arg(z.pass).arg(z.passBefund).arg(z.fail).arg(z.skip).arg(z.offen);
            if (!m_gespeichert.isEmpty())
                t += QStringLiteral("   ·   gespeichert %1").arg(m_gespeichert);
            t += QStringLiteral("   ·   %1").arg(QDir::toNativeSeparators(reportPfad()));
            m_summe->setText(t);
            m_summe->setToolTip(QDir::toNativeSeparators(reportPfad()));
        }

        // --- Aktionen (Konzept §6) ---------------------------------------------

        struct Lauf
        {
            bool aktiv = false;
            int idx = -1;                // Punkt, in dessen Log die Aktionen stehen
            QJsonArray liste;
            int pos = 0;
            quint64 token = 0;           // eine Antwort mit altem Token wird verworfen
            bool fortsetzen = false;     // Vorbereitung eines schon bewerteten Laufs (§6.4)
            bool abbrechbar = false;     // die laufende Aktion wartet auf den Menschen
            bool beendet = false;        // die laufende Aktion beendet die Anwendung …
            bool wartetAufNeu = false;   // … und danach stehen noch Aktionen an
            qint64 altePid = 0;
        };

        // Wechselt der nächste offene Punkt den Abschnitt, steht dessen Vorbereitung davor.
        int mitVorbereitung(int offen, int von) const
        {
            const auto sektion = [this](int i) { return schritt(i).value(QStringLiteral("section")).toString(); };
            if (von >= 0 && sektion(von) == sektion(offen)) return offen;
            while (offen > 0 && istVorbereitung(schritt(offen - 1))) --offen;
            return offen;
        }

        QString anwendungsName() const
        {
            const Steuerung::Anwendung* a = m_steuerung.anwendung(m_p.anwendung);
            return a ? QStringLiteral("%1 %2").arg(a->name, a->version) : m_p.anwendung;
        }

        QStringList schalterVon(const QString& aktion) const
        {
            const Steuerung::Anwendung* a = m_steuerung.anwendung(m_p.anwendung);
            if (!a) return {};
            for (const QJsonValue& v : a->aktionen)
                if (v.toObject().value(QStringLiteral("name")).toString() == aktion)
                    return v.toObject().value(QStringLiteral("schalter")).toVariant().toStringList();
            return {};
        }

        bool hatBewertung() const
        {
            const Zaehler z = zaehle(m_p.schritte);
            return z.pass + z.passBefund + z.fail + z.skip > 0;
        }

        // Statuszeile: Zustand der Steuerung, dazu die Aktionen der Liste, die die Anwendung nicht kennt.
        void zeigeSteuerung()
        {
            QString t = m_steuerung.zustandsText();
            if (m_steuerung.anwendung(m_p.anwendung))
            {
                QStringList fremd;
                for (int n = 0; n < m_p.schritte.size(); ++n)
                {
                    const QJsonArray aktionen = aktionenVon(m_p, n);
                    for (const QJsonValue& v : aktionen)
                    {
                        const QString name = v.toObject().value(QStringLiteral("aktion")).toString();
                        if (!v.toObject().value(QStringLiteral("unbekannt")).toBool() && !fremd.contains(name)
                            && !m_steuerung.kennt(m_p.anwendung, name))
                            fremd.append(name);
                    }
                }
                if (!fremd.isEmpty())
                    t += (fremd.size() == 1 ? QStringLiteral(" · 1 Aktion der Liste kennt %2 nicht: %3")
                                            : QStringLiteral(" · %1 Aktionen der Liste kennt %2 nicht: %3")
                                                  .arg(fremd.size()))
                             .arg(anwendungsName(), fremd.join(QStringLiteral(", ")));
            }
            m_steuerungZeile->setText(t);
        }

        void aktualisiereAktionen()
        {
            const bool da = m_idx >= 0 && m_idx < m_p.schritte.size();
            const bool prep = da && istVorbereitung(schritt(m_idx));
            const bool verbunden = m_steuerung.anwendung(m_p.anwendung) != nullptr;
            const QJsonArray eigene = da ? aktionenVon(m_p, m_idx) : QJsonArray();
            const int v = da && !prep ? vorbereitungVon(m_p, m_idx) : -1;

            for (QPushButton* k : std::as_const(m_urteil)) k->setVisible(!prep);
            m_weiterPrep->setVisible(prep);
            m_herstellen->setVisible(!eigene.isEmpty() || prep || m_lauf.aktiv);
            m_vorbereitungBtn->setVisible(v >= 0 && !aktionenVon(m_p, v).isEmpty());
            if (m_lauf.aktiv)
            {
                m_herstellen->setText(QStringLiteral("Abbrechen"));
                m_herstellen->setEnabled(m_lauf.abbrechbar || m_lauf.wartetAufNeu);
                m_vorbereitungBtn->setEnabled(false);
                return;
            }
            m_herstellen->setText(prep ? QStringLiteral("▶ Ausführen") : QStringLiteral("▶ Herstellen"));
            m_herstellen->setEnabled(verbunden && !eigene.isEmpty());
            m_vorbereitungBtn->setEnabled(verbunden);
            QStringList namen;
            for (const QJsonValue& a : eigene) namen.append(a.toObject().value(QStringLiteral("aktion")).toString());
            m_herstellen->setToolTip(verbunden
                ? QStringLiteral("Löst in %1 aus: %2").arg(anwendungsName(), namen.join(QStringLiteral(", ")))
                : QStringLiteral("Keine Anwendung verbunden — der Handgriff steht im Text."));
        }

        void starteAktionen(int logIdx, const QJsonArray& liste)
        {
            if (m_lauf.aktiv || liste.isEmpty() || (logIdx < 0 && logIdx != kNachbereitung)) return;
            if (!m_steuerung.anwendung(m_p.anwendung)) return;

            // Was Zustand verwerfen kann, fragt der Tester vorher — einmal für alle (§3, fragt_nach).
            QStringList heikel;
            for (const QJsonValue& v : liste)
            {
                const QString name = v.toObject().value(QStringLiteral("aktion")).toString();
                if (schalterVon(name).contains(QLatin1String("fragt_nach"))) heikel.append(name);
            }
            if (!heikel.isEmpty() && !m_ohneRueckfrage
                && QMessageBox::question(this, QStringLiteral("Sichttest"),
                       QStringLiteral("Gleich verändert %1 seinen Zustand — Ungespeichertes kann verloren gehen:\n\n%2\n\n"
                                      "Ausführen?").arg(anwendungsName(), heikel.join(QStringLiteral(", "))))
                   != QMessageBox::Yes)
                return;

            m_lauf = {};
            m_lauf.aktiv = true;
            m_lauf.idx = logIdx;
            m_lauf.liste = liste;
            m_lauf.token = ++m_token;
            m_lauf.fortsetzen = logIdx >= 0 && istVorbereitung(schritt(logIdx)) && hatBewertung();
            naechsteAktion();
        }

        // Nachbereitung der Liste (§6.2): am Ende des Laufs und beim Schließen angeboten,
        // nie während ein Lauf von Aktionen noch auf die Anwendung wartet. true = läuft.
        bool bieteNachbereitungAn()
        {
            if (m_p.nachbereitung.isEmpty() || m_nachbereitet || m_lauf.aktiv
                || !m_steuerung.anwendung(m_p.anwendung))
                return false;
            m_nachbereitet = true;   // einmal je Lauf fragen, nicht bei jedem Anlass erneut
            QStringList namen;
            for (const QJsonValue& v : std::as_const(m_p.nachbereitung))
                namen.append(v.toObject().value(QStringLiteral("aktion")).toString());
            if (!m_ohneRueckfrage
                && QMessageBox::question(this, QStringLiteral("Sichttest"),
                       QStringLiteral("Nachbereitung in %1 ausführen?\n\n%2")
                           .arg(anwendungsName(), namen.join(QStringLiteral(", "))))
                   != QMessageBox::Yes)
                return false;
            starteAktionen(kNachbereitung, m_p.nachbereitung);
            return m_lauf.aktiv;
        }

        // Die Anwendung starten: mit Projekt dessen Exe samt Argumenten; hatte sie sich in
        // dieser Sitzung schon gemeldet, die Exe aus ihrem hallo (§7) — sonst holte der
        // Tester nach dem Absturz einer Debug-Exe die Release-Exe.
        void starteAnwendung()
        {
            const Projekt& pr = projekt();
            QString exe = m_exeDatei;
            QStringList argumente;
            if (pr.gueltig() && !pr.startExe.isEmpty())
            {
                exe = m_gemeldeteExe.value(m_p.anwendung, m_exeDatei);
                argumente = pr.startArgumente;
            }
            if (exe.isEmpty()) return;
            if (!QProcess::startDetached(exe, argumente, QFileInfo(exe).absolutePath()))
            {
                m_aktionStand->setText(QStringLiteral("<span style='color:#d64545;'>ließ sich nicht starten: %1</span>")
                                           .arg(QDir::toNativeSeparators(exe).toHtmlEscaped()));
                return;
            }
            // Nur wer mit Argumenten des Projekts startet, erwartet eine Verbindung (§6.3).
            if (!argumente.isEmpty() && m_steuerung.lauscht()) m_startFrist.start(m_startFristMs);
        }

        // Neu verbundene Anwendungen: ihre Exe merken und die Listen ihres Projekts öffnen.
        void bemerkeAnwendungen()
        {
            const QList<Steuerung::Anwendung> alle = m_steuerung.anwendungen();
            for (const Steuerung::Anwendung& a : alle)
            {
                m_gemeldeteExe.insert(a.name, a.exe);
                if (m_gesehen.contains(a.pid)) continue;
                m_gesehen.insert(a.pid);
                m_startFrist.stop();
                // hallo nennt die Projektdatei ausdrücklich, sonst wird von der Exe aufwärts gesucht.
                const QString datei = !a.projektDatei.isEmpty() ? a.projektDatei
                                                                : findeProjektDatei(QFileInfo(a.exe).absolutePath());
                if (datei.isEmpty() || m_lauf.aktiv || QFileInfo(datei) == QFileInfo(projekt().pfad)) continue;
                m_projektGenannt = QFileInfo(datei).absoluteFilePath();
                setzeOrdner(QFileInfo(datei).absolutePath(), {});
            }
        }

        // Links der Listen des Comm Studio: `tab:<Titel>` und `sql:<Abfrage>` (ohne //).
        void folgeLink(const QUrl& url)
        {
            const QString art = url.scheme();
            if (art != QLatin1String("tab") && art != QLatin1String("sql"))
            {
                QDesktopServices::openUrl(url);
                return;
            }
            QString wert = QUrl::fromPercentEncoding(url.toEncoded());
            wert.remove(0, wert.indexOf(QLatin1Char(':')) + 1);
            while (wert.startsWith(QLatin1Char('/'))) wert.remove(0, 1);
            wert = wert.trimmed();
            if (wert.isEmpty()) return;
            // Die Zwischenablage füllt der Tester selbst (§6.2).
            if (art == QLatin1String("sql"))
            {
                m_kopiert = wert;
                if (!m_ohneZwischenablage) QGuiApplication::clipboard()->setText(wert);
            }
            const QString hand = art == QLatin1String("tab")
                ? QStringLiteral("Tab »%1« nach vorn holen").arg(wert)
                : QStringLiteral("Die Abfrage liegt in der Zwischenablage — im SQL-Terminal einfügen");
            QString aktion;
            QStringList schluessel;
            if (m_lauf.aktiv) return;
            if (!abbildung(projekt(), art, aktion, schluessel) || !m_steuerung.anwendung(m_p.anwendung))
            {
                m_aktionStand->setText(QStringLiteral("von Hand: %1").arg(hand.toHtmlEscaped()));
                return;
            }
            const QJsonObject mit{ { schluessel.value(0, QStringLiteral("wert")), wert } };
            starteAktionen(m_idx, QJsonArray{ QJsonObject{ { QStringLiteral("aktion"), aktion },
                                                           { QStringLiteral("mit"), mit },
                                                           { QStringLiteral("text"), hand } } });
        }

        void logge(const QJsonObject& aktion, const QString& status, const QString& text)
        {
            QJsonObject eintrag;
            eintrag.insert(QStringLiteral("aktion"), aktion.value(QStringLiteral("aktion")));
            eintrag.insert(QStringLiteral("mit"), aktion.value(QStringLiteral("mit")).toObject());
            eintrag.insert(QStringLiteral("status"), status);
            eintrag.insert(QStringLiteral("text"), text);
            eintrag.insert(QStringLiteral("at"), QDateTime::currentDateTime().toString(Qt::ISODate));
            if (m_lauf.idx == kNachbereitung)
            {
                m_p.nachbereitungLog.append(eintrag);
                return;
            }
            QJsonObject s = schritt(m_lauf.idx);
            QJsonArray log = s.value(QStringLiteral("actions")).toArray();
            log.append(eintrag);
            s.insert(QStringLiteral("actions"), log);
            m_p.schritte.replace(m_lauf.idx, s);
        }

        void naechsteAktion()
        {
            for (;;)
            {
                if (m_lauf.pos >= m_lauf.liste.size())
                {
                    beendeLauf({});
                    return;
                }
                const QJsonObject a = m_lauf.liste.at(m_lauf.pos).toObject();
                const QString name = a.value(QStringLiteral("aktion")).toString();
                const Steuerung::Anwendung* app = m_steuerung.anwendung(m_p.anwendung);
                if (!app)
                {
                    aktionFertig(QStringLiteral("getrennt"), QStringLiteral("keine Anwendung verbunden"));
                    return;
                }
                if (a.value(QStringLiteral("unbekannt")).toBool() || !m_steuerung.kennt(m_p.anwendung, name))
                {
                    aktionFertig(QStringLiteral("unbekannt"),
                                 QStringLiteral("%1 kennt diese Aktion nicht").arg(anwendungsName()));
                    return;
                }
                const QStringList schalter = schalterVon(name);
                if (m_lauf.fortsetzen && schalter.contains(QLatin1String("nur_erstlauf")))
                {
                    logge(a, QStringLiteral("ausgelassen"),
                          QStringLiteral("nur im ersten Lauf — beim Fortsetzen wird nicht abgeräumt"));
                    ++m_lauf.pos;
                    continue;
                }

                // Frist je Aufruf 10 s, in JSON je Aktion als "frist" in Sekunden änderbar;
                // wartet die Aktion auf den Menschen, gilt keine.
                m_lauf.abbrechbar = schalter.contains(QLatin1String("wartet_auf_mensch"));
                m_lauf.beendet = schalter.contains(QLatin1String("beendet_anwendung"));
                m_lauf.altePid = app->pid;
                const int fristMs = m_lauf.abbrechbar ? 0 : a.value(QStringLiteral("frist")).toInt(10) * 1000;
                m_aktionStand->setText((m_lauf.abbrechbar ? QStringLiteral("wartet auf die Anwendung: %1")
                                                          : QStringLiteral("läuft: %1 …")).arg(name.toHtmlEscaped()));
                aktualisiereAktionen();
                const quint64 token = m_lauf.token;
                // basis: Ordner der Projektdatei, sonst der Liste — ein Angebot, aufgelöst wird in der Anwendung.
                m_steuerung.rufe(m_p.anwendung, name, a.value(QStringLiteral("mit")).toObject(),
                                 projekt().gueltig() ? projekt().ordner : QFileInfo(m_p.pfad).absolutePath(), fristMs,
                                 [this, token](const QString& status, const QString& text) {
                                     if (m_lauf.aktiv && m_lauf.token == token) aktionFertig(status, text);
                                 });
                return;
            }
        }

        void aktionFertig(const QString& status, const QString& text)
        {
            const QJsonObject a = m_lauf.liste.at(m_lauf.pos).toObject();
            logge(a, status, text);
            // Endet die Anwendung, bevor ihre Antwort da ist, war das bei dieser Aktion gewollt.
            const bool gut = status == QLatin1String("ok") || (m_lauf.beendet && status == QLatin1String("getrennt"));
            if (!gut)
            {
                beendeLauf(text.isEmpty() ? status : text);
                return;
            }
            ++m_lauf.pos;
            if (m_lauf.beendet && m_lauf.pos < m_lauf.liste.size())
            {
                m_lauf.beendet = false;
                m_lauf.wartetAufNeu = true;
                m_neuFrist.start(60000);
                m_aktionStand->setText(QStringLiteral("Anwendung startet neu — warte auf die neue Verbindung …"));
                aktualisiereAktionen();
                weiterNachNeustart();
                return;
            }
            naechsteAktion();
        }

        void weiterNachNeustart()
        {
            if (!m_lauf.aktiv || !m_lauf.wartetAufNeu) return;
            const Steuerung::Anwendung* app = m_steuerung.anwendung(m_p.anwendung);
            if (!app || app->pid == m_lauf.altePid) return;
            m_lauf.wartetAufNeu = false;
            m_neuFrist.stop();
            naechsteAktion();
        }

        void brichAb()
        {
            if (!m_lauf.aktiv) return;
            if (m_lauf.pos < m_lauf.liste.size())
                logge(m_lauf.liste.at(m_lauf.pos).toObject(), QStringLiteral("abgebrochen"),
                      QStringLiteral("im Tester abgebrochen"));
            beendeLauf(QStringLiteral("abgebrochen — eine späte Antwort wird verworfen"));
        }

        // fehler leer = alles ausgeführt. Sonst steht die Meldung rot am Punkt, darunter der
        // Handgriff als Text — der Lauf hält nie an, das Urteil bleibt beim Menschen (§6.3).
        void beendeLauf(const QString& fehler)
        {
            const QJsonObject a = m_lauf.liste.at(m_lauf.pos).toObject();
            const int idx = m_lauf.idx;
            const int erledigt = m_lauf.pos;
            m_lauf.aktiv = false;
            m_neuFrist.stop();
            schreibe();
            if (fehler.isEmpty())
            {
                m_aktionStand->setText(erledigt == 1 ? QStringLiteral("✓ 1 Aktion ausgeführt")
                                                     : QStringLiteral("✓ %1 Aktionen ausgeführt").arg(erledigt));
            }
            else
            {
                QString hand = a.value(QStringLiteral("text")).toString();
                if (hand.isEmpty() && idx >= 0) hand = schritt(idx).value(QStringLiteral("text")).toString();
                if (hand.size() > 300) hand = hand.left(300) + QStringLiteral(" …");
                m_aktionStand->setText(QStringLiteral("<span style='color:#d64545;'>%1: %2</span><br>von Hand: %3")
                    .arg(a.value(QStringLiteral("aktion")).toString().toHtmlEscaped(), fehler.toHtmlEscaped(),
                         hand.toHtmlEscaped()));
            }
            aktualisiereAktionen();
            if (idx == kNachbereitung && m_schliesstDanach) QTimer::singleShot(0, this, &QWidget::close);
        }

        // --- Selbsttest der Aktionen -------------------------------------------

        using Pruefe = std::function<void(bool, const QString&)>;

        static bool warteBis(const std::function<bool()>& fertig, int ms)
        {
            QElapsedTimer uhr;
            uhr.start();
            while (!fertig() && uhr.elapsed() < ms)
            {
                QCoreApplication::processEvents();
                QThread::msleep(5);
            }
            return fertig();
        }

        int findeSchritt(const QString& id) const
        {
            for (int n = 0; n < m_p.schritte.size(); ++n)
                if (schritt(n).value(QStringLiteral("id")).toString() == id) return n;
            return -1;
        }

        QJsonArray ausgeloest(const QString& id) const
        {
            return schritt(findeSchritt(id)).value(QStringLiteral("actions")).toArray();
        }

        static QString aktionenListe()
        {
            return QStringLiteral(
                "# Aktionen\n\n"
                "**Anwendung:** Gegenprobe · **Exe:** `Gegenprobe.exe`\n\n"
                "- [ ] **V0 Vorbereiten:** Alles herrichten. `aktion: echo text=\"für alle\"`\n\n"
                "## G. Gruppe\n\n"
                "- [ ] **G0 Vorbereiten:** Vorlage laden, dann Edit an.\n"
                "      `aktion: echo datei=\"asset\\sicht test\\a.lvcomp\"` `aktion: echo an`\n"
                "- [ ] **G1 Eins:** Marke setzen. `aktion: echo zeit=0:20`\n"
                "- [ ] **G2 Zwei:** Wie G0, dazu mehr. `aktion: @G0` `aktion: echo edit=an`\n"
                "- [ ] **G3 Scheitert:** Von Hand zu tun. `aktion: scheitert`\n"
                "- [ ] **G4 Unbekannt:** Auch von Hand. `aktion: gibt_es_nicht`\n"
                "- [ ] **G5 Kreis:** Verweist auf sich. `aktion: @G5`\n"
                "- [ ] **G6 Neustart:** Beendet die Anwendung, danach weiter. `aktion: ende` `aktion: echo nach=neustart`\n"
                "- [ ] **G7 Ohne:** Ein Punkt wie bisher.\n");
        }

        // Der Leser allein, ohne Verbindung.
        void selbsttestListen(const QDir& d, const Pruefe& pruefe)
        {
            const Protokoll p = ausMarkdown(aktionenListe(), d.filePath(QStringLiteral("Aktionen.md")));
            auto idx = [&](const char* id) {
                for (int n = 0; n < p.schritte.size(); ++n)
                    if (p.schritte.at(n).toObject().value(QStringLiteral("id")).toString() == QLatin1String(id)) return n;
                return -1;
            };
            auto s = [&](const char* id) { return p.schritte.at(idx(id)).toObject(); };
            pruefe(p.schritte.size() == 9 && istVorbereitung(s("V0")) && istVorbereitung(s("G0"))
                   && !istVorbereitung(s("G1")) && zaehle(p.schritte).offen == 7,
                   QStringLiteral("Liste mit Aktionen: neun Punkte, zwei Vorbereitungen zählen nicht mit"));
            pruefe(p.anwendung == QLatin1String("Gegenprobe") && p.exe == QLatin1String("Gegenprobe.exe"),
                   QStringLiteral("Vorspann nennt die Anwendung (»%1«)").arg(p.anwendung));
            pruefe(s("G1").value(QStringLiteral("text")).toString() == QLatin1String("Marke setzen."),
                   QStringLiteral("das Stück `aktion: …` steht nicht im gezeigten Text"));
            const QJsonArray g0 = aktionenVon(p, idx("G0"));
            pruefe(g0.size() == 2
                   && g0.at(0).toObject().value(QStringLiteral("mit")).toObject().value(QStringLiteral("datei")).toString()
                          == QLatin1String("asset\\sicht test\\a.lvcomp")
                   && g0.at(1).toObject().value(QStringLiteral("mit")).toObject().value(QStringLiteral("wert")).toString()
                          == QLatin1String("an"),
                   QStringLiteral("Werte in \"…\" behalten Leerzeichen und \\; ein Wert ohne Schlüssel heißt wert"));
            const QJsonArray g2 = aktionenVon(p, idx("G2"));
            pruefe(g2.size() == 3
                   && g2.at(2).toObject().value(QStringLiteral("mit")).toObject().value(QStringLiteral("edit")).toString()
                          == QLatin1String("an"),
                   QStringLiteral("Verweis @G0 fügt dessen Aktionen an seiner Stelle ein"));
            pruefe(p.hinweise.size() == 1 && p.hinweise.first().contains(QLatin1String("Kreis"))
                   && aktionenVon(p, idx("G5")).at(0).toObject().value(QStringLiteral("unbekannt")).toBool(),
                   QStringLiteral("ein Verweis im Kreis wird beim Laden gemeldet (»%1«)").arg(p.hinweise.join(QLatin1Char(' '))));
            pruefe(vorbereitungVon(p, idx("G1")) == idx("G0") && vorbereitungVon(p, idx("G0")) == idx("G0"),
                   QStringLiteral("zu einem Punkt gehört die Vorbereitung seines Abschnitts"));

            const QJsonObject wurzel = QJsonDocument::fromJson(QByteArray(R"({
                "title": "J", "anwendung": "CommStudio",
                "vorbereitung": [ { "aktion": "datei_oeffnen", "mit": { "pfad": "examples/demo.project.json" },
                                    "text": "Projekt demo öffnen" } ],
                "nachbereitung": [ { "aktion": "testdb_abbauen" } ],
                "steps": [
                  { "id": "db-00", "kind": "prep", "section": "A", "title": "Vorbereiten", "text": "…",
                    "aktionen": [ { "aktion": "tab_zeigen", "mit": { "titel": "Datenbank" }, "frist": 20 } ] },
                  { "id": "db-01", "section": "A", "title": "Eins", "text": "…",
                    "aktionen": [ { "aktion": "@db-00" } ] },
                  { "id": "db-02", "section": "B", "title": "Zwei", "text": "…" } ] })")).object();
            const Protokoll j = ausJson(wurzel, d.filePath(QStringLiteral("J.testprotokoll.json")));
            pruefe(j.schritte.size() == 4 && j.anwendung == QLatin1String("CommStudio") && j.nachbereitung.size() == 1
                   && istVorbereitung(j.schritte.at(0).toObject()) && istVorbereitung(j.schritte.at(1).toObject())
                   && zaehle(j.schritte).offen == 2,
                   QStringLiteral("JSON: vorbereitung wird ein Punkt vor dem ersten Abschnitt, kind prep zählt nicht mit"));
            pruefe(aktionenVon(j, 2).size() == 1
                   && aktionenVon(j, 2).at(0).toObject().value(QStringLiteral("frist")).toInt() == 20
                   && vorbereitungVon(j, 3) == 0 && vorbereitungVon(j, 2) == 1,
                   QStringLiteral("JSON: aktionen mit Verweis und Frist; Abschnitt ohne eigene Vorbereitung nimmt die der Liste"));
        }

        // Die Knöpfe ▶ gegen die Gegenprobe.
        void selbsttestAktionen(const QDir& d, const Pruefe& pruefe, const std::function<bool(const QString&)>& klicke)
        {
            const QString liste = d.filePath(QStringLiteral("Aktionen.md"));
            {
                QFile f(liste);
                if (!f.open(QIODevice::WriteOnly)) { pruefe(false, QStringLiteral("Aktionen.md schreiben")); return; }
                f.write(aktionenListe().toUtf8());
            }
            m_ohneRueckfrage = true;
            oeffne(liste, true);
            pruefe(m_idx == findeSchritt(QStringLiteral("V0")),
                   QStringLiteral("eine Liste beginnt bei ihrer Vorbereitung"));
            pruefe(!m_herstellen->isEnabled() && m_herstellen->text() == QStringLiteral("▶ Ausführen")
                   && !m_weiterPrep->isHidden() && m_urteil.first()->isHidden(),
                   QStringLiteral("Vorbereitung: Ausführen und Weiter statt Urteil; ohne Anwendung ist ▶ grau"));

            const QString kanal = QStringLiteral("sichttest-selbsttest-%1-liste").arg(QCoreApplication::applicationPid());
            m_selbsttestKanal = kanal;
            const QString g = QStringLiteral("Gegenprobe");
            QProcess app;
            pruefe(m_steuerung.lausche(kanal) && starteGegenprobe(app, QStringLiteral("anwendung"), kanal)
                   && warteBis([&] { return m_steuerung.anwendung(g) != nullptr; }, 10000),
                   QStringLiteral("Fenster lauscht, die Gegenprobe ist verbunden"));
            pruefe(m_herstellen->isEnabled(), QStringLiteral("mit Anwendung ist ▶ Ausführen bereit"));
            pruefe(m_steuerungZeile->text().contains(QLatin1String("1 Aktion der Liste kennt Gegenprobe 1.2.3 nicht: gibt_es_nicht")),
                   QStringLiteral("Statuszeile nennt die Aktion, die die Anwendung nicht kennt"));

            auto fuehreAus = [&](const QString& id, const QString& knopf) {
                zeige(findeSchritt(id));
                const bool geklickt = klicke(knopf);
                warteBis([&] { return !m_lauf.aktiv; }, 15000);
                return geklickt && !m_lauf.aktiv;
            };
            auto status = [](const QJsonArray& log, int n) { return log.at(n).toObject().value(QStringLiteral("status")).toString(); };

            pruefe(fuehreAus(QStringLiteral("G0"), QStringLiteral("▶ Ausführen")), QStringLiteral("G0: Ausführen läuft durch"));
            QJsonArray log = ausgeloest(QStringLiteral("G0"));
            pruefe(log.size() == 2 && status(log, 0) == QLatin1String("ok") && status(log, 1) == QLatin1String("ok")
                   && log.at(0).toObject().value(QStringLiteral("text")).toString().contains(QLatin1String("a.lvcomp")),
                   QStringLiteral("G0: zwei Aktionen der Reihe nach, im Log des Punkts mit Status und Text"));
            pruefe(m_aktionStand->text().contains(QStringLiteral("2 Aktionen ausgeführt")),
                   QStringLiteral("G0: das Fenster meldet den Erfolg"));

            pruefe(fuehreAus(QStringLiteral("G2"), QStringLiteral("▶ Herstellen")) && ausgeloest(QStringLiteral("G2")).size() == 3,
                   QStringLiteral("G2: Herstellen führt den Verweis und die eigene Aktion aus (drei)"));

            fuehreAus(QStringLiteral("G3"), QStringLiteral("▶ Herstellen"));
            log = ausgeloest(QStringLiteral("G3"));
            pruefe(log.size() == 1 && status(log, 0) == QLatin1String("fehler")
                   && m_aktionStand->text().contains(QLatin1String("absichtlich gescheitert"))
                   && m_aktionStand->text().contains(QLatin1String("von Hand: Von Hand zu tun.")),
                   QStringLiteral("G3: Meldung der Anwendung am Punkt, darunter der Handgriff als Text"));
            pruefe(schritt(findeSchritt(QStringLiteral("G3"))).value(QStringLiteral("result")).toString() == QLatin1String("open"),
                   QStringLiteral("G3: eine gescheiterte Aktion macht den Punkt nicht zu Fail"));

            fuehreAus(QStringLiteral("G4"), QStringLiteral("▶ Herstellen"));
            log = ausgeloest(QStringLiteral("G4"));
            pruefe(log.size() == 1 && status(log, 0) == QLatin1String("unbekannt")
                   && m_aktionStand->text().contains(QLatin1String("von Hand")),
                   QStringLiteral("G4: unbekannte Aktion fällt auf den Text zurück"));

            pruefe(fuehreAus(QStringLiteral("G1"), QStringLiteral("↺ Vorbereitung")) && ausgeloest(QStringLiteral("G0")).size() == 4,
                   QStringLiteral("G1: ↺ Vorbereitung wiederholt die Vorbereitung des Abschnitts"));

            zeige(findeSchritt(QStringLiteral("G7")));
            pruefe(m_herstellen->isHidden() && !m_vorbereitungBtn->isHidden(),
                   QStringLiteral("G7: ein Punkt ohne Aktionen zeigt kein ▶ Herstellen, aber ↺ Vorbereitung"));

            const QJsonObject aufPlatte = leseJson(m_logPfad);
            pruefe(aufPlatte.value(QStringLiteral("steps")).toArray().at(findeSchritt(QStringLiteral("G0"))).toObject()
                       .value(QStringLiteral("actions")).toArray().size() == 4
                   && aufPlatte.value(QStringLiteral("summary")).toObject().value(QStringLiteral("open")).toInt() == 7,
                   QStringLiteral("das Testlog trägt actions[] je Punkt; die Summe zählt sieben offene Punkte"));

            // Neustart: die erste Aktion beendet die Anwendung, die zweite läuft nach dem Wiederverbinden.
            zeige(findeSchritt(QStringLiteral("G6")));
            klicke(QStringLiteral("▶ Herstellen"));
            pruefe(warteBis([&] { return m_lauf.aktiv && m_lauf.wartetAufNeu; }, 10000)
                   && warteBis([&] { return app.state() == QProcess::NotRunning; }, 5000),
                   QStringLiteral("G6: nach »ende« wartet der Tester auf die neue Verbindung, statt einen Fehler zu melden"));
            QProcess neu;
            starteGegenprobe(neu, QStringLiteral("anwendung"), kanal);
            warteBis([&] { return !m_lauf.aktiv; }, 15000);
            log = ausgeloest(QStringLiteral("G6"));
            pruefe(!m_lauf.aktiv && log.size() == 2 && status(log, 1) == QLatin1String("ok")
                   && log.at(1).toObject().value(QStringLiteral("text")).toString().contains(QLatin1String("neustart")),
                   QStringLiteral("G6: die neu gestartete Anwendung verbindet sich, die zweite Aktion läuft"));

            bool beendet = false;
            m_steuerung.rufe(g, QStringLiteral("ende"), {}, {}, 5000, [&beendet](const QString&, const QString&) { beendet = true; });
            warteBis([&] { return beendet && neu.state() == QProcess::NotRunning; }, 5000);
            if (neu.state() != QProcess::NotRunning) neu.kill();
            neu.waitForFinished(2000);
            app.waitForFinished(2000);
            warteBis([&] { return m_steuerung.anwendung(g) == nullptr; }, 2000);
        }

        static bool schreibeDatei(const QString& pfad, const QByteArray& inhalt)
        {
            QDir().mkpath(QFileInfo(pfad).absolutePath());
            QFile f(pfad);
            return f.open(QIODevice::WriteOnly) && f.write(inhalt) == inhalt.size();
        }

        // Projektdatei (§7), Felder des Comm Studio (§6.2) und Nachbereitung, gegen die Gegenprobe.
        // Läuft nach selbsttestAktionen(): das Fenster lauscht schon.
        void selbsttestProjekt(const QDir& d, const Pruefe& pruefe, const std::function<bool(const QString&)>& klicke)
        {
            QString gegenprobe, dll;
            if (!gegenprobePfade(gegenprobe, dll)) { pruefe(false, QStringLiteral("Gegenprobe ist gebaut")); return; }
            const QString g = QStringLiteral("Gegenprobe");
            const QDir wurzel(d.filePath(QStringLiteral("projekt")));
            QDir(wurzel.absolutePath()).removeRecursively();

            // Alle Felder zeigen auf Aktionen der Gegenprobe: echo schickt die Argumente zurück,
            // ende beendet sie (wie der Neustart des Comm Studio).
            const QJsonObject projektJson{
                { QStringLiteral("schema"), 1 },
                { QStringLiteral("anwendung"), g },
                { QStringLiteral("unbekannter_schluessel"), true },
                { QStringLiteral("start"), QJsonObject{ { QStringLiteral("exe"), gegenprobe },
                      { QStringLiteral("argumente"), QJsonArray{ dll, QStringLiteral("anwendung") } } } },
                { QStringLiteral("listen"), QJsonArray{
                      QJsonObject{ { QStringLiteral("ordner"), QStringLiteral("listen") },
                                   { QStringLiteral("ablage"), QStringLiteral("ablage/studio") } },
                      QJsonObject{ { QStringLiteral("ordner"), QStringLiteral("md") },
                                   { QStringLiteral("muster"), QStringLiteral("Composer_*.md") } },
                      QJsonObject{ { QStringLiteral("ordner"), QStringLiteral("fehlt") } } } },
                { QStringLiteral("abbildung"), QJsonObject{
                      { QStringLiteral("setup.close_all_tabs"), QStringLiteral("echo") },
                      { QStringLiteral("setup.open"), QStringLiteral("echo pfad") },
                      { QStringLiteral("tab"), QStringLiteral("echo titel") },
                      { QStringLiteral("restart"), QStringLiteral("ende") },
                      { QStringLiteral("sql"), QStringLiteral("echo sql") },
                      { QStringLiteral("test_db"), QStringLiteral("echo name seed") },
                      { QStringLiteral("test_db.ende"), QStringLiteral("echo") } } } };
            const QString projektDatei = wurzel.filePath(QStringLiteral("sichttest.projekt.json"));
            const QString studio = wurzel.filePath(QStringLiteral("listen/studio.testprotokoll.json"));
            const QString composer = wurzel.filePath(QStringLiteral("md/Composer_1.md"));
            bool geschrieben = schreibeDatei(projektDatei, QJsonDocument(projektJson).toJson());
            geschrieben = geschrieben && schreibeDatei(studio, QByteArray(R"({
                "title": "Studio", "description": "Felder des Comm Studio",
                "test_db": { "name": "studiotest", "_hinweis": "Kommentar", "seed": [ { "projekt": "P" } ] },
                "setup": { "close_all_tabs": true, "open": [ "examples/demo.project.json" ] },
                "steps": [
                  { "id": "s-00", "kind": "prep", "section": "A", "title": "Vorbereiten", "text": "Tab öffnen.", "areas": ["db"] },
                  { "id": "s-01", "section": "A", "title": "Eins", "tab": "Datenbank", "areas": ["db", "csv"],
                    "text": "Siehe [Tab](tab:Datenbank) und [Abfrage](sql:SELECT%20*%20FROM%20t)." },
                  { "id": "s-02", "section": "A", "title": "Zwei", "restart": true, "areas": [], "text": "Neustart." } ] })"));
            geschrieben = geschrieben && schreibeDatei(composer, "# Composer\n\n- [ ] **C1 Eins:** Text.\n");
            geschrieben = geschrieben && schreibeDatei(wurzel.filePath(QStringLiteral("md/Notiz.md")),
                                                       "# Notiz\n\n- [ ] **N1 Eins:** passt nicht auf das Muster.\n");
            pruefe(geschrieben, QStringLiteral("Projekt für den Selbsttest angelegt"));

            // Der Ordner einer Liste genügt: die Projektdatei wird aufwärts gefunden.
            setzeOrdner(wurzel.filePath(QStringLiteral("listen")), studio);
            pruefe(projekt().gueltig() && QFileInfo(projekt().pfad) == QFileInfo(projektDatei),
                   QStringLiteral("Projektdatei vom Listenordner aufwärts gefunden"));
            pruefe(m_combo->count() == 2 && m_combo->findData(studio) >= 0 && m_combo->findData(composer) >= 0,
                   QStringLiteral("Listen aus zwei Ordnern; das Muster gilt, ein fehlender Ordner wird übergangen (gezählt %1)")
                       .arg(m_combo->count()));
            pruefe(QFileInfo(logOrdner(studio)) == QFileInfo(wurzel.filePath(QStringLiteral("ablage/studio")))
                   && QFileInfo(logOrdner(composer)) == QFileInfo(wurzel.filePath(QStringLiteral("md/sichttest-logs"))),
                   QStringLiteral("Ablage je Listenordner; ohne Angabe sichttest-logs neben der Liste"));

            const int v0 = findeSchritt(QStringLiteral("V0"));
            pruefe(m_p.pfad == studio && m_p.anwendung == g && v0 == 0 && m_idx == 0
                   && aktionenVon(m_p, v0).size() == 3 && m_p.nachbereitung.size() == 1
                   && schritt(v0).value(QStringLiteral("text")).toString().contains(QStringLiteral("Test-DB »studiotest« einrichten"))
                   && schritt(v0).value(QStringLiteral("text")).toString().contains(QStringLiteral("Öffnen: examples/demo.project.json")),
                   QStringLiteral("test_db und setup werden die Vorbereitung der Liste: Text und drei Aktionen; Anwendung aus dem Projekt"));
            pruefe(aktionenVon(m_p, findeSchritt(QStringLiteral("s-01"))).size() == 1
                   && aktionenVon(m_p, findeSchritt(QStringLiteral("s-02"))).at(0).toObject()
                          .value(QStringLiteral("aktion")).toString() == QLatin1String("ende")
                   && zaehle(m_p.schritte).offen == 2,
                   QStringLiteral("tab und restart werden Aktionen des Schritts; zwei Vorbereitungen zählen nicht mit"));
            {
                const Projekt merk = projekt();
                setzeProjekt({});
                const Protokoll ohne = lade(studio);
                setzeProjekt(merk);
                pruefe(ohne.schritte.size() == 4 && aktionenVon(ohne, 0).isEmpty() && aktionenVon(ohne, 2).isEmpty()
                       && ohne.nachbereitung.isEmpty()
                       && ohne.schritte.at(0).toObject().value(QStringLiteral("text")).toString().contains(QLatin1String("studiotest")),
                       QStringLiteral("ohne Abbildung werden die Felder gelesen und als Text gezeigt, ohne Aktion"));
            }

            // Start über das Projekt; die Gegenprobe erbt den Kanal aus der Umgebung.
            qputenv("SICHTTEST_KANAL", m_selbsttestKanal.toUtf8());
            qputenv("SICHTTEST_EXE", "gibt-es-nicht.exe");
            pruefe(klicke(QStringLiteral("▶ Exe starten"))
                   && warteBis([&] { return m_steuerung.anwendung(g) != nullptr; }, 10000),
                   QStringLiteral("»Exe starten« startet die Anwendung mit den Argumenten des Projekts, sie verbindet sich"));

            auto status = [](const QJsonArray& log, int n) { return log.at(n).toObject().value(QStringLiteral("status")).toString(); };
            auto text = [](const QJsonArray& log, int n) { return log.at(n).toObject().value(QStringLiteral("text")).toString(); };
            zeige(v0);
            klicke(QStringLiteral("▶ Ausführen"));
            warteBis([&] { return !m_lauf.aktiv; }, 15000);
            QJsonArray log = ausgeloest(QStringLiteral("V0"));
            pruefe(log.size() == 3 && status(log, 2) == QLatin1String("ok")
                   && text(log, 0).contains(QLatin1String("\"projekt\":\"P\"")) && text(log, 0).contains(QLatin1String("studiotest"))
                   && text(log, 2).contains(QLatin1String("examples/demo.project.json")),
                   QStringLiteral("Vorbereitung: Test-DB (mit seed) zuerst, dann Tabs schließen, dann öffnen"));

            m_ohneZwischenablage = true;
            zeige(findeSchritt(QStringLiteral("s-01")));
            folgeLink(QUrl(QStringLiteral("tab:Datenbank")));
            warteBis([&] { return !m_lauf.aktiv; }, 5000);
            folgeLink(QUrl(QStringLiteral("sql:SELECT%20*%20FROM%20t")));
            warteBis([&] { return !m_lauf.aktiv; }, 5000);
            log = ausgeloest(QStringLiteral("s-01"));
            pruefe(log.size() == 2 && text(log, 0).contains(QLatin1String("\"titel\":\"Datenbank\""))
                   && text(log, 1).contains(QLatin1String("SELECT * FROM t")) && m_kopiert == QLatin1String("SELECT * FROM t"),
                   QStringLiteral("Links tab: und sql: im Text lösen die abgebildete Aktion aus; die Abfrage geht in die Zwischenablage"));

            pruefe(bieteNachbereitungAn() && warteBis([&] { return !m_lauf.aktiv; }, 5000)
                   && m_p.nachbereitungLog.size() == 1 && status(m_p.nachbereitungLog, 0) == QLatin1String("ok"),
                   QStringLiteral("Nachbereitung der Liste läuft und steht im Lauf"));
            const QJsonObject aufPlatte = leseJson(m_logPfad);
            pruefe(QFileInfo(m_logPfad).absolutePath() == QFileInfo(wurzel.filePath(QStringLiteral("ablage/studio"))).absoluteFilePath()
                   && aufPlatte.value(QStringLiteral("teardown_actions")).toArray().size() == 1
                   && aufPlatte.value(QStringLiteral("setup")).toObject().value(QStringLiteral("close_all_tabs")).toBool()
                   && aufPlatte.value(QStringLiteral("test_db")).toObject().value(QStringLiteral("name")).toString() == QLatin1String("studiotest")
                   && aufPlatte.value(QStringLiteral("steps")).toArray().at(2).toObject().value(QStringLiteral("areas")).toArray().size() == 2,
                   QStringLiteral("Testlog liegt in der Ablage des Projekts und trägt setup, test_db, areas und teardown_actions"));

            zeige(findeSchritt(QStringLiteral("s-02")));
            klicke(QStringLiteral("▶ Herstellen"));
            pruefe(warteBis([&] { return !m_lauf.aktiv && m_steuerung.anwendung(g) == nullptr; }, 10000)
                   && status(ausgeloest(QStringLiteral("s-02")), 0) == QLatin1String("ok"),
                   QStringLiteral("restart: die abgebildete Aktion beendet die Anwendung, ohne dass der Tester einen Fehler meldet"));

            // Meldet sich eine Anwendung mit einer Projektdatei im hallo, öffnet der Tester deren Listen.
            const QDir zwei(d.filePath(QStringLiteral("projekt2")));
            QDir(zwei.absolutePath()).removeRecursively();
            const QString projektZwei = zwei.filePath(QStringLiteral("sichttest.projekt.json"));
            const QJsonObject zweiJson{
                { QStringLiteral("anwendung"), g },
                { QStringLiteral("start"), QJsonObject{ { QStringLiteral("exe"), gegenprobe },
                      { QStringLiteral("argumente"), QJsonArray{ QStringLiteral("keine.dll"), QStringLiteral("nichts") } } } } };
            schreibeDatei(projektZwei, QJsonDocument(zweiJson).toJson());
            schreibeDatei(zwei.filePath(QStringLiteral("Zwei.md")), "# Zwei\n\n- [ ] **Z1 Eins:** Text.\n");
            QProcess app;
            starteGegenprobe(app, QStringLiteral("anwendung"), m_selbsttestKanal, projektZwei);
            pruefe(warteBis([&] { return m_steuerung.anwendung(g) != nullptr; }, 10000)
                   && QFileInfo(projekt().pfad) == QFileInfo(projektZwei) && m_p.pfad.endsWith(QLatin1String("Zwei.md")),
                   QStringLiteral("hallo nennt eine Projektdatei: der Tester öffnet deren Listen (ohne listen: der Ordner der Datei)"));
            bool beendet = false;
            m_steuerung.rufe(g, QStringLiteral("ende"), {}, {}, 5000, [&beendet](const QString&, const QString&) { beendet = true; });
            warteBis([&] { return beendet && app.state() == QProcess::NotRunning; }, 5000);
            if (app.state() != QProcess::NotRunning) app.kill();
            app.waitForFinished(2000);
            warteBis([&] { return m_steuerung.anwendung(g) == nullptr; }, 2000);

            // Gestartet, aber kein hallo: eigene Meldung (§6.3). Hier endet die Exe sofort.
            m_startFristMs = 300;
            m_gemeldeteExe.clear();
            klicke(QStringLiteral("▶ Exe starten"));
            pruefe(warteBis([&] { return m_aktionStand->text().contains(QLatin1String("gestartet, aber nicht verbunden")); }, 3000),
                   QStringLiteral("gestartet, aber nicht verbunden: der Tester sagt es"));

            setzeProjekt({});
            m_steuerung.beiAenderung = nullptr;
            m_steuerung.beiMeldung = nullptr;
        }

        static constexpr int kNachbereitung = -2;   // Lauf.idx: die Aktionen gehören zur Liste, nicht zu einem Punkt

        QString   m_kopiert;                  // zuletzt in die Zwischenablage gelegte Abfrage
        bool      m_ohneZwischenablage = false;   // nur der Selbsttest: die Zwischenablage des Menschen bleibt
        QString   m_selbsttestKanal;
        bool      m_nachbereitet = false;     // in diesem Lauf schon angeboten
        bool      m_schliesstDanach = false;
        QString   m_projektGenannt;           // ausdrücklich genannte Projektdatei (Aufruf oder hallo)
        QHash<QString, QString> m_gemeldeteExe;   // Anwendung → Exe aus ihrem letzten hallo
        QSet<qint64> m_gesehen;               // Pids, deren Verbindung schon ausgewertet ist
        QTimer    m_startFrist;
        int       m_startFristMs = 15000;

        Lauf      m_lauf;
        quint64   m_token = 0;
        bool      m_ohneRueckfrage = false;   // nur der Selbsttest: keine Rückfrage vor fragt_nach
        QTimer    m_neuFrist;
        QPushButton* m_herstellen = nullptr;
        QPushButton* m_vorbereitungBtn = nullptr;
        QPushButton* m_weiterPrep = nullptr;
        QLabel*      m_aktionStand = nullptr;
        QList<QPushButton*> m_urteil;

        Protokoll m_p;
        QString   m_ordner;
        QString   m_logPfad;
        QString   m_gestartet;
        QString   m_gespeichert;
        QString   m_exeDatei;
        QString   m_exeZeit;
        int       m_idx = -1;
        bool      m_fuellt = false;

        QComboBox*      m_combo = nullptr;
        QCheckBox*      m_vorn = nullptr;
        QCheckBox*      m_nurOffene = nullptr;
        QLabel*         m_kopfzeile = nullptr;
        QListWidget*    m_liste = nullptr;
        QLabel*         m_titel = nullptr;
        QLabel*         m_stand = nullptr;
        QTextBrowser*   m_text = nullptr;
        QPlainTextEdit* m_bemerkung = nullptr;
        QListWidget*    m_bilder = nullptr;
        QLabel*         m_summe = nullptr;
        QPushButton*    m_exeBtn = nullptr;
        QLabel*         m_steuerungZeile = nullptr;

        Steuerung m_steuerung;
    };

    // Schritte einer Datei ausgeben — zum Nachsehen, was der Leser erkennt.
    int pruefe(const QString& pfad)
    {
        QString fehler;
        const Protokoll p = lade(pfad, &fehler);
        QTextStream aus(stdout);
        aus.setEncoding(QStringConverter::Utf8);
        if (p.schritte.isEmpty())
        {
            aus << fehler << "\n";
            return 1;
        }
        aus << "Titel: " << p.titel << "\nExe:   " << p.exe << "\nSchritte: " << p.schritte.size() << "\n";
        for (const QJsonValue& v : p.schritte)
        {
            const QJsonObject s = v.toObject();
            aus << zeichen(s.value(QStringLiteral("result")).toString()) << " ["
                << s.value(QStringLiteral("section")).toString() << "] "
                << s.value(QStringLiteral("id")).toString() << " | "
                << s.value(QStringLiteral("title")).toString() << " | "
                << s.value(QStringLiteral("text")).toString().left(60).replace(QLatin1Char('\n'), QLatin1Char(' '))
                << "\n";
        }
        return 0;
    }
}

int main(int argc, char** argv)
{
    QApplication app(argc, argv);
    QCoreApplication::setOrganizationName(QStringLiteral("SichtTestHelper"));
    QCoreApplication::setApplicationName(QStringLiteral("Sichttest"));

    // --steuerung: so startet die DLL den Tester, wenn keiner lauscht (Konzept §4).
    // Er lauscht ohnehin; das Argument ist kein Ziel.
    QStringList args = QCoreApplication::arguments();
    args.removeAll(QStringLiteral("--steuerung"));
    if (args.size() >= 3 && args.at(1) == QLatin1String("--pruefe"))
        return pruefe(args.at(2));

    if (args.size() >= 3 && args.at(1) == QLatin1String("--selbsttest"))
    {
        // Eigene Einstellungen, damit der Test den gemerkten Ordner nicht verstellt.
        QCoreApplication::setApplicationName(QStringLiteral("Sichttest-Selbsttest"));
        Fenster fenster;
        return fenster.selbsttest(args.at(2));
    }

    // --schnapp <png> [datei|ordner]: Fenster aufbauen, als Bild ablegen, beenden.
    if (args.size() >= 3 && args.at(1) == QLatin1String("--schnapp"))
    {
        Fenster fenster;
        fenster.starte(args.size() >= 4 ? args.at(3) : QString());
        fenster.show();
        QApplication::processEvents();
        return fenster.grab().save(args.at(2)) ? 0 : 1;
    }

    Fenster fenster;
    fenster.starte(args.size() >= 2 ? args.at(1) : QString());
    fenster.show();
    return QApplication::exec();
}
