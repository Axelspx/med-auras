#pragma once

#include <windows.h>

#include <cstddef>

// Live backdrop blur behind the medication cards, built on Windows Composition.
//
// The compositor owns the blur: a CompositionBackdropBrush feeds a Gaussian blur effect, and one
// sprite visual per card presents it through a rounded-corner mask. Nothing here captures the
// desktop, and nothing here needs a repaint to keep the blur current.
//
// If any part of the path is unavailable the whole module reports inactive and the caller keeps
// its existing layered-window presentation.
namespace blur {

struct Tokens {
    // Gaussian standard deviation in pixels. Tune this alone; leave the tint fixed while you do.
    float blur_amount;
};

// Creates the compositor, rendering device, and shared brushes. No window is involved yet, so the
// caller can choose the window's extended style from the result.
//
// Succeeds as long as composition itself works. The Gaussian blur is a separate question: if only
// the effect brush fails, this still returns true and the widget runs in solid mode.
bool initialize(const Tokens& tokens);

// Whether the blurred-backdrop layer is available. False means only solid backgrounds can be
// offered, and the caller should not present blur as a choice.
bool blur_supported();

// Sets the card background. `blurred` adds the live blurred backdrop beneath the colour; the colour
// is a tint over it when blurred, and the card itself when not. Cheap and idempotent: unchanged
// values do no work, and a colour change alone does not rebuild the visual tree.
void set_background(bool blurred, BYTE red, BYTE green, BYTE blue, BYTE alpha);

// Binds the composition tree to a window. The window must not be WS_EX_LAYERED.
bool attach(HWND window);

bool active();

// Rebuilds the per-card visuals. Cards are uniform in size; origins are client pixels. Cheap and
// idempotent: identical geometry is ignored, so this can be called from the paint path.
void set_cards(
    const POINT* origins, std::size_t count, int card_width, int card_height, float corner_radius);

// Publishes one premultiplied top-down 32-bit BGRA frame as the foreground layer.
bool publish(const void* premultiplied_bgra, int width, int height);

void shutdown();

}  // namespace blur
