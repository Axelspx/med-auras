# MedAuras completion milestones

This roadmap covers the remaining path from the current implementation to the product defined by
[`AGENTS.md`](AGENTS.md) and [`README.md`](README.md). Those files remain authoritative for product constraints,
safety boundaries, and non-goals. The order below closes the remaining MVP workflow first, adds each persisted setting
only when its UI exists, and leaves visual polish until behavior is reliable.

## Current baseline

The application already has the native borderless widget, timestamp-derived cooldowns, minute-level event-driven
refresh, atomic local JSON writes, one-click **Taken**, multiple medication add/edit/pause/remove flows, fractional
interval units normalized to whole minutes, UTC timestamps edited in local time, and automatic current-user startup
after initial configuration. `medication-test` covers the timer model and medication JSON round trips.

Every milestone must preserve immediate **Taken** persistence, fact-based timer state, local/offline operation, and an
idle message loop with no continuous polling. Work should stay in the existing small modules until a concrete feature
makes one additional source file clearer than keeping it in `main.cpp`.

## Milestone 1 — Tray lifecycle and user-controlled startup

**Status: Complete — automated builds/tests and manual interaction acceptance passed on 2026-08-13.**

### Outcome

MedAuras can remain available without occupying the desktop, and the user can show or hide it, disable or re-enable
sign-in startup, and exit from one predictable tray menu. This completes the two currently missing MVP lifecycle
controls.

### Scoped tasks

- Add a native notification-area icon after the widget window is created; remove it on orderly shutdown.
- Give the tray icon a menu with **Show/Hide**, **Start with Windows** (checked from the actual current-user Run value),
  and **Exit**. Double-clicking the icon should show and foreground the widget.
- Extend `startup.cpp` with the minimum query/disable operations needed by that menu. Treat the registry as the source
  of truth instead of duplicating the startup preference in JSON.
- Keep the process alive when the widget is hidden, but stop visible-countdown refresh timers while hidden and
  reschedule once when shown.
- Re-add the tray icon after Explorer recreates the taskbar. Report registry or tray failures without losing medication
  data or silently changing the requested setting.

### Verification / acceptance

- A configured launch still opens directly to the restored medication rows; **Taken** remains a one-click action.
- Tray **Hide**, **Show**, icon double-click, and **Exit** work repeatedly without duplicate icons or orphaned processes.
- **Start with Windows** can be switched both ways, its check state matches the Run entry after relaunch, and disabling
  it prevents a later sign-in launch.
- Restarting Explorer restores one functional tray icon.
- While hidden, MedAuras performs no minute repaint work; showing it derives the correct current state from timestamps.
- MSVC and MinGW builds succeed and `ctest` still passes.

### Intentionally deferred

Window dragging/position state, always-on-top, medication images, and cosmetic changes remain for later milestones. No
service, scheduled task, installer, or background helper is introduced.

## Milestone 2 — Restorable placement and window controls

**Status: Complete — automated builds/tests and manual interaction acceptance passed on 2026-08-13.**

### Outcome

The widget can be placed once and reliably returns there, with simple lock and always-on-top controls that survive
restart without affecting medication timer correctness.

### Scoped tasks

- Allow dragging from non-interactive widget background while unlocked; medication buttons and row context actions must
  retain their existing behavior.
- Add **Lock position** and **Always on top** to the existing secondary/context interaction surface, with visible checked
  states.
- Persist only the settings now required: last normal window position, lock state, and always-on-top state. Store them
  beside `medications` in the existing JSON file and keep files containing only the current `medications` array valid.
- Reuse the existing temporary-file replacement for every settings write so a failed preference save cannot overwrite
  medication state. Do not persist transient countdown or visibility values.
- Restore the saved position at launch, clamping it to a current monitor work area after resolution, scaling, or monitor
  changes. Persist a move after the drag completes rather than on every mouse movement.
- Apply topmost state with Win32 window positioning flags; do not add a keep-alive timer.

### Verification / acceptance

- Drag, restart, and sign-in launch restore the same usable position on the same display.
- Locked mode prevents dragging; unlocking restores it. Always-on-top takes effect immediately and survives restart.
- A saved off-screen position is recovered onto an attached monitor, including after a secondary monitor is removed.
- Existing medication-only JSON loads with default window settings, and a new settings round-trip test passes.
- Simulated write failure leaves the last valid JSON readable and rolls the visible setting back or clearly reports the
  failure.
- Active, ready, paused, restart, sleep, and hibernation timing remain timestamp-correct.

### Intentionally deferred

No snapping system, per-monitor position history, docking, resize framework, layouts, or generic settings service is
added. Tray visibility remains session state rather than another persisted preference.

## Milestone 3 — Medication icon selection and rendering

**Status: Complete — automated builds/tests and manual interaction acceptance passed on 2026-08-13.**

### Outcome

Each medication can optionally use a user-selected local image, while missing or invalid images degrade to a polished
placeholder and never block the core timer workflow.

### Scoped tasks

- Add an optional icon chooser/clear action to the existing medication editor using a native Windows file picker.
- Decode common local image formats with Windows Imaging Component and render a cropped or letterboxed square in the
  existing icon area. Add no third-party image library.
- Use the already-persisted `icon_path`; do not copy, upload, catalogue, or fetch images.
- Cache decoded/scaled image resources outside `WM_PAINT`, replace them after icon edits, and release them on removal
  and shutdown.
- Preserve the current placeholder when no icon is set or the path cannot be read. Editing, **Taken**, and startup must
  remain usable when an icon file is moved or corrupt.

### Verification / acceptance

- Select, replace, clear, save, and relaunch all show the expected icon or placeholder for multiple medications.
- BMP, PNG, JPEG, and ICO samples render without changing row geometry or stretching outside the icon bounds.
- Missing, unsupported, and corrupt files do not crash, delay startup materially, or modify timer data.
- Repeated repaint/edit/remove cycles show no growing GDI handle count and no decoding work during idle paints.
- MSVC and MinGW builds and the complete automated test set pass.

### Intentionally deferred

No bundled medication catalogue, web search, asset downloads, image editor, animation, SVG pipeline, or cloud storage is
added.

## Milestone 4 — Glanceability, DPI, and interaction polish

**Status: Implementation and automated verification complete; manual interaction acceptance pending.**

### Outcome

The finished widget is compact and legible across normal Windows 11 display scales, with distinguishable READY,
active, soon, and paused states and no continuous visual effects.

### Scoped tasks

- Refine spacing, typography, button prominence, progress-bar contrast, placeholder treatment, and focus indication
  without enlarging the workflow or adding navigation.
- Give READY, active, soon, and paused states both text and visual treatment so color is not the only signal. Define
  "soon" as a display-only threshold derived from the configured interval; it must never advise whether taking a dose
  is safe.
- Add one concise local-time secondary timestamp when it improves scanning, using only the stored anchor and derived
  next-available time.
- Make layout and hit testing DPI-aware, including row dimensions, icon sizing, restored position clamping, and
  `WM_DPICHANGED` handling.
- Verify keyboard access for medication buttons, editor controls, context commands, tray menu, and a clear escape/cancel
  path. Keep redraws event-driven and static.

### Verification / acceptance

- Several long medication names/doses remain readable or ellipsized without covering status or **Taken** at 100%,
  125%, 150%, and 200% scale.
- READY, active, soon, and paused can be identified in grayscale and by their text labels; **Taken** remains the single
  obvious primary action.
- Moving between monitors with different DPI keeps the window on-screen and controls clickable.
- Keyboard-only add/edit/take/pause flows work, and focus is visible.
- There are no animations, seconds-level updates, render threads, or new idle wakeups.

### Intentionally deferred

No theming engine, custom skins, sound, toast reminders, dose history, scheduled-dose semantics, or medical urgency
logic is added.

## Milestone 5 — Completion and release gate

### Outcome

The documented MVP and pending product behavior are demonstrated on Windows 11, with repeatable build/test evidence and
measured idle behavior suitable for a small daily utility.

### Scoped tasks

- Expand the existing small test executable only for durable non-UI logic introduced above: legacy/new JSON loading,
  settings round trips, interval/status boundaries, and failure-safe replacement behavior. Keep UI checks manual unless
  a regression cannot be covered below the Win32 surface.
- Produce clean Release builds with MSVC and CLion's bundled MinGW toolchain, run `ctest` for both, and resolve practical
  warnings.
- Execute the complete daily-use matrix: initial configuration, immediate **Taken** persistence, edit/pause/remove,
  close/relaunch, Windows sign-in startup, startup opt-out, tray hide/show, position restore, monitor removal, and icon
  fallback.
- Validate active timers across app restart, Windows restart, sleep, and hibernation using wall-clock timestamps rather
  than waiting on decremented state.
- Measure a representative idle period for ready/paused and hidden states. Confirm no periodic application timer is
  running there and CPU is effectively 0%; while an active countdown is visible, confirm refresh is no more frequent
  than once per minute.
- Check repeated use for stable process, memory, USER, and GDI resource counts; confirm the executable performs no
  network activity.
- Update `README.md` current-build status and manual verification instructions to match the shipped behavior.

### Verification / acceptance

- Every item in the `AGENTS.md` MVP definition of done passes on Windows 11.
- Both supported toolchains produce working Release executables and all CTest cases pass.
- A **Taken** click survives an immediate process termination/relaunch, and corrupted or failed writes do not replace
  the last valid saved state.
- Timers show the correct result after restart, sleep, hibernation, and wall-clock advancement.
- Idle measurement shows effectively 0% CPU with no busy loop, high-frequency timer, worker thread, or decorative
  rendering activity.
- The README, executable behavior, and persisted JSON shape agree.

### Intentionally deferred

Packaging/auto-update infrastructure, telemetry, accounts, sync, web/mobile clients, medication databases, scheduling
history, notifications, and all medical decision logic remain out of scope unless separately requested.
