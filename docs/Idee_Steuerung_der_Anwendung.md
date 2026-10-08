# Idee: Der Sichttest steuert die geprüfte Anwendung

> **Stand:** 2026-10-08 · **Status:** Idee; die Richtung hat Patrik gesetzt
> (Abschnitt „Richtung"), Einzelheiten offen, nichts gebaut ·
> **Abgelegt von:** der LumiViz-Sitzung S103 auf Wunsch von Patrik ·
> **Abstimmung:** Sync `…\Visuals_Project\cmake\sync_sichttest`

## Zielbild (Patrik, 2026-10-08)

- Das Werkzeug wird **hier**, in `SichtTest_Helper`, entwickelt und gebaut.
- Im jeweiligen Projekt (zuerst LumiViz) startet man es **mit einem Argument
  der Anwendung**, wie heute beim Comm Studio.
- Aus dem Tester heraus lassen sich Handgriffe in der Anwendung auslösen
  (Tab oder Panel wechseln, eine Vorlage laden, einen Zustand herstellen).
- Dasselbe soll auch für das Comm Studio gelten.
- Offen ist, wie das Werkzeug ins Projekt kommt: als Kopie oder automatisch.

## Ausgangslage (gelesen am 2026-10-08)

| Wo | Was heute gilt | Beleg |
|---|---|---|
| Comm Studio | Der Tester ist ein Fenster **in** der Anwendung. Start mit `comm_studio.exe --testing`, Wiederaufnahme mit `--resume-log=<pfad>`. | `UART/gui/Comm_Studio/MainWindow.cpp:480`, `Main.cpp:7645` |
| Comm Studio | Die Steuerung sind Funktionen, die das Hauptfenster dem Tester übergibt: Tab nach Titel aktivieren, alle Tabs schließen, eine Datei öffnen. Ein Schritt nennt sein Ziel im Feld `"tab"`, das Protokoll seine Vorbereitung im Block `"setup"`. | `TestProtokollWindow.hpp:75–90` |
| SichtTest_Helper | Eigenes Programm, kennt keine Anwendung, startet nur deren Exe. Kein Git, keine Steuerung. | `README.md`, Abschnitt „Was der UART-Tester mehr kann" |
| LumiViz | Eine zweite, fast gleiche Kopie unter `projects/exec/Sichttest`. Abweichung: zwei Zeilen (Name der Organisation, ein Kommentar). | Vergleich der vier Quelldateien |
| LumiViz | Seit S101 lauscht die App auf einem lokalen Kanal (`QLocalServer`); er trägt heute nur einen Dateipfad. | `include/core/EinzelInstanz.hpp` |
| CMakeCraft | Kann Externals aus Git holen (so kommen Qt-ADS, ImGui, GLFW). Ob das für ein Repo ohne Server und für eine Bibliothek mit Qt-Fenster reicht, ist **nicht geprüft**. | `cmake/externals/core/Fetch.cmake` |

## Zwei Wege

### Weg 1 — Bibliothek, der Tester läuft in der Anwendung (wie Comm Studio)

`SichtTest_Helper` liefert eine Bibliothek (Listen lesen, Log schreiben, das
Fenster). Die Anwendung bindet sie ein, öffnet das Fenster bei `--testing`
und meldet ihre Aktionen an (Name → Funktion). Das eigenständige
`Sichttest.exe` bleibt als dünne Hülle für Programme ohne Einbindung.

- Dafür: genau das Verhalten des Comm Studio; die Steuerung ist ein
  Funktionsaufruf; kein Protokoll zwischen zwei Prozessen.
- Dagegen: stürzt die Anwendung ab oder hängt sie, ist der Tester mit weg
  (das Comm Studio brauchte dafür Neustart mit Wiederaufnahme). Der Tester
  teilt sich den GUI-Thread mit der Anwendung — in LumiViz zählt das, weil
  dort Ruckeln beurteilt wird. Die Bibliothek muss mit Qt und Compiler jeder
  Anwendung bauen.

### Weg 2 — eigenes Programm, gesteuert wird über einen lokalen Kanal

`Sichttest.exe` bleibt ein eigener Prozess. `SichtTest_Helper` liefert dazu
eine kleine **Gegenstelle** (Kanal und Aktionsliste), die die Anwendung
einbindet. `Anwendung.exe --testing` startet den Tester daneben und öffnet
den Kanal; der Tester schickt Aktionen, die Anwendung meldet Erfolg zurück.

- Dafür: der Tester überlebt Absturz und Neustart der Anwendung; er belastet
  ihren GUI-Thread nicht; eingebunden wird nur wenig Code.
- Dagegen: ein Protokoll zwischen zwei Prozessen (Fassung, Fehlerfälle,
  Zeitüberschreitung); das Comm Studio müsste seinen eingebauten Tester
  ablösen oder daneben weiterführen.

## Richtung (Patrik, 2026-10-08)

- **Weg 1 ist verworfen:** der ganze Tester wird nicht als Bibliothek in die
  Anwendungen eingebunden.
- **Weg 2 gilt, mit einer DLL als Gegenstelle:** `SichtTest_Helper` baut
  neben `Sichttest.exe` eine **DLL**, die die ganze Schnittstelle zur
  Steuerung bereitstellt. Jede Anwendung (LumiViz, Comm Studio, weitere)
  bindet diese DLL ein. Werkzeug und Anwendungen entwickeln sich getrennt
  weiter; geprüft wird die **Verträglichkeit über die Version**.
- **Comm Studio:** der eingebaute Tester soll **entfallen**, das Comm Studio
  wird gleich aufgebaut wie LumiViz. Das ist die Absicht; was dafür aus dem
  eingebauten Tester ins Werkzeug umziehen muss (Test-Datenbank,
  Nachtest-Indikator, Statistik, Neustart mit Wiederaufnahme, Befund-Archiv),
  klärt die Abstimmung mit der Comm-Studio-Seite.

### Was an der DLL zu klären ist (Hinweise der LumiViz-Seite, nicht entschieden)

- **Schnittstelle ohne Qt-Typen.** Reicht die DLL `QString` oder `QObject`
  über ihre Grenze, muss sie mit demselben Qt und einem verträglichen
  Compiler gebaut sein wie jede Anwendung — dann wäre die getrennte
  Entwicklung dahin. Vorschlag: eine schlichte C-Schnittstelle (Aktion
  anmelden: Name, Beschreibung, Rückruf; Kanal öffnen; Ereignisse abholen),
  dazu ein kleiner Kopf für C++ und Qt, der in der Anwendung übersetzt wird.
- **Kein Qt in der DLL selbst.** Zwei verschiedene Qt-Fassungen in einem
  Prozess vertragen sich nicht (beide heißen `Qt6Core.dll`). Die DLL müsste
  ihren Kanal ohne Qt öffnen (unter Windows eine benannte Pipe). LumiViz und
  das Werkzeug nennen als Plattformen auch Linux und macOS — offen, ob die
  Steuerung dort gebraucht wird.
- **Rückrufe im richtigen Thread.** Eine Aktion verändert die Oberfläche;
  sie muss im GUI-Thread der Anwendung laufen. Die DLL nimmt die Nachricht
  an, die Anwendung holt sie in ihrem Takt ab oder bekommt sie zugestellt.
- **Zwei Versionen.** (1) Die Fassung des Protokolls zwischen `Sichttest.exe`
  und DLL, beim Verbinden ausgehandelt. (2) Die Fassung der Schnittstelle
  zwischen DLL und Anwendung, beim Laden geprüft. Regel dazu: was ändert die
  große, was die kleine Nummer, und was tut der Tester bei einer Gegenstelle,
  die älter ist.
- **Nur im Test geladen.** Die DLL sollte nur bei `--testing` geladen
  werden, damit ein normaler Start nichts öffnet, was von außen steuerbar ist.

## Kopie oder automatisch

- **Kopie von Hand:** heute schon zwei Stände nach fünf Tagen; jede weitere
  Anwendung wäre ein weiterer. Nicht empfohlen.
- **Automatisch mit Pin (empfohlen):** `SichtTest_Helper` wird ein Git-Repo
  mit Versions-Tags. Die Anwendung nennt eine Version (Pin) und bezieht
  DLL, Import-Bibliothek und Kopf — so, wie sie heute BASS als fertige DLL
  einbindet, nur automatisch geholt. Für die Entwicklung ein Schalter auf
  den lokalen Ordner (Vorbild `CMAKECRAFT_LOCAL_DIR`).
- Offen: bezieht die Anwendung die **fertige DLL** (dann muss irgendwo ein
  gebauter Stand je Version liegen) oder die **Quellen** der DLL und baut
  sie mit. Die fertige DLL passt zu Patriks Richtung; sie setzt die
  C-Schnittstelle voraus.
- Voraussetzungen: Git und Tags hier; in CMakeCraft der Bezug und das
  Verteilen einer fertigen DLL neben die Exe (für committete Externals wie
  BASS vorhanden, für geholte ungeprüft).

## Was eine Liste dafür tragen muss

- Je Liste oder Abschnitt eine **Vorbereitung** (Datei laden, Panel zeigen,
  Zustand setzen), je Schritt optional eine **Aktion**.
- In Markdown eine Form, die das Lesen nicht stört und die das Werkzeug
  erkennt; in `*.testprotokoll.json` die Felder des Comm Studio (`"setup"`,
  `"tab"`) als Ausgangspunkt.
- Die Namen der Aktionen gehören der Anwendung, die Schreibweise dem Werkzeug.
- Eine Aktion, die die Anwendung nicht kennt, darf den Lauf nicht anhalten:
  der Tester zeigt den Handgriff dann als Text.

## Offene Fragen für die Abstimmung

1. Die Schnittstelle der DLL: reine C-Schnittstelle ohne Qt, wie oben
   vorgeschlagen? Welche Aufrufe, welche zwei Versionsnummern?
2. Comm Studio: was vom eingebauten Tester muss ins Werkzeug umziehen,
   bevor er entfallen kann — und in welcher Reihenfolge?
3. Welche Kopie gilt ab jetzt? Vorschlag: diese hier; LumiViz bezieht sie
   und entfernt `projects/exec/Sichttest`.
4. Reicht der Bezug über CMakeCraft, oder braucht es dort eine Ergänzung?
5. Wie nennt eine Markdown-Liste ihre Vorbereitung und ihre Aktionen?
6. Welche Aktionen braucht jede Seite zuerst? LumiViz: Komposition laden,
   Panel nach vorn, Edit an/aus, Composer an/aus, Abspielmarke setzen.

## Vorgeschlagener Sync

- **Ort:** `…\Visuals_Project\cmake\sync_sichttest`
- **Verbindliche Spezifikation:** dieses Dokument, später als Konzept; es
  gehört `SichtTest_Helper`.
- **Teilnehmer:** `SH` SichtTest_Helper · `LV` LumiViz · `CS` Comm Studio
  (UART) · `CC` CMakeCraft, falls Frage 4 eine Lücke zeigt.

## Bis dahin in LumiViz

Die Sichttest-Listen werden neu gefasst, mit festen Vorlagen. Die
Vorbereitung steht je Abschnitt als eigener, gleich gebauter Block — so kann
das Werkzeug ihn später ausführen, ohne dass die Listen neu geschrieben werden.
