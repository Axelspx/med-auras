# MedAuras redesign/fix handoff

## Purpose

Continue the native C++ MedAuras UI geometry work from the exact state after Option A, Option B, and the layered-window cold-launch fix. Work directly in this checkout. Do not create a new task or discard/revert any existing changes.

## Required orientation

Read, in order:

1. `AGENTS.md` — native C++20/Win32 constraints, event-driven idle behavior, and change discipline.
2. `docs/DESIGN_PLAN.md` — authoritative redesign plan and the detailed Option A/B decisions and verification record. Start at “Geometry and rendering quality follow-up,” especially “Option A implementation assessment,” “Option B approval,” and “Option B implementation assessment.”
3. The current diff in `src/main.cpp` before editing.

Do not duplicate the design plan here; it contains the rationale, Microsoft documentation links, material tradeoffs, and measured QA results.

## Repository state at handoff

- Workspace: `C:\Users\axels\Documents\CLion\med-auras`
- Branch: `ui-overhaul`
- HEAD: `d3d8904 Introduce redesigned widget layout, styling, and interaction improvements`
- Tracked uncommitted files: `docs/DESIGN_PLAN.md`, `src/main.cpp`
- Untracked pre-existing/user-owned file: `CLAUDE.md` — preserve it and do not fold it into this work without an explicit request.
- This handoff file is intentionally new and untracked.
- `git diff --check` passes; only Git’s existing LF-to-CRLF warnings are reported.
- No MedAuras QA process was running when this handoff was written.

Preserve all current uncommitted changes. Do not reset, checkout, clean, stage, commit, or rewrite unrelated files unless the user explicitly asks.

## Current implementation

Option A centralized the floating-point DIP geometry in shared `DesignTokens`, `RoundedShape`, and `RowLayout` data and moved custom card, border, capsule, progress, panel, and button geometry to anti-aliased Direct2D primitives. Drawing, hover detection, native control placement, and hit testing derive from the same layout.

Option B now uses dual presentation:

- **Solid mode:** `WS_EX_LAYERED` plus a premultiplied 32-bit DIB published by `UpdateLayeredWindow`. Direct2D supplies the authoritative per-pixel alpha, so the binary `HRGN` is removed and desktop-facing corners/gaps are smoothly anti-aliased. Existing GDI text/icons preserve that alpha channel.
- **Mica mode:** retains the Option A backdrop plus `HRGN` path. Its desktop-facing outer edge therefore retains the documented binary-region limitation. A multi-window Mica redesign was deliberately rejected as disproportionate.
- Native child buttons remain for accessibility, Tab/Enter/Space behavior, tooltips, focus, hit testing, and command routing. Their geometry is shared with the layered visual.
- Progress hover invalidates only the old/new progress-bar bounds from `RowLayout`, rather than repainting the whole parent. This removed the reported transient white button squares.

Key code areas in `src/main.cpp`:

- `invalidate_progress_bar`
- `apply_widget_region` / `apply_background_material`
- `begin_d2d` and alpha-mode tracking
- `draw_action_button_geometry`
- `render_redesigned_widget` / `paint_redesigned_widget`
- `WM_MOUSEMOVE`, `WM_MOUSELEAVE`, and `WM_DPICHANGED`
- initial window show near the end of `wWinMain`

## Latest regression and fix

After Option B, Solid mode could cold-launch as a fully transparent layered window. Hiding and showing it worked because `show_widget()` invalidated the visible window, causing the first `UpdateLayeredWindow` frame to be published. The startup path invalidated while hidden, called `ShowWindow`, and entered the message loop without synchronously publishing a post-show frame.

The fix is deliberately one native call immediately after the initial `ShowWindow`:

```cpp
RedrawWindow(window, nullptr, nullptr, RDW_INVALIDATE | RDW_UPDATENOW);
```

Do not replace this with a timer or new lifecycle abstraction without evidence that the native synchronous redraw is insufficient.

## Verification baseline

After the cold-launch fix:

- MSVC Release build passed; CTest passed 1/1.
- CLion MinGW Debug build passed; CTest passed 1/1.
- A true cold launch, without hide/unhide, produced a visible window and an opaque/hit-testable card surface within approximately 60 ms.

Earlier Option B QA, recorded in `docs/DESIGN_PLAN.md`, also established:

- smooth live Solid-mode outer corners and transparent row gaps;
- correct zero-alpha hit-through at the outer corner and row gap;
- retained native Taken button hit testing;
- no near-white action-panel frame across 30 sampled hover-transition frames;
- simulated 125%, 150%, and 200% DPI geometry captures; and
- settled 30-second idle behavior of 0 CPU time with stable handles.

Configured build commands on this machine:

```powershell
& 'C:\Program Files (x86)\Microsoft Visual Studio\18\BuildTools\Common7\IDE\CommonExtensions\Microsoft\CMake\CMake\bin\cmake.exe' --build build --config Release
& 'C:\Program Files (x86)\Microsoft Visual Studio\18\BuildTools\Common7\IDE\CommonExtensions\Microsoft\CMake\CMake\bin\ctest.exe' --test-dir build -C Release --output-on-failure
& 'C:\Users\axels\AppData\Local\Programs\CLion\bin\cmake\win\x64\bin\cmake.exe' --build cmake-build-debug
& 'C:\Users\axels\AppData\Local\Programs\CLion\bin\cmake\win\x64\bin\ctest.exe' --test-dir cmake-build-debug --output-on-failure
```

If linking reports `Permission denied`, first identify the exact `med-auras.exe` process and verify its executable path before stopping only that QA instance.

## Next-session guidance

The Solid-mode geometry and startup regression are implemented and verified. The next agent should first reproduce the user’s current observation against the unmodified diff, then address only the next concrete issue the user names. Do not start speculative rendering cleanup.

Known intentional follow-ups, only if requested:

- user assessment of the final Solid-mode build in normal use;
- live Mica-path verification;
- real-monitor checks at Windows 125%, 150%, and 200% scaling (simulated checks already passed);
- a multi-window Mica/per-pixel-alpha design only if the user explicitly accepts its added complexity.

## Suggested skills

- `ponytail:ponytail` (full/default) for any code change: reuse the existing shared geometry and native Win32 mechanisms, make the smallest root-cause fix, and avoid new dependencies or speculative abstractions.
- `handoff` only when another compact continuation document is explicitly requested; update or supersede this file intentionally rather than scattering redundant handoffs.

