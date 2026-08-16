# SCHEDULE_SYSTEM.md

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