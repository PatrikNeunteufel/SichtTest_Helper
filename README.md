# SichtTest_Helper

Ein eigenes Fenster zum Abhaken einer Sichttest-Liste. Es läuft **neben** dem
Programm, das geprüft wird, und kennt dieses Programm nicht: Punkt für Punkt
**Pass**, **Pass mit Befund**, **Fail** oder **Überspringen**, dazu eine
Bemerkung und Screenshots. Das Ergebnis ist ein Testlog und ein Report, die
ein Mensch und Claude lesen.

**Herkunft:** Vorbild ist das Abnahme-Protokoll des UART-Studios
(`UART/gui/Comm_Studio/TestProtokollWindow`), dort fest ins Studio eingebaut.
Diese eigenständige Fassung entstand am 2026-10-03 in LumiViz
(`projects/exec/Sichttest`, Session 93) und wurde von dort hierher kopiert.
Seit dem 2026-10-08 wird das Werkzeug **hier** entwickelt und gebaut. Die
Kopie in LumiViz besteht noch (Abweichung: zwei Zeilen); sie entfällt, sobald
LumiViz das Werkzeug von hier bezieht.

**Geplant:** Das Werkzeug soll die geprüfte Anwendung steuern (Vorlage laden,
Tab oder Panel wechseln, Zustand herstellen). Dazu baut dieses Projekt eine
DLL, die die Anwendungen einbinden. Stand: Konzept, nichts gebaut —
`docs/Konzept_Steuerung.md`, Ausgangspunkt `docs/Idee_Steuerung_der_Anwendung.md`.

## Aufbau

```
SichtTest_Helper/
├── CMakeLists.txt              Einstieg, holt CMakeCraft
├── CMakeCraftBootstrap.cmake   Bezug des Build-Systems
├── cmakecraft.pin              gepinnte CMakeCraft-Version
├── CMakePresets.json           Presets (aus LumiViz übernommen, ungekürzt)
├── CMakeUserPresets.json       lokal: QT_ROOT (nicht versioniert)
├── Solution.json               was gebaut wird: Executable Sichttest, Qt6
├── LICENSE, LICENSE-MIT, LICENSE-APACHE
├── beispiele/                  eine Liste in Markdown, eine in JSON
├── docs/                       Idee und Konzept der Steuerung
├── projects/libs/              die DLL zur Steuerung (geplant, noch leer)
└── projects/exec/Sichttest/src/
    ├── main.cpp                Fenster, Aufrufe, Selbsttest
    ├── Protokoll.hpp/.cpp      Listen lesen, Testlog und Report schreiben (ohne Widgets)
    └── Source.cmake            Quellenliste
```

Lokal und nicht versioniert: `out/`, `.externals/` (geholtes CMakeCraft),
`CMakeUserPresets.json`, `CLAUDE.md`, `.claude/`, `sichttest-logs/`.

## Bauen

```bash
cmake --preset windows-ninja-release-clang
```

```bash
cmake --build --preset build-ninja-release-clang --target Sichttest
```

Exe: `out/build/windows-ninja-release-clang/exec/Sichttest/bin/Release/Sichttest.exe`

Voraussetzung: `QT_ROOT` zeigt auf ein Qt-6-Kit (lokal in
`CMakeUserPresets.json`, etwa `C:/Qt/6.10.1/msvc2022_64`). CMakeCraft wird beim
Configure in der Version aus `cmakecraft.pin` nach `.externals/` geholt.

Prüfen: `Sichttest.exe --selbsttest <leerer Ordner>` endet mit Exit-Code 0.

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
- Liegt im Ordner oder darüber eine `sichttest.projekt.json`, gelten deren
  Listen und Ablage (siehe „Projektdatei").
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

**`*.testprotokoll.json`** — das Format des UART-Testers
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
- Scheitert eine Aktion oder kennt die Anwendung sie nicht, steht die Meldung rot am Punkt und
  darunter der Handgriff als Text. Der Lauf hält nie an, das Urteil bleibt beim Menschen.
- In JSON: `"anwendung"`, `"vorbereitung": [ … ]` an der Wurzel, je Schritt `"aktionen": [ { "aktion":
  …, "mit": { … }, "text": …, "frist": Sekunden } ]`, `"kind": "prep"` für eine Vorbereitung,
  `"nachbereitung": [ … ]` an der Wurzel für das Ende des Laufs.

### Projektdatei

Eine `sichttest.projekt.json` im Repo der geprüften Anwendung sagt dem Werkzeug, was es über
das Projekt wissen muss (Beschreibung: `docs/Konzept_Steuerung.md` §7). Pfade sind relativ zur
Datei; unbekannte Schlüssel werden überlesen.

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
| `listen[]` | Ordner mit Listen (oder einzelne Listen), je mit `ablage` und `muster` | der Ordner der Projektdatei, Ablage `sichttest-logs/` |
| `abbildung` | welche Aktion ein Feld des JSON-Formats auslöst | das Feld wird gelesen und als Text gezeigt |
| `gewichtung` | Dateien des Nachtest-Indikators | wird gelesen, wirkt noch nicht |

Die Felder des UART-Testers bleiben gültig und werden über `abbildung` zu Aktionen:
`setup.close_all_tabs`, `setup.open`, `test_db` und `test_db.ende` (Vorbereitung und
Nachbereitung der Liste), `tab` und `restart` (am Schritt), dazu die Links `tab:<Titel>` und
`sql:<Abfrage>` im Text. Eine Abfrage legt das Werkzeug selbst in die Zwischenablage.

## Was es schreibt

Neben die Liste, in den Unterordner `sichttest-logs/` — oder in die Ablage, die das Projekt
für den Ordner der Liste nennt:

| Datei | Inhalt |
|---|---|
| `<Liste>_<Zeit>.testlog.json` | der Lauf: je Schritt `result` (`pass`, `pass_remark`, `fail`, `skip`, `open`), `remark`, `screenshots`, `rated`, bei ausgelösten Aktionen `actions`; dazu `build` und `summary` — Format des UART-Testers. Aus der Liste gehen `setup`, `test_db` und `areas` unverändert mit, die Nachbereitung steht unter `teardown_actions` |
| `<Liste>_<Zeit>.report.md` | derselbe Stand zum Lesen: zuerst Fail und Pass mit Befund samt Bemerkung und Bildern, dann Übersprungen, Offen, Pass |
| `<Liste>_<Zeit>_<Kennung>_<n>.png` | die Screenshots |

Geschrieben wird nach jeder Bewertung, nach jedem Screenshot, beim Wechsel des
Schritts und beim Schließen. Beim nächsten Start wird der jüngste Lauf der
Liste fortgesetzt; **▶ Neuer Lauf** beginnt frisch.

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

## Was der UART-Tester mehr kann (nicht übernommen)

Sprung zu einem Tab der Anwendung, Setup-Phase (Tabs schließen, Projekte
öffnen), Test-Datenbank, Neustart der Anwendung mit Wiederaufnahme, Befund-
Archiv je Schritt, risikobasierter Nachtest-Indikator, Statistik über alle
Läufe. All das hängt dort an der Anwendung oder an einer Ergebnis-Datenbank.
Was davon mit der Steuerung hierher umzieht, klärt das Konzept
(`docs/Konzept_Steuerung.md`).

## Lizenz

SichtTest_Helper steht wahlweise unter **MIT** ([LICENSE-MIT](LICENSE-MIT)) oder
**Apache-2.0** ([LICENSE-APACHE](LICENSE-APACHE)). Das Werkzeug benutzt Qt 6, das unter
eigenen Lizenzen steht; Einzelheiten in [LICENSE](LICENSE).
