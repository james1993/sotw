#ifndef UI_FONT_H
#define UI_FONT_H

#include "raylib.h"

// Loads a real system TTF (DejaVu/Arial/Segoe/Helvetica, first found) so
// UI text is readable instead of raylib's blocky built-in bitmap font.
// Falls back silently to the default font if none exists. Call once
// after InitWindow.
void UIFont_Init(void);

// DrawText/MeasureText replacements that use the loaded body font.
void UIText(const char *text, int x, int y, int size, Color color);
int UITextWidth(const char *text, int size);

// The DISPLAY face - a Roman capital serif, for titles, window headers
// and zone names only. Deliberately not available to body text: the
// thing that makes it good at 40px is what makes it bad at 11px.
// Falls back to the body font when the display face is missing.
void UITextDisplay(const char *text, int x, int y, int size, Color color);
int UITextDisplayWidth(const char *text, int size);

// The one shared UI scale factor: screenHeight relative to an 800px
// reference window, clamped so tiny windows stay usable and huge ones
// don't balloon. Every screen-space widget sizes itself through this.
#define UI_REFERENCE_HEIGHT 800.0f
#define UI_MIN_SCALE 0.85f
#define UI_MAX_SCALE 5.0f
float UI_Scale(int screenHeight);

// Scales a font size by UI_Scale, for any screen-space text.
int UI_ScaledFontSize(int screenHeight, int baseSize);

#endif
