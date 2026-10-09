# GitHub einrichten und Versionen veröffentlichen

> **Stand:** 2026-10-08 · Entscheid Patrik E7: das Repo ist **öffentlich**, unter dem privaten
> Konto wie LumiViz. Alle Git-Befehle führt Patrik aus. Die GitHub-CLI (`gh`) ist auf diesem
> Rechner nicht installiert; die Anleitung nimmt deshalb die Webseite.

Adresse nach dem Einrichten: `https://github.com/PatrikNeunteufel/SichtTest_Helper`
(Kontoname aus dem Remote von LumiViz gelesen; der Repo-Name ist ein Vorschlag — er steht später
in jedem `sichttest.pin`, also vor Schritt 3 festlegen).

## Teil A — einmalig

### 1. Vor dem ersten Commit prüfen (das Repo wird öffentlich)

- **Was hineinkommt:** `git status` nach `git add .` zeigt es. Draußen bleiben müssen `out/`,
  `.externals/`, `.vs/`, `CMakeUserPresets.json`, `.claude/`, `sichttest-logs/` — die
  `.gitignore` regelt das schon.
- **`CLAUDE.md` und `.claude/`** bleiben lokal (Entscheid Patrik E8); beide stehen in der
  `.gitignore`. In der Liste von `git status` dürfen sie nicht auftauchen.
- **Lizenz:** dieselbe Doppellizenz wie LumiViz (Entscheid Patrik E9). `LICENSE-MIT` und
  `LICENSE-APACHE` sind unverändert von dort übernommen, `LICENSE` erklärt die Wahl und nennt
  Qt als Fremdkomponente des Pakets.
- **Adresse in den Commits:** jede Commit-Adresse ist öffentlich sichtbar. LumiViz benutzt
  `patrik.neunteufel@gmail.com`. Wer das nicht will, nimmt die Adresse
  `<id>+PatrikNeunteufel@users.noreply.github.com` (GitHub → Settings → Emails).

### 2. Lokales Repo anlegen

> **Stand 2026-10-09:** dieser Schritt ist erledigt (Commit `b9e348d`, Tag `v0.1.0`, lokale
> Identität gesetzt). Offen ist alles ab Schritt 3.

```bash
git -C "C:/Users/patri/source/repos/Visuals_Project/cmake/SichtTest_Helper" init -b master
```

```bash
git -C "C:/Users/patri/source/repos/Visuals_Project/cmake/SichtTest_Helper" config --local user.name "Patrik Neunteufel"
```

```bash
git -C "C:/Users/patri/source/repos/Visuals_Project/cmake/SichtTest_Helper" config --local user.email "patrik.neunteufel@gmail.com"
```

```bash
git -C "C:/Users/patri/source/repos/Visuals_Project/cmake/SichtTest_Helper" add .
```

```bash
git -C "C:/Users/patri/source/repos/Visuals_Project/cmake/SichtTest_Helper" status
```

Stimmt die Liste, die Commit-Message (unten) in eine Datei außerhalb des Repos legen, etwa
`%TEMP%\sichttest_commit.txt`, und:

```bash
git -C "C:/Users/patri/source/repos/Visuals_Project/cmake/SichtTest_Helper" commit -F "%TEMP%\sichttest_commit.txt"
```

```bash
git -C "C:/Users/patri/source/repos/Visuals_Project/cmake/SichtTest_Helper" tag -a v0.1.0 -m "Sichttest 0.1.0: eigenstaendiges Werkzeug, ohne Steuerung"
```

Commit-Message:

```
Sichttest 0.1.0: eigenes Projekt aus der LumiViz-Kopie vom 2026-10-03

- Werkzeug Sichttest.exe (Qt6, CMakeCraft v0.9.2): Markdown-Checklisten und
  *.testprotokoll.json abhaken, Testlog, Report und Screenshots schreiben
- README nachgezogen, Anleitung docs/GitHub_Einrichtung.md
- Doppellizenz MIT / Apache-2.0 wie LumiViz
- docs/Idee_Steuerung_der_Anwendung.md (aus LumiViz S103) und
  docs/Konzept_Steuerung.md: DLL als Gegenstelle zur Steuerung der
  geprueften Anwendung, abgestimmt mit LumiViz, Comm Studio und CMakeCraft,
  noch nichts gebaut
```

### 3. Repo auf GitHub anlegen

1. Im Browser unter dem **privaten** Konto anmelden, dann <https://github.com/new>.
2. **Repository name:** `SichtTest_Helper` · **Public**.
3. **Nichts** vorbelegen lassen: kein README, keine `.gitignore`, keine Lizenz — sonst hat
   GitHub einen ersten Commit, der mit dem lokalen kollidiert.
4. **Create repository**.

### 4. Verbinden und hochladen

```bash
git -C "C:/Users/patri/source/repos/Visuals_Project/cmake/SichtTest_Helper" remote add origin https://github.com/PatrikNeunteufel/SichtTest_Helper.git
```

```bash
git -C "C:/Users/patri/source/repos/Visuals_Project/cmake/SichtTest_Helper" push -u origin master
```

```bash
git -C "C:/Users/patri/source/repos/Visuals_Project/cmake/SichtTest_Helper" push origin v0.1.0
```

Beim ersten `push` fragt Git nach der Anmeldung (Browser-Fenster des Git Credential Manager).
**Auf das Konto achten:** ist dort ein anderes Konto gespeichert, schlägt der Push mit 403 fehl;
dann unter Windows in der Anmeldeinformationsverwaltung den Eintrag `git:https://github.com`
prüfen.

### 5. Prüfen

- Die Seite des Repos zeigt die Dateien und unter **Tags** `v0.1.0`.
- Unter **Commits** steht die richtige Adresse.
- Abgemeldet (privates Browserfenster) ist die Seite erreichbar — nur dann lädt der Bezug in
  LumiViz und im Comm Studio ohne Anmeldung.

`v0.1.0` bekommt **kein** Release-Paket: es ist der Stand ohne Steuerung, und Pakete schnüren
kann erst CMakeCraft v0.10.0.

## Teil B — je Version, sobald die DLL steht (ab v0.2.0)

Voraussetzung: CMakeCraft v0.10.0 (`packages` in `Solution.json`, Konzept §10).

1. **Version heben:** `solution.version` in `Solution.json`; S und P in `packaging/VERSION.in`
   nur, wenn sich Schnittstelle oder Protokoll geändert haben (Konzept §5).
2. **Bauen und prüfen:**

   ```bash
   cmake --preset windows-ninja-release-clang
   ```

   ```bash
   cmake --build --preset build-ninja-release-clang
   ```

   Danach `Sichttest.exe --selbsttest <leerer Ordner>`, Exit-Code 0. Der Selbsttest braucht alle
   Targets, deshalb ohne `--target`.
3. **Paket schnüren:**

   ```bash
   cmake --build --preset build-ninja-release-clang --target package_sichttest
   ```

   Ergebnis in `out/package/`: `sichttest-vX.Y.Z-win64/`, `sichttest-vX.Y.Z-win64.zip` und
   `sichttest-vX.Y.Z-win64.zip.sha256`. **Danach nicht noch einmal schnüren:** die Prüfsumme
   ändert sich mit jedem Lauf, auch bei gleichem Inhalt. Hochgeladen wird genau diese Datei, und
   ihre Prüfsumme gehört in die Pins.
4. **Committen, taggen, hochladen:** Commit, `git tag -a vX.Y.Z -m "…"`, `git push origin master`,
   `git push origin vX.Y.Z`.
5. **Release anlegen:** auf GitHub **Releases → Draft a new release**, den Tag `vX.Y.Z` wählen,
   die `.zip` anhängen (der Dateiname muss unverändert bleiben), **Publish release**.
   Die Adresse lautet dann
   `https://github.com/PatrikNeunteufel/SichtTest_Helper/releases/download/vX.Y.Z/sichttest-vX.Y.Z-win64.zip`.
6. **Pins nachziehen** — in jeder Anwendung, die die neue Version will, `sichttest.pin`:
   `SICHTTEST_VERSION` und `SICHTTEST_SHA256` (der Inhalt der `.sha256`-Datei). Solange eine
   Anwendung ihren Pin nicht ändert, bleibt sie beim alten Paket.
7. **Prüfen:** in der Anwendung Configure ohne `SICHTTEST_LOCAL_DIR`; das Paket wird geholt, die
   Prüfsumme stimmt, DLL und `sichttest\` liegen neben der Exe.

**Ein veröffentlichtes Paket wird nie ersetzt.** Wer die `.zip` eines Tags austauscht, macht die
Prüfsumme in allen Pins falsch. Fehler behebt eine neue Version.

## Was dabei schiefgehen kann

| Zeichen | Ursache | Abhilfe |
|---|---|---|
| Push scheitert mit 403 | falsches Konto in der Anmeldeinformationsverwaltung | Eintrag `git:https://github.com` löschen, erneut pushen |
| Push abgelehnt, „fetch first" | Repo wurde auf GitHub mit README oder Lizenz angelegt | Repo löschen und leer neu anlegen (solange noch nichts daran hängt) |
| Bezug meldet falsche Prüfsumme | `.zip` nach dem Eintragen ins Pin neu gebaut oder ersetzt | neue Version veröffentlichen, Pin darauf setzen |
| Bezug findet das Archiv nicht (404) | Release nur als Entwurf gespeichert, oder Dateiname geändert | Release veröffentlichen; Namen mit dem Pin vergleichen |
