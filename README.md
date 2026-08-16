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
- Remaining time, or `READY`
- One obvious **Taken** button
- Optional small secondary text such as last-taken or next-dose time

The visual direction should take inspiration from WoW cooldown trackers: compact, readable, stacked bars with strong status visibility. Do not copy WoW assets or reproduce an addon skin exactly.

Example:

```text
[icon] Elvanse 40 mg                    [Taken]
       ██████████████ 13:24:07 ░░░░░░░░

[icon] Ibuprofen 400 mg                 [Taken]
       █████ 02:11:45 ░░░░░░░░░░░░░░░░░

[icon] Vitamin D                        [Taken]
       ████████████ READY ██████████████
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

Timer state must be calculated from timestamps:

```text
last_taken_at + interval = next_available_at
remaining = next_available_at - current_time
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
- interval between doses
- last-taken timestamp
- next-available timestamp derived from stored state
- enabled/paused state

When adding or editing a medication, the user chooses a start date/time (defaulting to now), an interval value, and
minutes, hours, days, or weeks as the unit. Fractional intervals are supported when they resolve to at least one whole
minute. For example, `3.5 days` becomes `5040` stored minutes. The selected start is the initial timer anchor; clicking
**Taken** later replaces it with the actual click time.

### Cooldown bars

Each medication row should:

- visually represent time remaining
- show a concise countdown
- switch to a clear `READY` state when available
- support urgency/status styling
- be compact enough for several medications to stack vertically

### One-click Taken action

Each medication row should have one obvious **Taken** button.

Clicking it should immediately:

1. record the current time as `last_taken_at`
2. derive the new next-available time
3. persist the updated state
4. restart/redraw the cooldown bar

The normal path should not show a modal confirmation. Editing and uncommon actions should stay separate from this primary action.

### Secondary interaction

- right-click medication row: context menu
- edit medication
- pause/resume medication
- remove medication
- drag widget
- optionally lock position
- optionally always-on-top
- system tray show/hide
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
      "interval_minutes": 1440,
      "last_taken_at": "2026-08-13T10:30:00",
      "enabled": true
    }
  ]
}
```

Timestamps are stored as UTC. The editor displays and accepts local Windows date/time values.

## Current Development Build

The current build includes:

- compact stacked cooldown rows
- `READY`, active countdown, and paused states
- immediate persistence when **Taken** is clicked
- add/edit with start date/time and minute/hour/day/week interval units
- right-click pause/resume and confirmed removal
- automatic current-user Windows sign-in startup after initial configuration
- system-tray show/hide, startup toggle, and exit controls
- widget dragging with restorable on-screen position
- persistent position locking and always-on-top controls
- optional local medication icons with native image selection and placeholder fallback
- per-monitor DPI-aware layout, explicit cooldown state labels, local timestamps, and keyboard navigation

MedAuras MVP is complete. The automated, resource-usage, restart/sleep, and full manual acceptance gates passed on 2026-08-13.

### Widget appearance

The widget is a single layered window. Every frame is rendered into a premultiplied 32-bit DIB and published with
`UpdateLayeredWindow`, so Direct2D supplies the authoritative per-pixel alpha and the rounded card corners and the gaps
between rows are smoothly anti-aliased against the desktop. There is no window region and no DWM backdrop.

Cards are 364 × 66 DIP and use a flat neutral grey ramp with hairline borders at roughly 12% white. Text steps
primary, secondary, muted; `SOON`, `DUE`, and `PAUSED` keep chromatic accents so state is never signalled by grey
alone. The vertical rhythm — padding, the name/dose line, a gap, the progress bar, deeper bottom padding — is derived
from named tokens in `DesignTokens` rather than inline coordinates. The medication name and its dose share one line
and are aligned on a common baseline computed from font metrics, so they stay aligned across DPI and font-size
changes.

Each row has one action control, **Taken**. Editing is reached by clicking the medication's icon tile, which
crossfades to a pencil glyph on hover; both the tile and the Taken circle show a hand cursor. The countdown reads
`1:20:00` above an hour and `MM:SS` below it, and is drawn twice against a clip at the progress fill edge so the part
over the bright fill and the part over the dark track each contrast what is actually beneath them, splitting
mid-glyph where it crosses.

Editing has no Tab stop of its own, since the icon tile is painted rather than a control. It remains reachable from
the keyboard through the row context menu (Shift+F10 or the Menu key) → **Edit...**.

Selectable Solid/Mica/Acrylic background materials were implemented and then withdrawn; see
[`docs/DESIGN_PLAN.md`](docs/DESIGN_PLAN.md). Saved files that still contain `background_material` load normally — the
key is ignored and dropped on the next settings write.

### Build and run in CLion

Open this repository as a CMake project, reload CMake, select the `med-auras` CMake target, and run it. Both MSVC and
CLion's bundled MinGW toolchain are supported. The automated executable is the separate `medication-test` target.

Medication data is stored at:

```text
%LOCALAPPDATA%\MedAuras\medications.json
```

### Release verification

Run the automated model, persistence, legacy-file, malformed-file, and failed-write checks in each configured build:

```text
cmake --build <build-directory> --config Release
ctest --test-dir <build-directory> -C Release --output-on-failure
```

Before release, manually verify this daily-use path on Windows 11:

1. Configure medications, click **Taken**, terminate/relaunch, and confirm the saved anchor and derived timer remain
   correct.
2. Exercise edit via the icon tile, pause/resume, removal, icon fallback, tray hide/show, startup enable/disable,
   position restoration, monitor removal, always-on-top, and keyboard operation including Shift+F10 → **Edit...**.
3. Confirm active timers remain correct across sleep, hibernation, Windows restart, and wall-clock advancement.
4. Observe resource use in each state. Hidden, all-ready/paused, and fullscreen-app-in-front should all schedule no
   repaint and hold CPU at effectively 0%. A visible, running countdown ticks once per second and costs roughly 0.7%
   of one core; confirm it returns to zero when the widget is hidden or covered by a fullscreen app.
5. Confirm memory, handle, USER, and GDI counts remain stable through repeated use and that the process owns no network
   endpoints. The per-second repaint makes handle stability worth rechecking specifically.

The 2026-08-13 development verification produced passing MSVC and CLion-MinGW Release builds/CTest runs. A 20-second
visible-idle sample used 0 CPU seconds, one thread, approximately 2.8 MB private memory, stable GDI/USER counts, and no
TCP or UDP endpoints.

After the seconds countdown was added, a 20-second sample with a running countdown measured 141 ms of CPU (about 0.7%
of one core) and a 90-second sample held handles flat at 218 with private memory oscillating between roughly 6.6 and
7.5 MB. Cursor parked on the icon tile, on the Taken button, and away from the widget each measured 0 ms over 8
seconds, confirming the hover and focus transitions settle rather than run continuously.

## Status Styling

Suggested states:

- Ready: green or clearly highlighted
- Soon: amber/orange
- Active cooldown: neutral/blue
- Overdue or missed scheduled action: red, only if scheduled-dose behavior is added
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
