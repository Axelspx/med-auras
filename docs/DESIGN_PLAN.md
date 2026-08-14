# MedAuras UI Redesign Plan

Status: **approved for implementation on 2026-08-14**

Reference: `C:\Users\axels\Desktop\MedAurasDesign.png`

## Goal

Overhaul the MedAuras widget toward the supplied dark, rounded, high-contrast concept while preserving the project's native Win32 implementation, one-click daily workflow, timer correctness, accessibility, and effectively zero CPU use while idle.

## Feasibility

The design is feasible in native C++20/Win32. The current widget already custom-paints each medication row with GDI, loads icons through WIC, scales geometry per monitor DPI, and redraws only for events or useful minute-boundary timer updates. The proposed surfaces can therefore remain static and event-driven.

The following are straightforward custom-painted elements:

- dark rounded row background and border;
- rounded icon tile with image or initial fallback;
- medication name, dose, state badge, countdown, and next-time label;
- rounded cooldown track and fill;
- circular action surfaces, glyphs, focus/hover/pressed states;
- DPI-scaled sizing and spacing.

The outer rounded silhouette and soft shadow require deliberate window treatment. They are still possible, but the exact approach will be chosen only after the intended silhouette and transparency are settled. No animation or continuous rendering is required.

## Reference Design Reading

The supplied concept shows one wide medication card with:

- a near-black blue/charcoal background with a subtle lighter edge and rounded outer corners;
- a large rounded icon/dose tile on the left;
- a large medication label and outlined `ACTIVE` badge;
- a wide pill-shaped cooldown bar with the remaining time inside its right edge;
- a muted calendar glyph and `Next: 17 Aug 10:28` secondary line;
- a grouped action panel on the right containing circular check and cross controls;
- restrained highlights and shadows that create depth without visible animation.

The visual hierarchy is strong, but the meaning of the two icon-only actions is not yet defined. The screenshot is a direction rather than a pixel-perfect specification.

## Existing Baseline

- One borderless `WS_POPUP` widget, currently 420 device-independent pixels wide.
- Medication rows are custom-painted in `src/main.cpp` using GDI.
- Each row currently has a native child `BUTTON` labelled `Taken`.
- Icons are already loaded through Windows Imaging Component, with an initial fallback.
- `READY`, active/soon, and paused states already exist.
- Right-click provides edit, pause/resume, remove, add, position lock, and always-on-top actions.
- Keyboard navigation, per-monitor DPI scaling, tray behavior, persistence, and minute-level event-driven refresh already exist.
- The working tree was clean when this design discussion began.

## Non-Negotiable Project Constraints

- Recording a dose remains a single obvious click from the widget and persists immediately.
- No confirmation is added to the normal Taken path.
- Status must not rely on color alone.
- The UI must not imply medical approval or recommend whether a dose is safe.
- Rows remain compact enough to stack, including with long names and multiple medications.
- Rendering stays event-driven with no continuous animation or high-frequency polling.
- The implementation remains native C++20/Win32 using GDI or Direct2D only where justified.
- DPI scaling, keyboard focus, icon fallback, sleep/restart correctness, tray behavior, saved position, and idle-resource behavior must be preserved.
- No new UI framework or speculative theming architecture will be introduced.

## Likely Implementation Direction (Not Yet Approved)

Prefer extending the existing GDI paint path rather than changing rendering stacks. Replace the standard child Taken buttons only if custom-painted controls are needed to match the concept; if replaced, preserve keyboard activation, focus indication, hit testing, disabled state, and accessible naming. Use static gradients/highlights sparingly and redraw only after state or interaction changes.

Exact dimensions, controls, state styling, window silhouette, and scope remain open until the grilling is complete.

## Decision Log

### Q1 — Meaning of the two right-side controls (Resolved, later revised)

The concept contains a check and an `X`, but the current daily workflow has one primary **Taken** action and keeps removal/pause/edit behind the context menu.

**Initial decision:** retain two circular controls. The check is the one-click **Taken** action. Replace the concept's `X` with a pause/resume toggle.

The pause/resume control changes glyph and accessible name to reflect the action it will perform. Pausing disables Taken and gives the entire row a clearly labelled paused treatment. Resuming restores normal timer behavior. Edit and confirmed removal remain in the right-click menu.

The Taken action must have an accessible name and tooltip. During implementation, its persistent visible affordance must remain unmistakable rather than relying on an unexplained check glyph or color alone.

**Revision:** the secondary control will use a pencil glyph for editing rather than pause/resume. Pause/resume remains in the existing right-click secondary menu. The exact pencil behavior is addressed by Q9.

### Q2 — Row scale and stacking density (Resolved)

The reference is approximately 1795 × 421 pixels for one row and uses intentionally spacious proportions. Applied literally, a few medications would occupy much of a normal desktop and weaken the project's compact stacked-widget goal.

**Decision:** treat the image as a visual reference rather than a literal pixel-size target. Keep the redesigned row compact at approximately **80 DIP tall and 360–400 DIP wide**. This is close to the existing 420 × 76 DIP footprint and preserves practical multi-row stacking.

For a 1440-pixel-tall monitor, a 132 DIP row occupies approximately:

| Windows display scale | Physical row height | Share of screen height |
| --- | ---: | ---: |
| 100% | 132 px | 9.2% |
| 125% | 165 px | 11.5% |
| 150% | 198 px | 13.8% |
| 175% | 231 px | 16.0% |
| 200% | 264 px | 18.3% |

These percentages exclude the small gap and outer widget padding between stacked rows. If “1400p” means exactly 1400 physical pixels rather than the usual 1440p, 132 physical pixels at 100% is approximately 9.4% of the screen height.

**User decision:** Compact 80 DIP height and 360–400 DIP width preferred.

### Q3 — Exact baseline width and resizing (Resolved)

A fixed baseline makes layout, hit targets, ellipsis behavior, saved positioning, and visual QA deterministic. Making the widget horizontally resizable would add interaction and persistence work without a stated daily-use benefit.

**Decision:** use a fixed **400 × 80 DIP row canvas**, plus minimal outer gap/padding between rows. Do not add user resizing. Use a compact icon tile, a flexible center information region, and two small but accessible right-side action buttons. Long medication names and doses ellipsize rather than changing row height.

**User decision:** Accepted.

### Q4 — Icon tile content and dose duplication (Resolved)

The reference places a large initial and dose inside the left tile while also including the dose in the main heading. At 400 DIP wide, repeating the dose consumes space needed for medication names, status, the timer, and two usable action targets.

**Decision:** use the left tile only for the configured medication icon, falling back to a large initial. Show the medication name and dose once in the main information area, with the name dominant and dose secondary. Do not repeat the dose inside the icon tile.

**User decision:** Accepted.

### Q5 — Persistent Taken label versus two icon-only circles (Resolved, amended)

At compact scale, two circular glyph-only controls most closely match the reference. However, a check mark can mean confirm, complete, dismiss, or select. A tooltip and accessible name help discovery and assistive technology but do not make the primary daily action continuously obvious on sight.

**Decision:** use two icon-only circular controls, but make the check/Taken control physically larger and visually stronger than the secondary pencil/edit control. Do not show the word `Taken` in the row. Both controls retain tooltips, keyboard focus treatment, and explicit accessible names. The size difference establishes the check as the primary action.

**User decision:** Icon-only controls accepted with a larger primary check button; secondary glyph later changed from pause/resume to pencil/edit.

### Q6 — Explicit state vocabulary and color treatment (Resolved)

The reference shows an `ACTIVE` badge. MedAuras also needs visibly distinct soon, ready, and paused states, and the differences cannot depend on color alone.

**Decision:** remove the `ACTIVE` label entirely because the visible countdown and progress bar already communicate a normal running timer. Use `SOON` in amber, replace `READY` with `DUE` in red, and retain `PAUSED` in grey. Meaning is always communicated with text as well as color.

`DUE` means only that the user-configured interval has elapsed. It must not be worded or presented as a determination that another dose is medically safe.

The existing model defines `SOON` as a positive remaining duration no greater than 10% of the medication's configured interval, with a minimum threshold of one minute. The redesign will preserve that rule unless scope is explicitly changed later.

**User decision:** Remove `ACTIVE`; use amber `SOON`, red `DUE`, and grey `PAUSED`.

### Q7 — Cooldown bar direction (Resolved)

The existing bar represents time remaining: it starts full immediately after Taken and drains toward empty as the configured interval elapses. This matches a conventional cooldown display and requires no model change.

**Decision:** preserve the draining behavior. Normal cooldown uses the reference's cool pale fill; `SOON` shifts to amber; at `DUE`, the fill is empty and the track/border plus `DUE` text turn red.

**User decision:** Accepted.

### Q8 — Meaning of pause (Withdrawn from visible-control redesign)

The current `enabled`/paused behavior does not freeze clock time or rewrite the timer anchor. While paused, Taken is disabled and the row uses paused styling; when resumed, remaining time is recalculated from the original timestamp, so it may immediately be `DUE` if the configured interval elapsed during the pause.

A literal frozen countdown would be a different feature. It would need additional timestamp facts and resume logic, and could make the displayed next time diverge from the real interval since the last recorded Taken action.

**Outcome:** the visible pause/resume button is no longer proposed. Pause/resume stays in the existing context menu, and its current timestamp-based semantics remain intentionally unchanged by this visual overhaul. The row still needs the defined muted `PAUSED` presentation when the existing action is used.

**User decision:** Superseded by changing the secondary visible button to edit.

### Q9 — Pencil button behavior (Resolved)

The pencil can either open the medication editor immediately or display the same context menu normally reached by right-clicking the row. A pencil conventionally promises direct editing; opening a menu first would require a second click and duplicate the existing right-click entry point.

**Decision:** clicking the pencil opens that medication's existing edit dialog directly. Right-click continues to open the complete secondary menu containing Edit, Pause/Resume, Remove, and widget actions.

**User decision:** Open the edit dialog directly.

### Q10 — Multiple-row container treatment (Resolved)

The reference defines one rounded medication card, while MedAuras can display several vertically stacked medications. The stack can be rendered either as individually rounded cards with small gaps or as one continuous rounded outer container with internal separators.

**Decision:** vertically stack individually rounded 400 × 80 DIP medication cards close together with a slight genuinely transparent gap between them. They remain parts of one logical popup widget rather than independent OS windows, but the desktop showing through the gaps makes each card appear visually separate.

The exact gap will be tuned during visual QA, beginning at approximately 3–4 DIP. Rounded outer corners should also expose the desktop rather than showing a rectangular backing canvas.

**User decision:** Separate-looking cards with slight transparent gaps inside one widget.

### Q11 — Configurable background material (Resolved)

The user would like a choice between solid, Mica, Acrylic, and blur. The supported Windows materials and project constraints make these options unequal:

- **Solid:** simplest, deterministic, cheapest, and closest to the supplied dark mockup. Existing GDI painting can provide static depth with no compositor material.
- **Mica:** a performant, long-lived-window material that takes a subtle tint from the wallpaper rather than showing a live blurred view through the card. Microsoft recommends Mica for persistent app surfaces. On supported Windows 11 builds, native DWM can apply the main-window system backdrop; unsupported configurations must fall back to solid.
- **Desktop Acrylic:** live frosted transparency showing content behind the widget. It is visually strongest but is GPU-intensive, can be disabled by Battery Saver or the user's Transparency Effects setting, falls back to solid in several environments, and Microsoft primarily recommends it for transient surfaces rather than an always-visible widget.
- **Legacy blur:** not a useful separate supported option. Microsoft's documented `DwmEnableBlurBehindWindow` no longer produces blur beginning with Windows 8. Undocumented composition-attribute techniques would create compatibility debt. Desktop Acrylic is the modern supported blur-like choice.

Windows 11's `DWMWA_SYSTEMBACKDROP_TYPE` supports whole-window Mica and Desktop Acrylic beginning with build 22621. The intended minimal route is the native DWM attribute with runtime capability/failure fallback, not adopting the Windows App SDK, WinUI, XAML, or an additional runtime. The Windows App SDK's controller route would add substantial initialization and deployment machinery that is disproportionate to this native widget.

The main engineering risk is not the setting itself. It is ensuring that the system-drawn whole-window backdrop, the custom GDI foreground, and a non-rectangular union of rounded card regions produce genuine transparent gaps without artifacts. This needs a small rendering spike and visual verification on the target Windows 11 system before treating all materials as guaranteed.

**Initial recommendation:** expose exactly three choices under a single `Background material` setting:

1. `Solid` — default and guaranteed fallback;
2. `Mica` — recommended optional system material for a persistent widget;
3. `Acrylic` — opt-in frosted effect with an explicit understanding that Windows may replace it with solid.

Do not expose a separate `Blur` option. Preserve static dark tint/contrast layers above Mica and Acrylic so text and red/amber states remain readable. Store one small enum in the existing widget settings and expose it through the existing context/tray settings surface; do not build a general theme system.

**Decision:** implement only `Solid` and `Mica` for now. `Solid` is the default and guaranteed fallback. If Mica is selected but unavailable, disabled by system policy, or fails to initialize, render Solid without disrupting the widget. Acrylic and any separate blur mode are intentionally deferred.

Store one small material enum in the existing widget settings and expose it through the existing secondary settings surface. Do not introduce a general theme architecture or Windows App SDK dependency.

**User decision:** Solid and Mica only for the initial redesign.

### Q12 — Card shadows and depth (Resolved)

The reference uses soft outer shadows plus internal highlights. With multiple shaped cards inside one popup, true blurred shadows extending around every card would complicate alpha composition and enlarge the window's shaped regions. Subtle depth can instead be painted inside each card boundary using a dark gradient, fine border, and top-edge highlight.

**Decision:** omit true external drop shadows in the first implementation. Use a restrained internal gradient, 1 DIP border, and soft top/left highlight to reproduce the dark dimensional character without compromising the transparent gaps. Revisit compositor shadows only if the visually verified result still looks too flat.

**User decision:** Accepted.

### Q13 — Typography and glyph source (Resolved, amended)

The 80 DIP row cannot use the reference's oversized typography literally. It also needs crisp check, pencil, and calendar glyphs at several DPI scales without adding image assets or an icon library.

**Decision:** use Windows' native Segoe UI family for all text and Segoe Fluent Icons (with Segoe MDL2 Assets fallback where needed) for the check and pencil glyphs. Suggested hierarchy: medication name approximately 14 DIP semibold; dose approximately 11 DIP regular; countdown approximately 12–13 DIP semibold; state and timestamp approximately 10–11 DIP. Keep text off pure white, use muted cool grey for secondary information, and validate ellipsis at 400 DIP width. Do not bundle a custom font or icon dependency. The later hover-timestamp decision removes the need for a separately drawn calendar glyph.

**User decision:** Accepted.

### Q14 — Countdown and hover timestamp content (Resolved)

The reference shows concise remaining time inside the bar and a calendar-prefixed next timestamp below it. At 80 DIP height, keeping both visible consumes valuable vertical space.

**Decision:** use a single text slot inside the progress bar. Its normal content is the concise glance state, and hovering anywhere over that medication's progress-bar bounds immediately swaps it to the relevant factual local timestamp:

- Normal/`SOON`: `80h 34m` becomes `17 Aug 10:28`.
- `DUE`: `DUE` becomes `Since 17 Aug 10:28`.
- `PAUSED`: `PAUSED` becomes `Was due 17 Aug 10:28`.
- No anchor: retain `DUE` on hover because no factual due timestamp exists.

The text reverts immediately when the pointer leaves the bar. There is no fade, animation, extra timer, or persistent secondary timestamp line. A `WM_MOUSEMOVE`/`TrackMouseEvent` plus `WM_MOUSELEAVE` state transition is sufficient; the current code has no existing hover tracking. Redraw only when the hovered bar changes, not on every mouse-move message.

The hover timestamp is supplementary detail; the primary countdown/state remains continuously visible, and the underlying timestamp remains available in the edit dialog. Therefore the core state is not made hover-only.

**User decision:** Requested and accepted as the compact timestamp presentation.

### Q15 — Material-setting location (Resolved)

Solid/Mica is an uncommon widget preference and does not belong in the primary row workflow.

**Decision:** add a `Background material` submenu to the existing right-click widget menu, with mutually exclusive checked `Solid` and `Mica` entries. Persist the choice immediately in existing widget settings and repaint/reapply the backdrop without restarting. Do not create a settings window for this single option.

**User decision:** Accepted.

### Q16 — Icon-button interaction feedback (Resolved)

Because Taken and Edit use glyphs without persistent text labels, their interactive states must make hit targets and meaning clear without introducing animation.

**Decision:** use static state changes only:

- default: visible circular boundary and high-contrast glyph;
- hover: slightly lighter fill/border;
- pressed: slightly darker/inset treatment;
- keyboard focus: clear 1–2 DIP focus ring;
- disabled Taken on paused rows: muted glyph/fill with no hover response;
- tooltips: `Taken` and `Edit medication`;
- accessible names match those tooltip labels;
- `Enter`/`Space` activates the focused control, and Tab order visits Taken before Edit.

No pulsing, fades, ripples, or continuous animation. The Taken target remains physically larger than Edit while both satisfy a practical minimum pointer target in the constrained 80 DIP row.

**User decision:** Accepted.

## Consolidated Approved Design

The proposed redesign is now fully specified, subject only to the final shared-understanding approval gate:

- one fixed-width popup widget containing vertically stacked medication cards;
- each card is 400 × 80 DIP with approximately 3–4 DIP of genuinely transparent separation;
- rounded dark cards follow the supplied reference's visual direction without copying its literal size;
- configured icon or large initial fallback in the left tile; dose is not duplicated there;
- medication name is primary and dose is secondary in the center information region;
- no redundant `ACTIVE` badge;
- amber `SOON`, red `DUE`, and grey `PAUSED`, always communicated by text as well as color;
- progress starts full after Taken and drains toward empty; normal fill is pale/cool, soon is amber, and due is an empty red treatment;
- progress-bar text normally shows remaining duration or state and swaps immediately to the factual local timestamp while hovered;
- larger circular check control records Taken in one click; smaller circular pencil opens the medication editor directly;
- pause/resume, confirmed removal, and other secondary actions remain in the right-click menu;
- icon-only buttons have static hover/pressed/focus/disabled states, tooltips, accessible names, and keyboard activation;
- native Segoe UI typography and Segoe Fluent/MDL2 glyphs; no bundled font/icon library;
- internal painted gradient, fine border, and top/left highlight provide depth; true external shadows are deferred;
- user-selectable Solid and Mica materials via a persisted right-click submenu; Solid is the default and fallback;
- Acrylic, legacy blur, animation, resizing, a settings window, new framework dependencies, and a general theme system are out of scope.

## Implementation Outline

1. Add the persisted Solid/Mica enum to existing widget settings with backward-compatible Solid default.
2. Refactor row geometry to a single 400 × 80 DIP layout and 3–4 DIP stack gap.
3. Introduce a shaped union of rounded card regions so gaps/corners expose the desktop; reapply on row count and DPI changes.
4. Add native DWM Mica application with runtime fallback to Solid and immediate menu switching.
5. Extend the existing event-driven GDI paint path for rounded surfaces, internal depth, typography, state colors, progress bar, and glyph controls.
6. Replace/rework the standard Taken controls only as needed to achieve the specified custom appearance while preserving native keyboard behavior and accessible naming; add the Edit control.
7. Add minimal hit testing and state-change-only redraw for button hover/press/focus and progress-bar timestamp hover.
8. Preserve current context-menu actions, one-click Taken persistence, timestamp model, minute-boundary refresh scheduling, DPI behavior, tray behavior, position restoration, and icon loading.
9. Build and run automated tests under supported MSVC and MinGW configurations, then visually verify Solid/Mica, scaling, long text, multiple cards, every state, keyboard use, transparent gaps, and idle resource behavior.

## Acceptance Criteria

- The configured widget still records Taken with one click and persists before showing the restarted cooldown.
- Cards measure 400 × 80 DIP and form a close, aligned stack with visible desktop through their gaps and rounded corners.
- Solid matches the dark reference direction; Mica applies when supported and silently falls back to a readable Solid treatment otherwise.
- Names/doses remain legible or ellipsize cleanly at the fixed width and common DPI scales.
- Normal, Soon, Due, and Paused treatments match the decisions above and never rely on color alone.
- Hovering only the progress bar swaps its text exactly as specified and leaving restores it without animation.
- Taken and Edit work by mouse and keyboard, expose meaningful tooltips/accessibility names, and show static interaction states.
- Right-click behavior and all existing secondary features remain available.
- Hidden state has no refresh timer; visible timers update no more often than required; no continuous render loop or animation is introduced.
- Existing automated tests pass, GDI/USER handles remain stable, and idle process CPU remains effectively zero.

## Remaining Decisions

None. Exact color values, corner radii, internal padding, 3-versus-4-DIP gap, and minor optical alignment are implementation tuning details constrained by this plan and will be settled through visual QA rather than further product decisions.

## Interview Context

- 2026-08-13: User supplied `MedAurasDesign.png` as a rough design goal and requested a small grilling session to lock a shared plan before implementation.
- 2026-08-13: Feasibility confirmed after inspecting the reference at original resolution and reviewing the current GDI paint path, native Taken buttons, popup-window setup, DPI handling, and input behavior.
- 2026-08-13: Q1 resolved: check means Taken; the second circular control is a pause/resume toggle rather than an `X`.
- 2026-08-13: Q2 resolved: target approximately 80 DIP tall and 360–400 DIP wide per row.
- 2026-08-13: Clarified that DIP size depends on Windows display scaling; documented the physical size of a 132 DIP row at common scale settings on a 1440p display.
- 2026-08-13: Q3 opened: choose an exact fixed baseline or introduce width variability/resizing.
- 2026-08-13: Q3 resolved: fixed 400 × 80 DIP rows; no user resizing.
- 2026-08-13: Q4 opened: decide whether the compact icon tile repeats the dose shown in the main information area.
- 2026-08-13: Q4 resolved: icon/initial only in the tile; name and dose appear once in the main information region.
- 2026-08-13: Q5 opened: decide between reference-faithful icon-only circles and a persistently labelled Taken control.
- 2026-08-13: Q5 resolved: two icon-only circles; Taken/check is physically larger than pause/resume and remains explicitly named for tooltips/accessibility.
- 2026-08-13: Q6 opened: agree on explicit state badge vocabulary and restrained state accents.
- 2026-08-14: Q6 resolved: no `ACTIVE` label; `SOON` is amber, elapsed state is red `DUE`, and `PAUSED` remains grey. `DUE` is explicitly a configured-timer state, not medical advice.
- 2026-08-14: Verified from the model that `SOON` currently begins at the final 10% of the interval, with a minimum one-minute window.
- 2026-08-14: Q7 opened: confirm whether the cooldown bar continues to drain from full to empty.
- 2026-08-14: Q7 resolved: retain a full-to-empty draining bar, with pale normal fill, amber soon treatment, and an empty red due treatment.
- 2026-08-14: Q8 opened: clarify whether pause preserves current timestamp-based semantics or literally freezes remaining time.
- 2026-08-14: Visible secondary action revised from pause/resume to a pencil/edit control. Pause/resume remains in the right-click menu with existing semantics, so Q8 is withdrawn from the redesign scope.
- 2026-08-14: Q9 opened: determine whether the pencil opens the edit dialog directly or opens the context menu.
- 2026-08-14: Q9 resolved: pencil opens the selected medication's editor directly; the full context menu remains on right-click.
- 2026-08-14: Q10 opened: decide how the single-card reference becomes a multi-medication stack.
- 2026-08-14: Q10 resolved: close vertical stack of rounded cards with slight true transparency between rows, implemented as one logical widget.
- 2026-08-14: Q11 opened: decide whether dark cards are opaque painted surfaces or live translucent glass.
- 2026-08-14: Q11 expanded after the user proposed selectable Solid/Mica/Acrylic/Blur modes. Official Windows guidance was checked. Recommendation is Solid/Mica/Acrylic only, with Solid as default/fallback and no legacy/undocumented Blur mode.
- 2026-08-14: Q11 resolved: initial redesign supports Solid and Mica only. Solid is the default/fallback; Acrylic and Blur are deferred.
- 2026-08-14: Q12 opened: choose internal painted depth or true external compositor shadows.
- 2026-08-14: Q12 resolved: use internal painted depth; defer true external shadows pending visual QA.
- 2026-08-14: Q13 opened: agree on native Windows typography and glyph sources for the compact layout.
- 2026-08-14: Q13 resolved: native Segoe UI typography and Segoe Fluent/MDL2 glyphs; no font or icon dependency.
- 2026-08-14: Q14 opened: define countdown and secondary timestamp wording across normal, due, and paused states.
- 2026-08-14: Q14 resolved: remove the persistent secondary line and swap the progress-bar text to the relevant local timestamp only while that bar is hovered.
- 2026-08-14: Verified that the current renderer has no hover tracking; implementation needs a minimal enter/leave state with redraws only when hover state changes.
- 2026-08-14: Q15 opened: choose where the Solid/Mica preference is exposed.
- 2026-08-14: Q15 resolved: persisted Solid/Mica submenu in the existing right-click menu; changes apply immediately.
- 2026-08-14: Q16 opened: agree on feedback and accessibility behavior for the two icon-only controls.
- 2026-08-14: Q16 resolved: static hover/pressed/focus/disabled feedback, tooltips, accessible names, and keyboard activation accepted.
- 2026-08-14: Grilling branches consolidated. No unresolved product decisions remain; awaiting final shared-understanding approval before implementation.

## Approval Gate

The user explicitly confirmed shared understanding and approved this plan on 2026-08-14. Implementation may proceed in a subsequent code-change task, following the implementation outline and acceptance criteria above.
