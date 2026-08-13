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
`notices/`. It includes `README.md` but excludes saves, logs, captures, build
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

`-SkipRealWindow` keeps the deterministic clean-root, archive and negative
checks while omitting the visible hardware window. The full Windows build gate
uses that non-intrusive form; formal R5 acceptance runs the three-frame window.
Generated directories, ZIP files and logs live below `bin/package_runs/` and
remain ignored.

The accepted 2026-08-13 Release run contains 61 inventoried files and the
optional `example-stone` pack. Both repeated archive builds produced SHA-256
`F4F3C448E75031F30EB788FF72C5F22A6A32CDF6C85A90164D1E16B7F807BB69`.
