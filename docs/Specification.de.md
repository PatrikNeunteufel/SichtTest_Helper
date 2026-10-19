[English](Specification.md) · **Deutsch**

# Sichttest-Steuerung — Spezifikation

> **Übersetzung.** Maßgeblich ist die englische Fassung ([Specification.md](Specification.md)); bei
> Abweichung gilt sie. Dieses Dokument legt fest, wie das Werkzeug `Sichttest.exe` eine geprüfte
> Anwendung steuert: die C-Schnittstelle **S** der DLL, das Protokoll **P** zwischen DLL und
> Werkzeug, die Regeln der Versionen, die Listenformate, die Projektdatei, das Laden und das Paket.
> Es beschreibt die Produktversion **v0.4.0**, die Schnittstelle **S 1.1** und das Protokoll
> **P 1.1**.
>
> Eine Anwendung Schritt für Schritt anbinden: [Integration_Guide.de.md](Integration_Guide.de.md).
> Vorlagen: [templates/](../templates/). Was je Version dazukam: [CHANGELOG.de.md](../CHANGELOG.de.md).

## 1. Grundsätze

1. Das Werkzeug ist ein eigenes Programm. Es wird nicht als Bibliothek in die Anwendungen
   eingebunden.
2. Dieses Projekt baut eine **DLL**, die die ganze Schnittstelle zur Steuerung einer Anwendung
   bereitstellt. Die Anwendungen laden sie; Werkzeug, DLL und Anwendungen entwickeln getrennt
   weiter, geprüft wird die Verträglichkeit über Versionsnummern (§5).
3. Gesteuert wird eine Anwendung nur, wenn sie mit einem eigenen Argument gestartet wird, nach
   Konvention `<Anwendung>.exe --testing`.
4. Das Werkzeug kennt die Listenformate, aber **keinen Aktionsnamen irgendeiner Anwendung**.
   Aktionsnamen, ihre Bedeutung und alles Projektspezifische gehören der Anwendung: ihr Quelltext
   meldet die Aktionen an, ihre Projektdatei (§7) sagt, wo Listen und Ergebnisse liegen.
5. **Der Lauf hält nie an, und das Urteil fällt immer der Mensch.** Jede Aktion hat einen Text,
   der den Handgriff beschreibt; ohne Verbindung, oder wenn eine Aktion scheitert, bleibt dieser
   Text.

## 2. Die Teile und wem sie gehören

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
| `Sichttest.exe` | diesem Projekt | Listen, Bewertung, Testlog, Report; Kanal (lauscht), Aktionen auslösen, Handgriff als Text zeigen |
| `SichttestSteuerung<S>.dll` | diesem Projekt | Kanal, Protokoll P, Warteschlange; **kein Qt, keine C++-Typen an der Grenze** |
| `sichttest_steuerung.h` | diesem Projekt | die C-Schnittstelle S (einzige verbindliche Grenze) |
| `sichttest_steuerung.hpp` | diesem Projekt | kleiner Kopf für C++: lädt die DLL zur Laufzeit, RAII, Lambdas als Aktionen; **wird in der Anwendung übersetzt** |
| `sichttest_steuerung_qt.hpp` | diesem Projekt | Zusatz für Qt: stellt die Rückrufe im GUI-Thread zu, `QString`/`QJsonObject`-Bequemlichkeit; ebenfalls in der Anwendung übersetzt |
| Listenformate, Format der Projektdatei | diesem Projekt | §6, §7 |
| Aktionen (Namen, Wirkung, Schalter) | jeder Anwendung | werden zur Laufzeit angemeldet |
| Projektdatei, Listen, Gewichtungsdateien | jeder Anwendung | im Repo der Anwendung |

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
    const char* anwendung;       /* "MeineAnwendung" — der Name, den Listen und Projektdatei nennen */
    const char* version;         /* Version der Anwendung, landet im Testlog */
    const char* projekt_datei;   /* optional: Pfad zur Projektdatei in einem Ordner .sichttest; sonst Suchregel §7 */
} sts_konfig;

/* Fassungen der DLL, ohne Sitzung abfragbar. */
void       sts_fassung(uint16_t* s_major, uint16_t* s_minor,
                       uint16_t* p_major_min, uint16_t* p_major_max,
                       const char** produkt);                 /* "0.4.0" = Tag des Repos ohne v */

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
   abgelehnt ist. */
const char* sts_letzter_fehler(sts_sitzung* s);
/* Schreibt wartende Antworten und `tschuess` noch hinaus (höchstens 2 s), dann Ende. */
void       sts_schliesse(sts_sitzung* s);
```

**Was nicht in der Schnittstelle steht:** Pfade zu Listen, Ablage, Gewichtung, der Ort des Testers
und wie die Anwendung gestartet wird. Das alles steht in der **Projektdatei** (§7) und nicht im
Quelltext der Anwendung. Im Quelltext stehen nur der Name, die Version und die Aktionen — also
das, was nur der Quelltext wissen kann. Eine neue Anwendung braucht damit keine Änderung an der
Schnittstelle, und ein neuer Listenordner keine Übersetzung.

**Die vier Schalter einer Aktion** setzt die Anwendung beim Anmelden:

| Schalter | Bedeutung | Was der Tester tut |
|---|---|---|
| `STS_FRAGT_NACH` | verändert Zustand, der verloren gehen kann (Tabs schließen, Ungespeichertes verwerfen), fragt aber **nicht selbst** | fragt vorher, einmal je Vorbereitung — auch beim Fortsetzen. Die Rückfrage trägt den Haken „In dieser Sitzung nicht mehr fragen“: gesetzt und mit Ja beantwortet, fragt der Tester nicht mehr, bis er beendet wird; er gilt für alle Anwendungen und Listen |
| `STS_NUR_ERSTLAUF` | räumt ab, statt herzustellen; gehört nur an den Anfang eines neuen Laufs | lässt die Aktion beim **Fortsetzen** eines Laufs aus (§6.4) |
| `STS_WARTET_AUF_MENSCH` | die Aktion fragt **in der Anwendung** nach (Dialoge), ihre Dauer hängt am Menschen | keine Frist; zeigt „wartet auf die Anwendung“ mit **Abbrechen** (der Tester wartet dann nicht weiter, eine späte Antwort wird verworfen) |
| `STS_BEENDET_ANWENDUNG` | nach der Antwort endet die Anwendung oder startet neu | wertet das Ende der Verbindung nicht als Fehler, zeigt „Anwendung startet neu“ und wartet auf die neue Verbindung |

Eine Aktion trägt `STS_FRAGT_NACH` **oder** `STS_WARTET_AUF_MENSCH`, nie beide — gefragt wird an
einer Stelle, dort, wo der Text stimmt.

„Fragt vorher“ und „nur im ersten Lauf“ sind getrennt, weil sie nicht immer zusammenfallen: eine
Aktion, die alle Tabs schließt, trägt beide; eine Aktion, die eine Vorlage lädt und dabei
Ungespeichertes ersetzt, trägt `STS_FRAGT_NACH`, **ist** aber das Herstellen und muss beim
Fortsetzen laufen.

**Kein Rückruf vor `sts_verbinde()`.** Vorher gibt es keine Verbindung und damit keinen Aufruf;
danach laufen Rückrufe nur in `sts_pumpe()`. Die Anwendung bestimmt mit dem Zeitpunkt von
`sts_verbinde()` also, ab wann sie gesteuert werden kann (etwa erst nach dem Wiederherstellen
ihres Startzustands).

**Wiedereintritt.** Ein Rückruf darf einen modalen Dialog öffnen; der dreht die Ereignisschleife,
und der Wecker stellt `sts_pumpe()` erneut zu, während der erste Rückruf noch läuft. Festgelegt:
je Sitzung läuft **höchstens ein Rückruf zugleich**. Ein `sts_pumpe()` innerhalb eines Rückrufs
führt nichts aus und liefert 0; was wartet, kommt nach dessen Ende an die Reihe (die DLL weckt
dann erneut). Kein Aufruf wird zweimal ausgeführt. Der Tester schickt je Verbindung ohnehin nur
einen Aufruf zur Zeit.

**Rückrufe im GUI-Thread.** Die DLL liest den Kanal in einem eigenen Thread und legt jeden Aufruf
in eine Warteschlange. Ausgeführt wird **nur** in `sts_pumpe()`, das die Anwendung in ihrem
GUI-Thread ruft. Damit sie nicht pollen muss, ruft die DLL den **Wecker**; der Qt-Kopf setzt ihn
auf `QMetaObject::invokeMethod(…, Qt::QueuedConnection)`, sodass `sts_pumpe()` im nächsten
Durchlauf der Ereignisschleife läuft. Die DLL kennt dabei weder Qt noch eine Ereignisschleife.

**So sieht es in einer Qt-Anwendung aus** (mit `sichttest_steuerung_qt.hpp`):

```cpp
if (QCoreApplication::arguments().contains("--testing")) {
    m_steuerung = sichttest::Steuerung::lade("MeineAnwendung", version);   // lädt die DLL, sonst leer
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

**Die Klassen der Köpfe:** `sichttest::Sitzung` in `sichttest_steuerung.hpp` (ohne Qt; Aktionen
als Lambdas, die die Argumente als JSON-Text bekommen) und `sichttest::Steuerung` in
`sichttest_steuerung_qt.hpp` (Argumente als `QJsonObject`, Zustellung im GUI-Thread). Beide
entstehen über `lade(anwendung, version, …)` und sind leer, wenn die DLL fehlt oder die Fassung
nicht passt; beide kennen `aktion()`, `verbinde()`, `melde()`, `zustand()`, `letzterFehler()`,
`fassung()` und ab S 1.1 `meldeZustand()` und `beiEnde()`. Ergebnisse baut man mit
`sichttest::ok()`, `sichttest::fehler(text)` und `sichttest::ungueltig(text)`. Eine Ausnahme aus
einer Aktion wird zum Status `fehler` mit ihrem Text.

**Zustände der Anwendung (S 1.1).** Was eine Anwendung herstellt und wieder abräumt, kennt nur sie
selbst — eine Test-Datenbank kann aus einem alten Lauf noch stehen oder von Hand abgebaut sein.
Deshalb meldet die Anwendung solche Zustände mit Namen (`sts_melde_zustand`, in den Köpfen
`meldeZustand(name, steht, text)`), und der Tester führt keine eigene Vorstellung davon, was wahr
ist. Über `sts_bei_ende` (`beiEnde`) erfährt sie, dass der Tester gegangen ist. Eine Anwendung, die
mit den Köpfen S 1.0 übersetzt ist, läuft gegen die DLL S 1.1 unverändert weiter (§5).

**Bewusst nicht in Fassung 1:** Aktionen, die erst später fertig werden (Antwort nach dem
Rückruf), Ereignisse der Anwendung als Bedingung eines Schritts. Beides lässt sich als kleine
Fassung (neue Aufrufe) nachtragen.

## 4. Kanal und Protokoll P (Werkzeug ↔ DLL)

**Der Tester lauscht, die DLL verbindet sich** — nicht umgekehrt.

- Die Anwendung öffnet nichts, worauf jemand von außen zugreifen könnte; sie baut bei
  `--testing` eine Verbindung **nach außen** auf.
- Der Tester überlebt Absturz und Neustart der Anwendung. Die neu gestartete Anwendung verbindet
  sich wieder, der Lauf geht beim selben Schritt weiter, ohne dass der Tester etwas über den
  Neustart wissen muss.
- **Beenden:** Verabschiedet sich die Anwendung geordnet (`tschuess`) und hat sie den Tester
  gestartet (`--steuerung`), zeigt der Tester unter der Statuszeile „Die Anwendung … wurde
  beendet“ mit dem Knopf **Tester beenden**. Er schließt nicht von selbst und fragt nicht modal;
  verbindet sich die Anwendung wieder, verschwindet der Hinweis. Kein Hinweis bei einem Abriss,
  nach einer Aktion mit `STS_BEENDET_ANWENDUNG` und bei einem von Hand gestarteten Tester. Beendet
  der Mensch den Tester, läuft die Anwendung weiter; sie erfährt es über `sts_bei_ende` (S 1.1).
- Mehrere Anwendungen können zugleich verbunden sein; eine Liste nennt ihre Anwendung (§6), der
  Tester wählt die Verbindung nach dem Namen aus `sts_konfig.anwendung`.

**Ablauf beim Start** (`Anwendung.exe --testing`):

1. Anwendung lädt die DLL (§8), meldet ihre Aktionen an, ruft `sts_verbinde()`.
2. Die DLL sucht den Kanal des Testers. Lauscht keiner, startet sie `Sichttest.exe` (Suchregel
   §7.1) mit `--steuerung` und versucht es bis zu 10 s erneut.
3. `hallo` → `willkommen` (oder `abgelehnt`). Aus `hallo` kennt der Tester den Pfad der Exe; von
   dort findet er die Projektdatei (§7) und öffnet deren Listen. Danach Aufrufe.

Die DLL liest keine Projektdatei und kennt keine Pfade außer dem des Testers.

Ein Tester, der von Hand gestartet wird, lauscht ebenfalls; eine Liste ohne Aktionen verhält sich
genau wie ohne DLL.

**Kanal:** unter Windows eine benannte Pipe `\\.\pipe\sichttest-<Benutzer>`, nur für den
angemeldeten Benutzer zugänglich (`QLocalServer::UserAccessOption`). Dass nur ein Tester je
Benutzer lauscht, sichert ein benannter Mutex `Global\sichttest-<Benutzer>`, den der Tester vor
dem Lauschen nimmt. Ein zweiter Tester lauscht nicht, sagt das in der Statuszeile und arbeitet
ohne Steuerung. Die DLL benutzt die Windows-API direkt; der Tester benutzt `QLocalServer` (unter
Windows dieselbe Pipe). Andere Plattformen: §10, D3.

Zwei Umgebungsvariablen liest **nur die DLL**, beide für Entwicklung und Selbsttest:
`SICHTTEST_KANAL` ersetzt den Namen der Pipe, `SICHTTEST_EXE` den Pfad des Testers, den die DLL
startet (sonst `<Ordner der DLL>\sichttest\Sichttest.exe`, §7.1). Der Tester selbst liest keine von
beiden; er lauscht immer auf `sichttest-<Benutzer>`.

**Rahmen:** 4 Byte Länge (little endian) + ein JSON-Objekt in UTF-8. Die DLL bringt dafür einen
eigenen kleinen JSON-Leser mit; die Argumente einer Aktion reicht sie ungeprüft als Text weiter.
Welche Nachricht ein Rahmen trägt, steht im Feld **`nachricht`** (`{"nachricht":"hallo", …}`).

| Nachricht | Richtung | Inhalt |
|---|---|---|
| `hallo` | DLL → Tester | `p_min`, `p_max` (große Fassungen von P, die die DLL spricht), `s`, `produkt`, `anwendung`, `version`, `exe`, `pid`, `projekt_datei` (falls gesetzt), `aktionen[]` (`name`, `beschreibung`, `parameter`, `schalter[]`). Die Schalter stehen als **Wörter**: `fragt_nach`, `nur_erstlauf`, `wartet_auf_mensch`, `beendet_anwendung` |
| `willkommen` | Tester → DLL | gewählte Fassung `p`, Produktversion des Testers als `tester` |
| `abgelehnt` | Tester → DLL | Grund als Text (keine gemeinsame Fassung) |
| `aktionen` | DLL → Tester | nachgemeldete Aktionen |
| `aufruf` | Tester → DLL | `id`, `aktion`, `argumente{}`, `basis` (Root des Projekts nach §7, sonst der Ordner der Liste). Der Tester reicht jeden Wert **unverändert** durch, auch Pfade; aufgelöst wird in der Anwendung (die etwa nur Ziele unterhalb ihres Repos zulassen kann), `basis` ist nur ein Angebot |
| `antwort` | DLL → Tester | `id`, `status` (`ok`, `fehler`, `unbekannt`, `ungueltig`), `text` |
| `meldung` | DLL → Tester | freier Text |
| `zustand` | DLL → Tester | ab P 1.1: `name`, `steht` (wahr/falsch), `text` — der letzte Stand eines Zustands, bei jeder Änderung und nach jedem Verbinden |
| `tschuess` | beide | geordnetes Ende |

Eine Nachricht, die die DLL nicht kennt, beantwortet sie mit `antwort`, Status `unbekannt` (mit der
`id`, falls die Nachricht eine trug); der Tester überliest Unbekanntes (§5).

**Gemeldet und ausgehandelt wird von P nur die große Fassung.** `sts_fassung`, `p_min` / `p_max`
im `hallo` und `p=` in `VERSION` nennen sie als Spanne; die kleine Fassung steht allein im Kopf
(`STS_P_MINOR`). Eine DLL, die P 1.1 spricht, meldet deshalb „1 bis 1“ — eine kleine Fassung
ergänzt nur (hier die Nachricht `zustand`), und wer sie nicht kennt, überliest sie.

Der Tester wartet je Aufruf 10 s (in der Liste je Aktion änderbar); für Aktionen mit
`STS_WARTET_AUF_MENSCH` gilt keine Frist (§3). Bleibt die Antwort aus, gilt §6.3: der Handgriff
erscheint als Text, eine verspätete Antwort wird verworfen.

**Vordergrund:** Vor jedem Aufruf erlaubt der Tester der Anwendung, sich nach vorn zu holen
(`AllowSetForegroundWindow` mit der `pid` aus `hallo`) — sonst bliebe ein Dialog der Aktion
hinter dem Tester. Nach vorn holen muss sich die Anwendung selbst.

## 5. Drei Nummern, zwei davon werden geprüft

| Nummer | Wo sie steht | Wann geprüft | Von wem |
|---|---|---|---|
| **P** — Protokoll Tester ↔ DLL, `groß.klein` | in Tester und DLL einkompiliert | beim Verbinden (`hallo`) | Tester |
| **S** — Schnittstelle DLL ↔ Anwendung, `groß.klein` | `STS_S_MAJOR/MINOR` im Kopf, also in der Anwendung; in der DLL; die große Nummer zusätzlich **im Dateinamen** `SichttestSteuerung1.dll` | beim Laden (`sts_oeffne`) | DLL |
| **Produkt** — Tag des Repos, etwa `v0.4.0` | `Solution.json`, Git-Tag | gar nicht zur Laufzeit; das ist der **Pin** der Anwendung | — |

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
  `zustand` ist `verbunden` oder `getrennt` — dann bleiben die Angaben der zuletzt verbundenen
  Anwendung stehen — oder `abgelehnt`; dann trägt der Block statt der Angaben den `grund`. Er
  nennt die Anwendung der Liste. Hat sich in einem Lauf nie eine Anwendung gemeldet, fehlt er.

Die **Aktionen** tragen keine eigene Nummer von dieser Seite: ihre Namen gehören der Anwendung,
und der Tester erfährt bei jedem Verbinden, welche es gibt.

## 6. Listen

Eine **Aktion** ist: Name, Argumente (Schlüssel = Wert), dazu der **Text**, der den Handgriff für
einen Menschen beschreibt. Der Text ist immer da — er ist der Rückfall.

### 6.1 Markdown

Die Liste bleibt als gewöhnliche Checkliste lesbar. Eine Aktion ist ein **Code-Stück, das mit
`aktion:` beginnt**; es steht im Text des Punkts, zu dem es gehört:

```markdown
**Anwendung:** MeineAnwendung · **Exe:** `out\build\MeineAnwendung.exe`

## M. Die Marke bleibt an ihrer Zeit

- [ ] **M0 Vorbereiten:** Im Panel Composer **Laden...** → `vorlagen\szene_A.vorlage`.
      Dann **Bearbeiten** an, einmal **Einpassen** klicken. Du siehst: Länge 1:00 …
      `aktion: vorlage_laden datei="vorlagen\szene_A.vorlage"`
      `aktion: bearbeiten an` `aktion: einpassen`
- [ ] **M1 Die Marke ist zu hören und zu sehen:** *Tun:* Abspielmarke auf 0:20 setzen, Play …
      `aktion: abspielmarke zeit=0:20`
```

- Form: `` `aktion: <name> [wert] [schlüssel=wert …]` ``; Werte mit Leerzeichen in `"…"`. Ein
  einzelner Wert ohne Schlüssel kommt als Argument `wert` an.
- **Vorbereitung:** ein Punkt, dessen Titel mit **Vorbereiten** beginnt. Er bekommt kein Urteil,
  sondern **▶ Ausführen** und **Weiter →**; seine Aktionen laufen der Reihe nach. Steht er vor dem
  ersten Abschnitt, gilt er für die ganze Liste.
- **Je Schritt:** die Aktionen eines gewöhnlichen Punkts laufen **nicht von selbst**, sondern auf
  den Knopf **▶ Herstellen** — der Mensch entscheidet, ob er den Handgriff selbst macht (oft ist
  gerade der Handgriff das, was geprüft wird).
- **Neu laden:** jeder Punkt zeigt zusätzlich **↺ Vorbereitung**, das die Vorbereitung seines
  Abschnitts wiederholt („Hast du etwas verstellt, lade die Vorlage einfach neu“).
- Das Werkzeug nimmt die `aktion:`-Stücke aus dem angezeigten Text heraus; in jedem anderen
  Markdown-Betrachter stehen sie als unauffälliger Code am Ende des Punkts.
- **Verweis:** `` `aktion: @G0` `` führt die Aktionen des Punkts mit der Kennung `G0` derselben
  Liste aus, an dieser Stelle der Reihe; eigene Aktionen dürfen davor und danach stehen. So
  schreibt „wie G0, aber Bearbeiten an“ nur `` `aktion: @G0` `aktion: bearbeiten an` `` und läuft
  nicht auseinander. Ein Verweis auf eine unbekannte Kennung oder im Kreis wird beim Laden der
  Liste gemeldet und wie eine unbekannte Aktion behandelt (§6.3).
- `**Anwendung:**` im Vorspann nennt den Namen, unter dem sich die Anwendung meldet. Fehlt er,
  gilt die einzige verbundene Anwendung.

### 6.2 `*.testprotokoll.json`

Allgemeine Form:

```json
{
  "title": "…", "anwendung": "MeineAnwendung",
  "vorbereitung": [ { "aktion": "datei_oeffnen", "mit": { "pfad": "beispiele/demo.meinprojekt" },
                      "text": "Projekt demo öffnen" } ],
  "steps": [
    { "id": "db-01", "section": "A", "title": "…", "text": "…",
      "aktionen": [ { "aktion": "tab_zeigen", "mit": { "titel": "Datenbank" } } ] },
    { "id": "db-00", "kind": "prep", "title": "Vorbereiten", "text": "…", "aktionen": [ … ] }
  ],
  "nachbereitung": [ { "aktion": "testdb_abbauen", "wenn": "test_db", "text": "…" } ]
}
```

Ein Eintrag einer Aktion trägt `aktion`, `mit` (Argumente), `text` (Handgriff) und optional
`frist` (Sekunden) und `wenn` (siehe unten). Schlüssel mit `_` vorn sind Kommentare und werden
überlesen.

**`nachbereitung`** an der Wurzel: Aktionen, die der Tester am Ende des Laufs anbietet (bei FERTIG
und beim Schließen), nie während er auf eine neu startende Anwendung wartet. Beim Schließen bietet
er sie an, wenn der Lauf eine Bewertung trägt **oder in dieser Sitzung eine Aktion der
Vorbereitung mit `ok` gelaufen ist** (sonst bliebe stehen, was die Vorbereitung hergestellt hat);
läuft die Vorbereitung nach einer Nachbereitung erneut, wird die Nachbereitung wieder fällig. Im
Testlog stehen ihre ausgelösten Aktionen an der Wurzel unter `teardown_actions`, in derselben Form
wie `actions[]` eines Schritts.

**`wenn` und die Zustände der Anwendung** (§3): eine Aktion der Nachbereitung kann mit
`"wenn": "<zustand>"` nennen, wofür sie da ist — `{ "aktion": "testdb_abbauen", "wenn":
"test_db" }`, in der `abbildung` als `"test_db.ende": "testdb_abbauen wenn=test_db"`. Sie gehört
nur zum Angebot, solange die Anwendung diesen Zustand nicht als abgeräumt gemeldet hat; ohne
`wenn`, oder wenn die Anwendung den Zustand nie meldet, gilt die Regel oben. Steht ein Zustand, zu
dem die offene Liste eine Nachbereitung hat, zeigt der Tester es als Zeile unter der Statuszeile —
„In MeineAnwendung 1.2.0 steht noch: test_db (meinetestdb)“ mit dem Knopf **Nachbereitung
ausführen** —, sofort nach dem Verbinden und ohne Dialog; beim Schließen bietet er sie dann in
jedem Fall an. Trägt ein Lauf schon eine Bewertung, hat er noch offene Schritte und meldet die
Anwendung den Zustand als abgeräumt, warnt dieselbe Zeile. Das Testlog trägt an der Wurzel
`zustaende { name: { steht, text } }`: was die Anwendung zuletzt gemeldet hat.

**`{fortgesetzt}`**: steht dieser Wert in den Argumenten irgendeiner Aktion — in Markdown
`aktion: x lauf={fortgesetzt}`, in `vorbereitung`, `aktionen` oder `nachbereitung` —, ersetzt ihn
der Tester beim Aufruf durch wahr oder falsch (§6.4). Der vorbelegte Schlüssel der `abbildung`
(§7) bleibt gültig.

**Felder, die über `abbildung` zu Aktionen werden.** Das JSON-Format kennt einige Felder, die für
einen Schritt oder eine Liste etwas bedeuten, deren Aktion aber der Anwendung gehört. **Welche
Aktion** ein Feld auslöst, steht in der Projektdatei unter `abbildung` (§7); ohne Zeile dort wird
das Feld gelesen und als Text gezeigt.

| Feld | wird zu |
|---|---|
| `setup.close_all_tabs` | Vorbereitung: eine Aktion, typisch mit `STS_FRAGT_NACH` und `STS_NUR_ERSTLAUF` |
| `setup.open[]` | Vorbereitung: eine Aktion je Eintrag, der Eintrag als erstes Argument |
| Schritt `"tab"` und Link `[…](tab://Titel)` im Text | eine Aktion mit dem Titel als Argument; zusätzlich als Link **Tab: …** unter dem Titel des Schritts |
| Schritt `"restart": true` | eine Aktion, typisch mit `STS_BEENDET_ANWENDUNG` |
| Link `[…](sql:SELECT…)` im Text (ohne `//`) | eine Aktion mit der Abfrage als Argument; die Zwischenablage füllt der Tester selbst |
| `test_db { name, seed[] }` | Vorbereitung, läuft **vor** `setup`; mit `test_db.ende` als Nachbereitung |
| Schritt `"kind": "prep"` | Vorbereitungs-Punkt ohne Urteil (wie 6.1) |
| Schritt `"areas": […]` | keine Aktion; wird gelesen und ins Log getragen (Nachtest-Indikator, §6.7) |
| Schlüssel mit `_` vorn | Kommentar, wird überlesen |

**Links allgemein:** `tab` und `sql` sind kein Sonderfall. **Jeder Schlüssel der `abbildung` ist
zugleich ein Link** im Text eines Schritts — `[Text](<schlüssel>:<wert>)` löst die dort genannte
Aktion aus, der Wert geht als ihr erstes Argument mit (etwa `"datei": "datei_oeffnen pfad"` und
`[Vorlage](datei:configs/a.json)`). Ein Schema, das die `abbildung` nicht nennt, öffnet sich
außerhalb. Nur zweierlei bleibt an den Namen `tab` und `sql` hängen: `sql` füllt zusätzlich die
Zwischenablage, und beide zeigen ohne Zeile der `abbildung` den Handgriff, statt sich außerhalb zu
öffnen. Als Link taugt nur ein Schlüssel, der ein gültiges URL-Schema ist (Kleinbuchstaben,
Ziffern, `.`, `+`, `-`; also nicht `test_db`).

**`test_db.active` im Testlog** folgt dem Zustand, den die Nachbereitung aus `test_db.ende` mit
`wenn` nennt, sobald die Anwendung ihn je gemeldet hat. Meldet sie keinen (Köpfe S 1.0), ist es
`true` nach einem `ok` der Aktion aus `test_db` und `false` nach einem `ok` der aus
`test_db.ende`. Damit das stimmt, antwortet eine Aktion, die einen Zustand herstellt, **nie `ok`,
ohne ihn hergestellt zu haben** — `fehler`, wenn der Mensch ablehnt, und bei jedem Fehlschlag (die
Vorbereitung endet dort, §6.3), `ok` sonst; Einzelheiten gehören in den Antworttext. Ein Abbau, der
nichts abzubauen findet, antwortet `ok`. Über den vorbelegten Schlüssel `fortgesetzt` erfährt die
herstellende Aktion, ob der Lauf fortgesetzt wird, und kann einen stehenden Zustand ohne Rückfrage
weiterverwenden.

**Testlog älterer Läufe:** Das Werkzeug liest und schreibt zusätzlich die Felder `history[]` je
Schritt, `setup` und `test_db` samt `test_db.active` an der Wurzel und `pass_remark` in der
Zusammenfassung, damit Läufe früherer Tester lesbar bleiben.

### 6.3 Unbekannte Aktion, keine Verbindung, Fehler

Für alle Fälle dieselbe Regel: **der Lauf hält nie an, der Handgriff erscheint als Text.**

| Fall | Was der Tester tut |
|---|---|
| keine Anwendung verbunden | Knöpfe ▶ sind grau; der Text des Punkts steht da wie in jeder Liste |
| Aktion steht nicht in der Liste aus `hallo` | schon beim Laden der Liste: Hinweis „3 Aktionen kennt MeineAnwendung 1.2.0 nicht“ mit den Namen; am Punkt: „von Hand:“ + Text |
| Antwort `unbekannt` / `ungueltig` / `fehler` | Meldung der Anwendung in Rot am Punkt, darunter „von Hand:“ + Text |
| keine Antwort in der Frist | wie Fehler, Text „keine Antwort nach 10 s“ |
| der Tester hat die Anwendung gestartet (`start`, §7), aber in der Frist kommt kein `hallo` | eigene Meldung: „gestartet, aber nicht verbunden — läuft die Anwendung schon ohne `--testing`?“; typisch für eine Anwendung, die je Exe nur eine Instanz zulässt |
| Aktion mit `fragt_nach` (schließt Tabs, verwirft Ungespeichertes) | der Tester fragt einmal je Vorbereitung, was gleich passiert |

Jede ausgelöste Aktion steht mit Name, Status und Text im Testlog des Schritts (`actions[]`).
Das Urteil bleibt immer beim Menschen — eine gescheiterte Aktion macht keinen Punkt zu Fail.

**Mehrere Aktionen an einem Punkt** laufen der Reihe nach, und die Reihe **endet bei der ersten,
die scheitert**: die folgenden laufen nicht, denn sie bauen meist auf der gescheiterten auf. Der
Tester nennt die gescheiterte Aktion und zeigt den Handgriff. Status im Testlog: von der Anwendung
`ok`, `fehler`, `unbekannt`, `ungueltig`; vom Tester `frist` (keine Antwort), `getrennt` (keine
Verbindung), `ausgelassen` (`nur_erstlauf` beim Fortsetzen) und `abgebrochen` (der Mensch hat bei
`wartet_auf_mensch` abgebrochen).

### 6.4 Fortsetzen eines Laufs

Wird ein Lauf fortgesetzt (neuer Start des Testers, oder die Anwendung hat sich neu verbunden),
bietet der Tester die Vorbereitung erneut an, lässt dabei aber jede Aktion mit `STS_NUR_ERSTLAUF`
aus: es wird nur hergestellt, nie abgeräumt. Aktionen mit `STS_FRAGT_NACH` laufen, nach der
Rückfrage. Das setzt voraus, dass die übrigen Aktionen der Vorbereitung wiederholbar sind — eine
Aktion, die eine Datei öffnet, antwortet `ok`, wenn die Datei schon offen ist; eine, die eine
Test-Datenbank einrichtet, wenn sie schon aktiv ist.

**Woran der Tester das Fortsetzen erkennt:** der Lauf trägt schon mindestens eine Bewertung (Pass,
Pass mit Befund, Fail oder übersprungen). Ein Lauf ohne Bewertung gilt als erster Lauf, auch wenn
die Vorbereitung schon einmal gelaufen ist.

**Welchen Lauf der Tester beim Öffnen einer Liste nimmt:** den jüngsten der Liste, **solange er
offene Schritte hat** (`summary.open` des Testlogs über null). Ist der jüngste abgeschlossen,
beginnt ein neuer Lauf; die Kopfzeile sagt es, und der abgeschlossene bleibt unverändert. Sortiert
wird nach dem Namen, der den Zeitstempel trägt.

**Der Build hängt am Urteil:** jeder bewertete Schritt trägt `build { exe, exe_timestamp }` vom
Zeitpunkt der Bewertung — die Exe der verbundenen Anwendung aus `hallo`, sonst die der Liste oder
`start.exe` des Projekts; der Stempel ist ISO. Wird ein Urteil zurückgenommen, fällt sein `build`
weg. Der Block `build` an der Wurzel nennt den zuletzt benutzten Build; ohne Verbindung bleibt er
beim Fortsetzen stehen, bis wieder bewertet wird. Ein Urteil ohne eigenen Stempel (aus einem
älteren Lauf) bekommt beim Fortsetzen den, den der Lauf bis dahin an der Wurzel nannte — so bleibt
ein Pass vom Juni ein Pass gegen den Build vom Juni, auch wenn der Lauf im Oktober weitergeht.

**Zeitpunkt des Urteils:** neben `rated` (Tag und Uhrzeit ohne Jahr, zum Lesen) trägt jedes Urteil
`rated_at` (ISO, Ortszeit). Beim Zurücknehmen auf offen geht `rated_at` mit `rated`.

**Verwaiste Urteile:** Texte, Links, `areas` und die Schrittmenge kommen beim Fortsetzen aus der
aktuellen Liste. Trägt das Testlog ein Urteil, eine Bemerkung oder Bilder zu einem Schritt, den die
Liste nicht mehr kennt, bleibt er mit `"verwaist": true` in `steps[]`, steht am Ende unter „Nicht
mehr in der Liste“, lässt sich ansehen, nicht bewerten, und zählt weder in der Summe noch im
Indikator.

**Regeln fürs Testlog:** ein Report je Lauf, `<lauf>.report.md`, bei jedem Schreiben; die erzeugte
Vorbereitung `V0` steht in `steps[]` als `kind: prep`; `summary.open` zählt Vorbereitungen nie.

**Öffnen und Verbinden ändern kein Testlog**, auch nicht das eines fortgesetzten Laufs; geschrieben
wird erst nach einer Bewertung, einem Screenshot, einem Schrittwechsel oder beim Schließen.

### 6.5 Ablage

Ohne Projektdatei schreibt das Werkzeug nach `<Ordner der Liste>/sichttest-logs/`. Mit
Projektdatei (§7) nennt jeder Listenordner seine Ablage (`ablage`); dorthin gehen Testlog, Report,
Screenshots und die Ergebnis-DB.

Die **Kennung eines Schritts** ist je Ablage eindeutig; die letzten Ergebnisse eines Schritts
werden je Ablage über alle Listen gelesen, damit ein verschobener Schritt seine Historie behält.

### 6.6 Ergebnis-DB

Je Ablage führt das Werkzeug `testergebnisse.sqlite`, gespiegelt bei jedem Schreiben; **das
Testlog bleibt die Wahrheit**, die DB ist der Index dazu. Tabellen `testlauf`, `testschritt`,
`protokoll`. `log_datei` trägt den Dateinamen des Testlogs ohne Ordner. Spalten werden nur
ergänzt; bisher ergänzt: `testschritt.build_exe`, `testschritt.build_timestamp`,
`testschritt.rated_at`. Vorbereitungen werden nicht gespiegelt, verwaiste Urteile schon.

- Die DB wird beim ersten Schreiben in einer Ablage angelegt. Öffnet das Werkzeug eine bestehende
  DB, kürzt es einmalig jede `log_datei` mit Ordner auf den Namen; tragen zwei Zeilen danach
  denselben (eine Ablage wurde kopiert und weiterbenutzt), bleibt die mit der höheren `lauf_id`,
  die ältere geht samt ihren Schritten. Gesucht wird ein Lauf über den Namen ohne
  Groß-/Kleinschreibung. **Das Öffnen einer Ablage kann die DB also ändern** (Umstellung, ergänzte
  Spalten), nie aber ein Testlog.
- Ein Fehler der DB hält nichts an; er steht einmal im Fenster.
- **Ältere Testlogs einlesen** spiegelt alle `*.testlog.json` der Ablage, deren Name die DB nicht
  kennt — über alle Listen der Ablage, **nur auf diesen Klick**, nie beim Öffnen einer Liste, und
  ohne ein Testlog zu ändern. Eine Datei, die sich nicht als Testlog lesen lässt, wird übergangen
  und gezählt.
- Ein älterer Lauf öffnet aus der Statistik **nur zum Ansehen**: nichts wird geschrieben, weder
  Testlog noch DB. **Diesen Lauf fortsetzen** macht einen Lauf mit offenen Schritten bearbeitbar,
  **Diesen Lauf bearbeiten** einen abgeschlossenen; die Kopfzeile nennt ihn dann „wieder geöffnet“.

### 6.7 Nachtest-Indikator

**Wann es ihn gibt:** sobald die Projektdatei `gewichtung` nennt, auch leer — dann wirkt nur die
Historie (nie verifiziert, letzter Fail). Ohne den Schlüssel, und ohne Projektdatei, gibt es
keinen.

**Dringlichkeit je Schritt, 0 bis 1:** Vorbereitung 0 · in `retest_steps` genannt 1,0 · nie
verifiziert 1,0 · letztes Ergebnis Fail 1,0 · sonst das höchste `weight` einer Area des Schritts,
die nach dem letzten Pass geändert wurde. `skip` ist nicht verifiziert, `pass_remark` zählt als
verifiziert. Die git-Schicht hebt nur auf `auto_weight` (Vorgabe 0,5), nie senkt sie.

- Schwellen: 🔴 ab 0,7 · 🟡 ab 0,3 · darunter unkritisch.
- Bezug beider Schichten ist der Build am Urteil, sonst der des Laufs, sonst sein Start.
  Zeitstempel werden als Zeitpunkte verglichen, nicht als Text (die git-Datei trägt eine Zone, der
  Build-Stempel nicht; ohne Zone gilt Ortszeit). Die Hand-Schicht vergleicht tageweise, wenn
  `changed` ein reines Datum ist — gegen den Tag des Urteils (`rated_at`); ein Urteil ohne
  `rated_at` gilt am Start seines Laufs.
- „Das letzte Ergebnis“ eines Schritts ist das mit dem jüngsten Urteil, über alle Listen der Ablage
  (nach Schritt-Kennung), nicht das aus dem zuletzt begonnenen Lauf.
- **↷ Unkritische überspringen** setzt offene Schritte unter 0,3 nach einer Rückfrage auf `skip`,
  mit der Bemerkung „Auto-Skip (Nachtest-Indikator …): …“ und dem Merkmal `"auto_skip": true`;
  derselbe Knopf nimmt genau diese wieder auf, ohne von Hand Übersprungene anzufassen.
- Im Fenster: 🔴 und 🟡 nur an offenen Schritten; die Begründung in einer Zeile am Schritt; in der
  Listenwahl je Liste die Zahl der 🔴 und 🟡. Wird ein Lauf gegen einen anderen Build fortgesetzt
  als den, den sein Testlog nennt, sagt es die Kopfzeile.

**Die Hand-Datei** (`gewichtung.hand[]`): `areas { <name>: { weight, changed, note?, auto_weight?,
paths[]? } }`, `retest_steps { <kennung>: <grund> }`, `auto_weight` an der Wurzel. `changed` ist ein
Datum oder ein ISO-Zeitstempel; `paths` sind Muster (wie `fnmatch`) für die git-Schicht.
**Die git-Datei** (`gewichtung.git[]`): `areas { <name>: { changed, commit, file, commits } }`,
`changed` als ISO-Zeitstempel mit Zone. Sie ist rechnerlokal. Mehrere Dateien je Schicht werden in
der Reihenfolge der Liste verschmolzen: `areas` und `retest_steps` je Schlüssel, alles andere
überschrieben. Eine genannte Datei, die fehlt, wird still übergangen.

**Der Hook** `werkzeuge/gewichtung_autoupdate.py` (in diesem Repo `tools/`; post-commit und
post-merge, eingerichtet von `install_hooks.ps1`, braucht Python) nimmt `hand[i]` als Quelle der
Muster und schreibt `git[i]`; fehlt `git[i]`, schreibt er für `hand[i]` nichts. Er endet immer mit
0.

### 6.8 Befund-Archiv

`history[]` je Schritt: `{ archived, result, remark, screenshots, rated, build }`.

- **🗄 Archivieren** nimmt den Befund, wie er im Feld steht, leert Bemerkung und Bilder und lässt
  das Urteil stehen; ohne Bemerkung und Bild gibt es nichts zu archivieren.
- **Von selbst**, wenn sich der Text eines Schritts ändert, der nicht offen ist (ohne Rand
  verglichen) — beim Neubewerten, beim Sichern der Bemerkung (auch beim Schrittwechsel) und vor
  einem neuen Screenshot.
- Bei jedem Wechsel eines Urteils in ein anderes, auch beim Zurücknehmen auf offen: der Eintrag
  trägt dann Urteil, `rated`, `build` und die Bemerkung des alten Stands, auch wenn sie leer ist,
  und keine Bilder; Bemerkung und Bilder bleiben am Schritt, solange der Text gleich bleibt. Hat
  die geänderte Bemerkung den alten Befund schon abgeheftet, entsteht kein zweiter Eintrag. Ein
  Bild allein und dasselbe Urteil noch einmal archivieren nichts.
- Das nächste Bild bekommt eine Nummer über der höchsten, die der Schritt je trug, auch im Archiv.
- Am Schritt steht eine Zeile mit den jüngsten zwei Einträgen, alle im Tooltip; der Report nennt je
  Schritt alle, den jüngsten zuerst (Abschnitt **Historie**). Die Ergebnis-DB spiegelt `history`.

## 7. Die Projektdatei

**Eine Datei je Anwendung, im Repo der Anwendung, versioniert:**
`.sichttest/sichttest.projekt.json`. Sie trägt alles, was der Tester über das Projekt wissen muss
und was sich ändern kann, ohne dass jemand übersetzt. Vorlage:
[templates/.sichttest/sichttest.projekt.json](../templates/.sichttest/sichttest.projekt.json).

**Ort und Root:** Die Datei liegt im Ordner `.sichttest` im Root des Projekts. **Alle Pfade in der
Datei gelten ab dem Root** — dem Ordner über `.sichttest` —, nicht relativ zur Datei.

```json
{
  "schema": 1,
  "anwendung": "MeineAnwendung",
  "start": { "exe": "out/build/release/MeineAnwendung.exe", "argumente": ["--testing"] },
  "listen": [
    { "ordner": "tests/sichttest", "ablage": "tests/sichttest-logs" },
    { "ordner": "docs", "muster": "Sichttest_*.md", "ablage": "tests/sichttest-logs" }
  ],
  "gewichtung": {
    "hand": [ "tests/sichttest/test-gewichtung.json" ],
    "git":  [ "tests/sichttest/gitGewichtung.json" ]
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
| `listen[]` | Listenordner (oder einzelne Listen), je mit `ablage` und optional `muster` (Dateimuster, etwa `"Sichttest_*.md"` — ohne es nimmt das Werkzeug jede `*.md` mit Schritten). Ein genannter Ordner, den es nicht gibt, wird still übergangen (etwa ein lokaler Ordner, der in einem frischen Klon fehlt) | der Root des Projekts; Ablage `sichttest-logs/` im Root |
| `gewichtung { hand[], git[] }` | **schaltet den Nachtest-Indikator ein** (§6.7), auch leer. Dateien in **zwei Schichten**, die verschieden gerechnet werden: `hand` trägt je Bereich `weight`, `changed`, `note`, dazu `retest_steps` und `auto_weight`; `git` trägt `changed`, `commit`, `file` und hebt nur auf `auto_weight`. Innerhalb einer Schicht gilt die Reihenfolge der Liste. Eine genannte Datei, die fehlt, wird still übergangen. Format und Pflege bleiben bei der Anwendung | kein Indikator |
| `abbildung` | welche Aktion ein Feld des JSON-Formats auslöst (§6.2): der Name der Aktion, danach die Schlüssel, die als Argumente unverändert aus dem Feld mitgehen (ein Array wie `seed` bleibt ein Array; was die Zeile nicht nennt, geht nicht mit). Jeder Schlüssel ist zugleich ein Link im Text (`tab://…`, `sql:…`, `datei:…`; §6.2). **Vorbelegt ist der Schlüssel `fortgesetzt`**: er ist kein Feld der Liste, der Tester füllt ihn beim Aufruf mit `true`, wenn der Lauf schon eine Bewertung trägt (§6.4), sonst mit `false`. Ein Stück `wenn=<zustand>` ist kein Schlüssel: es nennt den Zustand der Anwendung, für den die Aktion da ist (§6.2) | das Feld wird gelesen und als Text gezeigt |

Die kleinste sinnvolle Datei nennt nur die Anwendung, ihren Start und ihre Listen:

```json
{ "schema": 1, "anwendung": "MeineAnwendung",
  "start": { "exe": "out/build/release/MeineAnwendung.exe", "argumente": ["--testing"] },
  "listen": [ { "ordner": "docs/tests", "muster": "Sichttest_*.md" } ] }
```

**Wie sie gefunden wird**

- Start über die Anwendung: `hallo` nennt den Pfad der Exe. Der Tester sucht von dort **aufwärts**
  bis zum ersten Ordner `.sichttest`, der eine `sichttest.projekt.json` enthält.
  `sts_konfig.projekt_datei` setzt den Pfad ausdrücklich, falls die Exe außerhalb des Repos liegt.
- **Wechsel der Listen bei `hallo`:** Meldet sich eine Anwendung, deren Projektdatei eine andere
  ist als die gerade offene, öffnet der Tester die Listen ihres Projekts — auch wenn er von Hand
  mit anderen Listen gestartet war. Er wechselt nicht, solange Aktionen laufen, und nicht, wenn die
  Anwendung keine Projektdatei hat.
- Start von Hand: `Sichttest <projektdatei>` oder `Sichttest <ordner>` — im zweiten Fall sucht er
  vom Ordner aufwärts. Ohne Projektdatei arbeitet er mit dem Ordner allein.
- **Eine ausdrücklich genannte Projektdatei** (`Sichttest <projektdatei>`,
  `sts_konfig.projekt_datei`) muss ebenfalls in einem Ordner `.sichttest` liegen; ihr Name ist
  frei (im Aufruf von Hand muss er auf `.projekt.json` enden, daran erkennt der Tester sie). Root
  ist der Ordner über `.sichttest`, wie bei der gesuchten Datei. Liegt sie woanders, lädt der
  Tester sie nicht, öffnet keine Liste und meldet: „Die Projektdatei <pfad> liegt nicht in einem
  Ordner .sichttest.“
- Eine Projektdatei, die sich nicht laden lässt (falscher Ort, nicht lesbar, kein JSON), wird
  gemeldet und nicht umgangen: der Tester zeigt dann auch die Listen des Ordners nicht.

**Was die Datei zusammenführt:** ein Ort für Listen, Ablage, Gewichtung und Start; derselbe für
einen von der Anwendung und einen von Hand gestarteten Tester; und die Namen der Aktionen bleiben
bei der Anwendung. Eine neue Anwendung schreibt eine solche Datei und meldet Aktionen an; an
Werkzeug, DLL und Schnittstelle ändert sich dafür nichts.

### 7.1 Wo der Tester bei der Anwendung liegt

`Sichttest.exe` bringt **sein eigenes Qt** mit. Es darf deshalb **nicht neben der Exe der
Anwendung liegen**: dort liegt deren Qt, und beide Sätze heißen `Qt6Core.dll` (die Anwendung kann
eine andere Qt-Fassung benutzen, oder ein Debug-Build lädt `Qt6Cored.dll`).

Suchregel der DLL für den Tester, in dieser Reihenfolge: Umgebungsvariable `SICHTTEST_EXE` (für
die Entwicklung am Werkzeug) → `<Ordner der DLL>\sichttest\Sichttest.exe` (§10, D2).

## 8. Laden nur bei `--testing`

Die Anwendung wird **nicht gegen die DLL gelinkt**. Der Kopf `sichttest_steuerung.hpp` lädt
`SichttestSteuerung1.dll` zur Laufzeit (`LoadLibrary`, Adressen der Aufrufe einzeln geholt), und
zwar nur dort, wo die Anwendung es verlangt — im Zweig `--testing`. Geladen wird mit **vollem
Pfad** aus dem Ordner der Exe, nie über den Suchpfad.

- Ein gewöhnlicher Start lädt nichts und öffnet nichts. Das ist durch den Aufbau gegeben, nicht
  durch eine Abfrage, die jemand vergessen kann.
- Fehlt die DLL, sagt `--testing` das in einem Satz, und die Anwendung läuft weiter.
- Die Anwendung braucht zum **Bauen nur die Köpfe**; DLL und Tester sind Laufzeit-Beigaben. Eine
  Import-Bibliothek und ein Linker-Schalter für verzögertes Laden entfallen.
- **Welche DLL geladen ist**, sagen `Sitzung::fassung()` und `Steuerung::fassung()`: S, die Spanne
  von P und das Produkt. Die Datei `VERSION` liegt nur in der Wurzel des Pakets, nicht neben der
  Exe.
- Der Kopf `sichttest_steuerung.hpp` bindet `<windows.h>` ein und setzt davor
  `WIN32_LEAN_AND_MEAN` und `NOMINMAX`, falls sie fehlen; beide gelten dann für den Rest der
  einbindenden Datei. Deshalb gehört der Kopf in **eine** `.cpp` und nicht in einen Kopf der
  Anwendung.

**Fehlt das Paket, fehlen auch die Köpfe** (andere Plattform, D3; oder kein Netz beim ersten
Holen). Die Anwendung übersetzt dann ohne Steuerung. Ist das Paket da, setzt der Bezug (§9) am
Target neben dem Include-Pfad das Define `SICHTTEST_VORHANDEN=1`:

```cpp
#ifdef SICHTTEST_VORHANDEN
#  include <sichttest_steuerung_qt.hpp>
#endif
```

Alles, was die Steuerung benutzt, steht hinter `#ifdef SICHTTEST_VORHANDEN` an **einer** Stelle
der Anwendung (dem Anmelden der Aktionen). Das Define kommt aus derselben Funktion wie der
Include-Pfad; welcher Zweig gilt, steht damit im Configure-Log.

## 9. Paket und Bezug

**Was eine Version liefert** (ein Paket je Tag `vX.Y.Z`, als Release auf GitHub):

```
sichttest-vX.Y.Z-win64/
├── include/    sichttest_steuerung.h · sichttest_steuerung.hpp · sichttest_steuerung_qt.hpp
├── bin/        SichttestSteuerung1.dll
├── sichttest/  Sichttest.exe mit seinem Qt
├── werkzeuge/  gewichtung_autoupdate.py, install_hooks.ps1 (Nachtest-Indikator)
└── VERSION     drei Zeilen: produkt=0.4.0 · s=1.1 · p=1 (von P nur die große Fassung)
```

Gebaut wird das Paket nur als Release, ohne `.pdb`. Verbindlich stehen S und P im Kopf
`sichttest_steuerung.h`; die Datei `VERSION` entsteht aus der Schablone `packaging/VERSION.in`
(`@VERSION@` aus `Solution.json`, S und P von Hand), und der Selbsttest hält S und P der
Schablone gegen den Kopf. Die Produktversion steht nicht im Kopf: DLL und Tester bekommen sie
beim Bauen als Define `STS_PRODUKT` aus `Solution.json`, die Anwendung fragt sie mit
`sts_fassung()` ab.

**Pin:** jede Anwendung nennt in einer Datei `sichttest.pin` Version, Adresse und Prüfsumme
(Vorlage: [templates/sichttest.pin](../templates/sichttest.pin)). Die Prüfsumme ist die der
**veröffentlichten** Datei — sie ändert sich mit jedem Schnüren, auch bei gleichem Inhalt:

```cmake
set(SICHTTEST_VERSION "v0.4.0")
set(SICHTTEST_URL     "https://github.com/PatrikNeunteufel/SichtTest_Helper/releases/download/${SICHTTEST_VERSION}/sichttest-${SICHTTEST_VERSION}-win64.zip")
set(SICHTTEST_SHA256  "<64 Hex-Zeichen>")
set(SICHTTEST_FALLBACK_PATHS "../SichtTest_Helper/out/package")   # optional
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

**Mit CMakeCraft** (ab v0.10.0; Vorlage:
[templates/integration/Solution.external.json](../templates/integration/Solution.external.json)):

| Bedarf | Wie |
|---|---|
| fertiges Paket in gepinnter Version holen | External mit `"archive": true` und `"pin": "sichttest.pin"`; Reihenfolge und Prüfsumme wie oben |
| Include-Pfad und Define, ohne zu linken | `include_dirs`, `define` — beides gilt **nur am Target, das das External nennt** (PRIVATE), nicht an denen, die davon abhängen |
| DLL und den Ordner des Testers neben die Exe legen | `runtime { files, dirs }`, je Konfiguration; abschaltbar je Target mit `external_options` |

**Ohne CMakeCraft** (Vorlage:
[templates/integration/sichttest.cmake](../templates/integration/sichttest.cmake)): die
eigenständige Datei `CMakeCraftPackage.cmake` aus dem Repo von CMakeCraft, als unveränderte Kopie
im Repo der Anwendung, mit `craft_package_fetch` und `craft_package_deploy`. Mindestversion von
CMake: 3.26.

Zwei Dinge, die eine Anwendung wissen muss:

- **Die Kopie neben die Exe läuft, wenn das Target gebaut wird.** Nach einem Wechsel der
  Paketversion die Exe bauen, sonst bleiben DLL und Tester daneben die alten.
- **Der Zwischenspeicher gilt über die Version, nicht über die Prüfsumme.** Wird unter derselben
  Version neu geschnürt, den Ordner `.externals/sichttest/<version>/` löschen.

**In diesem Projekt** steht das Paket im Block `packages` von `Solution.json`; das Target
`package_sichttest` (nicht in ALL, nur Release) legt `out/package/sichttest-v<version>-win64/`, die
`.zip` und die `.zip.sha256` an. Veröffentlichen: [Releasing.de.md](Releasing.de.md).

## 10. Grundentscheidungen

| # | Entscheidung | Warum |
|---|---|---|
| D1 | **Bezug:** Jede Version liefert ein fertiges Paket (Köpfe, DLL, Tester mit seinem Qt) als Release am Git-Tag; die Anwendung nennt die Version in `sichttest.pin` und holt das Paket beim Configure; `SICHTTEST_LOCAL_DIR` zeigt für die Entwicklung auf einen lokalen Stand. | Anwendungen bauen verschieden (mit und ohne CMakeCraft, clang und MSVC); ein fertiges Paket hängt nicht daran, wie sie bauen. |
| D2 | **Ort des Testers:** Er liegt im Paket und wird als Unterordner `sichttest\` neben die Exe der Anwendung gelegt, mit seinem eigenen Qt. Die DLL findet ihn dort. | Zwei Qt-Sätze in einem Ordner stoßen zusammen (§7.1). |
| D3 | **Plattform:** Die Steuerung gibt es zuerst nur unter Windows. Die Schnittstelle ist plattformneutral; auf anderen Plattformen fehlt die DLL, und `--testing` läuft ohne Steuerung. | — |
| D4 | **Laden:** Die Anwendung linkt nicht gegen die DLL. Der Kopf lädt sie zur Laufzeit im Zweig `--testing`; fehlt sie, läuft die Anwendung ohne Steuerung weiter (§8). | Ein gewöhnlicher Start darf nichts laden können; eine fehlende DLL darf weder den Bau noch den Start brechen. |
| D5 | **Kanal:** Der Tester lauscht. Die DLL verbindet sich bei `--testing` und startet den Tester, falls keiner läuft. Nach einem Neustart der Anwendung verbindet sie sich erneut, der Lauf geht beim selben Schritt weiter. | Die Anwendung öffnet nichts, was andere erreichen könnten; der Tester überlebt Absturz und Neustart. |
| D6 | **Projektdatei statt Quelltext:** Was der Tester über ein Projekt wissen muss (Listen, Ablage, Start, Gewichtung, Abbildung), steht in `.sichttest/sichttest.projekt.json`, nicht im Quelltext der Anwendung; die Pfade gelten ab dem Root. | Die Schnittstelle passt für heutige und künftige Anwendungen ohne Änderung; ein neuer Listenordner braucht keine Übersetzung. |
| D7 | **Keine C++-Typen an der Grenze**, nur C. | Schon ohne Qt bindet eine C++-Grenze beide Seiten an denselben Compiler, dieselbe Laufzeitbibliothek und dieselbe Bauart (Debug und Release legen `std::string` verschieden an); hier treffen clang, MSVC und Debug-Builds aufeinander. |
| D8 | **Kein Qt in der DLL.** | Zwei Qt in einem Prozess stoßen schon bei einer Qt-Fassung zusammen (eine Release-DLL zieht `Qt6Core.dll` in einen Debug-Prozess, der `Qt6Cored.dll` geladen hat), und eine gegen eine Qt-Fassung gebaute DLL läuft nicht gegen die andere Fassung einer Anwendung. Die DLL verlangt nur die Release-Laufzeit von Visual C++, die jede Qt-Anwendung mitbringt; weil kein Speicher den Besitzer wechselt (§3), darf sie auch in einem Debug-Prozess laufen. |
| D9 | **C-Schnittstelle plus zwei kleine Köpfe** (`.hpp` ohne Qt, `_qt.hpp` für die Zustellung im GUI-Thread), in der Anwendung übersetzt. | Sie sind damit immer mit Compiler und Qt der Anwendung gebaut. |
| D10 | **Aktionen im GUI-Thread durch Abholen mit Wecker** (§3). | „Zustellen“ durch die DLL ginge nur, wenn sie die Ereignisschleife kennte — also mit Qt. |
| D11 | **Drei Nummern** (§5): P und S werden geprüft, das Produkt wird gepinnt. | Ohne die Trennung würde jeder Fehlerbehebung im Tester ein Pin-Wechsel in allen Anwendungen folgen. |
| D12 | **Das Werkzeug kennt keinen Aktionsnamen.** Felder des JSON-Formats werden erst über die `abbildung` der Projektdatei zu Aktionen. | Die Abhängigkeit zeigt in eine Richtung: die Anwendungen kennen das Werkzeug, das Werkzeug kennt keine Anwendung. |

## Glossar

Die englische Fassung erklärt die deutschen Bezeichner in einem Glossar
([Specification.md#glossary](Specification.md#glossary)); auf Deutsch sind sie selbsterklärend.
