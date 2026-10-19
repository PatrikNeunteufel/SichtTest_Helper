# Example with actions

**Anwendung:** MyApp · **Exe:** `out\build\MyApp.exe`

A list whose items carry actions: the tool triggers them in the application under test when it
runs with `--testing` and has registered the actions. Without a connection everything stays as in
a list without actions — the manual step is always there as text.

- [ ] **V0 Vorbereiten:** Start the application and open the example project.
      `aktion: open_file path="examples\demo.myproj"`

## A. View

- [ ] **A0 Vorbereiten:** In the View panel load the template, then switch **Edit** on.
      `aktion: load_template file="presets\view A.tpl"` `aktion: edit on`
- [ ] **A1 The marker sits at its time:** *Do:* set the marker to 0:20. *See:* the display shows
      0:20, the bar is at one third. `aktion: set_marker time=0:20`
- [ ] **A2 Like A0, but without editing:** *Do:* reload the template, switch **Edit** off.
      *See:* the handles are gone. `aktion: @A0` `aktion: edit off`
- [ ] **A3 By hand:** Make the window narrower at its right edge. The display does not wrap.
