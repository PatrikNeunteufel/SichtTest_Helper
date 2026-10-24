**English** · [Deutsch](CHANGELOG.de.md)

# Changelog

Each version is a Git tag `vX.Y.Z`; from v0.2.0 on, a release on GitHub carries the package
`sichttest-vX.Y.Z-win64.zip` (headers, DLL, tester). Interface **S** and protocol **P** have their
own numbers ([Specification §5](docs/Specification.md#5-three-numbers-two-of-them-checked)).

## Unreleased

- Documentation in English by default, German next to it (`.de.md`). New: the
  [Specification](docs/Specification.md) (replaces `docs/Konzept_Steuerung.md`), the
  [Integration Guide](docs/Integration_Guide.md), [templates/](templates/) for applications, an
  example project in [examples/project/](examples/project/), [Releasing](docs/Releasing.md)
  (replaces `docs/GitHub_Einrichtung.md`). `beispiele/` is now `examples/`.
- Build: CMakeCraft v0.13.1 (MSVC compiles in parallel with `/MP8` and with `/bigobj`; the configuration `Testing` is optimized with asserts and debug info). `CMakeLists.txt` now calls `project(SichtTest_Helper LANGUAGES NONE)`, required since v0.13.0.

## v0.4.0 — 2026-10-11 · S 1.1 · P 1.1

- **States of the application:** `sts_melde_zustand` / `meldeZustand` reports a named state (for
  example a test database); a teardown with `"wenn": "<state>"` is offered while it stands, also
  right after connecting. `sts_bei_ende` / `beiEnde` tells the application that the tester has
  gone. The test log records `zustaende`. Applications built with S 1.0 headers keep working.
- `{fortgesetzt}` as a value in the arguments of any action.
- **Build at the verdict:** every rated step carries the executable and its timestamp; orphaned
  verdicts (steps no longer in the list) stay in the test log.
- **Result database** `testergebnisse.sqlite` per storage; window **Statistik…**; older runs open
  read only and can be continued or edited; **Ältere Testlogs einlesen** imports test logs the
  database does not know.
- **Retest indicator** (🔴/🟡) from `gewichtung` in the project file, with hand and git layer,
  **↷ Unkritische überspringen** and a git hook in the package (`werkzeuge/`).
- **Finding archive:** `history[]` per step, filed by button and by itself; completed runs can be
  edited.
- **Links:** every key of the `abbildung` is a link in the step text; the tab of a step is a link
  below its title.
- `rated_at` (time of the verdict): the indicator compares against the day of the verdict; the last
  result of a step is its most recent verdict.
- Window: splitter between step text and remark; list selection and step list show the title
  first.
- One report per run; `summary.open` never counts preparations.

## v0.3.6 — 2026-10-10

- Closing after a preparation alone (nothing rated yet) offers the teardown.

## v0.3.5 — 2026-10-10

- `test_db.active` in the test log; predefined key `fortgesetzt` in the `abbildung`.
- Before every call the tester allows the application to come to the front.

## v0.3.4 — 2026-10-10

- A completed run is no longer resumed: opening its list starts a new run.
- `build` is kept when a run is resumed without a connection.

## v0.3.3 — 2026-10-09

- The test log names the connected application and the DLL (block `steuerung`).

## v0.3.2 — 2026-10-09

- The question before `STS_FRAGT_NACH` actions has the checkbox “In dieser Sitzung nicht mehr
  fragen”.
- After the application has exited, the tester offers **Tester beenden**.

## v0.3.1 — 2026-10-09

- The headers report the version of the loaded DLL (`fassung()`).

## v0.3.0 — 2026-10-09

- The project file lies in `.sichttest/sichttest.projekt.json`; its paths are relative to the
  project root.

## v0.2.0 — 2026-10-09 · S 1.0 · P 1

- **Control of the application under test:** DLL `SichttestSteuerung1.dll` with the C interface
  S 1.0, headers for C++ and Qt; the tester listens on a named pipe (P 1).
- Actions in lists (Markdown `aktion:`, JSON `vorbereitung` / `aktionen` / `nachbereitung`), buttons
  ▶, the manual step as fallback.
- Project file; fields of the JSON format that become actions through the `abbildung`.
- First package and release, built with CMakeCraft v0.10.0.

## v0.1.0 — 2026-10-08

- The tool as a project of its own: checking off Markdown checklists and `*.testprotokoll.json`,
  writing test log, report and screenshots.
