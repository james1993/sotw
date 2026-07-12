#include "ui_font.h"
#include <stddef.h>
#include <stdio.h>

static Font g_font;
static bool g_fontLoaded = false;

void UIFont_Init(void) {
    // Rasterize the atlas to match the monitor: UI text is drawn at up
    // to ~5x the 800px-reference sizes on a 4K display, and upscaling a
    // fixed 48px atlas there turns soft. Scale the raster size with the
    // monitor (clamped - a 95-glyph atlas at 160px is still tiny).
    int monitorH = GetMonitorHeight(GetCurrentMonitor());
    if (monitorH <= 0) monitorH = (int)UI_REFERENCE_HEIGHT;
    int rasterSize = (int)(48.0f * ((float)monitorH / UI_REFERENCE_HEIGHT));
    if (rasterSize < 48) rasterSize = 48;
    if (rasterSize > 160) rasterSize = 160;

    // The bundled font ships in assets/ next to the executable (CMake
    // copies it post-build), so text looks the same on every machine.
    // System fonts are only a fallback for running a bare binary.
    char bundled[512];
    snprintf(bundled, sizeof(bundled), "%sassets/fonts/DejaVuSans.ttf",
             GetApplicationDirectory()); // includes the trailing separator

    const char *candidates[] = {
        bundled,
        "assets/fonts/DejaVuSans.ttf", // running from the repo root
        // Linux
        "/usr/share/fonts/truetype/dejavu/DejaVuSans.ttf",
        "/usr/share/fonts/TTF/DejaVuSans.ttf",
        "/usr/share/fonts/truetype/liberation/LiberationSans-Regular.ttf",
        // macOS
        "/System/Library/Fonts/Supplemental/Arial.ttf",
        "/System/Library/Fonts/Supplemental/Helvetica.ttf",
        "/Library/Fonts/Arial.ttf",
        // Windows
        "C:/Windows/Fonts/segoeui.ttf",
        "C:/Windows/Fonts/arial.ttf",
    };

    for (unsigned i = 0; i < sizeof(candidates) / sizeof(candidates[0]); i++) {
        if (!FileExists(candidates[i])) continue;
        Font f = LoadFontEx(candidates[i], rasterSize, NULL, 0);
        if (f.texture.id != 0) {
            g_font = f;
            g_fontLoaded = true;
            // Bilinear so downscaled text stays smooth instead of chunky
            // nearest-neighbor.
            SetTextureFilter(g_font.texture, TEXTURE_FILTER_BILINEAR);
            return;
        }
    }
    // No font found anywhere - raylib's built-in font still works, just
    // blockier. UIText falls back automatically.
}

void UIText(const char *text, int x, int y, int size, Color color) {
    if (g_fontLoaded) {
        DrawTextEx(g_font, text, (Vector2){ (float)x, (float)y }, (float)size, size * 0.04f, color);
    } else {
        DrawText(text, x, y, size, color);
    }
}

int UITextWidth(const char *text, int size) {
    if (g_fontLoaded) {
        return (int)MeasureTextEx(g_font, text, (float)size, size * 0.04f).x;
    }
    return MeasureText(text, size);
}

float UI_Scale(int screenHeight) {
    float scale = (float)screenHeight / UI_REFERENCE_HEIGHT;
    if (scale < UI_MIN_SCALE) scale = UI_MIN_SCALE;
    if (scale > UI_MAX_SCALE) scale = UI_MAX_SCALE;
    return scale;
}

int UI_ScaledFontSize(int screenHeight, int baseSize) {
    return (int)(baseSize * UI_Scale(screenHeight));
}
