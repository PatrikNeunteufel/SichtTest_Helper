/* sichttest_steuerung.h — Schnittstelle S der DLL SichttestSteuerung (C).
 *
 * Die einzige verbindliche Grenze zwischen der geprüften Anwendung und dem
 * Tester Sichttest.exe. Beschreibung: docs/Konzept_Steuerung.md §3 bis §5.
 *
 * Grundsätze: nur C-Typen; Zeichenketten sind UTF-8, nullterminiert; Speicher
 * wechselt nie den Besitzer (was die DLL liefert, gilt bis zum nächsten Aufruf
 * oder bis zum Ende des Rückrufs; was die Anwendung übergibt, kopiert die DLL
 * sofort); jede Struktur beginnt mit ihrer Größe; kein Aufruf wirft.
 *
 * Die Anwendung linkt nicht gegen die DLL, sie lädt sie zur Laufzeit (§8).
 */
#ifndef SICHTTEST_STEUERUNG_H
#define SICHTTEST_STEUERUNG_H

#include <stdint.h>

#define STS_S_MAJOR 1          /* Fassung der Schnittstelle S, mit der die Anwendung übersetzt ist */
#define STS_S_MINOR 0

#define STS_P_MAJOR 1          /* Fassung des Protokolls P zwischen DLL und Tester */
#define STS_P_MINOR 0

/* Die Produktversion (Tag des Repos) steht nicht hier: die DLL trägt sie in sich, die
   Anwendung fragt sie mit sts_fassung() ab. Beim Bauen von DLL und Tester kommt sie als
   Define STS_PRODUKT aus Solution.json. */

#if defined(_WIN32) && defined(SichttestSteuerung_EXPORTS)
#  define STS_API __declspec(dllexport)
#else
#  define STS_API
#endif

#ifdef __cplusplus
extern "C" {
#endif

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

/* Schalter einer Aktion (sts_aktion.schalter), Bedeutung in §3. */
#define STS_FRAGT_NACH        0x1u   /* verändert Zustand, der verloren gehen kann; der Tester fragt vorher */
#define STS_NUR_ERSTLAUF      0x2u   /* räumt ab; entfällt beim Fortsetzen eines Laufs */
#define STS_WARTET_AUF_MENSCH 0x4u   /* fragt in der Anwendung nach; keine Frist im Tester */
#define STS_BEENDET_ANWENDUNG 0x8u   /* nach der Antwort endet die Anwendung oder startet neu */

/* sts_zustand() */
#define STS_GETRENNT   0
#define STS_VERBINDET  1
#define STS_VERBUNDEN  2
#define STS_ABGELEHNT  3

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
                                    STS_BEENDET_ANWENDUNG */
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

/* Fassungen der DLL, ohne Sitzung abfragbar. Jeder Zeiger darf NULL sein. */
STS_API void       sts_fassung(uint16_t* s_major, uint16_t* s_minor,
                               uint16_t* p_major_min, uint16_t* p_major_max,
                               const char** produkt);                 /* "0.2.0" = Tag des Repos */

/* Sitzung anlegen; prüft S (§5). Öffnet noch nichts. Scheitert es, nennt
   sts_letzter_fehler(NULL) den Grund. */
STS_API sts_status sts_oeffne(const sts_konfig* k, sts_sitzung** s);
/* Aktion anmelden; vor oder nach dem Verbinden (später Angemeldetes wird nachgemeldet). */
STS_API sts_status sts_melde_aktion(sts_sitzung* s, const sts_aktion* a);
/* Wecker: wird aus dem Lesethread der DLL gerufen, sobald etwas wartet. Darf nur
   einen Anstoß in den GUI-Thread stellen (z. B. ein Ereignis posten). */
STS_API void       sts_setze_wecker(sts_sitzung* s, void (*wecker)(void* nutzer), void* nutzer);
/* Mit dem Tester verbinden; startet ihn, falls keiner lauscht. Kehrt sofort zurück,
   das Ergebnis kommt über den Zustand. */
STS_API sts_status sts_verbinde(sts_sitzung* s);
/* Im GUI-Thread: wartende Aufrufe ausführen (Rückrufe laufen HIER). Liefert die Zahl. */
STS_API int        sts_pumpe(sts_sitzung* s);
/* Text zur Antwort legen (nur im Rückruf). */
STS_API void       sts_antwort_text(sts_antwort* antwort, const char* text);
/* Freie Meldung an den Tester, erscheint dort in der Statuszeile und im Log. */
STS_API sts_status sts_melde(sts_sitzung* s, const char* text);
/* STS_GETRENNT, STS_VERBINDET, STS_VERBUNDEN oder STS_ABGELEHNT (Fassung). */
STS_API int        sts_zustand(sts_sitzung* s);
STS_API const char* sts_letzter_fehler(sts_sitzung* s);
/* Schreibt `tschuess` noch hinaus (höchstens 2 s), dann Ende. Nicht aus einem Rückruf rufen. */
STS_API void       sts_schliesse(sts_sitzung* s);

#ifdef __cplusplus
}
#endif

#endif /* SICHTTEST_STEUERUNG_H */
