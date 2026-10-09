// Gegenprobe — kleine Anwendung ohne Qt. Sie lädt die DLL so, wie es eine
// geprüfte Anwendung tut (Konzept §8): zur Laufzeit, mit vollem Pfad, ohne
// gegen sie gelinkt zu sein. Der Selbsttest von Sichttest.exe startet sie.
//
// Aufruf:  Gegenprobe <dll> <szenario>
//            lokal      Prüfungen ohne Tester (Fassungen, Ablehnung durch S, kein Tester)
//            anwendung  meldet Aktionen an und läuft, bis der Tester »ende« ruft
//            kopf       wie anwendung, aber über sichttest_steuerung.hpp
//            start      verbindet nur — die DLL startet dabei den Tester, falls keiner lauscht
//          Exit = gescheiterte Prüfungen; 100 = DLL nicht geladen,
//          101 = Aufruf falsch, 102 = ein Einstieg fehlt in der DLL
//
// Der Kanal kommt aus SICHTTEST_KANAL, der Tester aus SICHTTEST_EXE (beides liest die DLL).

#include <sichttest_steuerung.hpp>

#include <shellapi.h>

#include <cstdio>
#include <cstring>
#include <stdexcept>
#include <string>

#pragma comment(lib, "shell32")

namespace
{
    // Die Einstiege der DLL, einzeln geholt (§8).
    struct Dll
    {
        decltype(&sts_fassung) fassung = nullptr;
        decltype(&sts_oeffne) oeffne = nullptr;
        decltype(&sts_melde_aktion) melde_aktion = nullptr;
        decltype(&sts_setze_wecker) setze_wecker = nullptr;
        decltype(&sts_verbinde) verbinde = nullptr;
        decltype(&sts_pumpe) pumpe = nullptr;
        decltype(&sts_antwort_text) antwort_text = nullptr;
        decltype(&sts_melde) melde = nullptr;
        decltype(&sts_zustand) zustand = nullptr;
        decltype(&sts_letzter_fehler) letzter_fehler = nullptr;
        decltype(&sts_schliesse) schliesse = nullptr;
    };

    Dll d;
    int g_fehler = 0;
    HANDLE g_weck = nullptr;
    bool g_ende = false;
    sts_sitzung* g_sitzung = nullptr;

    void pruefe(bool gut, const char* was)
    {
        std::printf("%s %s\n", gut ? "ok  " : "FAIL", was);
        if (!gut) ++g_fehler;
    }

    template <typename F>
    bool hole(HMODULE dll, const char* name, F& ziel)
    {
        ziel = reinterpret_cast<F>(reinterpret_cast<void*>(GetProcAddress(dll, name)));
        if (!ziel) std::fprintf(stderr, "%s fehlt in der DLL\n", name);
        return ziel != nullptr;
    }

    void wecke(void*) { SetEvent(g_weck); }

    sts_status melde(const char* name, const char* beschreibung, const char* parameter, uint32_t schalter,
                     sts_rueckruf rueckruf)
    {
        sts_aktion a{};
        a.groesse = sizeof a;
        a.name = name;
        a.beschreibung = beschreibung;
        a.parameter = parameter;
        a.schalter = schalter;
        a.rueckruf = rueckruf;
        return d.melde_aktion(g_sitzung, &a);
    }

    sts_status echo(void*, const char*, const char* argumente, sts_antwort* antwort)
    {
        d.antwort_text(antwort, argumente);
        return STS_OK;
    }

    sts_status scheitert(void*, const char*, const char*, sts_antwort* antwort)
    {
        d.antwort_text(antwort, "absichtlich gescheitert");
        return STS_FEHLER;
    }

    sts_status ungueltig(void*, const char*, const char*, sts_antwort*) { return STS_UNGUELTIG; }

    sts_status schlaeft(void*, const char*, const char*, sts_antwort*)
    {
        Sleep(800);
        return STS_OK;
    }

    // Wiedereintritt (§3): ein Rückruf, der selbst pumpt, darf nichts auslösen.
    sts_status pumpt(void*, const char*, const char*, sts_antwort* antwort)
    {
        d.antwort_text(antwort, std::to_string(d.pumpe(g_sitzung)).c_str());
        return STS_OK;
    }

    sts_status meldet(void*, const char*, const char*, sts_antwort*)
    {
        return d.melde(g_sitzung, "Meldung aus der Aktion");
    }

    sts_status spaet(void*, const char*, const char*, sts_antwort*)
    {
        return melde("nachgemeldet", "Erst nach dem Verbinden angemeldet", "", 0, echo);
    }

    sts_status ende(void*, const char*, const char*, sts_antwort*)
    {
        g_ende = true;
        return STS_OK;
    }

    sts_konfig konfig()
    {
        sts_konfig k{};
        k.groesse = sizeof k;
        k.s_major = STS_S_MAJOR;
        k.s_minor = STS_S_MINOR;
        k.anwendung = "Gegenprobe";
        k.version = "1.2.3";
        return k;
    }

    void lokal()
    {
        uint16_t sMajor = 0, sMinor = 0, pMin = 0, pMax = 0;
        const char* produkt = nullptr;
        d.fassung(&sMajor, &sMinor, &pMin, &pMax, &produkt);
        pruefe(sMajor == STS_S_MAJOR && sMinor == STS_S_MINOR, "sts_fassung: S der DLL ist S des Kopfs");
        pruefe(pMin == STS_P_MAJOR && pMax == STS_P_MAJOR && produkt && std::strcmp(produkt, STS_PRODUKT) == 0,
               "sts_fassung: P und Produkt der DLL sind die des Kopfs");

        sts_konfig k = konfig();
        k.s_major = STS_S_MAJOR + 1;
        pruefe(d.oeffne(&k, &g_sitzung) == STS_UNVERTRAEGLICH && !g_sitzung && *d.letzter_fehler(nullptr),
               "sts_oeffne: andere große Fassung von S ist unverträglich, mit Klartext");
        std::printf("info %s\n", d.letzter_fehler(nullptr));
        k = konfig();
        k.s_minor = STS_S_MINOR + 1;
        pruefe(d.oeffne(&k, &g_sitzung) == STS_UNVERTRAEGLICH && !g_sitzung,
               "sts_oeffne: kleine Fassung der Anwendung über der der DLL ist unverträglich");
        pruefe(d.oeffne(nullptr, &g_sitzung) == STS_UNGUELTIG, "sts_oeffne: ohne Konfiguration ungültig");
        k = konfig();
        pruefe(d.oeffne(&k, &g_sitzung) == STS_OK && g_sitzung, "sts_oeffne: passende Fassung wird angenommen");
        if (!g_sitzung) return;

        pruefe(melde("Mit Leerzeichen", "", "", 0, echo) == STS_UNGUELTIG,
               "sts_melde_aktion: Name außerhalb [a-z0-9_.] ist ungültig");
        pruefe(melde("echo", "", "", 0, echo) == STS_OK && melde("echo", "", "", 0, echo) == STS_UNGUELTIG,
               "sts_melde_aktion: derselbe Name zweimal ist ungültig");
        pruefe(melde("beides", "", "", STS_FRAGT_NACH | STS_WARTET_AUF_MENSCH, echo) == STS_UNGUELTIG,
               "sts_melde_aktion: fragt_nach und wartet_auf_mensch zugleich ist ungültig");
        pruefe(d.zustand(g_sitzung) == STS_GETRENNT && d.pumpe(g_sitzung) == 0
               && d.melde(g_sitzung, "x") == STS_KEIN_TESTER,
               "vor dem Verbinden: getrennt, nichts zu pumpen, sts_melde sagt kein Tester");

        pruefe(d.verbinde(g_sitzung) == STS_OK, "sts_verbinde kehrt sofort zurück");
        const ULONGLONG bis = GetTickCount64() + 3000;
        while (d.zustand(g_sitzung) == STS_VERBINDET && GetTickCount64() < bis) Sleep(20);
        pruefe(d.zustand(g_sitzung) == STS_GETRENNT && *d.letzter_fehler(g_sitzung),
               "ohne Tester: Zustand getrennt, mit Klartext");
        std::printf("info %s\n", d.letzter_fehler(g_sitzung));
        d.schliesse(g_sitzung);
    }

    void anwendung()
    {
        const sts_konfig k = konfig();
        if (d.oeffne(&k, &g_sitzung) != STS_OK) { pruefe(false, "sts_oeffne"); return; }
        d.setze_wecker(g_sitzung, wecke, nullptr);
        bool gut = melde("echo", "Schickt die Argumente als Text zurück", "beliebig", 0, echo) == STS_OK;
        gut = gut && melde("scheitert", "Scheitert immer", "", 0, scheitert) == STS_OK;
        gut = gut && melde("ungueltig", "Lehnt die Argumente ab", "", 0, ungueltig) == STS_OK;
        gut = gut && melde("schlaeft", "Antwortet erst nach 800 ms", "", 0, schlaeft) == STS_OK;
        gut = gut && melde("pumpt", "Ruft sts_pumpe() im Rückruf", "", 0, pumpt) == STS_OK;
        gut = gut && melde("meldet", "Schickt eine Meldung", "", 0, meldet) == STS_OK;
        gut = gut && melde("spaet", "Meldet eine weitere Aktion an", "", 0, spaet) == STS_OK;
        gut = gut && melde("ende", "Beendet die Gegenprobe", "", STS_BEENDET_ANWENDUNG, ende) == STS_OK;
        pruefe(gut, "acht Aktionen angemeldet");
        d.verbinde(g_sitzung);

        const ULONGLONG bis = GetTickCount64() + 30000;
        bool abgelehnt = false;
        while (!g_ende && !abgelehnt && GetTickCount64() < bis)
        {
            WaitForSingleObject(g_weck, 100);
            d.pumpe(g_sitzung);
            const int z = d.zustand(g_sitzung);
            abgelehnt = z == STS_ABGELEHNT;
            if (z == STS_GETRENNT)
            {
                std::printf("info %s\n", d.letzter_fehler(g_sitzung));
                break;
            }
        }
        if (abgelehnt) std::printf("info %s\n", d.letzter_fehler(g_sitzung));
        else pruefe(g_ende, "der Tester hat die Aktion ende gerufen");
        d.schliesse(g_sitzung);
    }

    // Dasselbe über den Kopf für C++ (sichttest_steuerung.hpp) statt über die C-Schnittstelle.
    void kopf(const wchar_t* dllPfad)
    {
        std::string grund;
        pruefe(!sichttest::Sitzung::lade("GegenprobeKopf", "1.2.3", &grund, L"C:\\gibt\\es\\nicht.dll")
               && !grund.empty(),
               "Kopf: eine fehlende DLL liefert leer, mit Klartext");
        std::printf("info %s\n", grund.c_str());

        const std::unique_ptr<sichttest::Sitzung> s = sichttest::Sitzung::lade("GegenprobeKopf", "1.2.3", &grund, dllPfad);
        pruefe(s != nullptr, "Kopf: DLL geladen, Sitzung angelegt");
        if (!s) return;

        bool fertig = false;
        bool gut = s->aktion("echo", "Schickt die Argumente zurück", "beliebig",
                             [](const char* argumente) { return sichttest::ok(argumente); });
        gut = gut && s->aktion("wirft", "Wirft eine Ausnahme", "",
                               [](const char*) -> sichttest::Ergebnis { throw std::runtime_error("absichtlich"); });
        gut = gut && s->aktion("ende", "Beendet die Gegenprobe", "",
                               [&fertig](const char*) { fertig = true; return sichttest::ok(); },
                               STS_BEENDET_ANWENDUNG);
        pruefe(gut, "Kopf: drei Aktionen als Lambdas angemeldet");
        pruefe(!s->aktion("echo", "", "", [](const char*) { return sichttest::ok(); }) && !s->letzterFehler().empty(),
               "Kopf: derselbe Name zweimal wird abgewiesen, mit Klartext");

        s->setzeWecker([] { SetEvent(g_weck); });
        s->verbinde();
        const ULONGLONG bis = GetTickCount64() + 30000;
        while (!fertig && GetTickCount64() < bis)
        {
            WaitForSingleObject(g_weck, 100);
            s->pumpe();
            if (s->zustand() == STS_GETRENNT || s->zustand() == STS_ABGELEHNT)
            {
                std::printf("info %s\n", s->letzterFehler().c_str());
                break;
            }
        }
        pruefe(fertig, "Kopf: der Tester hat die Aktion ende gerufen");
    }

    void start()
    {
        const sts_konfig k = konfig();
        if (d.oeffne(&k, &g_sitzung) != STS_OK) { pruefe(false, "sts_oeffne"); return; }
        d.verbinde(g_sitzung);
        const ULONGLONG bis = GetTickCount64() + 15000;
        while (d.zustand(g_sitzung) == STS_VERBINDET && GetTickCount64() < bis) Sleep(50);
        pruefe(d.zustand(g_sitzung) == STS_VERBUNDEN, "die DLL hat den Tester gestartet und ist verbunden");
        if (d.zustand(g_sitzung) != STS_VERBUNDEN) std::printf("info %s\n", d.letzter_fehler(g_sitzung));
        d.schliesse(g_sitzung);
    }
}

int main()
{
    int argc = 0;
    wchar_t** argv = CommandLineToArgvW(GetCommandLineW(), &argc);
    if (!argv || argc < 3)
    {
        std::fputs("Aufruf: Gegenprobe <dll> lokal|anwendung|kopf|start\n", stderr);
        return 101;
    }

    const HMODULE dll = LoadLibraryExW(argv[1], nullptr, LOAD_WITH_ALTERED_SEARCH_PATH);
    if (!dll)
    {
        std::fprintf(stderr, "DLL nicht geladen (Windows-Fehler %lu)\n", GetLastError());
        return 100;
    }
    // Jeden holen, damit alle fehlenden genannt werden.
    int da = 0;
    da += hole(dll, "sts_fassung", d.fassung);
    da += hole(dll, "sts_oeffne", d.oeffne);
    da += hole(dll, "sts_melde_aktion", d.melde_aktion);
    da += hole(dll, "sts_setze_wecker", d.setze_wecker);
    da += hole(dll, "sts_verbinde", d.verbinde);
    da += hole(dll, "sts_pumpe", d.pumpe);
    da += hole(dll, "sts_antwort_text", d.antwort_text);
    da += hole(dll, "sts_melde", d.melde);
    da += hole(dll, "sts_zustand", d.zustand);
    da += hole(dll, "sts_letzter_fehler", d.letzter_fehler);
    da += hole(dll, "sts_schliesse", d.schliesse);
    if (da != 11) return 102;

    g_weck = CreateEventW(nullptr, FALSE, FALSE, nullptr);
    const std::wstring szenario = argv[2];
    if (szenario == L"lokal") lokal();
    else if (szenario == L"anwendung") anwendung();
    else if (szenario == L"kopf") kopf(argv[1]);
    else if (szenario == L"start") start();
    else return 101;

    std::fflush(stdout);
    FreeLibrary(dll);
    LocalFree(argv);
    return g_fehler;
}
