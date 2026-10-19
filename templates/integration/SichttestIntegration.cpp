// SichttestIntegration.cpp — template: let Sichttest drive a Qt application.
//
// Copy into your application and adapt. Rules (docs/Integration_Guide.md):
//   - Include the Sichttest headers in exactly ONE .cpp, this one (they include
//     <windows.h> with WIN32_LEAN_AND_MEAN and NOMINMAX).
//   - Everything stays behind SICHTTEST_VORHANDEN: the define is set only when the
//     package was found, so the application also builds without it.
//   - Load only in the --testing branch; an ordinary start loads nothing.
//   - Connect (verbinde) only once the application can be driven, i.e. after it has
//     restored its start state. No callback runs before that.
//   - Callbacks run in the GUI thread. An action that asks in the application (a
//     dialog) carries STS_WARTET_AUF_MENSCH; one that may discard unsaved work but does
//     not ask itself carries STS_FRAGT_NACH (never both).
//
// Names in the interface are German: Steuerung = control, lade = load, aktion = action,
// verbinde = connect, meldeZustand = report state, beiEnde = on end, Ergebnis = result,
// fehler = error, ungueltig = invalid.

#include "SichttestIntegration.hpp"

#include "MainWindow.hpp"   // your window

#include <QCoreApplication>
#include <QJsonObject>

#ifdef SICHTTEST_VORHANDEN
#  include <sichttest_steuerung_qt.hpp>
#endif

struct SichttestIntegration::Impl
{
#ifdef SICHTTEST_VORHANDEN
    std::unique_ptr<sichttest::Steuerung> steuerung;
#endif
};

SichttestIntegration::SichttestIntegration(MainWindow& window, const QString& version)
    : m_impl(std::make_unique<Impl>())
{
#ifdef SICHTTEST_VORHANDEN
    if (!QCoreApplication::arguments().contains(QStringLiteral("--testing"))) return;

    QString reason;
    // "MyApp" = the name your lists and the project file use (anwendung).
    m_impl->steuerung = sichttest::Steuerung::lade(QStringLiteral("MyApp"), version, &reason);
    sichttest::Steuerung* const st = m_impl->steuerung.get();
    if (!st)
    {
        // DLL missing or incompatible: say so once and run on without control.
        window.showStatus(QStringLiteral("--testing without Sichttest: ") + reason);
        return;
    }

    // An action: name [a-z0-9_.], one-sentence description, parameter hint, callback, flags.
    st->aktion(QStringLiteral("open_file"), QStringLiteral("Opens a project file"), QStringLiteral("path=<file>"),
               [&window](const QJsonObject& args) -> sichttest::Ergebnis {
                   const QString path = args.value(QStringLiteral("path")).toString();
                   if (path.isEmpty()) return sichttest::ungueltig(QStringLiteral("path is missing"));
                   // Repeatable: answer ok if the file is already open (a resumed run prepares again).
                   return window.openFile(path) ? sichttest::ok()
                                                : sichttest::fehler(QStringLiteral("cannot open ") + path);
               },
               STS_FRAGT_NACH);   // replaces unsaved work; the tester asks first

    st->aktion(QStringLiteral("show_tab"), QStringLiteral("Brings a tab to the front"), QStringLiteral("title=<tab title>"),
               [&window](const QJsonObject& args) -> sichttest::Ergebnis {
                   return window.showTab(args.value(QStringLiteral("title")).toString())
                              ? sichttest::ok()
                              : sichttest::fehler(QStringLiteral("no tab with this title"));
               });

    // A state the application sets up and tears down (S 1.1): report it whenever it changes.
    // The tester offers the teardown that names "wenn": "test_db" while the state is standing.
    st->aktion(QStringLiteral("testdb_setup"), QStringLiteral("Sets up the test database"),
               QStringLiteral("name=<db> resumed=<true|false>"),
               [&window, st](const QJsonObject& args) -> sichttest::Ergebnis {
                   const QString name = args.value(QStringLiteral("name")).toString();
                   // Never answer ok without a test database: the tester derives its state from it.
                   if (!window.setUpTestDb(name)) return sichttest::fehler(QStringLiteral("test database not set up"));
                   st->meldeZustand(QStringLiteral("test_db"), true, name);
                   return sichttest::ok(QStringLiteral("test database ") + name);
               },
               STS_WARTET_AUF_MENSCH);   // asks in the application; no time limit in the tester

    st->aktion(QStringLiteral("testdb_teardown"), QStringLiteral("Removes the test database"), QString(),
               [&window, st](const QJsonObject&) -> sichttest::Ergebnis {
                   window.tearDownTestDb();
                   st->meldeZustand(QStringLiteral("test_db"), false);
                   return sichttest::ok();
               });

    // Report a state that is still standing from an earlier session right away.
    if (window.testDbActive()) st->meldeZustand(QStringLiteral("test_db"), true, window.testDbName());

    // Optional (S 1.1): learn that the tester went away. geordnet = closed, otherwise lost.
    st->beiEnde([&window](bool geordnet) {
        window.showStatus(geordnet ? QStringLiteral("Sichttest closed") : QStringLiteral("Sichttest connection lost"));
    });

    st->verbinde();   // from here on the application can be driven; returns at once
#else
    (void)window;
    (void)version;
#endif
}

SichttestIntegration::~SichttestIntegration() = default;
