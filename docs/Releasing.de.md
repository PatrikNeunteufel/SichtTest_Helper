[English](Releasing.md) · **Deutsch**

# Eine Version veröffentlichen

Für den Betreuer dieses Repos. Repo: `https://github.com/PatrikNeunteufel/SichtTest_Helper`
(öffentlich; die Anwendungen laden das Release-Archiv ohne Anmeldung).

## Je Version

1. **Version heben:** `solution.version` in `Solution.json`; S und P in `packaging/VERSION.in` und
   in `sichttest_steuerung.h` nur, wenn sich Schnittstelle oder Protokoll geändert haben
   ([Spezifikation §5](Specification.de.md#5-drei-nummern-zwei-davon-werden-geprüft)). Die Version
   in `CHANGELOG.md` und `CHANGELOG.de.md` eintragen.
2. **Bauen und prüfen:**

   ```bash
   cmake --preset windows-ninja-release-clang
   ```

   ```bash
   cmake --build --preset build-ninja-release-clang
   ```

   Danach `Sichttest.exe --selbsttest <leerer Ordner>`, Exit-Code 0. Der Selbsttest braucht alle
   Targets, deshalb ohne `--target`.
3. **Committen und taggen** — das Paket muss einem Commit entsprechen, deshalb erst danach
   schnüren: Commit, `git tag -a vX.Y.Z -m "…"`.
4. **Paket schnüren:**

   ```bash
   cmake --build --preset build-ninja-release-clang --target package_sichttest
   ```

   Ergebnis in `out/package/`: `sichttest-vX.Y.Z-win64/`, `sichttest-vX.Y.Z-win64.zip` und
   `sichttest-vX.Y.Z-win64.zip.sha256`. **Danach nicht noch einmal schnüren:** die Prüfsumme
   ändert sich mit jedem Lauf, auch bei gleichem Inhalt. Hochgeladen wird genau diese Datei, und
   ihre Prüfsumme gehört in die Pins.
5. **Hochladen:** `git push origin master`, `git push origin vX.Y.Z`.
6. **Release anlegen:** auf GitHub **Releases → Draft a new release**, den Tag `vX.Y.Z` wählen,
   die `.zip` anhängen (der Dateiname muss unverändert bleiben), **Publish release**. Die Adresse
   lautet dann
   `https://github.com/PatrikNeunteufel/SichtTest_Helper/releases/download/vX.Y.Z/sichttest-vX.Y.Z-win64.zip`.
7. **Hochgeladenes prüfen:** der `digest` der Datei in der GitHub-API
   (`https://api.github.com/repos/PatrikNeunteufel/SichtTest_Helper/releases/tags/vX.Y.Z`) gleicht
   dem Inhalt der `.zip.sha256`, und die Download-Adresse antwortet mit 200.
8. **Pins:** jede Anwendung, die die neue Version will, ändert `SICHTTEST_VERSION` und
   `SICHTTEST_SHA256` in ihrem `sichttest.pin`. Solange eine Anwendung ihren Pin nicht ändert,
   bleibt sie beim alten Paket.

**Ein veröffentlichtes Paket wird nie ersetzt.** Wer die `.zip` eines Tags austauscht, macht die
Prüfsumme in allen Pins falsch. Fehler behebt eine neue Version.

**Release-Texte** beschreiben das Werkzeug selbst — was die Version für den bringt, der Sichttest
benutzt — und sagen „Header“ für die drei Kopfdateien.

## Was nicht ins Repo gehört

`out/`, `.externals/`, `.vs/`, `CMakeUserPresets.json`, `CLAUDE.md`, `.claude/`, `sichttest-logs/`
— die `.gitignore` regelt das. Jede Commit-Adresse ist öffentlich sichtbar.

## Was dabei schiefgehen kann

| Zeichen | Ursache | Abhilfe |
|---|---|---|
| Push scheitert mit 403 | falsches Konto in der Anmeldeinformationsverwaltung | Eintrag `git:https://github.com` löschen, erneut pushen |
| Bezug meldet falsche Prüfsumme | `.zip` nach dem Eintragen ins Pin neu gebaut oder ersetzt | neue Version veröffentlichen, Pin darauf setzen |
| Bezug findet das Archiv nicht (404) | Release nur als Entwurf gespeichert, oder Dateiname geändert | Release veröffentlichen; Namen mit dem Pin vergleichen |
