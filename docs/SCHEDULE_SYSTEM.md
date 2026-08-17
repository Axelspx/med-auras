# SCHEDULE_SYSTEM.md

**Status: implemented, 2026-08-16.** This document is the contract. The decisions section at the end records the
points that were resolved while building it, including one place where the implementation deliberately differs from
rule 6 below.

## Purpose

Replace the current free-running interval timer with a fixed recurring medication schedule.

A medication always tracks one **active scheduled occurrence**. Pressing **Taken** marks that occurrence as taken and immediately advances tracking to the next scheduled occurrence.

## Schedule types

### Hourly

Repeat every **N hours**, anchored to a chosen start time.

```text
Every 4 hours
Start: 08:30

08:30 → 12:30 → 16:30 → 20:30 → ...
```

### Daily

One or more fixed times every day.

```text
08:30
12:30
16:30
```

### Weekly

One or more weekday + time entries.

```text
Wednesday 12:00
Sunday    00:00
```

### Monthly

One or more day-of-month + time entries.

```text
1st  09:00
15th 18:00
```

If a selected day does not exist in a month, skip that occurrence.

## Core contract

1. Schedule times are fixed and must not drift based on when the user presses **Taken**.
2. Each medication has exactly one **active occurrence** at a time.
3. Pressing **Taken** marks the active occurrence as taken, records the actual taken timestamp, and advances to the next scheduled occurrence.
4. Taking a dose early or late does not move later scheduled doses.
5. If an active occurrence passes its scheduled time, it remains active and becomes **overdue**.
6. The app must not automatically skip or advance an overdue occurrence.
7. The next occurrence is calculated from the recurring schedule, not from `taken_at`.
8. Restarting the app must restore the correct active occurrence from persisted schedule/state data.

Example:

```text
Schedule:
08:30
12:30
16:30

Active: 12:30
Taken pressed at 12:47

Record:
scheduled_at = 12:30
taken_at     = 12:47

New active occurrence:
16:30
```

## Suggested model

```text
Medication
├── schedule_type
├── schedule_entries[]
└── active_occurrence

ScheduleOccurrence
├── scheduled_at
├── status
└── taken_at
```

Minimum occurrence statuses:

```text
pending
taken
```

Keep the model easy to extend later with statuses such as `skipped` or `missed`.

## UI behaviour

The medication card always represents the active occurrence.

Before due:

```text
Next dose
12:30
2h 14m
```

After due:

```text
12:30
35m overdue
```

After pressing **Taken**, the card immediately begins tracking the next scheduled occurrence.

### Potential UI Layout Example
---
(The following would be the layout for Daily, Weekly, Monthly)

Medication

[ Name 	]	[ Dosage 	]


Schedule interval

[ Weekly ▾ ] (Hourly, Weekly, Monthly)


Monday		08:00	[x]
Wednesday 	12:00	[x]
Friday		16:00	[x]

[ + add a time ]


[ Save ]	[ Cancel ] (buttons)

---
(The following would be the layout for Hourly)

Medication

[ Name 	]	[ Dosage 	]


Schedule interval

[ Hourly ▾ ] (Hourly, Weekly, Monthly)


Every        [ 4 ] hours
Starting at  [ 08:30 ]


[ Save ]	[ Cancel ] (buttons)

---

Those UI Layout Examples are only a rough example/guideline. If you can see/think of any minimal/simple UI/UX improvements, feel free to adapt the layout to that.


## Implementation rules

- Keep all next-occurrence calculation in one scheduling component/function.
- Do not calculate schedules independently in UI code.
- Do not use `last_taken + interval` to determine the next dose.
- Preserve existing persistence and low-resource/event-driven behaviour where possible.
- Schedule changes should recompute the active occurrence deterministically from the new schedule.

## Decisions

**Catch-up, and the deliberate deviation from rule 6.** A press of **Taken** records the occurrence that is due
*now* — the latest unresolved one at or before the click — marks every unresolved occurrence before it as `missed`,
and advances to the next. So one press after a three-day absence resolves everything and lands on a future
occurrence; it never leaves another past occurrence active, and there is no catch-up sequence of presses. This skips
occurrences, which rule 6 forbids, but the skip is user-initiated by the press rather than automatic. Nothing is ever
resolved by the passage of time alone: an unattended occurrence stays active and overdue indefinitely, and the app
writes nothing on its own. Missed doses are never presented as owed and the app never suggests making one up.

**Overdue is one duration, not a count.** It is measured from the first unresolved occurrence, and the scheduled time
the card displays is that same occurrence, so the number and the time next to it always agree. There is no "N doses
behind" language.

**Pause is a non-destructive suspend.** No occurrences accrue as missed while paused, which is what keeps a long hold
from producing a history full of noise. The history is retained, and resuming recomputes the active occurrence as the
next future one. Both events are written to the history so a gap reads as paused rather than unexplained. It exists
mainly because history makes removal destructive in a way it was not before.

**Every occurrence is logged**, viewable per medication from the row context menu, capped at the most recent 500
records — over a year at one dose a day.

**Hourly reuses `interval_minutes` plus an anchor timestamp** rather than a whole number of hours. This is what lets a
pre-schedule file migrate with its interval untouched; rounding a 30-minute schedule up to an hour would be the app
altering a prescribed interval, which `AGENTS.md` forbids. Hourly repeats continuously from the anchor and does not
restart each day, so an interval that does not divide 24 walks across days. Occurrences exist before the anchor too,
which is invisible except that a card tracking the first dose still has a previous occurrence to size its progress
bar against.

**The progress bar drains** across the gap between the previous scheduled occurrence and the active one, and reads
empty once overdue rather than saturating full — the `OVERDUE` badge, red border, and red countdown already carry the
state, and this keeps the existing visual language unchanged.

**Occurrence scanning is a day-by-day walk**, not modular arithmetic: generate the times a schedule places on a local
date, step the date until one lands on the right side of the cursor. Monthly's "skip a day the month does not have"
falls out for free, and the worst case is about 31 cheap iterations. Schedule entries are local wall-clock and are
converted to UTC per date, so a fixed time stays fixed across a daylight-saving change.

**Deferred:** a `skipped` status and a manual skip action to resolve a dose without recording one. `DoseStatus` has
room for it.