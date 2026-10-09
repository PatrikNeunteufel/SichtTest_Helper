// Der kleine JSON-Leser und -Schreiber der DLL (Konzept §4, „Rahmen").
//
// Gelesen wird nur die oberste Ebene einer Nachricht: Schlüssel → Rohtext des
// Werts. Was darunter liegt (die Argumente einer Aktion), reicht die DLL als
// Text weiter, ohne es zu deuten.

#pragma once

#include <map>
#include <string>
#include <string_view>

namespace sts::json
{
    // "…" mit Anführungszeichen, für JSON maskiert. Eingabe ist UTF-8.
    std::string zitiert(std::string_view text);

    // Zerlegt ein Objekt. false, wenn der Text kein wohlgeformtes Objekt ist.
    bool zerlege(std::string_view text, std::map<std::string, std::string>& felder);

    // Rohtext eines Werts als Zeichenkette lesen; false, wenn es keine ist.
    bool alsText(std::string_view roh, std::string& aus);
    // Rohtext als ganze Zahl; false, wenn es keine ist.
    bool alsZahl(std::string_view roh, long long& aus);
}
