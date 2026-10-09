// Kanal der DLL zum Tester (Konzept §4): eine benannte Pipe, direkt über die
// Windows-API — ohne Qt. Der Tester lauscht, die DLL verbindet sich.
//
// Rahmen: 4 Byte Länge (little endian) + ein JSON-Objekt in UTF-8.
//
// Die Pipe ist überlappend geöffnet, damit ein Thread lesen kann, während ein
// anderer schreibt: ein Thread darf lies() rufen, ein zweiter sende().

#pragma once

#include <string>

namespace sts
{
    enum class KanalStatus
    {
        Ok,
        Frist,      // nichts in der Frist (verbinde: kein Tester lauscht)
        Getrennt,   // die Gegenseite hat die Verbindung beendet
        Fehler      // alles andere, Text in leseFehler()/schreibFehler()
    };

    class Kanal
    {
    public:
        Kanal();
        ~Kanal();
        Kanal(const Kanal&) = delete;
        Kanal& operator=(const Kanal&) = delete;

        // name ohne Vorspann: "sichttest-<Benutzer>" → \\.\pipe\sichttest-<Benutzer>
        // Frist heißt hier: es lauscht kein Tester (fristMs 0 = ein Versuch).
        KanalStatus verbinde(const std::wstring& name, unsigned fristMs);
        KanalStatus sende(const std::string& json);
        // Bytes, wie sie sind. Nimmt der Tester 2 s lang nichts ab: Frist — der
        // Rahmen ist dann angerissen, die Verbindung muss beendet werden.
        KanalStatus sendeRoh(const char* daten, size_t anzahl);
        bool offen() const { return m_pipe != nullptr; }
        // Ein ganzer Rahmen. Läuft die Frist ab, bleibt schon Gelesenes erhalten.
        KanalStatus lies(std::string& json, unsigned fristMs);
        void schliesse();

        // Besitzer und Zugriffsliste der Pipe als SDDL; leer, wenn nicht lesbar.
        std::string sicherheit() const;

        const std::string& leseFehler() const { return m_leseFehler; }
        const std::string& schreibFehler() const { return m_schreibFehler; }

    private:
        void* m_pipe = nullptr;             // HANDLE
        void* m_leseEreignis = nullptr;     // HANDLE
        void* m_schreibEreignis = nullptr;  // HANDLE
        std::string m_eingang;
        std::string m_leseFehler;
        std::string m_schreibFehler;
    };
}
