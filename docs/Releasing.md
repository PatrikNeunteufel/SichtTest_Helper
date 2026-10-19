**English** · [Deutsch](Releasing.de.md)

# Releasing a version

For the maintainer of this repository. Repository: `https://github.com/PatrikNeunteufel/SichtTest_Helper`
(public; applications download the release archive without signing in).

## Per version

1. **Raise the version:** `solution.version` in `Solution.json`; S and P in `packaging/VERSION.in`
   and in `sichttest_steuerung.h` only if the interface or the protocol changed
   ([Specification §5](Specification.md#5-three-numbers-two-of-them-checked)). Add the version to
   `CHANGELOG.md` and `CHANGELOG.de.md`.
2. **Build and check:**

   ```bash
   cmake --preset windows-ninja-release-clang
   ```

   ```bash
   cmake --build --preset build-ninja-release-clang
   ```

   Then `Sichttest.exe --selbsttest <empty folder>`, exit code 0. The self-test needs all targets,
   so build without `--target`.
3. **Commit and tag** — the package must correspond to a commit, so build it only afterwards:
   commit, `git tag -a vX.Y.Z -m "…"`.
4. **Build the package:**

   ```bash
   cmake --build --preset build-ninja-release-clang --target package_sichttest
   ```

   Result in `out/package/`: `sichttest-vX.Y.Z-win64/`, `sichttest-vX.Y.Z-win64.zip` and
   `sichttest-vX.Y.Z-win64.zip.sha256`. **Do not build it again afterwards:** the checksum changes
   with every build, even with identical content. Exactly this file is uploaded, and its checksum
   goes into the pins.
5. **Push:** `git push origin master`, `git push origin vX.Y.Z`.
6. **Create the release:** on GitHub **Releases → Draft a new release**, choose the tag `vX.Y.Z`,
   attach the `.zip` (the file name must stay unchanged), **Publish release**. The address is then
   `https://github.com/PatrikNeunteufel/SichtTest_Helper/releases/download/vX.Y.Z/sichttest-vX.Y.Z-win64.zip`.
7. **Check the upload:** the `digest` of the asset in the GitHub API
   (`https://api.github.com/repos/PatrikNeunteufel/SichtTest_Helper/releases/tags/vX.Y.Z`) equals
   the content of the `.zip.sha256`, and the download address answers 200.
8. **Pins:** every application that wants the new version changes `SICHTTEST_VERSION` and
   `SICHTTEST_SHA256` in its `sichttest.pin`. As long as an application does not change its pin,
   it stays with the old package.

**A published package is never replaced.** Whoever exchanges the `.zip` of a tag breaks the
checksum in every pin. Mistakes are fixed by a new version.

**Release texts** describe the tool itself — what the version adds for whoever uses Sichttest —
and say “headers” for the three header files.

## What stays out of the repository

`out/`, `.externals/`, `.vs/`, `CMakeUserPresets.json`, `CLAUDE.md`, `.claude/`, `sichttest-logs/`
— the `.gitignore` takes care of it. Every commit address is publicly visible.

## What can go wrong

| Symptom | Cause | Remedy |
|---|---|---|
| push fails with 403 | wrong account in the Windows Credential Manager | delete the entry `git:https://github.com`, push again |
| consumption reports a wrong checksum | `.zip` rebuilt or replaced after it was entered in the pin | publish a new version, set the pin to it |
| consumption does not find the archive (404) | release only saved as a draft, or file name changed | publish the release; compare the name with the pin |
