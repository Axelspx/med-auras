# Medication Cooldown Widget

A minimal Windows desktop medication timer inspired by World of Warcraft-style ability cooldown bars.

## Goal

Provide an always-visible, glanceable way to track when medications were last taken and when the next dose is available, while using as little CPU, memory, and background activity as practical.

After initial setup, normal use should be nearly frictionless: the widget launches automatically with Windows, restores the user's configured medications, and recording a dose requires one click.

## Core UI

The main window is a small borderless desktop widget containing vertically stacked medication rows.

Each row should show:

- Medication icon
- Medication name
- Dose
- Horizontal cooldown/progress bar
- The scheduled time of the next dose, and the time remaining or how overdue it is
- One obvious **Taken** button

The visual direction should take inspiration from WoW cooldown trackers: compact, readable, stacked bars with strong status visibility. Do not copy WoW assets or reproduce an addon skin exactly.

Example:

```text
[icon] Elvanse 40 mg                    [Taken]
       ██ 08:30 ███████ 13:24:07 ░░░░░░░

[icon] Ibuprofen 400 mg                 [Taken]
       █ 12:30 ░░░░░ 02:11:45 ░░░░░░░░░░

[icon] Vitamin D                        [Taken]
       ░ 09:00 ░░░░░ 35:12 overdue ░░░░░
```

## Technology

- Language: C++20
- Platform: Windows 11
- UI: native Win32
- Rendering: Win32/GDI or Direct2D where useful
- Storage: local JSON
- Dependencies: keep to an absolute minimum

Avoid:

- Electron
- Chromium/WebView UI
- Qt
- Python runtimes
- .NET
- background web servers
- unnecessary third-party frameworks

## Performance Requirements

Resource usage is a primary project constraint.

The application should:

- remain effectively at 0% CPU while idle
- avoid continuous polling loops
- avoid high-frequency UI updates
- use timestamp-based timers rather than decrementing counters
- use Windows/event-driven timers only when work is actually required
- update displayed countdowns no more often than necessary
- avoid unnecessary worker threads
- minimize allocations and startup work
- keep memory usage small
- perform no network activity

Timer state must be calculated from the schedule, never from when a dose was taken:

```text
active_at = the earliest scheduled occurrence not yet taken or missed
remaining = active_at - current_time      (negative means overdue)
```

Timers must remain correct through:

- app restarts
- Windows restarts
- sleep
- hibernation
- the application being closed

## Daily-Use Experience

Once medications are configured, the app should behave like a passive desktop utility rather than something the user has to repeatedly open and manage.

Expected flow:

```text
Windows sign-in
    ↓
Widget launches automatically
    ↓
Configured medication bars appear immediately
    ↓
User takes medication
    ↓
Clicks Taken once
    ↓
Current timestamp is saved
    ↓
Cooldown restarts immediately
```

No extra window, confirmation dialog, form, or navigation should be required for the normal "I just took this medication" action.

## MVP Features

### Medication tracking

Each medication should support:

- name
- dose
- optional icon
- a fixed recurring schedule
- the active occurrence it is currently tracking
- dose history
- enabled/paused state

Schedules are fixed and do not drift when a dose is recorded:

- **Hourly** — every N hours from a chosen start date/time, repeating continuously rather than restarting each day.
- **Daily** — one or more fixed times every day.
- **Weekly** — one or more weekday and time pairs.
- **Monthly** — one or more day-of-month and time pairs, skipping months that do not have the day.

Hourly accepts a value in minutes, hours, days, or weeks, fractional as long as it resolves to at least one whole
minute; `3.5 days` becomes `5040` stored minutes. The other types are edited as a list of times built with **Add**
and **Remove**.

A medication tracks exactly one occurrence at a time. Clicking **Taken** records the dose that is due at that moment,
marks any earlier unresolved doses as missed, and moves to the next scheduled occurrence — taking a dose early or
late never moves it. An occurrence whose time has passed stays active and is shown as overdue, measured from the
first dose missed, and the app never skips or advances it on its own.

### Cooldown bars

Each medication row should:

- visually represent time remaining until the scheduled dose
- show the scheduled time and a concise countdown
- switch to a clear `OVERDUE` state once that time has passed
- support urgency/status styling
- be compact enough for several medications to stack vertically

### One-click Taken action

Each medication row should have one obvious **Taken** button.

Clicking it should immediately:

1. record the occurrence that is due as taken, with the actual click time
2. record any earlier unresolved occurrences as missed
3. advance to the next scheduled occurrence
4. persist the updated state
5. redraw the row against the new occurrence

The normal path should not show a modal confirmation. Editing and uncommon actions should stay separate from this primary action.

### Secondary interaction

- right-click medication row: context menu
- edit medication
- view dose history
- pause/resume medication
- remove medication
- drag widget
- optionally lock position
- optionally always-on-top
- system tray show/hide
- background colour, opacity, and solid/blur
- settings
- exit

### Start with Windows

The widget should launch automatically when the user signs in to Windows.

Requirements:

- enable startup during/after initial setup unless the user disables it
- use a lightweight native Windows startup mechanism
- do not create a Windows service solely for startup
- restore previous widget position and relevant UI state
- if medications are already configured, launch directly into the widget
- do not repeatedly show onboarding/setup after configuration is complete
- keep startup work minimal

### Local persistence

Store all data locally.

A small JSON file is sufficient for the MVP. Do not introduce SQLite unless later requirements justify it.

Suggested data shape:

```json
{
  "medications": [
    {
      "name": "Medication",
      "dose": "40 mg",
      "enabled": true,
      "schedule_type": "daily",
      "entries": [{ "day": 0, "minute": 510 }, { "day": 0, "minute": 750 }],
      "active_at": "2026-08-16T10:30:00Z",
      "history": [
        { "scheduled_at": "2026-08-16T06:30:00Z", "taken_at": "2026-08-16T06:41:00Z", "status": "taken" }
      ]
    }
  ],
  "settings": {
    "background_blur": true,
    "background_color": { "r": 0, "g": 0, "b": 0, "a": 99 }
  }
}
```

An hourly medication carries `interval_minutes` and `anchor_at` instead of `entries`. `minute` is minutes since local
midnight; `day` is a weekday (0 = Sunday) for weekly schedules and a day of the month for monthly ones. History
statuses are `taken`, `missed`, `paused`, and `resumed`, capped at the most recent 500 records per medication.

Files written before the schedule system carried `interval_minutes` and `last_taken_at`. They load as an hourly
schedule with the same interval anchored at the last dose, and the dose that was pending stays pending — overdue if
its time has already gone by. Nothing about an existing interval is rounded or altered. Settings keys that are absent
take their defaults, so older files load unchanged.

Timestamps are stored as UTC. The editor displays and accepts local Windows date/time values, and schedule entries
are local wall-clock times, so a fixed time stays fixed across a daylight-saving change.

## Current Development Build

The current build includes:

- compact stacked cooldown rows
- fixed hourly, daily, weekly, and monthly schedules that do not drift when a dose is recorded
- active countdown, `SOON`, `OVERDUE`, and paused states
- immediate persistence when **Taken** is clicked, resolving missed doses in the same press
- per-medication dose history from the row context menu
- add/edit with a schedule type and either an interval and start or a list of times
- right-click pause/resume and confirmed removal
- automatic current-user Windows sign-in startup after initial configuration
- system-tray show/hide, startup toggle, and exit controls
- widget dragging with restorable on-screen position
- persistent position locking and always-on-top controls
- optional local medication icons with native image selection and placeholder fallback
- configurable card background: solid or blur, with colour and opacity, previewed live
- per-monitor DPI-aware layout, explicit cooldown state labels, local timestamps, and keyboard navigation

MedAuras MVP is complete. The automated, resource-usage, restart/sleep, and full manual acceptance gates passed on 2026-08-13.

### Widget appearance

The widget is a single borderless window whose frame is rendered into a premultiplied 32-bit DIB, so Direct2D supplies
the authoritative per-pixel alpha and the rounded card corners and the gaps between rows are smoothly anti-aliased
against the desktop. There is no window region and no DWM system backdrop.

Each card's background is configurable: **solid** or **blurred**, with an RGBA colour. Blurred shows a live,
Gaussian-blurred view of whatever is behind the widget, tinted by that colour; solid shows the colour alone. In both
cases the alpha is real translucency and the card's text, icons, and progress bar stay fully opaque above it. Set it
from the row context menu → **Background...**, which previews live, or by editing `background_blur` and
`background_color` in the JSON. Cards are clipped to their rounded shape either way. That comes from Windows Composition: a backdrop brush feeds a blur effect, and masked sprite
visuals per card sit beneath the frame, which is uploaded to a composition surface rather than published with
`UpdateLayeredWindow`. Solid mode is the same tree without the blurred layer. Nothing captures the screen and the
compositor keeps the blur current without the app repainting. If composition is unavailable the window falls back to
`WS_EX_LAYERED`/`UpdateLayeredWindow` with the original opaque card gradient, and **Background...** is disabled since
there is no translucency to configure. Blur strength stays a design token (`blur_amount`).

Cards are 364 × 66 DIP and use a flat neutral grey ramp with hairline borders at roughly 12% white. Text steps
primary, secondary, muted; `SOON`, `DUE`, and `PAUSED` keep chromatic accents so state is never signalled by grey
alone. The vertical rhythm — padding, the name/dose line, a gap, the progress bar, deeper bottom padding — is derived
from named tokens in `DesignTokens` rather than inline coordinates. The medication name and its dose share one line
and are aligned on a common baseline computed from font metrics, so they stay aligned across DPI and font-size
changes.

Each row has one action control, **Taken**. Editing is reached by clicking the medication's icon tile, which
crossfades to a pencil glyph on hover; both the tile and the Taken circle show a hand cursor. The progress bar shows
the scheduled time of the tracked occurrence on the left and the countdown on the right; hovering replaces the
countdown with the full date and drops the short time. The countdown reads `1:20:00` above an hour and `MM:SS` below
it, then counts up as `MM:SS overdue`, `2h 14m overdue`, and `3d 4h overdue`. Every run is drawn twice against a clip
at the progress fill edge so the part over the bright fill and the part over the dark track each contrast what is
actually beneath them, splitting mid-glyph where it crosses.

The bar drains across the gap between the previous scheduled occurrence and the tracked one, and is empty once
overdue, where the `OVERDUE` badge, red border, and red countdown carry the state.

Editing has no Tab stop of its own, since the icon tile is painted rather than a control. It remains reachable from
the keyboard through the row context menu (Shift+F10 or the Menu key) → **Edit...**.

Selectable Solid/Mica/Acrylic background materials were implemented and then withdrawn; see
[`docs/DESIGN_PLAN.md`](docs/DESIGN_PLAN.md). Saved files that still contain `background_material` load normally — the
key is ignored and dropped on the next settings write.

### Build and run in CLion

Open this repository as a CMake project, reload CMake, select the `med-auras` CMake target, and run it. The automated
executable is the separate `medication-test` target.

Medication data is stored at:

```text
%LOCALAPPDATA%\MedAuras\medications.json
```

#### Toolchain state

The source compiles cleanly under both MSVC (`/W4 /permissive-`) and CLion's bundled MinGW, but **CMake configure
currently fails for MSVC 19.50 / Visual Studio 18**:

```text
target_compile_features no known features for CXX compiler "MSVC"
```

This is a CMake/compiler-version mismatch in `CMakeLists.txt`, not a source problem, and it predates the blur work.
Replacing the two `target_compile_features(... cxx_std_20)` calls with
`set_property(TARGET ... PROPERTY CXX_STANDARD 20)` is the known fix; it has not been applied. Until it is, MSVC can
only be verified by invoking `cl` directly, and MinGW is the only toolchain that configures.

#### Sandbox copies

A copy of the executable under any other name runs against its own state, so a test build cannot disturb real
medication data. The data file follows the executable's stem — `med-auras-ss.exe` reads and writes
`medications-med-auras-ss.json` beside `medications.json` — and the Windows Run entry does the same, so a renamed
copy can never repoint the real widget's sign-in launch at itself. A renamed copy also never claims a sign-in launch
on its own; its tray toggle still works if you deliberately want one. Both can run at the same time.

```bash
cp build/Release/med-auras.exe build/Release/med-auras-ss.exe
```

Keep `libwinpthread-1.dll` beside them, and re-copy after a rebuild — the copy is not a CMake target.

#### Standalone executable

The binary registered for Windows startup lives at `build\Release\med-auras.exe`. When built with MinGW it must be
linked so it does not depend on DLLs that only exist inside CLion, or it will fail to launch from Explorer or at
sign-in, where CLion is not on `PATH`:

```text
-DCMAKE_EXE_LINKER_FLAGS="-static-libgcc -static-libstdc++ -Wl,-s"
```

That removes `libstdc++-6.dll` and `libgcc_s_seh-1.dll`. A full `-static` link is not possible with CLion's bundled
MinGW — its static `libmsvcrt`/`libwinpthread` fail with `undefined reference to __intrinsic_setjmpex` — so
`libwinpthread-1.dll` remains a dependency and is kept beside the executable. **Keep those two files together.**

#### Versioning

Builds carry a version resource, set in one place — the `project(... VERSION ...)` line in `CMakeLists.txt` — and
generated into the executable through `src/version.h.in`. Check a binary with:

```bash
powershell -c "(Get-Item build\Release\med-auras.exe).VersionInfo.FileVersion"
```

To bump, edit that one `VERSION` line and set `MED_AURAS_VERSION_SUFFIX` to `""` for a release or `"-dev"` while the
work is in progress — nothing else needs editing. Copy each build worth keeping to `dist/med-auras-<version>.exe`.
Binaries stay out of git; `docs/VERSIONING.md` is the record of what each archived build was.

### Release verification

Run the automated model, persistence, legacy-file, malformed-file, and failed-write checks in each configured build:

```text
cmake --build <build-directory> --config Release
ctest --test-dir <build-directory> -C Release --output-on-failure
```

Before release, manually verify this daily-use path on Windows 11:

1. Configure medications, click **Taken**, terminate/relaunch, and confirm the saved schedule and active occurrence
   remain correct.
2. Exercise edit via the icon tile, pause/resume, removal, icon fallback, tray hide/show, startup enable/disable,
   position restoration, monitor removal, always-on-top, and keyboard operation including Shift+F10 → **Edit...**.
3. Confirm active timers remain correct across sleep, hibernation, Windows restart, and wall-clock advancement.
4. Exercise each schedule type: an hourly interval that does not divide 24 crossing midnight, a daily list, a weekly
   list, and a monthly entry on the 31st skipping a short month. Confirm scheduled times do not move when **Taken**
   is pressed early or late, that leaving a dose unattended shows a growing overdue duration measured from the first
   dose missed, and that one press then records the dose due now, logs the rest as missed, and lands on a future
   occurrence. Check **History...** shows them, and that a pause/resume cycle adds no missed entries.
5. Observe resource use in each state. Hidden, all-paused, and fullscreen-app-in-front should all schedule no repaint
   and hold CPU at effectively 0%. A visible, running countdown ticks once per second and costs roughly 0.5% of one
   core; confirm it returns to zero when the widget is hidden or covered by a fullscreen app, and that a card left
   overdue for more than an hour drops to a once-a-minute tick and then to hourly past a day.
6. Confirm memory, handle, USER, and GDI counts remain stable through repeated use and that the process owns no network
   endpoints. The per-second repaint makes handle stability worth rechecking specifically. Memory settles near 59 MB
   because of the Direct3D/composition device; what matters is that it is stable, not small.
7. Drag the widget across other windows and over a high-contrast wallpaper and confirm the card interiors track what
   is behind them, that the gaps between cards stay fully transparent, and that text, icons, and the progress bar
   stay sharp. Repeat in solid mode and at a low opacity, where text must stay fully opaque over a translucent card.
8. Confirm clicks pass through the row gaps and the rounded outer corners to whatever is beneath, and that the
   **Taken** button and the icon tile still respond. `WM_NCHITTEST` should answer `HTCLIENT` only inside a card.
9. Launch `build\Release\med-auras.exe` with CLion off `PATH` to confirm the shipped binary has no toolchain-local
   DLL dependency.

The 2026-08-13 development verification produced passing MSVC and CLion-MinGW Release builds/CTest runs. A 20-second
visible-idle sample used 0 CPU seconds, one thread, approximately 2.8 MB private memory, stable GDI/USER counts, and no
TCP or UDP endpoints.

After the seconds countdown was added, a 20-second sample with a running countdown measured 141 ms of CPU (about 0.7%
of one core) and a 90-second sample held handles flat at 218 with private memory oscillating between roughly 6.6 and
7.5 MB. Cursor parked on the icon tile, on the Taken button, and away from the widget each measured 0 ms over 8
seconds, confirming the hover and focus transitions settle rather than run continuously.

After live backdrop blur was added, a 20-second Release sample with a running countdown measured 94 ms of CPU (about
0.47% of one core, lower than before, since a composition-surface upload is cheaper than `UpdateLayeredWindow`) with
handles flat at 531. Hosting a Direct3D 11 device and the composition runtime raised private memory from roughly 7 MB
to 59 MB and thread count from 1 to about 69. No new timer, poll, or redraw loop was introduced.

The 2026-08-16 blur verification produced clean MinGW Debug and Release builds, a clean MSVC `/W4 /permissive-`
compile via `cl` (CMake configure for MSVC is broken, see above), CTest 1/1 in both MinGW trees, live blur confirmed
against a hard-edged colour-stripe window, `WM_NCHITTEST` returning `HTCLIENT` only inside cards, and the composition
path falling back to `WS_EX_LAYERED` with opaque cards when initialization is forced to fail. Real multi-monitor DPI
changes and sleep/resume were **not** re-tested against the new presentation path and remain open release checks.

## Status Styling

Suggested states:

- Soon: amber/orange
- Active countdown: neutral
- Overdue: red
- Paused: grey

The UI should remain readable without relying on color alone.

## Project Principles

1. Performance over framework convenience.
2. Near-zero-friction daily use after initial setup.
3. One-click dose recording from the widget.
4. Automatic Windows startup without a background service.
5. Simple native implementation over abstraction-heavy architecture.
6. Timestamp correctness over live counters.
7. Local-first and offline-only.
8. Compact UI over large application windows.
9. Clear behavior over feature quantity.

## Non-Goals for MVP

Do not add these unless explicitly requested:

- user accounts
- cloud sync
- mobile applications
- web dashboard
- analytics
- telemetry
- online medication databases
- prescription management
- pharmacy integration
- dosage recommendations
- medical advice
- complex calendar systems
- animated effects that increase idle resource usage

## Safety Boundary

This application is a personal timer/reminder utility only.

It must not:

- recommend medication doses
- decide whether a medication is medically safe to take
- alter prescribed intervals automatically
- present itself as a medical decision-making tool

Users configure their own medication names, doses, and intervals.

## Build Direction

Keep the executable and project structure small.

A reasonable initial structure is:

```text
src/
  main.cpp
  app.cpp
  app.h
  widget_window.cpp
  widget_window.h
  medication.cpp
  medication.h
  storage.cpp
  storage.h
  startup.cpp
  startup.h

assets/
  icons/

config/
  medications.json
```

Do not create extra layers, services, managers, interfaces, or abstractions unless they solve a concrete problem in the current scope.
