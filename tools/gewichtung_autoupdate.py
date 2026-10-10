#!/usr/bin/env python3
# =============================================================================
# gewichtung_autoupdate.py — git-Schicht des Nachtest-Indikators
# =============================================================================
# Teil des Werkzeugs Sichttest (docs/Konzept_Steuerung.md §14.1 Teil C).
# Herkunft: Comm Studio, tools/gewichtung_autoupdate.py (2026-06-12); hier
# verallgemeinert — die Pfade kommen aus der Projektdatei, nicht aus dem Skript.
#
# Läuft als git-Hook (post-commit: der frische Commit; post-merge:
# ORIG_HEAD..HEAD = der geholte Bereich) und schreibt je Area den jüngsten
# Commit, der ihre `paths`-Muster berührt hat.
#
# Was es liest:  <Repo>/.sichttest/sichttest.projekt.json, Schlüssel
#                "gewichtung": { "hand": [ … ], "git": [ … ] }
#                hand[i] ist eine Hand-Datei mit "areas": { name: { "paths": [Muster] } },
#                git[i] die Datei, die für sie geschrieben wird. Fehlt git[i],
#                wird für hand[i] nichts geschrieben.
# Was es schreibt: git[i] mit "areas": { name: { changed, commit, file, commits } }.
#                Keine Gewichte — die stehen in der Hand-Datei. Die Datei ist
#                rechnerlokal und gehört nicht ins Repo (.gitignore).
#
# Aufruf:  python gewichtung_autoupdate.py [REV-BEREICH]
#          ohne Argument = HEAD (ein Commit). Als Hook stört es nie: Exit immer 0.
# =============================================================================
import fnmatch
import json
import subprocess
import sys
from pathlib import Path

PROJEKTDATEI = ".sichttest/sichttest.projekt.json"


def git(*args: str) -> str:
    return subprocess.run(["git", *args], capture_output=True, text=True,
                          encoding="utf-8", check=True).stdout.strip()


def lies(pfad: Path) -> dict:
    return json.loads(pfad.read_text(encoding="utf-8-sig"))


def main() -> int:
    repo = Path(git("rev-parse", "--show-toplevel"))
    projekt_p = repo / PROJEKTDATEI
    if not projekt_p.exists():
        return 0
    gewichtung = lies(projekt_p).get("gewichtung", {})
    hand = gewichtung.get("hand", [])
    ziel = gewichtung.get("git", [])

    # Commits des Bereichs (Vorgabe: nur HEAD) mit Datum und Dateien.
    bereich = sys.argv[1] if len(sys.argv) > 1 else None
    if bereich:
        zeilen = git("log", "--format=%H\x1f%cI", bereich).splitlines()
    else:
        zeilen = git("log", "-1", "--format=%H\x1f%cI", "HEAD").splitlines()
    commits = []
    for zeile in zeilen:
        if "\x1f" not in zeile:
            continue
        sha, zeit = zeile.split("\x1f", 1)
        dateien = git("diff-tree", "--no-commit-id", "--name-only", "-r",
                      "--root", sha).splitlines()
        commits.append((sha, zeit, dateien))

    for i, hand_datei in enumerate(hand):
        if i >= len(ziel):
            continue   # keine Datei für die git-Schicht genannt
        hand_p = repo / hand_datei
        ziel_p = repo / ziel[i]
        if not hand_p.exists():
            continue
        areas = lies(hand_p).get("areas", {})

        auto = {"_description": "Automatisch geschrieben (git-Hook, "
                                "gewichtung_autoupdate.py): je Area der jüngste "
                                "Commit, der ihre paths-Muster aus der Hand-Datei "
                                "berührt hat. Nicht von Hand pflegen; rechnerlokal.",
                "areas": {}}
        if ziel_p.exists():
            try:
                auto = lies(ziel_p)
                auto.setdefault("areas", {})
            except Exception:
                pass

        geaendert = False
        for sha, zeit, dateien in commits:
            for name, area in areas.items():
                muster = area.get("paths", [])
                treffer = next((d for d in dateien
                                for m in muster if fnmatch.fnmatch(d, m)), None)
                if not treffer:
                    continue
                eintrag = auto["areas"].get(name, {})
                if zeit > eintrag.get("changed", ""):
                    eintrag["changed"] = zeit           # voller ISO-Zeitstempel
                    eintrag["commit"] = sha[:10]
                    eintrag["file"] = treffer           # ein Beispiel
                eintrag["commits"] = int(eintrag.get("commits", 0)) + 1
                auto["areas"][name] = eintrag
                geaendert = True

        if geaendert:
            ziel_p.parent.mkdir(parents=True, exist_ok=True)
            ziel_p.write_text(json.dumps(auto, indent=2, ensure_ascii=False)
                              + "\n", encoding="utf-8")
    return 0


if __name__ == "__main__":
    try:
        sys.exit(main())
    except Exception:
        sys.exit(0)   # ein Hook darf nie stören
