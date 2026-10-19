[English](Integration_Guide.md) · **Deutsch**

# Eine Anwendung anbinden

Diese Anleitung zeigt, wie eine Anwendung von Sichttest steuerbar wird: welche Aktionen sie
anbietet, wie sie das Paket bezieht, was in ihren Quelltext, ihre Projektdatei und ihre Listen
gehört. Jede Regel, auf die sie sich bezieht, steht in der [Spezifikation](Specification.de.md);
die Dateien zum Kopieren liegen in [templates/](../templates/). Die Beispiele benutzen eine
Anwendung namens **MyApp**, wie die (englischen) Vorlagen.

Ohne all das arbeitet Sichttest schon als Checklisten-Werkzeug neben der Anwendung — die Anbindung
fügt nur hinzu, dass Punkte einer Liste Aktionen in ihr auslösen können.

## Überblick

| Schritt | Was | Ergebnis |
|---|---|---|
| 1 | Namen und Aktionen wählen | eine kurze Liste von Aktionen mit Schaltern |
| 2 | Paket beziehen | Köpfe beim Bauen; DLL und Tester neben der Exe |
| 3 | Aktionen im Code anmelden | `MyApp.exe --testing` verbindet sich mit dem Tester |
| 4 | Projektdatei schreiben | der Tester findet Listen, Ablage und Start |
| 5 | Listen schreiben | Punkte lösen Aktionen aus; der Text bleibt der Rückfall |
| 6 | wahlweise: Nachtest-Indikator | 🔴/🟡 an Schritten, deren Code sich geändert hat |
| 7 | ausprobieren | die Prüfungen am Ende dieser Anleitung |

## 1. Name und Aktionen

**Der Name** (`anwendung`) ist der, unter dem sich MyApp beim Tester meldet. Listen und
Projektdatei benutzen denselben Namen. Einmal wählen; er erscheint in den Testlogs.

**Aktionen** sind, was eine Liste auslösen darf: eine Vorlage laden, ein Tab nach vorn holen, eine
Test-Datenbank einrichten. Die Namen gehören der Anwendung — `[a-z0-9_.]`, etwa `datei_oeffnen`,
`tab_zeigen`, `bearbeiten`. Das Werkzeug kennt keinen davon; es erfährt sie bei jedem Verbinden.

Gute Kandidaten sind die Handgriffe, die ein Tester sonst **vor** der eigentlichen Prüfung von Hand
täte (einen Zustand herstellen), nicht das, was geprüft wird: lautet die Prüfung „der Knopf
Bearbeiten schaltet das Bearbeiten ein“, soll die Liste ihn nicht über eine Aktion drücken. Je
Aktion entscheiden:

| Frage | Wenn ja |
|---|---|
| Kann sie etwas zerstören, das der Mensch vermissen würde (Tabs schließen, Ungespeichertes verwerfen) — ohne selbst zu fragen? | Schalter `STS_FRAGT_NACH`: der Tester fragt vorher |
| Räumt sie ab, statt herzustellen, gehört also nur an den Anfang eines neuen Laufs? | Schalter `STS_NUR_ERSTLAUF`: entfällt beim Fortsetzen |
| Öffnet sie einen Dialog in MyApp, dauert also so lange, wie der Mensch braucht? | Schalter `STS_WARTET_AUF_MENSCH`: keine Frist. Nie zusammen mit `STS_FRAGT_NACH` |
| Endet MyApp nach der Antwort oder startet neu? | Schalter `STS_BEENDET_ANWENDUNG`: der Tester wartet auf die neue Verbindung |

Zwei Regeln machen fortgesetzte Läufe möglich:

- **Aktionen der Vorbereitung sind wiederholbar.** Beim Fortsetzen bereitet der Tester erneut vor
  (ohne `STS_NUR_ERSTLAUF`). Eine Aktion, die eine Datei öffnet, antwortet `ok`, wenn die Datei
  schon offen ist.
- **Nie `ok` ohne Ergebnis.** Eine Aktion, die einen Zustand herstellt, antwortet `fehler`, wenn der
  Mensch ablehnt oder etwas scheitert, und meldet den Zustand (Schritt 3). Der Tester verlässt sich
  darauf.

Antworten sind `ok`, `fehler` (gescheitert, mit einem Text, der sagt, warum) oder `ungueltig`
(Argumente passen nicht). Der Text der Antwort erscheint im Tester.

## 2. Paket beziehen

[templates/sichttest.pin](../templates/sichttest.pin) in den Root des Repos kopieren und Version
und Prüfsumme setzen (aus der `.zip.sha256` des Release). Dann eines von beiden:

- **Mit CMakeCraft** (ab v0.10.0):
  [templates/integration/Solution.external.json](../templates/integration/Solution.external.json)
  in `Solution.json` einfügen und `"sichttest"` unter `externals` der Exe nennen, die die Aktionen
  anmeldet.
- **Ohne CMakeCraft:** `CMakeCraftPackage.cmake` unverändert aus dem Repo von CMakeCraft nach
  `cmake/` kopieren, [templates/integration/sichttest.cmake](../templates/integration/sichttest.cmake)
  daneben legen, `MyApp` durch das eigene Target ersetzen und nach `add_executable` mit
  `include()` einbinden.

In beiden Fällen bekommt das Target den Include-Pfad und das Define `SICHTTEST_VORHANDEN`, nichts
wird gelinkt, und `SichttestSteuerung1.dll` samt Ordner `sichttest\` (der Tester mit seinem eigenen
Qt) wird beim Bauen neben die Exe kopiert. Lässt sich das Paket nicht holen, warnt der Configure,
und MyApp baut ohne Steuerung. Den Zwischenspeicher (`.externals/` oder das eigene `CACHE_DIR`)
nicht versionieren.

Zur Entwicklung gegen einen lokalen Bau dieses Repos mit
`-DSICHTTEST_LOCAL_DIR=<Pfad zu out/package/sichttest-vX.Y.Z-win64>` konfigurieren; der Pin gilt
dann nicht.

## 3. Aktionen im Code anmelden

[templates/integration/SichttestIntegration.hpp](../templates/integration/SichttestIntegration.hpp)
und [.cpp](../templates/integration/SichttestIntegration.cpp) kopieren und anpassen. Die Vorlage ist
für Qt (`sichttest_steuerung_qt.hpp`); ohne Qt gibt es `sichttest_steuerung.hpp`, deren Klasse
`sichttest::Sitzung` die Argumente als JSON-Text übergibt.

Worauf es ankommt:

- **Eine `.cpp`, hinter `#ifdef SICHTTEST_VORHANDEN`.** Die Köpfe binden `<windows.h>` ein; aus
  eigenen Köpfen heraushalten. Ohne das Define übersetzt die Datei zu nichts.
- **Nur bei `--testing`.** `sichttest::Steuerung::lade("MyApp", version, &grund)` lädt die DLL aus
  dem Ordner der Exe; fehlt sie oder passt sie nicht, kommt nichts zurück — `grund` einmal zeigen
  und weiterlaufen.
- **Erst anmelden, dann verbinden.** `aktion(name, beschreibung, parameterHinweis, rueckruf,
  schalter)` je Aktion, danach `verbinde()`. `verbinde()` erst rufen, wenn MyApp gesteuert werden
  kann (nach dem Wiederherstellen des Startzustands): vorher läuft kein Rückruf, danach laufen alle
  im GUI-Thread.
- **Zustände melden**, die die Anwendung herstellt und abräumt: `meldeZustand("test_db", true,
  name)` beim Herstellen, `meldeZustand("test_db", false)` wenn er weg ist — auch für einen Zustand,
  der aus einer früheren Sitzung übrig ist. Der Tester bietet dann die passende Nachbereitung von
  selbst an.
- **Wahlweise:** `beiEnde([](bool geordnet) { … })` meldet, dass der Tester gegangen ist.
- **Pfade kommen unverändert an.** Ein Pfad-Argument ist genau das, was in der Liste steht; in
  MyApp auflösen (der Aufruf trägt dazu `basis`, den Root des Projekts, als Angebot) und ablehnen,
  was nicht geöffnet werden soll.

Eine Anwendung, die je Exe nur eine Instanz zulässt: startet der Tester MyApp selbst (`start` in
der Projektdatei), während MyApp schon ohne `--testing` läuft, endet der zweite Start sofort; der
Tester meldet dann „gestartet, aber nicht verbunden“.

## 4. Die Projektdatei

[templates/.sichttest/sichttest.projekt.json](../templates/.sichttest/sichttest.projekt.json) nach
`.sichttest/sichttest.projekt.json` im Root des Repos kopieren und versionieren. Alle Pfade gelten
ab dem Root des Repos.

- `anwendung`: der Name aus Schritt 1.
- `start`: die Exe und `["--testing"]`, damit der Tester MyApp selbst starten kann.
- `listen`: die Ordner mit den Listen, je mit ihrer `ablage` (für Testlogs, Reports, Screenshots
  und `testergebnisse.sqlite`). Die Ablagen in `.gitignore` aufnehmen, wenn Ergebnisse lokal
  bleiben sollen.
- `abbildung`: nur, wenn die JSON-Listen die Felder `setup.*`, `tab`, `restart`, `sql`, `test_db`,
  `test_db.ende` benutzen oder eigene Link-Schemata gewünscht sind (Schritt 5). Sonst weglassen.

## 5. Listen

**Markdown** ([templates/Sichttest_List.de.md](../templates/Sichttest_List.de.md)): eine
gewöhnliche Checkliste; eine Aktion ist ein Code-Stück `` `aktion: <name> [schlüssel=wert …]` `` im
Punkt, zu dem sie gehört. Ein Punkt mit dem Titel **Vorbereiten…** ist eine Vorbereitung ohne
Urteil; `` `aktion: @A0` `` übernimmt die Aktionen des Punkts `A0`.

**JSON** ([templates/List.testprotokoll.json](../templates/List.testprotokoll.json)):
`vorbereitung` und `nachbereitung` an der Wurzel, `aktionen` je Schritt, jede Aktion mit `aktion`,
`mit` (Argumente) und `text` (der Handgriff). Eine Nachbereitung mit `"wenn": "test_db"` wird nur
angeboten, solange MyApp diesen Zustand als stehend meldet.

Jeden Punkt so schreiben, dass ein Mensch ihn **ohne** die Aktion tun kann: der Text ist, was
erscheint, wenn MyApp nicht verbunden ist oder eine Aktion scheitert. Und den geprüften Handgriff
dem Menschen lassen — Aktionen stellen her, der Mensch prüft.

**Ein vorhandenes JSON-Format.** Hat MyApp schon Testlisten mit Feldern wie `setup.open`, `tab`,
`restart` oder `test_db`, bleiben sie: das Werkzeug liest diese Felder, und die `abbildung` der
Projektdatei sagt, welche Aktion der Anwendung jedes auslöst. Jeder Schlüssel der `abbildung` ist
zugleich ein Link im Text des Schritts — mit `"datei": "datei_oeffnen pfad"` öffnet der Text
`[Vorlage](datei:configs/a.json)` die Datei mit einem Klick in MyApp. Ein vollständiges Beispiel
liegt in [examples/project/](../examples/project/).

## 6. Wahlweise: Nachtest-Indikator

Nennt die Projektdatei `gewichtung`, zeigt der Tester an jedem offenen Schritt, wie dringend er
wieder zu prüfen ist.

- **Hand-Datei** ([templates/test-gewichtung.json](../templates/test-gewichtung.json)): Bereiche
  von MyApp mit `weight`, `changed` und `paths`; Schritte nennen ihre Bereiche im Feld `areas`
  (JSON-Listen). Wer den Code ändert, zieht `changed` nach; `retest_steps` erzwingt den Nachtest
  einzelner Schritte.
- **git-Datei:** schreibt der Hook aus dem Paketordner `werkzeuge/`; `install_hooks.ps1` einmal je
  Klon aufrufen (braucht Python). Sie ist rechnerlokal — nicht versionieren.

## 7. Ausprobieren

1. MyApp bauen. Neben `MyApp.exe` liegen `SichttestSteuerung1.dll` und der Ordner `sichttest\`;
   das Configure-Log nennt das Paket.
2. `MyApp.exe --testing` starten. Der Tester öffnet sich von selbst (oder verbindet sich mit einem
   laufenden) und zeigt die Listen des Projekts; seine Statuszeile nennt MyApp und ihre Version.
3. Eine Liste mit Aktionen öffnen: die Knöpfe ▶ sind aktiv. Unbekannte Aktionsnamen werden beim
   Laden der Liste gemeldet.
4. MyApp ohne `--testing` starten: nichts wird geladen, nichts öffnet sich.

| Anzeichen | Ursache |
|---|---|
| `--testing` meldet, die DLL fehle | das Paket wurde beim Configure nicht gefunden (siehe Warnung), oder die Exe wurde nach dem Wechsel des Pins nicht neu gebaut |
| der Tester meldet die Fassungen als unverträglich | der Pin von MyApp ist für den Tester zu alt oder zu neu; Pin heben oder Tester erneuern |
| „gestartet, aber nicht verbunden“ | MyApp läuft schon ohne `--testing`, oder `verbinde()` wird nie erreicht |
| „Die Projektdatei … liegt nicht in einem Ordner .sichttest.“ | die Projektdatei liegt nicht in einem Ordner namens `.sichttest` |
| alte DLL nach dem Wechsel der Version | die Exe neu bauen; wurde unter derselben Version neu geschnürt, `.externals/sichttest/<version>/` löschen |
| eine Aktion tut nichts, am Punkt steht „von Hand:“ | MyApp hat `fehler`, `unbekannt` oder `ungueltig` geantwortet; der Grund steht rot am Punkt und im Testlog unter `actions` |

## Aktualisieren

Version und Prüfsumme in `sichttest.pin` ändern, konfigurieren, bauen. Die Schnittstelle ist
versioniert (§5 der Spezifikation): eine Anwendung, die gegen ältere Köpfe übersetzt ist, läuft mit
einer neueren DLL derselben großen Fassung weiter, und der Tester spricht die aktuelle und die
vorige Fassung des Protokolls. [CHANGELOG.de.md](../CHANGELOG.de.md) sagt, was je Version dazukam.
