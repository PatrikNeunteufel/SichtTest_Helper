# Visual test: <feature>

**Anwendung:** MyApp · **Exe:** `out\build\release\MyApp.exe`

<!--
Template of a Markdown list for Sichttest. Copy it into a folder named under "listen"
in .sichttest/sichttest.projekt.json and replace the placeholders.

Fixed keywords (the tool recognises them in German):
  **Anwendung:**  name the application reports itself with (optional with one application)
  Vorbereiten     an item whose title starts with it is a preparation: no verdict
  `aktion: …`     an action the tool triggers; the text of the item stays the manual fallback
  `aktion: @V0`   runs the actions of item V0 at this point

The action names (open_file, load_template, …) belong to your application:
it registers them; the tool knows none of them.
Starting state per item: [ ] open, [x] Pass, [!] Fail, [-] skipped.
-->

- [ ] **V0 Vorbereiten:** Start MyApp and open the demo project.
      `aktion: open_file path="examples\demo.myproj"`

## A. <First section>

- [ ] **A0 Vorbereiten:** Load template A, then switch **Edit** on.
      `aktion: load_template file="presets\a.tpl"` `aktion: edit on`
- [ ] **A1 <What is checked>:** *Do:* <the manual step>. *See:* <what must be visible>.
      `aktion: <action> <key>=<value>`
- [ ] **A2 Like A0, but without editing:** *Do:* reload the template, switch **Edit** off.
      *See:* the handles are gone. `aktion: @A0` `aktion: edit off`
- [ ] **A3 By hand only:** *Do:* make the window narrower at its right edge. *See:* the view
      does not wrap.

## B. <Second section>

- [ ] **B1 <What is checked>:** *Do:* … *See:* …
