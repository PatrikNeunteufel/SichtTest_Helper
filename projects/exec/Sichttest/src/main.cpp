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
#include <QSettings>
#include <QShortcut>
#include <QSplitter>
#include <QTextBrowser>
#include <QTextStream>
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

#ifdef Q_OS_WIN
            fehler += selbsttestSteuerung(aus);
#endif

            aus << (fehler == 0 ? "Selbsttest bestanden" : "Selbsttest GESCHEITERT") << "\n";
            return fehler;
        }

    protected:
        void closeEvent(QCloseEvent* e) override
        {
            if (merkeBemerkung()) schreibe();
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
            m_text->setOpenExternalLinks(true);
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
            m_steuerung.beiAenderung = [this]() { m_steuerungZeile->setText(m_steuerung.zustandsText()); };
            m_steuerung.beiMeldung = [this](const QString& anwendung, const QString& text) {
                m_steuerungZeile->setText(QStringLiteral("%1 — %2: %3")
                                              .arg(m_steuerung.zustandsText(), anwendung, text));
            };

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
            connect(m_exeBtn, &QPushButton::clicked, this, [this]() {
                if (!m_exeDatei.isEmpty())
                    QProcess::startDetached(m_exeDatei, {}, QFileInfo(m_exeDatei).absolutePath());
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
            const QStringList pfade = ordner.isEmpty() ? QStringList() : findeProtokolle(ordner);
            for (const QString& p : pfade)
            {
                const Protokoll kurz = lade(p);
                const Zaehler z = zaehleMitLauf(kurz);
                m_combo->addItem(QStringLiteral("%1   ·   ○ %2  ✗ %3   (%4)")
                                     .arg(QFileInfo(p).fileName()).arg(z.offen).arg(z.fail)
                                     .arg(kurz.titel), p);
            }
            if (pfade.isEmpty())
            {
                m_p = {};
                m_idx = -1;
                fuelleListe();
                m_kopfzeile->setText(ordner.isEmpty()
                    ? QStringLiteral("Kein Ordner gewählt — über »Ordner…« einen Ordner mit Sichttest-Listen öffnen.")
                    : QStringLiteral("In %1 liegt keine Liste mit Punkten der Form »- [ ] **A1 Titel:** Text«.")
                          .arg(QDir::toNativeSeparators(ordner)));
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
            m_p = p;
            m_gespeichert.clear();
            m_exeDatei = findeExeDatei(m_p.exe, pfad);
            m_exeZeit = m_exeDatei.isEmpty() ? QString()
                : QFileInfo(m_exeDatei).lastModified().toString(QStringLiteral("dd.MM.yyyy HH:mm"));
            m_exeBtn->setVisible(!m_exeDatei.isEmpty());
            m_exeBtn->setToolTip(QDir::toNativeSeparators(m_exeDatei));
            QSettings().setValue(QStringLiteral("protokoll"), pfad);

            QString kopf = QStringLiteral("<b>%1</b>").arg(m_p.titel.toHtmlEscaped());
            if (!m_p.exe.isEmpty())
                kopf += m_exeDatei.isEmpty()
                    ? QStringLiteral(" · Exe nicht gefunden: %1").arg(m_p.exe.toHtmlEscaped())
                    : QStringLiteral(" · Exe vom %1").arg(m_exeZeit);
            kopf += lauf.isEmpty() ? QStringLiteral(" · neuer Lauf")
                                   : QStringLiteral(" · Lauf vom %1 fortgesetzt").arg(m_gestartet);
            m_kopfzeile->setText(kopf);
            m_kopfzeile->setToolTip(m_p.beschreibung);

            m_idx = -1;
            fuelleListe();
            const int erster = naechsterOffene(-1);
            zeige(erster >= 0 ? erster : 0);
            aktualisiereSumme();
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
        }

        void zeigeStand(const QJsonObject& s)
        {
            const QString r = s.value(QStringLiteral("result")).toString();
            static const QHash<QString, QString> namen = {
                { QStringLiteral("pass"), QStringLiteral("Pass") },
                { QStringLiteral("pass_remark"), QStringLiteral("Pass mit Befund") },
                { QStringLiteral("fail"), QStringLiteral("Fail") },
                { QStringLiteral("skip"), QStringLiteral("übersprungen") },
            };
            QString t = QStringLiteral("%1 · %2 von %3 · ")
                .arg(s.value(QStringLiteral("section")).toString())
                .arg(m_idx + 1).arg(m_p.schritte.size());
            t += zeichen(r) + QLatin1Char(' ') + namen.value(r, QStringLiteral("offen"));
            const QString wann = s.value(QStringLiteral("rated")).toString();
            if (!wann.isEmpty()) t += QStringLiteral(" (%1)").arg(wann);
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
            if (m_idx < 0) return;
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

            const int weiter = ergebnis == QLatin1String("open") ? -1 : naechsterOffene(m_idx);
            if (weiter >= 0)
            {
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
