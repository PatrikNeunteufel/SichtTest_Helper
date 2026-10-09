# Beispiel mit Aktionen

**Anwendung:** MeineAnwendung · **Exe:** `out\build\MeineAnwendung.exe`

Eine Liste, deren Punkte Aktionen tragen: Das Werkzeug löst sie in der geprüften Anwendung aus,
wenn diese mit `--testing` läuft und die Aktionen angemeldet hat. Ohne Verbindung bleibt alles
wie bei einer Liste ohne Aktionen — der Handgriff steht immer als Text da.

- [ ] **V0 Vorbereiten:** Anwendung starten und das Beispielprojekt öffnen.
      `aktion: datei_oeffnen pfad="beispiele\demo.projekt.json"`

## A. Ansicht

- [ ] **A0 Vorbereiten:** Im Panel Ansicht die Vorlage laden, dann **Bearbeiten** einschalten.
      `aktion: vorlage_laden datei="vorlagen\ansicht A.vorlage"` `aktion: bearbeiten an`
- [ ] **A1 Die Marke steht an ihrer Zeit:** *Tun:* Marke auf 0:20 setzen. *Sehen:* Die Anzeige
      zeigt 0:20, der Balken steht bei einem Drittel. `aktion: marke zeit=0:20`
- [ ] **A2 Wie A0, aber ohne Bearbeiten:** *Tun:* Vorlage neu laden, **Bearbeiten** ausschalten.
      *Sehen:* Die Griffe sind weg. `aktion: @A0` `aktion: bearbeiten aus`
- [ ] **A3 Von Hand:** Das Fenster an der rechten Kante schmaler ziehen. Die Anzeige bricht
      nicht um.
