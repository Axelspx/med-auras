# MedAuras UI Redesign Plan

Status: **approved for implementation on 2026-08-14**

Implementation status: **initial redesign implemented and verified on 2026-08-14**

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

## Implementation Verification

The initial implementation was completed on 2026-08-14 using the existing Win32/GDI renderer plus native DWM Mica support. Solid remains the persisted default/fallback. The fixed 400 × 80 DIP cards, 4 DIP shaped transparent gaps, redesigned state/progress styling, larger Taken check, direct-edit pencil, tooltips/accessibility names, keyboard controls, and progress-bar hover timestamp behavior are present.

Both configured toolchains built successfully and passed the automated medication/storage test:

- MSVC Release: build passed; CTest 1/1 passed.
- CLion MinGW Debug: build passed; CTest 1/1 passed.

Runtime visual QA used a two-medication stack at 400 × 164 pixels at 100% scale and confirmed the non-rectangular combined window region and four action controls. A 20-second visible-idle sample recorded 0 CPU seconds, approximately 2.22 MB private memory, three threads, stable handle/GDI/USER counts, and no TCP or UDP endpoints.

## Geometry-quality follow-up

Status: **requirements recorded and implementation options researched on 2026-08-14; awaiting user approval before code changes**

### Reported issue and required outcome

The redesigned layout and behavior are accepted, but close inspection shows visible irregularities in thin borders, rounded corners, capsules, circular controls, and nested containers, especially where DPI scaling produces fractional physical-pixel coordinates. The corrective work must address the shared rendering and geometry model rather than patching individual coordinates.

The approved visual design and functionality remain unchanged. The follow-up must:

- render borders, corners, capsules, buttons, progress tracks/fills, icon tiles, badges, action panels, and card containers with smooth, straight, symmetrical, reusable native primitives;
- derive fill, stroke, clipping, interaction state, and hit testing from one authoritative geometry for each component;
- centralize design tokens for radii, stroke widths, padding, gaps, and control sizes;
- calculate layout in floating-point DIPs and convert to physical pixels only at the rendering or HWND boundary;
- align thin strokes using the active DPI and the stroke centerline rather than unrelated integer offsets;
- make identical components use the same layout and drawing path;
- remove duplicated/overlapping strokes, separately calculated visual bounds, inconsistent radius semantics, and hard-coded optical corrections;
- preserve native C++20/Win32, one-click Taken persistence, keyboard/accessibility behavior, Mica/Solid selection, event-driven redraw, and the effectively-zero-idle-CPU target; and
- pass visual QA at 100%, 125%, 150%, and 200% Windows scaling.

### Current implementation findings

The artifacts are plausibly produced by several interacting causes in the current GDI path:

1. `scaled(int)` rounds every coordinate independently through `MulDiv`, so related edges, centers, diameters, and insets can round in different directions before drawing.
2. Layout is stored as integer `RECT` values. At 125% and 150% scale, valid DIP edges often fall between physical pixels, but that fractional information has already been discarded.
3. GDI `RoundRect`, `CreateRoundRectRgn`, pens, and region clipping are pixel/region based and do not provide Direct2D per-primitive anti-aliasing. The binary union `HRGN` is also the final outer-window silhouette, so its stair-stepped edge cannot be repaired by improving only the interior paint.
4. A nominal one-DIP border is created as an integer GDI pen after scaling. GDI centers the pen on integer geometry bounds, while the fill, outline, gradient clip, and window region use slightly different inclusive/exclusive edge adjustments (`+1`, inflation, and separate rectangles).
5. The card top highlight is a separate `MoveToEx`/`LineTo` segment rather than part of the card geometry or a geometry-derived clipped highlight. That can visibly disagree with the rounded card edge.
6. Buttons, cards, progress bars, badges, and panels reuse drawing helpers, but their layout, radii, fill bounds, outline bounds, child-window bounds, hover bounds, and parent-painted backgrounds are still calculated in separate places. This allows small mismatches and overlapping paint at component boundaries.
7. Capsules use a radius/diameter convention inconsistently: some calls pass a fixed scaled value while buttons derive the value from their already-rounded integer height. Equal logical shapes can therefore rasterize differently.

Microsoft's guidance supports moving geometry-sensitive drawing to Direct2D: Direct2D uses floating-point DIPs and maps them through the render target DPI, provides per-primitive anti-aliasing, and exposes native rounded-rectangle and ellipse geometries whose fill/stroke containment operations can also drive hit testing. GDI remains supported for interoperability, so the migration does not require replacing the Win32 application model, native child controls, persistence, timers, or tray behavior.

References:

- [DPI and device-independent pixels](https://learn.microsoft.com/en-us/windows/win32/learnwin32/dpi-and-device-independent-pixels)
- [Direct2D geometries overview](https://learn.microsoft.com/en-us/windows/win32/direct2d/direct2d-geometries-overview)
- [Direct2D anti-alias mode](https://learn.microsoft.com/en-us/windows/win32/api/d2d1/nf-d2d1-id2d1rendertarget-setantialiasmode)
- [Direct2D/GDI interoperability](https://learn.microsoft.com/en-us/windows/win32/direct2d/interoperability-overview)
- [SetWindowRgn](https://learn.microsoft.com/en-us/windows/win32/api/winuser/nf-winuser-setwindowrgn)
- [Direct2D pixel formats and alpha modes](https://learn.microsoft.com/en-us/windows/win32/direct2d/supported-pixel-formats-and-alpha-modes)

### Implementation options

#### Option A — Shared DIP layout plus Direct2D for all custom geometry

Replace the custom GDI shape helpers with Direct2D rounded rectangles, ellipses, gradients, geometry masks/clips, and strokes. Introduce one compact `DesignTokens` value set and one `RowLayout` result containing floating-point DIP geometry for every component. Painting, hover detection, and any custom hit testing consume that same layout. Keep native Win32 child buttons and existing GDI/Win32 text or WIC icon behavior where interoperation is sufficient.

Thin-stroke alignment is handled centrally: the helper transforms a DIP edge to device pixels, aligns the stroke centerline for the actual physical stroke width, and transforms it back before drawing. Fill and border consume the same rounded rectangle, with the stroke centered or explicitly inset by half its width according to one documented convention.

This is the smallest architecture change capable of fixing interior anti-aliasing and consistency. It preserves the current event-driven paint lifecycle and should not add idle work. Its limitation is that `SetWindowRgn` remains a binary outer mask, so the very outside card corners and transparent inter-card gaps may still show region-edge aliasing even when every interior primitive is smooth.

#### Option B — Option A plus a per-pixel-alpha window surface

Render the complete widget with Direct2D to a premultiplied-alpha surface and present it through a layered/composition window. Use the same card geometries both as alpha masks and as painted shapes, eliminating the binary `HRGN` from the visible silhouette and producing anti-aliased outer corners and gaps.

This is the only option that directly addresses both interior geometry and the final transparent window boundary. It is more invasive: transparent-surface text needs deliberate alpha handling, native child HWND composition must be verified or reworked, and the existing DWM Mica backdrop may not compose through the same per-pixel-alpha path. Mica compatibility therefore requires a focused prototype before this option can be approved as a drop-in replacement.

#### Option C — Normalize the existing GDI geometry only

Centralize tokens and layout, compute once, remove the standalone highlight line, standardize radius semantics, and derive all integer rectangles from a single DIP layout at the last possible moment, while retaining GDI `RoundRect` and `HRGN` rendering.

This would remove many asymmetries and duplicated calculations with the least code churn, but it cannot satisfy the requirement for consistently anti-aliased curves and outer edges at fractional scaling. It is useful only as a fallback or diagnostic step, not as the production-quality final fix.

### Proposed direction and approval gate

The recommended sequence is a bounded Option A rendering spike on one complete medication card, including both owner-drawn action controls, at all four target DPI values. The spike should prove the shared floating-point layout, geometry-based paint/hover logic, stroke alignment, device-loss recovery, Mica/Solid behavior, and unchanged idle scheduling. If only the binary outer silhouette remains visibly aliased, evaluate the smallest Option B composition prototype before deciding whether that additional complexity is justified.

No rendering implementation should begin until the user confirms an option. Coordinate-level visual patches and an open-ended rendering-framework rewrite are explicitly rejected.

### Option selection

On 2026-08-14, the user approved **Option A — Shared DIP layout plus Direct2D for all custom geometry** as the first implementation step. The result must be visually assessed before any decision about Option B. Option B remains unapproved and must not be introduced unless the completed Option A assessment shows that the binary outer-window region is still materially visible and the user explicitly accepts the additional composition work.

### Option A implementation assessment

Option A was implemented on 2026-08-14. All custom card, tile, badge, progress, panel, and action-button geometry now uses Direct2D rounded-rectangle/ellipse primitives with per-primitive anti-aliasing. One floating-point DIP `RowLayout` and centralized design-token set supplies drawing, native child placement, progress hover detection, card hit testing, and circular button hit testing. Fill and border geometry use one shared outer shape with a consistent inset-stroke convention. The separately painted card highlight line and the previous GDI `RoundRect`/gradient clip helpers were removed.

The Direct2D DC render target uses software rendering so geometry quality does not retain a hardware graphics-device footprint. A 50 ms one-shot cleanup timer shares Direct2D resources across a repaint burst and then releases the target and factory; this is event-scoped cleanup rather than continuous polling.

Verification results:

- MSVC Release build passed; CTest passed 1/1.
- CLion MinGW Debug build passed; CTest passed 1/1.
- Actual 100% visual QA passed for card geometry, capsules, icon tiles, action panel, and centered circular controls.
- Simulated `WM_DPICHANGED` captures at 125%, 150%, and 200% remained smooth, aligned, and symmetrical; real-monitor verification at those scales remains a release check.
- Progress hover still swaps to the factual timestamp using the shared rounded progress geometry.
- Native Taken button hit testing returned `HTCLIENT` at the circle center and `HTTRANSPARENT` at a square corner, without activating the action.
- A 20-second visible-idle sample recorded 0 CPU seconds, approximately 4.74 MB private memory, 11 threads, stable GDI/USER counts, and no handle growth.

The known Option A limitation remains: `SetWindowRgn` still applies an integer/binary union region to expose the desktop around cards and between rows. Direct2D now renders the interior borders and curves smoothly, but the final outside silhouette cannot have true per-pixel alpha while that `HRGN` remains. Solid-mode captures show the remaining limitation is confined to that outside boundary. Mica code paths and fallback behavior still compile, but Mica requires a live visual check after this renderer change.

Option B remains deferred pending the user's assessment of the completed Option A result.

### Option B approval and reported follow-up defects

On 2026-08-14, the user assessed Option A as substantially improved but still found the desktop-facing card-corner radii insufficiently smooth. The user approved beginning **Option B — Option A plus a per-pixel-alpha window surface** so the binary `SetWindowRgn` silhouette can be removed and the same anti-aliased card geometry can supply the final window alpha.

A second defect was reported at the same approval gate: hovering a progress bar to reveal its timestamp causes the native owner-drawn action buttons to flash white squares. This is not assumed to be an outer-alpha artifact. The current hover path invalidates the complete parent window while the two controls remain separate child `BUTTON` HWNDs with their own erase/owner-draw lifecycle. Option B must eliminate the flash by ensuring the action controls and their background are composed atomically, or by otherwise proving that retained native child controls cannot expose an intermediate erased surface.

Option B acceptance therefore requires both:

- genuinely anti-aliased per-pixel outer card corners and inter-card gaps with no visible binary region edge; and
- no white-square or intermediate-background flash when entering, leaving, or moving between progress bars.

Mica, keyboard navigation, accessible control names, one-click Taken behavior, tooltips, DPI handling, and idle-resource behavior remain protected requirements. If the native per-pixel-alpha presentation path cannot coexist with the current Mica system backdrop, implementation must stop at a documented material decision rather than silently removing or degrading Mica.

### Option B feasibility and material decision

Research and code tracing identified a viable native composition design:

- create the top-level popup with `WS_EX_NOREDIRECTIONBITMAP` so its normal opaque redirection surface does not cover transparent pixels;
- bind a DirectComposition target to the HWND and place one premultiplied-alpha Direct2D surface in a topmost visual;
- render the complete card stack, including action-button visuals and text, into that one surface before committing it;
- retain the existing native child `BUTTON` HWNDs beneath the topmost visual for Tab/Enter/Space behavior, tooltips, accessible names, focus, and command routing; and
- derive the visual surface alpha and transparent-window hit testing from the existing shared card geometry, removing the visible `SetWindowRgn` mask in the Solid path.

Microsoft documents that a DirectComposition target can be placed above an HWND's child windows and that child-window clipping depends on that target layer. This permits native controls to remain the interaction/accessibility layer while one atomic composition visual supplies their appearance. It should remove the white-square flash because a child button's intermediate erase/owner-draw surface is never exposed above the committed visual.

References:

- [DirectComposition target layering relative to child HWNDs](https://learn.microsoft.com/en-us/windows/win32/api/dcomp/nf-dcomp-idcompositiondesktopdevice-createtargetforhwnd)
- [DirectComposition surfaces and per-pixel alpha](https://learn.microsoft.com/en-us/windows/win32/directcomp/bitmap-surfaces)
- [`WS_EX_NOREDIRECTIONBITMAP`](https://learn.microsoft.com/en-us/windows/win32/winmsg/extended-window-styles)
- [`UpdateLayeredWindow` per-pixel alpha alternative](https://learn.microsoft.com/en-us/windows/win32/api/winuser/nf-winuser-updatelayeredwindow)
- [Mica as an opaque base-layer material](https://learn.microsoft.com/en-us/windows/apps/desktop/modernize/ui/apply-mica-win32)

The remaining decision is material behavior. Mica is an opaque whole-window backdrop; the Option B surface requires zero-alpha pixels around cards and in row gaps so the actual desktop remains visible. In the current one-HWND architecture, enabling the backdrop would fill those transparent pixels with Mica rather than expose the desktop. The viable scopes are:

1. **Solid-only Option B:** use per-pixel composition for every widget and remove/defer Mica. This is the smallest implementation and gives uniform smooth corners.
2. **Dual presentation:** use Option B for Solid and retain the Option A `HRGN`/backdrop path for Mica. Mica remains available, but its outer corners keep the known binary-region limitation. Shared layout and drawing code should still be reused; only presentation differs.
3. **Multi-window material composition:** split cards into separately shaped/backdropped HWNDs so Mica and desktop-visible gaps can coexist. This materially expands window lifecycle, positioning, input, tray, and accessibility complexity and is not recommended for this lightweight widget.

The implementation will use **dual presentation**. Solid mode receives a per-pixel-alpha layered-window path, while Mica retains the existing Option A `HRGN`/backdrop path. This preserves the current material option without expanding the widget into multiple coordinated top-level windows. The known binary outer-edge limitation therefore remains confined to Mica mode; solving that combination would require the separately scoped multi-window material design.

### Option B implementation assessment

The bounded Solid-mode implementation uses the smaller documented `UpdateLayeredWindow` alternative instead of retaining a Direct3D/DirectComposition device graph. The top-level popup becomes `WS_EX_LAYERED` only in Solid mode. Each requested frame is rendered into a top-down 32-bit DIB: Direct2D clears it to transparent, draws the shared rounded geometry with premultiplied alpha, and the existing GDI text/icon pass is then applied while preserving the authoritative Direct2D alpha channel. `UpdateLayeredWindow` publishes the complete bitmap in one operation. Solid mode no longer owns an `HRGN`; Mica removes `WS_EX_LAYERED` and keeps the existing backdrop/region path.

The hover flash was removed at its repaint source. Entering or leaving a progress bar now invalidates only the old and new progress-bar bounds derived from `RowLayout`, rather than invalidating the complete parent and causing unrelated child `BUTTON` windows to repaint. The parent also uses `WS_CLIPCHILDREN`. The layered frame contains the same shared action-button geometry, while the native buttons remain present for hit testing, keyboard input, command routing, tooltips, and accessibility.

Verification results:

- MSVC Release and CLion MinGW Debug builds passed; CTest passed 1/1 in both build trees.
- A live 100% Solid-mode capture showed smooth desktop-facing card corners and transparent inter-card gaps.
- Live hit testing passed through the zero-alpha outer corner and row gap to the desktop, retained the widget on the card body, and retained native button ID 100 at the Taken circle center.
- Progress hover displayed the timestamp correctly. Thirty action-panel samples taken across a live hover transition contained no transient near-white frame or square.
- Simulated `WM_DPICHANGED` captures at 125%, 150%, and 200% retained smooth, symmetrical card, capsule, and circular-control geometry. Real-monitor checks at those scales remain a release check.
- A settled 30-second visible-idle sample recorded 0 CPU seconds, approximately 8.34 MB private memory after the multi-DPI QA sequence, 16 threads, and stable handles (219 to 219).

Mica remains intentionally on the Option A region/backdrop presentation path and still needs a live material check. Perfect per-pixel desktop-facing Mica corners remain outside this bounded implementation for the architectural reason documented above.

## Background material withdrawn (2026-08-14)

Status: **implemented; Q11 and Q15 are withdrawn and the widget is Solid-only**

The deferred live Mica check was finally run and the material feature was withdrawn as a result. Q11 ("Solid and Mica only") and Q15 (persisted material submenu) no longer describe the product. Every statement about selectable materials earlier in this document is superseded by this section.

### What the live check found

1. **Mica never rendered as designed.** The non-layered path skipped the card fill so the backdrop could show through, but skipping is not the same as painting. The card interiors kept whatever pixels the redirection surface already held, so after switching from Solid they showed the previous opaque Solid frame. A fresh launch happened to start from a cleared surface, which is why Mica appeared to work once and then "broke" after toggling. The user's original report of toggle-dependent breakage was correct.
2. **The correct erase must come from GDI, not Direct2D.** A DWM backdrop is only visible through pixels left at zero alpha. GDI does not write the alpha byte, so a black `FillRect` clears stale content without making it opaque. An equivalent Direct2D clear under `D2D1_ALPHA_MODE_IGNORE` writes alpha 255 and blocks the backdrop completely. This was confirmed by trying the Direct2D version first and observing a fully opaque widget.
3. **Mica was judged too subtle once it did work.** Mica is specified as an opaque wallpaper-tinted material, not a transparency, so this is inherent rather than a defect.
4. **Acrylic was implemented, then rejected on cost.** `DWMSBT_TRANSIENTWINDOW` looked correct, but dragging the widget became visibly laggy. Sampling during interaction showed `dwm.exe` averaging above one full core while the application itself stayed near 7–11%, so the cost is compositor-side and not addressable from the paint path. Acrylic is also documented as a transient-window material, which suits menus and flyouts rather than a persistent widget that is repositioned by hand.

### Decision

Adopt scope option 1 from the material decision above — **Solid-only Option B** — and delete the material feature rather than keep a second presentation path. Dual presentation existed to preserve a choice that has now been evaluated and declined, and it was the source of every defect in this section.

### What was removed

- `BackgroundMaterial`, the `background_material` setting, its JSON read/write, and the right-click submenu.
- `apply_background_material`, `apply_widget_region`, and the whole `HRGN` silhouette path.
- All `dwmapi` usage, including `DWMWA_SYSTEMBACKDROP_TYPE`, `DwmExtendFrameIntoClientArea`, and the `Dwmapi` link dependency.
- The dual-path branching in `render_redesigned_widget` and `paint_redesigned_widget`; the layered path is now the only path and `WS_EX_LAYERED` is set once at window creation.

Consequences: the binary outer-edge limitation documented for Mica no longer exists anywhere, because every card is drawn through per-pixel alpha. Saved files containing `background_material` remain loadable; the key is ignored and dropped on the next settings write, and a test covers that.

## Minimal grey palette (2026-08-14)

Status: **implemented**

The redesign's colours were a blue-tinted grey — blue ran 14 to 20 points above red in nearly every value — which read as a themed surface rather than a neutral one. The user supplied a reference specifying a `#18181B` background, `#F4F4F5` primary text, `#A1A1AA` secondary text, and 1px borders at 12% white. Those anchors are the Zinc scale, so the ramp was extended consistently from them rather than invented.

Q6's requirement that state never depends on colour alone is unchanged. `SOON` amber, `DUE` red, and `PAUSED` grey keep their accents and their text labels.

- Surfaces step base (`#18181B`) to raised (`#27272A`) to elevated (`#3F3F46`).
- Borders are a computed hairline: 12% white composited over the base gives `RGB(52, 52, 56)`.
- Text steps primary, secondary, muted (`#71717A`).
- Colours are named constants beside `DesignTokens`, replacing roughly 35 scattered `RGB()` literals. This satisfies the geometry follow-up's requirement to centralize tokens, extended to colour.

The progress bar retains its original cool-toned track and fill at the user's explicit instruction. It is therefore the one element whose palette is deliberately inconsistent with its surroundings.

## Card and row layout revision (2026-08-14)

Status: **implemented**

Iterative visual QA on the implemented cards produced the following changes. Q2's fixed 400 DIP width is unchanged; the 80 DIP row height from Q3 is superseded.

- **Icon tile.** Was a lighter gradient (`#3F3F46` to `#2E2E30`) with a strong border, which read as a different material from every other raised surface. It is now a flat `surface_raised` fill with a hairline border, matching the badge.
- **Action panel removed.** The rounded container behind the Taken and Edit controls was removed outright, including its `RowLayout` shape and four design tokens. The controls now sit directly on the card. `action_panel_left` was renamed `action_area_left`, since it now only marks where the button cluster begins.
- **Name and dose share one line.** The dose no longer occupies its own row beneath the name; it follows the name's measured width plus a gap, and is skipped rather than clipped when a long name consumes the line. The two are aligned on a **common baseline computed from `GetTextMetrics`**, not by centring both in one band — centring two different font sizes cannot align their glyph bottoms, and a fixed pixel offset drifts across DPI and would silently break on a font-size change. The name font increased from 14 to 15 DIP, and the baseline alignment absorbed that automatically.
- **Optical inset.** The text line is nudged 3 DIP right of `content_left`. The name and the progress bar share the same x, but the bar is a capsule whose rounded ends make its apparent edge sit right of its true bounds; matching the geometry exactly looked wrong.
- **Row height 80 to 66 DIP.** The vertical rhythm is now derived from tokens — `row_padding` 8, `info_height` 22, `info_progress_gap` 4, `progress_height` 22, `row_padding_bottom` 10 — whose sum is `row_height`. Padding above and below is explicit, so the card is evenly spaced by construction. The bottom is 2 DIP deeper so the progress bar does not crowd the card edge.
- **Icon tile 56x64 to 48x48.** Now a true square spanning the padded height, with its image centred by a computed inset. `content_left` moved 72 to 64, giving text and the progress bar 8 DIP more width.
- **Badge collision.** Because the name and dose now occupy the badge's band, the narrower text variant (`info_line_with_state`) was restored so a long name cannot draw underneath `SOON` or `DUE`.

Every rect in `row_layout` now derives from named tokens; no standalone coordinate literals remain.

## Edit affordance, motion, and seconds countdown (2026-08-14)

Status: **implemented**

This round reverses parts of Q5, Q9, Q14, and Q16, and the "no animations" acceptance criterion. Each reversal was explicitly requested; they are recorded here so the earlier entries are not read as current.

### Edit moves to the icon tile (supersedes Q5 and Q9)

The pencil control is gone. Q5's "two icon-only circles" is now one: **Taken**. Editing is reached by clicking the medication's icon tile, which crossfades to a pencil glyph on hover. Removing the pencil and its gap freed 36 DIP, so the card narrowed from 400 to **364 DIP**, superseding Q3's fixed 400 DIP width. Text and progress widths are unchanged; only the right margin closed up.

Two costs were accepted rather than solved:

- **The edit action lost its Tab stop**, along with the tooltip and accessible name a real control provided. A painted region cannot carry them. Edit remains keyboard-reachable through the row context menu (Shift+F10 → **Edit...**), so there is still a keyboard path, but it is less discoverable than a focusable control. This is a real regression against Milestone 4's keyboard acceptance criteria.
- **The tile no longer drags the widget**, since it consumes the click. The rest of the card still does.

### Bounded motion (supersedes Q16 and the "no animations" criterion)

Q16 resolved on static hover, pressed, focus, and disabled feedback, and the acceptance criteria said no animations. Three transitions now exist, all bounded — each runs on a timer that kills itself on arrival, so a settled widget schedules no work:

- the icon tile's hover crossfade, where the tile's own content recedes as the pencil arrives and the tile lights to the same `surface_elevated` the action button uses;
- the focus ring receding after a **pointer**-driven focus. Keyboard focus keeps a solid ring, because fading it unconditionally would remove the only visible focus indicator and break Milestone 4's "focus is visible". The distinction is drawn by flagging `WM_LBUTTONDOWN` before the control takes focus.

Measured: cursor resting on the tile, on the Taken button, at either boundary, and away from the widget all held 0 ms of CPU over 8 seconds.

### Layered windows do not show child control state

A latent defect surfaced here. The Taken button's hover, pressed, and focus visuals had existed since Option A but were never visible: a layered window presents only the bitmap published by `UpdateLayeredWindow`, so a child control repainting itself never reaches the screen. Two things were required, and the first alone was not enough:

1. the parent must be invalidated when the control's state changes, and
2. the state must be one the control actually reports. `BST_HOT` never materialised for this control, so hover is tracked directly in the subclass via `WM_MOUSEMOVE`/`WM_MOUSELEAVE`, the same way the progress bar and icon tile already worked.

Invalidating on every mouse move republished the whole frame continuously — measured at 281 ms/8 s while moving and 109 ms/8 s after the pointer had left. Repainting only on a genuine state transition, compared against a value cached in the subclass reference data, returned it to zero.

### Countdown format and refresh cadence (supersedes Q14)

The countdown now reads `1:20:00` above an hour and `MM:SS` below it, replacing `13h 24m`. Hours are unpadded and omitted when zero; below an hour the field is fixed-width.

Showing seconds requires a one-second tick, which supersedes the minute-level refresh assumed throughout this document. Nothing seconds-level is persisted: `remaining_seconds` derives from the same stored anchor, so the timestamp model and restart/sleep correctness are untouched.

Cost, measured: **about 0.7% of one core** while a countdown is visible. It returns to zero when the widget is hidden, when every medication is ready or paused, and when a fullscreen or presentation-mode app is in front — the last via `SHQueryUserNotificationState`, which skips the repaint while still ticking, because the tick is what notices the fullscreen app leaving. A plain maximised window on top is not detected; that ceiling is marked with a `ponytail:` comment in the source.

### Progress text contrast

The countdown sits over a bar whose fill is bright and whose track is dark, so a single text colour could not stay legible across the sweep. It is now drawn twice against a clip at the fill edge, each half coloured by `contrasting_text()`, which picks dark or light from the Rec. 709 luma of whatever is beneath. A glyph crossing the boundary splits mid-character. Both the fill and the clip take the edge from one shared `progress_fraction()`, so they cannot drift apart. `DUE` keeps its red rather than deferring to the generic rule, since that colour is a state signal.

## Live backdrop blur behind the cards (2026-08-16)

Status: **implemented**

Implements `docs/BLUR_IMPLEMENTATION.md`: each medication card now shows a live, Gaussian-blurred view of whatever
is behind the widget, tinted dark, clipped to the card's rounded shape. The gaps between cards stay fully
transparent and the foreground stays sharp. Nothing captures the screen, and no DWM injection is involved.

This is the first change since the 2026-08-14 withdrawal to reintroduce a see-through material. It is not a
reversal of that decision: what was withdrawn was a *system backdrop* (`DWMWA_SYSTEMBACKDROP_TYPE`) applied to the
whole window, which could not coexist with per-pixel-transparent gaps and cost more than a full core in `dwm.exe`
during drags. What is added here is an app-owned composition visual per card. The `SetWindowRgn` silhouette and the
second presentation path stay gone.

### Why the renderer had to change

A layered window presents only the bitmap given to `UpdateLayeredWindow`. A `CompositionBackdropBrush` samples what
the compositor has behind the window, which a layered window's bitmap can never contain. The two are mutually
exclusive, so the widget moves to a composition target:

- the top-level window is created `WS_EX_NOREDIRECTIONBITMAP` instead of `WS_EX_LAYERED`;
- `ICompositorDesktopInterop::CreateDesktopWindowTarget` binds a visual tree to it, above the child controls;
- the existing frame is rendered exactly as before — Direct2D geometry into a premultiplied DIB, then the GDI text
  and icon pass — and that finished bitmap is uploaded into a composition drawing surface instead of being handed
  to `UpdateLayeredWindow`.

The drawing code is untouched. Only the publish step changed, which is why the text pipeline, baseline alignment,
progress-text clipping, and DPI behaviour all carry over unchanged rather than being ported to DirectWrite.

### Effect graph

Deliberately the smallest thing that matches the reference:

```text
CompositionBackdropBrush -> D2D1GaussianBlur (sd 20, HARD, SPEED) -> CompositionEffectBrush
```

`Windows.UI.Composition` consumes an effect through `IGraphicsEffectD2D1Interop`, which `blur.cpp` implements
directly. That avoids Win2D and keeps the dependency list unchanged apart from `d3d11`/`dxgi`/`dxguid`.

**The dark tint is a separate composition brush, not a node in the graph.** The reference recipe composites the tint
inside the effect; doing that here would have been equivalent only if the tint stayed off the foreground. Because
the card's own text and icons are drawn into the same bitmap, giving the card a translucent fill would have made the
text translucent with it — GDI does not write alpha, so the text inherits whatever alpha the fill left behind. Two
masked sprite visuals per card (blur, then tint) put the translucency entirely below the foreground layer, and cost
less code than hand-writing `Composite` and `Flood` effect classes.

### Rounded corners

Each card layer is a `CompositionMaskBrush` whose mask is one shared drawing surface containing the card silhouette,
filled with `FillRoundedRectangle`. A composition clip would have given binary edges — precisely the defect that
Option B existed to remove. All cards are the same size, so one mask surface and two brushes serve every card; only
the sprite visuals are per-card.

Geometry comes from the same `RowLayout` that drawing and hit testing use, so the painted border and the blurred
interior cannot drift apart at any DPI.

### Hit testing

A layered window passed clicks through its zero-alpha pixels for free. A composition window does not, so
`WM_NCHITTEST` now returns `HTTRANSPARENT` for anything outside the card shapes, using the same `contains()` test as
the rest of the widget. Verified: card bodies `HTCLIENT`, row gaps and rounded corners `HTTRANSPARENT`, the Taken
button still resolves to control ID 100.

### The tooltip defect

Composition initialised cleanly, every call returned `S_OK`, the visual tree reported its children — and the window
presented nothing. A standalone window with identical styles worked, and so did a second window created inside the
running app, which narrowed it to the widget window itself. Bisecting the window procedure found it in `WM_CREATE`:

**Giving a `WS_EX_NOREDIRECTIONBITMAP` window an owned top-most popup makes DWM stop presenting that window's
composition target entirely.** The popup was the tooltip control, owned by the widget since the MVP.

The fix is to create the tooltip with no owner. Ownership bought nothing: `TTM_ADDTOOL` names the widget as the tool
window and `TTF_SUBCLASS` does the message relaying, both independent of the tooltip's owner. `WS_EX_TOOLWINDOW`
keeps the ownerless popup out of the taskbar. Tooltips still work.

Worth remembering when adding any future owned popup to this window.

### Fallback

If any part of the path is unavailable — no `CoreMessaging`/`combase` entry points, no Direct3D device, no
compositor, no backdrop brush — `blur::initialize` reports failure *before the window is created*, so the window
takes `WS_EX_LAYERED` and the original `UpdateLayeredWindow` path runs with the opaque card gradient restored. The
ordering matters: the window's extended style is fixed at creation. Verified by forcing initialisation to fail: the
widget renders exactly as it did before this change. Failures are logged with `OutputDebugString` in debug builds.

### Resource cost, measured

| | before | after |
|---|---|---|
| CPU, visible with a running countdown | ~0.7% of one core | **0.47%** of one core |
| private memory | ~7 MB | **59 MB** |
| threads | 1 | **~69** |

CPU went *down*: uploading the frame to a composition surface is cheaper than `UpdateLayeredWindow`. No new timer,
poll, or redraw loop exists — the compositor keeps the backdrop live on its own, so the widget still repaints only
for the countdown tick, and handles stayed flat at 531 over the sample.

Memory and thread count are the real cost, and they are substantial against this project's stated mission. They are
inherent to hosting a Direct3D 11 device and the composition runtime in-process, not something the paint path can
reclaim. This is the trade the feature buys; it deserves a decision rather than an assumption.
