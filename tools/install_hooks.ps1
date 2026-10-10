# =============================================================================
# install_hooks.ps1 - git-Hooks fuer die git-Schicht des Nachtest-Indikators
# =============================================================================
# Teil des Werkzeugs Sichttest (docs/Konzept_Steuerung.md, Abschnitt 14.1, Teil C).
# Herkunft: Comm Studio, tools/install_hooks.ps1.
#
# Im Repo der Anwendung aufrufen. Schreibt .git/hooks/post-commit und
# post-merge; beide rufen gewichtung_autoupdate.py aus DIESEM Ordner. Hooks
# gelten je Klon - auf jedem Rechner einmal ausfuehren. Wiederholbar: eigene
# Hooks werden ueberschrieben, fremde gleichen Namens nicht angefasst, nur
# gemeldet.
$repo = git rev-parse --show-toplevel
if (-not $repo) { Write-Error "kein git-Repo"; exit 1 }
$hooks = Join-Path $repo ".git/hooks"
$marke = "gewichtung_autoupdate"
$skript = (Join-Path $PSScriptRoot "gewichtung_autoupdate.py") -replace '\\', '/'

$postCommit = "#!/bin/sh`n# $marke (install_hooks.ps1)`npython `"$skript`" >/dev/null 2>&1 || true`n"
$postMerge  = "#!/bin/sh`n# $marke (install_hooks.ps1)`npython `"$skript`" ORIG_HEAD..HEAD >/dev/null 2>&1 || true`n"

foreach ($paar in @(@("post-commit", $postCommit), @("post-merge", $postMerge))) {
    $name = $paar[0]; $inhalt = $paar[1]
    $pfad = Join-Path $hooks $name
    if ((Test-Path $pfad) -and -not (Select-String -Path $pfad -Pattern $marke -Quiet)) {
        Write-Warning "$name gibt es schon (fremd) - bitte von Hand zusammenfuehren: $pfad"
        continue
    }
    [System.IO.File]::WriteAllText($pfad, $inhalt)
    Write-Host "eingerichtet: $name -> $skript"
}
