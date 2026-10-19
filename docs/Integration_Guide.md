**English** · [Deutsch](Integration_Guide.de.md)

# Integrating an application

This guide shows how an application becomes drivable by Sichttest: which actions it offers, how it
obtains the package, what goes into its source code, its project file and its lists. Every rule
referred to here is defined in the [Specification](Specification.md); the files to copy are in
[templates/](../templates/). The examples use an application called **MyApp**.

Without any of this, Sichttest already works as a checklist tool next to your application — the
integration only adds that list items can trigger actions in it.

## Overview

| Step | What | Result |
|---|---|---|
| 1 | choose a name and the actions | a short list of actions with flags |
| 2 | consume the package | headers at build time; DLL and tester next to the executable |
| 3 | register the actions in the code | `MyApp.exe --testing` connects to the tester |
| 4 | write the project file | the tester finds lists, storage and start |
| 5 | write lists | items trigger actions; the text remains the fallback |
| 6 | optional: retest indicator | 🔴/🟡 at steps whose code changed |
| 7 | try it | the checks at the end of this guide |

## 1. Name and actions

**The name** (`anwendung`) is how MyApp reports itself to the tester. Lists and the project file
use the same name. Choose it once; it appears in test logs.

**Actions** are what a list may trigger: load a template, bring a tab to the front, set up a test
database. The names are yours — `[a-z0-9_.]`, for example `open_file`, `show_tab`, `edit`. The tool
knows none of them; it learns them on every connect.

Good candidates are the steps a tester would otherwise do by hand **before** the actual check
(setting up a state), not the thing being checked: if the check is “the Edit button switches
editing on”, the list should not press it through an action. For each action decide:

| Question | If yes |
|---|---|
| Can it destroy something the human would miss (close tabs, discard unsaved work) — without asking itself? | flag `STS_FRAGT_NACH`: the tester asks before |
| Does it clean up instead of set up, so it belongs only at the start of a fresh run? | flag `STS_NUR_ERSTLAUF`: skipped when a run is resumed |
| Does it open a dialog in MyApp, so it takes as long as the human needs? | flag `STS_WARTET_AUF_MENSCH`: no time limit. Never together with `STS_FRAGT_NACH` |
| Does MyApp exit or restart after answering? | flag `STS_BEENDET_ANWENDUNG`: the tester waits for the new connection |

Two rules make resumed runs work:

- **Preparation actions are repeatable.** When a run is resumed, the tester prepares again
  (skipping `STS_NUR_ERSTLAUF`). An action that opens a file answers `ok` if the file is already
  open.
- **Never answer `ok` without the result.** An action that sets up a state answers `fehler` if the
  human declines or anything fails, and reports the state (step 3). The tester relies on it.

Answers are `ok`, `fehler` (failed, with a text that says why) or `ungueltig` (arguments do not
fit). The text of the answer is shown to the tester.

## 2. Consume the package

Copy [templates/sichttest.pin](../templates/sichttest.pin) to the root of your repository and set
version and checksum (from the `.zip.sha256` of the release). Then one of:

- **With CMakeCraft** (v0.10.0 or later): merge
  [templates/integration/Solution.external.json](../templates/integration/Solution.external.json)
  into `Solution.json` and name `"sichttest"` in the `externals` of the executable that registers
  the actions.
- **Without CMakeCraft:** copy `CMakeCraftPackage.cmake` unchanged from the CMakeCraft repository
  to `cmake/`, copy [templates/integration/sichttest.cmake](../templates/integration/sichttest.cmake)
  next to it, replace `MyApp` with your target, and `include()` it after `add_executable`.

Either way the target gets the include path and the define `SICHTTEST_VORHANDEN`, nothing is
linked, and `SichttestSteuerung1.dll` plus the folder `sichttest\` (the tester with its own Qt) are
copied next to the executable when it is built. If the package cannot be fetched, configure warns
and MyApp builds without control. Keep the cache folder (`.externals/` or your `CACHE_DIR`) out of
Git.

For development against a local build of this repository, configure with
`-DSICHTTEST_LOCAL_DIR=<path to out/package/sichttest-vX.Y.Z-win64>`; the pin is then ignored.

## 3. Register the actions in the code

Copy [templates/integration/SichttestIntegration.hpp](../templates/integration/SichttestIntegration.hpp)
and [.cpp](../templates/integration/SichttestIntegration.cpp) and adapt them. The template is for
Qt (`sichttest_steuerung_qt.hpp`); without Qt use `sichttest_steuerung.hpp`, whose class
`sichttest::Sitzung` hands the arguments over as JSON text.

The points that matter:

- **One `.cpp`, behind `#ifdef SICHTTEST_VORHANDEN`.** The headers include `<windows.h>`; keep them
  out of your own headers. Without the define the file compiles to nothing.
- **Only with `--testing`.** `sichttest::Steuerung::lade("MyApp", version, &reason)` loads the DLL
  from the folder of the executable; it returns empty if the DLL is missing or incompatible —
  show `reason` once and carry on.
- **Register, then connect.** `aktion(name, description, parameterHint, callback, flags)` for each
  action, then `verbinde()`. Call `verbinde()` only once MyApp can be driven (after restoring its
  start state): no callback runs before, all callbacks run in the GUI thread afterwards.
- **Report states** you set up and tear down: `meldeZustand("test_db", true, name)` when set up,
  `meldeZustand("test_db", false)` when gone — also for a state left over from an earlier session.
  The tester then offers the matching teardown on its own.
- **Optional:** `beiEnde([](bool geordnet) { … })` tells you the tester has gone.
- **Paths arrive unchanged.** A path argument is exactly what the list says; resolve it in MyApp
  (the call also carries `basis`, the project root, as an offer) and reject what you do not want to
  open.

An application that allows only one instance per executable: when the tester starts MyApp itself
(`start` in the project file) while MyApp is already running without `--testing`, the second start
ends at once; the tester then reports “gestartet, aber nicht verbunden” (started but not
connected).

## 4. The project file

Copy [templates/.sichttest/sichttest.projekt.json](../templates/.sichttest/sichttest.projekt.json)
to `.sichttest/sichttest.projekt.json` in the root of your repository and commit it. All paths are
relative to the repository root.

- `anwendung`: the name from step 1.
- `start`: the executable and `["--testing"]`, so that the tester can start MyApp itself.
- `listen`: the folders with your lists, each with its `ablage` (storage for test logs, reports,
  screenshots and `testergebnisse.sqlite`). Add the storage folders to `.gitignore` if results
  should stay local.
- `abbildung`: only if your JSON lists use the fields `setup.*`, `tab`, `restart`, `sql`, `test_db`,
  `test_db.ende`, or if you want link schemes of your own (step 5). Remove it otherwise.

## 5. Lists

**Markdown** ([templates/Sichttest_List.md](../templates/Sichttest_List.md)): an ordinary
checklist; an action is a code span `` `aktion: <name> [key=value …]` `` in the item it belongs to.
An item titled **Vorbereiten…** is a preparation without verdict; `` `aktion: @A0` `` reuses the
actions of item `A0`. The keywords `aktion:`, `Vorbereiten` and `**Anwendung:**` are fixed German
words.

**JSON** ([templates/List.testprotokoll.json](../templates/List.testprotokoll.json)): `vorbereitung`
and `nachbereitung` at the root, `aktionen` per step, each action with `aktion`, `mit` (arguments)
and `text` (the manual step). A teardown with `"wenn": "test_db"` is offered only while MyApp reports
that state as standing.

Write every item so that a human can do it **without** the action: the text is what appears when
MyApp is not connected or an action fails. And leave the step being checked to the human — actions
set up, the human checks.

**An existing JSON format.** If MyApp already has test lists with fields like `setup.open`, `tab`,
`restart` or `test_db`, keep them: the tool reads these fields, and the `abbildung` of the project
file says which of your actions each one triggers. Every key of the `abbildung` is also a link in
the step text — with `"file": "open_file path"`, the text `[Template](file:configs/a.json)` opens
the file in MyApp with one click. A complete example is
[examples/project/](../examples/project/).

## 6. Optional: retest indicator

Name `gewichtung` in the project file to have the tester show at each open step how urgently it
should be tested again.

- **Hand file** ([templates/test-gewichtung.json](../templates/test-gewichtung.json)): areas of
  MyApp with `weight`, `changed` and `paths`; steps name their areas in the field `areas` (JSON
  lists). Whoever changes the code updates `changed`; `retest_steps` forces a retest of single
  steps.
- **Git file:** written by the hook from the package folder `werkzeuge/`; run `install_hooks.ps1`
  once per clone (needs Python). It is local to the machine — keep it out of Git.

## 7. Try it

1. Build MyApp. Next to `MyApp.exe` there should be `SichttestSteuerung1.dll` and the folder
   `sichttest\`; the configure log names the Sichttest package.
2. Start `MyApp.exe --testing`. The tester opens by itself (or connects to one already running)
   and shows the lists of your project; its status line names MyApp and its version.
3. Open a list with actions: the ▶ buttons are enabled. Unknown action names are reported when the
   list is loaded.
4. Start MyApp without `--testing`: nothing is loaded, nothing opens.

| Symptom | Cause |
|---|---|
| `--testing` says the DLL is missing | the package was not found at configure time (see the warning), or the executable was not rebuilt after changing the pin |
| the tester reports the versions as incompatible | MyApp's pin is too old or too new for the tester; raise the pin, or update the tester |
| “gestartet, aber nicht verbunden” | MyApp is already running without `--testing`, or `verbinde()` is never reached |
| “Die Projektdatei … liegt nicht in einem Ordner .sichttest.” | the project file is not in a folder named `.sichttest` |
| old DLL after changing the version | build the executable again; if a package was rebuilt under the same version, delete `.externals/sichttest/<version>/` |
| an action does nothing, the item shows “von Hand:” | MyApp answered `fehler`, `unbekannt` or `ungueltig`; the reason is in red at the item and in the test log under `actions` |

## Updating

Change version and checksum in `sichttest.pin`, configure, build. The interface is versioned
(§5 of the specification): an application compiled against older headers keeps working with a
newer DLL of the same major version, and the tester speaks the current and the previous protocol
version. [CHANGELOG.md](../CHANGELOG.md) says what each version added.
