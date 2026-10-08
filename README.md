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
Sichttest [datei|ordner]
Sichttest --pruefe <datei>
Sichttest --schnapp <png> [datei|ordner]
Sichttest --selbsttest <leerer ordner>
```

- Ohne Argument: der zuletzt benutzte Ordner und das zuletzt benutzte
  Protokoll; beim ersten Start der Ordner `.claude/handover`, vom
  Arbeitsverzeichnis oder vom Ort der Exe aufwärts gesucht.
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

## Was es schreibt

Neben die Liste, in den Unterordner `sichttest-logs/`:

| Datei | Inhalt |
|---|---|
| `<Liste>_<Zeit>.testlog.json` | der Lauf: je Schritt `result` (`pass`, `pass_remark`, `fail`, `skip`, `open`), `remark`, `screenshots`, `rated`; dazu `build` und `summary` — Format des UART-Testers |
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
