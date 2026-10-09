// sichttest_steuerung.hpp — kleiner Kopf für C++ über der C-Schnittstelle S.
//
// Lädt die DLL zur Laufzeit (Konzept §8): mit vollem Pfad aus dem Ordner der
// Exe, nie über den Suchpfad, und nur dort, wo die Anwendung es verlangt.
// Die Anwendung linkt nicht gegen die DLL; dieser Kopf wird in der Anwendung
// übersetzt und braucht nur C++17.
//
//     auto sitzung = sichttest::Sitzung::lade("MeineAnwendung", "1.0.0", &grund);
//     if (sitzung) {
//         sitzung->aktion("tu_etwas", "Tut etwas", "wert=<zahl>",
//             [](const char* argumente_json) { return sichttest::ok(); });
//         sitzung->setzeWecker([] { /* Anstoß in den GUI-Thread stellen */ });
//         sitzung->verbinde();
//     }
//     …im GUI-Thread, nach dem Anstoß:  sitzung->pumpe();
//
// Für Qt gibt es sichttest_steuerung_qt.hpp; der stellt die Rückrufe selbst zu.

#ifndef SICHTTEST_STEUERUNG_HPP
#define SICHTTEST_STEUERUNG_HPP

#include "sichttest_steuerung.h"

#include <exception>
#include <functional>
#include <list>
#include <memory>
#include <string>
#include <utility>

#ifdef _WIN32
#  ifndef WIN32_LEAN_AND_MEAN
#    define WIN32_LEAN_AND_MEAN
#  endif
#  ifndef NOMINMAX
#    define NOMINMAX
#  endif
#  include <windows.h>
#endif

namespace sichttest
{
    // Was eine Aktion zurückgibt: Status und ein Text für den Tester.
    struct Ergebnis
    {
        sts_status status = STS_OK;
        std::string text;
    };

    inline Ergebnis ok() { return {}; }
    inline Ergebnis ok(const char* text) { return { STS_OK, text ? text : "" }; }
    inline Ergebnis ok(const std::string& text) { return { STS_OK, text }; }
    inline Ergebnis fehler(const char* text) { return { STS_FEHLER, text ? text : "" }; }
    inline Ergebnis fehler(const std::string& text) { return { STS_FEHLER, text }; }
    inline Ergebnis ungueltig(const char* text) { return { STS_UNGUELTIG, text ? text : "" }; }
    inline Ergebnis ungueltig(const std::string& text) { return { STS_UNGUELTIG, text }; }

    // Was die geladene DLL über sich sagt (sts_fassung): S, die Spanne von P, das Produkt.
    struct Fassung
    {
        uint16_t sMajor = 0;
        uint16_t sMinor = 0;
        uint16_t pMin = 0;
        uint16_t pMax = 0;
        std::string produkt;   // Tag des Repos ohne "v", etwa "0.3.1"
    };

    class Sitzung
    {
    public:
        // Bekommt die Argumente als JSON-Objekt in UTF-8. Läuft im Thread, der pumpe() ruft.
        using Aktion = std::function<Ergebnis(const char* argumente_json)>;

        // Lädt die DLL und legt die Sitzung an. Liefert leer, wenn die DLL fehlt
        // oder die Fassung von S nicht passt; grund nennt dann einen Satz dazu.
        // dllPfad: voller Pfad; ohne ihn SichttestSteuerung<S>.dll im Ordner der Exe.
        static std::unique_ptr<Sitzung> lade(const char* anwendung, const char* version,
                                             std::string* grund = nullptr, const wchar_t* dllPfad = nullptr,
                                             const char* projektDatei = nullptr)
        {
            auto sage = [grund](std::string text) { if (grund) *grund = std::move(text); };
#ifdef _WIN32
            const std::wstring pfad = dllPfad ? std::wstring(dllPfad) : standardPfad();
            const HMODULE dll = LoadLibraryExW(pfad.c_str(), nullptr, LOAD_WITH_ALTERED_SEARCH_PATH);
            if (!dll)
            {
                sage("Die DLL der Sichttest-Steuerung ließ sich nicht laden: " + utf8(pfad) + " (Windows-Fehler "
                     + std::to_string(GetLastError()) + ")");
                return nullptr;
            }
            std::unique_ptr<Sitzung> s(new Sitzung);
            s->m_dll = dll;
            const bool alle = s->hole("sts_oeffne", s->m_f.oeffne) && s->hole("sts_melde_aktion", s->m_f.melde_aktion)
                           && s->hole("sts_setze_wecker", s->m_f.setze_wecker) && s->hole("sts_verbinde", s->m_f.verbinde)
                           && s->hole("sts_pumpe", s->m_f.pumpe) && s->hole("sts_antwort_text", s->m_f.antwort_text)
                           && s->hole("sts_melde", s->m_f.melde) && s->hole("sts_zustand", s->m_f.zustand)
                           && s->hole("sts_letzter_fehler", s->m_f.letzter_fehler)
                           && s->hole("sts_fassung", s->m_f.fassung)
                           && s->hole("sts_schliesse", s->m_f.schliesse);
            if (!alle)
            {
                sage("Der DLL der Sichttest-Steuerung fehlt ein Einstieg: " + utf8(pfad));
                return nullptr;
            }
            sts_konfig k{};
            k.groesse = sizeof k;
            k.s_major = STS_S_MAJOR;
            k.s_minor = STS_S_MINOR;
            k.anwendung = anwendung;
            k.version = version;
            k.projekt_datei = projektDatei;
            if (s->m_f.oeffne(&k, &s->m_s) != STS_OK)
            {
                sage(s->m_f.letzter_fehler(nullptr));
                return nullptr;
            }
            return s;
#else
            (void)anwendung; (void)version; (void)dllPfad; (void)projektDatei;
            sage("Die Sichttest-Steuerung gibt es nur unter Windows.");
            return nullptr;
#endif
        }

        ~Sitzung()
        {
            if (m_s) m_f.schliesse(m_s);
#ifdef _WIN32
            if (m_dll) FreeLibrary(static_cast<HMODULE>(m_dll));
#endif
        }
        Sitzung(const Sitzung&) = delete;
        Sitzung& operator=(const Sitzung&) = delete;

        // Vor oder nach verbinde(). false: Name ungültig oder schon vergeben (letzterFehler()).
        bool aktion(const char* name, const char* beschreibung, const char* parameter, Aktion a,
                    uint32_t schalter = 0)
        {
            m_aktionen.push_back({ this, std::move(a) });
            sts_aktion eintrag{};
            eintrag.groesse = sizeof eintrag;
            eintrag.name = name;
            eintrag.beschreibung = beschreibung;
            eintrag.parameter = parameter;
            eintrag.schalter = schalter;
            eintrag.rueckruf = &Sitzung::rufe;
            eintrag.nutzer = &m_aktionen.back();
            if (m_f.melde_aktion(m_s, &eintrag) == STS_OK) return true;
            m_aktionen.pop_back();
            return false;
        }

        // Vor verbinde() setzen. Der Wecker läuft im Lesethread der DLL und darf
        // nur einen Anstoß in den GUI-Thread stellen, der dort pumpe() ruft.
        void setzeWecker(std::function<void()> wecker)
        {
            m_wecker = std::move(wecker);
            m_f.setze_wecker(m_s, m_wecker ? &Sitzung::wecke : nullptr, this);
        }

        // Kehrt sofort zurück; das Ergebnis zeigt zustand().
        bool verbinde() { return m_f.verbinde(m_s) == STS_OK; }
        // Im GUI-Thread: wartende Aufrufe ausführen. Liefert ihre Zahl.
        int pumpe() { return m_f.pumpe(m_s); }
        bool melde(const char* text) { return m_f.melde(m_s, text) == STS_OK; }
        // STS_GETRENNT, STS_VERBINDET, STS_VERBUNDEN oder STS_ABGELEHNT
        int zustand() const { return m_f.zustand(m_s); }
        // Grund des letzten Fehlschlags; bleibt stehen, bis ein neuer ihn ersetzt (ein Erfolg leert nicht).
        std::string letzterFehler() const { return m_f.letzter_fehler(m_s); }
        // Fassung der geladenen DLL, etwa für das Log der Anwendung.
        Fassung fassung() const
        {
            Fassung f;
            const char* produkt = nullptr;
            m_f.fassung(&f.sMajor, &f.sMinor, &f.pMin, &f.pMax, &produkt);
            if (produkt) f.produkt = produkt;
            return f;
        }

    private:
        Sitzung() = default;

        struct Eintrag
        {
            Sitzung* sitzung;
            Aktion aktion;
        };

        // Die Einstiege der DLL, einzeln geholt.
        struct Einstiege
        {
            decltype(&sts_oeffne) oeffne = nullptr;
            decltype(&sts_melde_aktion) melde_aktion = nullptr;
            decltype(&sts_setze_wecker) setze_wecker = nullptr;
            decltype(&sts_verbinde) verbinde = nullptr;
            decltype(&sts_pumpe) pumpe = nullptr;
            decltype(&sts_antwort_text) antwort_text = nullptr;
            decltype(&sts_melde) melde = nullptr;
            decltype(&sts_zustand) zustand = nullptr;
            decltype(&sts_letzter_fehler) letzter_fehler = nullptr;
            decltype(&sts_fassung) fassung = nullptr;
            decltype(&sts_schliesse) schliesse = nullptr;
        };

        static sts_status rufe(void* nutzer, const char*, const char* argumente_json, sts_antwort* antwort)
        {
            auto* e = static_cast<Eintrag*>(nutzer);
            Ergebnis r;
            // Über die Grenze der DLL darf nichts geworfen werden.
            try
            {
                r = e->aktion(argumente_json);
            }
            catch (const std::exception& x)
            {
                r = fehler(std::string("Ausnahme in der Aktion: ") + x.what());
            }
            catch (...)
            {
                r = fehler("Ausnahme in der Aktion");
            }
            e->sitzung->m_f.antwort_text(antwort, r.text.c_str());
            return r.status;
        }

        static void wecke(void* nutzer)
        {
            auto* s = static_cast<Sitzung*>(nutzer);
            if (s->m_wecker) s->m_wecker();
        }

#ifdef _WIN32
        template <typename F>
        bool hole(const char* name, F& ziel)
        {
            ziel = reinterpret_cast<F>(reinterpret_cast<void*>(GetProcAddress(static_cast<HMODULE>(m_dll), name)));
            return ziel != nullptr;
        }

        // Die große Fassung von S steht im Dateinamen: eine unverträgliche DLL wird gar nicht geladen (§5).
        static std::wstring standardPfad()
        {
            wchar_t puffer[32768];
            const DWORD n = GetModuleFileNameW(nullptr, puffer, 32768);
            std::wstring pfad(puffer, n);
            const size_t schnitt = pfad.find_last_of(L"\\/");
            pfad = schnitt == std::wstring::npos ? std::wstring() : pfad.substr(0, schnitt + 1);
            return pfad + L"SichttestSteuerung" + std::to_wstring(STS_S_MAJOR) + L".dll";
        }

        static std::string utf8(const std::wstring& w)
        {
            if (w.empty()) return {};
            const int n = WideCharToMultiByte(CP_UTF8, 0, w.data(), static_cast<int>(w.size()), nullptr, 0, nullptr,
                                              nullptr);
            std::string s(static_cast<size_t>(n), '\0');
            WideCharToMultiByte(CP_UTF8, 0, w.data(), static_cast<int>(w.size()), &s[0], n, nullptr, nullptr);
            return s;
        }
#endif

        void* m_dll = nullptr;          // HMODULE
        Einstiege m_f;
        sts_sitzung* m_s = nullptr;
        std::list<Eintrag> m_aktionen;  // Liste: die Adressen der Einträge bleiben gültig
        std::function<void()> m_wecker;
    };
}

#endif // SICHTTEST_STEUERUNG_HPP
