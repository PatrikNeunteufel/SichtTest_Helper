// GegenprobeQt — kleine Qt-Anwendung für den Selbsttest. Sie benutzt
// sichttest_steuerung_qt.hpp so, wie es LumiViz und das Comm Studio tun
// werden: Aktionen als Lambdas mit QJsonObject, Zustellung im GUI-Thread.
//
// Aufruf:  GegenprobeQt <dll>
//          Exit = gescheiterte Prüfungen. Läuft, bis der Tester »ende« ruft.
//
// Der Kanal kommt aus SICHTTEST_KANAL (liest die DLL).

#include <sichttest_steuerung_qt.hpp>

#include <QCoreApplication>
#include <QThread>
#include <QTimer>

#include <cstdio>
#include <stdexcept>

namespace
{
    int g_fehler = 0;

    void pruefe(bool gut, const char* was)
    {
        std::printf("%s %s\n", gut ? "ok  " : "FAIL", was);
        if (!gut) ++g_fehler;
    }
}

int main(int argc, char** argv)
{
    QCoreApplication app(argc, argv);
    const QStringList args = QCoreApplication::arguments();
    if (args.size() < 2)
    {
        std::fputs("Aufruf: GegenprobeQt <dll>\n", stderr);
        return 101;
    }

    QString grund;
    pruefe(!sichttest::Steuerung::lade("GegenprobeQt", "1.2.3", &grund, QStringLiteral("C:/gibt/es/nicht.dll"))
           && !grund.isEmpty(),
           "Qt-Kopf: eine fehlende DLL liefert leer, mit Klartext");

    const std::unique_ptr<sichttest::Steuerung> steuerung
        = sichttest::Steuerung::lade("GegenprobeQt", "1.2.3", &grund, args.at(1));
    pruefe(steuerung != nullptr, "Qt-Kopf: DLL geladen, Sitzung angelegt");
    pruefe(steuerung && steuerung->fassung().sMajor == STS_S_MAJOR && !steuerung->fassung().produkt.empty(),
           "Qt-Kopf: fassung() nennt S und Produkt der geladenen DLL");
    if (!steuerung)
    {
        std::printf("info %s\n", qUtf8Printable(grund));
        return g_fehler;
    }

    bool fertig = false;
    bool gut = steuerung->aktion("echo", "Schickt die Argumente zurück", "beliebig",
        [](const QJsonObject& arg) {
            return sichttest::ok(QString::fromUtf8(QJsonDocument(arg).toJson(QJsonDocument::Compact)));
        });
    gut = gut && steuerung->aktion("faden", "Sagt, ob die Aktion im GUI-Thread läuft", "",
        [](const QJsonObject&) -> sichttest::Ergebnis {
            return QThread::currentThread() == QCoreApplication::instance()->thread()
                 ? sichttest::ok("gui") : sichttest::fehler("nicht im GUI-Thread");
        });
    gut = gut && steuerung->aktion("wirft", "Wirft eine Ausnahme", "",
        [](const QJsonObject&) -> sichttest::Ergebnis { throw std::runtime_error("absichtlich"); });
    gut = gut && steuerung->aktion("ende", "Beendet die Gegenprobe", "",
        [&fertig](const QJsonObject&) {
            fertig = true;
            QTimer::singleShot(0, QCoreApplication::instance(), &QCoreApplication::quit);
            return sichttest::ok();
        },
        STS_BEENDET_ANWENDUNG);
    pruefe(gut, "Qt-Kopf: vier Aktionen als Lambdas angemeldet");

    steuerung->verbinde();
    QTimer::singleShot(30000, &app, &QCoreApplication::quit);
    QCoreApplication::exec();
    pruefe(fertig, "Qt-Kopf: der Tester hat die Aktion ende gerufen");
    if (!fertig) std::printf("info %s\n", qUtf8Printable(steuerung->letzterFehler()));
    std::fflush(stdout);
    return g_fehler;
}
