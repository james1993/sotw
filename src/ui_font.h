#ifndef UI_FONT_H
#define UI_FONT_H

#include "raylib.h"

// Loads a real system TTF (DejaVu/Arial/Segoe/Helvetica, first found) so
// UI text is readable instead of raylib's blocky built-in bitmap font.
// Falls back silently to the default font if none exists. Call once
// after InitWindow.
void UIFont_Init(void);

// DrawText/MeasureText replacements that use the loaded font.
void UIText(const char *text, int x, int y, int size, Color color);
int UITextWidth(const char *text, int size);

#endif
