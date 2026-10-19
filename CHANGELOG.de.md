[English](CHANGELOG.md) · **Deutsch**

# Änderungen

Jede Version ist ein Git-Tag `vX.Y.Z`; ab v0.2.0 trägt ein Release auf GitHub das Paket
`sichttest-vX.Y.Z-win64.zip` (Köpfe, DLL, Tester). Schnittstelle **S** und Protokoll **P** haben
eigene Nummern ([Spezifikation §5](docs/Specification.de.md#5-drei-nummern-zwei-davon-werden-geprüft)).

## Unveröffentlicht

- Dokumentation englisch als Standard, deutsch daneben (`.de.md`). Neu: die
  [Spezifikation](docs/Specification.de.md) (ersetzt `docs/Konzept_Steuerung.md`), die
  [Anleitung zur Anbindung](docs/Integration_Guide.de.md), [templates/](templates/) für
  Anwendungen, ein Beispielprojekt in [examples/project/](examples/project/),
  [Veröffentlichen](docs/Releasing.de.md) (ersetzt `docs/GitHub_Einrichtung.md`). `beispiele/`
  heißt jetzt `examples/`.
- Build: CMakeCraft v0.11.1.

## v0.4.0 — 2026-10-11 · S 1.1 · P 1.1

- **Zustände der Anwendung:** `sts_melde_zustand` / `meldeZustand` meldet einen benannten Zustand
  (etwa eine Test-Datenbank); eine Nachbereitung mit `"wenn": "<zustand>"` wird angeboten, solange
  er steht, auch gleich nach dem Verbinden. `sts_bei_ende` / `beiEnde` meldet der Anwendung, dass
  der Tester gegangen ist. Das Testlog führt `zustaende`. Anwendungen mit Köpfen S 1.0 laufen
  weiter.
- `{fortgesetzt}` als Wert in den Argumenten jeder Aktion.
- **Build am Urteil:** jeder bewertete Schritt trägt die Exe und ihren Zeitstempel; verwaiste
  Urteile (Schritte, die die Liste nicht mehr kennt) bleiben im Testlog.
- **Ergebnis-DB** `testergebnisse.sqlite` je Ablage; Fenster **Statistik…**; ältere Läufe öffnen
  nur zum Ansehen und lassen sich fortsetzen oder bearbeiten; **Ältere Testlogs einlesen** nimmt
  Testlogs auf, die die DB nicht kennt.
- **Nachtest-Indikator** (🔴/🟡) aus `gewichtung` der Projektdatei, mit Hand- und git-Schicht,
  **↷ Unkritische überspringen** und einem git-Hook im Paket (`werkzeuge/`).
- **Befund-Archiv:** `history[]` je Schritt, abgeheftet per Knopf und von selbst; abgeschlossene
  Läufe sind bearbeitbar.
- **Links:** jeder Schlüssel der `abbildung` ist ein Link im Text des Schritts; der Tab eines
  Schritts ist ein Link unter seinem Titel.
- `rated_at` (Zeitpunkt des Urteils): der Indikator vergleicht gegen den Tag des Urteils; das
  letzte Ergebnis eines Schritts ist sein jüngstes Urteil.
- Fenster: Teiler zwischen Schritttext und Bemerkung; Listenwahl und Schrittliste zeigen zuerst den
  Titel.
- Ein Report je Lauf; `summary.open` zählt Vorbereitungen nie.

## v0.3.6 — 2026-10-10

- Schließen nach bloßer Vorbereitung (noch nichts bewertet) bietet die Nachbereitung an.

## v0.3.5 — 2026-10-10

- `test_db.active` im Testlog; vorbelegter Schlüssel `fortgesetzt` in der `abbildung`.
- Vor jedem Aufruf erlaubt der Tester der Anwendung, nach vorn zu kommen.

## v0.3.4 — 2026-10-10

- Ein abgeschlossener Lauf wird nicht mehr fortgesetzt: das Öffnen seiner Liste beginnt einen
  neuen.
- `build` bleibt erhalten, wenn ein Lauf ohne Verbindung fortgesetzt wird.

## v0.3.3 — 2026-10-09

- Das Testlog nennt die verbundene Anwendung und die DLL (Block `steuerung`).

## v0.3.2 — 2026-10-09

- Die Rückfrage vor Aktionen mit `STS_FRAGT_NACH` hat den Haken „In dieser Sitzung nicht mehr
  fragen“.
- Nach dem Ende der Anwendung bietet der Tester **Tester beenden** an.

## v0.3.1 — 2026-10-09

- Die Köpfe nennen die Fassung der geladenen DLL (`fassung()`).

## v0.3.0 — 2026-10-09

- Die Projektdatei liegt unter `.sichttest/sichttest.projekt.json`; ihre Pfade gelten ab dem Root
  des Projekts.

## v0.2.0 — 2026-10-09 · S 1.0 · P 1

- **Steuerung der geprüften Anwendung:** DLL `SichttestSteuerung1.dll` mit der C-Schnittstelle
  S 1.0, Köpfe für C++ und Qt; der Tester lauscht auf einer benannten Pipe (P 1).
- Aktionen in Listen (Markdown `aktion:`, JSON `vorbereitung` / `aktionen` / `nachbereitung`),
  Knöpfe ▶, der Handgriff als Rückfall.
- Projektdatei; Felder des JSON-Formats, die über die `abbildung` zu Aktionen werden.
- Erstes Paket und Release, gebaut mit CMakeCraft v0.10.0.

## v0.1.0 — 2026-10-08

- Das Werkzeug als eigenes Projekt: Markdown-Checklisten und `*.testprotokoll.json` abhaken,
  Testlog, Report und Screenshots schreiben.
