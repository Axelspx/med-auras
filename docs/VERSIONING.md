# Versioning

Every build of `med-auras.exe` carries a version. The version is set in exactly one
place — the `project(... VERSION ...)` line in `CMakeLists.txt` — and flows from there
into the executable's Windows file properties.

## How it works

```text
CMakeLists.txt  project(med_auras VERSION 0.2.0)
                MED_AURAS_VERSION_SUFFIX  "-dev" (empty for a release)
        |
        v  configure_file
build tree      version.h
        |
        v  #include
src/resources.rc  VS_VERSION_INFO block
        |
        v
med-auras.exe   right-click -> Properties -> Details
```

`src/version.h.in` is the template; `version.h` is generated into the build tree and is
not checked in. The version string is *not* passed as a preprocessor define — `windres`
mangles the quoting on a `-D` string define, so the generated header is the only route
that works on both toolchains.

`FILEVERSION` and `PRODUCTVERSION` are the numeric `MAJOR, MINOR, PATCH, 0`. The
`FileVersion` and `ProductVersion` strings carry the pre-release suffix as well, so a
development build reads `0.2.0-dev` while its numeric fields read `0.2.0.0`.

## Bumping the version

1. Edit the `VERSION` in `project(...)` in `CMakeLists.txt`.
2. Set `MED_AURAS_VERSION_SUFFIX` to `""` for a release, or `"-dev"` while the work for
   that version is still in progress.
3. Rebuild. Confirm with:

```bash
powershell -c "(Get-Item build\Release\med-auras.exe).VersionInfo.FileVersion"
```

4. Copy the result to `dist/med-auras-<version>.exe`.

Which component to bump follows ordinary semantics for a pre-1.0 app: patch for fixes,
minor for a user-visible feature. There is no 1.0 planned yet.

## Archive

Built executables live in `dist/`, named by version. They are **not** in git — `.gitignore`
excludes `*.exe`, deliberately, so the repository stays source-only. `dist/` is a local
archive; this file is the record of what each build was.

| File | Version | Built | Source state |
|---|---|---|---|
| `med-auras-0.0.1.exe` | 0.0.1 | 2026-08-13 21:58 | MVP complete (`b68ad64` / `a5fa7d1`). All five milestones passed. Debug build; the only binary surviving from that era. |
| `med-auras-0.1.0.exe` | 0.1.0 | 2026-08-16 11:54 | Live composition backdrop blur (`7924f85`) plus the configurable card background dialog (`ab37882`). Taken from `build/Release/med-auras-ss.exe`, the sandbox copy of that day's Release build — which is why a snapshot of it survived a later rebuild of `med-auras.exe`. |
| `med-auras-0.1.0-debug.exe` | 0.1.0 | 2026-08-16 11:55 | Same source state, Debug configuration. Kept only because it exists; not a distinct release. |
| `med-auras-0.2.0-dev.exe` | 0.2.0-dev | 2026-08-17 | Schedule system — fixed recurring schedules, overdue tracking, dose history. First build with a version resource compiled in. |

The three pre-0.2.0 binaries have **no** version resource — they were linked before this
scheme existed, so their Details tab is blank and the filename is the only record. They
were not rebuilt, because rebuilding them from their old source would produce different
binaries than the ones actually run at the time.

0.1.0 is approximate by a few hours: blur landed at `7924f85` but nothing was built at
that commit, and the earliest surviving post-blur binary already includes the background
settings dialog on top of it.

An archived build is sandboxed for free. The data file follows the executable's stem, so
`dist/med-auras-0.1.0.exe` reads `medications-med-auras-0.1.0.json` and cannot touch the
real `medications.json`. Running an old version to compare behaviour is safe. Keep
`libwinpthread-1.dll` beside it, as with any other copy — see the standalone-executable
notes in `README.md`.
