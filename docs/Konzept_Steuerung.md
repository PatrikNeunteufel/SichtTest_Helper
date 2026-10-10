# Konzept: Sichttest steuert die geprüfte Anwendung

> **Stand:** 2026-10-09 · **Status:** abgestimmt mit LV, CS und CC, E1–E7 entschieden (§11),
> Umsetzung von Patrik am 2026-10-08 gestaffelt freigegeben. **Gebaut und veröffentlicht sind die
> Schritte 1 bis 3 aus §13** (bei SH und CC): DLL mit S 1.0, Protokoll P 1, die drei Köpfe,
> Aktionen aus Listen, Projektdatei, Stufe 1 aus §14, CMakeCraft v0.10.0, Paket und Release
> `v0.2.0`; seit `v0.3.0` liegt die Projektdatei unter `.sichttest/` (P2, §7), seit `v0.3.1` nennen die Köpfe
> die Fassung der geladenen DLL (§8), seit `v0.3.2` hat die Rückfrage einen Haken (§3) und der
> Tester bietet nach dem Ende der Anwendung das Beenden an (§4), seit `v0.3.3` trägt das Testlog
> den Block `steuerung` (§5), seit `v0.3.4` wird ein abgeschlossener Lauf nicht mehr fortgesetzt,
> und `build` bleibt beim Fortsetzen erhalten (§6.4), seit `v0.3.5` setzt der Tester
> `test_db.active`, füllt den Schlüssel `fortgesetzt` (§6.2, §7) und erlaubt der Anwendung den
> Vordergrund (§4), seit `v0.3.6` bietet er die Nachbereitung beim Schließen auch nach bloßer
> Vorbereitung an (§6.2). Die Festlegungen, die beim
> Bauen fielen (im Sync SH-15), stehen seit dem 2026-10-09 in §3, §4, §6 und §7, jeweils mit
> „beim Bauen festgelegt" gekennzeichnet. Offen: Schritt 4 (LumiViz, der Bezug steht), Schritte 5 und 6 (Comm Studio).
> Gezählt wird hier nach §13; im Sync heißen dieselben Abschnitte nach der Freigabe „3 und 4"
> (CMakeCraft und Paket), „5" (LumiViz) und „6" (Comm Studio) ·
> **Gehört:** SichtTest_Helper (Sync-Prefix SH) · **Verbindliche Spezifikation** seit 2026-10-09;
> die Idee (`Idee_Steuerung_der_Anwendung.md`) ist abgelöst ·
> **Abstimmung:** Sync `…\Visuals_Project\cmake\sync_sichttest`
>
> Was beim Bauen festgelegt wurde, wo das Konzept nichts sagte, steht in der Sync-Nachricht
> `SH-20261009-1305-…` und in `STATUS.md` des Syncs (SH-14, SH-15); es wird beim Eröffnen von
> Schritt 5 mit CS hier eingearbeitet.

## 1. Gesetzt (Patrik, 2026-10-08 — nicht neu verhandelt)

1. Das Werkzeug wird in `SichtTest_Helper` entwickelt und gebaut und bleibt ein eigenes Programm.
2. Der ganze Tester wird nicht als Bibliothek in die Anwendungen eingebunden.
3. Dieses Projekt baut eine **DLL**, die die ganze Schnittstelle zur Steuerung bereitstellt. Die
   Anwendungen (LumiViz, Comm Studio, spätere) binden sie ein; alle entwickeln getrennt weiter,
   geprüft wird die Verträglichkeit über die Version.
4. Absicht: auch im Comm Studio entfällt der eingebaute Tester (`TestProtokollWindow`).
5. Gestartet wird im Projekt mit einem Argument der Anwendung (`<Anwendung>.exe --testing`).

## 2. Die Teile und wer was besitzt

```
 Sichttest.exe (Qt, eigener Prozess)            Anwendung.exe --testing (ihr eigenes Qt)
 ┌───────────────────────────────┐              ┌──────────────────────────────────────────┐
 │ Listen, Bewertung, Log, Report│   Kanal      │ SichttestSteuerung1.dll  (kein Qt)        │
 │ Aktionen einer Liste auslösen │◄────────────►│   Kanal, Rahmen, Warteschlange            │
 │ lauscht (Server)              │ Protokoll P  │ ───────── C-Schnittstelle S ───────────── │
 └───────────────────────────────┘              │ Kopf sichttest_steuerung.h (+ .hpp, Qt)   │
                                                │   in der Anwendung übersetzt              │
                                                │ Aktionen der Anwendung (Name → Funktion)  │
                                                └──────────────────────────────────────────┘
```

| Teil | Gehört | Inhalt |
|---|---|---|
| `Sichttest.exe` | SH | wie heute; neu: Kanal (lauscht), Aktionen aus Listen, Anzeige des Handgriffs als Text |
| `SichttestSteuerung<S>.dll` | SH | Kanal, Protokoll P, Warteschlange; **kein Qt, keine C++-Typen an der Grenze** |
| `sichttest_steuerung.h` | SH | die C-Schnittstelle S (einzige verbindliche Grenze) |
| `sichttest_steuerung.hpp` | SH | kleiner Kopf für C++: lädt die DLL zur Laufzeit, RAII, Lambdas als Aktionen; **wird in der Anwendung übersetzt** |
| `sichttest_steuerung_qt.hpp` | SH | Zusatz für Qt: stellt die Rückrufe im GUI-Thread zu, `QString`/`QJsonObject`-Bequemlichkeit; ebenfalls in der Anwendung übersetzt |
| Aktionen (Namen, Wirkung) | jede Anwendung | LumiViz: LV · Comm Studio: CS |
| Schreibweise in Listen | SH | §6 |

## 3. Schnittstelle S der DLL (C)

Grundsätze: nur C-Typen; Zeichenketten sind UTF-8, nullterminiert; **Speicher wechselt nie den
Besitzer** (was die DLL liefert, gilt bis zum nächsten Aufruf oder bis zum Ende des Rückrufs; was
die Anwendung übergibt, kopiert die DLL sofort); jede Struktur beginnt mit ihrer Größe, damit sie
in einer kleinen Fassung wachsen kann; kein Aufruf wirft.

```c
#define STS_S_MAJOR 1          /* Fassung der Schnittstelle S, mit der die Anwendung übersetzt ist */
#define STS_S_MINOR 1          /* 1.1: sts_melde_zustand, sts_bei_ende */

typedef struct sts_sitzung sts_sitzung;        /* undurchsichtig */
typedef struct sts_antwort sts_antwort;        /* undurchsichtig, lebt nur im Rückruf */

typedef enum {
    STS_OK = 0,            /* erledigt */
    STS_FEHLER = 1,        /* Aktion bekannt, aber gescheitert (Text sagt, warum) */
    STS_UNBEKANNT = 2,     /* diese Aktion kennt die Anwendung nicht */
    STS_UNGUELTIG = 3,     /* Argumente passen nicht */
    STS_UNVERTRAEGLICH = 4,/* Fassung S oder P passt nicht */
    STS_KEIN_TESTER = 5    /* kein Tester erreichbar */
} sts_status;

/* Rückruf einer Aktion. Läuft im Thread, der sts_pumpe() ruft — also im GUI-Thread. */
typedef sts_status (*sts_rueckruf)(void* nutzer,
                                   const char* aktion,          /* Name, wie angemeldet */
                                   const char* argumente_json,  /* JSON-Objekt, UTF-8 */
                                   sts_antwort* antwort);       /* Text für den Tester */

typedef struct {
    uint32_t     groesse;        /* sizeof(sts_aktion) */
    const char*  name;           /* [a-z0-9_.], gehört der Anwendung */
    const char*  beschreibung;   /* ein Satz, erscheint im Tester */
    const char*  parameter;      /* Hinweis: "datei=<pfad>" o. ä., frei */
    uint32_t     schalter;       /* STS_FRAGT_NACH | STS_NUR_ERSTLAUF | STS_WARTET_AUF_MENSCH |
                                    STS_BEENDET_ANWENDUNG, s. u. */
    sts_rueckruf rueckruf;
    void*        nutzer;
} sts_aktion;

typedef struct {
    uint32_t    groesse;         /* sizeof(sts_konfig) */
    uint16_t    s_major, s_minor;/* STS_S_MAJOR / STS_S_MINOR der Anwendung */
    const char* anwendung;       /* "LumiViz", "CommStudio" — der Name, den Listen nennen */
    const char* version;         /* Version der Anwendung, landet im Testlog */
    const char* projekt_datei;   /* optional: Pfad zur Projektdatei in einem Ordner .sichttest; sonst Suchregel §7 */
} sts_konfig;

/* Fassungen der DLL, ohne Sitzung abfragbar. */
void       sts_fassung(uint16_t* s_major, uint16_t* s_minor,
                       uint16_t* p_major_min, uint16_t* p_major_max,
                       const char** produkt);                 /* "0.3.6" = Tag des Repos ohne v */

/* Sitzung anlegen; prüft S (§5). Öffnet noch nichts. Scheitert es, gibt es keine Sitzung;
   den Grund nennt dann sts_letzter_fehler(NULL). */
sts_status sts_oeffne(const sts_konfig* k, sts_sitzung** s);
/* Aktion anmelden; vor oder nach dem Verbinden (später Angemeldetes wird nachgemeldet). */
sts_status sts_melde_aktion(sts_sitzung* s, const sts_aktion* a);
/* Wecker: wird aus dem Lesethread der DLL gerufen, sobald etwas wartet. Darf nur
   einen Anstoß in den GUI-Thread stellen (z. B. ein Ereignis posten). */
void       sts_setze_wecker(sts_sitzung* s, void (*wecker)(void* nutzer), void* nutzer);
/* Mit dem Tester verbinden; startet ihn, falls keiner lauscht. Kehrt sofort zurück,
   das Ergebnis kommt über den Zustand. */
sts_status sts_verbinde(sts_sitzung* s);
/* Im GUI-Thread: wartende Aufrufe ausführen (Rückrufe laufen HIER). Liefert die Zahl. */
int        sts_pumpe(sts_sitzung* s);
/* Text zur Antwort legen (nur im Rückruf). */
void       sts_antwort_text(sts_antwort* antwort, const char* text);
/* Freie Meldung an den Tester, erscheint dort in der Statuszeile und im Log. */
sts_status sts_melde(sts_sitzung* s, const char* text);
/* Ab S 1.1. Einen benannten Zustand melden, den die Anwendung herstellt und wieder abräumt
   (etwa eine Test-Datenbank). name: [a-z0-9_.]; steht: 0 oder 1; text: frei, darf NULL sein.
   Jederzeit erlaubt, auch vor sts_verbinde(): die DLL merkt sich den letzten Stand je Name
   und schickt ihn bei jeder Änderung und nach jedem Verbinden. */
sts_status sts_melde_zustand(sts_sitzung* s, const char* name, int steht, const char* text);
/* Ab S 1.1. Rückruf, wenn eine bestehende Verbindung zum Tester endet. geordnet = 1: der
   Tester wurde geschlossen; 0: Abriss. Läuft wie eine Aktion in sts_pumpe(). Die DLL
   verbindet danach nicht von selbst neu. */
void       sts_bei_ende(sts_sitzung* s, void (*ende)(void* nutzer, int geordnet), void* nutzer);
/* 0 = getrennt, 1 = verbindet, 2 = verbunden, 3 = abgelehnt (Fassung). */
int        sts_zustand(sts_sitzung* s);
/* Grund des letzten Fehlschlags dieser Sitzung. Der Text bleibt stehen, bis ein neuer
   Fehlschlag ihn ersetzt; ein Erfolg leert ihn nicht. Aussagekräftig ist er nur unmittelbar
   nach einem Aufruf, der einen Fehler gemeldet hat, oder solange der Zustand getrennt oder
   abgelehnt ist. (Befund LV, Entscheid Patrik 2026-10-09) */
const char* sts_letzter_fehler(sts_sitzung* s);
/* Schreibt wartende Antworten und `tschuess` noch hinaus (höchstens 2 s), dann Ende. */
void       sts_schliesse(sts_sitzung* s);
```

**Was nicht in der Schnittstelle steht** (Vorgabe Patrik, 2026-10-08): Pfade zu Listen, Ablage,
Gewichtung, der Ort des Testers und wie die Anwendung gestartet wird. Das alles steht in der
**Projektdatei** (§7) und nicht im Quelltext der Anwendung. Im Quelltext stehen nur der Name, die
Version und die Aktionen — also das, was nur der Quelltext wissen kann. Eine neue Anwendung
braucht damit keine Änderung an der Schnittstelle, und ein neuer Listenordner keine Übersetzung.

**Die vier Schalter einer Aktion** setzt die Anwendung beim Anmelden:

| Schalter | Bedeutung | Was der Tester tut |
|---|---|---|
| `STS_FRAGT_NACH` | verändert Zustand, der verloren gehen kann (Tabs schließen, Ungespeichertes verwerfen), fragt aber **nicht selbst** | fragt vorher, einmal je Vorbereitung — auch beim Fortsetzen. Die Rückfrage trägt den Haken „In dieser Sitzung nicht mehr fragen“: gesetzt und mit Ja beantwortet, fragt der Tester nicht mehr, bis er beendet wird; er gilt für alle Anwendungen und Listen (ab `v0.3.2`, Patrik 2026-10-09) |
| `STS_NUR_ERSTLAUF` | räumt ab, statt herzustellen; gehört nur an den Anfang eines neuen Laufs | lässt die Aktion beim **Fortsetzen** eines Laufs aus (§6.4) |
| `STS_WARTET_AUF_MENSCH` | die Aktion fragt **in der Anwendung** nach (Dialoge), ihre Dauer hängt am Menschen | keine Frist; zeigt „wartet auf die Anwendung" mit **Abbrechen** (der Tester wartet dann nicht weiter, eine späte Antwort wird verworfen) |
| `STS_BEENDET_ANWENDUNG` | nach der Antwort endet die Anwendung oder startet neu | wertet das Ende der Verbindung nicht als Fehler, zeigt „Anwendung startet neu" und wartet auf die neue Verbindung |

Eine Aktion trägt `STS_FRAGT_NACH` **oder** `STS_WARTET_AUF_MENSCH`, nie beide — gefragt wird an
einer Stelle, dort, wo der Text stimmt.

„Fragt vorher" und „nur im ersten Lauf" sind getrennt, weil sie nur beim Comm Studio
zusammenfallen (Befund LV, 08.10.2026): `tabs_schliessen` trägt beide; `komposition_laden` von
LumiViz ersetzt Ungespeichertes und trägt `STS_FRAGT_NACH`, **ist** aber das Herstellen und muss
beim Fortsetzen laufen.

**Kein Rückruf vor `sts_verbinde()`.** Vorher gibt es keine Verbindung und damit keinen Aufruf;
danach laufen Rückrufe nur in `sts_pumpe()`. Die Anwendung bestimmt mit dem Zeitpunkt von
`sts_verbinde()` also, ab wann sie gesteuert werden kann (LumiViz: erst nach dem Wiederherstellen
seines Startzustands, `LumiViz/…/MainWindow.cpp:217–263`).

**Wiedereintritt.** Ein Rückruf darf einen modalen Dialog öffnen; der dreht die Ereignisschleife,
und der Wecker stellt `sts_pumpe()` erneut zu, während der erste Rückruf noch läuft. Festgelegt:
je Sitzung läuft **höchstens ein Rückruf zugleich**. Ein `sts_pumpe()` innerhalb eines Rückrufs
führt nichts aus und liefert 0; was wartet, kommt nach dessen Ende an die Reihe (die DLL weckt
dann erneut). Kein Aufruf wird zweimal ausgeführt. Der Selbsttest prüft das mit einem Rückruf, der
selbst `sts_pumpe()` ruft. Der Tester schickt je Verbindung ohnehin nur einen Aufruf zur Zeit.

**Rückrufe im GUI-Thread.** Die DLL liest den Kanal in einem eigenen Thread und legt jeden Aufruf
in eine Warteschlange. Ausgeführt wird **nur** in `sts_pumpe()`, das die Anwendung in ihrem
GUI-Thread ruft. Damit sie nicht pollen muss, ruft die DLL den **Wecker**; der Qt-Kopf setzt ihn
auf `QMetaObject::invokeMethod(…, Qt::QueuedConnection)`, sodass `sts_pumpe()` im nächsten
Durchlauf der Ereignisschleife läuft. Die DLL kennt dabei weder Qt noch eine Ereignisschleife.

**So sieht es in einer Qt-Anwendung aus** (mit `sichttest_steuerung_qt.hpp`):

```cpp
if (QCoreApplication::arguments().contains("--testing")) {
    m_steuerung = sichttest::Steuerung::lade("LumiViz", version);   // lädt die DLL, sonst leer
    if (m_steuerung) {
        m_steuerung->aktion("panel_zeigen", "Holt ein Panel nach vorn", "name=<Titel>",
            [this](const QJsonObject& arg) -> sichttest::Ergebnis {
                return zeigePanel(arg["name"].toString())
                     ? sichttest::ok() : sichttest::fehler("kein Panel dieses Namens");
            });
        m_steuerung->verbinde();
    }
}
```

**Die Klassen der Köpfe** (beim Bauen festgelegt, 2026-10-09): `sichttest::Sitzung` in
`sichttest_steuerung.hpp` (ohne Qt; Aktionen als Lambdas, die die Argumente als JSON-Text
bekommen) und `sichttest::Steuerung` in `sichttest_steuerung_qt.hpp` (Argumente als
`QJsonObject`, Zustellung im GUI-Thread). Beide entstehen über `lade(anwendung, version, …)` und
sind leer, wenn die DLL fehlt oder die Fassung nicht passt; beide kennen `aktion()`, `verbinde()`,
`melde()`, `zustand()`, `letzterFehler()` und seit `v0.3.1` `fassung()`. Ergebnisse baut man mit
`sichttest::ok()`, `sichttest::fehler(text)` und `sichttest::ungueltig(text)`. Eine Ausnahme aus
einer Aktion wird zum Status `fehler` mit ihrem Text.

**Zustände der Anwendung (S 1.1, ab `v0.4.0`; Vorschlag CS, Entscheid Patrik 2026-10-10).** Was
eine Anwendung herstellt und wieder abräumt, kennt nur sie selbst — eine Test-Datenbank kann aus
einem alten Lauf noch stehen oder von Hand abgebaut sein. Deshalb meldet die Anwendung solche
Zustände mit Namen (`sts_melde_zustand`, in den Köpfen `meldeZustand(name, steht, text)`), und der
Tester schreibt nicht mehr selbst mit, was er für wahr hält. Über `sts_bei_ende` (`beiEnde`)
erfährt sie, dass der Tester gegangen ist. Eine Anwendung, die mit den Köpfen S 1.0 übersetzt ist,
läuft gegen die DLL S 1.1 unverändert weiter (§5); der Selbsttest prüft das.

**Bewusst nicht in Fassung 1:** Aktionen, die erst später fertig werden (Antwort nach dem
Rückruf), Ereignisse der Anwendung als Bedingung eines Schritts. Beides lässt sich als kleine
Fassung (neue Aufrufe) nachtragen.

## 4. Kanal und Protokoll P (Sichttest.exe ↔ DLL)

**Der Tester lauscht, die DLL verbindet sich** — nicht umgekehrt.

- Die Anwendung öffnet nichts, worauf jemand von außen zugreifen könnte; sie baut bei
  `--testing` eine Verbindung **nach außen** auf.
- Der Tester überlebt Absturz und Neustart der Anwendung. Die neu gestartete Anwendung verbindet
  sich wieder, der Lauf geht beim selben Schritt weiter. Das ersetzt `--resume-log=<pfad>` des
  Comm Studio (`UART/gui/Comm_Studio/MainWindow.cpp:2323–2330`), ohne dass der Tester etwas
  über den Neustart wissen muss.
- **Beenden (ab `v0.3.2`, Patrik 2026-10-09):** Verabschiedet sich die Anwendung geordnet
  (`tschuess`) und hat sie den Tester gestartet (`--steuerung`), zeigt der Tester unter der
  Statuszeile „Die Anwendung … wurde beendet“ mit dem Knopf **Tester beenden**. Er schließt nicht
  von selbst und fragt nicht modal; verbindet sich die Anwendung wieder, verschwindet der
  Hinweis. Kein Hinweis bei einem Abriss, nach einer Aktion mit `STS_BEENDET_ANWENDUNG` und bei
  einem von Hand gestarteten Tester. Beendet der Mensch den Tester, läuft die Anwendung weiter;
  dass sie darauf reagiert, ist nicht beschlossen (bräuchte S 1.1).
- Mehrere Anwendungen können zugleich verbunden sein; eine Liste nennt ihre Anwendung (§6), der
  Tester wählt die Verbindung nach dem Namen aus `sts_konfig.anwendung`.

**Ablauf beim Start** (`Anwendung.exe --testing`):

1. Anwendung lädt die DLL (§8), meldet ihre Aktionen an, ruft `sts_verbinde()`.
2. Die DLL sucht den Kanal des Testers. Lauscht keiner, startet sie `Sichttest.exe` (Suchregel §7)
   mit `--steuerung` und versucht es bis zu 10 s erneut.
3. `hallo` → `willkommen` (oder `abgelehnt`). Aus `hallo` kennt der Tester den Pfad der Exe; von
   dort findet er die Projektdatei (§7) und öffnet deren Listen. Danach Aufrufe.

Die DLL liest keine Projektdatei und kennt keine Pfade außer dem des Testers.

Ein Tester, der wie heute von Hand gestartet wird, lauscht ebenfalls; eine Liste ohne Aktionen
verhält sich genau wie heute.

**Kanal:** unter Windows eine benannte Pipe `\\.\pipe\sichttest-<Benutzer>`, nur für den
angemeldeten Benutzer zugänglich (`QLocalServer::UserAccessOption`). Dass nur ein Tester je
Benutzer lauscht, sichert ein benannter Mutex `Global\sichttest-<Benutzer>`, den der Tester vor
dem Lauschen nimmt. Ein zweiter Tester lauscht nicht, sagt das in der Statuszeile und arbeitet
ohne Steuerung wie heute. Die DLL benutzt die Windows-API direkt; der Tester benutzt
`QLocalServer` (unter Windows dieselbe Pipe). Andere Plattformen: §11 E3.

Zwei Umgebungsvariablen liest **nur die DLL**, beide für Entwicklung und Selbsttest (beim Bauen
festgelegt, 2026-10-09): `SICHTTEST_KANAL` ersetzt den Namen der Pipe, `SICHTTEST_EXE` den Pfad
des Testers, den die DLL startet (sonst `<Ordner der DLL>\sichttest\Sichttest.exe`, §7.1). Der
Tester selbst liest keine von beiden; er lauscht immer auf `sichttest-<Benutzer>`.

> Gemessen am 2026-10-09 (SichtTest_Session2): DLL und `QLocalServer` reden über dieselbe Pipe.
> `QLocalServer` allein lässt aber einen zweiten Tester desselben Benutzers auf demselben Namen
> zu; deshalb der Mutex (Entscheid Patrik, 2026-10-09). Früherer Wortlaut: „nur eine erste
> Instanz (kein Unterschieben)". Meldung an LV, CS und CC im nächsten Sync.

**Rahmen:** 4 Byte Länge (little endian) + ein JSON-Objekt in UTF-8. Die DLL bringt dafür einen
eigenen kleinen JSON-Leser mit; die Argumente einer Aktion reicht sie ungeprüft als Text weiter.
Welche Nachricht ein Rahmen trägt, steht im Feld **`nachricht`** (`{"nachricht":"hallo", …}`).

| Nachricht | Richtung | Inhalt |
|---|---|---|
| `hallo` | DLL → Tester | `p_min`, `p_max` (große Fassungen von P, die die DLL spricht), `s`, `produkt`, `anwendung`, `version`, `exe`, `pid`, `projekt_datei` (falls gesetzt), `aktionen[]` (`name`, `beschreibung`, `parameter`, `schalter[]`). Die Schalter stehen als **Wörter**: `fragt_nach`, `nur_erstlauf`, `wartet_auf_mensch`, `beendet_anwendung` |
| `willkommen` | Tester → DLL | gewählte Fassung `p`, Produktversion des Testers als `tester` |
| `abgelehnt` | Tester → DLL | Grund als Text (keine gemeinsame Fassung) |
| `aktionen` | DLL → Tester | nachgemeldete Aktionen |
| `aufruf` | Tester → DLL | `id`, `aktion`, `argumente{}`, `basis` (Root des Projekts nach §7, sonst der Ordner der Liste). Der Tester reicht jeden Wert **unverändert** durch, auch Pfade; aufgelöst wird in der Anwendung (das Comm Studio löst gegen Repo und Exe-Ordner auf und lässt nur Ziele darunter zu, `TestProtokollWindow.cpp:1407–1417`), `basis` ist nur ein Angebot |
| `antwort` | DLL → Tester | `id`, `status` (`ok`, `fehler`, `unbekannt`, `ungueltig`), `text` |
| `meldung` | DLL → Tester | freier Text |
| `zustand` | DLL → Tester | ab P 1.1: `name`, `steht` (wahr/falsch), `text` — der letzte Stand eines Zustands, bei jeder Änderung und nach jedem Verbinden |
| `tschuess` | beide | geordnetes Ende |

Eine Nachricht, die die DLL nicht kennt, beantwortet sie mit `antwort`, Status `unbekannt` (mit der
`id`, falls die Nachricht eine trug); der Tester überliest Unbekanntes (§5).

Der Tester wartet je Aufruf 10 s (in der Liste je Aktion änderbar); für Aktionen mit
`STS_WARTET_AUF_MENSCH` gilt keine Frist (§3). Bleibt die Antwort aus, gilt §6.3: der Handgriff
erscheint als Text, eine verspätete Antwort wird verworfen.

**Vordergrund (ab `v0.3.5`, Hinweis CS):** Vor jedem Aufruf erlaubt der Tester der Anwendung,
sich nach vorn zu holen (`AllowSetForegroundWindow` mit der `pid` aus `hallo`) — sonst bliebe ein
Dialog der Aktion hinter dem Tester. Nach vorn holen muss sich die Anwendung selbst.

## 5. Drei Nummern, zwei davon werden geprüft

| Nummer | Wo sie steht | Wann geprüft | Von wem |
|---|---|---|---|
| **P** — Protokoll Tester ↔ DLL, `groß.klein` | in Tester und DLL einkompiliert | beim Verbinden (`hallo`) | Tester |
| **S** — Schnittstelle DLL ↔ Anwendung, `groß.klein` | `STS_S_MAJOR/MINOR` im Kopf, also in der Anwendung; in der DLL; die große Nummer zusätzlich **im Dateinamen** `SichttestSteuerung1.dll` | beim Laden (`sts_oeffne`) | DLL |
| **Produkt** — Tag des Repos, etwa `v0.3.6` | `Solution.json`, Git-Tag | gar nicht zur Laufzeit; das ist der **Pin** der Anwendung | — |

**Was die große, was die kleine Nummer ändert**

- **Kleine Nummer** (P und S): es kommt etwas dazu, nichts ändert seine Bedeutung. P: neue
  Nachrichten, neue Felder. S: neue Aufrufe, neue Felder am Ende einer Struktur, neue Statuswerte.
- **Große Nummer**: etwas Bestehendes ändert Form oder Bedeutung oder fällt weg.
- Pflicht dazu, auf beiden Seiten: unbekannte Felder werden überlesen; eine unbekannte Nachricht
  wird mit `unbekannt` beantwortet, nie mit einem Abbruch.

**Regel zur Verträglichkeit**

- **S:** Die DLL nimmt eine Anwendung an, wenn die große Nummer gleich ist und die kleine der
  Anwendung nicht über der der DLL liegt. Sonst `STS_UNVERTRAEGLICH` mit Klartext
  (`sts_letzter_fehler`); die Anwendung läuft ohne Steuerung weiter. Durch die große Nummer im
  Dateinamen kann eine unverträgliche DLL gar nicht erst geladen werden.
- **P:** Der **Tester** trägt die Last, denn er ist der Teil, der einmal je Rechner aktuell
  gehalten wird, während jede Anwendung ihren eigenen, älteren Pin hat. Er spricht die aktuelle
  große Fassung und die vorige. Gewählt wird die höchste gemeinsame.
  - Gegenstelle **älter**, gleiche große Fassung: was ihr fehlt, antwortet sie mit `unbekannt`;
    der Tester zeigt den Handgriff als Text.
  - Gegenstelle **älter als die vorige große Fassung**: `abgelehnt` mit dem Hinweis, den Pin der
    Anwendung zu heben. Der Lauf geht ohne Steuerung weiter.
  - Gegenstelle **neuer** als der Tester (große Fassung): `abgelehnt` mit dem Hinweis, den Tester
    zu erneuern. Der Lauf geht ohne Steuerung weiter.
- **In keinem Fall hält eine Unverträglichkeit den Lauf an.** Sie steht sichtbar in der
  Statuszeile des Testers und im Testlog (`steuerung: { anwendung, version, produkt, p, s, zustand }`).
  Der Block steht im Testlog seit `v0.3.3` (bis dahin nur im Konzept): `zustand` ist `verbunden`
  oder `getrennt` — dann bleiben die Angaben der zuletzt verbundenen Anwendung stehen — oder
  `abgelehnt`; dann trägt der Block statt der Angaben den `grund`. Er nennt die Anwendung der
  Liste. Hat sich in einem Lauf nie eine Anwendung gemeldet, fehlt er.

Die **Aktionen** tragen keine eigene Nummer von dieser Seite: ihre Namen gehören der Anwendung,
und der Tester erfährt bei jedem Verbinden, welche es gibt.

## 6. Was eine Liste trägt

Eine **Aktion** ist: Name, Argumente (Schlüssel = Wert), dazu der **Text**, der den Handgriff für
einen Menschen beschreibt. Der Text ist immer da — er ist der Rückfall.

### 6.1 Markdown

Die Liste bleibt lesbar wie heute. Eine Aktion ist ein **Code-Stück, das mit `aktion:` beginnt**;
es steht im Text des Punkts, zu dem es gehört:

```markdown
**Anwendung:** LumiViz · **Exe:** `out\build\…\LumiViz.exe`

## M. Der shape der Zeile MASTER bleibt an seiner Zeit

- [ ] **M0 Vorbereiten:** Im Panel Composer **Load...** → `asset\sichttest\composer_A_master.lvcomp`.
      Dann **Composer on** an, **Edit** an, einmal **Fit** klicken. Du siehst: Länge 1:00 …
      `aktion: komposition_laden datei="asset\sichttest\composer_A_master.lvcomp"`
      `aktion: composer an` `aktion: edit an`
- [ ] **M1 Der shape ist zu hören und zu sehen:** *Tun:* Abspielmarke auf 0:20 setzen, Play …
      `aktion: abspielmarke zeit=0:20`
```

- Form: `` `aktion: <name> [wert] [schlüssel=wert …]` ``; Werte mit Leerzeichen in `"…"`. Ein
  einzelner Wert ohne Schlüssel kommt als Argument `wert` an.
- **Vorbereitung:** ein Punkt, dessen Titel mit **Vorbereiten** beginnt (das Format der neuen
  LumiViz-Listen, `LumiViz/.claude/handover/Sichttest_Composer_1_Master-Zeilen.md`). Er bekommt
  kein Urteil, sondern **▶ Ausführen** und **Weiter →**; seine Aktionen laufen der Reihe nach.
  Steht er vor dem ersten Abschnitt, gilt er für die ganze Liste.
- **Je Schritt:** die Aktionen eines gewöhnlichen Punkts laufen **nicht von selbst**, sondern auf
  den Knopf **▶ Herstellen** — der Mensch entscheidet, ob er den Handgriff selbst macht (oft ist
  gerade der Handgriff das, was geprüft wird).
- **Neu laden:** jeder Punkt zeigt zusätzlich **↺ Vorbereitung**, das die Vorbereitung seines
  Abschnitts wiederholt („Hast du etwas verstellt, lade die Vorlage einfach neu").
- Das Werkzeug nimmt die `aktion:`-Stücke aus dem angezeigten Text heraus; in jedem anderen
  Markdown-Betrachter stehen sie als unauffälliger Code am Ende des Punkts.
- **Verweis:** `` `aktion: @G0` `` führt die Aktionen des Punkts mit der Kennung `G0` derselben
  Liste aus, an dieser Stelle der Reihe; eigene Aktionen dürfen davor und danach stehen. So
  schreibt „wie G0, aber Edit an" nur `` `aktion: @G0` `aktion: edit an` `` und läuft nicht
  auseinander. Ein Verweis auf eine unbekannte Kennung oder im Kreis wird beim Laden der Liste
  gemeldet und wie eine unbekannte Aktion behandelt (§6.3).
- `**Anwendung:**` im Vorspann nennt den Namen, unter dem sich die Anwendung meldet. Fehlt er,
  gilt die einzige verbundene Anwendung.

### 6.2 `*.testprotokoll.json`

Neu, allgemein:

```json
{
  "title": "…", "anwendung": "CommStudio",
  "vorbereitung": [ { "aktion": "datei_oeffnen", "mit": { "pfad": "examples/demo.project.json" },
                      "text": "Projekt demo öffnen" } ],
  "steps": [
    { "id": "db-01", "section": "A", "title": "…", "text": "…",
      "aktionen": [ { "aktion": "tab_zeigen", "mit": { "titel": "Datenbank" } } ] },
    { "id": "db-00", "kind": "prep", "title": "Vorbereiten", "text": "…", "aktionen": [ … ] }
  ]
}
```

**`wenn` und die Zustände der Anwendung** (ab `v0.4.0`, §3): eine Aktion der Nachbereitung kann
mit `"wenn": "<zustand>"` nennen, wofür sie da ist — `{ "aktion": "testdb_abbauen", "wenn":
"test_db" }`, in der `abbildung` als `"test_db.ende": "testdb_abbauen wenn=test_db"`. Sie gehört nur
zum Angebot, solange die Anwendung diesen Zustand nicht als abgeräumt gemeldet hat; ohne `wenn`,
oder wenn die Anwendung den Zustand nie meldet, gilt die Regel unten. Steht ein Zustand, zu dem
die offene Liste eine Nachbereitung hat, zeigt der Tester es als Zeile unter der Statuszeile —
„In CommStudio 0.4.0 steht noch: test_db (studiotest)" mit dem Knopf **Nachbereitung
ausführen** —, sofort nach dem Verbinden und ohne Dialog; beim Schließen bietet er sie dann in
jedem Fall an. Trägt ein Lauf schon eine Bewertung, hat er noch offene Schritte und meldet die
Anwendung den Zustand als abgeräumt, warnt dieselbe Zeile. Das Testlog trägt an der Wurzel
`zustaende { name: { steht, text } }`: was die Anwendung zuletzt gemeldet hat.

**`{fortgesetzt}`** (ab `v0.4.0`): steht dieser Wert in den Argumenten irgendeiner Aktion — in
Markdown `aktion: x lauf={fortgesetzt}`, in `vorbereitung`, `aktionen` oder `nachbereitung` —,
ersetzt ihn der Tester beim Aufruf durch wahr oder falsch (§6.4). Der vorbelegte Schlüssel der
`abbildung` (§7) bleibt gültig.

Dazu `"nachbereitung": [ … ]` an der Wurzel: Aktionen, die der Tester am Ende des Laufs anbietet
(bei FERTIG und beim Schließen), nie während er auf eine neu startende Anwendung wartet. Beim
Schließen bietet er sie an, wenn der Lauf eine Bewertung trägt **oder in dieser Sitzung eine Aktion
der Vorbereitung mit `ok` gelaufen ist** (ab `v0.3.6`, Befund CS L1 — sonst bliebe stehen, was die
Vorbereitung hergestellt hat); läuft die Vorbereitung nach einer Nachbereitung erneut, wird die
Nachbereitung wieder fällig. Im
Testlog stehen ihre ausgelösten Aktionen an der Wurzel unter `teardown_actions`, in derselben Form
wie `actions[]` eines Schritts.

Die **Felder des Comm Studio bleiben gültig**. Das Werkzeug kennt sie als Teil seines Formats;
**welche Aktion** ein Feld auslöst, steht in der Projektdatei unter `abbildung` (§7) — die Namen
in der Tabelle sind die des Comm Studio, nicht die des Werkzeugs. So werden sie abgebildet, damit
die zwölf vorhandenen Protokolle (11 in `UART/assets/testing/protocols/`, eines in
`UART/assets/v2/tester/`; 127 Schritte, gezählt von CS am 08.10.2026) nicht umgeschrieben werden:

| Feld heute | Beleg | wird zu |
|---|---|---|
| `setup.close_all_tabs` | `TestProtokollWindow.cpp:1227` | Vorbereitung `tabs_schliessen` (`STS_FRAGT_NACH` und `STS_NUR_ERSTLAUF`) |
| `setup.open[]` | `:1229`, `:1246–1254` | Vorbereitung `datei_oeffnen pfad=…` je Eintrag |
| Schritt `"tab"` und Link `[…](tab://Titel)` im Text | `:1733`, `:284`, `MainWindow.cpp:2251` | `tab_zeigen titel=…` |
| Schritt `"restart": true` | `:1741`, `MainWindow.cpp:2323` | `neustart` (`STS_BEENDET_ANWENDUNG`) |
| Link `[…](sql:SELECT…)` im Text (ohne `//`) | `:269–302`, `MainWindow.cpp:2294` | `sql_einfuegen sql=…`; die Zwischenablage füllt der Tester selbst |
| `test_db { name, seed[] }` | `:1258 ff.`, `:1421 ff.` | Vorbereitung `testdb_einrichten name=… seed=…` (läuft **vor** `setup`, `:1218–1221`), Nachbereitung `testdb_abbauen` (beide `STS_WARTET_AUF_MENSCH`) |
| Schritt `"kind": "prep"` | `:1097`, `:1746` | Vorbereitungs-Punkt ohne Urteil (wie 6.1) |
| Schritt `"areas": […]` | `:1126 ff.` | keine Aktion; wird gelesen und ins Log getragen (Nachtest-Indikator, §6.5) |
| Schlüssel mit `_` vorn | — | Kommentar, wird überlesen |

Die sieben Namen hat CS übernommen (Nachricht vom 08.10.2026, 22:05); sie gehören dem Comm
Studio. Einrichten und Abbauen der Test-DB samt aller Rückfragen bleiben im Studio.

**Testlog:** Das Werkzeug liest und schreibt zusätzlich, was das Studio heute schreibt, damit
alte Läufe lesbar bleiben: `history[]` je Schritt (`:1900–1908`), `setup` und `test_db` samt
`test_db.active` an der Wurzel, `pass_remark` in der Zusammenfassung.

**Die Test-DB** (Stufe 3 von §14; mit CS abgestimmt und von Patrik entschieden am 2026-10-10,
Sync `CS-20261010-1739-…`, im Tester ab `v0.3.5`):

- **`test_db.active`** ist ab `v0.4.0` abgeleitet: es folgt dem Zustand, den die Nachbereitung aus
  `test_db.ende` mit `wenn` nennt, sobald die Anwendung ihn je gemeldet hat. Meldet sie keinen
  (Köpfe S 1.0), gilt die Regel aus `v0.3.5`: `true` nach einem `ok` der Aktion aus `test_db`,
  `false` nach einem `ok` der aus `test_db.ende`.
- Damit das stimmt, antwortet `testdb_einrichten` **nie `ok` ohne Test-DB**: `fehler` bei »Nein«,
  bei »Ohne Test-DB« und bei jedem Fehlschlag (die Vorbereitung endet dort, §6.3); `ok` sonst,
  auch beim Rückfall auf SQLite. Zahlen und Probleme des Impfens stehen im Antworttext.
- `testdb_abbauen` fragt nicht selbst (der Tester fragt vor der Nachbereitung); ohne aktive
  Test-DB antwortet es `ok`. Klemmt das Wegräumen, bleibt es bei `ok` mit dem Problem im Text —
  die ursprüngliche Verbindung ist dann wiederhergestellt.
- Über den vorbelegten Schlüssel **`fortgesetzt`** (§7) erfährt `testdb_einrichten`, ob der Lauf
  fortgesetzt wird: dann verwendet das Studio eine aktive Test-DB still weiter, ohne erneutes
  Impfen. Bei einem neuen Lauf mit noch aktiver Test-DB fragt es „frisch oder weiterverwenden".
- Die Test-DB bleibt über `neustart` eingestellt; verbunden wird im Studio, nicht im Tester.

### 6.3 Unbekannte Aktion, keine Verbindung, Fehler

Für alle Fälle dieselbe Regel: **der Lauf hält nie an, der Handgriff erscheint als Text.**

| Fall | Was der Tester tut |
|---|---|
| keine Anwendung verbunden | Knöpfe ▶ sind grau; der Text des Punkts steht da wie heute |
| Aktion steht nicht in der Liste aus `hallo` | schon beim Laden der Liste: Hinweis „3 Aktionen kennt LumiViz 0.4.0 nicht" mit den Namen; am Punkt: „von Hand:" + Text |
| Antwort `unbekannt` / `ungueltig` / `fehler` | Meldung der Anwendung in Rot am Punkt, darunter „von Hand:" + Text |
| keine Antwort in der Frist | wie Fehler, Text „keine Antwort nach 10 s" |
| der Tester hat die Anwendung gestartet (`start`, §7), aber in der Frist kommt kein `hallo` | eigene Meldung: „gestartet, aber nicht verbunden — läuft die Anwendung schon ohne `--testing`?" (LumiViz lässt je Exe nur eine Instanz zu; ein zweiter Start endet sofort, `LumiViz/…/Application.cpp:354–383`) |
| Aktion mit `fragt_nach` (schließt Tabs, verwirft Ungespeichertes) | der Tester fragt einmal je Vorbereitung, was gleich passiert (wie heute `runEnvironmentSetup`, `TestProtokollWindow.cpp:1235–1243`) |

Jede ausgelöste Aktion steht mit Name, Status und Text im Testlog des Schritts (`actions[]`).
Das Urteil bleibt immer beim Menschen — eine gescheiterte Aktion macht keinen Punkt zu Fail.

**Mehrere Aktionen an einem Punkt** laufen der Reihe nach, und die Reihe **endet bei der ersten,
die scheitert** (beim Bauen festgelegt, 2026-10-09): die folgenden laufen nicht, denn sie bauen
meist auf der gescheiterten auf. Der Tester nennt die gescheiterte Aktion und zeigt den Handgriff.
Status im Testlog: von der Anwendung `ok`, `fehler`, `unbekannt`, `ungueltig`; vom Tester `frist`
(keine Antwort), `getrennt` (keine Verbindung), `ausgelassen` (`nur_erstlauf` beim Fortsetzen)
und `abgebrochen` (der Mensch hat bei `wartet_auf_mensch` abgebrochen).

### 6.4 Fortsetzen eines Laufs

Wird ein Lauf fortgesetzt (neuer Start des Testers, oder die Anwendung hat sich neu verbunden),
bietet der Tester die Vorbereitung erneut an, lässt dabei aber jede Aktion mit `STS_NUR_ERSTLAUF`
aus: es wird nur hergestellt, nie abgeräumt (heute `offerMissingSetupTabs`,
`TestProtokollWindow.cpp:570`). Aktionen mit `STS_FRAGT_NACH` laufen, nach der Rückfrage. Das setzt voraus, dass die übrigen Aktionen der Vorbereitung
wiederholbar sind — `datei_oeffnen` des Comm Studio antwortet `ok`, wenn die Datei schon offen
ist, `testdb_einrichten`, wenn die Test-DB schon aktiv ist.

**Woran der Tester das Fortsetzen erkennt** (beim Bauen festgelegt, 2026-10-09): der Lauf trägt
schon mindestens eine Bewertung (Pass, Pass mit Befund, Fail oder übersprungen). Ein Lauf ohne
Bewertung gilt als erster Lauf, auch wenn die Vorbereitung schon einmal gelaufen ist.

**Welchen Lauf der Tester beim Öffnen einer Liste nimmt** (ab `v0.3.4`, Befunde CS B1, B3, B6,
Entscheid Patrik 2026-10-10): den jüngsten der Liste, **solange er offene Schritte hat**
(`summary.open` des Testlogs über null — dieselbe Regel wie im eingebauten Tester des Comm Studio,
`TestProtokollWindow.cpp:901–916`). Ist der jüngste abgeschlossen, beginnt ein neuer Lauf; die
Kopfzeile sagt es, und der abgeschlossene bleibt unverändert. Sortiert wird nach dem Namen, der
den Zeitstempel trägt.

**Der Build hängt am Urteil** (ab `v0.4.0`, Entscheid Patrik 2026-10-10): jeder bewertete Schritt
trägt `build { exe, exe_timestamp }` vom Zeitpunkt der Bewertung — die Exe der verbundenen
Anwendung aus `hallo`, sonst die der Liste oder `start.exe` des Projekts; der Stempel ist ISO.
Wird ein Urteil zurückgenommen, fällt sein `build` weg. Der Block `build` an der Wurzel nennt den
zuletzt benutzten Build; ohne Verbindung bleibt er beim Fortsetzen stehen, bis wieder bewertet
wird. Ein Urteil ohne eigenen Stempel (aus einem Lauf vor `v0.4.0` oder aus dem Comm Studio)
bekommt beim Fortsetzen den, den der Lauf bis dahin an der Wurzel nannte — so bleibt ein Pass vom
Juni ein Pass gegen den Build vom Juni, auch wenn der Lauf im Oktober weitergeht.

**Verwaiste Urteile** (ab `v0.4.0`): Texte, Links, `areas` und die Schrittmenge kommen beim
Fortsetzen aus der aktuellen Liste. Trägt das Testlog ein Urteil, eine Bemerkung oder Bilder zu
einem Schritt, den die Liste nicht mehr kennt, bleibt er mit `"verwaist": true` in `steps[]`,
steht am Ende unter „Nicht mehr in der Liste", lässt sich ansehen, nicht bewerten, und zählt weder
in der Summe noch im Indikator.

**Regeln fürs Testlog** (Befunde CS B2, B4, B5; seit dem 2026-10-10 Regel, §14.1): ein Report je
Lauf, `<lauf>.report.md`, bei jedem Schreiben; die erzeugte Vorbereitung `V0` steht in `steps[]`
als `kind: prep`; `summary.open` zählt Vorbereitungen nie. Der eingebaute Tester des Comm Studio
hält es an allen drei Stellen anders; solange es ihn gibt, wird ein Lauf in dem Tester beendet, in
dem er begonnen wurde.

### 6.5 Ablage

Ohne Projektdatei schreibt das Werkzeug wie heute nach `<Ordner der Liste>/sichttest-logs/`. Mit
Projektdatei (§7) nennt jeder Listenordner seine Ablage; dorthin gehen Testlog, Report,
Screenshots und die Ergebnis-DB. (Ein früherer Entwurf sah je Listenordner eine Datei
`sichttest.ordner.json` vor; sie ist in der Projektdatei aufgegangen.)

Die **Kennung eines Schritts** ist je Ablage eindeutig; die letzten Ergebnisse eines Schritts
werden je Ablage über alle Protokolle gelesen (`TestProtokollWindow.cpp:1197–1201`), damit ein
verschobener Schritt seine Historie behält.

## 7. Die Projektdatei `.sichttest/sichttest.projekt.json`

**Eine Datei je Anwendung, im Repo der Anwendung, versioniert.** Sie trägt alles, was der Tester
über das Projekt wissen muss und was sich ändern kann, ohne dass jemand übersetzt.

**Ort und Root (P2, Patrik 2026-10-09):** Die Datei liegt im Ordner `.sichttest` im Root des
Projekts. **Alle Pfade in der Datei gelten ab dem Root** — dem Ordner über `.sichttest` —, nicht
relativ zur Datei. (Bis `v0.2.0` lag die Datei lose im Repo, und die Pfade galten relativ zu ihr.)

```json
{
  "schema": 1,
  "anwendung": "CommStudio",
  "start": { "exe": "Build/x64/Release/comm_studio/comm_studio.exe", "argumente": ["--testing"] },
  "listen": [
    { "ordner": "assets/testing/protocols", "ablage": "Studio-Testlogs" },
    { "ordner": "assets/v2/tester",         "ablage": "Studio-Testlogs/v2" }
  ],
  "gewichtung": {
    "hand": [ "assets/testing/test-gewichtung.json", "assets/v2/tester/test-gewichtung.json" ],
    "git":  [ "assets/testing/gitGewichtung.json",   "assets/v2/tester/gitGewichtung.json" ]
  },
  "abbildung": {
    "setup.close_all_tabs": "tabs_schliessen",
    "setup.open":           "datei_oeffnen pfad",
    "tab":                  "tab_zeigen titel",
    "restart":              "neustart",
    "sql":                  "sql_einfuegen sql",
    "test_db":              "testdb_einrichten name seed fortgesetzt",
    "test_db.ende":         "testdb_abbauen wenn=test_db"
  }
}
```

| Schlüssel | Bedeutung | fehlt er |
|---|---|---|
| `schema` | Fassung dieser Datei; unbekannte Schlüssel werden überlesen | gilt als 1 |
| `anwendung` | der Name, unter dem sich die Anwendung meldet (`sts_konfig.anwendung`) | der Tester nimmt die einzige verbundene Anwendung |
| `start` | wie der Tester die Anwendung startet, **wenn sich in seiner Sitzung noch keine gemeldet hat**. Hatte sich eine gemeldet (Absturz, Neustart), startet er die Exe aus deren `hallo` — sonst holte er nach dem Absturz einer Debug-Exe die Release-Exe | er startet sie nicht, der Mensch tut es |
| `listen[]` | Listenordner (oder einzelne Listen), je mit `ablage` und optional `muster` (Dateimuster, etwa `"Sichttest_Composer_*.md"` — ohne es nimmt das Werkzeug jede `*.md` mit Schritten). Ein genannter Ordner, den es nicht gibt, wird still übergangen (frischer Klon ohne `.claude/`) | der Root des Projekts; Ablage `sichttest-logs/` im Root |
| `gewichtung { hand[], git[] }` | Dateien des Nachtest-Indikators in **zwei Schichten**, die verschieden gerechnet werden: `hand` trägt je Bereich `weight`, `changed`, `note`, dazu `retest_steps` und `auto_weight`; `git` trägt `changed`, `commit`, `file` und hebt nur auf `auto_weight` (`TestProtokollWindow.cpp:1026–1083`, `:1148–1170`). Innerhalb einer Schicht gilt die Reihenfolge der Liste. Eine genannte Datei, die fehlt, wird still übergangen (`gitGewichtung.json` ist rechnerlokal). Format und Pflege bleiben bei der Anwendung | kein Indikator |
| `abbildung` | welche Aktion ein Feld des JSON-Formats auslöst (§6.2): der Name der Aktion, danach die Schlüssel, die als Argumente unverändert aus dem Feld mitgehen (ein Array wie `seed` bleibt ein Array; was die Zeile nicht nennt, geht nicht mit). `tab` und `sql` gelten auch für die Links `tab://…` und `sql:…` im Text. **Vorbelegt ist der Schlüssel `fortgesetzt`** (ab `v0.3.5`): er ist kein Feld der Liste, der Tester füllt ihn beim Aufruf mit `true`, wenn der Lauf schon eine Bewertung trägt (§6.4), sonst mit `false`. Ein Stück `wenn=<zustand>` ist kein Schlüssel: es nennt den Zustand der Anwendung, für den die Aktion da ist (§6.2, ab `v0.4.0`) | das Feld wird gelesen und als Text gezeigt |

Für LumiViz genügt:

```json
{ "schema": 1, "anwendung": "LumiViz",
  "start": { "exe": "out/build/windows-ninja-release-clang/exec/LumiViz/bin/Release/LumiViz.exe",
             "argumente": ["--testing"] },
  "listen": [ { "ordner": ".claude/handover", "muster": "Sichttest_Composer_*.md" } ] }
```

Die Aktionen von LumiViz (fünfzehn, von LV benannt am 08.10.2026, Sync-Nachricht
`LV-20261008-2244-…` mit Datei:Zeile): `komposition_laden datei`, `komposition_schliessen`,
`composer an|aus`, `edit an|aus`, `fit`, `abspielmarke zeit`, `wiedergabe start|pause|stopp`,
`schleife an|aus`, `sync_kette an|aus`, `panel_zeigen name`, `preset_laden datei`,
`karaoke_laden datei`, `karaoke an|aus`, `titel_laden datei [spielen=ja]`,
`player start|pause|stopp`. Vier davon ersetzen Ungespeichertes und tragen `STS_FRAGT_NACH`;
keine öffnet einen Dialog, keine braucht eine spätere Fertigmeldung. Die Namen gehören LumiViz.
Gebaut sind seit dem 09.10.2026 die ersten vier (`komposition_laden`, `composer`, `edit`, `fit`).
`komposition_laden` lädt ohne Dialog und **hält**: Composer aus, Stelle 0:00, nichts läuft;
`composer an` schaltet danach ein (Entscheid Patrik L1 bei LV, gemeldet in
`LV-20261009-1905-…`).

**Wie sie gefunden wird**

- Start über die Anwendung: `hallo` nennt den Pfad der Exe. Der Tester sucht von dort **aufwärts**
  bis zum ersten Ordner `.sichttest`, der eine `sichttest.projekt.json` enthält (so findet er
  heute schon `.claude/handover`). `sts_konfig.projekt_datei` setzt den Pfad ausdrücklich, falls
  die Exe außerhalb des Repos liegt.
- **Wechsel der Listen bei `hallo`** (beim Bauen festgelegt, 2026-10-09): Meldet sich eine
  Anwendung, deren Projektdatei eine andere ist als die gerade offene, öffnet der Tester die
  Listen ihres Projekts — auch wenn er von Hand mit anderen Listen gestartet war. Er wechselt
  nicht, solange Aktionen laufen, und nicht, wenn die Anwendung keine Projektdatei hat.
- Start von Hand: `Sichttest <projektdatei>` oder `Sichttest <ordner>` — im zweiten Fall sucht er
  vom Ordner aufwärts. Ohne Projektdatei verhält er sich wie heute.
- **Eine ausdrücklich genannte Projektdatei** (`Sichttest <projektdatei>`,
  `sts_konfig.projekt_datei`) muss ebenfalls in einem Ordner `.sichttest` liegen; ihr Name ist
  frei (im Aufruf von Hand muss er auf `.projekt.json` enden, daran erkennt der Tester sie). Root
  ist der Ordner über `.sichttest`, wie bei der gesuchten Datei. Liegt sie woanders, lädt der
  Tester sie nicht, öffnet keine Liste und meldet: „Die Projektdatei <pfad> liegt nicht in einem
  Ordner .sichttest." (Patrik 2026-10-09)
- Eine Projektdatei, die sich nicht laden lässt (falscher Ort, nicht lesbar, kein JSON), wird
  gemeldet und nicht umgangen: der Tester zeigt dann auch die Listen des Ordners nicht.

**Was die Datei zusammenführt:** ein Ort für Listen, Ablage, Gewichtung und Start; derselbe für
einen von der Anwendung und einen von Hand gestarteten Tester; und die Namen der Aktionen bleiben
bei der Anwendung — das Werkzeug kennt das JSON-Format, aber keinen Aktionsnamen irgendeiner
Anwendung. Eine dritte Anwendung schreibt eine solche Datei und meldet Aktionen an; an
Werkzeug, DLL und Schnittstelle ändert sich dafür nichts.

### 7.1 Wo der Tester bei der Anwendung liegt

`Sichttest.exe` bringt **sein eigenes Qt** mit. Es darf deshalb **nicht neben der Exe der
Anwendung liegen**: dort liegt deren Qt, und beide Sätze heißen `Qt6Core.dll` (Comm Studio baut
im Geschäft mit Qt 6.8.2, zu Hause mit 6.10.1 — `UART/CLAUDE.md`, Tabelle „Qt (auto-probed)";
LumiViz als Debug-Build lädt `Qt6Cored.dll`).

Suchregel der DLL für den Tester, in dieser Reihenfolge: Umgebungsvariable `SICHTTEST_EXE` (für
die Entwicklung am Werkzeug) → `<Ordner der DLL>\sichttest\Sichttest.exe` (§11 E2).

## 8. Laden nur bei `--testing`

Die Anwendung wird **nicht gegen die DLL gelinkt**. Der Kopf `sichttest_steuerung.hpp` lädt
`SichttestSteuerung1.dll` zur Laufzeit (`LoadLibrary`, Adressen der Aufrufe einzeln geholt), und
zwar nur dort, wo die Anwendung es verlangt — im Zweig `--testing`. Geladen wird mit **vollem
Pfad** aus dem Ordner der Exe, nie über den Suchpfad (so lädt das Comm Studio heute schon
`pl1000.dll`, `UART/core/PicoLogApi.cpp:165`).

- Ein gewöhnlicher Start lädt nichts und öffnet nichts. Das ist durch den Aufbau gegeben, nicht
  durch eine Abfrage, die jemand vergessen kann.
- Fehlt die DLL, sagt `--testing` das in einem Satz, und die Anwendung läuft weiter.
- Die Anwendung braucht zum **Bauen nur die Köpfe**; DLL und Tester sind Laufzeit-Beigaben. Eine
  Import-Bibliothek und ein Linker-Schalter für verzögertes Laden entfallen.
- **Welche DLL geladen ist**, sagen `Sitzung::fassung()` und `Steuerung::fassung()` (ab `v0.3.1`):
  S, die Spanne von P und das Produkt. Die Datei `VERSION` liegt nur in der Wurzel des Pakets,
  nicht neben der Exe (Befund LV, Entscheid Patrik 2026-10-09).
- Der Kopf `sichttest_steuerung.hpp` bindet `<windows.h>` ein und setzt davor
  `WIN32_LEAN_AND_MEAN` und `NOMINMAX`, falls sie fehlen; beide gelten dann für den Rest der
  einbindenden Datei. Deshalb gehört der Kopf in **eine** `.cpp` und nicht in einen Kopf der
  Anwendung (Hinweis LV).

**Fehlt das Paket, fehlen auch die Köpfe** (andere Plattform, E3; oder kein Netz beim ersten
Holen). Die Anwendung übersetzt dann ohne Steuerung. Ist das Paket da, setzt
`craft_package_deploy` (§10) am Target neben dem Include-Pfad das Define `SICHTTEST_VORHANDEN=1`:

```cpp
#ifdef SICHTTEST_VORHANDEN
#  include <sichttest_steuerung_qt.hpp>
#endif
```

Alles, was die Steuerung benutzt, steht hinter `#ifdef SICHTTEST_VORHANDEN` an **einer** Stelle
der Anwendung (dem Anmelden der Aktionen). Das Define kommt aus derselben Funktion wie der
Include-Pfad, in LumiViz wie im Comm Studio; welcher Zweig gilt, steht damit im Configure-Log
(Vorschlag CC, Wunsch CS — ein erster Entwurf mit `__has_include` ist dafür aufgegeben).

## 9. Bezug in den Anwendungen

Die zwei Anwendungen bauen **verschieden**: LumiViz mit CMakeCraft (`LumiViz/cmakecraft.pin`),
das Comm Studio mit eigenem CMake ohne CMakeCraft (`UART/CMakeLists.txt`, `UART/CMakePresets.json`,
kein `cmakecraft.pin`; MSVC aus Visual Studio 2026). Der Bezug darf deshalb nicht an CMakeCraft hängen.

**Was eine Version liefert** (ein Paket je Tag `vX.Y.Z`):

```
sichttest-vX.Y.Z-win64/
├── include/   sichttest_steuerung.h · sichttest_steuerung.hpp · sichttest_steuerung_qt.hpp
├── bin/       SichttestSteuerung1.dll
├── sichttest/ Sichttest.exe mit seinem Qt
└── VERSION    drei Zeilen: produkt=0.3.6 · s=1.0 · p=1
```

Gebaut wird das Paket nur als Release, ohne `.pdb`. Verbindlich stehen S und P im Kopf
`sichttest_steuerung.h`; die Datei `VERSION` entsteht aus der Schablone `packaging/VERSION.in`
(`@VERSION@` aus `Solution.json`, S und P von Hand), und der Selbsttest hält S und P der
Schablone gegen den Kopf. Die Produktversion steht nicht im Kopf: DLL und Tester bekommen sie
beim Bauen als Define `STS_PRODUKT` aus `Solution.json`, die Anwendung fragt sie mit
`sts_fassung()` ab.

**Pin:** jede Anwendung nennt in einer Datei `sichttest.pin` Version, Adresse und Prüfsumme
(Vorbild `cmakecraft.pin`). Die Prüfsumme ist die der **veröffentlichten** Datei — sie ändert
sich mit jedem Schnüren, auch bei gleichem Inhalt:

```cmake
set(SICHTTEST_VERSION "v0.3.6")
set(SICHTTEST_URL     "https://github.com/PatrikNeunteufel/SichtTest_Helper/releases/download/${SICHTTEST_VERSION}/sichttest-${SICHTTEST_VERSION}-win64.zip")
set(SICHTTEST_SHA256  "<64 Hex-Zeichen>")
set(SICHTTEST_FALLBACK_PATHS "../SichtTest_Helper/out/package")
```

Gesucht wird in dieser Reihenfolge: `SICHTTEST_LOCAL_DIR` (Entwicklung) → Zwischenspeicher
`.externals/sichttest/<version>/` → Herunterladen mit Prüfsumme → Fallback-Pfade.

**Ohne Netz baut die Anwendung weiter.** Zwei Fälle:

- Das Paket liegt schon im **Zwischenspeicher** (nach dem ersten Holen): kein Netz nötig, keine
  Warnung; Köpfe, Define und Laufzeitdateien sind da.
- Das Paket liegt **weder im Zwischenspeicher, noch lässt es sich holen:** es gibt eine Warnung
  (`[CraftPackage]`, W304), die Köpfe fehlen, das Define `SICHTTEST_VORHANDEN` wird nicht
  gesetzt, und es wird nichts kopiert. Die Anwendung übersetzt dann ohne Steuerung (§8), und
  `--testing` läuft ohne sie.

Das Comm Studio lädt bisher nichts
beim Configure aus dem Netz; für es ist das Holen neu, das Verteilen nicht (es legt Ordner schon
heute nach dem Bau neben die Exe, `UART/gui/Comm_Studio/CMakeLists.txt:672–713`).

Entschieden ist das fertige Paket je Tag (§11 E1). Wie CMakeCraft es schnürt und bezieht: §10.

## 10. CMakeCraft

Stand: **CMakeCraft v0.10.0** (Tag `25165cf`, 2026-10-09), in diesem Projekt und in LumiViz
gepinnt. Bis v0.9.2 konnte CMakeCraft weder ein Paket schnüren noch ein fertiges beziehen; beides
hat CC für diesen Zweck gebaut, statt dass ein Projekt es mit einem Behelf überbrückt. Referenz:
`CMakeCraft/docs/de/guide/references/Packages.md`.

**In diesem Projekt (bauen und schnüren)**

| Bedarf | Stand | Wie |
|---|---|---|
| Bibliothek als DLL, ohne Qt | geht | `"type": "SHARED"`, keine Externals; AUTOMOC und Qt-Link bekommen nur Targets, die `Qt6` nennen |
| Exportierte C-Schnittstelle | geht mit eigenem Makro | CMake setzt bei einer DLL `<Target>_EXPORTS`; `STS_API` im Kopf stützt sich darauf |
| Große Fassung von S im Dateinamen | geht seit v0.10.0 | `"output_name": "SichttestSteuerung1"` an der Bibliothek; das Target heißt weiter `SichttestSteuerung` |
| Produktversion im Quelltext | geht seit v0.10.0 | `"defines": ["STS_PRODUKT=\"{version}\""]` an DLL und Tester. `{version}` ist dort die Version des **Targets** — sein eigenes Feld `version`, sonst die der Solution. Hier trägt kein Target ein eigenes, also kommt die der Solution an; das muss so bleiben, denn `{version}` im Paketnamen und `@VERSION@` in der Schablone sind immer die der Solution |
| **Paket je Tag schnüren** | **geht seit v0.10.0** | Block `packages` in `Solution.json`; das Target `package_sichttest` (nicht in ALL, nur Release) legt `out/package/sichttest-v<version>-win64/`, die `.zip` und die `.zip.sha256` an |
| DLL neben die eigene Exe kopieren | wird nicht gebraucht | `Sichttest.exe` lädt die DLL nie (§4); die Gegenproben laden sie über ihren Pfad |
| Versionsangabe in der DLL-Datei (Windows-Ressource) | geht nicht — verzichtbar | `sts_fassung()` liefert dasselbe |
| Feste C-Laufzeit nur für die DLL (`/MT`) | geht nicht — verzichtbar (§12) | Bibliotheken kennen keine `compile_options` |

Der Block in `Solution.json`:

```json
"packages": [
  { "name": "sichttest", "archive": "sichttest-v{version}-win64", "config": "Release",
    "contents": [
      { "to": "include", "headers_of": "SichttestSteuerung",
        "files": ["sichttest_steuerung.h", "sichttest_steuerung.hpp", "sichttest_steuerung_qt.hpp"] },
      { "to": "bin", "binary_of": "SichttestSteuerung" },
      { "to": "sichttest", "output_dir_of": "Sichttest", "exclude": ["*.pdb", "*.ilk"] } ],
    "version_file": "packaging/VERSION.in" } ]
```

`output_dir_of` nimmt den Ordner der Exe mit allen Unterordnern, also auch das, was windeployqt
dort ablegt. Das Hochladen als Release bleibt Handarbeit von Patrik (`GitHub_Einrichtung.md`,
Teil B).

**In den Anwendungen (beziehen und verteilen)**

| Bedarf | Stand | Wie |
|---|---|---|
| Fertiges Paket in gepinnter Version holen | **geht seit v0.10.0** | External mit `"archive": true` und `"pin": "sichttest.pin"`; Reihenfolge und Prüfsumme wie in §9 |
| Include-Pfad und Define, ohne zu linken | geht | `include_dirs`, `define` — beides gilt **nur am Target, das das External nennt** (PRIVATE), nicht an denen, die davon abhängen |
| DLL und den Ordner des Testers neben die Exe legen | geht | `runtime { files, dirs }`, je Konfiguration; abschaltbar je Target mit `external_options` |
| Dasselbe ohne CMakeCraft (Comm Studio) | geht | die eigenständige Datei `CMakeCraftPackage.cmake` (`craft_package_fetch`, `craft_package_deploy`), als unveränderte Kopie im Repo |

Zwei Dinge, die eine Anwendung wissen muss:

- **Die Kopie neben die Exe läuft nur, wenn die Exe gebaut wird.** Nach einem Wechsel der
  Paketversion die Exe neu bauen, sonst bleiben DLL und Tester daneben die alten.
- **Der Zwischenspeicher gilt über die Version, nicht über die Prüfsumme.** Wird unter derselben
  Version neu geschnürt, den Ordner `.externals/sichttest/<version>/` löschen.

**Gemessen am 2026-10-09:** Schnüren hier (Ninja, clang, Release) und bei CC in einem
Probeprojekt auch mit Ninja Multi-Config; Schnüren mit MSVC hat niemand gesehen. Beziehen in LumiViz mit
Release-clang, Debug-MSVC und Testing-MSVC, über den Fallback-Pfad und durch Herunterladen von
der Release-Adresse auf GitHub. **Nicht gemessen:** ein Lauf mit getrenntem Netz; die DLL unter
MSVC gebaut; der Bezug im Comm Studio.

Mindestversion von CMake: 3.26.

## 11. Entscheide (Patrik)

| # | Entscheid | Stand |
|---|---|---|
| E1 | **Bezug:** Jede Version liefert ein fertiges Paket (Köpfe, DLL, Tester mit seinem Qt). Die Pakete liegen als Release am Git-Tag auf GitHub; die Anwendung nennt die Version in `sichttest.pin` und holt das Paket beim Configure. Für die Entwicklung zeigt `SICHTTEST_LOCAL_DIR` auf einen lokalen Stand. | entschieden 2026-10-08 |
| E2 | **Ort des Testers:** Er liegt im Paket und wird als Unterordner `sichttest\` neben die Exe der Anwendung gelegt, mit seinem eigenen Qt. Die DLL findet ihn dort. | entschieden 2026-10-08 |
| E3 | **Plattform:** Die Steuerung gibt es zuerst nur unter Windows. Die Schnittstelle ist plattformneutral; auf anderen Plattformen fehlt die DLL, und `--testing` läuft ohne Steuerung. | entschieden 2026-10-08 |
| E4 | **Laden:** Die Anwendung linkt nicht gegen die DLL. Der Kopf lädt sie zur Laufzeit im Zweig `--testing`; fehlt sie, läuft die Anwendung ohne Steuerung weiter. | entschieden 2026-10-08 |
| E5 | **Kanal:** Der Tester lauscht. Die DLL verbindet sich bei `--testing` und startet den Tester, falls keiner läuft. Nach einem Neustart der Anwendung verbindet sie sich erneut, der Lauf geht beim selben Schritt weiter. | entschieden 2026-10-08 |
| E6 | **Umzug:** Es zieht alles um außer der Test-DB, in den fünf Stufen von §14. Zuerst wird die Steuerung gebaut und LumiViz angebunden (§13, Schritte 1 bis 4), darin Stufe 1 (alle Felder des Comm Studio lesen, Ablage je Ordner). Danach folgen die Stufen 2 bis 5. `TestProtokollWindow` entfällt nach Stufe 4; bis dahin laufen beide Tester nebeneinander. | entschieden 2026-10-08 |
| V1 | **Vorgabe:** Was der Tester über ein Projekt wissen muss (Pfade zu Listen, Ablage, Start), steht in Konfigurationsdateien und nicht im Quelltext der Projekte; die Schnittstelle soll für die heutigen Projekte passen und für künftige nicht gleich geändert werden müssen. Umgesetzt als Projektdatei (§7). | Patrik 2026-10-08 |
| P2 | **Ort der Projektdatei:** Sie liegt in jedem Projekt unter `.sichttest/sichttest.projekt.json` im Root; die Pfade darin gelten ab dem Root (dem Ordner über `.sichttest`); der Tester sucht aufwärts bis zum ersten Ordner `.sichttest`, der die Datei enthält. Auch eine ausdrücklich genannte Datei muss in einem Ordner `.sichttest` liegen, ihr Name ist frei. Ohne `listen` gilt der Root. S und P ändern sich nicht (§7). | entschieden 2026-10-09, ab `v0.3.0` |
| E7 | **Sichtbarkeit:** Das Repo `SichtTest_Helper` wird auf GitHub öffentlich angelegt, unter dem privaten Konto wie LumiViz. Der Bezug lädt das Release-Archiv ohne Anmeldung. Einrichten und Veröffentlichen: `GitHub_Einrichtung.md`. | entschieden 2026-10-08 |

## 12. Stellungnahme zu den Hinweisen von LV

| Hinweis | Stellung | Begründung |
|---|---|---|
| Keine Qt-Typen über die Grenze | **übernommen, verschärft:** gar keine C++-Typen | Schon ohne Qt bindet eine C++-Grenze beide Seiten an denselben Compiler, dieselbe Laufzeitbibliothek und dieselbe Bauart (Debug/Release legen `std::string` verschieden an). Hier treffen clang (dieses Projekt, `CMakeCache.txt`), MSVC aus VS 2022/2026 und Debug-Builds aufeinander. |
| Kein Qt in der DLL | **übernommen** | Der genannte Grund (zwei Qt in einem Prozess) trifft zu, und er tritt schon bei *einer* Qt-Fassung ein: eine Release-DLL zieht `Qt6Core.dll` in einen Debug-Prozess, der `Qt6Cored.dll` geladen hat. Dazu: eine mit Qt 6.10 gebaute DLL läuft nicht gegen das Qt 6.8.2 des Comm Studio im Geschäft. Die DLL verlangt damit nur noch die Release-Laufzeit von Visual C++, die jede dieser Anwendungen mit ihrem Qt ohnehin mitbringt; weil kein Speicher den Besitzer wechselt (§3), darf sie auch in einem Debug-Prozess laufen. |
| C-Schnittstelle plus kleiner Kopf für C++/Qt | **übernommen**, als zwei Köpfe | `…​.hpp` ohne Qt (lädt die DLL, §8), `…_qt.hpp` für die Zustellung im GUI-Thread. Beide werden in der Anwendung übersetzt und sind damit immer mit deren Qt und Compiler gebaut. |
| Aktionen im GUI-Thread | **übernommen**, als Abholen mit Wecker (§3) | „Zustellen" durch die DLL ginge nur, wenn sie die Ereignisschleife kennte — also mit Qt. |
| Zwei Versionen | **übernommen**, dazu die Produktversion als Pin (§5) | Geprüft werden zwei Nummern, gepinnt wird eine dritte; ohne die Trennung würde jeder Fehlerbehebung im Tester ein Pin-Wechsel in allen Anwendungen folgen. |
| DLL nur bei `--testing` geladen | **übernommen**, durch Laden zur Laufzeit (§8) | |
| `EinzelInstanz` muss nicht der Kanal werden | **bestätigt: sie wird es nicht** | Sie ist `QObject` mit `QLocalServer` in der Anwendung (`EinzelInstanz.hpp:36`, `:74`) und trägt einen Dateipfad; die Steuerung braucht Anfrage und Antwort, und die Anwendung soll gerade **nicht** lauschen (§4). |

**Abweichung von der Idee:** Dort öffnet die Anwendung den Kanal („`Anwendung.exe --testing`
startet den Tester daneben und öffnet den Kanal"). Hier lauscht der Tester (§4; von Patrik
entschieden, E5).

## 13. Reihenfolge der Umsetzung

Gebaut wird erst, wenn alle Teilnehmer im Leerlauf sind und Patrik freigibt.

| Schritt | Wer | Was | Ergebnis |
|---|---|---|---|
| 0 | Patrik | Git-Repo hier anlegen, Tag `v0.1.0` (heutiger Stand ohne Steuerung) | Ausgangspunkt |
| 1 | SH | DLL (Kanal, P 1, S 1.0), die drei Köpfe, eine kleine Gegenprobe-Anwendung ohne Qt; `Sichttest.exe` lauscht; `--selbsttest` prüft Verbinden, Aufruf, unbekannte Aktion, Fristablauf, Ablehnung | im eigenen Repo vollständig prüfbar, ohne LV und CS |
| 2 | SH | Listen: `aktion:` in Markdown, `vorbereitung`/`aktionen` in JSON, Abbildung der Felder des Comm Studio; Knöpfe ▶ und Rückfall auf Text | Tag `v0.2.0`, erstes Paket |
| 3 | CC, dann SH | CMakeCraft v0.10.0 (`packages`, External-Art `archive`, `CMakeCraftPackage.cmake`); SH hebt den Pin, schnürt das erste Paket, Patrik lädt es als Release hoch | Anwendungen können beziehen |
| 4 | LV | `LumiViz.exe --testing`, die ersten Aktionen (LV-1), Listen mit `aktion:`; eigene Kopie `projects/exec/Sichttest` entfernen (LV-2) | LumiViz gesteuert |
| 5 | SH, CS | Umzug aus `TestProtokollWindow` in den Stufen von §14 (E6) | Werkzeug kann, was das Comm Studio braucht |
| 6 | CS | `TestProtokollWindow` entfernen — erst nach Stufe 4 von §14 | Comm Studio gleich aufgebaut |

## 14. Umzug aus dem Comm Studio (Vorschlag CS, von Patrik entschieden: E6)

Von allem, was der eingebaute Tester mehr kann, hängt **nur die Test-DB** an der Datenbank des
Studios (`TestDbSetup.cpp`: `Database`, `BomImport`, `SecureStore`); sie bleibt dort, hinter zwei
Aktionen. Die Ergebnis-DB `testergebnisse.sqlite` ist eine eigene SQLite-Datei
(`TestResultsDb.cpp:29–39`) und kann umziehen; der Tester braucht dafür Qt6Sql und das Plugin
`qsqlite` in seinem Paket.

| Stufe | Was das Werkzeug dann kann | Wirkung |
|---|---|---|
| 1 | Felder lesen ohne Wirkung (`tab`, `areas`, `kind: prep`, `restart`, `setup`, `test_db`, Links im Text); Projektdatei `sichttest.projekt.json` (§7) | alle 12 Protokolle laufen im Werkzeug wie heute, ohne Steuerung |
| 2 | Steuerung: `tab_zeigen`, `sql_einfuegen`, Vorbereitung, `neustart` | Sichttest ohne Datenbank gleichwertig (8 von 12 Protokollen) |
| 3 | Test-DB über `testdb_einrichten` / `testdb_abbauen` | die übrigen 4 Protokolle |
| 4 | Ergebnis-DB im Schema des Studios weiterschreiben; Nachtest-Indikator, »Unkritische überspringen«, Befund-Archiv, Statistik | **danach kann `TestProtokollWindow` entfallen** |
| 5 | Protokoll-Register (unbekannt, verschollen, neu lokalisieren) | Komfort |

**Stand 2026-10-10:** Stufe 1 seit `v0.2.0`; Stufe 2 seit dem 10.10. im Comm Studio (fünf
Aktionen, `--testing` gehört dem Werkzeug); Stufe 3 mit `v0.3.5`/`v0.3.6` und im Comm Studio am
Bildschirm gelaufen. Stufe 4 ist abgestimmt (§14.1); **die Teile A und B sind gebaut**, A auch
im Comm Studio und dort am Bildschirm gelaufen (S 1.1, P 1.1; §3, §4, §6.2, §6.4); beides
unveröffentlicht.

### 14.1 Stufe 4 in vier Teilen (abgestimmt mit CS, Entscheide Patrik 2026-10-10)

**Maßstab (Vorgabe Patrik):** nichts wird nur für das Comm Studio gebaut. Was ein Projekt mit
einer Projektdatei und Listen in der allgemeinen Form hat, bekommt dasselbe. Belege und Wortlaut
der Abstimmung: Sync `CS-20261010-1926-…`, `SH-20261010-1937-…`.

Je Teil schnürt SH das Paket lokal, das Comm Studio baut über `SICHTTEST_LOCAL_DIR` dagegen
(§9), dann die Probe am Bildschirm gegen eine Kopie der Ablage. Veröffentlicht wird einmal, wenn
Stufe 4 fertig ist, als `v0.4.0` (Entscheid Patrik 2026-10-10); bis dahin bleibt der Pin der
Anwendungen auf `v0.3.6`. Bis Teil D gilt: ein Lauf wird in dem Tester beendet, in
dem er begonnen wurde.

**Teil A — Testlog und Zustand** (kleine Fassungen **S 1.1** und **P 1.1**; Anwendungen mit S 1.0
laufen weiter, §5)

- **Zustand der Anwendung:** Die Anwendung meldet benannte Zustände, die sie herstellt und wieder
  abräumt (`sts_melde_zustand(sitzung, name, steht, text)`), bei jeder Änderung und einmal je
  Zustand nach dem Verbinden. Der Tester führt den Merker nicht mehr selbst, er leitet ihn ab:
  `test_db.active` im Testlog ist, was die Anwendung zuletzt gemeldet hat. Eine Nachbereitung
  nennt mit `"wenn": "<zustand>"` (in der `abbildung` an `test_db.ende`), wofür sie da ist, und
  wird angeboten, wenn dieser Zustand steht — auch wenn er schon beim Verbinden steht (Rest eines
  alten Laufs). Ohne `wenn` gilt die Regel aus `v0.3.6`. Verlangt eine Liste einen Zustand, der
  beim Fortsetzen nicht steht, warnt der Tester.
- Die Anwendung erfährt das Ende des Testers.
- **`{fortgesetzt}`** als Wert-Platzhalter, den jede Aktion nennen kann (Markdown, `vorbereitung`,
  `aktionen`, `abbildung`); der vorbelegte Schlüssel aus `v0.3.5` bleibt gültig.
- **Regeln fürs Testlog:** die erzeugte Vorbereitung `V0` steht in `steps[]` als `kind: prep`;
  `summary.open` zählt Vorbereitungen nie; ein Report je Lauf, `<lauf>.report.md`, bei jedem
  Schreiben (Befunde CS B4, B5, B2).
- **Der Build-Stempel hängt am Urteil:** jeder bewertete Schritt trägt Exe und Stempel vom
  Zeitpunkt der Bewertung; `build` an der Wurzel nennt den zuletzt benutzten Build (Befunde B7, L2).
- **Verwaiste Urteile bleiben:** ein Urteil zu einem Schritt, den es in der Liste nicht mehr gibt,
  bleibt im Testlog und steht am Ende als „nicht mehr in der Liste"; in Summe und Indikator zählt
  es nicht.

**Teil B — Ergebnis-DB und Statistik**

- `testergebnisse.sqlite` je Ablage, gespiegelt bei jedem Schreiben; das Testlog bleibt die
  Wahrheit. Das Schema ist das des Comm Studio (`testlauf`, `testschritt`, `protokoll`,
  `UART/gui/Comm_Studio/TestResultsDb.cpp:56–90`) mit zwei Änderungen: **`log_datei` trägt den
  Dateinamen ohne Ordner** (das Werkzeug stellt Bestandszeilen einmalig um), und `testschritt`
  bekommt ergänzte Spalten für den Build am Urteil. Vorbereitungen werden nicht gespiegelt.
  Sobald `TestProtokollWindow` entfällt, gehört das Schema dem Werkzeug; Spalten werden nur ergänzt.
- Der Tester bringt dafür Qt6Sql und das Plugin `qsqlite` mit.
- Statistik (Läufe je Build, Problemschritte) und das Öffnen eines älteren Laufs.

Gebaut (2026-10-10): die DB wird beim ersten Schreiben in einer Ablage angelegt. Ihre einmalige
Umstellung kürzt jede `log_datei` mit Ordner auf den Namen; tragen zwei Zeilen danach denselben
(die Ablage wurde einmal kopiert und weiterbenutzt), bleibt die mit der höheren `lauf_id`, die
ältere geht samt ihren Schritten. Gesucht wird ein Lauf über den Namen ohne
Groß-/Kleinschreibung. Verwaiste Urteile werden gespiegelt, Vorbereitungen nicht. Die ergänzten
Spalten heißen `testschritt.build_exe` und `testschritt.build_timestamp`. Ein Fehler der DB hält
nichts an; er steht einmal im Fenster.

Die **Statistik** ist ein eigenes Fenster (Knopf **Statistik…**), kein Reiter, damit das
Testfenster schmal bleibt (Entscheid Patrik). Ein **älterer Lauf** öffnet daraus **nur zum
Ansehen**: nichts wird geschrieben, weder Testlog noch DB. Hat er offene Schritte, macht
**Diesen Lauf fortsetzen** ihn bearbeitbar; abgeschlossene Läufe werden erst mit Teil D wieder
bearbeitbar, weil bis dahin kein Befund-Archiv ein überschriebenes Urteil aufhebt.

**Teil C — Nachtest-Indikator**

- Dringlichkeit je Schritt, 0 bis 1: Vorbereitung 0 · in `retest_steps` genannt 1,0 · nie
  verifiziert 1,0 · letzter Fail 1,0 · sonst das höchste `weight` einer Area des Schritts, die nach
  dem letzten Pass geändert wurde. `skip` ist nicht verifiziert, `pass_remark` zählt als
  verifiziert. Die git-Schicht hebt nur auf `auto_weight` (Vorgabe 0,5), nie senkt sie.
- Verglichen wird gegen den Build-Stempel des Urteils; die Hand-Schicht tut das, wenn `changed`
  eine Uhrzeit trägt, bei reinem Datum gilt der Tagesvergleich.
- Schritt-Kennungen gelten je Ablage über alle Listen. Zwei Dateien je Schicht werden
  verschmolzen, die zweite überlagert die erste je Schlüssel.
- Schwellen: 🔴 ab 0,7 · 🟡 ab 0,3 · darunter unkritisch. »Unkritische überspringen« nimmt offene
  Schritte unter 0,3 und lässt sich zurücknehmen, ohne von Hand Übersprungene anzufassen.
- Im Fenster: Dringlichkeit je Liste in der Listenwahl, Begründung am Schritt.
- Der git-Hook, der die git-Schicht schreibt, zieht aus dem Comm Studio in dieses Projekt, liest
  die Pfade aus `gewichtung` der Projektdatei und kommt ins Paket. Das Format beider Dateien wird
  hier beschrieben, sobald Teil C gebaut ist.

**Teil D — Befund-Archiv**

- `history[]` je Schritt: `{ archived, result, remark, screenshots }`; archiviert wird über einen
  Knopf und von selbst beim Neubewerten, beim Ändern der Bemerkung und bei einem neuen Screenshot
  eines schon bewerteten Schritts. Die Bilder bleiben liegen, ihr Zähler läuft über das Archiv
  weiter. Der Report bekommt den Abschnitt Historie.

**Danach:** derselbe Lauf in beiden Testern, dieselben Zahlen; dann entfernt CS
`TestProtokollWindow`, `TestResultsDb` und `--resume-log=`.
