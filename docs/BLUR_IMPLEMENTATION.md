# BLUR_IMPLEMENTATION.md

## Goal

Implement a true live transparent blur behind each medication card.

The target appearance is the simple **Blur** effect produced by the combination of ExplorerBlurMica + DWMBlurGlass:

- live desktop/window content visible through the card;
- strong smooth Gaussian blur;
- neutral dark translucent tint;
- no acrylic noise texture;
- no strong saturation/luminosity processing;
- rounded card corners;
- fully transparent gaps between cards.

Do **not** copy DWMBlurGlass's DWM injection/hooking architecture. Reproduce only its visual blur recipe inside this app.

---

## Reference blur recipe

DWMBlurGlass's `BlurBackdrop` is effectively:

```text
Backdrop
    ↓
Gaussian Blur
    ↓
Dark translucent tint
    ↓
Card content
```

Use these values as the initial reference:

```text
Blur standard deviation: 20.0
Border mode:             HARD
Optimization:            SPEED
Tint:                    black / dark neutral
Tint opacity:            ~0.39
Noise:                   none
Saturation processing:   none
```

The blur amount should be centralized as a design token so it can be tuned later.

---

## Required implementation approach

Use **Windows Composition** for the live backdrop effect.

The card blur should be built from:

```text
Compositor
    └── BackdropBrush
          ↓
       GaussianBlurEffect
          ↓
       translucent dark tint
          ↓
       CompositionEffectBrush
          ↓
       SpriteVisual
          ↓
       rounded clip matching the medication card
```

The effect should use the same underlying concepts as DWMBlurGlass:

- `Compositor::CreateBackdropBrush()`
- `D2D1GaussianBlur`
- `D2D1_GAUSSIANBLUR_PROP_STANDARD_DEVIATION`
- `D2D1_BORDER_MODE_HARD`
- `D2D1_GAUSSIANBLUR_OPTIMIZATION_SPEED`
- tint composited over the blurred backdrop

Prefer the public Windows Composition path. Do not hook or inject into DWM.

---

## Important existing-renderer constraint

The app currently uses a layered-window presentation path with a premultiplied 32-bit DIB and `UpdateLayeredWindow`.

Do not try to fake the blur inside that bitmap by:

- taking desktop screenshots;
- repeatedly capturing the wallpaper;
- blurring copied background bitmaps;
- applying a Direct2D Gaussian blur to the app's own already-rendered pixels.

Those methods are not true live backdrop blur.

Before changing the renderer, inspect the current presentation code and `docs/DESIGN_PLAN.md`. The project has previously experimented with backdrop/presentation changes, so preserve the reasons documented there unless this implementation explicitly replaces them.

Do not introduce two competing permanent presentation systems.

---

## Recommended architecture

Keep the current application logic unchanged:

- medication model;
- timers;
- persistence;
- input handling;
- tray/startup behaviour;
- scheduling;
- existing layout calculations.

Change only the visual presentation needed for blur.

Recommended composition structure:

```text
HWND
└── Composition target
    └── Root visual
        ├── Medication card 1
        │   ├── blurred backdrop visual
        │   └── existing foreground/card content
        ├── Medication card 2
        │   ├── blurred backdrop visual
        │   └── existing foreground/card content
        └── ...
```

Each card must have its own blur visual so the spaces between cards remain genuinely transparent.

---

## Card blur construction

For each medication card:

1. Use the existing card rectangle/layout as the source of truth.
2. Create or reuse a `SpriteVisual` sized and positioned to that rectangle.
3. Fill it with the blur `CompositionEffectBrush`.
4. Clip it to the same rounded-corner geometry as the card.
5. Draw the normal medication UI above the blur visual.
6. Keep the area outside all card shapes transparent.

Do not maintain separate hard-coded geometry for:

- drawing;
- hit testing;
- clipping;
- composition visual placement.

All of them should derive from the same card layout values.

---

## Effect graph

The simple target effect should remain intentionally small.

Pseudo-graph:

```text
CompositionEffectSourceParameter("Backdrop")
    ↓
GaussianBlurEffect
    StandardDeviation = 20.0
    BorderMode = HARD
    Optimization = SPEED
    ↓
Composite dark tint over blurred backdrop
    ↓
CompositionEffectBrush
```

Use `CreateBackdropBrush()` as the `Backdrop` source.

Do not add Acrylic-style noise, luminosity blending, saturation changes, or extra decorative effects unless explicitly requested later.

---

## Suggested design tokens

Centralize the blur styling rather than scattering constants through rendering code.

Example:

```cpp
struct BlurTokens
{
    float blurAmount = 20.0f;
    float tintOpacity = 0.39f;

    // Neutral dark tint.
    uint8_t tintR = 0;
    uint8_t tintG = 0;
    uint8_t tintB = 0;

    float cornerRadius = /* existing card radius */;
};
```

If pure black looks too heavy against the current design, test a very dark neutral tint such as `RGB(24, 24, 24)` or `RGB(40, 40, 40)` while keeping opacity adjustable.

Do not change the blur radius and tint simultaneously when visually tuning. Tune one variable at a time.

---

## Resource lifetime

Do not create a new compositor/effect factory/effect graph every frame.

Prefer:

```text
Application lifetime:
    Compositor
    Composition target
    Root visual
    Effect factory
    Shared blur effect brush/factory where valid

Per-card lifetime:
    SpriteVisual
    clip
    position/size
```

Update card visual size/position only when layout changes.

The blur should be compositor-driven and should not require a CPU redraw loop while the widget is idle.

---

## Performance requirements

The app is intended to remain very lightweight.

Requirements:

- no continuous software background capture;
- no busy rendering loop;
- no polling just to refresh blur;
- no per-frame creation/destruction of composition resources;
- preserve the app's event-driven timer/update model;
- allow Windows/DWM composition to update the live backdrop;
- avoid extra top-level windows unless there is a proven technical need.

The blur should continue to react naturally when windows or wallpaper behind the widget change without the app manually repainting at high frequency.

---

## Rounded corners

The medication card blur must stop exactly at the visual card boundary.

Preferred approach:

- use a composition rounded clip if supported by the chosen implementation;
- otherwise use a composition geometry/shape mask that matches the existing card radius.

Do not approximate the corners with an unrelated `SetWindowRgn` silhouette unless there is a specific documented reason.

The foreground card border and the blur clip must visually align at all DPI scales.

---

## Foreground rendering

Keep text, icons, progress/timer elements, buttons and borders sharp.

Only the backdrop is blurred.

Correct layering:

```text
background windows / wallpaper
        ↓
backdrop blur
        ↓
dark tint
        ↓
card border
        ↓
text/icons/timer/progress UI
```

Never blur the card's own foreground content.

---

## Transparency behaviour

Expected result:

```text
card area       = blurred live backdrop + tint
gap between     = fully transparent
outside widget  = fully transparent
foreground UI   = opaque/sharp as currently designed
```

Moving the widget over another window should make that window visibly appear through the medication cards as a blurred image.

Moving the widget over a high-contrast wallpaper should make the wallpaper visibly influence the blurred card background.

---

## Fallback behaviour

If the required composition/backdrop path is unavailable or fails to initialize:

- fail gracefully;
- keep the app usable;
- fall back to the existing dark translucent/opaque card background;
- do not crash;
- do not silently start expensive screenshot-based blur emulation.

Log the reason in debug builds.

---

## Implementation order

### Phase 1 — inspect

Before editing:

1. Find the current HWND creation and layered-window flags.
2. Find the `UpdateLayeredWindow` presentation code.
3. Read `docs/DESIGN_PLAN.md`.
4. Find the existing card geometry/layout source.
5. Identify the smallest renderer change that can host Windows Composition correctly.

Do not change behaviour yet.

### Phase 2 — proof of concept

Implement one temporary blurred composition rectangle behind one card.

Success criteria:

- live content behind the window is visible;
- Gaussian blur is clearly visible;
- changing the blur amount changes blur strength;
- no screenshot capture is involved.

### Phase 3 — card integration

Apply the effect to every medication card.

Use the existing card bounds and corner radius.

### Phase 4 — presentation cleanup

Once the composition approach is proven:

- remove any superseded experimental presentation code;
- ensure only one intended production presentation path remains;
- keep fallback code small and explicit.

### Phase 5 — tuning

Start with:

```text
blurAmount  = 20.0
tintOpacity = 0.39
tintColor   = RGB(0, 0, 0)
```

Then tune only if needed to visually match the ExplorerBlurMica + DWMBlurGlass reference.

---

## Do not implement

Do not:

- inject into `dwm.exe`;
- copy DWMBlurGlass's MinHook/internal-DWM code;
- use undocumented DWM structure offsets;
- hook `CTopLevelWindow`;
- hook `UpdateClientBlur`;
- require ExplorerBlurMica or DWMBlurGlass to be installed;
- capture the desktop every frame;
- blur screenshots;
- use Acrylic noise;
- replace the effect with Mica;
- apply blur to the entire widget when only the cards should be blurred;
- blur foreground text/icons;
- add a permanent second rendering architecture without removing or formally retaining the old one.

---

## Acceptance criteria

The implementation is complete when all of the following are true:

- each medication card shows a live blurred view of whatever is behind the widget;
- the blur visually resembles DWMBlurGlass's simple `Blur` material rather than Acrylic or Mica;
- card gaps remain fully transparent;
- card rounded corners are clean and symmetrical;
- text and controls remain sharp;
- the effect works while dragging the widget across other windows;
- no screenshot/background-capture loop exists;
- idle CPU usage remains effectively negligible;
- no DWM injection or global system modification is required;
- failure of the blur path falls back safely;
- existing medication/timer behaviour is unchanged.

---

## Agent reporting requirements

After implementation, report:

1. **What changed** — renderer/composition files and the important effect/layout changes.
2. **What this means for the developer** — what new capability now exists and any architectural trade-off.
3. **Before vs after** — what is visibly different when the app is opened now.
4. **Performance impact** — whether any new recurring CPU work, timers or redraw loops were introduced.
5. **Fallback behaviour** — what happens if composition blur cannot initialize.
6. **Verification** — build/tests performed and manual blur checks completed.

Do not describe the work only as a list of functions/classes changed.
