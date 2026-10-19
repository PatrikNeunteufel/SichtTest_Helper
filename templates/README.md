**English** · [Deutsch](README.de.md)

# Templates

Files to copy into the repository of an application that Sichttest should drive. The steps are
described in [docs/Integration_Guide.md](../docs/Integration_Guide.md); every key and rule is
defined in [docs/Specification.md](../docs/Specification.md). Keys starting with `_` in the JSON
files are comments and are ignored by the tool — delete them or keep them.

| File | Copy to | Purpose |
|---|---|---|
| [.sichttest/sichttest.projekt.json](.sichttest/sichttest.projekt.json) | `<repo>/.sichttest/sichttest.projekt.json` | what the tool needs to know about the project: name, start, lists, storage, retest indicator, mapping of fields to actions |
| [sichttest.pin](sichttest.pin) | `<repo>/sichttest.pin` | which package version the build fetches, with checksum |
| [integration/Solution.external.json](integration/Solution.external.json) | merge into `Solution.json` | consuming the package with CMakeCraft |
| [integration/sichttest.cmake](integration/sichttest.cmake) | `<repo>/cmake/sichttest.cmake` | consuming the package without CMakeCraft (needs a copy of `CMakeCraftPackage.cmake`) |
| [integration/SichttestIntegration.hpp](integration/SichttestIntegration.hpp), [.cpp](integration/SichttestIntegration.cpp) | your sources | loading the DLL with `--testing`, registering actions, reporting a state (Qt) |
| [Sichttest_List.md](Sichttest_List.md), [Sichttest_List.de.md](Sichttest_List.de.md) | a list folder | a Markdown list with preparation and actions |
| [List.testprotokoll.json](List.testprotokoll.json) | a list folder | a JSON list with preparation, teardown, actions and areas |
| [test-gewichtung.json](test-gewichtung.json) | where `gewichtung.hand` points | hand file of the retest indicator |

The action names in the templates (`open_file`, `show_tab`, `testdb_setup`, …) are examples:
the names belong to your application, which registers them. A complete example project lies in
[examples/project/](../examples/project/).
