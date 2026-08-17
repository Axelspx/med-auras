# AGENTS.md

## Project Mission

Build an extremely lightweight Windows medication timer widget using native C++20 and Win32.

The UI is inspired by World of Warcraft-style cooldown bars: compact stacked medication rows with an icon, medication name/dose, visual cooldown bar, remaining-time or `READY` indicator, and a single obvious **Taken** button.

Performance, simplicity, and near-zero-friction daily use are first-class requirements.

After initial configuration, the intended flow is:

```text
Windows starts -> widget appears -> user clicks Taken -> timer restarts
```

## Required Stack

- C++20
- Native Win32
- Windows 11
- Local JSON persistence
- GDI or Direct2D only where needed

Do not introduce Electron, WebView, Qt, Python, .NET, a browser UI, a web server, or a large third-party framework.

Prefer Windows APIs and the standard library.

## Primary Constraint: Resource Usage

The app should consume effectively 0% CPU while idle.

Do not implement timer state with a continuously running loop.

Bad:

```cpp
while (running) {
    updateTimers();
    Sleep(100);
}
```

Required approach:

- store timestamps
- derive remaining time from the current time
- use event-driven Win32 behavior
- only schedule timer callbacks when useful
- update visible countdown text no more often than necessary

Update the visible countdown only as often as its format actually requires, and only when someone can see
it.

The countdown currently shows seconds, which the user requested explicitly, so it ticks once per second while
a countdown is on screen. That costs roughly 0.5% of one core. It is the exception, not licence for
high-frequency work generally: a coarser format must go back to a coarser tick, and the overdue text does
exactly that — seconds for the first hour, then whole minutes to a 60-second tick, then whole hours to an
hourly one. No timer is scheduled at all when the widget is hidden or when every medication is paused, and
the repaint is skipped while a fullscreen or presentation-mode app is in front, which returns the process to
zero.

This constraint is about *recurring work*, and that part still holds exactly. The footprint no longer does:
the live backdrop blur hosts a Direct3D 11 device and the composition runtime, which costs roughly 59 MB and
about 69 threads against the 7 MB and single thread this section was written for. That was an explicit user
request, measured and recorded in `docs/DESIGN_PLAN.md`. Do not treat it as permission to relax the rules
above — no polling, no busy loop, and no timer that a visible countdown does not require.

Timer state must survive application restart, Windows restart, sleep, and hibernation.

## Timer Model

Store facts, not continuously changing countdown state. Never persist a value such as `remaining_seconds`.

A medication follows a **fixed recurring schedule** and tracks exactly one **active occurrence** at a time: the
earliest scheduled dose that has been neither taken nor missed. The schedule is authoritative. When a dose is
actually taken changes nothing about when later doses are due.

```text
schedule_type    hourly | daily | weekly | monthly
active_at        the tracked occurrence
remaining        active_at - now        (negative means overdue)
```

Rules that must not be broken:

- The next occurrence is derived from the schedule, never from `taken_at`. `last_taken + interval` is wrong.
- Taking a dose early or late does not move any later dose.
- An occurrence whose time has passed stays active and becomes overdue. Nothing auto-skips or auto-advances it.
- **Taken** records the occurrence that is due *now* — the latest unresolved one at or before the click — marks every
  unresolved occurrence before it as missed, and advances to the next. One press always resolves everything
  outstanding, so a press never lands on another past occurrence.
- Overdue is reported as one duration measured from the first unresolved occurrence. No dose count, and never a
  suggestion to make missed doses up.
- All next-occurrence calculation lives in `medication.cpp`. UI code must not work out when a dose is due.
- A schedule change recomputes the active occurrence from scratch. Editing only a name or icon leaves it alone.

Hourly is stored as `interval_minutes` plus an anchor timestamp, which is what makes a pre-schedule file migrate
without altering anyone's interval. It repeats continuously from the anchor and does not restart each day. Daily,
weekly, and monthly are lists of fixed local wall-clock times; a monthly day a given month does not have is skipped.

Timestamps are stored in UTC and presented and edited in local time. Schedule entries are local wall-clock, so a
fixed time stays fixed across a daylight-saving change.

Every resolved occurrence is appended to a per-medication history (`taken`, `missed`, `paused`, `resumed`), viewable
from the row context menu, capped at the most recent 500 records.

## MVP Scope

Implement only the current project requirements.

### Medication model

Each medication needs:

- unique ID
- name
- dose
- optional icon path/resource
- schedule type and its entries
- the active occurrence timestamp
- dose history
- enabled/paused state

For an hourly schedule the editor may accept fractional intervals in minutes, hours, days, or weeks, but must
normalize them to a positive whole-minute value for the model and JSON. For example, `3.5 days` is stored as `5040`
minutes. The other schedule types are lists of local times, with a weekday for weekly and a day of month for monthly.

Pausing is a non-destructive suspend, not a stopped clock: no occurrences accrue as missed while paused, the history
is kept, and resuming recomputes the active occurrence as the next future one. It exists so a medication can be set
aside without deleting its history.

### Main widget

Create a small borderless Win32 window with vertically stacked medication cooldown rows.

Each row should contain:

- icon
- medication name
- dose
- progress/cooldown bar
- remaining time or `READY`
- one obvious **Taken** button
- optional secondary timestamp

UI inspiration is WoW cooldown tracking, but do not use copyrighted game assets or attempt an exact clone.

## Primary Interaction: One-click Taken

The main daily action must require exactly one click from the widget.

When **Taken** is clicked:

1. record the current timestamp as `last_taken_at`
2. derive the new `next_available_at`
3. persist the state immediately
4. redraw that medication row with the restarted cooldown

Do not show a modal confirmation in the normal path. Do not require opening a menu, settings window, or secondary screen to record a dose.

The configured widget should be usable immediately after launch without setup friction.

## Secondary Interaction

Keep uncommon actions out of the primary workflow:

- right-click row for medication actions
- edit
- dose history
- pause/resume
- remove
- drag widget
- lock/unlock widget position
- optional always-on-top
- card background: solid/blur, colour, and opacity
- system tray show/hide
- settings
- exit

Avoid complicated interaction systems.

## Windows Startup

The app should launch automatically when the user signs in to Windows.

Requirements:

- use a lightweight native Windows startup mechanism
- do not create a Windows service just to keep the app available
- startup must be user-disableable
- restore saved widget position and relevant state
- if medications already exist, launch directly into the widget
- do not repeatedly show onboarding/setup after configuration is complete
- keep startup work minimal and preserve the idle-resource target

## Persistence

Use a small human-readable JSON file.

Do not add SQLite unless a later feature genuinely requires relational querying or substantial history.

Writes should be safe enough to avoid corrupting medication state on normal shutdown or interruption.

The one-click **Taken** action must persist immediately so a crash or restart does not lose the newly recorded timestamp.

## UI Rules

Prioritize glanceability and one-click operation.

Medication rows should be:

- compact
- vertically stackable
- easy to read quickly
- visually clear in `READY`, active, soon, and paused states
- easy to operate with one obvious primary button

Avoid:

- large headers
- unnecessary navigation
- large settings screens
- continuous animations
- decorative effects with ongoing CPU/GPU cost

The widget should look polished but remain cheap to render.

Short, bounded transitions are permitted where the user has asked for one: the icon tile's hover crossfade
and the focus ring's recede both run on a timer that stops itself when the transition completes. A transition
that cannot end, or that runs while nothing is being interacted with, is a continuous animation and is still
out of scope. The seconds countdown is likewise an explicit request rather than a default.

## Architecture Rules

Keep architecture proportional to the app.

Prefer simple modules such as:

```text
main
application lifecycle
widget window
medication model
storage
startup integration
settings
tray integration
```

Do not create speculative abstractions such as generic service containers, repository interfaces, dependency injection frameworks, plugin systems, or event buses unless a concrete requirement needs them.

Avoid overengineering.

## Safety Rules

This is a timer/reminder application, not a medical advisor.

Never implement logic that:

- recommends doses
- changes intervals based on medical reasoning
- decides that taking a medication is safe
- interprets symptoms
- substitutes for prescribing instructions

The user supplies medication details and timing rules.

## Change Discipline

When modifying code:

1. Read the relevant existing code before changing architecture.
2. Make the smallest coherent change that satisfies the requirement.
3. Preserve the low-resource/event-driven design.
4. Preserve the one-click daily workflow.
5. Do not add dependencies without a concrete justification.
6. Build and test after meaningful changes.
7. Keep warnings clean where practical.
8. Do not silently expand scope.
9. Version and archive any build worth keeping (see **Versioning** below).

Building, as of 2026-08-16: CMake configure fails for MSVC 19.50 (`target_compile_features no known features`), so
MinGW is the only toolchain that configures and MSVC must be checked by invoking `cl` directly. The fix is known and
recorded in the README but has not been applied. The executable at `build\Release\med-auras.exe` is the one Windows
startup launches; when built with MinGW it needs `-static-libgcc -static-libstdc++` and the `libwinpthread-1.dll`
kept beside it, or it will not start outside CLion.

### Versioning

Every build carries a version. The number lives in **one** place — the `project(... VERSION ...)` line of
`CMakeLists.txt` — and reaches the executable via the generated `version.h` and the `VS_VERSION_INFO` block in
`src/resources.rc`. To ship a build:

1. Bump `VERSION` in `CMakeLists.txt`. Pre-1.0 semantics: patch for a fix, minor for a user-visible feature.
2. Set `MED_AURAS_VERSION_SUFFIX` to `""` to release, or leave `"-dev"` while that version is still in progress.
3. Build, then confirm:
   `powershell -c "(Get-Item build\Release\med-auras.exe).VersionInfo.FileVersion"`
4. Copy to `dist/med-auras-<version>.exe` and add a row to the table in `docs/VERSIONING.md` saying what it was.

Do not add a second place to edit a version number, do not hand-edit a number into `resources.rc`, and do not pass the
version string as a `-D` define — `windres` mangles the quoting and the RC compile fails. Binaries are gitignored on
purpose; `docs/VERSIONING.md` is the record, not git. Full detail and the build history are in that file.

## Response Style

After completing code work, explain:

- what changed
- what the change means for the developer
- the before-and-after behavior
- whether there is any visible difference
- what remains intentionally unchanged

Keep replies compact and developer-facing rather than listing implementation details without explaining their effect.

## Current Priorities

Unless explicitly reordered, work in this general sequence:

1. Minimal native window and application lifecycle
2. Medication data model
3. JSON load/save
4. One working cooldown row
5. Timestamp-based timer behavior
6. One-click **Taken** action with immediate persistence
7. Multiple stacked rows
8. Windows startup launch and state restoration
9. Context menu/editing
10. Tray integration
11. Drag/lock/always-on-top behavior
12. Polish and resource-usage verification

Do not jump into non-MVP features before the core widget works reliably.

## Current Implementation State

The MVP is complete. All five milestones in `MILESTONES.md` passed their acceptance gates on 2026-08-13: the native
borderless widget, timestamp-derived cooldown rows, safe local JSON persistence, one-click **Taken**, multiple
medications, add/edit with start date/time and interval units, pause/resume, removal, current-user Windows sign-in
startup, tray show/hide, widget position restoration, drag/lock/always-on-top, medication icons, and per-monitor DPI
awareness.

Work since then is the post-MVP UI redesign recorded in `docs/DESIGN_PLAN.md`, which remains the authoritative record
for visual decisions, and the schedule system recorded in `docs/SCHEDULE_SYSTEM.md`, which replaced the free-running
interval cooldown with fixed recurring schedules, overdue tracking, and per-medication dose history. Pre-schedule
JSON files migrate on load to an hourly schedule anchored at their last dose, keeping the same interval and the same
pending dose. Current rendering state:

- One borderless top-level window. Each frame is drawn into a premultiplied 32-bit DIB, so Direct2D owns per-pixel
  alpha and card corners and row gaps are anti-aliased.
- The card background is a user setting — solid or blurred, with an RGBA colour — stored in the existing settings
  block of `medications.json` as `background_blur` and `background_color`, and edited from the row context menu via
  **Background...** with live preview. Do not add a second config file for it. Solid and blur are the same
  composition tree with and without the blurred layer; a translucent card **must** stay a composition layer rather
  than a painted fill, because GDI does not write alpha and a translucent fill drags the card's text opacity down
  with it.
- Cards sit on a live Gaussian-blurred backdrop supplied by Windows Composition (`src/blur.cpp`). The window is
  `WS_EX_NOREDIRECTIONBITMAP` with a desktop window target; the DIB is uploaded to a composition surface above two
  masked sprite visuals per card. If composition fails to initialize the window is created `WS_EX_LAYERED` instead
  and the original `UpdateLayeredWindow` path runs with opaque card fills. Do not give this window an owned popup:
  an owned top-most popup makes DWM stop presenting its composition target, which is why the tooltip has no owner.
  See `docs/DESIGN_PLAN.md`.
- Geometry is floating-point DIP, centralized in `DesignTokens` and resolved once per row by `RowLayout`. Drawing,
  native child placement, hover detection, and hit testing all consume that same layout.
- Colour is centralized alongside it as a neutral grey ramp with chromatic status accents.
- Native child `BUTTON` controls are retained for keyboard, focus, tooltips, accessibility, and command routing; their
  appearance comes from the composed layered frame. Because a layered window presents only its published bitmap, a
  control repainting itself is never seen: any hover, pressed, focus, or enablement change must invalidate the
  parent, and only on a real state transition, or the frame is republished continuously.
- Cards are 364 x 66 DIP. There is one action control, **Taken**. Editing is reached by clicking the icon tile,
  which crossfades to a pencil on hover; both it and the Taken circle show a hand cursor.
- The progress bar carries the scheduled time of the active occurrence on the left and the countdown on the right;
  hovering replaces the countdown with the full date and drops the short time. The countdown reads `1:20:00` above an
  hour and `MM:SS` below it, then `MM:SS overdue`, `2h 14m overdue`, `3d 4h overdue` once the occurrence has passed.
  Each run is drawn twice against a clip at the progress fill edge so it contrasts the colour actually beneath it.
- The bar drains across the gap between the previous scheduled occurrence and the active one, and reads empty once
  overdue, where the `OVERDUE` badge, the red border, and the red countdown carry the state.

Selectable Solid/Mica/Acrylic background materials were implemented and then removed. Do not reintroduce a DWM
system backdrop, `SetWindowRgn` silhouette, or second presentation path without reading the withdrawal rationale in
`docs/DESIGN_PLAN.md` first. The per-card composition blur is not a reversal of that: it is app-owned, per card, and
leaves the row gaps genuinely transparent.

Hosting Direct3D 11 and the composition runtime costs roughly 59 MB private memory and about 69 threads, up from
7 MB and 1. CPU is unaffected (slightly lower than before). That trade is documented rather than assumed.

## Definition of Done for MVP

The MVP is complete when the user can:

1. configure medications once
2. sign in to Windows and have the widget launch automatically
3. see previously configured medication cooldown bars immediately
4. record a taken medication with one click on its **Taken** button
5. see that medication's timer restart immediately
6. close/restart the app without losing timer correctness
7. see timers remain correct after sleep/restart
8. show/hide the widget from the system tray
9. disable automatic startup if desired
10. leave the app idle with effectively no measurable CPU activity

Item 10 now means: zero while hidden, zero while every medication is ready or paused, and zero while a
fullscreen app is in front. A visible, running countdown costs about 0.5% of one core because it displays
seconds at the user's request. It is a statement about CPU only — memory and thread count are no longer
small, for the reason given under Primary Constraint.
