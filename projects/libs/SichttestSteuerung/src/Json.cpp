#include "Json.hpp"

#include <cstdio>

namespace sts::json
{
    namespace
    {
        constexpr int kMaxTiefe = 64;

        void leer(std::string_view t, size_t& i)
        {
            while (i < t.size() && (t[i] == ' ' || t[i] == '\t' || t[i] == '\n' || t[i] == '\r')) ++i;
        }

        void haengeUtf8An(std::string& aus, unsigned cp)
        {
            if (cp < 0x80) aus.push_back(static_cast<char>(cp));
            else if (cp < 0x800)
            {
                aus.push_back(static_cast<char>(0xC0 | (cp >> 6)));
                aus.push_back(static_cast<char>(0x80 | (cp & 0x3F)));
            }
            else if (cp < 0x10000)
            {
                aus.push_back(static_cast<char>(0xE0 | (cp >> 12)));
                aus.push_back(static_cast<char>(0x80 | ((cp >> 6) & 0x3F)));
                aus.push_back(static_cast<char>(0x80 | (cp & 0x3F)));
            }
            else
            {
                aus.push_back(static_cast<char>(0xF0 | (cp >> 18)));
                aus.push_back(static_cast<char>(0x80 | ((cp >> 12) & 0x3F)));
                aus.push_back(static_cast<char>(0x80 | ((cp >> 6) & 0x3F)));
                aus.push_back(static_cast<char>(0x80 | (cp & 0x3F)));
            }
        }

        bool hex4(std::string_view t, size_t i, unsigned& wert)
        {
            if (i + 4 > t.size()) return false;
            wert = 0;
            for (size_t k = 0; k < 4; ++k)
            {
                const char c = t[i + k];
                unsigned z;
                if (c >= '0' && c <= '9') z = static_cast<unsigned>(c - '0');
                else if (c >= 'a' && c <= 'f') z = static_cast<unsigned>(c - 'a') + 10;
                else if (c >= 'A' && c <= 'F') z = static_cast<unsigned>(c - 'A') + 10;
                else return false;
                wert = wert * 16 + z;
            }
            return true;
        }

        // Liest eine Zeichenkette ab dem öffnenden Anführungszeichen; aus darf nullptr sein.
        bool zeichenkette(std::string_view t, size_t& i, std::string* aus)
        {
            if (i >= t.size() || t[i] != '"') return false;
            ++i;
            while (i < t.size())
            {
                const char c = t[i++];
                if (c == '"') return true;
                if (static_cast<unsigned char>(c) < 0x20) return false;
                if (c != '\\')
                {
                    if (aus) aus->push_back(c);
                    continue;
                }
                if (i >= t.size()) return false;
                const char e = t[i++];
                char ersatz = 0;
                switch (e)
                {
                case '"': ersatz = '"'; break;
                case '\\': ersatz = '\\'; break;
                case '/': ersatz = '/'; break;
                case 'b': ersatz = '\b'; break;
                case 'f': ersatz = '\f'; break;
                case 'n': ersatz = '\n'; break;
                case 'r': ersatz = '\r'; break;
                case 't': ersatz = '\t'; break;
                case 'u':
                {
                    unsigned cp = 0;
                    if (!hex4(t, i, cp)) return false;
                    i += 4;
                    if (cp >= 0xD800 && cp < 0xDC00)   // Ersatzpaar
                    {
                        unsigned tief = 0;
                        if (i + 6 > t.size() || t[i] != '\\' || t[i + 1] != 'u' || !hex4(t, i + 2, tief)
                            || tief < 0xDC00 || tief > 0xDFFF)
                            return false;
                        i += 6;
                        cp = 0x10000 + ((cp - 0xD800) << 10) + (tief - 0xDC00);
                    }
                    if (aus) haengeUtf8An(*aus, cp);
                    continue;
                }
                default: return false;
                }
                if (aus) aus->push_back(ersatz);
            }
            return false;
        }

        bool wert(std::string_view t, size_t& i, int tiefe);

        bool behaelter(std::string_view t, size_t& i, int tiefe, char zu, bool mitSchluessel)
        {
            ++i;
            leer(t, i);
            if (i < t.size() && t[i] == zu) { ++i; return true; }
            for (;;)
            {
                leer(t, i);
                if (mitSchluessel)
                {
                    if (!zeichenkette(t, i, nullptr)) return false;
                    leer(t, i);
                    if (i >= t.size() || t[i] != ':') return false;
                    ++i;
                }
                if (!wert(t, i, tiefe + 1)) return false;
                leer(t, i);
                if (i >= t.size()) return false;
                if (t[i] == zu) { ++i; return true; }
                if (t[i] != ',') return false;
                ++i;
            }
        }

        // Überspringt einen Wert beliebiger Art.
        bool wert(std::string_view t, size_t& i, int tiefe)
        {
            if (tiefe > kMaxTiefe) return false;
            leer(t, i);
            if (i >= t.size()) return false;
            const char c = t[i];
            if (c == '"') return zeichenkette(t, i, nullptr);
            if (c == '{') return behaelter(t, i, tiefe, '}', true);
            if (c == '[') return behaelter(t, i, tiefe, ']', false);
            for (const std::string_view wort : { std::string_view("true"), std::string_view("false"),
                                                 std::string_view("null") })
                if (t.substr(i, wort.size()) == wort) { i += wort.size(); return true; }
            const size_t anfang = i;
            while (i < t.size() && ((t[i] >= '0' && t[i] <= '9') || t[i] == '-' || t[i] == '+' || t[i] == '.'
                                    || t[i] == 'e' || t[i] == 'E'))
                ++i;
            return i > anfang;
        }
    }

    std::string zitiert(std::string_view text)
    {
        std::string aus;
        aus.reserve(text.size() + 2);
        aus.push_back('"');
        for (const char c : text)
        {
            switch (c)
            {
            case '"': aus += "\\\""; break;
            case '\\': aus += "\\\\"; break;
            case '\n': aus += "\\n"; break;
            case '\r': aus += "\\r"; break;
            case '\t': aus += "\\t"; break;
            default:
                if (static_cast<unsigned char>(c) < 0x20)
                {
                    char puffer[8];
                    std::snprintf(puffer, sizeof puffer, "\\u%04x", static_cast<unsigned>(c));
                    aus += puffer;
                }
                else aus.push_back(c);
            }
        }
        aus.push_back('"');
        return aus;
    }

    bool zerlege(std::string_view t, std::map<std::string, std::string>& felder)
    {
        felder.clear();
        size_t i = 0;
        leer(t, i);
        if (i >= t.size() || t[i] != '{') return false;
        ++i;
        leer(t, i);
        if (i < t.size() && t[i] == '}') { ++i; leer(t, i); return i == t.size(); }
        for (;;)
        {
            leer(t, i);
            std::string schluessel;
            if (!zeichenkette(t, i, &schluessel)) return false;
            leer(t, i);
            if (i >= t.size() || t[i] != ':') return false;
            ++i;
            leer(t, i);
            const size_t anfang = i;
            if (!wert(t, i, 1)) return false;
            felder[schluessel] = std::string(t.substr(anfang, i - anfang));
            leer(t, i);
            if (i >= t.size()) return false;
            if (t[i] == '}') { ++i; break; }
            if (t[i] != ',') return false;
            ++i;
        }
        leer(t, i);
        return i == t.size();
    }

    bool alsText(std::string_view roh, std::string& aus)
    {
        aus.clear();
        size_t i = 0;
        return zeichenkette(roh, i, &aus) && i == roh.size();
    }

    bool alsZahl(std::string_view roh, long long& aus)
    {
        if (roh.empty() || roh.size() > 18) return false;
        size_t i = 0;
        const bool minus = roh[0] == '-';
        if (minus) ++i;
        if (i >= roh.size()) return false;
        long long n = 0;
        for (; i < roh.size(); ++i)
        {
            if (roh[i] < '0' || roh[i] > '9') return false;
            n = n * 10 + (roh[i] - '0');
        }
        aus = minus ? -n : n;
        return true;
    }
}
