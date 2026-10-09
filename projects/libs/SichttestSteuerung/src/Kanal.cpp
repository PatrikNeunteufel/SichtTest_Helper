#include "Kanal.hpp"

#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>

#include <aclapi.h>
#include <sddl.h>

#include <algorithm>
#include <cstdint>

#pragma comment(lib, "advapi32")

namespace sts
{
    namespace
    {
        constexpr uint32_t kMaxRahmen = 16u * 1024u * 1024u;
        constexpr DWORD kSchreibFristMs = 2000;

        KanalStatus ausCode(DWORD code, std::string& text)
        {
            if (code == ERROR_BROKEN_PIPE || code == ERROR_PIPE_NOT_CONNECTED || code == ERROR_NO_DATA)
            {
                text = "Verbindung beendet";
                return KanalStatus::Getrennt;
            }
            text = "Windows-Fehler " + std::to_string(code);
            return KanalStatus::Fehler;
        }
    }

    Kanal::Kanal()
        : m_leseEreignis(CreateEventW(nullptr, TRUE, FALSE, nullptr))
        , m_schreibEreignis(CreateEventW(nullptr, TRUE, FALSE, nullptr))
    {
    }

    Kanal::~Kanal()
    {
        schliesse();
        if (m_leseEreignis) CloseHandle(m_leseEreignis);
        if (m_schreibEreignis) CloseHandle(m_schreibEreignis);
    }

    KanalStatus Kanal::verbinde(const std::wstring& name, unsigned fristMs)
    {
        schliesse();
        const std::wstring pfad = L"\\\\.\\pipe\\" + name;
        const ULONGLONG start = GetTickCount64();
        const ULONGLONG ende = start + fristMs;
        // Lauscht ein Tester, ist aber gerade keine Instanz frei, lohnt kurzes Warten immer.
        const ULONGLONG endeBelegt = start + std::max(fristMs, 2000u);
        for (;;)
        {
            // SECURITY_IDENTIFICATION: der Server darf nicht in unserem Namen handeln.
            const HANDLE h = CreateFileW(pfad.c_str(), GENERIC_READ | GENERIC_WRITE, 0, nullptr, OPEN_EXISTING,
                                         FILE_FLAG_OVERLAPPED | SECURITY_SQOS_PRESENT | SECURITY_IDENTIFICATION,
                                         nullptr);
            if (h != INVALID_HANDLE_VALUE)
            {
                m_pipe = h;
                return KanalStatus::Ok;
            }
            const DWORD code = GetLastError();
            if (code != ERROR_FILE_NOT_FOUND && code != ERROR_PIPE_BUSY)
                return ausCode(code, m_schreibFehler);
            if (code == ERROR_PIPE_BUSY)
            {
                if (GetTickCount64() >= endeBelegt)
                {
                    m_schreibFehler = "der Tester nimmt die Verbindung nicht an";
                    return KanalStatus::Fehler;
                }
                WaitNamedPipeW(pfad.c_str(), 100);
                continue;
            }
            if (GetTickCount64() >= ende)
            {
                m_schreibFehler = "kein Tester lauscht";
                return KanalStatus::Frist;
            }
            Sleep(50);
        }
    }

    KanalStatus Kanal::sende(const std::string& json)
    {
        if (json.size() > kMaxRahmen)
        {
            m_schreibFehler = "Rahmen zu groß";
            return KanalStatus::Fehler;
        }
        const auto n = static_cast<uint32_t>(json.size());
        std::string rahmen;
        rahmen.reserve(4 + json.size());
        for (int i = 0; i < 4; ++i) rahmen.push_back(static_cast<char>((n >> (8 * i)) & 0xffu));
        rahmen += json;
        return sendeRoh(rahmen.data(), rahmen.size());
    }

    KanalStatus Kanal::sendeRoh(const char* daten, size_t anzahl)
    {
        if (!m_pipe)
        {
            m_schreibFehler = "nicht verbunden";
            return KanalStatus::Getrennt;
        }
        while (anzahl > 0)
        {
            OVERLAPPED ov{};
            ov.hEvent = m_schreibEreignis;
            const auto stueck = static_cast<DWORD>(std::min<size_t>(anzahl, 1u << 20));
            if (!WriteFile(m_pipe, daten, stueck, nullptr, &ov))
            {
                if (GetLastError() != ERROR_IO_PENDING) return ausCode(GetLastError(), m_schreibFehler);
                // Nimmt der Tester nichts ab, darf die Anwendung nicht hängen bleiben.
                if (WaitForSingleObject(m_schreibEreignis, kSchreibFristMs) != WAIT_OBJECT_0)
                    CancelIoEx(m_pipe, &ov);
            }
            DWORD geschrieben = 0;
            if (!GetOverlappedResult(m_pipe, &ov, &geschrieben, TRUE))
            {
                const DWORD code = GetLastError();
                if (code == ERROR_OPERATION_ABORTED)
                {
                    m_schreibFehler = "der Tester nimmt nichts ab";
                    return KanalStatus::Frist;
                }
                return ausCode(code, m_schreibFehler);
            }
            daten += geschrieben;
            anzahl -= geschrieben;
        }
        return KanalStatus::Ok;
    }

    KanalStatus Kanal::lies(std::string& json, unsigned fristMs)
    {
        const ULONGLONG ende = GetTickCount64() + fristMs;
        for (;;)
        {
            if (m_eingang.size() >= 4)
            {
                const auto* b = reinterpret_cast<const unsigned char*>(m_eingang.data());
                const uint32_t n = static_cast<uint32_t>(b[0]) | (static_cast<uint32_t>(b[1]) << 8)
                                 | (static_cast<uint32_t>(b[2]) << 16) | (static_cast<uint32_t>(b[3]) << 24);
                if (n > kMaxRahmen)
                {
                    m_leseFehler = "Rahmen zu groß";
                    return KanalStatus::Fehler;
                }
                if (m_eingang.size() >= 4u + n)
                {
                    json.assign(m_eingang, 4, n);
                    m_eingang.erase(0, 4u + n);
                    return KanalStatus::Ok;
                }
            }
            if (!m_pipe)
            {
                m_leseFehler = "nicht verbunden";
                return KanalStatus::Getrennt;
            }

            char puffer[65536];
            OVERLAPPED ov{};
            ov.hEvent = m_leseEreignis;
            if (!ReadFile(m_pipe, puffer, sizeof puffer, nullptr, &ov))
            {
                const DWORD code = GetLastError();
                if (code != ERROR_IO_PENDING) return ausCode(code, m_leseFehler);
                const ULONGLONG jetzt = GetTickCount64();
                const DWORD warte = jetzt >= ende ? 0 : static_cast<DWORD>(ende - jetzt);
                if (WaitForSingleObject(m_leseEreignis, warte) != WAIT_OBJECT_0) CancelIoEx(m_pipe, &ov);
            }
            // Auch nach dem Abbrechen abholen: kam zugleich noch etwas an, zählt es.
            DWORD gelesen = 0;
            if (!GetOverlappedResult(m_pipe, &ov, &gelesen, TRUE))
            {
                const DWORD code = GetLastError();
                if (code == ERROR_OPERATION_ABORTED)
                {
                    m_leseFehler = "nichts in der Frist";
                    return KanalStatus::Frist;
                }
                return ausCode(code, m_leseFehler);
            }
            m_eingang.append(puffer, gelesen);
        }
    }

    void Kanal::schliesse()
    {
        if (m_pipe) CloseHandle(m_pipe);
        m_pipe = nullptr;
        m_eingang.clear();
    }

    std::string Kanal::sicherheit() const
    {
        if (!m_pipe) return {};
        PSECURITY_DESCRIPTOR sd = nullptr;
        const SECURITY_INFORMATION was = OWNER_SECURITY_INFORMATION | DACL_SECURITY_INFORMATION;
        if (GetSecurityInfo(m_pipe, SE_KERNEL_OBJECT, was, nullptr, nullptr, nullptr, nullptr, &sd) != ERROR_SUCCESS)
            return {};
        std::string ergebnis;
        LPSTR text = nullptr;
        if (ConvertSecurityDescriptorToStringSecurityDescriptorA(sd, SDDL_REVISION_1, was, &text, nullptr))
        {
            ergebnis = text;
            LocalFree(text);
        }
        LocalFree(sd);
        return ergebnis;
    }
}
