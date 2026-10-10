# SichtTest_Helper

**Sichttest** ist ein eigenständiges Programm, mit dem ein Mensch Sichttests
durchführt. Es zeigt eine Liste Punkt für Punkt und nimmt je Punkt ein Urteil
auf — **Pass**, **Pass mit Befund**, **Fail** oder **Überspringen** —, dazu
eine Bemerkung und Screenshots. Das Ergebnis ist ein Testlog und ein Report,
die ein Mensch und Claude lesen. Das Programm läuft **neben** der Anwendung,
die geprüft wird, und braucht von ihr nichts.

**Zusätzlich** kann Sichttest die geprüfte Anwendung steuern: eine Vorlage
laden, ein Tab oder Panel nach vorn holen, einen Zustand herstellen. Dafür
bindet die Anwendung eine kleine DLL ein, die dieses Projekt mitliefert, und
meldet ihre Aktionen an; die Listen nennen dann je Punkt, was auszulösen ist.
Ohne die DLL arbeitet das Werkzeug wie bisher, und auch mit ihr bleibt jeder
Handgriff als Text stehen — das Urteil fällt immer der Mensch.
Beschreibung: `docs/Konzept_Steuerung.md`.

**Gebaut wird mit [CMakeCraft](https://github.com/PatrikNeunteufel/CMakeCraft):**
Das Build-System liegt nicht im Repo, es wird beim Configure in der Version aus
`cmakecraft.pin` geholt. Was gebaut und geschnürt wird, steht allein in
`Solution.json`. Anwendungen, die selbst mit CMakeCraft (ab v0.10.0) bauen,
beziehen das fertige Paket von hier als External der Art `archive`.

## Aufbau

```
SichtTest_Helper/
├── CMakeLists.txt              Einstieg, holt CMakeCraft
├── CMakeCraftBootstrap.cmake   Bezug des Build-Systems
├── cmakecraft.pin              gepinnte CMakeCraft-Version
├── CMakePresets.json           Presets
├── CMakeUserPresets.json       lokal: QT_ROOT (nicht versioniert)
├── Solution.json               was gebaut und geschnürt wird, Version
├── LICENSE, LICENSE-MIT, LICENSE-APACHE
├── beispiele/                  Listen in Markdown und JSON, eine mit Aktionen
├── docs/                       Idee und Konzept der Steuerung, Einrichtung und Veröffentlichung
├── packaging/VERSION.in        Schablone der Datei VERSION im Paket
├── projects/exec/Sichttest/src/        das Werkzeug
│   ├── main.cpp                Fenster, Aufrufe, Selbsttest
│   ├── Protokoll.hpp/.cpp      Listen lesen, Testlog und Report schreiben (ohne Widgets)
│   ├── Projekt.hpp/.cpp        die Projektdatei sichttest.projekt.json
│   ├── Steuerung.hpp/.cpp      Anwendungen annehmen, Aktionen aufrufen
│   └── Kanal.hpp/.cpp          die Pipe, auf der das Werkzeug lauscht
├── projects/libs/SichttestSteuerung/   die DLL für die geprüfte Anwendung (ohne Qt)
│   ├── include/                die drei Köpfe: C, C++, Qt
│   └── src/                    Kanal, Protokoll, Schnittstelle
└── projects/exec/Gegenprobe/, GegenprobeQt/   kleine Anwendungen für den Selbsttest
```

Lokal und nicht versioniert: `out/`, `.externals/` (geholtes CMakeCraft),
`CMakeUserPresets.json`, `CLAUDE.md`, `.claude/`, `sichttest-logs/`.

## Bauen

```bash
cmake --preset windows-ninja-release-clang
```

```bash
cmake --build --preset build-ninja-release-clang
```

Das baut alle Targets: das Werkzeug, die DLL `SichttestSteuerung1.dll` und die zwei Gegenproben,
die der Selbsttest braucht.

Exe: `out/build/windows-ninja-release-clang/exec/Sichttest/bin/Release/Sichttest.exe`

Voraussetzung: CMake ab 3.26; `QT_ROOT` zeigt auf ein Qt-6-Kit (lokal in
`CMakeUserPresets.json`, etwa `C:/Qt/6.10.1/msvc2022_64`). CMakeCraft wird beim
Configure in der Version aus `cmakecraft.pin` nach `.externals/` geholt.

Prüfen: `Sichttest.exe --selbsttest <leerer Ordner>` endet mit Exit-Code 0.

Das Paket für die Anwendungen (Köpfe, DLL, Werkzeug mit seinem Qt) schnürt

```bash
cmake --build --preset build-ninja-release-clang --target package_sichttest
```

nach `out/package/`. Ablauf einer Veröffentlichung: `docs/GitHub_Einrichtung.md`, Teil B.

## Aufruf

```bash
Sichttest [datei|ordner|projektdatei]
Sichttest --pruefe <datei>
Sichttest --schnapp <png> [datei|ordner]
Sichttest --selbsttest <leerer ordner>
Sichttest --steuerung
```

- Ohne Argument: der zuletzt benutzte Ordner und das zuletzt benutzte
  Protokoll; beim ersten Start der Ordner `.claude/handover`, vom
  Arbeitsverzeichnis oder vom Ort der Exe aufwärts gesucht.
- Liegt im Ordner oder darüber ein Ordner `.sichttest` mit einer
  `sichttest.projekt.json`, gelten deren Listen und Ablage (siehe „Projektdatei").
- Eine ausdrücklich genannte Projektdatei muss ebenfalls in einem Ordner
  `.sichttest` liegen; ihr Name ist frei, muss aber auf `.projekt.json` enden.
- `--steuerung`: so startet die DLL der geprüften Anwendung das Werkzeug, wenn
  keines lauscht. Es öffnet dann die Listen des Projekts dieser Anwendung.
- `--pruefe` gibt die erkannten Schritte auf die Standardausgabe.
- `--schnapp` baut das Fenster auf, legt es als Bild ab und endet.
- `--selbsttest` legt eine kleine Liste an, bewertet sie über die Knöpfe und
  prüft Log und Report; der Exit-Code ist die Zahl der gescheiterten Prüfungen.

## Was es liest

**Markdown-Checklisten** — jede `*.md` des Ordners, die Punkte enthält
(Beispiel: `beispiele/Beispiel.md`):

```markdown
## A. Abschnitt

- [ ] **A1 Titel des Punkts:** Was zu tun ist und was zu sehen sein muss,
      über mehrere eingerückte Zeilen.
```

Der Anfang des Fettdrucks ist die Kennung (`A1`), sofern er wie eine aussieht;
sonst zählt das Werkzeug durch (`P7`). Haken in der Datei gelten als
Ausgangsstand: `[x]` Pass, `[!]` Fail, `[-]` übersprungen. Eine Exe in
Backticks im Vorspann wird erkannt. **Die Liste selbst wird nie verändert.**

**`*.testprotokoll.json`** — dieselbe Liste als JSON: `title`, `description` und
`steps`, je Schritt `id`, `section`, `title`, `text`
(Beispiel: `beispiele/Beispiel.testprotokoll.json`).

### Aktionen

Ein Punkt kann Aktionen tragen, die das Werkzeug in der geprüften Anwendung auslöst
(Beispiel: `beispiele/Beispiel_Aktionen.md`, Beschreibung: `docs/Konzept_Steuerung.md` §6).
Die Anwendung muss dafür mit `--testing` laufen und ihre Aktionen angemeldet haben; ohne
Verbindung verhält sich die Liste wie eine ohne Aktionen.

```markdown
**Anwendung:** MeineAnwendung

- [ ] **A0 Vorbereiten:** Vorlage laden, dann Bearbeiten einschalten.
      `aktion: vorlage_laden datei="vorlagen\ansicht A.vorlage"` `aktion: bearbeiten an`
- [ ] **A1 Die Marke steht an ihrer Zeit:** Marke auf 0:20 setzen. `aktion: marke zeit=0:20`
- [ ] **A2 Wie A0, aber ohne Bearbeiten:** … `aktion: @A0` `aktion: bearbeiten aus`
```

- Form: `` `aktion: <name> [wert] [schlüssel=wert …]` ``, Werte mit Leerzeichen in `"…"`. Ein Wert
  ohne Schlüssel kommt als Argument `wert` an. Im Fenster stehen die Stücke nicht im Text.
- Ein Punkt, dessen Titel mit **Vorbereiten** beginnt, ist eine Vorbereitung: kein Urteil,
  dafür **▶ Ausführen** und **Weiter →**; er zählt nicht mit. Vor dem ersten Abschnitt gilt er
  für die ganze Liste.
- Die Aktionen eines gewöhnlichen Punkts laufen nur auf **▶ Herstellen**. **↺ Vorbereitung**
  wiederholt die Vorbereitung des Abschnitts.
- `` `aktion: @A0` `` führt die Aktionen des Punkts `A0` an dieser Stelle aus.
- Vor Aktionen, die Ungespeichertes verwerfen können, fragt das Werkzeug nach. Der Haken
  **In dieser Sitzung nicht mehr fragen** stellt das ab, bis das Werkzeug beendet wird.
- Hat die Anwendung das Werkzeug gestartet und wird sie beendet, bietet es unter der
  Statuszeile **Tester beenden** an. Es schließt nie von selbst: die Anwendung kann auch nur
  neu starten.
- Scheitert eine Aktion oder kennt die Anwendung sie nicht, steht die Meldung rot am Punkt und
  darunter der Handgriff als Text. Der Lauf hält nie an, das Urteil bleibt beim Menschen.
- In JSON: `"anwendung"`, `"vorbereitung": [ … ]` an der Wurzel, je Schritt `"aktionen": [ { "aktion":
  …, "mit": { … }, "text": …, "frist": Sekunden } ]`, `"kind": "prep"` für eine Vorbereitung,
  `"nachbereitung": [ … ]` an der Wurzel für das Ende des Laufs.

### Projektdatei

Die Datei `.sichttest/sichttest.projekt.json` im Root des Repos der geprüften Anwendung sagt dem
Werkzeug, was es über das Projekt wissen muss (Beschreibung: `docs/Konzept_Steuerung.md` §7).
Pfade gelten ab dem Root, also dem Ordner über `.sichttest`; unbekannte Schlüssel werden
überlesen. Eine Projektdatei, die nicht in einem Ordner `.sichttest` liegt, lädt das Werkzeug
nicht und meldet es.

```json
{
  "schema": 1,
  "anwendung": "MeineAnwendung",
  "start": { "exe": "out/build/MeineAnwendung.exe", "argumente": ["--testing"] },
  "listen": [
    { "ordner": "tests/sichttest", "ablage": "tests/sichttest-logs" },
    { "ordner": "docs", "muster": "Sichttest_*.md" }
  ],
  "abbildung": { "tab": "tab_zeigen titel", "setup.open": "datei_oeffnen pfad" }
}
```

| Schlüssel | Bedeutung | fehlt er |
|---|---|---|
| `anwendung` | Name, unter dem sich die Anwendung meldet; gilt für Listen, die keinen nennen | die zuletzt verbundene Anwendung |
| `start` | womit **▶ Exe starten** die Anwendung startet | die Exe aus der Liste, ohne Argumente |
| `listen[]` | Ordner mit Listen (oder einzelne Listen), je mit `ablage` und `muster` | der Root des Projekts, Ablage `sichttest-logs/` im Root |
| `abbildung` | welche Aktion ein Feld des JSON-Formats auslöst | das Feld wird gelesen und als Text gezeigt |
| `gewichtung` | Dateien des Nachtest-Indikators: `{ "hand": [ … ], "git": [ … ] }`; auch leer (`{}`) schaltet es den Indikator ein | kein Indikator |

Das JSON-Format kennt einige Felder, die erst über `abbildung` zu Aktionen werden:
`setup.close_all_tabs`, `setup.open`, `test_db` und `test_db.ende` (Vorbereitung und
Nachbereitung der Liste), `tab` und `restart` (am Schritt), dazu die Links `tab:<Titel>` und
`sql:<Abfrage>` im Text. Eine Abfrage legt das Werkzeug selbst in die Zwischenablage. Welche
Aktion ein Feld auslöst, bestimmt allein die Projektdatei; das Werkzeug kennt keine
Aktionsnamen irgendeiner Anwendung.

Eine Zeile der `abbildung` nennt nach dem Namen der Aktion die Schlüssel, die als Argumente
mitgehen (`"test_db": "testdb_einrichten name seed"`); sie kommen unverändert aus dem Feld der
Liste. Der Schlüssel **`fortgesetzt`** ist vorbelegt: ihn füllt das Werkzeug beim Aufruf mit
`true`, wenn der Lauf schon eine Bewertung trägt, sonst mit `false`. Antwortet die Aktion aus
`test_db` mit `ok`, steht im Testlog `test_db.active: true`; nach einem `ok` der Aktion aus
`test_db.ende` steht es auf `false`.

Vor jedem Aufruf erlaubt das Werkzeug der Anwendung, ihr Fenster oder einen Dialog nach vorn
zu holen.

Die Nachbereitung einer Liste bietet das Werkzeug an, wenn der letzte Punkt bewertet ist, und
beim Schließen — dort auch dann, wenn noch nichts bewertet, aber schon eine Aktion der
Vorbereitung gelaufen ist.

**Zustände der Anwendung.** Eine Anwendung kann melden, was sie hergestellt hat und was
davon noch steht (`meldeZustand("test_db", true, "studiotest")` in den Headern). Eine Aktion
der Nachbereitung nennt mit `"wenn": "test_db"` — in der `abbildung` als
`"test_db.ende": "testdb_abbauen wenn=test_db"` —, wofür sie da ist. Steht so ein Zustand,
zeigt das Werkzeug es gleich nach dem Verbinden als Zeile mit dem Knopf **Nachbereitung
ausführen**, auch wenn er aus einem früheren Lauf übrig ist; das Testlog trägt unter
`zustaende`, was die Anwendung zuletzt gemeldet hat. Der Wert `{fortgesetzt}` in den
Argumenten einer Aktion wird beim Aufruf durch wahr oder falsch ersetzt.

**Build am Urteil.** Jeder bewertete Schritt trägt im Testlog `build` mit der Exe und ihrem
Zeitstempel vom Zeitpunkt der Bewertung; `build` an der Wurzel nennt den zuletzt benutzten.
Ein Urteil zu einem Schritt, den die Liste nicht mehr kennt, bleibt im Testlog und steht am
Ende unter „Nicht mehr in der Liste".

## Was es schreibt

Neben die Liste, in den Unterordner `sichttest-logs/` — oder in die Ablage, die das Projekt
für den Ordner der Liste nennt:

| Datei | Inhalt |
|---|---|
| `<Liste>_<Zeit>.testlog.json` | der Lauf: je Schritt `result` (`pass`, `pass_remark`, `fail`, `skip`, `open`), `remark`, `screenshots`, `rated`, bei ausgelösten Aktionen `actions`; dazu `build` und `summary`. Aus der Liste gehen `setup`, `test_db` und `areas` unverändert mit, die Nachbereitung steht unter `teardown_actions`. War eine Anwendung verbunden, nennt `steuerung` ihren Namen, ihre Version, die Version der DLL (`produkt`), `p`, `s` und den `zustand` der Verbindung |
| `<Liste>_<Zeit>.report.md` | derselbe Stand zum Lesen: zuerst Fail und Pass mit Befund samt Bemerkung und Bildern, dann Übersprungen, Offen, Pass |
| `<Liste>_<Zeit>_<Kennung>_<n>.png` | die Screenshots |
| `testergebnisse.sqlite` | je Ablage eine kleine SQLite-Datei über alle Läufe: je Lauf Summe und Build, je Schritt Urteil, Bemerkung und der Build, gegen den es fiel. Das Testlog bleibt die Wahrheit, die Datei ist der Index dazu |

Geschrieben wird nach jeder Bewertung, nach jedem Screenshot, beim Wechsel des
Schritts und beim Schließen. Beim nächsten Start wird der jüngste Lauf der
Liste fortgesetzt, solange er offene Schritte hat. Ist er abgeschlossen, beginnt
ein neuer Lauf, und die Kopfzeile sagt es; der abgeschlossene bleibt, wie er ist.
**▶ Neuer Lauf** beginnt jederzeit frisch.

`build` nennt die Exe, gegen die der Lauf lief, und ihren Zeitstempel (ISO): die
der verbundenen Anwendung; ohne Verbindung die aus der Liste oder dem Projekt.
Ein fortgesetzter Lauf ohne Verbindung behält, was sein Testlog schon nennt.

## Nachtest-Indikator

Nennt die Projektdatei `gewichtung`, zeigt das Werkzeug an jedem offenen Schritt, wie dringend
er wieder zu prüfen ist: **🔴** dringend (ab 0,7), **🟡** empfohlen (ab 0,3), ohne Zeichen
unkritisch. Die Begründung steht am Schritt, die Listenwahl nennt je Liste die Zahl der 🔴 und 🟡.

| Fall | Dringlichkeit |
|---|---|
| in `retest_steps` genannt · noch nie Pass oder Fail · letzter Test Fail | 1,0 |
| letzter Test Pass, eine Area des Schritts wurde danach geändert | ihr `weight`, das höchste gilt |
| letzter Test Pass, danach ein Commit an den Pfaden einer Area | `auto_weight` (Vorgabe 0,5), hebt nur |
| letzter Test Pass, nichts geändert | 0 |

Übersprungen zählt nicht als geprüft. Das letzte Ergebnis kommt je Schritt-Kennung aus der
Ergebnis-DB der Ablage, über alle Listen. Verglichen wird gegen den Build, gegen den der Pass
fiel.

Die **Hand-Datei** (`hand`) pflegt, wer den Code ändert:

```json
{ "auto_weight": 0.5,
  "retest_steps": { "db-03": "Fix aus Lauf vom 10.10. nachtesten" },
  "areas": { "datenbank": { "weight": 0.8, "changed": "2026-10-10", "note": "Abfrage umgebaut",
                            "paths": [ "src/db/*.cpp" ] } } }
```

`changed` ist ein Datum oder ein Zeitstempel (`2026-10-10T14:30:00`). Ein Schritt nennt seine
Areas im Feld `areas` der Liste (JSON-Format). Die **git-Datei** (`git`) schreibt ein git-Hook:
je Area der jüngste Commit, der ihre `paths` berührt hat. Der Hook liegt im Paket unter
`werkzeuge/` (`install_hooks.ps1` im Repo der Anwendung einmal je Klon aufrufen, braucht
Python); er liest die Pfade aus der Projektdatei — die erste Datei unter `hand` gehört zur
ersten unter `git`, und so weiter. Die git-Datei ist rechnerlokal und gehört nicht ins Repo.

**↷ Unkritische überspringen** markiert alle offenen Schritte unter 0,3 als übersprungen;
derselbe Knopf nimmt genau diese wieder auf, von Hand Übersprungene bleiben.

## Statistik und ältere Läufe

**Statistik…** öffnet ein eigenes Fenster: die Lage in einem Satz, alle Läufe der Liste
(Start, Build, Zähler, Testlog) und ihre Schritte mit Pässen, Fails und dem letzten Ergebnis —
die Problemschritte zuerst. Es bleibt offen und frischt sich bei jedem Schreiben auf.

**Lauf öffnen** (oder ein Doppelklick) zeigt einen älteren Lauf im Hauptfenster, **nur zum
Ansehen**: Urteile, Bemerkungen und Bilder sind da, geschrieben wird nichts. Hat er noch offene
Schritte, macht **Diesen Lauf fortsetzen** ihn bearbeitbar; **Zurück zum aktuellen Lauf** führt
zurück.

## Bedienung

| Was | Wie |
|---|---|
| Pass / Pass mit Befund / Fail / Überspringen | Knöpfe oder Strg+1 / 2 / 3 / 4; danach zum nächsten offenen Punkt |
| Bewertung zurücknehmen | ○ Offen |
| Screenshot | Win+Shift+S, dann **📋 Screenshot anhängen**, Strg+Shift+V oder Strg+V im Bemerkungsfeld; Bilddateien ins Fenster ziehen |
| Screenshot ansehen / löschen | Maus darüber (Vorschau), Doppelklick öffnet, Entf oder Rechtsklick löscht |
| Fenster über dem Programm halten | Haken **Im Vordergrund** |
| nur das Unerledigte | Haken **Nur offene und Fail zeigen** |
| Befunde weitergeben | **Report-Pfad kopieren** und im Chat einfügen |

Fail und Pass mit Befund verlangen eine Bemerkung oder einen Screenshot.

## Was das Werkzeug noch nicht kann

Ein Befund-Archiv je Schritt. Geplant ist das in `docs/Konzept_Steuerung.md` §14.1.

Gebaut und geprüft ist nur Windows; die Steuerung gibt es nur dort.

## Lizenz

SichtTest_Helper steht wahlweise unter **MIT** ([LICENSE-MIT](LICENSE-MIT)) oder
**Apache-2.0** ([LICENSE-APACHE](LICENSE-APACHE)). Das Werkzeug benutzt Qt 6, das unter
eigenen Lizenzen steht; Einzelheiten in [LICENSE](LICENSE).
