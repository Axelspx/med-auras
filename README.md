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
[icon] Elvanse 40 mg          13h 24m   [Taken]
       ███████████████░░░░░░░░░░░░░
       Next dose: tomorrow 10:30

[icon] Ibuprofen 400 mg        2h 11m   [Taken]
       █████░░░░░░░░░░░░░░░░░░░░░░
       Last taken: 09:14

[icon] Vitamin D                READY   [Taken]
       █████████████████████████████
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
