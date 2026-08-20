# Windows Release Packaging

R5 produces a self-contained x64 directory and a deterministic ZIP from the
current Release client:

```powershell
powershell.exe -NoProfile -ExecutionPolicy Bypass `
  -File tools\package_windows_release.ps1 `
  -IncludePack example-stone
```

The distribution preserves the runtime root layout: the executable and config
templates are under `bin/`, resources and the startup manifest under `media/`,
optional directory packs under `packs/`, and dependency licenses under
`notices/`, including the vendored optional Tracy client notice. It includes
`README.md` but excludes saves, logs, captures, crashes, build
trees, test executables, symbols, libraries and developer tools.

`distribution-manifest.txt` records sorted SHA-256, byte length and relative
path for every payload file. The ZIP writer fixes entry timestamps, ordering
and compression settings. Repeating the same package inputs therefore produces
the same archive hash.

The packager copies the distribution to an isolated clean root and verifies:

- exact inventory and hashes;
- validation-only startup using only the copied root;
- a real Ogre window that exits after three frames;
- a missing terrain shader fails before Ogre construction with its logical
  path and category;
- an extra unlisted texture fails the distribution inventory check;
- optional packs are enabled through the packaged `bin/resource-packs.txt`
  without weakening the base startup manifest.
- every ordinary packaged process receives a crash directory outside both the
  copied distribution and its world-save tree, and produces no `.dmp`;
- a clean packaged controlled crash writes one external local report, and the
  next packaged startup discovers exactly that report without networking.

`-SkipRealWindow` keeps the deterministic clean-root, archive and negative
checks while omitting the visible hardware window. The full Windows build gate
uses that non-intrusive form; formal R5 acceptance runs the three-frame window.
Generated directories, ZIP files and logs live below `bin/package_runs/` and
remain ignored.

The accepted final 2026-08-20 RC gate contains 66 inventoried files with the
`example-stone` optional pack and produces archive SHA-256
`AB5E07D151E8C9973815F5969AA281CE7CBD57413A446825CC9FC12C04AAD81C`.
The distribution contains no PDB, symbol archive, old crash, save, log or test
binary. Its clean-package controlled crash and next-start local prompt both
pass, so H3 packaging is complete. Symbols remain a separate build-identity
archive and are never copied into this ZIP.
