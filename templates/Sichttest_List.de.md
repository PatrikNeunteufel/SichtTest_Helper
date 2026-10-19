# Sichttest: <Funktion>

**Anwendung:** MeineAnwendung · **Exe:** `out\build\release\MeineAnwendung.exe`

<!--
Vorlage einer Markdown-Liste für Sichttest. In einen Ordner kopieren, den
.sichttest/sichttest.projekt.json unter "listen" nennt, und die Platzhalter ersetzen.

Feste Schlüsselwörter:
  **Anwendung:**  Name, unter dem sich die Anwendung meldet (bei nur einer Anwendung optional)
  Vorbereiten     ein Punkt, dessen Titel damit beginnt, ist eine Vorbereitung: kein Urteil
  `aktion: …`     eine Aktion, die das Werkzeug auslöst; der Text des Punkts bleibt der Rückfall
  `aktion: @V0`   führt die Aktionen des Punkts V0 an dieser Stelle aus

Die Namen der Aktionen (datei_oeffnen, vorlage_laden, …) gehören der Anwendung:
sie meldet sie an, das Werkzeug kennt keinen davon.
Ausgangsstand je Punkt: [ ] offen, [x] Pass, [!] Fail, [-] übersprungen.
-->

- [ ] **V0 Vorbereiten:** MeineAnwendung starten und das Beispielprojekt öffnen.
      `aktion: datei_oeffnen pfad="beispiele\demo.meinprojekt"`

## A. <Erster Abschnitt>

- [ ] **A0 Vorbereiten:** Vorlage A laden, dann **Bearbeiten** einschalten.
      `aktion: vorlage_laden datei="vorlagen\a.vorlage"` `aktion: bearbeiten an`
- [ ] **A1 <Was geprüft wird>:** *Tun:* <der Handgriff>. *Sehen:* <was zu sehen sein muss>.
      `aktion: <aktion> <schlüssel>=<wert>`
- [ ] **A2 Wie A0, aber ohne Bearbeiten:** *Tun:* Vorlage neu laden, **Bearbeiten** ausschalten.
      *Sehen:* Die Griffe sind weg. `aktion: @A0` `aktion: bearbeiten aus`
- [ ] **A3 Nur von Hand:** *Tun:* Das Fenster an der rechten Kante schmaler ziehen. *Sehen:* Die
      Anzeige bricht nicht um.

## B. <Zweiter Abschnitt>

- [ ] **B1 <Was geprüft wird>:** *Tun:* … *Sehen:* …
