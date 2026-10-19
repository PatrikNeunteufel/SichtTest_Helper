**English** · [Deutsch](README.de.md)

# SichtTest_Helper

**Sichttest** is a standalone program for running visual (manual) tests. It shows a checklist
item by item and records a verdict for each item — **Pass**, **Pass with finding**, **Fail** or
**Skip** — together with a remark and screenshots. The result is a test log and a report,
readable by a human and by Claude. The program runs **next to** the application under test and
needs nothing from it.

**In addition**, Sichttest can drive the application under test: load a template, bring a tab or
panel to the front, set up a state. For this, the application loads a small DLL shipped by this
project and registers its actions; the lists then name, per item, what to trigger. Without the
DLL the tool works as before, and even with it every manual step remains as text — the verdict is
always given by the human.

- **Integrating an application:** [docs/Integration_Guide.md](docs/Integration_Guide.md), templates
  to copy in [templates/](templates/)
- **Binding description** of interface, protocol, list format and project file:
  [docs/Specification.md](docs/Specification.md)
- **What each version added:** [CHANGELOG.md](CHANGELOG.md)

The user interface, the list keywords and the identifiers of the interface are German (for
example `aktion:` in a list, the button **▶ Herstellen**). This documentation quotes them as they
are and gives the meaning on first use; the
[glossary](docs/Specification.md#glossary) collects them.

**Built with [CMakeCraft](https://github.com/PatrikNeunteufel/CMakeCraft):** the build system is
not part of the repository; configure fetches the version named in `cmakecraft.pin`. What is
built and packaged is defined solely in `Solution.json`. Applications that build with CMakeCraft
themselves (v0.10.0 or later) consume the finished package from here as an external of kind
`archive`; applications without CMakeCraft use the standalone file `CMakeCraftPackage.cmake`.

## Layout

```
SichtTest_Helper/
├── CMakeLists.txt              entry point, fetches CMakeCraft
├── CMakeCraftBootstrap.cmake   obtains the build system
├── cmakecraft.pin              pinned CMakeCraft version
├── CMakePresets.json           presets
├── CMakeUserPresets.json       local: QT_ROOT (not versioned)
├── Solution.json               what is built and packaged, version
├── CHANGELOG.md                what each version added
├── LICENSE, LICENSE-MIT, LICENSE-APACHE
├── docs/                       specification, integration guide, releasing
├── examples/                   lists in Markdown and JSON, one with actions, a project file
├── templates/                  templates for an application: project file, lists, pin, integration
├── tools/                      git hook of the retest indicator (in the package under werkzeuge/)
├── packaging/VERSION.in        template of the file VERSION in the package
├── projects/exec/Sichttest/src/        the tool
│   ├── main.cpp                window, command line, self-test
│   ├── Protokoll.hpp/.cpp      reading lists, writing test log and report (no widgets)
│   ├── Projekt.hpp/.cpp        the project file sichttest.projekt.json
│   ├── Steuerung.hpp/.cpp      accepting applications, calling actions
│   └── Kanal.hpp/.cpp          the pipe the tool listens on
├── projects/libs/SichttestSteuerung/   the DLL for the application under test (no Qt)
│   ├── include/                the three headers: C, C++, Qt
│   └── src/                    channel, protocol, interface
└── projects/exec/Gegenprobe/, GegenprobeQt/   small applications for the self-test
```

Every document exists in English (default) and German (`.de.md` next to it).

Local and not versioned: `out/`, `.externals/` (fetched CMakeCraft), `CMakeUserPresets.json`,
`CLAUDE.md`, `.claude/`, `sichttest-logs/`.

## Building

```bash
cmake --preset windows-ninja-release-clang
```

```bash
cmake --build --preset build-ninja-release-clang
```

This builds all targets: the tool, the DLL `SichttestSteuerung1.dll` and the two counter-check
applications (`Gegenprobe`) the self-test needs.

Executable: `out/build/windows-ninja-release-clang/exec/Sichttest/bin/Release/Sichttest.exe`

Requirements: CMake 3.26 or later; `QT_ROOT` points to a Qt 6 kit (locally in
`CMakeUserPresets.json`, for example `C:/Qt/6.10.1/msvc2022_64`). Configure fetches CMakeCraft in
the version from `cmakecraft.pin` into `.externals/`.

Check: `Sichttest.exe --selbsttest <empty folder>` exits with code 0.

The package for applications (headers, DLL, tool with its Qt) is built with

```bash
cmake --build --preset build-ninja-release-clang --target package_sichttest
```

into `out/package/`. How a release is published: [docs/Releasing.md](docs/Releasing.md).

## Command line

```bash
Sichttest [file|folder|projectfile]
Sichttest --pruefe <file>
Sichttest --schnapp <png> [file|folder]
Sichttest --selbsttest <empty folder>
Sichttest --steuerung
```

- Without an argument: the folder and list used last; on the very first start the folder
  `.claude/handover`, searched upwards from the working directory or from the executable.
- If the folder or one above it contains a folder `.sichttest` with a `sichttest.projekt.json`,
  its lists and storage apply (see “Project file”).
- A project file named explicitly must also lie in a folder `.sichttest`; its name is free but
  must end in `.projekt.json`.
- `--steuerung` (“control”): this is how the DLL of the application under test starts the tool
  when none is listening. The tool then opens the lists of that application's project.
- `--pruefe` (“check”) prints the recognised steps to standard output.
- `--schnapp` (“snapshot”) builds the window, saves it as an image and exits.
- `--selbsttest` (“self-test”) creates a small list, rates it through the buttons and checks log
  and report; the exit code is the number of failed checks.

## What it reads

**Markdown checklists** — every `*.md` in the folder that contains items
(example: [examples/Example.md](examples/Example.md)):

```markdown
## A. Section

- [ ] **A1 Title of the item:** What to do and what must be visible,
      over several indented lines.
```

The beginning of the bold text is the identifier (`A1`) if it looks like one; otherwise the tool
numbers the items (`P7`). Checkmarks in the file are the starting state: `[x]` Pass, `[!]` Fail,
`[-]` skipped. An executable in backticks in the preamble is recognised. **The list itself is
never modified.**

**`*.testprotokoll.json`** — the same list as JSON: `title`, `description` and `steps`, each step
with `id`, `section`, `title`, `text`
(example: [examples/Example.testprotokoll.json](examples/Example.testprotokoll.json)).

### Actions

An item can carry actions that the tool triggers in the application under test (example:
[examples/Example_Actions.md](examples/Example_Actions.md), description:
[Specification, “Lists”](docs/Specification.md#6-lists)). The application must run with
`--testing` and have registered its actions; without a connection the list behaves like one
without actions.

```markdown
**Anwendung:** MyApp

- [ ] **A0 Vorbereiten:** Load the template, then switch editing on.
      `aktion: vorlage_laden datei="presets\view A.tpl"` `aktion: bearbeiten an`
- [ ] **A1 The marker sits at its time:** Set the marker to 0:20. `aktion: marke zeit=0:20`
- [ ] **A2 Like A0, but without editing:** … `aktion: @A0` `aktion: bearbeiten aus`
```

- Form: `` `aktion: <name> [value] [key=value …]` `` (`aktion` = action); values containing spaces
  go in `"…"`. A value without a key arrives as argument `wert` (“value”). The window does not
  show these snippets in the text.
- An item whose title starts with **Vorbereiten** (“prepare”) is a preparation: no verdict, but
  **▶ Ausführen** (run) and **Weiter →** (next); it is not counted. Before the first section it
  applies to the whole list.
- The actions of an ordinary item run only on **▶ Herstellen** (“establish”).
  **↺ Vorbereitung** repeats the preparation of the section.
- `` `aktion: @A0` `` runs the actions of item `A0` at this point.
- Before actions that may discard unsaved work, the tool asks. The checkbox
  **In dieser Sitzung nicht mehr fragen** (“don't ask again in this session”) turns this off until
  the tool is closed.
- If the application started the tool and then exits, the tool offers **Tester beenden** (“close
  tester”) below the status line. It never closes by itself: the application may just be
  restarting.
- If an action fails or the application does not know it, the message appears in red at the item,
  with the manual step as text below it. The run never stops; the verdict stays with the human.
- In JSON: `"anwendung"` (application), `"vorbereitung": [ … ]` (preparation) at the root, per step
  `"aktionen": [ { "aktion": …, "mit": { … }, "text": …, "frist": seconds } ]`, `"kind": "prep"` for
  a preparation, `"nachbereitung": [ … ]` (teardown) at the root for the end of the run.

### Project file

The file `.sichttest/sichttest.projekt.json` in the root of the repository of the application
under test tells the tool what it needs to know about the project (description:
[Specification, “The project file”](docs/Specification.md#7-the-project-file), template:
[templates/.sichttest/sichttest.projekt.json](templates/.sichttest/sichttest.projekt.json)).
Paths are relative to the root, the folder above `.sichttest`; unknown keys are ignored. A project
file that does not lie in a folder `.sichttest` is not loaded, and the tool says so.

```json
{
  "schema": 1,
  "anwendung": "MyApp",
  "start": { "exe": "out/build/MyApp.exe", "argumente": ["--testing"] },
  "listen": [
    { "ordner": "tests/sichttest", "ablage": "tests/sichttest-logs" },
    { "ordner": "docs", "muster": "Sichttest_*.md" }
  ],
  "abbildung": { "tab": "tab_zeigen titel", "setup.open": "datei_oeffnen pfad" }
}
```

| Key | Meaning | if missing |
|---|---|---|
| `anwendung` | name the application reports itself with; applies to lists that name none | the application connected last |
| `start` | how **▶ Exe starten** (start exe) launches the application | the executable from the list, without arguments |
| `listen[]` | folders with lists (or single lists), each with `ablage` (storage) and `muster` (file pattern) | the project root, storage `sichttest-logs/` in the root |
| `abbildung` | which action a field of the JSON format triggers (“mapping”) | the field is read and shown as text |
| `gewichtung` | files of the retest indicator (“weighting”): `{ "hand": [ … ], "git": [ … ] }`; even empty (`{}`) it switches the indicator on | no indicator |

The JSON format knows a few fields that become actions only through `abbildung`:
`setup.close_all_tabs`, `setup.open`, `test_db` and `test_db.ende` (preparation and teardown of
the list), `tab` and `restart` (at a step). Every key of `abbildung` is also a link in the text
(`tab:<title>`, `sql:<query>`, `datei:<path>` …, see below). Which action a field triggers is
decided by the project file alone; the tool knows no action name of any application.

### Links in the text of a step

The text of a step is Markdown, in both list formats. **Every key of `abbildung` is also a
link:** `[Text](<key>:<value>)` triggers the action the project file assigns to the key, and the
value goes along as its first argument. A scheme that `abbildung` does not name (`https:` …)
opens outside as usual.

| Written in the text | One click | Line in `abbildung` (example) |
|---|---|---|
| `[Database](tab:Database)` | brings the tab with this title to the front | `"tab": "tab_zeigen titel"` |
| `[Query](sql:SELECT%20*%20FROM%20t)` | puts the query on the clipboard and hands it to the application | `"sql": "sql_einfuegen sql"` |
| `[Template](datei:configs/examples/a.json)` | opens the file in the application | `"datei": "datei_oeffnen pfad"` |

The tool knows the first two by name; the third line is an example: the project file chooses the
key freely.

- **Spelling:** a space in the target is written as `%20` (`tab:Firmware%20Update`), otherwise the
  link ends there. `tab://Title` is the same as `tab:Title`. A key can serve as a link if it starts
  with a letter and contains only lower-case letters, digits, `.`, `+` and `-` (`setup.open:`
  works, `test_db:` does not because of the underscore).
- **Argument:** the value goes along under the first key the `abbildung` line names after the
  action (`pfad` for `"datei": "datei_oeffnen pfad"`); if it names none, the key is `wert`.
- **Connected:** the action runs as with **▶ Herstellen**; the application's answer appears in the
  line next to the action buttons, and the test log records the call under `actions` of the step
  shown.
- **Not connected:** that line shows the manual step (“von Hand: …”, “by hand: …”). `tab:` and
  `sql:` show it even if the project file does not map them. The query of an `sql:` link is on the
  clipboard in any case.
- **The application answers with an error** (for example: no open tab with this title): the line
  names action and error in red, with the manual step below. The run does not stop.
- While an action is running, and in a run that is only being viewed, a click has no effect.

If a step carries the field `tab`, the tab also appears as a link **Tab: …** in the line below the
title; a click does the same as the link in the text.

An `abbildung` line names, after the action, the keys that go along as arguments
(`"test_db": "testdb_einrichten name seed"`); they come unchanged from the field of the list. The
key **`fortgesetzt`** (“resumed”) is predefined: the tool fills it with `true` when the run
already carries a verdict, otherwise with `false`. When the action from `test_db` answers `ok`,
the test log says `test_db.active: true`; after an `ok` of the action from `test_db.ende` it says
`false`.

Before each call the tool allows the application to bring its window or a dialog to the front.

The tool offers the teardown of a list when the last item is rated, and on closing — there also
when nothing is rated yet but an action of the preparation has already run.

**States of the application.** An application can report what it has set up and what of it is
still standing (`meldeZustand("test_db", true, "mytestdb")` in the headers). A teardown action
names with `"wenn": "test_db"` (“if”) — in the `abbildung` as
`"test_db.ende": "testdb_abbauen wenn=test_db"` — what it is for. If such a state is standing, the
tool shows it right after connecting as a line with the button **Nachbereitung ausführen** (“run
teardown”), even when it is left over from an earlier run; the test log records under `zustaende`
what the application reported last. The value `{fortgesetzt}` in the arguments of an action is
replaced by true or false at call time.

**Build at the verdict.** Every rated step carries `build` in the test log, with the executable
and its timestamp at the moment of rating; `build` at the root names the one used last. A verdict
for a step the list no longer contains stays in the test log and appears at the end under
“Nicht mehr in der Liste” (“no longer in the list”).

## What it writes

Next to the list, into the subfolder `sichttest-logs/` — or into the storage the project names
for the folder of the list:

| File | Content |
|---|---|
| `<list>_<time>.testlog.json` | the run: per step `result` (`pass`, `pass_remark`, `fail`, `skip`, `open`), `remark`, `screenshots`, `rated`, `rated_at` (time of the verdict, ISO), `actions` for triggered actions; plus `build` and `summary`. `setup`, `test_db` and `areas` are copied unchanged from the list; the teardown is recorded under `teardown_actions`. If an application was connected, `steuerung` names it, its version, the DLL version (`produkt`), `p`, `s` and the `zustand` of the connection |
| `<list>_<time>.report.md` | the same state for reading: first Fail and Pass with finding with remark and images, then skipped, open, pass |
| `<list>_<time>_<id>_<n>.png` | the screenshots |
| `testergebnisse.sqlite` | per storage a small SQLite file across all runs: per run the totals and the build, per step verdict, remark and the build it was given against. The test log remains the truth; the file is its index |

It writes after every verdict, after every screenshot, when changing step and on closing. On the
next start the most recent run of the list is resumed as long as it has open steps. If it is
complete, a new run begins and the header says so; the completed run stays as it is.
**▶ Neuer Lauf** (new run) starts afresh at any time.

`build` names the executable the run was made against and its timestamp (ISO): that of the
connected application; without a connection the one from the list or the project. A resumed run
without a connection keeps what its test log already names.

## Retest indicator

If the project file names `gewichtung`, the tool shows at every open step how urgently it should
be tested again: **🔴** urgent (from 0.7), **🟡** recommended (from 0.3), no mark: not critical.
The reason is shown at the step; the list selection shows the number of 🔴 and 🟡 per list. A step
counts as tested on the day its verdict was given — even in a run that began weeks earlier; the
last result of a step is its most recent verdict. An entry of the list selection starts with the
title of the list, then the counters, then the file name; the path is in the tooltip.

| Case | Urgency |
|---|---|
| named in `retest_steps` · never Pass or Fail · last test Fail | 1.0 |
| last test Pass, an area of the step was changed afterwards | its `weight`, the highest applies |
| last test Pass, a commit touched the paths of an area afterwards | `auto_weight` (default 0.5), only raises |
| last test Pass, nothing changed | 0 |

Skipped does not count as tested. The last result per step identifier comes from the result
database of the storage, across all lists. The comparison is against the build the Pass was given
against.

The **hand file** (`hand`) is maintained by whoever changes the code (template:
[templates/test-gewichtung.json](templates/test-gewichtung.json)):

```json
{ "auto_weight": 0.5,
  "retest_steps": { "db-03": "retest the fix from the run of 10 Oct" },
  "areas": { "database": { "weight": 0.8, "changed": "2026-10-10", "note": "query rebuilt",
                           "paths": [ "src/db/*.cpp" ] } } }
```

`changed` is a date or a timestamp (`2026-10-10T14:30:00`). A step names its areas in the field
`areas` of the list (JSON format). The **git file** (`git`) is written by a git hook: per area the
most recent commit that touched its `paths`. The hook is in the package under `werkzeuge/` (run
`install_hooks.ps1` once per clone in the application's repository; needs Python); it reads the
paths from the project file — the first file under `hand` belongs to the first under `git`, and so
on. The git file is local to the machine and does not belong in the repository.

**↷ Unkritische überspringen** (“skip non-critical”) marks all open steps below 0.3 as skipped;
the same button takes exactly these back, steps skipped by hand stay.

## Statistics and older runs

**Statistik…** opens a separate window: the situation in one sentence, all runs of the list
(start, build, counters, test log) and their steps with passes, fails and the last result —
problem steps first. It stays open and refreshes on every write.

The statistics read from the result database of the storage (`testergebnisse.sqlite`), which
fills with every write of a test log. Test logs written by an older version are not in it.
**Ältere Testlogs einlesen** (“import older test logs”) adds all test logs of the storage that are
missing in the database — for all lists of the storage, only on this click, and without changing
any test log. The retest indicator then counts them as well.

**Lauf öffnen** (open run, or a double click) shows an older run in the main window, **read
only**: verdicts, remarks and images are there, nothing is written. **Diesen Lauf fortsetzen**
(continue this run) makes it editable; for a completed run the button reads **Diesen Lauf
bearbeiten** (edit this run). **Zurück zum aktuellen Lauf** returns to the current run.

## Finding archive

Every step keeps a history of its findings. **🗄 Archivieren** (archive) files remark and
screenshots with time, verdict and build and clears the field for the next finding; the verdict
stays. This happens by itself when the remark of an already rated step changes — on re-rating, on
moving to another step, before a new screenshot. If a verdict gives way to another (or is taken
back) while the remark stays the same, the old verdict with its build goes into the history;
remark and screenshots stay at the step.

The archived findings appear in a line below the screenshots (most recent first, all in the
tooltip), in the test log under `history` of the step and in the report in the section
**Historie**. The image files stay; a new image gets the next free number.

## Operation

| What | How |
|---|---|
| Pass / Pass with finding / Fail / Skip | buttons or Ctrl+1 / 2 / 3 / 4; then on to the next open item |
| Take a verdict back | ○ Offen (open) |
| Screenshot | Win+Shift+S, then **📋 Screenshot anhängen** (attach), Ctrl+Shift+V or Ctrl+V in the remark field; drag image files into the window |
| View / delete a screenshot | hover (preview), double click opens, Del or right click deletes |
| File a finding, clear the field | **🗄 Archivieren** |
| Bring the step's tab to the front | click **Tab: …** in the line below the title (lists with the field `tab`) |
| More room for the step text or the remark | drag the splitter between them; its position is kept across sessions |
| Keep the window above the program | checkbox **Im Vordergrund** (on top) |
| Only what is left to do | checkbox **Nur offene und Fail zeigen** (show only open and fail) |
| Hand findings on | **Report-Pfad kopieren** (copy report path) and paste it into the chat |

Fail and Pass with finding require a remark or a screenshot.

## Not yet supported

Only Windows is built and tested; the control interface exists only there.

## License

SichtTest_Helper is available under either **MIT** ([LICENSE-MIT](LICENSE-MIT)) or **Apache-2.0**
([LICENSE-APACHE](LICENSE-APACHE)), at your option. The tool uses Qt 6, which is under its own
licenses; details in [LICENSE](LICENSE).
