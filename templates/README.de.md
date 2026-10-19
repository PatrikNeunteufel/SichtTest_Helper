[English](README.md) · **Deutsch**

# Vorlagen

Dateien zum Kopieren in das Repo einer Anwendung, die Sichttest steuern soll. Die Schritte
beschreibt [docs/Integration_Guide.de.md](../docs/Integration_Guide.de.md); jeder Schlüssel und
jede Regel steht in [docs/Specification.de.md](../docs/Specification.de.md). Schlüssel, die in den
JSON-Dateien mit `_` beginnen, sind Kommentare und werden vom Werkzeug überlesen — löschen oder
stehen lassen.

| Datei | Kopieren nach | Zweck |
|---|---|---|
| [.sichttest/sichttest.projekt.json](.sichttest/sichttest.projekt.json) | `<repo>/.sichttest/sichttest.projekt.json` | was das Werkzeug über das Projekt wissen muss: Name, Start, Listen, Ablage, Nachtest-Indikator, Abbildung von Feldern auf Aktionen |
| [sichttest.pin](sichttest.pin) | `<repo>/sichttest.pin` | welche Paketversion der Build holt, mit Prüfsumme |
| [integration/Solution.external.json](integration/Solution.external.json) | in `Solution.json` einfügen | Bezug des Pakets mit CMakeCraft |
| [integration/sichttest.cmake](integration/sichttest.cmake) | `<repo>/cmake/sichttest.cmake` | Bezug ohne CMakeCraft (braucht eine Kopie von `CMakeCraftPackage.cmake`) |
| [integration/SichttestIntegration.hpp](integration/SichttestIntegration.hpp), [.cpp](integration/SichttestIntegration.cpp) | eigene Quellen | DLL bei `--testing` laden, Aktionen anmelden, einen Zustand melden (Qt) |
| [Sichttest_List.md](Sichttest_List.md), [Sichttest_List.de.md](Sichttest_List.de.md) | ein Listenordner | eine Markdown-Liste mit Vorbereitung und Aktionen |
| [List.testprotokoll.json](List.testprotokoll.json) | ein Listenordner | eine JSON-Liste mit Vorbereitung, Nachbereitung, Aktionen und Areas |
| [test-gewichtung.json](test-gewichtung.json) | wohin `gewichtung.hand` zeigt | Hand-Datei des Nachtest-Indikators |

Die Kommentare in den Vorlagen sind englisch. Die Namen der Aktionen darin (`open_file`,
`show_tab`, `testdb_setup`, …) sind Beispiele: sie gehören der Anwendung, die sie anmeldet. Ein
vollständiges Beispielprojekt liegt in [examples/project/](../examples/project/).
