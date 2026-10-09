// Die Schnittstelle S (sichttest_steuerung.h) und das Protokoll P auf dem Kanal.
//
// Je Sitzung ein Faden: er verbindet, tauscht hallo/willkommen und liest danach
// Nachrichten des Testers. Aufrufe legt er in eine Warteschlange; ausgeführt
// werden sie nur in sts_pumpe(), also im Thread der Anwendung (Konzept §3).

#include "sichttest_steuerung.h"

#include "Json.hpp"
#include "Kanal.hpp"

#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>

#include <atomic>
#include <deque>
#include <map>
#include <mutex>
#include <string>
#include <thread>
#include <vector>

struct sts_antwort
{
    std::string text;
};

namespace
{
    using sts::KanalStatus;
    namespace json = sts::json;

    struct Aktion
    {
        std::string name;
        std::string beschreibung;
        std::string parameter;
        uint32_t schalter = 0;
        sts_rueckruf rueckruf = nullptr;
        void* nutzer = nullptr;
        bool gemeldet = false;
    };

    struct Aufruf
    {
        std::string id;          // Rohtext, geht unverändert in die Antwort
        std::string aktion;
        std::string argumente;   // Rohtext des JSON-Objekts
    };

    // Grund, warum sts_oeffne() gescheitert ist — dann gibt es keine Sitzung, die ihn trüge.
    thread_local std::string t_oeffneFehler;

    std::string utf8(const std::wstring& w)
    {
        if (w.empty()) return {};
        const int n = WideCharToMultiByte(CP_UTF8, 0, w.data(), static_cast<int>(w.size()), nullptr, 0, nullptr, nullptr);
        std::string s(static_cast<size_t>(n), '\0');
        WideCharToMultiByte(CP_UTF8, 0, w.data(), static_cast<int>(w.size()), s.data(), n, nullptr, nullptr);
        return s;
    }

    std::wstring umgebung(const wchar_t* name)
    {
        wchar_t puffer[32768];
        const DWORD n = GetEnvironmentVariableW(name, puffer, 32768);
        return n > 0 && n < 32768 ? std::wstring(puffer, n) : std::wstring();
    }

    // Derselbe Name wie KanalServer::kanalName() im Tester. SICHTTEST_KANAL
    // ersetzt ihn (Entwicklung und Selbsttest).
    std::wstring kanalName()
    {
        std::wstring name = umgebung(L"SICHTTEST_KANAL");
        if (!name.empty()) return name;
        name = L"sichttest-" + umgebung(L"USERNAME");
        for (wchar_t& c : name)
            if (c == L'\\') c = L'_';
        return name;
    }

    std::wstring modulPfad(HMODULE modul)
    {
        wchar_t puffer[32768];
        const DWORD n = GetModuleFileNameW(modul, puffer, 32768);
        return std::wstring(puffer, n);
    }

    // Suchregel §7.1: SICHTTEST_EXE, sonst <Ordner der DLL>\sichttest\Sichttest.exe.
    std::wstring testerExe()
    {
        std::wstring exe = umgebung(L"SICHTTEST_EXE");
        if (!exe.empty()) return exe;
        HMODULE dll = nullptr;
        GetModuleHandleExW(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS | GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,
                           reinterpret_cast<LPCWSTR>(&sts_fassung), &dll);
        exe = modulPfad(dll);
        const size_t schnitt = exe.find_last_of(L"\\/");
        exe = schnitt == std::wstring::npos ? std::wstring() : exe.substr(0, schnitt + 1);
        return exe + L"sichttest\\Sichttest.exe";
    }

    bool starteTester(std::string& grund)
    {
        const std::wstring exe = testerExe();
        if (GetFileAttributesW(exe.c_str()) == INVALID_FILE_ATTRIBUTES)
        {
            grund = "und Sichttest.exe liegt nicht unter " + utf8(exe);
            return false;
        }
        std::wstring zeile = L"\"" + exe + L"\" --steuerung";
        STARTUPINFOW si{};
        si.cb = sizeof si;
        PROCESS_INFORMATION pi{};
        if (!CreateProcessW(exe.c_str(), zeile.data(), nullptr, nullptr, FALSE, 0, nullptr, nullptr, &si, &pi))
        {
            grund = "und Sichttest.exe lässt sich nicht starten (Windows-Fehler " + std::to_string(GetLastError()) + ")";
            return false;
        }
        CloseHandle(pi.hThread);
        CloseHandle(pi.hProcess);
        return true;
    }

    bool nameGueltig(const char* name)
    {
        if (!name || !*name) return false;
        for (const char* p = name; *p; ++p)
            if (!((*p >= 'a' && *p <= 'z') || (*p >= '0' && *p <= '9') || *p == '_' || *p == '.')) return false;
        return true;
    }

    std::string alsJson(const Aktion& a)
    {
        std::string schalter;
        auto dazu = [&](uint32_t bit, const char* wort) {
            if (!(a.schalter & bit)) return;
            if (!schalter.empty()) schalter += ",";
            schalter += json::zitiert(wort);
        };
        dazu(STS_FRAGT_NACH, "fragt_nach");
        dazu(STS_NUR_ERSTLAUF, "nur_erstlauf");
        dazu(STS_WARTET_AUF_MENSCH, "wartet_auf_mensch");
        dazu(STS_BEENDET_ANWENDUNG, "beendet_anwendung");
        return "{\"name\":" + json::zitiert(a.name) + ",\"beschreibung\":" + json::zitiert(a.beschreibung)
             + ",\"parameter\":" + json::zitiert(a.parameter) + ",\"schalter\":[" + schalter + "]}";
    }

    const char* statusWort(sts_status st)
    {
        switch (st)
        {
        case STS_OK: return "ok";
        case STS_UNBEKANNT: return "unbekannt";
        case STS_UNGUELTIG: return "ungueltig";
        default: return "fehler";
        }
    }
}

struct sts_sitzung
{
    std::string anwendung;
    std::string version;
    std::string projektDatei;

    std::mutex m;                       // schützt die fünf folgenden
    std::vector<Aktion> aktionen;
    std::deque<Aufruf> wartend;
    std::string fehler;
    void (*wecker)(void*) = nullptr;
    void* weckerNutzer = nullptr;

    std::string fehlerAusgabe;          // was sts_letzter_fehler() zuletzt herausgab
    std::atomic<int> zustand{ STS_GETRENNT };
    std::atomic<bool> ende{ false };    // sts_schliesse() läuft
    std::atomic<bool> abbruch{ false }; // ein Schreiben ist gescheitert, die Verbindung ist hin
    std::atomic<bool> inRueckruf{ false };

    std::mutex sendeMutex;              // schützt das Schreiben und das Öffnen/Schließen des Kanals
    sts::Kanal kanal;
    std::thread faden;
};

namespace
{
    void setzeFehler(sts_sitzung* s, const std::string& text)
    {
        const std::lock_guard<std::mutex> sperre(s->m);
        s->fehler = text;
    }

    bool sende(sts_sitzung* s, const std::string& nachricht)
    {
        const std::lock_guard<std::mutex> sperre(s->sendeMutex);
        if (!s->kanal.offen()) return false;
        if (s->kanal.sende(nachricht) == KanalStatus::Ok) return true;
        s->abbruch = true;
        return false;
    }

    void schliesseKanal(sts_sitzung* s)
    {
        const std::lock_guard<std::mutex> sperre(s->sendeMutex);
        s->kanal.schliesse();
    }

    // Schickt, was seit hallo oder der letzten Nachmeldung dazukam.
    void meldeNach(sts_sitzung* s)
    {
        std::string liste;
        {
            const std::lock_guard<std::mutex> sperre(s->m);
            for (Aktion& a : s->aktionen)
            {
                if (a.gemeldet) continue;
                if (!liste.empty()) liste += ",";
                liste += alsJson(a);
                a.gemeldet = true;
            }
        }
        if (!liste.empty()) sende(s, "{\"nachricht\":\"aktionen\",\"aktionen\":[" + liste + "]}");
    }

    std::string hallo(sts_sitzung* s)
    {
        std::string liste;
        {
            const std::lock_guard<std::mutex> sperre(s->m);
            for (Aktion& a : s->aktionen)
            {
                if (!liste.empty()) liste += ",";
                liste += alsJson(a);
                a.gemeldet = true;
            }
        }
        std::string n = "{\"nachricht\":\"hallo\",\"p_min\":" + std::to_string(STS_P_MAJOR)
                      + ",\"p_max\":" + std::to_string(STS_P_MAJOR)
                      + ",\"s\":\"" + std::to_string(STS_S_MAJOR) + "." + std::to_string(STS_S_MINOR) + "\""
                      + ",\"produkt\":" + json::zitiert(STS_PRODUKT)
                      + ",\"anwendung\":" + json::zitiert(s->anwendung)
                      + ",\"version\":" + json::zitiert(s->version)
                      + ",\"exe\":" + json::zitiert(utf8(modulPfad(nullptr)))
                      + ",\"pid\":" + std::to_string(GetCurrentProcessId());
        if (!s->projektDatei.empty()) n += ",\"projekt_datei\":" + json::zitiert(s->projektDatei);
        return n + ",\"aktionen\":[" + liste + "]}";
    }

    std::string feldText(const std::map<std::string, std::string>& felder, const char* name)
    {
        std::string text;
        const auto it = felder.find(name);
        if (it != felder.end()) json::alsText(it->second, text);
        return text;
    }

    // Verbinden; lauscht keiner, den Tester starten und bis zu 10 s erneut versuchen (§4).
    bool verbinde(sts_sitzung* s)
    {
        const std::wstring name = kanalName();
        bool gestartet = false;
        const ULONGLONG bis = GetTickCount64() + 10000;
        for (;;)
        {
            if (s->ende) return false;
            KanalStatus st;
            {
                const std::lock_guard<std::mutex> sperre(s->sendeMutex);
                st = s->kanal.verbinde(name, 0);
                if (st == KanalStatus::Fehler) setzeFehler(s, s->kanal.schreibFehler());
            }
            if (st == KanalStatus::Ok) return true;
            if (st == KanalStatus::Fehler) return false;
            if (!gestartet)
            {
                std::string grund;
                if (!starteTester(grund))
                {
                    setzeFehler(s, "kein Tester lauscht, " + grund);
                    return false;
                }
                gestartet = true;
            }
            if (GetTickCount64() >= bis)
            {
                setzeFehler(s, "der gestartete Tester lauscht nach 10 s nicht");
                return false;
            }
            Sleep(100);
        }
    }

    // hallo schicken und auf willkommen oder abgelehnt warten. Liefert den neuen Zustand.
    int begruesse(sts_sitzung* s)
    {
        if (!sende(s, hallo(s)))
        {
            setzeFehler(s, "hallo ließ sich nicht schicken");
            return STS_GETRENNT;
        }
        std::string text;
        const ULONGLONG bis = GetTickCount64() + 5000;
        for (;;)
        {
            const KanalStatus st = s->kanal.lies(text, 200);
            if (st == KanalStatus::Ok) break;
            if (st == KanalStatus::Frist && GetTickCount64() < bis && !s->ende) continue;
            setzeFehler(s, st == KanalStatus::Frist ? "der Tester antwortet nicht auf hallo" : s->kanal.leseFehler());
            return STS_GETRENNT;
        }
        std::map<std::string, std::string> felder;
        json::zerlege(text, felder);
        const std::string nachricht = feldText(felder, "nachricht");
        if (nachricht == "abgelehnt")
        {
            setzeFehler(s, "vom Tester abgelehnt: " + feldText(felder, "grund"));
            return STS_ABGELEHNT;
        }
        if (nachricht != "willkommen")
        {
            setzeFehler(s, "unerwartete Antwort des Testers auf hallo");
            return STS_GETRENNT;
        }
        return STS_VERBUNDEN;
    }

    void lies(sts_sitzung* s)
    {
        std::string text;
        while (!s->ende && !s->abbruch)
        {
            const KanalStatus st = s->kanal.lies(text, 200);
            if (st == KanalStatus::Frist) continue;
            if (st != KanalStatus::Ok)
            {
                setzeFehler(s, s->kanal.leseFehler());
                return;
            }
            std::map<std::string, std::string> felder;
            json::zerlege(text, felder);
            const std::string nachricht = feldText(felder, "nachricht");
            if (nachricht == "tschuess")
            {
                setzeFehler(s, "der Tester hat sich verabschiedet");
                return;
            }
            const auto id = felder.find("id");
            if (nachricht == "aufruf")
            {
                Aufruf a;
                a.id = id != felder.end() ? id->second : "null";
                a.aktion = feldText(felder, "aktion");
                const auto arg = felder.find("argumente");
                a.argumente = arg != felder.end() ? arg->second : "{}";
                void (*wecker)(void*) = nullptr;
                void* nutzer = nullptr;
                {
                    const std::lock_guard<std::mutex> sperre(s->m);
                    s->wartend.push_back(std::move(a));
                    wecker = s->wecker;
                    nutzer = s->weckerNutzer;
                }
                if (wecker) wecker(nutzer);
                continue;
            }
            // §5: eine unbekannte Nachricht wird beantwortet, nie mit einem Abbruch quittiert.
            sende(s, std::string("{\"nachricht\":\"antwort\"") + (id != felder.end() ? ",\"id\":" + id->second : "")
                         + ",\"status\":\"unbekannt\",\"text\":"
                         + json::zitiert("unbekannte Nachricht: " + nachricht) + "}");
        }
    }

    void lauf(sts_sitzung* s)
    {
        int zustand = STS_GETRENNT;
        if (verbinde(s))
        {
            zustand = begruesse(s);
            if (zustand == STS_VERBUNDEN)
            {
                s->zustand = STS_VERBUNDEN;
                meldeNach(s);
                lies(s);
                zustand = STS_GETRENNT;
            }
        }
        schliesseKanal(s);
        {
            const std::lock_guard<std::mutex> sperre(s->m);
            s->wartend.clear();
        }
        s->zustand = zustand;
    }
}

extern "C" {

void sts_fassung(uint16_t* s_major, uint16_t* s_minor, uint16_t* p_major_min, uint16_t* p_major_max,
                 const char** produkt)
{
    if (s_major) *s_major = STS_S_MAJOR;
    if (s_minor) *s_minor = STS_S_MINOR;
    if (p_major_min) *p_major_min = STS_P_MAJOR;
    if (p_major_max) *p_major_max = STS_P_MAJOR;
    if (produkt) *produkt = STS_PRODUKT;
}

sts_status sts_oeffne(const sts_konfig* k, sts_sitzung** s)
{
    try
    {
        t_oeffneFehler.clear();
        if (s) *s = nullptr;
        if (!k || !s || k->groesse < sizeof(sts_konfig) || !k->anwendung || !*k->anwendung)
        {
            t_oeffneFehler = "sts_oeffne: Konfiguration fehlt oder nennt keine Anwendung";
            return STS_UNGUELTIG;
        }
        if (k->s_major != STS_S_MAJOR || k->s_minor > STS_S_MINOR)
        {
            t_oeffneFehler = "Schnittstelle S passt nicht: die Anwendung ist mit " + std::to_string(k->s_major) + "."
                           + std::to_string(k->s_minor) + " übersetzt, die DLL bietet " + std::to_string(STS_S_MAJOR)
                           + "." + std::to_string(STS_S_MINOR);
            return STS_UNVERTRAEGLICH;
        }
        auto* neu = new sts_sitzung;
        neu->anwendung = k->anwendung;
        neu->version = k->version ? k->version : "";
        neu->projektDatei = k->projekt_datei ? k->projekt_datei : "";
        *s = neu;
        return STS_OK;
    }
    catch (...)
    {
        return STS_FEHLER;
    }
}

sts_status sts_melde_aktion(sts_sitzung* s, const sts_aktion* a)
{
    try
    {
        if (!s) return STS_UNGUELTIG;
        if (!a || a->groesse < sizeof(sts_aktion) || !a->rueckruf || !nameGueltig(a->name))
        {
            setzeFehler(s, "sts_melde_aktion: Name nur aus [a-z0-9_.], Rückruf und Größe müssen gesetzt sein");
            return STS_UNGUELTIG;
        }
        const bool beides = (a->schalter & STS_FRAGT_NACH) && (a->schalter & STS_WARTET_AUF_MENSCH);
        {
            const std::lock_guard<std::mutex> sperre(s->m);
            bool doppelt = false;
            for (const Aktion& vorhanden : s->aktionen) doppelt = doppelt || vorhanden.name == a->name;
            if (doppelt || beides)
            {
                s->fehler = doppelt ? std::string("Aktion schon angemeldet: ") + a->name
                                    : std::string("Aktion trägt STS_FRAGT_NACH und STS_WARTET_AUF_MENSCH zugleich: ")
                                          + a->name;
                return STS_UNGUELTIG;
            }
            Aktion neu;
            neu.name = a->name;
            neu.beschreibung = a->beschreibung ? a->beschreibung : "";
            neu.parameter = a->parameter ? a->parameter : "";
            neu.schalter = a->schalter;
            neu.rueckruf = a->rueckruf;
            neu.nutzer = a->nutzer;
            s->aktionen.push_back(std::move(neu));
        }
        if (s->zustand == STS_VERBUNDEN) meldeNach(s);
        return STS_OK;
    }
    catch (...)
    {
        return STS_FEHLER;
    }
}

void sts_setze_wecker(sts_sitzung* s, void (*wecker)(void* nutzer), void* nutzer)
{
    if (!s) return;
    const std::lock_guard<std::mutex> sperre(s->m);
    s->wecker = wecker;
    s->weckerNutzer = nutzer;
}

sts_status sts_verbinde(sts_sitzung* s)
{
    try
    {
        if (!s) return STS_UNGUELTIG;
        const int z = s->zustand;
        if (z == STS_VERBINDET || z == STS_VERBUNDEN) return STS_OK;
        if (s->faden.joinable()) s->faden.join();
        s->ende = false;
        s->abbruch = false;
        s->zustand = STS_VERBINDET;
        s->faden = std::thread(lauf, s);
        return STS_OK;
    }
    catch (...)
    {
        s->zustand = STS_GETRENNT;
        return STS_FEHLER;
    }
}

int sts_pumpe(sts_sitzung* s)
{
    if (!s) return 0;
    // Höchstens ein Rückruf zugleich: ein Aufruf aus einem Rückruf heraus führt nichts aus.
    if (s->inRueckruf.exchange(true)) return 0;
    int anzahl = 0;
    try
    {
        for (;;)
        {
            Aufruf auf;
            sts_rueckruf rueckruf = nullptr;
            void* nutzer = nullptr;
            {
                const std::lock_guard<std::mutex> sperre(s->m);
                if (s->wartend.empty()) break;
                auf = std::move(s->wartend.front());
                s->wartend.pop_front();
                for (const Aktion& a : s->aktionen)
                    if (a.name == auf.aktion) { rueckruf = a.rueckruf; nutzer = a.nutzer; }
            }
            sts_antwort antwort;
            sts_status st = STS_UNBEKANNT;
            if (!rueckruf) antwort.text = "diese Aktion kennt die Anwendung nicht: " + auf.aktion;
            else
            {
                try
                {
                    st = rueckruf(nutzer, auf.aktion.c_str(), auf.argumente.c_str(), &antwort);
                }
                catch (...)
                {
                    st = STS_FEHLER;
                    antwort.text = "Ausnahme im Rückruf der Aktion";
                }
            }
            sende(s, "{\"nachricht\":\"antwort\",\"id\":" + auf.id + ",\"status\":\"" + statusWort(st)
                         + "\",\"text\":" + json::zitiert(antwort.text) + "}");
            ++anzahl;
        }
    }
    catch (...)
    {
    }
    s->inRueckruf = false;
    return anzahl;
}

void sts_antwort_text(sts_antwort* antwort, const char* text)
{
    try
    {
        if (antwort) antwort->text = text ? text : "";
    }
    catch (...)
    {
    }
}

sts_status sts_melde(sts_sitzung* s, const char* text)
{
    try
    {
        if (!s) return STS_UNGUELTIG;
        if (s->zustand != STS_VERBUNDEN) return STS_KEIN_TESTER;
        return sende(s, "{\"nachricht\":\"meldung\",\"text\":" + json::zitiert(text ? text : "") + "}") ? STS_OK
                                                                                                         : STS_FEHLER;
    }
    catch (...)
    {
        return STS_FEHLER;
    }
}

int sts_zustand(sts_sitzung* s)
{
    return s ? s->zustand.load() : STS_GETRENNT;
}

const char* sts_letzter_fehler(sts_sitzung* s)
{
    if (!s) return t_oeffneFehler.c_str();
    try
    {
        const std::lock_guard<std::mutex> sperre(s->m);
        s->fehlerAusgabe = s->fehler;
    }
    catch (...)
    {
    }
    return s->fehlerAusgabe.c_str();
}

void sts_schliesse(sts_sitzung* s)
{
    if (!s) return;
    if (s->zustand == STS_VERBUNDEN) sende(s, "{\"nachricht\":\"tschuess\"}");
    s->ende = true;
    if (s->faden.joinable()) s->faden.join();
    delete s;
}

} // extern "C"
