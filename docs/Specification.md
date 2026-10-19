**English** · [Deutsch](Specification.de.md)

# Sichttest control — Specification

> **Binding.** This document specifies how the tool `Sichttest.exe` drives an application under
> test: the C interface **S** of the DLL, the protocol **P** between DLL and tool, the version
> rules, the list formats, the project file, loading and the package. It describes product version
> **v0.4.0**, interface **S 1.1** and protocol **P 1.1**. The German translation
> ([Specification.de.md](Specification.de.md)) follows this text; where they differ, the English
> text applies.
>
> How to integrate an application step by step: [Integration_Guide.md](Integration_Guide.md).
> Templates: [templates/](../templates/). What each version added: [CHANGELOG.md](../CHANGELOG.md).
>
> Identifiers of the interface, the protocol, the list formats and the project file are German
> (`sts_melde_aktion`, `anwendung`, `abbildung`, …); they are part of the contract and are quoted
> as they are. The [glossary](#glossary) gives their meaning.

## 1. Principles

1. The tool is a program of its own. It is not linked into the applications as a library.
2. This project builds a **DLL** that provides the whole interface for controlling an
   application. Applications load it; tool, DLL and applications evolve separately, and
   compatibility is checked through version numbers (§5).
3. An application is driven only when it is started with an argument of its own, by convention
   `<Application>.exe --testing`.
4. The tool knows the list formats, but **no action name of any application**. Action names, their
   meaning and everything project-specific belong to the application: its source code registers
   the actions, its project file (§7) says where lists and results live.
5. **The run never stops, and the human always gives the verdict.** Every action has a text that
   describes the manual step; without a connection, or when an action fails, that text is what
   remains.

## 2. Parts and ownership

```
 Sichttest.exe (Qt, own process)                Application.exe --testing (its own Qt)
 ┌───────────────────────────────┐              ┌──────────────────────────────────────────┐
 │ lists, rating, log, report    │   channel    │ SichttestSteuerung1.dll  (no Qt)          │
 │ triggers actions of a list    │◄────────────►│   channel, framing, queue                 │
 │ listens (server)              │  protocol P  │ ───────── C interface S ───────────────── │
 └───────────────────────────────┘              │ header sichttest_steuerung.h (+ .hpp, Qt) │
                                                │   compiled in the application             │
                                                │ actions of the application (name → func)  │
                                                └──────────────────────────────────────────┘
```

| Part | Owned by | Content |
|---|---|---|
| `Sichttest.exe` | this project | lists, rating, test log, report; channel (listens), triggering actions, showing the manual step as text |
| `SichttestSteuerung<S>.dll` | this project | channel, protocol P, queue; **no Qt, no C++ types at the boundary** |
| `sichttest_steuerung.h` | this project | the C interface S (the only binding boundary) |
| `sichttest_steuerung.hpp` | this project | small C++ header: loads the DLL at run time, RAII, lambdas as actions; **compiled in the application** |
| `sichttest_steuerung_qt.hpp` | this project | Qt add-on: delivers callbacks in the GUI thread, `QString`/`QJsonObject` convenience; also compiled in the application |
| list formats, project file format | this project | §6, §7 |
| actions (names, effect, flags) | each application | registered at run time |
| project file, lists, weighting files | each application | in the application's repository |

## 3. Interface S of the DLL (C)

Principles: C types only; strings are UTF-8 and null-terminated; **memory never changes owner**
(what the DLL returns is valid until the next call or the end of the callback; what the
application passes in, the DLL copies at once); every structure starts with its size so that it
can grow in a minor version; no call throws.

```c
#define STS_S_MAJOR 1          /* version of interface S the application is compiled with */
#define STS_S_MINOR 1          /* 1.1: sts_melde_zustand, sts_bei_ende */

typedef struct sts_sitzung sts_sitzung;        /* opaque: a session */
typedef struct sts_antwort sts_antwort;        /* opaque, lives only during the callback */

typedef enum {
    STS_OK = 0,            /* done */
    STS_FEHLER = 1,        /* action known but failed (the text says why) */
    STS_UNBEKANNT = 2,     /* the application does not know this action */
    STS_UNGUELTIG = 3,     /* arguments do not fit */
    STS_UNVERTRAEGLICH = 4,/* version S or P does not fit */
    STS_KEIN_TESTER = 5    /* no tester reachable */
} sts_status;

/* Callback of an action. Runs in the thread that calls sts_pumpe() — i.e. the GUI thread. */
typedef sts_status (*sts_rueckruf)(void* nutzer,
                                   const char* aktion,          /* name as registered */
                                   const char* argumente_json,  /* JSON object, UTF-8 */
                                   sts_antwort* antwort);       /* text for the tester */

typedef struct {
    uint32_t     groesse;        /* sizeof(sts_aktion) */
    const char*  name;           /* [a-z0-9_.], owned by the application */
    const char*  beschreibung;   /* one sentence, shown in the tester */
    const char*  parameter;      /* hint: "file=<path>" or similar, free text */
    uint32_t     schalter;       /* STS_FRAGT_NACH | STS_NUR_ERSTLAUF | STS_WARTET_AUF_MENSCH |
                                    STS_BEENDET_ANWENDUNG, see below */
    sts_rueckruf rueckruf;
    void*        nutzer;
} sts_aktion;

typedef struct {
    uint32_t    groesse;         /* sizeof(sts_konfig) */
    uint16_t    s_major, s_minor;/* STS_S_MAJOR / STS_S_MINOR of the application */
    const char* anwendung;       /* "MyApp" — the name lists and the project file use */
    const char* version;         /* version of the application, goes into the test log */
    const char* projekt_datei;   /* optional: path of the project file in a folder .sichttest; otherwise search rule §7 */
} sts_konfig;

/* Versions of the DLL, available without a session. */
void       sts_fassung(uint16_t* s_major, uint16_t* s_minor,
                       uint16_t* p_major_min, uint16_t* p_major_max,
                       const char** produkt);                 /* "0.4.0" = repository tag without v */

/* Create a session; checks S (§5). Opens nothing yet. On failure there is no session;
   sts_letzter_fehler(NULL) then names the reason. */
sts_status sts_oeffne(const sts_konfig* k, sts_sitzung** s);
/* Register an action; before or after connecting (later registrations are sent on). */
sts_status sts_melde_aktion(sts_sitzung* s, const sts_aktion* a);
/* Wake-up: called from the DLL's reader thread as soon as something is waiting. May only
   post a nudge into the GUI thread (e.g. post an event). */
void       sts_setze_wecker(sts_sitzung* s, void (*wecker)(void* nutzer), void* nutzer);
/* Connect to the tester; starts it if none is listening. Returns at once; the result
   shows in the state. */
sts_status sts_verbinde(sts_sitzung* s);
/* In the GUI thread: run waiting calls (callbacks run HERE). Returns their number. */
int        sts_pumpe(sts_sitzung* s);
/* Attach a text to the answer (only inside the callback). */
void       sts_antwort_text(sts_antwort* antwort, const char* text);
/* Free message to the tester; appears in its status line and in the log. */
sts_status sts_melde(sts_sitzung* s, const char* text);
/* From S 1.1. Report a named state that the application sets up and tears down again
   (for example a test database). name: [a-z0-9_.]; steht: 0 or 1; text: free, may be NULL.
   Allowed at any time, also before sts_verbinde(): the DLL keeps the latest value per name
   and sends it on every change and after every connect. */
sts_status sts_melde_zustand(sts_sitzung* s, const char* name, int steht, const char* text);
/* From S 1.1. Callback when an existing connection to the tester ends. geordnet = 1: the
   tester was closed; 0: connection lost. Runs like an action inside sts_pumpe(). The DLL
   does not reconnect by itself afterwards. */
void       sts_bei_ende(sts_sitzung* s, void (*ende)(void* nutzer, int geordnet), void* nutzer);
/* 0 = disconnected, 1 = connecting, 2 = connected, 3 = rejected (version). */
int        sts_zustand(sts_sitzung* s);
/* Reason of the last failure of this session. The text stays until a new failure replaces
   it; a success does not clear it. It is meaningful only right after a call that reported an
   error, or while the state is disconnected or rejected. */
const char* sts_letzter_fehler(sts_sitzung* s);
/* Writes out waiting answers and `tschuess` (at most 2 s), then ends. */
void       sts_schliesse(sts_sitzung* s);
```

**What is not in the interface:** paths to lists, storage, weighting, the location of the tester
and how the application is started. All of that is in the **project file** (§7), not in the
application's source code. The source code holds only the name, the version and the actions —
what only the source code can know. A new application therefore needs no change to the
interface, and a new list folder needs no recompilation.

**The four flags of an action** are set by the application when it registers the action:

| Flag | Meaning | What the tester does |
|---|---|---|
| `STS_FRAGT_NACH` | changes state that can be lost (closes tabs, discards unsaved work) but does **not ask itself** | asks before, once per preparation — also when resuming. The question carries the checkbox “In dieser Sitzung nicht mehr fragen” (don't ask again in this session): checked and answered yes, the tester asks no more until it is closed; this applies to all applications and lists |
| `STS_NUR_ERSTLAUF` | tears down instead of setting up; belongs only at the start of a new run | skips the action when a run is **resumed** (§6.4) |
| `STS_WARTET_AUF_MENSCH` | the action asks **in the application** (dialogs); its duration depends on the human | no time limit; shows “wartet auf die Anwendung” (waiting for the application) with **Abbrechen** (cancel): the tester stops waiting, a late answer is discarded |
| `STS_BEENDET_ANWENDUNG` | after the answer the application exits or restarts | does not treat the end of the connection as an error, shows “Anwendung startet neu” (application restarting) and waits for the new connection |

An action carries `STS_FRAGT_NACH` **or** `STS_WARTET_AUF_MENSCH`, never both — the question is
asked in one place, where its text is right.

“Asks before” and “first run only” are separate flags because they do not always coincide: an
action that closes all tabs carries both; an action that loads a template and thereby replaces
unsaved work carries `STS_FRAGT_NACH`, but **is** the setting up and must run when resuming.

**No callback before `sts_verbinde()`.** Before it there is no connection and therefore no call;
afterwards callbacks run only inside `sts_pumpe()`. With the moment it calls `sts_verbinde()` the
application decides from when on it can be driven (for example only after it has restored its
start state).

**Re-entrancy.** A callback may open a modal dialog; that spins the event loop, and the wake-up
delivers `sts_pumpe()` again while the first callback is still running. Defined: per session **at
most one callback runs at a time**. An `sts_pumpe()` inside a callback runs nothing and returns 0;
what is waiting comes next after the callback ends (the DLL wakes again). No call is executed
twice. The tester sends only one call at a time per connection anyway.

**Callbacks in the GUI thread.** The DLL reads the channel in a thread of its own and puts every
call into a queue. Calls are executed **only** in `sts_pumpe()`, which the application calls in
its GUI thread. So that it need not poll, the DLL calls the **wake-up**; the Qt header sets it to
`QMetaObject::invokeMethod(…, Qt::QueuedConnection)`, so that `sts_pumpe()` runs in the next pass
of the event loop. The DLL knows neither Qt nor an event loop.

**In a Qt application** (with `sichttest_steuerung_qt.hpp`):

```cpp
if (QCoreApplication::arguments().contains("--testing")) {
    m_steuerung = sichttest::Steuerung::lade("MyApp", version);   // loads the DLL, else empty
    if (m_steuerung) {
        m_steuerung->aktion("show_panel", "Brings a panel to the front", "name=<title>",
            [this](const QJsonObject& arg) -> sichttest::Ergebnis {
                return showPanel(arg["name"].toString())
                     ? sichttest::ok() : sichttest::fehler("no panel of this name");
            });
        m_steuerung->verbinde();
    }
}
```

**The classes of the headers:** `sichttest::Sitzung` in `sichttest_steuerung.hpp` (no Qt; actions
as lambdas that receive the arguments as JSON text) and `sichttest::Steuerung` in
`sichttest_steuerung_qt.hpp` (arguments as `QJsonObject`, delivery in the GUI thread). Both are
created through `lade(anwendung, version, …)` and are empty when the DLL is missing or its version
does not fit; both offer `aktion()`, `verbinde()`, `melde()`, `zustand()`, `letzterFehler()`,
`fassung()` and, from S 1.1, `meldeZustand()` and `beiEnde()`. Results are built with
`sichttest::ok()`, `sichttest::fehler(text)` and `sichttest::ungueltig(text)`. An exception thrown
by an action becomes status `fehler` with its text.

**States of the application (S 1.1).** What an application sets up and tears down again, only the
application itself knows — a test database may still stand from an old run or may have been
removed by hand. The application therefore reports such states by name (`sts_melde_zustand`, in
the headers `meldeZustand(name, steht, text)`), and the tester does not keep its own idea of what
is true. Through `sts_bei_ende` (`beiEnde`) the application learns that the tester has gone. An
application compiled with the S 1.0 headers keeps running unchanged against an S 1.1 DLL (§5).

**Deliberately not in version 1:** actions that complete later (answer after the callback has
returned), and events of the application as a condition of a step. Both can be added as a minor
version (new calls).

## 4. Channel and protocol P (tool ↔ DLL)

**The tester listens, the DLL connects** — not the other way round.

- The application opens nothing that could be accessed from outside; with `--testing` it builds a
  connection **outwards**.
- The tester survives a crash and restart of the application. The restarted application connects
  again, and the run continues at the same step, without the tester having to know anything about
  the restart.
- **Ending:** if the application says goodbye in order (`tschuess`) and it started the tester
  (`--steuerung`), the tester shows below the status line “Die Anwendung … wurde beendet” (the
  application … has exited) with the button **Tester beenden** (close tester). It does not close by
  itself and does not ask modally; if the application connects again, the notice disappears. No
  notice after a lost connection, after an action with `STS_BEENDET_ANWENDUNG`, and for a tester
  started by hand. If the human closes the tester, the application keeps running; it learns of it
  through `sts_bei_ende` (S 1.1).
- Several applications can be connected at the same time; a list names its application (§6), and
  the tester chooses the connection by the name from `sts_konfig.anwendung`.

**Start sequence** (`Application.exe --testing`):

1. The application loads the DLL (§8), registers its actions, calls `sts_verbinde()`.
2. The DLL looks for the tester's channel. If none is listening, it starts `Sichttest.exe`
   (search rule §7.1) with `--steuerung` and retries for up to 10 s.
3. `hallo` → `willkommen` (or `abgelehnt`). From `hallo` the tester knows the path of the
   executable; from there it finds the project file (§7) and opens its lists. Then calls follow.

The DLL reads no project file and knows no path except that of the tester.

A tester started by hand listens as well; a list without actions behaves exactly as without the
DLL.

**Channel:** on Windows a named pipe `\\.\pipe\sichttest-<user>`, accessible only to the logged-on
user (`QLocalServer::UserAccessOption`). That only one tester per user listens is ensured by a
named mutex `Global\sichttest-<user>`, which the tester takes before listening. A second tester
does not listen, says so in its status line and works without control. The DLL uses the Windows
API directly; the tester uses `QLocalServer` (on Windows the same pipe). Other platforms: §10, D3.

Two environment variables are read **only by the DLL**, both for development and the self-test:
`SICHTTEST_KANAL` replaces the name of the pipe, `SICHTTEST_EXE` the path of the tester the DLL
starts (otherwise `<folder of the DLL>\sichttest\Sichttest.exe`, §7.1). The tester itself reads
neither; it always listens on `sichttest-<user>`.

**Frame:** 4 bytes of length (little endian) + one JSON object in UTF-8. The DLL brings its own
small JSON reader; it passes the arguments of an action on as text without checking them. Which
message a frame carries is in the field **`nachricht`** (`{"nachricht":"hallo", …}`).

| Message | Direction | Content |
|---|---|---|
| `hallo` | DLL → tester | `p_min`, `p_max` (major versions of P the DLL speaks), `s`, `produkt`, `anwendung`, `version`, `exe`, `pid`, `projekt_datei` (if set), `aktionen[]` (`name`, `beschreibung`, `parameter`, `schalter[]`). The flags are written as **words**: `fragt_nach`, `nur_erstlauf`, `wartet_auf_mensch`, `beendet_anwendung` |
| `willkommen` | tester → DLL | chosen version `p`, product version of the tester as `tester` |
| `abgelehnt` | tester → DLL | reason as text (no common version) |
| `aktionen` | DLL → tester | actions registered later |
| `aufruf` | tester → DLL | `id`, `aktion`, `argumente{}`, `basis` (project root per §7, otherwise the folder of the list). The tester passes every value on **unchanged**, paths included; they are resolved in the application (which may, for example, accept only targets below its repository), `basis` is only an offer |
| `antwort` | DLL → tester | `id`, `status` (`ok`, `fehler`, `unbekannt`, `ungueltig`), `text` |
| `meldung` | DLL → tester | free text |
| `zustand` | DLL → tester | from P 1.1: `name`, `steht` (true/false), `text` — the latest value of a state, on every change and after every connect |
| `tschuess` | both | orderly end |

A message the DLL does not know is answered with `antwort`, status `unbekannt` (with the `id` if
the message carried one); the tester ignores what it does not know (§5).

**Only the major version of P is reported and negotiated.** `sts_fassung`, `p_min` / `p_max` in
`hallo` and `p=` in `VERSION` name it as a range; the minor version is only in the header
(`STS_P_MINOR`). A DLL that speaks P 1.1 therefore reports “1 to 1” — a minor version only adds
(here the message `zustand`), and whoever does not know it ignores it.

The tester waits 10 s per call (changeable per action in the list); for actions with
`STS_WARTET_AUF_MENSCH` there is no limit (§3). If the answer does not come, §6.3 applies: the
manual step appears as text, a late answer is discarded.

**Foreground:** before every call the tester allows the application to bring itself to the front
(`AllowSetForegroundWindow` with the `pid` from `hallo`) — otherwise a dialog of the action would
stay behind the tester. Bringing itself to the front is the application's job.

## 5. Three numbers, two of them checked

| Number | Where it is | When checked | By whom |
|---|---|---|---|
| **P** — protocol tester ↔ DLL, `major.minor` | compiled into tester and DLL | on connecting (`hallo`) | tester |
| **S** — interface DLL ↔ application, `major.minor` | `STS_S_MAJOR/MINOR` in the header, i.e. in the application; in the DLL; the major number also **in the file name** `SichttestSteuerung1.dll` | on loading (`sts_oeffne`) | DLL |
| **Product** — repository tag, e.g. `v0.4.0` | `Solution.json`, Git tag | not at run time; it is the application's **pin** | — |

**What the major and the minor number change**

- **Minor number** (P and S): something is added, nothing changes its meaning. P: new messages,
  new fields. S: new calls, new fields at the end of a structure, new status values.
- **Major number**: something existing changes form or meaning, or is removed.
- Mandatory on both sides: unknown fields are ignored; an unknown message is answered with
  `unbekannt`, never with an abort.

**Compatibility rule**

- **S:** the DLL accepts an application if the major numbers are equal and the application's minor
  number is not above the DLL's. Otherwise `STS_UNVERTRAEGLICH` with plain text
  (`sts_letzter_fehler`); the application runs on without control. Because the major number is in
  the file name, an incompatible DLL cannot even be loaded.
- **P:** the **tester** carries the load, because it is the part kept current once per machine,
  while every application has its own, older pin. It speaks the current major version and the
  previous one. The highest common one is chosen.
  - Peer **older**, same major version: what it lacks, it answers with `unbekannt`; the tester
    shows the manual step as text.
  - Peer **older than the previous major version**: `abgelehnt` with the advice to raise the
    application's pin. The run continues without control.
  - Peer **newer** than the tester (major version): `abgelehnt` with the advice to update the
    tester. The run continues without control.
- **In no case does an incompatibility stop the run.** It shows in the tester's status line and in
  the test log (`steuerung: { anwendung, version, produkt, p, s, zustand }`). `zustand` is
  `verbunden` (connected) or `getrennt` (disconnected) — then the details of the application
  connected last remain — or `abgelehnt` (rejected); then the block carries `grund` (reason)
  instead of the details. It names the application of the list. If no application ever connected
  during a run, the block is missing.

**Actions** carry no number of their own on this side: their names belong to the application, and
the tester learns on every connect which ones exist.

## 6. Lists

An **action** is: a name, arguments (key = value), and the **text** that describes the manual step
for a human. The text is always there — it is the fallback.

### 6.1 Markdown

The list stays readable as a plain checklist. An action is a **code span starting with
`aktion:`**; it stands in the text of the item it belongs to:

```markdown
**Anwendung:** MyApp · **Exe:** `out\build\MyApp.exe`

## M. The marker stays at its time

- [ ] **M0 Vorbereiten:** In the panel Composer **Load...** → `presets\scene_A.tpl`.
      Then switch **Edit** on and click **Fit** once. You see: length 1:00 …
      `aktion: load_template file="presets\scene_A.tpl"`
      `aktion: edit on` `aktion: fit`
- [ ] **M1 The marker can be heard and seen:** *Do:* set the playhead to 0:20, Play …
      `aktion: playhead time=0:20`
```

- Form: `` `aktion: <name> [value] [key=value …]` ``; values with spaces in `"…"`. A single value
  without a key arrives as argument `wert`.
- **Preparation:** an item whose title starts with **Vorbereiten** (prepare). It gets no verdict,
  but **▶ Ausführen** (run) and **Weiter →** (next); its actions run in order. Placed before the
  first section, it applies to the whole list.
- **Per step:** the actions of an ordinary item do **not** run by themselves, but on the button
  **▶ Herstellen** (establish) — the human decides whether to do the manual step themselves (often
  the manual step is exactly what is being tested).
- **Reload:** every item additionally shows **↺ Vorbereitung**, which repeats the preparation of
  its section (“if you changed something, simply reload the template”).
- The tool removes the `aktion:` snippets from the displayed text; in any other Markdown viewer
  they appear as inconspicuous code at the end of the item.
- **Reference:** `` `aktion: @G0` `` runs the actions of the item with identifier `G0` of the same
  list, at this position in the sequence; own actions may stand before and after it. So “like G0,
  but Edit on” is just `` `aktion: @G0` `aktion: edit on` `` and cannot drift apart. A reference
  to an unknown identifier, or a circular one, is reported when the list is loaded and treated
  like an unknown action (§6.3).
- `**Anwendung:**` in the preamble names the name the application reports itself with. Without
  it, the only connected application applies.

### 6.2 `*.testprotokoll.json`

General form:

```json
{
  "title": "…", "anwendung": "MyApp",
  "vorbereitung": [ { "aktion": "open_file", "mit": { "path": "examples/demo.myproj" },
                      "text": "Open the demo project" } ],
  "steps": [
    { "id": "db-01", "section": "A", "title": "…", "text": "…",
      "aktionen": [ { "aktion": "show_tab", "mit": { "title": "Database" } } ] },
    { "id": "db-00", "kind": "prep", "title": "Vorbereiten", "text": "…", "aktionen": [ … ] }
  ],
  "nachbereitung": [ { "aktion": "testdb_teardown", "wenn": "test_db", "text": "…" } ]
}
```

An action entry carries `aktion`, `mit` (arguments), `text` (manual step) and optionally `frist`
(time limit in seconds) and `wenn` (see below). Keys starting with `_` are comments and are
ignored.

**`nachbereitung`** (teardown) at the root: actions the tester offers at the end of the run (on
completion and on closing), never while it waits for a restarting application. On closing it
offers them if the run carries a verdict **or an action of the preparation has run with `ok` in
this session** (otherwise what the preparation set up would be left standing); if the preparation
runs again after a teardown, the teardown becomes due again. In the test log the triggered
actions are recorded at the root under `teardown_actions`, in the same form as `actions[]` of a
step.

**`wenn` and the states of the application** (§3): a teardown action can name with
`"wenn": "<state>"` what it is for — `{ "aktion": "testdb_teardown", "wenn": "test_db" }`, in the
`abbildung` as `"test_db.ende": "testdb_teardown wenn=test_db"`. It belongs to the offer only as
long as the application has not reported this state as torn down; without `wenn`, or if the
application never reports the state, the rule above applies. If a state is standing for which the
open list has a teardown, the tester shows it as a line below the status line — “In MyApp 1.2.0
steht noch: test_db (mytest)” (still standing) with the button **Nachbereitung ausführen** (run
teardown) — right after connecting and without a dialog; on closing it then offers the teardown
in any case. If a run already carries a verdict, still has open steps, and the application reports
the state as torn down, the same line warns. The test log carries at the root
`zustaende { name: { steht, text } }`: what the application reported last.

**`{fortgesetzt}`** (resumed): if this value stands in the arguments of any action — in Markdown
`aktion: x run={fortgesetzt}`, in `vorbereitung`, `aktionen` or `nachbereitung` — the tester
replaces it at call time with true or false (§6.4). The predefined key of the `abbildung` (§7)
remains valid.

**Fields that become actions through `abbildung`.** The JSON format knows a few fields that carry
meaning for a step or a list, but whose action belongs to the application. **Which action** a
field triggers is stated in the project file under `abbildung` (§7); without a line there, the
field is read and shown as text.

| Field | Becomes |
|---|---|
| `setup.close_all_tabs` | preparation: one action, typically with `STS_FRAGT_NACH` and `STS_NUR_ERSTLAUF` |
| `setup.open[]` | preparation: one action per entry, the entry as first argument |
| step `"tab"` and link `[…](tab://Title)` in the text | an action with the title as argument; also shown as link **Tab: …** below the step title |
| step `"restart": true` | an action, typically with `STS_BEENDET_ANWENDUNG` |
| link `[…](sql:SELECT…)` in the text (without `//`) | an action with the query as argument; the tester fills the clipboard itself |
| `test_db { name, seed[] }` | preparation, runs **before** `setup`; with `test_db.ende` as the teardown |
| step `"kind": "prep"` | a preparation item without verdict (as in 6.1) |
| step `"areas": […]` | no action; read and carried into the log (retest indicator, §6.7) |
| keys starting with `_` | comment, ignored |

**Links in general:** `tab` and `sql` are no special case. **Every key of the `abbildung` is also
a link** in the text of a step — `[Text](<key>:<value>)` triggers the action named there, the value
goes along as its first argument (for example `"file": "open_file path"` and
`[Template](file:configs/a.json)`). A scheme that the `abbildung` does not name opens outside. Only
two things remain tied to the names `tab` and `sql`: `sql` additionally fills the clipboard, and
both show the manual step without an `abbildung` line instead of opening outside. Only a key that
is a valid URL scheme can serve as a link (lower-case letters, digits, `.`, `+`, `-`; thus not
`test_db`).

**`test_db.active` in the test log** follows the state that the teardown from `test_db.ende` names
with `wenn`, as soon as the application has ever reported it. If the application reports no state
(headers S 1.0), it is `true` after an `ok` of the action from `test_db` and `false` after an `ok`
of the action from `test_db.ende`. For this to be right, an action that sets up a state must
**never answer `ok` without having set it up** — `fehler` when the human declines and on every
failure (the preparation ends there, §6.3), `ok` otherwise; details belong in the answer text. A
teardown that finds nothing to tear down answers `ok`. Through the predefined key `fortgesetzt`
the setup action learns whether the run is being resumed and can reuse a standing state without
asking.

**Test log of older runs:** the tool also reads and writes the fields `history[]` per step,
`setup` and `test_db` including `test_db.active` at the root, and `pass_remark` in the summary, so
that runs written by earlier testers stay readable.

### 6.3 Unknown action, no connection, failure

One rule for all cases: **the run never stops, the manual step appears as text.**

| Case | What the tester does |
|---|---|
| no application connected | ▶ buttons are grey; the text of the item is there as in any list |
| action not in the list from `hallo` | already when loading the list: notice “3 Aktionen kennt MyApp 1.2.0 nicht” (MyApp does not know 3 actions) with the names; at the item: “von Hand:” (by hand) + text |
| answer `unbekannt` / `ungueltig` / `fehler` | the application's message in red at the item, below it “von Hand:” + text |
| no answer within the time limit | like an error, text “keine Antwort nach 10 s” (no answer after 10 s) |
| the tester started the application (`start`, §7), but no `hallo` arrives within the limit | its own message: “gestartet, aber nicht verbunden — läuft die Anwendung schon ohne `--testing`?” (started but not connected — is the application already running without `--testing`?); typical for an application that allows only one instance per executable |
| action with `fragt_nach` (closes tabs, discards unsaved work) | the tester asks once per preparation what is about to happen |

Every triggered action is recorded with name, status and text in the test log of the step
(`actions[]`). The verdict always stays with the human — a failed action never turns an item into
Fail.

**Several actions on one item** run in order, and the sequence **ends at the first one that
fails**: the following ones do not run, since they usually build on the failed one. The tester
names the failed action and shows the manual step. Status in the test log: from the application
`ok`, `fehler`, `unbekannt`, `ungueltig`; from the tester `frist` (no answer), `getrennt` (no
connection), `ausgelassen` (skipped: `nur_erstlauf` when resuming) and `abgebrochen` (the human
cancelled during `wartet_auf_mensch`).

### 6.4 Resuming a run

When a run is resumed (new start of the tester, or the application has reconnected), the tester
offers the preparation again, but skips every action with `STS_NUR_ERSTLAUF`: it only sets up,
never tears down. Actions with `STS_FRAGT_NACH` run, after the question. This requires the other
actions of the preparation to be repeatable — an action that opens a file answers `ok` if the file
is already open; one that sets up a test database, if it is already active.

**How the tester recognises resuming:** the run already carries at least one verdict (Pass, Pass
with finding, Fail or skipped). A run without a verdict counts as a first run, even if the
preparation has already run once.

**Which run the tester takes when opening a list:** the most recent one of the list, **as long as
it has open steps** (`summary.open` of the test log above zero). If the most recent one is
complete, a new run begins; the header says so, and the completed run stays unchanged. Runs are
sorted by their name, which carries the timestamp.

**The build belongs to the verdict:** every rated step carries `build { exe, exe_timestamp }` from
the moment of rating — the executable of the connected application from `hallo`, otherwise that of
the list or `start.exe` of the project; the timestamp is ISO. If a verdict is taken back, its
`build` is removed. The block `build` at the root names the build used last; without a connection
it stays when resuming until something is rated again. A verdict without its own stamp (from an
older run) gets, when resuming, the one the run named at the root until then — so a Pass from June
remains a Pass against the June build, even if the run continues in October.

**Time of the verdict:** next to `rated` (day and time without year, for reading) every verdict
carries `rated_at` (ISO, local time). Taking a verdict back to open removes `rated_at` together
with `rated`.

**Orphaned verdicts:** texts, links, `areas` and the set of steps come from the current list when
resuming. If the test log carries a verdict, a remark or images for a step the list no longer
contains, it stays in `steps[]` with `"verwaist": true` (orphaned), appears at the end under
“Nicht mehr in der Liste” (no longer in the list), can be viewed but not rated, and counts neither
in the totals nor in the indicator.

**Rules for the test log:** one report per run, `<run>.report.md`, on every write; the generated
preparation `V0` stands in `steps[]` as `kind: prep`; `summary.open` never counts preparations.

**Opening and connecting change no test log**, also not that of a resumed run; a test log is
written only after a verdict, a screenshot, a change of step or on closing.

### 6.5 Storage

Without a project file the tool writes to `<folder of the list>/sichttest-logs/`. With a project
file (§7) every list folder names its storage (`ablage`); test log, report, screenshots and the
result database go there.

The **identifier of a step** is unique per storage; the last results of a step are read per
storage across all lists, so that a step moved to another list keeps its history.

### 6.6 Result database

Per storage the tool keeps `testergebnisse.sqlite`, mirrored on every write; **the test log
remains the truth**, the database is its index. Tables `testlauf` (run), `testschritt` (step),
`protokoll` (list). `log_datei` carries the file name of the test log without folder. Columns are
only ever added; added so far: `testschritt.build_exe`, `testschritt.build_timestamp`,
`testschritt.rated_at`. Preparations are not mirrored, orphaned verdicts are.

- The database is created on the first write in a storage. When the tool opens an existing
  database it converts every `log_datei` with a folder to the bare name, once; if two rows then
  carry the same name (a storage that was copied and used further), the one with the higher
  `lauf_id` stays, the older one goes with its steps. A run is looked up by name, case-insensitive.
  **Opening a storage can therefore change the database** (conversion, added columns), but never a
  test log.
- A database error stops nothing; it is shown once in the window.
- **Ältere Testlogs einlesen** (import older test logs) mirrors every `*.testlog.json` of the
  storage whose name the database does not know — across all lists of the storage, **only on this
  click**, never when a list is opened, and without changing any test log. A file that cannot be
  read as a test log is skipped and counted.
- An older run opens from the statistics **read only**: nothing is written, neither test log nor
  database. **Diesen Lauf fortsetzen** (continue) makes a run with open steps editable, **Diesen
  Lauf bearbeiten** (edit) a completed one; the header then calls it “wieder geöffnet” (reopened).

### 6.7 Retest indicator

**When it exists:** as soon as the project file names `gewichtung`, even empty — then only the
history counts (never verified, last Fail). Without the key, and without a project file, there is
no indicator.

**Urgency per step, 0 to 1:** preparation 0 · named in `retest_steps` 1.0 · never verified 1.0 ·
last result Fail 1.0 · otherwise the highest `weight` of an area of the step that was changed
after the last Pass. `skip` does not count as verified, `pass_remark` does. The git layer only
raises to `auto_weight` (default 0.5), never lowers.

- Thresholds: 🔴 from 0.7 · 🟡 from 0.3 · below that not critical.
- The reference of both layers is the build at the verdict, otherwise that of the run, otherwise
  its start. Timestamps are compared as points in time, not as text (the git file carries a zone,
  the build stamp does not; without a zone, local time applies). The hand layer compares by day
  when `changed` is a plain date — against the day of the verdict (`rated_at`); a verdict without
  `rated_at` counts at the start of its run.
- “The last result” of a step is the one with the most recent verdict, across all lists of the
  storage (by step identifier), not the one from the run started last.
- **↷ Unkritische überspringen** (skip non-critical) sets open steps below 0.3 to `skip` after a
  question, with the remark “Auto-Skip (Nachtest-Indikator …): …” and the marker
  `"auto_skip": true`; the same button takes exactly these back without touching steps skipped by
  hand.
- In the window: 🔴 and 🟡 only at open steps; the reason in one line at the step; in the list
  selection the number of 🔴 and 🟡 per list. If a run is resumed against a different build than
  its test log names, the header says so.

**The hand file** (`gewichtung.hand[]`): `areas { <name>: { weight, changed, note?, auto_weight?,
paths[]? } }`, `retest_steps { <id>: <reason> }`, `auto_weight` at the root. `changed` is a date
or an ISO timestamp; `paths` are patterns (like `fnmatch`) for the git layer.
**The git file** (`gewichtung.git[]`): `areas { <name>: { changed, commit, file, commits } }`,
`changed` as ISO timestamp with zone. It is local to the machine. Several files per layer are
merged in the order of the list: `areas` and `retest_steps` per key, everything else overwritten. A
named file that is missing is skipped silently.

**The hook** `werkzeuge/gewichtung_autoupdate.py` (in this repository `tools/`; post-commit and
post-merge, installed by `install_hooks.ps1`, needs Python) takes `hand[i]` as the source of the
patterns and writes `git[i]`; if `git[i]` is missing, it writes nothing for `hand[i]`. It always
exits with 0.

### 6.8 Finding archive

`history[]` per step: `{ archived, result, remark, screenshots, rated, build }`.

- **🗄 Archivieren** (archive) takes the finding as it stands in the field, clears remark and
  images and leaves the verdict; without remark and image there is nothing to archive.
- **By itself** when the text of a step that is not open changes (compared without surrounding
  whitespace) — on re-rating, on saving the remark (also when moving to another step) and before a
  new screenshot.
- On every change of a verdict to another one, also when taking it back to open: the entry then
  carries verdict, `rated`, `build` and the remark of the old state, even if empty, and no images;
  remark and images stay at the step as long as the text stays the same. If the changed remark has
  already filed the old finding, no second entry arises. An image alone, and the same verdict
  again, archive nothing.
- The next image gets a number above the highest the step ever carried, archive included.
- At the step a line shows the two most recent entries, all of them in the tooltip; the report
  lists all per step, most recent first (section **Historie**). The result database mirrors
  `history`.

## 7. The project file

**One file per application, in the application's repository, versioned:**
`.sichttest/sichttest.projekt.json`. It carries everything the tester needs to know about the
project and that may change without anyone recompiling. Template:
[templates/.sichttest/sichttest.projekt.json](../templates/.sichttest/sichttest.projekt.json).

**Location and root:** the file lies in the folder `.sichttest` in the root of the project. **All
paths in the file are relative to the root** — the folder above `.sichttest` —, not to the file.

```json
{
  "schema": 1,
  "anwendung": "MyApp",
  "start": { "exe": "out/build/release/MyApp.exe", "argumente": ["--testing"] },
  "listen": [
    { "ordner": "tests/sichttest", "ablage": "tests/sichttest-logs" },
    { "ordner": "docs", "muster": "Sichttest_*.md", "ablage": "tests/sichttest-logs" }
  ],
  "gewichtung": {
    "hand": [ "tests/sichttest/test-gewichtung.json" ],
    "git":  [ "tests/sichttest/gitGewichtung.json" ]
  },
  "abbildung": {
    "setup.close_all_tabs": "close_all_tabs",
    "setup.open":           "open_file path",
    "tab":                  "show_tab title",
    "restart":              "restart",
    "sql":                  "insert_sql sql",
    "test_db":              "testdb_setup name seed fortgesetzt",
    "test_db.ende":         "testdb_teardown wenn=test_db"
  }
}
```

| Key | Meaning | if missing |
|---|---|---|
| `schema` | version of this file; unknown keys are ignored | counts as 1 |
| `anwendung` | the name the application reports itself with (`sts_konfig.anwendung`) | the tester takes the only connected application |
| `start` | how the tester starts the application **if none has connected in its session yet**. If one had connected (crash, restart), it starts the executable from that application's `hallo` — otherwise it would fetch the release build after a debug build crashed | it does not start it; the human does |
| `listen[]` | list folders (or single lists), each with `ablage` and optionally `muster` (file pattern, e.g. `"Sichttest_*.md"` — without it the tool takes every `*.md` with steps). A named folder that does not exist is skipped silently (for example a local folder missing in a fresh clone) | the project root; storage `sichttest-logs/` in the root |
| `gewichtung { hand[], git[] }` | **switches the retest indicator on** (§6.7), even empty. Files in **two layers** that are computed differently: `hand` carries `weight`, `changed`, `note` per area, plus `retest_steps` and `auto_weight`; `git` carries `changed`, `commit`, `file` and only raises to `auto_weight`. Within a layer the order of the list applies. A named file that is missing is skipped silently. Format and maintenance stay with the application | no indicator |
| `abbildung` | which action a field of the JSON format triggers (§6.2): the name of the action, then the keys that go along unchanged from the field as arguments (an array like `seed` stays an array; what the line does not name does not go along). Every key is also a link in the text (`tab://…`, `sql:…`, `file:…`; §6.2). **The key `fortgesetzt` is predefined**: it is not a field of the list; the tester fills it at call time with `true` if the run already carries a verdict (§6.4), otherwise `false`. A part `wenn=<state>` is not a key: it names the state of the application the action is for (§6.2) | the field is read and shown as text |

The smallest useful file names only the application, its start and its lists:

```json
{ "schema": 1, "anwendung": "MyApp",
  "start": { "exe": "out/build/release/MyApp.exe", "argumente": ["--testing"] },
  "listen": [ { "ordner": "docs/tests", "muster": "Sichttest_*.md" } ] }
```

**How it is found**

- Start through the application: `hallo` names the path of the executable. The tester searches
  **upwards** from there for the first folder `.sichttest` that contains a `sichttest.projekt.json`.
  `sts_konfig.projekt_datei` sets the path explicitly, for an executable outside the repository.
- **Switching lists on `hallo`:** if an application connects whose project file differs from the
  one currently open, the tester opens the lists of its project — even if it was started by hand
  with other lists. It does not switch while actions are running, and not if the application has
  no project file.
- Start by hand: `Sichttest <projectfile>` or `Sichttest <folder>` — in the second case it searches
  upwards from the folder. Without a project file it works with the folder alone.
- **A project file named explicitly** (`Sichttest <projectfile>`, `sts_konfig.projekt_datei`) must
  also lie in a folder `.sichttest`; its name is free (on the command line it must end in
  `.projekt.json`, by which the tester recognises it). The root is the folder above `.sichttest`, as
  for the file found by searching. If it lies elsewhere, the tester does not load it, opens no list
  and reports: “Die Projektdatei <path> liegt nicht in einem Ordner .sichttest.” (the project file
  does not lie in a folder .sichttest)
- A project file that cannot be loaded (wrong place, not readable, not JSON) is reported and not
  bypassed: the tester then does not show the lists of the folder either.

**What the file brings together:** one place for lists, storage, weighting and start; the same for
a tester started by the application and one started by hand; and the action names stay with the
application. A new application writes such a file and registers actions; tool, DLL and interface
do not change for it.

### 7.1 Where the tester lies next to the application

`Sichttest.exe` brings **its own Qt**. It must therefore **not lie next to the application's
executable**: the application's Qt is there, and both sets are called `Qt6Core.dll` (the
application may use another Qt version, or a debug build loading `Qt6Cored.dll`).

Search rule of the DLL for the tester, in this order: environment variable `SICHTTEST_EXE` (for
development on the tool) → `<folder of the DLL>\sichttest\Sichttest.exe` (§10, D2).

## 8. Loading only with `--testing`

The application is **not linked against the DLL**. The header `sichttest_steuerung.hpp` loads
`SichttestSteuerung1.dll` at run time (`LoadLibrary`, the addresses of the calls fetched one by
one), and only where the application asks for it — in the `--testing` branch. It loads with the
**full path** from the folder of the executable, never through the search path.

- An ordinary start loads nothing and opens nothing. This follows from the structure, not from a
  check someone could forget.
- If the DLL is missing, `--testing` says so in one sentence and the application runs on.
- To **build**, the application needs **only the headers**; DLL and tester are run-time additions.
  An import library and a linker switch for delayed loading are not needed.
- **Which DLL is loaded** is told by `Sitzung::fassung()` and `Steuerung::fassung()`: S, the range
  of P and the product. The file `VERSION` lies only in the root of the package, not next to the
  executable.
- The header `sichttest_steuerung.hpp` includes `<windows.h>` and defines `WIN32_LEAN_AND_MEAN` and
  `NOMINMAX` before it if they are missing; both then apply to the rest of the including file.
  Therefore the header belongs in **one** `.cpp`, not in a header of the application.

**If the package is missing, the headers are missing too** (another platform, D3; or no network on
the first fetch). The application then compiles without control. If the package is there, the
consumption (§9) sets the define `SICHTTEST_VORHANDEN=1` at the target, next to the include path:

```cpp
#ifdef SICHTTEST_VORHANDEN
#  include <sichttest_steuerung_qt.hpp>
#endif
```

Everything that uses the control stands behind `#ifdef SICHTTEST_VORHANDEN` in **one** place of
the application (registering the actions). The define comes from the same function as the include
path, so the configure log shows which branch applies.

## 9. Package and consumption

**What a version delivers** (one package per tag `vX.Y.Z`, as a release on GitHub):

```
sichttest-vX.Y.Z-win64/
├── include/    sichttest_steuerung.h · sichttest_steuerung.hpp · sichttest_steuerung_qt.hpp
├── bin/        SichttestSteuerung1.dll
├── sichttest/  Sichttest.exe with its Qt
├── werkzeuge/  gewichtung_autoupdate.py, install_hooks.ps1 (retest indicator)
└── VERSION     three lines: produkt=0.4.0 · s=1.1 · p=1 (of P only the major version)
```

The package is built as Release only, without `.pdb`. S and P are binding in the header
`sichttest_steuerung.h`; the file `VERSION` is generated from the template `packaging/VERSION.in`
(`@VERSION@` from `Solution.json`, S and P by hand), and the self-test checks S and P of the
template against the header. The product version is not in the header: DLL and tester receive it
at build time as define `STS_PRODUKT` from `Solution.json`; the application asks for it with
`sts_fassung()`.

**Pin:** every application names version, address and checksum in a file `sichttest.pin`
(template: [templates/sichttest.pin](../templates/sichttest.pin)). The checksum is that of the
**published** file — it changes with every package build, even with identical content:

```cmake
set(SICHTTEST_VERSION "v0.4.0")
set(SICHTTEST_URL     "https://github.com/PatrikNeunteufel/SichtTest_Helper/releases/download/${SICHTTEST_VERSION}/sichttest-${SICHTTEST_VERSION}-win64.zip")
set(SICHTTEST_SHA256  "<64 hex characters>")
set(SICHTTEST_FALLBACK_PATHS "../SichtTest_Helper/out/package")   # optional
```

Search order: `SICHTTEST_LOCAL_DIR` (development) → cache `.externals/sichttest/<version>/` →
download with checksum → fallback paths.

**Without network the application still builds.** Two cases:

- The package is already in the **cache** (after the first fetch): no network needed, no warning;
  headers, define and run-time files are there.
- The package is **neither in the cache nor can it be fetched:** a warning (`[CraftPackage]`,
  W304), the headers are missing, the define `SICHTTEST_VORHANDEN` is not set, and nothing is
  copied. The application then compiles without control (§8), and `--testing` runs without it.

**With CMakeCraft** (v0.10.0 or later; template:
[templates/integration/Solution.external.json](../templates/integration/Solution.external.json)):

| Need | How |
|---|---|
| fetch the finished package in the pinned version | external with `"archive": true` and `"pin": "sichttest.pin"`; order and checksum as above |
| include path and define, without linking | `include_dirs`, `define` — both apply **only to the target that names the external** (PRIVATE), not to targets depending on it |
| put the DLL and the tester folder next to the executable | `runtime { files, dirs }`, per configuration; can be switched off per target with `external_options` |

**Without CMakeCraft** (template:
[templates/integration/sichttest.cmake](../templates/integration/sichttest.cmake)): the standalone
file `CMakeCraftPackage.cmake` from the CMakeCraft repository, as an unchanged copy in the
application's repository, with `craft_package_fetch` and `craft_package_deploy`. Minimum CMake
version 3.26.

Two things an application must know:

- **The copy next to the executable runs when the target is built.** After changing the package
  version, build the executable, otherwise DLL and tester next to it stay the old ones.
- **The cache is keyed by version, not by checksum.** If a package is rebuilt under the same
  version, delete the folder `.externals/sichttest/<version>/`.

**In this project** the package is defined in the block `packages` of `Solution.json`; the target
`package_sichttest` (not in ALL, Release only) creates `out/package/sichttest-v<version>-win64/`,
the `.zip` and the `.zip.sha256`. Publishing: [Releasing.md](Releasing.md).

## 10. Design decisions

| # | Decision | Why |
|---|---|---|
| D1 | **Consumption:** every version delivers a finished package (headers, DLL, tester with its Qt) as a release on the Git tag; the application names the version in `sichttest.pin` and fetches the package at configure time; `SICHTTEST_LOCAL_DIR` points to a local state for development. | Applications build differently (with and without CMakeCraft, clang and MSVC); a finished package does not depend on how they build. |
| D2 | **Location of the tester:** it lies in the package and is placed as subfolder `sichttest\` next to the application's executable, with its own Qt. The DLL finds it there. | Two Qt sets in one folder collide (§7.1). |
| D3 | **Platform:** the control exists on Windows first. The interface is platform-neutral; on other platforms the DLL is missing and `--testing` runs without control. | — |
| D4 | **Loading:** the application does not link against the DLL. The header loads it at run time in the `--testing` branch; if it is missing, the application runs on without control (§8). | An ordinary start must not be able to load anything; a missing DLL must never break the build or the start. |
| D5 | **Channel:** the tester listens. The DLL connects with `--testing` and starts the tester if none is running. After a restart of the application it connects again; the run continues at the same step. | The application opens nothing others could reach; the tester survives crashes and restarts. |
| D6 | **Project file instead of source code:** what the tester must know about a project (lists, storage, start, weighting, mapping) stands in `.sichttest/sichttest.projekt.json`, not in the application's source; paths are relative to the root. | The interface fits today's and future applications without change; a new list folder needs no recompilation. |
| D7 | **No C++ types at the boundary**, only C. | Already without Qt a C++ boundary binds both sides to the same compiler, runtime library and build type (debug and release lay out `std::string` differently); clang, MSVC and debug builds meet here. |
| D8 | **No Qt in the DLL.** | Two Qt in one process collide already with a single Qt version (a release DLL pulls `Qt6Core.dll` into a debug process that loaded `Qt6Cored.dll`), and a DLL built against one Qt version does not run against an application's other version. The DLL needs only the Visual C++ release runtime, which every Qt application brings; since no memory changes owner (§3), it may run in a debug process. |
| D9 | **C interface plus two small headers** (`.hpp` without Qt, `_qt.hpp` for delivery in the GUI thread), compiled in the application. | They are always built with the application's compiler and Qt. |
| D10 | **Actions in the GUI thread by fetching with a wake-up** (§3). | “Delivering” by the DLL would only be possible if it knew the event loop — that is, with Qt. |
| D11 | **Three numbers** (§5): P and S are checked, the product is pinned. | Without the separation every bug fix in the tester would force a pin change in all applications. |
| D12 | **The tool knows no action name.** Fields of the JSON format become actions only through the project file's `abbildung`. | The dependency points one way: applications know the tool, the tool knows no application. |

## Glossary

German identifiers that appear in the interface, the protocol, the formats and the window.

| Identifier | Meaning |
|---|---|
| `ablage` | storage folder for test logs, reports, screenshots and the result database |
| `abbildung` | mapping of JSON fields (and link schemes) to actions of the application |
| `aktion`, `aktionen` | action(s) |
| `anwendung` | application (the name it reports itself with) |
| `antwort`, `aufruf` | answer, call (protocol messages) |
| `ausgelassen`, `abgebrochen`, `frist`, `getrennt` | skipped, cancelled, time limit (no answer), disconnected (action status set by the tester) |
| `beiEnde`, `sts_bei_ende` | on end (of the connection to the tester) |
| `fassung` | version (of S, P) |
| `fehler`, `unbekannt`, `ungueltig` | error, unknown, invalid (answer status) |
| `fortgesetzt` | resumed (run) |
| `gewichtung` | weighting (retest indicator); `hand` = maintained by hand |
| `hallo`, `willkommen`, `abgelehnt`, `tschuess` | hello, welcome, rejected, goodbye (protocol messages) |
| `lade`, `verbinde`, `pumpe`, `melde`, `schliesse` | load, connect, pump, report, close |
| `listen`, `ordner`, `muster` | lists, folder, file pattern |
| `meldeZustand`, `sts_melde_zustand`, `zustand`, `zustaende`, `steht` | report state, state(s), standing |
| `mit` | with (the arguments of an action) |
| `nachbereitung`, `vorbereitung`, `Vorbereiten` | teardown, preparation, “prepare” (title of a preparation item) |
| `produkt` | product version (repository tag without v) |
| `rueckruf`, `wecker`, `nutzer` | callback, wake-up, user data |
| `schalter` | flags of an action |
| `Sitzung`, `Steuerung` | session; control (classes of the headers) |
| `start`, `argumente` | start, arguments (of the application) |
| `testergebnisse.sqlite` | result database |
| `verwaist` | orphaned (verdict for a step no longer in the list) |
| `wenn` | if / when (the state a teardown is for) |
| `wert` | value (argument without a key) |
| `--testing`, `--steuerung`, `--pruefe`, `--schnapp`, `--selbsttest` | start under test (application); started by the DLL, check, snapshot, self-test (tool) |
