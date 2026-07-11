#include "ui_font.h"
#include <stddef.h>

static Font g_font;
static bool g_fontLoaded = false;

void UIFont_Init(void) {
    const char *candidates[] = {
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
        // Rasterized large so downscaled text stays smooth with bilinear
        // filtering, instead of chunky nearest-neighbor upscaling.
        Font f = LoadFontEx(candidates[i], 48, NULL, 0);
        if (f.texture.id != 0) {
            g_font = f;
            g_fontLoaded = true;
            SetTextureFilter(g_font.texture, TEXTURE_FILTER_BILINEAR);
            return;
        }
    }
    // No system font found - raylib's built-in font still works, just
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
